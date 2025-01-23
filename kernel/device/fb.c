#include "fb.h"
#include "common/lib.h"
#include "common/printf.h"
#include "include/fb.h"
#include <stddef.h>

/**
 * The framebuffer which we work with
 */
static struct limine_framebuffer *main_framebuffer = NULL;

/**
 * Current width and height which we expect from write syscall
 */
static uint64_t current_width, current_height;

/**
 * Initializes the frame buffer based on the Limine boot protocol frame buffer
 */
void fb_init(struct limine_framebuffer *fb) {
  main_framebuffer = fb;
  current_width = fb->width;
  current_height = fb->height;
  kprintf("Initialized framebuffer with %llux%llu dimensions and %u bpp\n",
          current_width, current_height, (uint32_t)fb->bpp);
  if (fb->bpp != 32)
    panic("fb_init: expected the framebuffer to be 32 bits per pixel");
}

/**
 * Show the given buffer in the screen. Size is the size of the buffer in pixels
 * (4 bytes). Note that the buffer will fill the screen based on the
 * current_width and current_height values which can be set from the fb control.
 * To make things easier for you, we suggest to use struct FramebufferPixel
 * defined in "include/fb.h" as the buffer array.
 */
int fb_write(const char *buffer, size_t size) {
  if (main_framebuffer == NULL)
    return -1;
  struct FramebufferPixel *fb_ptr =
      (struct FramebufferPixel *)main_framebuffer->address;
  const struct FramebufferPixel *given_buffer =
      (struct FramebufferPixel *)buffer;
  // Copy each row
  size_t row = 0, displayed = 0;
  while (size > current_width && row < current_height) {
    memcpy(&fb_ptr[row * (main_framebuffer->pitch / 4)], given_buffer,
           current_width * 4);
    // Advance the row and reduce the remaining pixels
    row++;
    displayed += current_width;
    given_buffer += current_width;
    size -= current_width;
  }
  // Copy the final remaining row
  if (row < current_height && size != 0) {
    memcpy(&fb_ptr[row * (main_framebuffer->pitch / 4)], given_buffer,
           size * 4);
    displayed += size;
  }

  return displayed;
}

/**
 * Control the framebuffer options. Or maybe read frame buffer options.
 *
 * Read the include/fd.h for more information
 */
int fb_control(int command, void *data) {
  // Sanity check
  if (main_framebuffer == NULL)
    return -1;
  // Do the command
  switch (command) {
  case FRAMEBUFFER_CTL_GET_WIDTH:
    memcpy(data, &current_width, sizeof(current_width));
    return 0;
  case FRAMEBUFFER_CTL_GET_MAX_WIDTH:
    memcpy(data, &main_framebuffer->width, sizeof(main_framebuffer->width));
    return 0;
  case FRAMEBUFFER_CTL_SET_WIDTH:
    if ((uint64_t)data <= main_framebuffer->width) {
      current_width = (uint64_t)data;
      return 0;
    } else { // invalid width
      return -3;
    }
    return 0;
  case FRAMEBUFFER_CTL_GET_HEIGHT:
    memcpy(data, &current_height, sizeof(current_height));
    return 0;
  case FRAMEBUFFER_CTL_GET_MAX_HEIGHT:
    memcpy(data, &main_framebuffer->height, sizeof(main_framebuffer->height));
    return 0;
  case FRAMEBUFFER_CTL_SET_HEIGHT:
    if ((uint64_t)data <= main_framebuffer->height) {
      current_height = (uint64_t)data;
      return 0;
    } else { // invalid height
      return -3;
    }
  case FRAMEBUFFER_CTL_CLEAR:
    memset(main_framebuffer->address, 0,
           main_framebuffer->width * main_framebuffer->height *
               main_framebuffer->bpp / 4);
    return 0;
  default:
    return -2;
  }
}
