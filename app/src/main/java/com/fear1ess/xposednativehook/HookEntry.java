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
import java.lang.reflect.Method;

import de.robv.android.xposed.IXposedHookLoadPackage;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedHelpers;
import de.robv.android.xposed.callbacks.XC_LoadPackage;

public class HookEntry implements IXposedHookLoadPackage {
    private final String TAG = "nativehook123";
    private final String hookPluginSoName = "haha123";
    private Context cxt;
    private Object app;
    private String pkgName;
    private String processName;
    private ClassLoader cl;
    private volatile boolean startTrace = false;
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
                        super.afterHookedMethod(param);
                        cxt = (Context) param.args[0];
                        app = param.thisObject;
                        if(cxt != null && app != null) {
                            Log.d("nativehook123", "start native hook");
                            if(!hookAppNativeLib(pkgName)){
                                Log.d("nativehook123", "native hook failed");
                                return;
                            }
                            Log.d("nativehook123", "native hook success");
                            if(processName.equals("com.taou.maimai")){
                                startHookJava();
                                /*
                                new Thread(){
                                    @Override
                                    public void run() {
                                        Debug.startMethodTracingSampling("/sdcard/ccc.trace", 102400, 1000);
                                        try {
                                            Thread.sleep(30*1000);
                                        } catch (InterruptedException e) {
                                            e.printStackTrace();
                                        }
                                        Debug.stopMethodTracing();
                                    }
                                }.start();*/
                            }
                        }
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
            if(cls == null) Log.d(TAG, "classloader cls is null");
            else Log.d(TAG, "classloader cls is not null");
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
                    //mInitialApplication
                    Object app = Class.forName("android.app.ActivityThread").getDeclaredMethod("currentApplication").invoke(null);
                    String appClsName = app.getClass().getName();
                    Log.d(TAG, "appClsName: " + appClsName);
                  //  getDexFile(param.thisObject);
                }
            }
        });

        XposedHelpers.findAndHookMethod("cn.shuzilm.core.DUHelper", cl, "query",
                Context.class, String.class, String.class, new XC_MethodHook() {
                    @Override
                    protected void beforeHookedMethod(MethodHookParam param) throws Throwable {
                        Log.d(TAG, "call shumeng query...");
                                    /*
                                    Class<?> cls = cl.loadClass("cn.shuzilm.core.DUHelper");
                                        int mMeic = XposedHelpers.getStaticIntField(cls, "mMeic");
                                        int mPopu = XposedHelpers.getStaticIntField(cls, "mPopu");
                                        int mPort = XposedHelpers.getStaticIntField(cls, "mPort");
                                    Log.d(TAG, "mMeic: " + mMeic + ", mPopu: " + mPopu + ", mPort: " + mPort);*/
                        Log.d(TAG, "str1: " + (String)param.args[1]);
                        Log.d(TAG, "str2: " + (String)param.args[2]);

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
}
