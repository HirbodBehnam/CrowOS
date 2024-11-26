/* controller register offsets */
#define NVME_CAP_OFFSET		0x0000  /* Controller Capabilities */
#define NVME_VER_OFFSET		0x0008  /* Version */
#define NVME_INTMS_OFFSET	0x000c  /* Interrupt Mask Set */
#define NVME_INTMC_OFFSET	0x0010  /* Interrupt Mask Clear */
#define NVME_CC_OFFSET		0x0014  /* Controller Configuration */
#define NVME_CSTS_OFFSET	0x001c  /* Controller Status */
#define NVME_AQA_OFFSET		0x0024  /* Admin Queue Attributes */
#define NVME_ASQ_OFFSET		0x0028  /* Admin Submission Queue Base Address */
#define NVME_ACQ_OFFSET		0x0030  /* Admin Completion Queue Base Address */
#define NVME_SQ0_OFFSET		0x1000  /* Submission Queue 0 (admin) Tail Doorbell */
#define NVME_CQ0_OFFSET		0x1004  /* Completion Queue 0 (admin) Head Doorbell */

void nvme_init(void);