//
// Created by Fear1ess on 2021/2/1.

#include <cstdio>
#include <cstring>
#include <pthread.h>
#include "hook.h"
#include "dobby.h"

#define TAG "nativehook123_hookjni"
#define HookJniCallMethodV(type) DobbyHook((void *)env->functions->CallStatic##type##MethodV, (void *)fake_CallStatic##type##MethodV, (void **)&ori_CallStatic##type##MethodV); \
                                DobbyHook((void *)env->functions->Call##type##MethodV, (void *)fake_Call##type##MethodV, (void **)&ori_Call##type##MethodV)
#ifdef __cplusplus
extern "C" {
#endif

jclass method_cls;
jclass class_cls;
jclass hook_cls;
jmethodID getname_method;
jmethodID getdeclaringclass_method;
jmethodID getmethodsig_method;
jmethodID getmethodname_method;
jmethodID getmethodshorty_method;
jmethodID getinitsig_method;
jmethodID getinitname_method;
jmethodID getinitshorty_method;
jmethodID gettype_method;
static bool is_hook_jni = false;
pthread_t hook_tid;
pthread_t hook_tid2;

void get_param_valist_val_str(JNIEnv* env, va_list list, const char* shorty, char* param_str) {
    int shorty_len = 0;
    if(shorty != NULL) shorty_len = strlen(shorty);
    int n = 0;
    for(int i = 1; i < shorty_len; ++i) {
        char ss[256] = {0};
        memset(ss, 0, 256);
        jobject obj;
        switch(shorty[i]) {
            case 'Z':
                n += sprintf(ss, "arg%d: %d\t", i - 1, va_arg(list, jboolean));
                break;
            case 'C':
                n += sprintf(ss, "arg%d: %d\t", i - 1, va_arg(list, jchar));
                break;
            case 'B':
                n += sprintf(ss, "arg%d: %d\t", i - 1, va_arg(list, jbyte));
                break;
            case 'S':
                n += sprintf(ss, "arg%d: %d\t", i - 1, va_arg(list, jshort));
                break;
            case 'I':
                n += sprintf(ss, "arg%d: %d\t", i - 1, va_arg(list, jint));
                break;
            case 'J':
                n += sprintf(ss, "arg%d: %lld\t", i - 1, va_arg(list, jlong));
                break;
            case 'F':
                n += sprintf(ss, "arg%d: %f\t", i - 1,  va_arg(list, jfloat));
                break;
            case 'D':
                n += sprintf(ss, "arg%d: %lf\t", i - 1 , va_arg(list, jdouble));
                break;
            case 'L':
                obj = va_arg(list, jobject);
                if(obj != NULL && env->IsInstanceOf(obj, env->FindClass("java/lang/String")) == JNI_TRUE) {
                    const char* str = env->GetStringUTFChars((jstring)obj, 0);
                    n += snprintf(ss, 200,  "arg%d: \"%s\"\t", i - 1,  str);
                    env->ReleaseStringUTFChars((jstring)obj, str);
                }
                break;
            default: break;
        }
        if(n < 500){
            strcat(param_str, ss);
        }else{
            logd(TAG, "param is too long...");
            break;
        }
    }
}

void log_method_info(JNIEnv *env, jobject thiz, jmethodID method_id, va_list params, bool isStatic, bool isInit, const char* res) {
    if (gettype_method == method_id || getmethodshorty_method == method_id ||
        getmethodname_method == method_id || getmethodsig_method == method_id ||
        getinitname_method == method_id || getinitsig_method == method_id ||
        getinitshorty_method == method_id) return;
    jclass cls;
    if(!isStatic) cls = env->GetObjectClass(thiz);
    else cls = (jclass)thiz;
    jobject method = env->ToReflectedMethod(cls, method_id, false);
    jobject methodStr = env->CallStaticObjectMethod(hook_cls, isInit ? getinitname_method : getmethodname_method, method);
    const char* method_name = env->GetStringUTFChars((jstring)methodStr, 0);
    const char* method_name_final = isInit ? "<init>" : method_name;
    jobject shortyStr = env->CallStaticObjectMethod(hook_cls, isInit ? getinitshorty_method : getmethodshorty_method, method);
    const char* shorty = env->GetStringUTFChars((jstring)shortyStr, 0);
    jobject classStr = env->CallStaticObjectMethod(hook_cls, gettype_method, cls);
    const char* class_name = env->GetStringUTFChars((jstring)classStr, 0);
    jobject sigStr = env->CallStaticObjectMethod(hook_cls, isInit ? getinitsig_method : getmethodsig_method, method);
    const char* sig_str = env->GetStringUTFChars((jstring)sigStr, 0);
    char call_method_str[1024] = {0};
    sprintf(call_method_str, "%s -> %s%s\t%s\n\t", class_name, method_name_final, sig_str, shorty);
    /*
    static bool has = false;
    if(strcmp(class_name, "org/json/JSONObject") == 0 &&
        strcmp(method_name, "toString") == 0 && !has) {
        FILE* fp = fopen("/sdcard/maimai-report","wb+");
        fwrite(resInfo, strlen(resInfo), 1, fp);
        fclose(fp);
        has = true;
    }*/
    get_param_valist_val_str(env, params, shorty, call_method_str);
    logd(TAG, "%s\n\t%s\n", call_method_str, res);
    _Unwind_Backtrace(unwind_backtrace_callback, NULL);
    logd(TAG, "==================================================================");
    env->ReleaseStringUTFChars((jstring)methodStr, method_name);
    env->ReleaseStringUTFChars((jstring)classStr, class_name);
    env->ReleaseStringUTFChars((jstring)sigStr, sig_str);
    env->ReleaseStringUTFChars((jstring)shortyStr, shorty);
}


jboolean (*ori_CallBooleanMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jboolean fake_CallBooleanMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    jvalue j;
    handleMethodCall(env, thiz, method_id, list, false, false, 'Z', &j);
    return j.z;
}
jboolean (*ori_CallStaticBooleanMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jboolean fake_CallStaticBooleanMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    jvalue j;
    handleMethodCall(env, thiz, method_id, list, true, false, 'Z', &j);
    return j.z;
}


jbyte (*ori_CallByteMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jbyte fake_CallByteMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    jvalue j;
    handleMethodCall(env, thiz, method_id, list, false, false, 'B', &j);
    return j.b;
}
jbyte (*ori_CallStaticByteMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jbyte fake_CallStaticByteMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    jvalue j;
    handleMethodCall(env, thiz, method_id, list, true, false, 'B', &j);
    return j.b;
}

jchar (*ori_CallCharMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jchar fake_CallCharMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    jvalue j;
    handleMethodCall(env, thiz, method_id, list, false, false, 'C', &j);
    return j.c;
}
jchar (*ori_CallStaticCharMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jchar fake_CallStaticCharMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    jvalue j;
    handleMethodCall(env, thiz, method_id, list, true, false, 'C', &j);
    return j.c;
}

jshort (*ori_CallShortMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jshort fake_CallShortMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    jvalue j;
    handleMethodCall(env, thiz, method_id, list, false, false, 'S', &j);
    return j.s;
}
jshort (*ori_CallStaticShortMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jshort fake_CallStaticShortMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    jvalue j;
    handleMethodCall(env, thiz, method_id, list, true, false, 'S', &j);
    return j.s;
}

jint (*ori_CallIntMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jint fake_CallIntMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    jvalue j;
    handleMethodCall(env, thiz, method_id, list, false, false, 'I', &j);
    return j.i;
}
jint (*ori_CallStaticIntMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jint fake_CallStaticIntMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    jvalue j;
    handleMethodCall(env, thiz, method_id, list, true, false, 'I', &j);
    return j.i;
}

jlong (*ori_CallStaticLongMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jlong fake_CallStaticLongMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    jvalue j;
    handleMethodCall(env, thiz, method_id, list, true, false, 'J', &j);
    return j.j;
}
jlong (*ori_CallLongMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jlong fake_CallLongMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    jvalue j;
    handleMethodCall(env, thiz, method_id, list, false, false, 'J', &j);
    return j.j;
}

jfloat (*ori_CallFloatMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jfloat fake_CallFloatMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    jvalue j;
    handleMethodCall(env, thiz, method_id, list, false, false, 'F', &j);
    return j.f;
}
jfloat (*ori_CallStaticFloatMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jfloat fake_CallStaticFloatMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    jvalue j;
    handleMethodCall(env, thiz, method_id, list, true, false, 'F', &j);
    return j.f;
}

jdouble (*ori_CallDoubleMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jdouble fake_CallDoubleMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    jvalue j;
    handleMethodCall(env, thiz, method_id, list, false, false, 'D', &j);
    return j.d;
}
jdouble (*ori_CallStaticDoubleMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jdouble fake_CallStaticDoubleMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    jvalue j;
    handleMethodCall(env, thiz, method_id, list, true, false, 'D', &j);
    return j.d;
}


jobject (*ori_CallStaticObjectMethodV)(JNIEnv *env, jclass thiz, jmethodID method_id, va_list list);
jobject fake_CallStaticObjectMethodV(JNIEnv *env, jclass thiz, jmethodID method_id, va_list list) {
    jvalue j;
    handleMethodCall(env, thiz, method_id, list, true, false, 'L', &j);
    return j.l;
}
jobject (*ori_CallObjectMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jobject fake_CallObjectMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    jvalue j;
    handleMethodCall(env, thiz, method_id, list, false, false, 'L', &j);
    return j.l;
}

void (*ori_CallStaticVoidMethodV)(JNIEnv *env, jclass thiz, jmethodID method_id, va_list list);
void fake_CallStaticVoidMethodV(JNIEnv *env, jclass thiz, jmethodID method_id, va_list list) {
    handleMethodCall(env, thiz, method_id, list, true, false, 'V', NULL);
}
void (*ori_CallVoidMethodV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
void fake_CallVoidMethodV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    handleMethodCall(env, thiz, method_id, list, false, false, 'V', NULL);
}

jobject (*ori_NewObjectV)(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list);
jobject fake_NewObjectV(JNIEnv *env, jobject thiz, jmethodID method_id, va_list list) {
    jvalue j;
    handleMethodCall(env, thiz, method_id, list, true, true,  'L', &j);
    return j.l;
}

jbyteArray (*ori_NewByteArray)(JNIEnv* env, jsize size);
jbyteArray fake_NewByteArray(JNIEnv* env, jsize size) {
    logd(TAG, "NewByteArray, size: %d", size);
    return ori_NewByteArray(env, size);
}

void handleMethodCall(JNIEnv* env, jobject thiz, jmethodID method_id, va_list list, bool isStatic, bool isInit, char retType, jvalue* j) {
    va_list list2;
    va_copy(list2, list);
    char res[1024];
    switch(retType) {
        case 'Z':
            j->z = isStatic ? ori_CallStaticBooleanMethodV(env, (jclass)thiz, method_id, list) :
                   ori_CallBooleanMethodV(env, thiz, method_id, list);
            sprintf(res, "res: %d", j->z);
            break;
        case 'B':
            j->b = isStatic ? ori_CallStaticByteMethodV(env, (jclass)thiz, method_id, list) :
                   ori_CallByteMethodV(env, thiz, method_id, list);
            sprintf(res, "res: %d", j->b);
            break;
        case 'C':
            j->c = isStatic ? ori_CallStaticCharMethodV(env, (jclass)thiz, method_id, list) :
                   ori_CallCharMethodV(env, thiz, method_id, list);
            sprintf(res, "res: %d", j->c);
            break;
        case 'S':
            j->s = isStatic ? ori_CallStaticShortMethodV(env, (jclass)thiz, method_id, list) :
                   ori_CallShortMethodV(env, thiz, method_id, list);
            sprintf(res, "res: %d", j->s);
            break;
        case 'I':
            j->i = isStatic ? ori_CallStaticIntMethodV(env, (jclass)thiz, method_id, list) :
                   ori_CallIntMethodV(env, thiz, method_id, list);
            sprintf(res, "res: %d", j->i);
            break;
        case 'J':
            j->j = isStatic ? ori_CallStaticLongMethodV(env, (jclass)thiz, method_id, list) :
                   ori_CallLongMethodV(env, thiz, method_id, list);
            sprintf(res, "res: %d", j->j);
            break;
        case 'F':
            j->f = isStatic ? ori_CallStaticFloatMethodV(env, (jclass)thiz, method_id, list) :
                   ori_CallFloatMethodV(env, thiz, method_id, list);
            sprintf(res, "res: %d", j->f);
            break;
        case 'D':
            j->d = isStatic ? ori_CallStaticDoubleMethodV(env, (jclass)thiz, method_id, list) :
                   ori_CallDoubleMethodV(env, thiz, method_id, list);
            sprintf(res, "res: %d", j->d);
            break;
        case 'L':
            if(isInit) j->l = ori_NewObjectV(env, (jclass)thiz, method_id, list);
            else{
                j->l = isStatic ? ori_CallStaticObjectMethodV(env, (jclass)thiz, method_id, list) :
                       ori_CallObjectMethodV(env, thiz, method_id, list);
            }
            if(env->ExceptionOccurred()) {
                env->ExceptionClear();
            }
            if(env->IsInstanceOf(j->l, env->FindClass("java/lang/String")) == JNI_TRUE && j->l != NULL) {
                const char* str = env->GetStringUTFChars((jstring)j->l, 0);
                snprintf(res, 1024,  "res: %s",  str);
                env->ReleaseStringUTFChars((jstring)j->l, str);
            }else{
                sprintf(res, "res: object");
            }
            break;
        case 'V':
            isStatic ? ori_CallStaticVoidMethodV(env, (jclass)thiz, method_id, list) :
                   ori_CallVoidMethodV(env, thiz, method_id, list);
            sprintf(res, "res: null");
            break;
        default: break;
    }
    if(env->ExceptionOccurred()) {
        env->ExceptionClear();
    }
    pthread_t cur_tid = pthread_self();
    if(is_hook_jni && (cur_tid == hook_tid || cur_tid == hook_tid2)){
        log_method_info(env, thiz, method_id, list2, isStatic, isInit, res);
    }
    va_end(list2);
}


void hook_jni_function(JNIEnv *env, pthread_t tid) {
    if(is_hook_jni) return;
    logd(TAG, "start hook jni...");
    is_hook_jni = true;

    hook_tid = tid;

    // hook Method
    HookJniCallMethodV(Void);
    HookJniCallMethodV(Boolean);
    HookJniCallMethodV(Byte);
    HookJniCallMethodV(Char);
    HookJniCallMethodV(Short);
    HookJniCallMethodV(Int);
    HookJniCallMethodV(Long);
    HookJniCallMethodV(Float);
    HookJniCallMethodV(Double);
    HookJniCallMethodV(Object);

    //hook Init
    DobbyHook((void*)env->functions->NewObjectV, (void*)fake_NewObjectV, (void**)&ori_NewObjectV);
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
    getdeclaringclass_method = env->GetMethodID(method_cls, "getDeclaringClass",
                                                "()Ljava/lang/Class;");
    getname_method = env->GetMethodID(method_cls, "getName", "()Ljava/lang/String;");
    gettype_method = env->GetStaticMethodID(hook_cls, "getType","(Ljava/lang/Class;)Ljava/lang/String;");
    getmethodshorty_method = env->GetStaticMethodID(hook_cls, "getMethodShorty", "(Ljava/lang/reflect/Method;)Ljava/lang/String;");
    getmethodname_method = env->GetStaticMethodID(hook_cls, "getMethodName","(Ljava/lang/reflect/Method;)Ljava/lang/String;");
    getmethodsig_method = env->GetStaticMethodID(hook_cls, "getMethodSig","(Ljava/lang/reflect/Method;)Ljava/lang/String;");
    getinitname_method = env->GetStaticMethodID(hook_cls, "getConstructorName", "(Ljava/lang/reflect/Constructor;)Ljava/lang/String;");
    getinitshorty_method = env->GetStaticMethodID(hook_cls, "getConstructorShorty","(Ljava/lang/reflect/Constructor;)Ljava/lang/String;");
    getinitsig_method = env->GetStaticMethodID(hook_cls, "getConstructorSig","(Ljava/lang/reflect/Constructor;)Ljava/lang/String;");
}

#ifdef __cplusplus
}
#endif