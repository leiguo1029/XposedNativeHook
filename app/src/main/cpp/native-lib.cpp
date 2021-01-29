#include <jni.h>
#include <android/log.h>
#include <dlfcn.h>
#include <string.h>
#include <cstdio>
#include <asm/mman.h>
#include "include/hook.h"
#include "include/dobby.h"

#define TAG "nativehook123_nativelog"
#include "syscall.h"
#include "sys/socket.h"
#define logd(fmt,args...) __android_log_print(ANDROID_LOG_DEBUG, TAG, fmt, ##args)

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
    logd("decStr: %s", res);
    return res;
}

void* (*ori_sendfunc)(void* arg1, void* arg2, void* arg3, void* arg4);
void* fake_sendfunc(void* arg1, void* arg2, void* arg3, void* arg4){
    logd("sendfunc called....");
    ori_sendfunc(arg1, arg2, arg3, arg4);
}

void (*ori_init)();
void fake_init() {
    ctx* du_handle = (ctx*)fake_dlopen("libdu.so", RTLD_LAZY);
    void* decstr = pointer_add(du_handle->load_addr, 0x176e5);
    uint32_t checksum1 = *(uint32_t*)decstr;
    logd("decstr checksum1: %d", checksum1);
    ori_init();
    uint32_t checksum2 = *(uint32_t*)decstr;
    logd("decstr checksum2: %d", checksum2);
    DobbyHook(decstr, (void*)fake_decstr, (void**)&ori_decstr);
}

void* (*ori_dlopen_ext)(char*, int, void*);
void* fake_dlopen_ext(char* libPath, int mode, void* extInfo){
 //   logd("loadlibrary by android_dlopen_ext: %s", libPath);
    void* handle =  ori_dlopen_ext(libPath, mode, extInfo);
    if(strcmp(package_name, "com.festearn.likeread") == 0) {
        if(strstr(libPath, "libflutter.so")) {
            ctx *so_info = (ctx *)fake_dlopen("libflutter.so", RTLD_LAZY);
            logd("libflutter.so load addr: %p, bias: %p", so_info->load_addr, so_info->bias);
            void *hook_func = pointer_add(so_info->load_addr, 0x5873d4);
            logd("libflutter.so hook func: %p", hook_func);
            DobbyHook(hook_func, (void *) my_func, (void **) &ori_func);
        }
    }
    if(strstr(libPath, "libdu.so")) {
            ctx* du_handle = (ctx*)fake_dlopen("libdu.so", RTLD_LAZY);
            logd("libdu.so load addr: %p", du_handle->load_addr);
        void* decstr = pointer_add(du_handle->load_addr, 0x176e5);
        uint32_t checksum = *(uint32_t*)decstr;
        logd("decstr checksum: %d", checksum);
        DobbyHook(decstr, (void*)fake_decstr, (void**)&ori_decstr);

        void* sendfunc = pointer_add(du_handle->load_addr, 0x1f01d);
        DobbyHook(sendfunc, (void*)fake_sendfunc, (void**)&ori_sendfunc);
        }

    return handle;
}

void* (*ori_dexfile)(void* thiz, void* dex_file, size_t len, void* arg4, void* arg5, void* arg6);
void* fake_dexfile(void* thiz, void* dex_file, size_t len, void* arg4, void* arg5, void* arg6) {
    logd("load dexfile addr=%p, len=%lx", dex_file, len);
    return ori_dexfile(thiz, dex_file, len, arg4, arg5, arg6);
}

int (*ori_sendto)(int fd, const void* const buf, size_t len, int flags, const struct sockaddr* dest_addr, socklen_t addr_len);
int fake_sendto(int fd, const void* const buf, size_t len, int flags, const struct sockaddr* dest_addr, socklen_t addr_len) {
    char* bytes = new char[3*len];
    for(int i = 0; i < len; ++i) {
        sprintf(bytes + 3*i, "%02x ", ((char*)buf)[i]);
    }
    logd("sendto: hex --> %s", bytes);
    logd("sendto: buf --> %s", buf);
    delete bytes;
    return ori_sendto(fd, buf, len, flags, dest_addr, addr_len);
}

void* dexfile = NULL;

JNIEXPORT void Java_com_fear1ess_xposednativehook_HookEntry_doNativeHook(JNIEnv *env, jobject thiz, jstring pkg_name_jstr) {
    jboolean isCopy = 0;
    const char *pkg_name = env->GetStringUTFChars(pkg_name_jstr, &isCopy);
    logd("hahaha current pkg_name: %s", pkg_name);
    size_t len = strlen(pkg_name);
    if(len >= 100) {
        len = 99;
    }
    memcpy(package_name, pkg_name, len);
    if (strcmp(package_name, "com.taou.maimai") == 0) {
        void* art_handle = fake_dlopen("libart.so", RTLD_LAZY);
        dexfile = fake_dlsym(art_handle, "_ZN3art7DexFileC2EPKhjRKNSt3__112basic_stringIcNS3_11char_traitsIcEENS3_9allocatorIcEEEEjPKNS_10OatDexFileE");
     //   void* dexfile = fake_dlsym(art_handle, "_ZN3art7DexFile10OpenCommonEPKhjRKNSt3__112basic_stringIcNS3_11char_traitsIcEENS3_9allocatorIcEEEEjPKNS_10OatDexFileEbbPS9_PNS0_12VerifyResultE");
        logd("dexfile offset: 0x%lx", (char*)dexfile - (char*)((ctx*)art_handle)->load_addr);
        DobbyHook(dexfile, (void*)fake_dexfile, (void**)&ori_dexfile);
      //  DobbyHook((void*)sendto, (void*)fake_sendto, (void**)&ori_sendto);
    }
    env->ReleaseStringUTFChars(pkg_name_jstr, pkg_name);
}

static int canDump = 0;

JNIEXPORT void JNICALL
Java_com_fear1ess_xposednativehook_HookEntry_getDexFileNative(JNIEnv *env, jobject thiz, jlong dex_file_ptr) {
    void* dex_file = (void*) dex_file_ptr;
    void* base = value_at(dex_file, 4);
    size_t len = (size_t)value_at(dex_file, 8);
    logd("dex base: %p, len: 0x%lx", base, len);
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

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    logd("%s", "hello world haha123");
    void* dl_handle = fake_dlopen("libdl.so", RTLD_LAZY);
    void* dlopen_ext = fake_dlsym(dl_handle,"android_dlopen_ext");
    if(!dlopen_ext) {
        logd("%s", "dlopen_ext is null!!!");
        return JNI_VERSION_1_6;
    }
    DobbyHook((void*)dlopen_ext,(void*)fake_dlopen_ext,(void**)&ori_dlopen_ext);
    return JNI_VERSION_1_6;
}

#ifdef __cplusplus
}
#endif
