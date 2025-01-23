#pragma once
#include <stdint.h>

// Get the current width of the frame buffer
#define FRAMEBUFFER_CTL_GET_WIDTH 0
// Get the maximum width of the frame buffer (screen size)
#define FRAMEBUFFER_CTL_GET_MAX_WIDTH 1
// Set the width of the frame buffer
#define FRAMEBUFFER_CTL_SET_WIDTH 2
// Get the current height of the frame buffer
#define FRAMEBUFFER_CTL_GET_HEIGHT 3
// Get the maximum height of the frame buffer (screen size)
#define FRAMEBUFFER_CTL_GET_MAX_HEIGHT 4
// Set the height of the frame buffer
#define FRAMEBUFFER_CTL_SET_HEIGHT 5
// Clear the screen
#define FRAMEBUFFER_CTL_CLEAR 6

/**
 * Each framebuffer pixel contains three colors: Red Green Blue each between [0,
 * 255]. When you are writing to the framebuffer, send an array of this struct
 * to the driver.
 */
struct FramebufferPixel {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t padding;
};

_Static_assert(sizeof(struct FramebufferPixel) == 4,
               "Frame buffer pixel must be 4 bytes");