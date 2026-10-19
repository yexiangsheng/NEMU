#include <proc.h>
#include <elf.h>
#include <ramdisk.h>
#include <memory.h>
#include <fs.h>

#ifdef __LP64__
#define Elf_Ehdr Elf64_Ehdr
#define Elf_Phdr Elf64_Phdr
#else
#define Elf_Ehdr Elf32_Ehdr
#define Elf_Phdr Elf32_Phdr
#endif

// #define PROGRAM_HEADER_OFFSET sizeof(unsigned char) * 16 + sizeof(Elf32_Half) * 2 + sizeof(Elf32_Word) + sizeof(Elf32_Addr)
// #define MACHINE_OFFSET sizeof(Elf32_Half) * 2
// #define PROGRAM_SIZE_OFFSET PROGRAM_HEADER_OFFSET + sizeof(Elf32_Off) * 2 + sizeof(Elf32_Word) + sizeof(Elf32_Half)
// #define PROGRAM_NUM_OFFSET PROGRAM_SIZE_OFFSET + sizeof(Elf32_Half)
// #define PROGRAM_TYPE_IS_LOAD_NUM 10
//  typedef struct
//{
//  Elf32_Addr e_entry;
//  Elf32_Off e_phoff;
//  Elf32_Half e_phentsize;

//}

// loader() 最终实现
// 非 static: context_uload 也要用它加载 ELF(proc.h 有声明)
uintptr_t loader(PCB *pcb, const char *filename)
{
  int fd = fs_open(filename, 0, 0);
  Elf32_Ehdr ehdr;
  fs_read(fd, &ehdr, sizeof(ehdr)); // 从文件读 ELF 头
  assert(ehdr.e_ident[0] == ELFMAG0 && ehdr.e_ident[1] == 'E' &&
         ehdr.e_ident[2] == ELFMAG2 && ehdr.e_ident[3] == ELFMAG3);
  Log("entry = 0x%x, phoff = 0x%x, phnum = %d", ehdr.e_entry, ehdr.e_phoff, ehdr.e_phnum);
  for (int i = 0; i < ehdr.e_phnum; i++)
  {
    Elf32_Phdr phdr;
    fs_lseek(fd, ehdr.e_phoff + i * sizeof(phdr), SEEK_SET); // 定位到程序头表
    fs_read(fd, &phdr, sizeof(phdr));
    if (phdr.p_type != PT_LOAD)
      continue;
    fs_lseek(fd, phdr.p_offset, SEEK_SET); // 段数据在文件中的位置
    fs_read(fd, (void *)(uintptr_t)phdr.p_vaddr, phdr.p_filesz);
    memset((void *)(uintptr_t)(phdr.p_vaddr + phdr.p_filesz), 0,
           phdr.p_memsz - phdr.p_filesz); // 清 .bss
  }
  return ehdr.e_entry;
}

#if 0 // 第二版
static uintptr_t loader(PCB *pcb, const char *filename)
{
  // TODO();

  /* the ramdisk holds exactly one ELF file at this stage, so `filename'
   * is ignored and the ELF header sits at the very beginning of the ramdisk.
   */
  int fd = fs_open(filename, 0, 0);
  Elf32_Ehdr *ehdr = fs_read(fd, &ramdisk_start, sizeof());

  /* check the ELF magic */
  assert(ehdr->e_ident[EI_MAG0] == ELFMAG0 &&
         ehdr->e_ident[EI_MAG1] == ELFMAG1 &&
         ehdr->e_ident[EI_MAG2] == ELFMAG2 &&
         ehdr->e_ident[EI_MAG3] == ELFMAG3);

  /* check the machine type */
#if defined(__ISA_AM_NATIVE__)
#define EXPECT_TYPE EM_X86_64
#elif defined(__ISA_X86__)
#define EXPECT_TYPE EM_386 // see /usr/include/elf.h to get the right type
#elif defined(__riscv)
#define EXPECT_TYPE EM_RISCV
#else
#error Unsupported ISA
#endif
  if (ehdr->e_machine != EXPECT_TYPE)
    panic("unexpected machine type 0x%x (expect 0x%x)", ehdr->e_machine, EXPECT_TYPE);

  Log("entry = 0x%x, phoff = 0x%x, phnum = %d", ehdr->e_entry, ehdr->e_phoff, ehdr->e_phnum);

  /* load every PT_LOAD segment: copy [p_offset, p_offset + p_filesz)
   * from the ramdisk to [p_vaddr, p_vaddr + p_filesz), then zero out
   * the rest [p_vaddr + p_filesz, p_vaddr + p_memsz) (e.g. .bss).
   */
  Elf32_Phdr *phdr = (Elf32_Phdr *)((uint8_t *)ehdr + ehdr->e_phoff);
  for (int i = 0; i < ehdr->e_phnum; i++)
  {
    if (phdr[i].p_type != PT_LOAD)
      continue;
    Log("loading segment %d: vaddr = 0x%x, filesz = 0x%x, memsz = 0x%x",
        i, phdr[i].p_vaddr, phdr[i].p_filesz, phdr[i].p_memsz);
    // ramdisk_read((void *)(uintptr_t)phdr[i].p_vaddr, phdr[i].p_offset, phdr[i].p_filesz);
    fs_read(fd, (void *)(uintptr_t)phdr[i].p_vaddr, phdr[i].p_filesz);
    memset((void *)(uintptr_t)(phdr[i].p_vaddr + phdr[i].p_filesz), 0,
           phdr[i].p_memsz - phdr[i].p_filesz);
  }

  return ehdr->e_entry;
}
#endif

