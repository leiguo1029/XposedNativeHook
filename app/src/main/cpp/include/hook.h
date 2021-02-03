//
// Created by Fear1ess on 2021/1/21.
//

#ifndef XPOSEDNATIVEHOOK_HOOK_H
#define XPOSEDNATIVEHOOK_HOOK_H

#include <jni.h>
#include <sys/types.h>
#include <android/log.h>
#include <unwind.h>

#define logd(tag, fmt, args...) __android_log_print(ANDROID_LOG_DEBUG, tag, fmt, ##args)

#ifdef __cplusplus
extern "C" {
#endif

void *fake_dlopen(const char *libpath, int flags);

void *fake_dlsym(void *handle, const char *name);

int fake_dlclose(void *handle);

void hook_jni_function(JNIEnv* env);
void unhook_jni_function(JNIEnv* env);
void jni_hook_init(JNIEnv* env);

_Unwind_Reason_Code unwind_backtrace_callback(struct _Unwind_Context* context, void* arg);

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
