#include <jni.h>
#include <dlfcn.h>
#include <string.h>
#include <cstdio>
#include <asm/mman.h>
#include <bits/signal_types.h>
#include <signal.h>
#include "include/hook.h"
#include "include/dobby.h"
#include <pthread.h>
#include <unistd.h>
#include "syscall.h"
#include "sys/socket.h"

#ifdef __arm__
#include "include/UnicornVM.h"
#endif
#ifdef __aarch64__
#include "include/UnicornVM64.h"
#endif

#define TAG "nativehook123_nativelog"
#ifdef __cplusplus
extern "C" {
#endif

char package_name[100] = {0};
int index = 0;

extern pthread_t hook_tid2;
ctx* du_ctx;

void* pointer_add(void *p, int i) {
    return (void *) ((char*) p + i);
}

void* value_at(void* p, int offset) {
    return (void *) (*(long*)((char*) p + offset));
}

int my_func() {
    logd("%s", "enter ssl verify...");
    return 1;
}
int (*ori_func)();

const char* (*ori_decstr)(void* arg);
const char* fake_decstr(void* arg) {
    const char* res = ori_decstr(arg);
    logd("nativehook123_decStr", "%s", res);
    return res;
}

jstring (*ori_query)(JNIEnv* env, jclass cls, jobject cxt, jstring str1, jstring str2);
jstring fake_query(JNIEnv* env, jclass cls, jobject cxt, jstring str1, jstring str2) {
    pthread_t tid = pthread_self();
 //   hook_jni_function(env, tid);
    jstring res = ori_query(env, cls, cxt, str1, str2);
  //  unhook_jni_function(env);
    const char* smid = env->GetStringUTFChars(res, 0);
    logd(TAG, "get smid: %s", smid);
    env->ReleaseStringUTFChars(res, smid);
    return res;
}

vc_callback_return_t unicornvm_callback(vc_callback_args_t *args){
    switch (args->op) {
        case vcop_read:
        case vcop_write: {
            break;
        }
        case vcop_call: {
            Dl_info info;
            const void* callee_addr = args->info.call.callee;
            /*
            if((uintptr_t)callee_addr > (uintptr_t)du_ctx->load_addr &&
                    (uintptr_t)callee_addr < (uintptr_t)du_ctx->base + du_ctx->mem_size) {
                uintptr_t offset = (uintptr_t)callee_addr - (uintptr_t)du_ctx->base;
               // dladdr((void*)args->info.call.callee, &info);
              //  uintptr_t so_offset = (uintptr_t)args->info.call.callee - (uintptr_t)info.dli_fbase;
             //   logd(TAG, "unicornvm call --> module: %s, offset: 0x%x(%s + 0x%x)", info.dli_fname, so_offset,
                    // info.dli_sname, (uintptr_t)args->info.call.callee - (uintptr_t)info.dli_saddr);
                return cbret_recursive;
            } */
            logd(TAG, "unicornvm call --> offset: 0x%x", (uintptr_t)callee_addr - (uintptr_t)du_ctx->base);
            return cbret_directcall;
        }
        case vcop_return: {
           // logd(TAG, "vcop return --> offset:  0x%x", (uintptr_t)args->info.ret.hitaddr - (uintptr_t)du_ctx->base);
            break;
        }
        case vcop_svc: {
            logd(TAG, "vcop syscall : syscall number %d",
                 args->info.svc.sysno);
            break;
        }
        case vcop_ifetch: {
            logd(TAG, "unicornvm ifetch --> offset: 0x%x", (uintptr_t)args->armctx->pc.w - (uintptr_t)du_ctx->base);
            break;
        }
        default: {
            logd(TAG, "unknown vcop %d.\n", args->op);
            break;
        }
    }
    return cbret_continue;
}

void (*ori_query_internal)(void* arg);
void fake_query_internal(void* arg){
    logd(TAG, "start query new thread...");
    pthread_t tid = pthread_self();
    hook_tid2 = tid;
  //  const void* vcall = vc_make_callee((void*)ori_query_internal, NULL, unicornvm_callback);
  //  ((void (*)(void*))vcall)(arg);
     ori_query_internal(arg);
}

int (*ori_query_internal_x)(void* arg1, void* arg2, void* arg3, void* arg4, void* arg5, void* arg6);
int fake_query_internal_x(void* arg1, void* arg2, void* arg3, void* arg4, void* arg5, void* arg6){
    const void* vcall = vc_make_callee((void*)ori_query_internal_x, NULL, unicornvm_callback);
    ((void (*)(void*, void*, void*, void*, void*, void*))vcall)(arg1, arg2, arg3, arg4, arg5, arg6);
   //return ori_query_internal_x(arg1, arg2, arg3, arg4);
}
1;

void* (*ori_dlopen_ext)(char*, int, void*);
void* fake_dlopen_ext(char* libPath, int mode, void* extInfo){
    logd(TAG, "loadlibrary by android_dlopen_ext: %s", libPath);
    void* handle =  ori_dlopen_ext(libPath, mode, extInfo);
    if(strcmp(package_name, "com.festearn.likeread") == 0) {
        if(strstr(libPath, "libflutter.so")) {
            ctx *so_info = (ctx *)fake_dlopen("libflutter.so", RTLD_LAZY);
            logd(TAG, "libflutter.so load addr: %p, bias: %p", so_info->load_addr, so_info->bias);
            void *hook_func = pointer_add(so_info->load_addr, 0x5873d4);
            logd(TAG, "libflutter.so hook func: %p", hook_func);
            DobbyHook(hook_func, (void *) my_func, (void **) &ori_func);
        }
    }
    if(strstr(libPath, "libdu.so")) {
        du_ctx = (ctx*)fake_dlopen("libdu.so", RTLD_LAZY);
        logd(TAG, "libdu.so load addr: %p", du_ctx->load_addr);
        void* decstr = pointer_add(du_ctx->load_addr, 0x176e5);
        DobbyHook(decstr, (void*)fake_decstr, (void**)&ori_decstr);
        void* query = pointer_add(du_ctx->load_addr, 0x23f79);
        DobbyHook(query, (void*)fake_query, (void**)&ori_query);
        void* query_internal =  pointer_add(du_ctx->load_addr, 0x21d71);
        DobbyHook(query_internal, (void*)fake_query_internal, (void**)&ori_query_internal);
        void* query_internal_x =  pointer_add(du_ctx->load_addr, 0x1f969);
        DobbyHook(query_internal_x, (void*)fake_query_internal_x, (void**)&ori_query_internal_x);
    }
    return handle;
}

void* (*ori_dexfile)(void* thiz, void* dex_file, size_t len, void* arg4, void* arg5, void* arg6);
void* fake_dexfile(void* thiz, void* dex_file, size_t len, void* arg4, void* arg5, void* arg6) {
    logd(TAG, "load dexfile addr=%p, len=%lx", dex_file, len);
    return ori_dexfile(thiz, dex_file, len, arg4, arg5, arg6);
}

int (*ori_sendto)(int fd, const void* const buf, size_t len, int flags, const struct sockaddr* dest_addr, socklen_t addr_len);
int fake_sendto(int fd, const void* const buf, size_t len, int flags, const struct sockaddr* dest_addr, socklen_t addr_len) {
    char* bytes = new char[3*len];
    for(int i = 0; i < len; ++i) {
        sprintf(bytes + 3*i, "%02x ", ((char*)buf)[i]);
    }
    logd(TAG, "sendto: hex --> %s", bytes);
    logd(TAG, "sendto: buf --> %s", buf);
    delete bytes;
    return ori_sendto(fd, buf, len, flags, dest_addr, addr_len);
}

void* dexfile = NULL;

JNIEXPORT void Java_com_fear1ess_xposednativehook_HookEntry_doNativeHook(JNIEnv *env, jobject thiz, jstring pkg_name_jstr) {
    jboolean isCopy = 0;
    const char *pkg_name = env->GetStringUTFChars(pkg_name_jstr, &isCopy);
    logd(TAG, "hahaha current pkg_name: %s", pkg_name);
    size_t len = strlen(pkg_name);
    if(len >= 100) {
        len = 99;
    }
    memcpy(package_name, pkg_name, len);

    if (strcmp(package_name, "com.taou.maimai") == 0) {
        /*
        void* art_handle = fake_dlopen("libart.so", RTLD_LAZY);
        dexfile = fake_dlsym(art_handle, "_ZN3art7DexFileC2EPKhjRKNSt3__112basic_stringIcNS3_11char_traitsIcEENS3_9allocatorIcEEEEjPKNS_10OatDexFileE");
        DobbyHook(dexfile, (void*)fake_dexfile, (void**)&ori_dexfile);*/
    }
    env->ReleaseStringUTFChars(pkg_name_jstr, pkg_name);
}

static int canDump = 0;

JNIEXPORT void JNICALL
Java_com_fear1ess_xposednativehook_HookEntry_getDexFileNative(JNIEnv *env, jobject thiz, jlong dex_file_ptr) {
    void* dex_file = (void*) dex_file_ptr;
    void* base = value_at(dex_file, 4);
    size_t len = (size_t)value_at(dex_file, 8);
    logd(TAG, "dex base: %p, len: 0x%lx", base, len);
    if(len == 0xb3d65c) canDump = 1;
    if(canDump && index < 4) {
        const char* path_format = "/sdcard/maimai-dex/%d.dex";
        char path[100] = {0};
        sprintf(path, path_format, index++);
        FILE* file = fopen(path, "wb+");
        fwrite(base, len, 1, file);
        fclose(file);
    }
}

int (*ori_X509_verify)(void* arg1, void* arg2);
int fake_X509_verify(void* arg1, void* arg2) {
    int res = ori_X509_verify(arg1, arg2);
    logd(TAG, "X509 verify res: %d", res);
    return res;
}

_Unwind_Reason_Code unwind_backtrace_callback(struct _Unwind_Context* context, void* arg) {
    _Unwind_Word pc = _Unwind_GetIP(context);
    if(pc) {
        Dl_info info;
        dladdr((void*)pc, &info);
        uintptr_t so_offset = pc - (uintptr_t)info.dli_fbase;
        if(so_offset == 0xd80de) {
            logd("nativehook123_hookjni", "unwind --> stop unwind");
            return _URC_NORMAL_STOP;
        }
        logd("nativehook123_hookjni", "unwind --> module: %s, offset: 0x%x(%s + 0x%x)", info.dli_fname, so_offset,
                info.dli_sname, pc - (uintptr_t)info.dli_saddr);
    }
    return _URC_NO_REASON;
}

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    void* dl_handle = fake_dlopen("libdl.so", RTLD_LAZY);
    void* dlopen_ext = fake_dlsym(dl_handle,"android_dlopen_ext");
    if(!dlopen_ext) {
        logd(TAG, "%s", "dlopen_ext is null!!!");
        return JNI_VERSION_1_6;
    }
    DobbyHook((void*)dlopen_ext,(void*)fake_dlopen_ext,(void**)&ori_dlopen_ext);

    /*
    struct sigaction sa_old, sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = my_signal_handler;
    sa.sa_flags = SA_SIGINFO;
    if (sigaction(SIGSEGV, &sa, &sa_old) == 0) {
        logd(TAG, "set segv sig handler success");
    }*/

    JNIEnv* env = NULL;
    vm->GetEnv((void**)&env, JNI_VERSION_1_6);
    if(env != NULL) {
        jni_hook_init(env);
    }
    return JNI_VERSION_1_6;
}

#ifdef __cplusplus
}
#endif