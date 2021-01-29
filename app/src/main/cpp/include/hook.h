//
// Created by Fear1ess on 2021/1/21.
//

#ifndef XPOSEDNATIVEHOOK_HOOK_H
#define XPOSEDNATIVEHOOK_HOOK_H

#ifdef __cplusplus
extern "C" {
#endif

void *fake_dlopen(const char *libpath, int flags);

void *fake_dlsym(void *handle, const char *name);

int fake_dlclose(void *handle);

struct ctx {
    void *load_addr;
    void *dynstr;
    void *dynsym;
    void *symtab;
    void *strtab;
    int nsymtabs;
    int nsyms;
    off_t bias;
};

#ifdef __cplusplus
}
#endif


#endif //XPOSEDNATIVEHOOK_HOOK_H
