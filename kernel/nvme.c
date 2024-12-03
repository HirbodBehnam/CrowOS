/**
 * Mostly from
 * https://chromium.googlesource.com/chromiumos/platform/depthcharge/+/master/src/drivers/storage/nvme.c
 */

#include "nvme.h"
#include "mem.h"
#include "pcie.h"
#include "printf.h"
#include "vmm.h"
#include <stddef.h>
#include <stdint.h>

/**
 * Base register for NVMe
 */
static void *nvme_base;

/**
 * Next command ID which we will issue for the submission ID
 */
static uint16_t next_command_id = 0;
// Gets the next command ID
#define NEXT_COMMAND_ID()                                                      \
  (__atomic_fetch_add(&next_command_id, 1, __ATOMIC_RELAXED))

// Returns the addres of a 4 byte long register
#define NVME_REG4(offset) (*((uint32_t volatile *)(nvme_base + offset)))
// Returns the addres of a 8 byte long register
#define NVME_REG8(offset) (*((uint64_t volatile *)(nvme_base + offset)))
// Queue size for admin SQ and CQ
#define NVME_ADMIN_QUEUE_SIZE 2
// Queue size for IO SQ and CQ
#define NVME_IO_QUEUE_SIZE 8
// Each page size is 2^(this_value)
#define NVME_PAGE_SIZE_BITS 12
// Each page size of NVMe buffers
#define NVME_PAGE_SIZE (1ULL < NVME_PAGE_SIZE_BITS)
// Doorbell stride, bytes
#define NVME_CAP_DSTRD(x)	(1 << (2 + (((x) >> 32) & 0xf)))

/*
 * These register offsets are defined as 0x1000 + (N * (DSTRD bytes))
 * Get the doorbell stride bit shift value from the controller capabilities.
 */
#define NVME_SQTDBL_OFFSET(QID, DSTRD)	(0x1000 + ((2 * (QID)) * (DSTRD)))	/* Submission Queue y (NVM) Tail Doorbell */
#define NVME_CQHDBL_OFFSET(QID, DSTRD)	(0x1000 + (((2 * (QID)) + 1) * (DSTRD)))	/* Completion Queue y (NVM) Head Doorbell */


/* controller register offsets */
#define NVME_CAP_OFFSET 0x0000   /* Controller Capabilities */
#define NVME_VER_OFFSET 0x0008   /* Version */
#define NVME_INTMS_OFFSET 0x000c /* Interrupt Mask Set */
#define NVME_INTMC_OFFSET 0x0010 /* Interrupt Mask Clear */
#define NVME_CC_OFFSET 0x0014    /* Controller Configuration */
#define NVME_CSTS_OFFSET 0x001c  /* Controller Status */
#define NVME_AQA_OFFSET 0x0024   /* Admin Queue Attributes */
#define NVME_ASQ_OFFSET 0x0028   /* Admin Submission Queue Base Address */
#define NVME_ACQ_OFFSET 0x0030   /* Admin Completion Queue Base Address */
#define NVME_SQ0_OFFSET 0x1000   /* Submission Queue 0 (admin) Tail Doorbell */
#define NVME_CQ0_OFFSET 0x1004   /* Completion Queue 0 (admin) Head Doorbell */

/* NVMe Admin Cmd Opcodes */
#define NVME_ADMIN_SETFEATURES_OPC 9
#define NVME_ADMIN_SETFEATURES_NUMQUEUES 7

/* Submission Queue */
typedef struct {
  uint8_t opc;   /* Opcode */
  uint8_t flags; /* FUSE and PSDT, only 0 setting supported */
  uint16_t cid;  /* Command Identifier */
  uint32_t nsid; /* Namespace Identifier */
  uint64_t rsvd1;
  uint64_t mptr;   /* Metadata Pointer */
  uint64_t prp[2]; /* PRP entries only, SGL not supported */
  uint32_t cdw10;
  uint32_t cdw11;
  uint32_t cdw12;
  uint32_t cdw13;
  uint32_t cdw14;
  uint32_t cdw15;
} NVME_SQ_ENTRY;

/* Completion Queue */
typedef struct {
  uint32_t cdw0;
  uint32_t rsvd1;
  uint16_t sqhd; /* Submission Queue Head Pointer */
  uint16_t sqid; /* Submission Queue Identifier */
  uint16_t cid;  /* Command Identifier */
  uint16_t flags;
#define NVME_CQ_FLAGS_PHASE 0x1
#define NVME_CQ_FLAGS_SC(x) (((x) & 0x1FE) >> 1)
#define NVME_CQ_FLAGS_SCT(x) (((x) & 0xE00) >> 9)
} NVME_CQ_ENTRY;

/**
 * Declares a pair of submission and completion queue which are used
 * in NVMe interface
 */
struct nvme_queue {
  // The queue entries must be dword aligned. So we use kalloc
  NVME_SQ_ENTRY *submission_queue;
  // The queue entries must be dword aligned. So we use kalloc
  NVME_CQ_ENTRY *completion_queue;
  uint32_t submission_queue_doorbell;
  uint32_t completion_queue_doorbell;
  uint32_t queue_index;
};

/**
 * Data about the NVMe device attached to PCIe
 */
