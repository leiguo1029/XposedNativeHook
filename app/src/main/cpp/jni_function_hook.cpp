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

void log_method_info(JNIEnv *env, jboolean isStatic, jobject thiz, jmethodID method_id, va_list params, const char* resInfo) {
    env->PushLocalFrame(40);
    jclass cls;
    if(!isStatic) cls = env->GetObjectClass(thiz);
    else cls = (jclass)thiz;
    jobject method = env->ToReflectedMethod(cls, method_id, false);
    jobject methodStr = env->CallStaticObjectMethod(hook_cls, getmethodname_method, method);
    const char* method_name = env->GetStringUTFChars((jstring)methodStr, 0);
    jobject shortyStr = env->CallStaticObjectMethod(hook_cls, getparamshorty_method, method);
    const char* shorty = env->GetStringUTFChars((jstring)shortyStr, 0);
    jobject classStr = env->CallStaticObjectMethod(hook_cls, gettype_method, cls);
    const char* class_name = env->GetStringUTFChars((jstring)classStr, 0);
    jobject sigStr = env->CallStaticObjectMethod(hook_cls, getmethodsig_method, method);
    const char* sig_str = env->GetStringUTFChars((jstring)sigStr, 0);
    char call_method_str[1024] = {0};
    sprintf(call_method_str, "%s -> %s%s\t%s\n\t", class_name, method_name, sig_str, shorty);
    static bool has = false;
    if(strcmp(class_name, "org/json/JSONObject") == 0 &&
        strcmp(method_name, "toString") == 0 && !has) {
        FILE* fp = fopen("/sdcard/maimai-report","wb+");
        fwrite(resInfo, strlen(resInfo), 1, fp);
        fclose(fp);
        _Unwind_Backtrace(unwind_backtrace_callback, NULL);
        has = true;
    }
    int shorty_len = 0;
    if(sig_str != NULL) shorty_len = strlen(shorty);
    for(int i = 1; i < shorty_len; ++i) {
        char ss[256] = {0};
        memset(ss, 0, 256);
        jobject obj;
        switch(shorty[i]) {
            case 'Z':
                sprintf(ss, "arg%d: %d\t", i - 1, va_arg(params, jboolean));
                break;
            case 'C':
                sprintf(ss, "arg%d: %d\t", i - 1, va_arg(params, jchar));
                break;
            case 'B':
                sprintf(ss, "arg%d: %d\t", i - 1, va_arg(params, jbyte));
                break;
            case 'S':
                sprintf(ss, "arg%d: %d\t", i - 1, va_arg(params, jshort));
                break;
            case 'I':
                sprintf(ss, "arg%d: %d\t", i - 1, va_arg(params, jint));
                break;
            case 'J':
                sprintf(ss, "arg%d: %lld\t", i - 1, va_arg(params, jlong));
                break;
            case 'F':
                sprintf(ss, "arg%d: %f\t", i - 1, va_arg(params, jfloat));
                break;
            case 'D':
                sprintf(ss, "arg%d: %lf\t", i - 1, va_arg(params, jdouble));
                break;
            case 'L':
                obj = va_arg(params, jobject);
                if(env->IsInstanceOf(obj, env->FindClass("java/lang/String"))) {
                    const char* str = env->GetStringUTFChars((jstring)obj, 0);
                    snprintf(ss, 256, "arg%d: \"%s\"\t", i - 1, str);
                    env->ReleaseStringUTFChars((jstring)obj, str);
                }
                break;
            default: break;
        }
        strcat(call_method_str, ss);
    }
    logd(TAG, "%s\n\tres: %s\n", call_method_str, resInfo);
    env->ReleaseStringUTFChars((jstring)methodStr, method_name);
    env->ReleaseStringUTFChars((jstring)classStr, class_name);
    env->ReleaseStringUTFChars((jstring)sigStr, sig_str);
    env->ReleaseStringUTFChars((jstring)shortyStr, shorty);
    env->PopLocalFrame(NULL);
}


jboolean (*ori_CallBooleanMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jboolean fake_CallBooleanMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    JValue j;
    handleMethodCall(env, thiz, method_id, list, false, 'Z', &j);
    return j.z;
}
jboolean (*ori_CallStaticBooleanMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jboolean fake_CallStaticBooleanMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    JValue j;
    handleMethodCall(env, thiz, method_id, list, true, 'Z', &j);
    return j.z;
}

jbyte (*ori_CallByteMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jbyte fake_CallByteMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    JValue j;
    handleMethodCall(env, thiz, method_id, list, false, 'B', &j);
    return j.b;
}
jbyte (*ori_CallStaticByteMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jbyte fake_CallStaticByteMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    JValue j;
    handleMethodCall(env, thiz, method_id, list, true, 'B', &j);
    return j.b;
}

jchar (*ori_CallCharMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jchar fake_CallCharMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    JValue j;
    handleMethodCall(env, thiz, method_id, list, false, 'C', &j);
    return j.c;
}
jchar (*ori_CallStaticCharMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jchar fake_CallStaticCharMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    JValue j;
    handleMethodCall(env, thiz, method_id, list, true, 'C', &j);
    return j.c;
}

