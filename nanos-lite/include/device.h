#ifndef __DEVICE_H__
#define __DEVICE_H__

#include <common.h>

// VFS 特殊文件的读写函数, 实现在 src/device.c
// 在 file_table 中作为 read/write 函数指针登记
size_t serial_write(const void *buf, size_t offset, size_t len);
size_t events_read(void *buf, size_t offset, size_t len);
size_t dispinfo_read(void *buf, size_t offset, size_t len);
size_t fb_write(const void *buf, size_t offset, size_t len);

void init_device();

#endif