#if 0 // 旧实现
static uintptr_t loader(PCB *pcb, const char *filename)
{
  // TODO();

  Elf32_Ehdr *header_buf = (Elf32_Ehdr *)&ramdisk_start;
  ramdisk_read(header_buf, 0, sizeof(Elf32_Ehdr));
  // TODO: check out the magic
  unsigned char magic[EI_NIDENT] = {};
  // for (int i = 0; i < EI_NIDENT; i++)
  //{
  // uint8_t *pmagic = *(uint8_t *)header_buf;
  // magic[i] = *pmagic;
  // pmagic++;
  //}
  // Use memcpy to move bit betweens buf and buf, buf and mem
  memcpy(magic, header_buf, EI_NIDENT);
  assert(magic[EI_MAG0] == ELFMAG0 &&
         magic[EI_MAG1] == ELFMAG1 &&
         magic[EI_MAG2] == ELFMAG2 &&
         magic[EI_MAG3] == ELFMAG3 &&
         magic[EI_CLASS] == 1 &&
         magic[EI_DATA] == ELFDATA2LSB &&
         magic[EI_VERSION] == EV_CURRENT &&
         magic[EI_OSABI] == ELFOSABI_NONE &&
         magic[EI_ABIVERSION] == 0 &&
         magic[EI_PAD] == 0 &&
         magic[EI_NIDENT] == EI_NIDENT);
  // TODO: select machine type
  Elf32_Half machine = *(Elf32_Half *)((uint8_t *)header_buf + MACHINE_OFFSET);
#if defined(__ISA_AM_NATIVE__)
#define EXPECT_TYPE EM_X86_64
#elif defined(__ISA_X86__)
#define EXPECT_TYPE EM_X86_64 // see /usr/include/elf.h to get the right type
#elif defined(__riscv)
#define EXPECT_TYPE EM_RISCV
#else
#error Unsupported ISA
#endif
  // can use header_buf-> to replace marco
  // Elf32_Off *phoff = (Elf32_Off *)((uint8_t *)header_buf + PROGRAM_HEADER_OFFSET);
  // Elf32_Half *phsize = (Elf32_Half *)((uint8_t *)header_buf + PROGRAM_SIZE_OFFSET);
  // Elf32_Half *phnum = (Elf32_Half *)((uint8_t *)header_buf + PROGRAM_NUM_OFFSET);
  Elf32_Off *phoff = header_buf->e_phoff;
  Elf32_Half *phsize = header_buf->e_phentsize;
  Elf32_Half *phnum = header_buf->e_phnum;
  printf("Program header offset = 0x%x\n", *phoff);
  printf("Program header size = 0x%x\n", *phsize);
  printf("Program header number = 0x%s\n", *phnum);

  // TODO: get all program header -> ramdisk_read()
  Elf32_Phdr *all_program = (Elf32_Phdr *)&ramdisk_start + *phoff;
  size_t all_program_len = ramdisk_read(all_program, 0, (*phsize) * (*phnum));
  int actual_load_num = 0;
  Elf32_Phdr program_of_vector[PROGRAM_TYPE_IS_LOAD_NUM] = {};
  // TODO: get suitable program header -> check out Type -> if(type == LOAD)
  for (int i = 0; i < (*phnum); i++)
  {
    // Why???
    // big pointer -> small pointer -> * --> yes
    // big pointer -> * -> small value type --> wrong
    // Why???
    // Elf32_Phdr* can point to a vector Eld32_Phdr
    // how I go through them?

    // I really not how to handle type convertion balabala...
    // uint32_t type = *(uint32_t *)all_program + (uint32_t)(*phsize);
    uint32_t type = all_program[i].p_type;
    if (type == PT_LOAD)
    {
      // program_of_vector[i] = (*all_program) + i * (*phsize); // why?
      program_of_vector[i] = all_program[i];
      actual_load_num++;
      assert(actual_load_num <= PROGRAM_TYPE_IS_LOAD_NUM);
    }
  }
  // TODO: get Offset, VirAddr, FileSiz, MenSiz
  Elf32_Off Offsets[actual_load_num];
  Elf32_Word VirAddrs[actual_load_num];
  Elf32_Word FileSizes[actual_load_num];
  Elf32_Word MemSizes[actual_load_num];
  for (int i = 0; i < actual_load_num; i++)
  {
    assert(program_of_vector != NULL);
    Elf32_Phdr Segment = program_of_vector[i];
    Offsets[i] = Segment.p_offset;
    VirAddrs[i] = Segment.p_vaddr;
    FileSizes[i] = Segment.p_filesz;
    MemSizes[i] = Segment.p_memsz;
    // TODO: Set zero
    set_zero(VirAddrs[i] + FileSizes[i], VirAddrs[i] + MemSizes[i]);
  }
  // TODO: move ramdisk to memory [VirAddr, VirAddr + MemSiz) and set bit 0 in [VirAddr + FileSiz, VirSiz + MemSiz)
  // how??
  // how to express addr in ramdisk_wirte second args? I don't konw
  ramdisk_read(&program_of_vector[0], , sizeof(Elf32_Phdr) * actual_load_num);

  return 0;
}
#endif

void naive_uload(PCB *pcb, const char *filename)
{
  uintptr_t entry = loader(pcb, filename);
  Log("Jump to entry = %p", entry);
  ((void (*)())entry)();
}
