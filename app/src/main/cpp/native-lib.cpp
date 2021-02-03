#include <jni.h>
#include <dlfcn.h>
#include <string.h>
#include <cstdio>
#include <asm/mman.h>
#include "include/hook.h"
#include "include/dobby.h"


#include "syscall.h"
#include "sys/socket.h"

#define TAG "nativehook123_nativelog"
#ifdef __cplusplus
extern "C" {
#endif

char package_name[100] = {0};
int index = 0;

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
    if(strcmp(res, "api1.shuzilm.cn") == 0) {
        _Unwind_Backtrace(unwind_backtrace_callback, NULL);
    }
    return res;
}

jstring (*ori_query)(JNIEnv* env, jclass cls, jobject cxt, jstring str1, jstring str2);
jstring fake_query(JNIEnv* env, jclass cls, jobject cxt, jstring str1, jstring str2) {
    hook_jni_function(env);
    jstring res = ori_query(env, cls, cxt, str1, str2);
    unhook_jni_function(env);
    const char* smid = env->GetStringUTFChars(res, 0);
    logd(TAG, "get smid: %s", smid);
    env->ReleaseStringUTFChars(res, smid);
    return res;
}

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
        ctx* du_handle = (ctx*)fake_dlopen("libdu.so", RTLD_LAZY);
        logd(TAG, "libdu.so load addr: %p", du_handle->load_addr);
        void* decstr = pointer_add(du_handle->load_addr, 0x176e5);
        DobbyHook(decstr, (void*)fake_decstr, (void**)&ori_decstr);
        void* query = pointer_add(du_handle->load_addr, 0x23f79);
        DobbyHook(query, (void*)fake_query, (void**)&ori_query);
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
        logd("nativehook123_unwind", "unwind --> module: %s, offset: 0x%x(%s + 0x%x)", info.dli_fname, pc - (uintptr_t)info.dli_fbase,
                info.dli_sname, pc - (uintptr_t)info.dli_saddr);
    }
    return _URC_NO_REASON;
}

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    void* dl_handle = fake_dlopen("libdl.so", RTLD_LAZY);
    void* dlopen_ext = fake_dlsym(dl_handle,"android_dlopen_ext");
    void* crypto_handle = fake_dlopen("libcrypto.so", RTLD_LAZY);
    logd(TAG, "ssl_handle: %p", crypto_handle);
    void* X509_verify = fake_dlsym(crypto_handle, "X509_verify");
    if(!dlopen_ext) {
        logd(TAG, "%s", "dlopen_ext is null!!!");
        return JNI_VERSION_1_6;
    }
    logd(TAG, "x509_verify: %p", X509_verify);
    DobbyHook((void*)dlopen_ext,(void*)fake_dlopen_ext,(void**)&ori_dlopen_ext);
    DobbyHook((void*)X509_verify, (void*)fake_X509_verify, (void**)&ori_X509_verify);

    JNIEnv* env = NULL;
    vm->GetEnv((void**)&env, JNI_VERSION_1_6);
    if(env != NULL) {
        jni_hook_init(env);
    }
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL
Java_com_fear1ess_xposednativehook_HookEntry_startHookJni(JNIEnv *env, jobject thiz) {
    hook_jni_function(env);
}

JNIEXPORT void JNICALL
Java_com_fear1ess_xposednativehook_HookEntry_stopHookJni(JNIEnv *env, jobject thiz) {
    unhook_jni_function(env);
}

#ifdef __cplusplus
}
#endif