static struct nvme_device {
  uint64_t cap;
  struct nvme_queue admin_queue;
  struct nvme_queue io_queue;
} nvme_device;

/**
 * Disables the NVMe device attached
 */
static void nvme_disable_device(void) {
  // clear EN bit
  NVME_REG4(NVME_CC_OFFSET) &= ~(1UL);
  // Wait unitl the controller shuts down
  // (Check RDY bit)
  while ((NVME_REG4(NVME_CSTS_OFFSET) & 1) == 1)
    ;
  __sync_synchronize();
}

/**
 * Enables the NVMe device and sets default CC register values.
 * Waits for the controller to start.
 */

static void nvme_enable_device(void) {
  // Set enable bit, IOCQES and IOSQES
  // See figure 312 in NVMe specification for more info about
  // 6 and 4 values for IOCQES and IOSQES
  const uint32_t cc = 1 | (6 << 16) | (4 << 20);
  // Write back the control configuration register to enable the device
  NVME_REG4(NVME_CC_OFFSET) = cc;
  // Wait for controller to start
  // (Check RDY bit)
  while ((NVME_REG4(NVME_CSTS_OFFSET) & 1) == 0)
    ;
  __sync_synchronize();
}

/*
 * Submit and complete 1 command by polling CQ for phase change
 * Rings SQ doorbell, polls waiting for completion, rings CQ doorbell
 */
static void nvme_do_one_cmd_synchronous(struct nvme_queue *queue) {
  // Increment the submission queue tail
  if (++(queue->submission_queue_doorbell) > (NVME_ADMIN_QUEUE_SIZE - 1))
    queue->submission_queue_doorbell = 0;
  // Ring the doorbell
  // 0 is the index of the admin queue. It always is zero
  NVME_REG4(NVME_SQTDBL_OFFSET(queue->queue_index, NVME_CAP_DSTRD(nvme_device.cap))) = queue->submission_queue_doorbell;
  // Wait for the queue to complete
  // TODO: DO THIS
}

/**
 * Sets the number of IO queues
 */
static void nvme_set_queue_count(uint32_t queue_count) {
  // Allocate a submission request from the queue
  NVME_SQ_ENTRY *sq =
      &nvme_device.admin_queue
           .submission_queue[nvme_device.admin_queue.submission_queue_doorbell];
  memset(sq, 0, sizeof(NVME_SQ_ENTRY));
  // Set the information
  sq->opc = NVME_ADMIN_SETFEATURES_OPC;
  sq->cid = NEXT_COMMAND_ID();
  sq->cdw10 = NVME_ADMIN_SETFEATURES_NUMQUEUES;
  /* Count is a 0's based value, so subtract one */
  queue_count--;
  /* Set count number of IO SQs and CQs */
  sq->cdw11 = queue_count;
  sq->cdw11 |= (queue_count << 16);
  nvme_do_one_cmd_synchronous(&nvme_device.admin_queue);
}

/**
 * Initialize NVMe driver
 *
 * Under the hood, it looks for NVMe devices attached to PCIe,
 * initializes the IO queues and setups the interrupts.
 */
void nvme_init(void) {
  // Get the base of NVMe registers
  uint64_t nvme_base_physical = pcie_get_nvme_base();
  // Map for IO based region
  nvme_base = vmm_io_memmap(nvme_base_physical, 0x1000);
  if (nvme_base == NULL)
    panic("nvme: could not get NVMe base");
  // Read CAP register
  nvme_device.cap = NVME_REG8(NVME_CAP_OFFSET);
  if (((nvme_device.cap >> 37) & 1) == 0) // CSS
    panic("nvme: NCSS not supported");
  if ((12 + ((nvme_device.cap >> 48) & 0xf)) > NVME_PAGE_SIZE_BITS) // MPSMIN
    panic("nvme: Driver does not support 4kb pages");
  if ((nvme_device.cap & 0xffff) < NVME_IO_QUEUE_SIZE) // MQES
    panic("nvme: Small queue size");
  // Allocate the Queues
  nvme_device.admin_queue.submission_queue = kcalloc();
  nvme_device.admin_queue.completion_queue = kcalloc();
  nvme_device.io_queue.submission_queue = kcalloc();
  nvme_device.io_queue.completion_queue = kcalloc();
  // Set queue index
  nvme_device.admin_queue.queue_index = 0;
  nvme_device.io_queue.queue_index = 1;
  // Disable the device to set the control registers
  nvme_disable_device();
  // Set admin queue attributes
  // First 11 bytes are Admin Submission Queue Size
  // Bytes from 16:27 are Admin Completion Queue Size
  const uint32_t aqa =
      (NVME_ADMIN_QUEUE_SIZE - 1) | ((NVME_ADMIN_QUEUE_SIZE - 1) << 16);
  NVME_REG4(NVME_AQA_OFFSET) = aqa;
  NVME_REG8(NVME_ASQ_OFFSET) = V2P(nvme_device.admin_queue.submission_queue);
  NVME_REG8(NVME_ACQ_OFFSET) = V2P(nvme_device.admin_queue.completion_queue);
  // Enable the device because we have set the stuff we need
  nvme_enable_device();
}