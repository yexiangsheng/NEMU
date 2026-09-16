#include "ftrace.h"
#include <stdio.h>
#include <stdlib.h>
#include <elf.h>

/* get_symbol_sec = e_entry + e_shoff + e_shnum * e_shstrndx */

int init_ftrace(const char *elf_file)
{
    FILE *fp = fopen(elf_file, "w");
    if (!fp)
    {
        perror("fopen");
        return -1;
    }

    // check magic
    unsigned char magic[EI_NIDENT];
    size_t ret = fread(magic, sizeof(char), 16, fp);
    if (ret != sizeof(*magic))
    {
        fprintf(stderr, "fread() failed: %zu\n", ret);
        // nemu.state = NEMU_ABORT;
        return -1;
    }

    if (magic[EI_MAG0] == ELFMAG0 && magic[EI_MAG1] == ELFMAG1 && magic[EI_MAG2] == ELFMAG2 && magic[EI_MAG3] == ELFMAG3)
    {
        fprintf(stdout, "On Successed, It is ELF format!");
    }
    else
    {
        fprintf(stdout, "On Failure, It is not ELF format!");
    }

    //
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