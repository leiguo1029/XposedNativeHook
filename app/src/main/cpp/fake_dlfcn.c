#include <stdio.h>
#include <elf.h>
#include <android/log.h>
#include <malloc.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define TAG_NAME	"fake_dlfcn"


#define log_info(fmt,args...) __android_log_print(ANDROID_LOG_INFO, TAG_NAME, (const char *) fmt, ##args)
#define log_err(fmt,args...) __android_log_print(ANDROID_LOG_ERROR, TAG_NAME, (const char *) fmt, ##args)


#ifdef LOG_DBG
#define log_dbg log_info
#else
#define log_dbg(...)
#endif

#ifdef __arm__
#define Elf_Ehdr Elf32_Ehdr
#define Elf_Phdr Elf32_Phdr
#define Elf_Shdr Elf32_Shdr
#define Elf_Sym  Elf32_Sym
#elif defined(__aarch64__)
#define Elf_Ehdr Elf64_Ehdr
#define Elf_Phdr Elf64_Phdr
#define Elf_Shdr Elf64_Shdr
#define Elf_Sym  Elf64_Sym
#else
#error "Arch unknown, please port me"
#endif

struct ctx {
    void *base;
    void *load_addr;
    void *dynstr;
    void *dynsym;
    void *symtab;
    void* strtab;
    int nsymtabs;
    int nsyms;
    off_t bias;
    size_t mem_size;
};


