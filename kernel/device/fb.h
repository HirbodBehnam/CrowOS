#pragma once
#include "limine.h"
#include <stddef.h>
#include <stdint.h>

#define FRAMEBUFFER_DEVICE_NAME "fb"

void fb_init(struct limine_framebuffer *fb);
int fb_write(const char *buffer, size_t size);
int fb_control(int command, void *data);