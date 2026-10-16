package com.bilindesk.bilin;

import android.app.Activity;
import android.content.ComponentName;
import android.util.Log;
import android.view.Window;
import android.view.WindowManager;

import java.lang.reflect.Method;

// 控制着电脑时，系统自己的快捷键（Win+R、Win+E、Alt+Tab……）也交给程序发给对方（App/src/mobile/phone_system.h）。
// Android 16 QPR1 起是 WindowManager.LayoutParams.setKeyboardCaptureEnabled（要 CAPTURE_KEYBOARD 权限，
// AndroidManifest.xml）；之前的三星手机用三星自己的 SemWindowManager.requestMetaKeyEvent。两样都没有的手机照旧给系统。
// 编译用的 SDK 里没有这两样：按名字找
public final class SystemKeys
{
    private static final String Tag = "Bilin";

    public static void capture(Activity activity, boolean capture)
    {
        activity.runOnUiThread(() -> {
            if (!captureByWindow(activity.getWindow(), capture))
                captureOnSamsung(activity.getComponentName(), capture);
        });
    }

    private static boolean captureByWindow(Window window, boolean capture)
    {
        final Method method;
        try {
            method = WindowManager.LayoutParams.class.getMethod("setKeyboardCaptureEnabled", boolean.class);
        } catch (NoSuchMethodException e) {
            return false;
        }
        try {
            WindowManager.LayoutParams attributes = window.getAttributes();
            method.invoke(attributes, capture);
            window.setAttributes(attributes);
        } catch (ReflectiveOperationException e) {
            Log.w(Tag, "系统快捷键没能交给程序", e);
        }
        return true;
    }

    private static void captureOnSamsung(ComponentName activity, boolean capture)
    {
        try {
            Class<?> type = Class.forName("com.samsung.android.view.SemWindowManager");
            Object manager = type.getMethod("getInstance").invoke(null);
            if (manager != null)
                type.getDeclaredMethod("requestMetaKeyEvent", ComponentName.class, boolean.class)
                    .invoke(manager, activity, capture);
        } catch (ClassNotFoundException e) {
            // 不是三星
        } catch (ReflectiveOperationException e) {
            Log.w(Tag, "三星的系统快捷键没能交给程序", e);
        }
    }
}
