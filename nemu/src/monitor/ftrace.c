#include "ftrace.h"
#include <stdio.h>
#include <stdlib.h>
#include <elf.h>

typedef struct
{
    uint32_t addr;
    uint32_t inst;
    const char *name;
} FuncSym;
// static FuncSym *funcsym = NULL;

static void get_func(size_t seek_size, size_t size, size_t nmenb, void *ret_ptr, FILE *fp)
{
    if (!fseek(fp, seek_size, SEEK_CUR))
    {
        fprintf(stdout, "Succeed to call fseek func\n");
    }
    size_t ret = fread(ret_ptr, size, nmenb, fp);
    if (ret != size * nmenb)
    {
        fprintf(stderr, "fread() failed: %zu\n", ret);
    }
}

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
    get_func(0, sizeof(char), 16, magic, fp);
    // size_t ret = fread(magic, sizeof(char), 16, fp);
    // if (ret != sizeof(char) * 16)
    // {
    //     fprintf(stderr, "fread() failed: %zu\n", ret);

    //     // nemu.state = NEMU_ABORT;
    //     // return -1;
    // }

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
    // get entry
    Elf32_Addr *entry = NULL;
    get_func(sizeof(Elf32_Half) * 2 + sizeof(Elf32_Word), sizeof(Elf32_Addr), 1, entry, fp);
    printf("entry = 0x%08x\n", *entry);
    // if (!fseek(fp, sizeof(Elf32_Half) * 2 + sizeof(Elf32_Word), SEEK_CUR))
    // {
    //     fprintf(stdout, "Succeed to call fseek func\n");
    // }
    // size_t entry_ret = fread(&entry, sizeof(Elf32_Addr), 1, fp);
    // if (entry_ret != sizeof(Elf32_Addr) * 1)
    // {
    //     fprintf(stderr, "fread() failed: %zu\n", entry_ret);
    // }

    // get_shs
    Elf32_Off *shoff = NULL;
    get_func(sizeof(Elf32_Off), sizeof(Elf32_Off), 1, shoff, fp);
    printf("section header start = 0x%08x\n", *shoff);
    // if (!fseek(fp, sizeof(Elf32_Off), SEEK_CUR))
    // {
    //     fprintf(stdout, "Succeed to call fseek func\n");
    // }
    // size_t shoff_ret = fread(&shoff, sizeof(Elf32_Off), 1, fp);
    // if (shoff_ret != sizeof(Elf32_Off) * 1)
    // {
    //     fprintf(stderr, "fread() failed: %zu\n", shoff_ret);
    // }

    // get_sofsh
    Elf32_Off *shentsize = NULL;
    get_func(sizeof(Elf32_Word) + sizeof(Elf32_Half) * 3, sizeof(Elf32_Off), 1, shentsize, fp);
    printf("section header start = 0x%08x\n", *shentsize);
    // if (!fseek(fp, sizeof(Elf32_Word) + sizeof(Elf32_Half) * 3, SEEK_CUR))
    // {
    //     fprintf(stdout, "Succeed to call fseek func\n");
    // }
    // size_t shentsize_ret = fread(&shentsize, sizeof(Elf32_Off), 1, fp);
    // if (shentsize_ret != sizeof(Elf32_Off) * 1)
    // {
    //     fprintf(stderr, "fread() failed: %zu\n", shentsize_ret);
    // }

    // get_nofsh
    // if (!fseek(fp, sizeof(Elf32_Off), SEEK_CUR))
    // {
    //     fprintf(stdout, "Suceed to call fseek func\n");
    // }
    Elf32_Off *nofsh = NULL;
    get_func(0, sizeof(Elf32_Half), 1, nofsh, fp);
    printf("section header start = 0x%08x\n", *nofsh);
    // size_t nofsh_ret = fread(&nofsh, sizeof(Elf32_Half), 1, fp);
    // if (nofsh_ret != sizeof(Elf32_Half) * 1)
    // {
    //     fprintf(stderr, "fread() failed: %zu\n", nofsh_ret);
    // }

    // get_shstrndx
    Elf32_Off *shstrndx = NULL;
    get_func(sizeof(Elf32_Off), sizeof(Elf32_Off), 1, shstrndx, fp);
    printf("section header start = 0x%08x\n", *shstrndx);
    // if (!fseek(fp, sizeof(Elf32_Off), SEEK_CUR))
    // {
    //     fprintf(stdout, "Succeed to call fseek func\n");
    // }
    // size_t shstrndx_ret = fread(&shstrndx, sizeof(Elf32_Off), 1, fp);
    // if (shstrndx_ret != sizeof(Elf32_Off) * 1)
    // {
    //     fprintf(stderr, "fread() failed: %zu\n", shstrndx_ret);
    // }
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