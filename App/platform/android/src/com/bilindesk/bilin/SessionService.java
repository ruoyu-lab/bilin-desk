package com.bilindesk.bilin;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.IBinder;

// 连着电脑的时候开着的前台服务（App/src/mobile/session_keeper.h）：程序到了后台系统也不冻结本进程，连接不断。
// 通知栏里说正在连接，点了回到程序。从任务列表里划掉程序时跟着停（stopWithTask，AndroidManifest.xml）
public class SessionService extends Service
{
    private static final String Channel = "session";
    private static final int NotificationId = 1;

    public static void start(Context context)
    {
        context.startForegroundService(new Intent(context, SessionService.class));
    }

    public static void stop(Context context)
    {
        context.stopService(new Intent(context, SessionService.class));
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId)
    {
        NotificationChannel channel = new NotificationChannel(Channel, getString(R.string.session_channel),
                                                              NotificationManager.IMPORTANCE_LOW);
        getSystemService(NotificationManager.class).createNotificationChannel(channel);
        Intent open = getPackageManager().getLaunchIntentForPackage(getPackageName());
        Notification notification = new Notification.Builder(this, Channel)
            .setSmallIcon(R.drawable.ic_notification)
            .setContentTitle(getString(R.string.session_title))
            .setContentText(getString(R.string.session_text))
            .setContentIntent(PendingIntent.getActivity(this, 0, open, PendingIntent.FLAG_IMMUTABLE))
            .setOngoing(true)
            .build();
        startForeground(NotificationId, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE);
        // 被系统结束了不再自己起来：那时程序也没了
        return START_NOT_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent)
    {
        return null;
    }
}
