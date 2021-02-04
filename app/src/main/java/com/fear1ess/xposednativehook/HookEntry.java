package com.fear1ess.xposednativehook;

import android.app.slice.Slice;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Debug;
import android.os.Handler;
import android.util.Log;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Type;

import de.robv.android.xposed.IXposedHookLoadPackage;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedHelpers;
import de.robv.android.xposed.callbacks.XC_LoadPackage;

public class HookEntry implements IXposedHookLoadPackage {
    private final String TAG = "nativehook123_java";
    private final String hookPluginSoName = "haha123";
    private Context cxt;
    private Object app;
    private String pkgName;
    private String processName;
    private ClassLoader cl;
    private volatile boolean startTrace = false;
    private volatile boolean isAppHooked = false;
    @Override
    public void handleLoadPackage(XC_LoadPackage.LoadPackageParam lpparam) throws Throwable {

        processName = (String)Class.forName("android.app.ActivityThread").getDeclaredMethod("currentProcessName").invoke(null);
        if(!processName.equals("com.taou.maimai") && !(processName.equals("com.festearn.likeread"))) return;
        pkgName = lpparam.packageName;
        cl = lpparam.classLoader;
        XposedHelpers.findAndHookMethod("android.app.Application", cl, "attach",
                Context.class, new XC_MethodHook() {
                    @Override
                    protected void afterHookedMethod(MethodHookParam param) throws Throwable {
                        if(isAppHooked) return;
                        cxt = (Context) param.args[0];
                        app = param.thisObject;
                        if(cxt != null && app != null) {
                            Log.d(TAG, "start native hook");
                            if(!hookAppNativeLib(pkgName)){
                                Log.d(TAG, "native hook failed");
                                return;
                            }
                            Log.d(TAG, "native hook success");
                            if(processName.equals("com.taou.maimai")){
                                startHookJava();
                            }
                        }
                        isAppHooked = true;
                    }
                });
    }

    public void startHookJava() {
        Intent intent = cxt.getPackageManager().getLaunchIntentForPackage(pkgName);
        ComponentName cn = intent.getComponent();
        String className = cn.getClassName();
        Log.d(TAG, "main activity clsname: " + className);
        try {
            Class<?> cls = ClassLoader.class.getClassLoader().loadClass("java.lang.ClassLoader");
        } catch (ClassNotFoundException e) {
            e.printStackTrace();
        }
        XposedHelpers.findAndHookMethod("java.lang.ClassLoader", ClassLoader.class.getClassLoader(), "loadClass",
                String.class, new XC_MethodHook() {
            @Override
            protected void beforeHookedMethod(MethodHookParam param) throws Throwable {
                super.beforeHookedMethod(param);
                String clsName = (String) param.args[0];
                if(clsName.equals(className)) {
                    Log.d(TAG, "load main activity class...");
                    cl = (ClassLoader) param.thisObject;
                    //mInitialApplication
                    Object app = Class.forName("android.app.ActivityThread").getDeclaredMethod("currentApplication").invoke(null);
                    String appClsName = app.getClass().getName();
                    Log.d(TAG, "applicationClsName: " + appClsName);
                    Class<?> duhelper = null;
                    try{
                        duhelper = cl.loadClass("cn.shuzilm.core.DUHelper");
                    }catch (Exception e){
                        Log.d(TAG, "find duhelper: " + e.getMessage());
                    }
                    Class<?> duhelper2 = duhelper;
                    /*
                    XposedHelpers.findAndHookMethod(duhelper, "query",
                            Context.class, String.class, String.class, new XC_MethodHook() {
                                @Override
                                protected void beforeHookedMethod(MethodHookParam param) throws Throwable {
                                    startHookJni();
                                }

                                @Override
                                protected void afterHookedMethod(MethodHookParam param) throws Throwable {
                                    stopHookJni();
                                    Log.d(TAG, "call shumeng query...");
                                    int mMeic = XposedHelpers.getStaticIntField(duhelper2, "mMeic");
                                    int mPopu = XposedHelpers.getStaticIntField(duhelper2, "mPopu");
                                    int mPort = XposedHelpers.getStaticIntField(duhelper2, "mPort");
                                    Log.d(TAG, "mMeic: " + mMeic + ", mPopu: " + mPopu + ", mPort: " + mPort);
                                    Log.d(TAG, "query str1: " + (String)param.args[1]);
                                    Log.d(TAG, "query str2: " + (String)param.args[2]);
                                    Log.d(TAG, "query res: " + (String)param.getResult());
                                }
                            });*/
                  //  getDexFile(param.thisObject);
                }
            }
        });



        XposedHelpers.findAndHookMethod("com.android.org.conscrypt.ConscryptFileDescriptorSocket$SSLOutputStream", cl, "write",
                byte[].class, int.class, int.class, new XC_MethodHook() {
                    @Override
                    protected void beforeHookedMethod(MethodHookParam param) throws Throwable {
                        byte[] data = (byte[]) param.args[0];
                        int pos = (int) param.args[1];
                        int len = (int) param.args[2];
                        String payload = new String(data, pos, len);
                        Log.d(TAG, "SSLOutputSteam data: " + payload);
                    }
                });
    }

    public void getDexFile(Object cl) {
        Object pathList = XposedHelpers.getObjectField(cl, "pathList");
        Object[] dexElements = (Object[]) XposedHelpers.getObjectField(pathList, "dexElements");
        for(Object element : dexElements) {
            Object dexFile = XposedHelpers.getObjectField(element, "dexFile");
            long[] mCookie = (long[]) XposedHelpers.getObjectField(dexFile, "mCookie");
            if(mCookie.length < 2) {
                Log.d(TAG, "has no dex file...");
                return;
            }
            for(int i = 0; i < mCookie.length; ++i) {
                long dexFilePtr = mCookie[i];
                Log.d(TAG, "dexFilePtr: " + dexFilePtr);
                if(dexFilePtr != 0) {
                    getDexFileNative(dexFilePtr);
                }
            }
        }
    }