jshort (*ori_CallShortMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jshort fake_CallShortMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    JValue j;
    handleMethodCall(env, thiz, method_id, list, false, 'S', &j);
    return j.s;
}
jshort (*ori_CallStaticShortMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jshort fake_CallStaticShortMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    JValue j;
    handleMethodCall(env, thiz, method_id, list, true, 'S', &j);
    return j.s;
}

jint (*ori_CallIntMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jint fake_CallIntMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    JValue j;
    handleMethodCall(env, thiz, method_id, list, false, 'I', &j);
    return j.i;
}
jint (*ori_CallStaticIntMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jint fake_CallStaticIntMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    JValue j;
    handleMethodCall(env, thiz, method_id, list, true, 'I', &j);
    return j.i;
}

jlong (*ori_CallStaticLongMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jlong fake_CallStaticLongMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    JValue j;
    handleMethodCall(env, thiz, method_id, list, true, 'J', &j);
    return j.j;
}
jlong (*ori_CallLongMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jlong fake_CallLongMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    JValue j;
    handleMethodCall(env, thiz, method_id, list, false, 'J', &j);
    return j.j;
}

jfloat (*ori_CallFloatMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jfloat fake_CallFloatMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    JValue j;
    handleMethodCall(env, thiz, method_id, list, false, 'F', &j);
    return j.f;
}
jfloat (*ori_CallStaticFloatMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jfloat fake_CallStaticFloatMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    JValue j;
    handleMethodCall(env, thiz, method_id, list, true, 'F', &j);
    return j.f;
}

jdouble (*ori_CallDoubleMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jdouble fake_CallDoubleMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    JValue j;
    handleMethodCall(env, thiz, method_id, list, false, 'D', &j);
    return j.d;
}
jdouble (*ori_CallStaticDoubleMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jdouble fake_CallStaticDoubleMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    JValue j;
    handleMethodCall(env, thiz, method_id, list, true, 'D', &j);
    return j.d;
}


jobject (*ori_CallStaticObjectMethodV)(JNIEnv *env, jclass thiz, jmethodID method_id, va_list list);
jobject fake_CallStaticObjectMethodV(JNIEnv *env, jclass thiz, jmethodID method_id, va_list list) {
    JValue j;
    handleMethodCall(env, thiz, method_id, list, true, 'L', &j);
    return j.l;
}
jobject (*ori_CallObjectMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jobject fake_CallObjectMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    const char* call_info = "CallObjectMethodV";
    /*
    va_list list2;
    va_copy(list2, list);
    jobject res = ori_CallObjectMethodV(env, thiz, method_id, list);
    if(env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    if(env->IsInstanceOf(res, env->FindClass("java/lang/String"))) {
        const char* str = env->GetStringUTFChars((jstring)res, 0);
        log_method_info(env, false, thiz, method_id, list2, str);
        env->ReleaseStringUTFChars((jstring)res, str);
    }else {
        log_method_info(env, false, thiz, method_id, list2, NULL);
    }
    va_end(list2);*/
    JValue j;
    handleMethodCall(env, thiz, method_id, list, false, 'L', &j);
    return j.l;
}

jbyteArray (*ori_NewByteArray)(JNIEnv* env, jsize size);
jbyteArray fake_NewByteArray(JNIEnv* env, jsize size) {
    logd(TAG, "NewByteArray, size: %d", size);
    return ori_NewByteArray(env, size);
}

void handleMethodCall(JNIEnv* env, jobject thiz, jmethodID method_id, va_list list, bool isStatic, char retType, JValue* j) {
    va_list list2;
    va_copy(list2, list);
    switch(retType) {
        case 'L':
            j->l = isStatic ? ori_CallStaticObjectMethodV(env, (jclass)thiz, method_id, list) :
            ori_CallObjectMethodV(env, thiz, method_id, list);
            break;
        default: break;
    }
    if(env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    /*
    log_method_info(env, thiz, method_id, list2, isStatic, retType, j);*/
}


void hook_jni_function(JNIEnv *env) {
    if(is_hook_jni) return;
    logd(TAG, "start hook jni...");
    is_hook_jni = true;

    DobbyHook((void *)env->functions->CallStaticObjectMethodV, (void *)fake_CallStaticObjectMethodV, (void **)&ori_CallStaticObjectMethodV);
    DobbyHook((void *)env->functions->CallObjectMethodV, (void *)fake_CallObjectMethodV, (void **)&ori_CallObjectMethodV);
    DobbyHook((void *)env->functions->CallStaticIntMethodV, (void *)fake_CallIntMethodV, (void **)&ori_CallStaticIntMethodV);
    DobbyHook((void *)env->functions->CallIntMethodV, (void *)fake_CallStaticIntMethodV, (void **)&ori_CallIntMethodV);
 //   DobbyHook((void *)env->functions->NewByteArray, (void *)fake_NewByteArray,(void **)&ori_NewByteArray);
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