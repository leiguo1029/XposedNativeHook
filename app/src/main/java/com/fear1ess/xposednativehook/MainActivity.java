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
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

public class MainActivity extends Activity {
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
        ClassLoader cl1 = MainActivity.class.getClassLoader();
        ClassLoader cl2 = ClassLoader.class.getClassLoader();
        Runtime r = Runtime.getRuntime();
        try {
            Process p = r.exec("su -c ls /data/app");
            InputStream is = p.getInputStream();
            InputStreamReader isr = new InputStreamReader(is);
            BufferedReader bufferedReader = new BufferedReader(isr);
            ArrayList<NativeLibInfo> libInfoArray = new ArrayList<>();
            byte[] b = new byte[1024];
            String line;
            while((line = bufferedReader.readLine()) != null) {
                if(isHookPackage(line)){
                    boolean is64 = false;
                    String libPath = "/data/app/" + line + "/lib/arm";
                    if(!new File(libPath).exists()) {
                        libPath = "/data/app/" + line + "/lib/arm64";
                        if(!new File(libPath).exists()) {
                            Log.d("nativeohook123", "hook app has no lib path...");
                            return;
                        }
                        is64 = true;
                    }
                    libInfoArray.add(new NativeLibInfo(libPath, is64));
                }
            }
            Log.d("nativehook123", "onCreate: ");
            String unzipSoPath = getFilesDir().getCanonicalPath() + "/libhaha123-arm.so";
            String unzipSo64Path = getFilesDir().getCanonicalPath() + "/libhaha123-arm64.so";
            File soFile = new File(unzipSoPath);
            File so64File = new File(unzipSo64Path);
            if(!soFile.exists()) soFile.createNewFile();
            if(!so64File.exists()) so64File.createNewFile();
            ZipFile zf = new ZipFile(getApkPath(this));
            Enumeration<?> entries = zf.entries();
            while(entries.hasMoreElements()) {
                ZipEntry entry = (ZipEntry) entries.nextElement();
                String name = entry.getName();
                FileOutputStream os = null;
                if(name.contains("libhaha123.so")) {
                    if(name.contains("armeabi-v7a")) {
                        os = new FileOutputStream(soFile);
                    }else if(name.contains("arm64-v8a")){
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
                if(libInfo.is64Bit) {
                    srcSoPath = unzipSo64Path;
                }else{
                    srcSoPath = unzipSoPath;
                }
                r.exec("su -c cp " + srcSoPath + " " + libInfo.libPath + "/libhaha123.so");
                r.exec("su -c chmod 755 " + libInfo.libPath + "/libhaha123.so");
                Log.d("nativehook123", "success copy from " + srcSoPath + " to " + libInfo.libPath);
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
        finish();
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
}