    private native void getDexFileNative(long dexFilePtr);

    public boolean hookAppNativeLib(String pkgName) {
        String appSoDir = getNativeLibDir(cxt);
        if(!new File(appSoDir).exists()){
            appSoDir = getNativeLib64Dir(cxt);
            if(!new File(appSoDir).exists()){
                Log.d(TAG, "app has no native lib");
                return false;
            }
        }
        String destSoPath = appSoDir + "/lib" + hookPluginSoName + ".so";
        Log.d(TAG, "destSoPath: " + destSoPath);
        if(!load(destSoPath)) return false;
      //  System.loadLibrary(hookPluginSoName);
        doNativeHook(pkgName);
        return true;
    }

    public boolean load(String libraryPath) {
        File fi = new File(libraryPath);
        if(!fi.exists()) {
            Log.d(TAG, "lib not exists...");
            return false;
        }
        if (libraryPath == null) {
            return false;
        }
        try {
            System.load(libraryPath);
            return true;
        } catch (Throwable e) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                try {

                    Method forName = Class.class.getDeclaredMethod("forName", String.class);
                    Method getDeclaredMethod = Class.class.getDeclaredMethod("getDeclaredMethod", String.class, Class[].class);
                    Class<?> systemClass = (Class<?>) forName.invoke(null, "java.lang.System");
                    Method load = (Method) getDeclaredMethod.invoke(systemClass, "load", new Class[]{String.class});
                    load.invoke(systemClass, libraryPath);
                    return true;

                } catch (Throwable ex) {
                    Log.e(TAG, "load library failed:", ex);
                }
            } else {
                Log.e(TAG, "load library failed:", e);
            }
        }
        return false;
    }

    public String getNativeLibDir(Context cxt) {
        String prp = cxt.getPackageResourcePath();
        try {
            return new File(prp + "/../lib/arm").getCanonicalPath();
        } catch (IOException e) {
            e.printStackTrace();
        }
        return null;
    }

    public String getNativeLib64Dir(Context cxt) {
        String prp = cxt.getPackageResourcePath();
        try {
            return new File(prp + "/../lib/arm64").getCanonicalPath();

        } catch (IOException e) {
            e.printStackTrace();
        }
        return null;
    }

    public native void doNativeHook(String pkgName);
    public native void startHookJni();
    public native void stopHookJni();

    public static String getMethodName(Method m) {
        return m.getName();
    }

    public static char getShortyLetter(Class<?> cls) {
        switch(cls.getName()){
            case "boolean": return 'Z';
            case "byte": return 'B';
            case "char": return 'C';
            case "short": return 'S';
            case "int": return 'I';
            case "float": return 'F';
            case "double": return 'D';
            case "long": return 'J';
            case "void": return 'V';
        }
        return 'L';
    }

    public static String getMethodShorty(Method m) {
        Class<?>[] paramsCls = m.getParameterTypes();
        Class<?> retCls = m.getReturnType();
        String shorty = "";
        shorty += getShortyLetter(retCls);
        for(Class<?> cls : paramsCls) {
            shorty += getShortyLetter(cls);
        }
        return shorty;
    }

    public static String getMethodSig(final Method method) {
        final StringBuffer buf = new StringBuffer();
        buf.append("(");
        final Class<?>[] types = method.getParameterTypes();
        for (int i = 0; i < types.length; ++i) {
            buf.append(getDesc(types[i]));
        }
        buf.append(")");
        buf.append(getDesc(method.getReturnType()));
        return buf.toString();
    }
    public static String getDesc(Class type) {
        if (type.isPrimitive()) {
            return getPrimitiveLetter(type);
        }
        if (type.isArray()) {
            return "[" + getDesc(type.getComponentType());
        }
        return "L" + getType(type) + ";";
    }
    public static String getType(Class parameterType) {
        if (parameterType.isArray()) {
            return "[" + getDesc(parameterType.getComponentType());
        }
        if (!parameterType.isPrimitive()) {
            final String clsName = parameterType.getName();
            return clsName.replaceAll("\\.", "/");
        }
        return getPrimitiveLetter(parameterType);
    }
    public static String getPrimitiveLetter(Class<?> type) {
        if (int.class.equals(type)) {
            return "I";
        }
        if (void.class.equals(type)) {
            return "V";
        }
        if (boolean.class.equals(type)) {
            return "Z";
        }
        if (char.class.equals(type)) {
            return "C";
        }
        if (byte.class.equals(type)) {
            return "B";
        }
        if (short.class.equals(type)) {
            return "S";
        }
        if (float.class.equals(type)) {
            return "F";
        }
        if (long.class.equals(type)) {
            return "J";
        }
        if (double.class.equals(type)) {
            return "D";
        }
        throw new IllegalStateException("Type: " + type.getCanonicalName() + " is not a primitive type");
    }
    public static Type getMethodType(final Class<?> clazz, final String methodName) {
        try {
            final Method method = clazz.getMethod(methodName, (Class<?>[]) new Class[0]);
            return method.getGenericReturnType();
        }
        catch (Exception ex) {
            return null;
        }
    }
    public static Type getFieldType(final Class<?> clazz, final String fieldName) {
        try {
            final Field field = clazz.getField(fieldName);
            return field.getGenericType();
        }
        catch (Exception ex) {
            return null;
        }
    }

}
