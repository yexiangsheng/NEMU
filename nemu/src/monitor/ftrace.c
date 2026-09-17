#include "ftrace.h"
#include <stdio.h>
#include <stdlib.h>
#include <elf.h>

typedef struct
{
    const char *addr;
    const char *name;
    const char *inst;
} FuncSym;

FuncSym *funcsym = NULL;
/* get_symbol_sec = e_entry + e_shoff + e_shnum * e_shstrndx */

int init_ftrace(const char *elf_file)
{
    FILE *fp = fopen(elf_file, "r");
    if (!fp)
    {
        perror("fopen");
        return -1;
    }

    // 1. check magic
    unsigned char magic[EI_NIDENT];
    size_t ret = fread(magic, sizeof(char), 16, fp);
    if (ret != sizeof(char) * 16)
    {
        fprintf(stderr, "fread() failed: %zu\n", ret);

        // nemu.state = NEMU_ABORT;
        // return -1;
    }

    if (magic[EI_MAG0] == ELFMAG0 && magic[EI_MAG1] == ELFMAG1 && magic[EI_MAG2] == ELFMAG2 && magic[EI_MAG3] == ELFMAG3)
    {
        fprintf(stdout, "On Successed, It is ELF format!\n");
        if (magic[EI_CLASS] == ELFCLASS32)
        {
            fprintf(stdout, "ELF class is: ELF32\n");
        }
        else
        {
            fprintf(stdout, " Other elf class");
        }
    }
    else
    {
        fprintf(stdout, "On Failure, It is not ELF format!\n");
    }

    // 2. get symbol table
    if (!fseek(fp, sizeof(Elf32_Half) * 2 + sizeof(Elf32_Word), SEEK_CUR))
    {
        fprintf(stdout, "Succeed to call fseek func\n");
    }
    unsigned char get_entry[32];
    size_t entry_ret = fread(get_entry, sizeof(Elf32_Addr), 1, fp);
    if (entry_ret != sizeof(Elf32_Addr) * 1)
    {
        fprintf(stderr, "fread() failed: %zu\n", entry_ret);
    }

    Elf32_Addr entry = (uint32_t)*get_entry;
    printf("entry = %o", entry);

    // Elf32_Addr *entry = NULL;
    // size_t entry_ret = fread(entry, sizeof(Elf32_Addr), 1, fp);
    // if (ret != sizeof(Elf32_Addr))
    // {
    //     fprintf(stderr, "fread() failed: %zu\n", entry_ret);

    //     // nemu.state = NEMU_ABORT;
    //     // return -1;
    // }
    // fprintf(stdout, "ELF entry is: %o", *entry);
    // 3. get string table

    return 0;
}

/* fread EXAMPLES */
// #include <stdio.h>
// #include <stdlib.h>

// #define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

// int main(void)
// {
//     FILE *fp = fopen("/bin/sh", "rb");
//     if (!fp)
//     {
//         perror("fopen");
//         return EXIT_FAILURE;
//     }

//     unsigned char buffer[4];
//     size_t ret = fread(buffer, ARRAY_SIZE(buffer), sizeof(*buffer), fp);
//     if (ret != sizeof(*buffer))
//     {
//         fprintf(stderr, "fread() failed: %zu\n", ret);
//         exit(EXIT_FAILURE);
//     }

//     printf("ELF magic: %#04x%02x%02x%02x\n", buffer[0], buffer[1],
//            buffer[2], buffer[3]);

//     ret = fread(buffer, 1, 1, fp);
//     if (ret != 1)
//     {
//         fprintf(stderr, "fread() failed: %zu\n", ret);
//         exit(EXIT_FAILURE);
//     }

//     printf("Class: %#04x\n", buffer[0]);

//     fclose(fp);

//     exit(EXIT_SUCCESS);
// }