int fake_dlclose(void *handle)
{
    if(handle) {
        struct ctx *ctx = (struct ctx *) handle;
        if(ctx->dynsym) free(ctx->dynsym);	/* we're saving dynsym and dynstr */
        if(ctx->dynstr) free(ctx->dynstr);	/* from library file just in case */
        if(ctx->symtab) free(ctx->symtab);	/* from library file just in case */
        free(ctx);
    }
    return 0;
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "err_ovl_ambiguous_call"
/* flags are ignored */

void *fake_dlopen(const char *libpath, int flags)
{
    FILE *maps;
    char buff[256];
    struct ctx *ctx = 0;
    off_t load_addr, size;
    int k, fd = -1, found = 0;
    void *shoff;
    Elf_Ehdr *elf = MAP_FAILED;

#define fatal(fmt,args...) do { log_err(fmt,##args); goto err_exit; } while(0)

    maps = fopen("/proc/self/maps", "r");
    if(!maps) fatal("failed to open maps");

    while(!found && fgets(buff, sizeof(buff), maps))
        //if(strstr(buff,"r-xp") && strstr(buff,libpath)) found = 1;
        if(strstr(buff,libpath)) found = 1;

    fclose(maps);

    if(!found) fatal("%s not found in my userspace", libpath);

    long end_addr = 0;
    long len = 0;
    int int1 = 0;
    int int2 = 0;
    int int3 = 0;
    char attr[16] = {0};
    char path[256] = {0};
    if(sscanf(buff, "%lx-%lx%s%lx%x:%x%x%s", &load_addr, &end_addr, attr, &len, &int1, &int2, &int3, path) != 8)
        fatal("failed to read load address for %s", libpath);

    log_info("%s loaded in Android at 0x%08lx", libpath, load_addr);

    /* Now, mmap the same library once again */

    fd = open(path, O_RDONLY);
    if(fd < 0) fatal("failed to open %s", path);

    size = lseek(fd, 0, SEEK_END);
    if(size <= 0) fatal("lseek() failed for %s", libpath);

    elf = (Elf_Ehdr *) mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    fd = -1;

    if(elf == MAP_FAILED) fatal("mmap() failed for %s", libpath);

    ctx = (struct ctx *) calloc(1, sizeof(struct ctx));
    if(!ctx) fatal("no memory for %s", libpath);

    ctx->load_addr = (void *) load_addr;
   // libart_start=load_addr;
    shoff = ((void *) elf) + elf->e_shoff;
    Elf_Shdr *shstrndx = ((Elf_Shdr *) shoff+elf->e_shstrndx);
    char* shstr=((void *) elf) + shstrndx->sh_offset;

    void* phoff = ((void *) elf) + elf->e_phoff;
    size_t load_size = 0;
    int find_bias = 0;
    for(int j = 0; j < elf->e_phnum; j++, phoff += elf->e_phentsize) {
        Elf_Phdr* ph = phoff;
        if(ph->p_type == PT_LOAD) {
            if(!find_bias) {
                ctx->bias = ph->p_vaddr;
                ctx->base = ctx->load_addr - ctx->bias;
                find_bias = 1;
            }
            size_t addr = ph->p_vaddr + ph ->p_memsz;
            if(load_size < addr) {
                load_size = addr;
            }
        }
    }
    ctx->mem_size = load_size;

    log_info("ctx->bias: 0x%x, ctx->mem_size: 0x%x", ctx->bias, ctx->mem_size);

    for(k = 0; k < elf->e_shnum; k++, shoff += elf->e_shentsize)  {

        Elf_Shdr *sh = (Elf_Shdr *) shoff;
        log_dbg("%s: k=%d shdr=%p type=%x", __func__, k, sh, sh->sh_type);
        char* shname = shstr+sh->sh_name;
        if(strcmp(shname,".symtab") == 0){
            if(ctx->symtab) fatal("%s: duplicate SYMTAB sections", libpath); /* .dynsym */
            ctx->symtab = malloc(sh->sh_size);
            if(!ctx->symtab) fatal("%s: no memory for .symtab", libpath);
            memcpy(ctx->symtab, ((void *) elf) + sh->sh_offset, sh->sh_size);
            ctx->nsymtabs = (sh->sh_size/sizeof(Elf_Sym)) ;
        }else if(strcmp(shname,".strtab")==0){
            ctx->strtab = malloc(sh->sh_size);
            if(!ctx->strtab) fatal("%s: no memory for .strtab", libpath);
            memcpy(ctx->strtab, ((void *) elf) + sh->sh_offset, sh->sh_size);
        }else if(strcmp(shname,".dynsym")==0){
            if(ctx->dynsym) fatal("%s: duplicate DYNSYM sections", libpath); /* .dynsym */
            ctx->dynsym = malloc(sh->sh_size);
            if(!ctx->dynsym) fatal("%s: no memory for .dynsym", libpath);
            memcpy(ctx->dynsym, ((void *) elf) + sh->sh_offset, sh->sh_size);
            ctx->nsyms = (sh->sh_size/sizeof(Elf_Sym)) ;
        }else if(strcmp(shname,".dynstr")==0){
            ctx->dynstr = malloc(sh->sh_size);
            if(!ctx->dynstr) fatal("%s: no memory for .dynstr", libpath);
            memcpy(ctx->dynstr, ((void *) elf) + sh->sh_offset, sh->sh_size);
        }else if(strcmp(shname,".text")==0){
          //  ctx->bias = (off_t) sh->sh_addr - (off_t) sh->sh_offset;
        }

        /*
        switch(sh->sh_type) {

            case SHT_SYMTAB:
                if(ctx->symtab) fatal("%s: duplicate SYMTAB sections", libpath);
                ctx->symtab = malloc(sh->sh_size);
                if(!ctx->symtab) fatal("%s: no memory for .symtab", libpath);
                memcpy(ctx->symtab, ((void *) elf) + sh->sh_offset, sh->sh_size);
                ctx->nsymtabs = (sh->sh_size/sizeof(Elf_Sym)) ;
                break;



            case SHT_DYNSYM:
                if(ctx->dynsym) fatal("%s: duplicate DYNSYM sections", libpath);
                ctx->dynsym = malloc(sh->sh_size);
                if(!ctx->dynsym) fatal("%s: no memory for .dynsym", libpath);
                memcpy(ctx->dynsym, ((void *) elf) + sh->sh_offset, sh->sh_size);
                ctx->nsyms = (sh->sh_size/sizeof(Elf_Sym)) ;
                break;

            case SHT_STRTAB:
                if(ctx->dynstr) break;
                ctx->dynstr = malloc(sh->sh_size);
                if(!ctx->dynstr) fatal("%s: no memory for .dynstr", libpath);
                memcpy(ctx->dynstr, ((void *) elf) + sh->sh_offset, sh->sh_size);
                break;

            case SHT_PROGBITS:
                if(!ctx->dynstr || !ctx->dynsym||!ctx->symtab) break;

                ctx->bias = (off_t) sh->sh_addr - (off_t) sh->sh_offset;
                k = elf->e_shnum;
                break;
        }*/
    }

    munmap(elf, size);
    elf = 0;

    if(!ctx->dynstr || !ctx->dynsym) fatal("dynamic sections not found in %s", libpath);

#undef fatal

    log_dbg("%s: ok, dynsym = %p, dynstr = %p", libpath, ctx->dynsym, ctx->dynstr);

    return ctx;

    err_exit:
    if(fd >= 0) close(fd);
    if(elf != MAP_FAILED) munmap(elf, size);
    fake_dlclose(ctx);
    return 0;
}
#pragma clang diagnostic pop

void *fake_dlsym(void *handle, const char *name)
{
    int k;
    struct ctx *ctx = (struct ctx *) handle;
    Elf_Sym *sym = (Elf_Sym *) ctx->dynsym;
    char *strings = (char *) ctx->dynstr;

    for(k = 0; k < ctx->nsyms; k++, sym++)
        if(strcmp(strings + sym->st_name, name) == 0) {
            /*  NB: sym->st_value is an offset into the section for relocatables,
            but a VMA for shared libs or exe files, so we have to subtract the bias */
            void *ret = ctx->load_addr + sym->st_value - ctx->bias;
            log_info("%s found at %p", name, ret);
            return ret;
        }

    strings = ctx->strtab;
    sym = (Elf_Sym *) ctx->symtab;
    for(int m=0;m<ctx->nsymtabs;m++,sym++)
        if(strcmp(strings + sym->st_name, name) == 0) {
            /*  NB: sym->st_value is an offset into the section for relocatables,
            but a VMA for shared libs or exe files, so we have to subtract the bias */
            void *ret = ctx->load_addr + sym->st_value - ctx->bias;
            log_info("%s found at %p", name, ret);
            return ret;
        }
    return 0;
}

