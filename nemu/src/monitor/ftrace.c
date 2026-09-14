#include <common.h>
#include <elf.h>
#include <cpu/decode.h>
#include "ftrace.h"

typedef struct
{
    word_t addr;
    word_t size;
    char *name;
} FuncSym;
static FuncSym *funcs = NULL;
static int nr_func = 0;

// qsort 比较函数: 按函数起始地址升序
// 注意不能写成 a->addr - b->addr: word_t 是无符号数, 减法可能回绕
static int cmp_by_addr(const void *a, const void *b)
{
    word_t x = ((const FuncSym *)a)->addr, y = ((const FuncSym *)b)->addr;
    return x < y ? -1 : x > y;
}

void init_ftrace(const char *elf_file)
{
    if (elf_file == NULL)
    {
        Log("ftrace: no ELF file, only addresses will be traced");
        return;
    }

    FILE *fp = fopen(elf_file, "r");
    Assert(fp, "ftrace: can not open ELF file '%s'", elf_file);

    Elf32_Ehdr ehdr;
    Assert(fread(&ehdr, sizeof(ehdr), 1, fp) == 1, "ftrace: read ELF header failed");
    Assert(ehdr.e_ident[EI_MAG0] == 0x7f && ehdr.e_ident[EI_MAG1] == 'E' &&
               ehdr.e_ident[EI_MAG2] == 'L' && ehdr.e_ident[EI_MAG3] == 'F' &&
               ehdr.e_ident[EI_CLASS] == ELFCLASS32,
           "ftrace: not a 32-bit ELF file");

    // 1. 读整个节头表
    Elf32_Shdr *shdrs = malloc(ehdr.e_shentsize * ehdr.e_shnum);
    fseek(fp, ehdr.e_shoff, SEEK_SET);
    Assert(fread(shdrs, ehdr.e_shentsize, ehdr.e_shnum, fp) == ehdr.e_shnum, "read shdrs failed");

    // 2. 找 .symtab, 并经 sh_link 拿到字符串表
    Elf32_Shdr *symtab = NULL, *strtab = NULL;
    // for (int i = 0; i < ehdr.e_shnum; i++)
    //     if (shdrs[i].sh_type == SHT_SYMTAB)
    //     {
    //         symtab = &shdrs[i];
    //         strtab = &shdrs[symtab->sh_link];
    //         break;
    //     }
    // .symtab .strtab 通常在节头表的末尾, 所以从后往前找
    for (int i = ehdr.e_shnum; i > 0; i--)
    {
        if (shdrs[i - 1].sh_type == SHT_SYMTAB)
        {
            symtab = &shdrs[i - 1];
            strtab = &shdrs[symtab->sh_link];
            break;
        }
    }
    Assert(symtab, "ftrace: no .symtab in '%s' (stripped?)", elf_file);

    // 3. 把两个节的内容读进内存
    Elf32_Sym *syms = malloc(symtab->sh_size);
    fseek(fp, symtab->sh_offset, SEEK_SET);
    Assert(fread(syms, symtab->sh_size, 1, fp) == 1, "ftrace: read symtab failed");
    char *strs = malloc(strtab->sh_size);
    fseek(fp, strtab->sh_offset, SEEK_SET);
    Assert(fread(strs, strtab->sh_size, 1, fp) == 1, "ftrace: read strtab failed");
    fclose(fp);

    // 4. 过滤出函数符号
    int cnt = symtab->sh_size / symtab->sh_entsize;
    funcs = malloc(cnt * sizeof(FuncSym));
    for (int i = 0; i < cnt; i++)
    {
        if (ELF32_ST_TYPE(syms[i].st_info) == STT_FUNC && syms[i].st_value != 0)
        {
            funcs[nr_func].addr = syms[i].st_value;
            funcs[nr_func].size = syms[i].st_size;
            funcs[nr_func].name = strdup(&strs[syms[i].st_name]);
            nr_func++;
        }
    }
    // 5. 按地址排序, 供"包含查询"使用
    qsort(funcs, nr_func, sizeof(FuncSym), cmp_by_addr);
    Log("ftrace: loaded %d function symbols from %s", nr_func, elf_file);
}

// 返回包含 addr 的函数符号; 没有精确命中时取起始地址 <= addr 的最近一个
static const FuncSym *sym_lookup(word_t addr)
{
    for (int i = nr_func - 1; i >= 0; i--)
        if (funcs[i].addr <= addr)
            return &funcs[i];
    return NULL;
}

// ***  格式化输出  ***
static int depth = 0;

void ftrace_call(vaddr_t pc, vaddr_t target)
{
    const FuncSym *f = sym_lookup(target);
    if (f)
        log_write(FMT_WORD ": %*scall [%s@" FMT_WORD "]\n", pc, depth * 2, "", f->name, target);
    else
        log_write(FMT_WORD ": %*scall [?@" FMT_WORD "]\n", pc, depth * 2, "", target);
    depth++; // 先按当前深度缩进打印, 再进层
}

void ftrace_ret(vaddr_t pc)
{
    depth--;                           // 先退层, 再打印
    const FuncSym *f = sym_lookup(pc); // ret 指令所在的函数 = 正在返回的函数
    if (f)
        log_write(FMT_WORD ": %*sret  [%s]\n", pc, depth * 2, "", f->name);
    else
        log_write(FMT_WORD ": %*sret  [?]\n", pc, depth * 2, "");
}

// ftrace.c —— 在这条指令执行完后被调用, s->dnpc 已就绪
void ftrace_step(Decode *s)
{
    uint32_t i = s->isa.inst;
    int opcode = i & 0x7f;
    int rd = BITS(i, 11, 7);
    int rs1 = BITS(i, 19, 15);

    if (opcode == 0x6f && rd == 1) // jal ra
        ftrace_call(s->pc, s->dnpc);
    else if (opcode == 0x67)
    { // jalr
        if (rd == 0 && rs1 == 1)
            ftrace_ret(s->pc); // ret
        else if (rd == 1)
            ftrace_call(s->pc, s->dnpc); // 间接调用
    }
}
