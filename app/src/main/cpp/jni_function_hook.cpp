//
// Created by Fear1ess on 2021/2/1.

#include <cstdio>
#include <cstring>
#include "hook.h"
#include "dobby.h"

#define TAG "nativehook123_hookjni"
#ifdef __cplusplus
extern "C" {
#endif

jclass method_cls;
jclass class_cls;
jclass hook_cls;
jmethodID getname_method;
jmethodID getparamshorty_method;
jmethodID getdeclaringclass_method;
jmethodID getmethodsig_method;
jmethodID getmethodname_method;
jmethodID gettype_method;
static bool is_hook_jni = false;

/*
void log_static_method_info(JNIEnv *env, jclass thiz, jmethodID method_id, jstring logInfo, va_list params) {
    if (gettype_method == method_id || getparamshorty_method == method_id || getmethodname_method == method_id) return;
    jobject method = env->ToReflectedMethod(thiz, method_id, true);
    jint param_count = env->CallStaticIntMethod(hook_cls, getparamcount_method, method);
    jclass obj_cls = env->FindClass("java/lang/Object");
    jobjectArray paramArray = env->NewObjectArray(param_count, obj_cls, NULL);
    logd(TAG, "param_count: %d", param_count);
    for(int i = 0; i < param_count; ++i) {
        jobject item = va_arg(params, jobject);
        if(!item) logd(TAG, "objArr item is null");
        env->SetObjectArrayElement(paramArray, i, item);
    }
    env->CallStaticVoidMethod(hook_cls, logmethodinfo_method, (jobject)thiz, method, logInfo, paramArray);
    env->DeleteLocalRef(method);
    env->DeleteLocalRef(paramArray);
    env->DeleteLocalRef(obj_cls);
}*/

void log_method_info(JNIEnv *env, jobject thiz, jmethodID method_id, va_list params, const char* callInfo) {
    env->PushLocalFrame(40);
    jclass cls = env->GetObjectClass(thiz);
    jobject method = env->ToReflectedMethod(cls, method_id, false);
    jobject shortyStr = env->CallStaticObjectMethod(hook_cls, getparamshorty_method, method);
    const char* shorty = env->GetStringUTFChars((jstring)shortyStr, 0);
    jobject methodStr = env->CallStaticObjectMethod(hook_cls, getmethodname_method, method);
    const char* method_name = env->GetStringUTFChars((jstring)methodStr, 0);
    jobject classStr = env->CallStaticObjectMethod(hook_cls, gettype_method, cls);
    const char* class_name = env->GetStringUTFChars((jstring)classStr, 0);
    jobject sigStr = env->CallStaticObjectMethod(hook_cls, getmethodsig_method, method);
    const char* sig_str = env->GetStringUTFChars((jstring)sigStr, 0);
    char call_method_str[256] = {0};
    sprintf(call_method_str, "%s -> %s%s   shorty:%s", class_name, method_name, sig_str, shorty);
    logd(TAG, "%s, %s", callInfo, call_method_str);
    for(int i = 1; i < strlen(shorty); ++i) {
        switch(shorty[i]) {
            case 'Z': va_arg(params, jboolean);
        }
    }
    env->PopLocalFrame(NULL);
}

/*
jobject (*ori_CallStaticObjectMethodV)(JNIEnv *env, jclass thiz, jmethodID method_id, va_list list);
jobject fake_CallStaticObjectMethodV(JNIEnv *env, jclass thiz, jmethodID method_id, va_list list) {
    jstring loginfo = env->NewStringUTF("CallStaticObjectMethodV");
    va_list list2;
    va_copy(list2, list);
    jobject res = ori_CallStaticObjectMethodV(env, thiz, method_id, list);
    log_static_method_info(env, thiz, method_id, loginfo, list2);
    env->DeleteLocalRef(loginfo);
    return res;
}*/

jobject (*ori_CallObjectMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jobject fake_CallObjectMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    const char* call_info = "CallObjectMethodV";
    va_list list2;
    va_copy(list2, list);
    jobject res = ori_CallObjectMethodV(env, thiz, method_id, list);
    log_method_info(env, thiz, method_id, list2, call_info);
    return res;
}

jbyteArray (*ori_NewByteArray)(JNIEnv* env, jsize size);
jbyteArray fake_NewByteArray(JNIEnv* env, jsize size) {
    logd(TAG, "NewByteArray, size: %d", size);
    return ori_NewByteArray(env, size);
}


void hook_jni_function(JNIEnv *env) {
    if(is_hook_jni) return;
    logd(TAG, "start hook jni...");
    is_hook_jni = true;

 //   DobbyHook((void *)env->functions->CallStaticObjectMethodV, (void *)fake_CallStaticObjectMethodV,(void **)&ori_CallStaticObjectMethodV);
    DobbyHook((void *)env->functions->CallObjectMethodV, (void *)fake_CallObjectMethodV,(void **)&ori_CallObjectMethodV);
    DobbyHook((void *)env->functions->NewByteArray, (void *)fake_NewByteArray,(void **)&ori_NewByteArray);
}

void unhook_jni_function(JNIEnv* env) {
    if(!is_hook_jni) return;
    logd(TAG, "stop hook jni...");
  //  DobbyDestroy((void*)env->functions->CallStaticVoidMethodV);
 //   DobbyDestroy((void*)env->functions->NewByteArray);
    is_hook_jni = false;
}

void jni_hook_init(JNIEnv* env) {
    jclass method_cls_local = env->FindClass("java/lang/reflect/Method");
    jclass class_cls_local = env->FindClass("java/lang/Class");
    jclass hook_cls_local = env->FindClass("com/fear1ess/xposednativehook/HookEntry");
    //   jclass hook_cls_local = env->FindClass("com/fear1ess/xposednativehook/MainActivity");
    method_cls = (jclass) (env->NewGlobalRef(method_cls_local));
    class_cls = (jclass) (env->NewGlobalRef(class_cls_local));
    hook_cls = (jclass) (env->NewGlobalRef(hook_cls_local));
    env->DeleteLocalRef(method_cls_local);
    env->DeleteLocalRef(class_cls_local);
    env->DeleteLocalRef(hook_cls_local);
    getname_method = env->GetMethodID(method_cls, "getName", "()Ljava/lang/String;");
    getparamshorty_method = env->GetStaticMethodID(hook_cls, "getMethodShorty", "(Ljava/lang/reflect/Method;)Ljava/lang/String;");
    getdeclaringclass_method = env->GetMethodID(method_cls, "getDeclaringClass",
                                                "()Ljava/lang/Class;");
    getmethodname_method = env->GetStaticMethodID(hook_cls, "getMethodName","(Ljava/lang/reflect/Method;)Ljava/lang/String;");
    gettype_method = env->GetStaticMethodID(hook_cls, "getType","(Ljava/lang/Class;)Ljava/lang/String;");
    getmethodsig_method = env->GetStaticMethodID(hook_cls, "getMethodSig","(Ljava/lang/reflect/Method;)Ljava/lang/String;");
}

#ifdef __cplusplus
}
#endif