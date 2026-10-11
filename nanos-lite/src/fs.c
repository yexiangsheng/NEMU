#include <fs.h>
#include <ramdisk.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define FILE_NUM sizeof(file_table) / sizeof(Finfo)

// IO func pointer
typedef size_t (*ReadFn)(void *buf, size_t offset, size_t len);
typedef size_t (*WriteFn)(const void *buf, size_t offset, size_t len);

typedef struct
{
  char *name;         // file name
  size_t size;        // file size
  size_t disk_offset; // file offset in ramdisk
  size_t open_offset; // offset in opened file
  ReadFn read;
  WriteFn write;
} Finfo;

// File Descriptor
enum
{
  FD_STDIN,
  FD_STDOUT,
  FD_STDERR,
  FD_FB
};

size_t invalid_read(void *buf, size_t offset, size_t len)
{
  panic("should not reach here");
  return 0;
}

size_t invalid_write(const void *buf, size_t offset, size_t len)
{
  panic("should not reach here");
  return 0;
}

/* This is the information about all files in disk. */
static Finfo file_table[] __attribute__((used)) = {
    [FD_STDIN] = {"stdin", 0, 0, 0, invalid_read, invalid_write},
    [FD_STDOUT] = {"stdout", 0, 0, 0, invalid_read, invalid_write},
    [FD_STDERR] = {"stderr", 0, 0, 0, invalid_read, invalid_write},
#include "files.h"
};

int fs_open(const char *pathname, int flags, int mode)
{
  for (int i = 0; i < FILE_NUM; i++)
  {
    if (strcmp(file_table[i].name, pathname) == 0)
    {
      return i;
    }
  }
  return (int)-1;
}

size_t fs_read(int fd, void *buf, size_t len)
{
  size_t file_location = file_table[fd].disk_offset;
  size_t file_open_offset = file_table[fd].open_offset;
  ramdisk_read(buf, file_location + file_open_offset, len);
  file_table[fd].open_offset += len;
  assert(file_table[fd].open_offset <= file_table[fd].size);
  return len;
}

size_t fs_write(int fd, const void *buf, size_t len)
{
  size_t file_location = file_table[fd].disk_offset;
  size_t file_open_offset = file_table[fd].open_offset;
  ramdisk_write(buf, file_location + file_open_offset, len);
  file_table[fd].open_offset += len;
  assert(file_table[fd].open_offset <= file_table[fd].size);
  return len;
}

size_t fs_lseek(int fd, size_t offset, int whence)
{
  size_t file_location = file_table[fd].disk_offset;
  size_t file_size = file_table[fd].size;
  size_t file_open_offset = file_table[fd].open_offset;
  if (whence == SEEK_SET)
  {
    assert(offset <= file_table[fd].size);
    return file_location + offset;
  }
  if (whence == SEEK_CUR)
  {
    assert(file_open_offset + offset <= file_table[fd].size);
    return file_location + file_open_offset + offset;
  }
  if (whence == SEEK_END)
  {
    assert(file_size + offset <= file_table[fd].size);
    return file_location + file_size + offset;
  }
  return (off_t)-1;
}

int fs_close(int fd)
{
  return 0;
}

void init_fs()
{
  // TODO: initialize the size of /dev/fb
}
