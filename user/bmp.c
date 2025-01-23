#include "include/fb.h"
#include "include/file.h"
#include "libc/stdio.h"
#include "libc/usyscalls.h"
#include <stdint.h>

#define DATA_OFFSET_OFFSET 0x000A
#define WIDTH_OFFSET 0x0012
#define HEIGHT_OFFSET 0x0016
#define BITS_PER_PIXEL_OFFSET 0x001C

int read_image(const char *filename, uint8_t **pixels, uint32_t *width,
               uint32_t *height, uint32_t *bytes_per_pixel) {
  // Open the file for reading in binary mode
  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    puts("cannot open bmp file");
    return -1;
  }
  // Read data offset
  uint32_t dataOffset;
  lseek(fd, DATA_OFFSET_OFFSET, SEEK_SET);
  read(fd, &dataOffset, sizeof(dataOffset));
  // Read width
  lseek(fd, WIDTH_OFFSET, SEEK_SET);
  read(fd, width, sizeof(*width));
  // Read height
  lseek(fd, HEIGHT_OFFSET, SEEK_SET);
  read(fd, height, sizeof(*height));
  // Read bits per pixel
  int16_t bits_per_pixel;
  lseek(fd, BITS_PER_PIXEL_OFFSET, SEEK_SET);
  read(fd, &bits_per_pixel, sizeof(bits_per_pixel));
  // Allocate a pixel array
  *bytes_per_pixel = ((uint32_t)bits_per_pixel) / 8;

  // Rows are stored bottom-up
  // Each row is padded to be a multiple of 4 bytes.
  // We calculate the padded row size in bytes
  int padded_row_size = (int)(*width + (*width % 4 != 0)) * (*bytes_per_pixel);
  // We are not interested in the padded bytes, so we allocate memory just for
  // the pixel data
  int unpadded_row_size = (*width) * (*bytes_per_pixel);
  // Total size of the pixel data in bytes
  int totalSize = unpadded_row_size * (*height);
  *pixels = sbrk(totalSize);
  // Read the pixel data Row by Row.
  // Data is padded and stored bottom-up
  // point to the last row of our pixel array (unpadded)
  uint8_t *currentRowPointer = *pixels + ((*height - 1) * unpadded_row_size);
  for (uint32_t i = 0; i < *height; i++) {
    // put file cursor in the next row from top to bottom
    lseek(fd, dataOffset + (i * padded_row_size), SEEK_SET);
    // read only unpaddedRowSize bytes (we can ignore the padding bytes)
    read(fd, currentRowPointer, unpadded_row_size);
    // point to the next row (from bottom to top)
    currentRowPointer -= unpadded_row_size;
  }

  close(fd);
  return 0;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    puts("Please pass the file name as the first argument");
    exit(1);
  }
  // Read the image
  uint8_t *pixels;
  uint32_t width, height, bytes_per_pixel;
  int result = read_image(argv[1], &pixels, &width, &height, &bytes_per_pixel);
  if (result < 0)
    exit(1);
  printf("Read image with %lux%lu dimensions and %lu bytes per pixel.\n", width,
         height, bytes_per_pixel);
  // Check the assumptions of the file
  if (bytes_per_pixel != 3) {
    puts("Expected 3 bytes per pixel");
    exit(1);
  }
  // Convert the image to a format that framebuffer understands
  struct FramebufferPixel *fb_pixels =
      sbrk(width * height * sizeof(struct FramebufferPixel));
  for (size_t i = 0; i < width * height * bytes_per_pixel;
       i += bytes_per_pixel) {
    fb_pixels[i / bytes_per_pixel].red = pixels[i];
    fb_pixels[i / bytes_per_pixel].green = pixels[i + 1];
    fb_pixels[i / bytes_per_pixel].blue = pixels[i + 2];
  }
  // Open the frame buffer
  int fb = open("fb", O_DEVICE);
  if (fb < 0) {
    printf("cannot open frame buffer: %d\n", fb);
    exit(1);
  }
  // Check the screen size
  uint64_t screen_width, screen_height;
  ioctl(fb, FRAMEBUFFER_CTL_GET_MAX_WIDTH, &screen_width);
  ioctl(fb, FRAMEBUFFER_CTL_GET_MAX_HEIGHT, &screen_height);
  if (screen_width <  width || screen_height < height) {
    puts("Small screen!");
    exit(1);
  }
  // Clear the screen
  ioctl(fb, FRAMEBUFFER_CTL_CLEAR, NULL);
  // Set the dimensions
  ioctl(fb, FRAMEBUFFER_CTL_SET_WIDTH, (void *)((uint64_t)width));
  ioctl(fb, FRAMEBUFFER_CTL_SET_HEIGHT, (void *)((uint64_t)height));
  // Show it
  write(fb, fb_pixels, width * height);
  close(fb);

  return 0;
}