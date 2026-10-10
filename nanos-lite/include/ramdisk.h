#ifndef __RAMDISK_H__
#define __RAMDISK_H__

// 声明成不完整数组:ramdisk_start/end 只是 resources.S 里的链接地址标记,
// 声明为 uint8_t[] 后编译器不会做数组边界检查(标量声明会被 -Warray-bounds 误报)
extern uint8_t ramdisk_start[];
extern uint8_t ramdisk_end[];
#define RAMDISK_SIZE (ramdisk_end - ramdisk_start)

size_t ramdisk_read(void *buf, size_t offset, size_t len);
size_t ramdisk_write(const void *buf, size_t offset, size_t len);
void init_ramdisk();
size_t get_ramdisk_size();

#endif