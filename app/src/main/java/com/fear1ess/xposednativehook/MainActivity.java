package com.fear1ess.xposednativehook;

import android.app.Activity;
import android.app.Application;
import android.content.Context;
import android.os.Bundle;
import android.util.Log;

import androidx.annotation.Nullable;

import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

public class MainActivity extends Activity {
    public static String[] libNameArr = new String[] {"haha123", "unicornvm"};
    static {
//        HookEntry h = new HookEntry();
 //       System.loadLibrary("haha123");
    }
    public class NativeLibInfo {
        public NativeLibInfo(String str, boolean bo) {
            libPath = str;
            is64Bit = bo;
        }
        String libPath;
        boolean is64Bit;
    }
    private ArrayList<String> hookPackagesName = new ArrayList<>();

    protected void initHookClasses() {
        hookPackagesName.add("com.festearn.likeread");
        hookPackagesName.add("com.taou.maimai");
    }

    protected boolean isHookPackage(String line) {
        for(String name : hookPackagesName) {
            if(line.contains(name)) {
                return true;
            }
        }
        return false;
    }

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        initHookClasses();
        String clsName = getSystemService(TELEPHONY_SERVICE).getClass().getName();
        Log.d("nativehook123_java", "tel clsName: " + clsName);
        Runtime r = Runtime.getRuntime();
        ArrayList<NativeLibInfo> libInfoArray = new ArrayList<>();
        try {
            for(String packageName : hookPackagesName) {
                Process p = r.exec("su -c find /data/app | grep " + packageName);
                InputStream is = p.getInputStream();
                InputStreamReader isr = new InputStreamReader(is);
                BufferedReader bufferedReader = new BufferedReader(isr);
                String line;
                while((line = bufferedReader.readLine()) != null) {
                    if(isHookPackage(line)){
                        boolean is64 = false;
                        String libPath = line + "/lib/arm";
                        if(!new File(libPath).exists()) {
                            libPath = line + "/lib/arm64";
                            if(!new File(libPath).exists()) {
                                Log.d("nativeohook123", "hook app has no lib path...");
                                return;
                            }
                            is64 = true;
                        }
                        libInfoArray.add(new NativeLibInfo(libPath, is64));
                        bufferedReader.close();
                        isr.close();
                        is.close();
                        break;
                    }
                }
            }
            Log.d("nativehook123", "onCreate: ");

            ZipFile zf = new ZipFile(getApkPath(this));
            Enumeration<?> entries = zf.entries();
            while(entries.hasMoreElements()) {
                ZipEntry entry = (ZipEntry) entries.nextElement();
                String name = entry.getName();
                FileOutputStream os = null;
                String libName = isCopyLib(name);
                if(libName != null) {
                    if(name.contains("armeabi-v7a")) {
                        String unzipSoPath = getFilesDir().getCanonicalPath() + "/lib" + libName + "-arm.so";
                        File soFile = new File(unzipSoPath);
                        if(!soFile.exists()) soFile.createNewFile();
                        os = new FileOutputStream(soFile);
                    }else if(name.contains("arm64-v8a")){
                        String unzipSo64Path = getFilesDir().getCanonicalPath() + "/lib" + libName + "-arm64.so";
                        File so64File = new File(unzipSo64Path);
                        if(!so64File.exists()) so64File.createNewFile();
                        os = new FileOutputStream(so64File);
                    }
                    InputStream is2 = zf.getInputStream(entry);
                    int read = 0;
                    byte[] b2 = new byte[4096];
                    while((read = is2.read(b2, 0, b2.length)) > 0){
                        os.write(b2, 0, read);
                    }
                    os.flush();
                    os.close();
                }
            }
            for(NativeLibInfo libInfo : libInfoArray) {
                String srcSoPath = null;
                for(String libName : libNameArr) {
                    if(libInfo.is64Bit) {
                        srcSoPath = getFilesDir().getCanonicalPath() + "/lib" + libName + "-arm64.so";
                    }else{
                        srcSoPath = getFilesDir().getCanonicalPath() + "/lib" + libName + "-arm.so";
                    }
                    r.exec("su -c cp " + srcSoPath + " " + libInfo.libPath + "/lib" + libName + ".so");
                    r.exec("su -c chmod 755 " + libInfo.libPath + "/lib" + libName + ".so");
                    Log.d("nativehook123", "success copy from " + srcSoPath + " to " + libInfo.libPath);
                }

            }
        } catch (IOException e) {
            e.printStackTrace();
        }
        finish();
    }

    public String isCopyLib(String name) {
        for(String item : libNameArr) {
            if(name.contains(item)){
                return item;
            }
        }
        return null;
    }

    public String getApkPath(Context cxt) {
        String prp = cxt.getPackageResourcePath();
        try {
            String path =  new File(prp + "/../base.apk").getCanonicalPath();
            Log.d("nativehook123", "path: " + path);
            return path;
        } catch (IOException e) {
            e.printStackTrace();
        }
        return null;
    }


    public static String xxx() {
        Log.d("nativehook123", "xxxxxxxxx123456");
        return "flsdjflsjfls";
    }

    public static void logMethodInfo(Method m) {
        StringBuffer sb = new StringBuffer();
        String clsName = m.getDeclaringClass().getName();
        sb.append(clsName + "->");
        String methodName = m.getName();
        sb.append(methodName);
        Log.d("nativehook123", "call method: " + sb.toString());

        /*
        Class<?>[] params_cls = m.getParameterTypes();
        for(Class<?> param_cls : params_cls) {

        }*/
    }
}
