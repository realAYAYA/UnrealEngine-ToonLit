// Copyright Epic Games, Inc. All Rights Reserved.

package com.epicgames.unreal;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.NotificationChannel;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import androidx.core.app.NotificationCompat;

public class LocalNotificationReceiver extends BroadcastReceiver
{
	private static final String NOTIFICATION_CHANNEL_ID = "ue4-push-notification-channel-id";
	private static final CharSequence NOTICATION_CHANNEL_NAME = "ue4-push-notification-channel";

	public static final String KEY_LOCAL_NOTIFICATION_ID = "local-notification-ID";
	public static final String KEY_LOCAL_NOTIFICATION_TITLE = "local-notification-title";
	public static final String KEY_LOCAL_NOTIFICATION_BODY = "local-notification-body";
	public static final String KEY_LOCAL_NOTIFICATION_ACTION = "local-notification-action";
	public static final String KEY_LOCAL_NOTIFICATION_ACTION_EVENT = "local-notification-activationEvent";

	public void onReceive(Context context, Intent intent)
	{
		int notificationID = intent.getIntExtra(KEY_LOCAL_NOTIFICATION_ID , 0);
		String title  = intent.getStringExtra(KEY_LOCAL_NOTIFICATION_TITLE);
		String details  = intent.getStringExtra(KEY_LOCAL_NOTIFICATION_BODY);
		String action = intent.getStringExtra(KEY_LOCAL_NOTIFICATION_ACTION);
		String activationEvent = intent.getStringExtra(KEY_LOCAL_NOTIFICATION_ACTION_EVENT);

		if(title == null || details == null || action == null || activationEvent == null)
		{
			// Do not schedule any local notification if any allocation failed
			return;
		}

		// Open UE4 app if clicked
		Intent notificationIntent = new Intent(context, GameActivity.class);

		// launch if closed but resume if running
		notificationIntent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
		notificationIntent.putExtra("localNotificationID" , notificationID);
		notificationIntent.putExtra("localNotificationAppLaunched" , true);
		notificationIntent.putExtra("localNotificationLaunchActivationEvent", activationEvent);

		int notificationIconID = getNotificationIconID(context);
		PendingIntent pendingNotificationIntent = PendingIntent.getActivity(context, notificationID, notificationIntent, PendingIntent.FLAG_IMMUTABLE);

		NotificationManager notificationManager = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
		@SuppressWarnings("deprecation")
		NotificationCompat.Builder builder = new NotificationCompat.Builder(context, NOTIFICATION_CHANNEL_ID)
			.setSmallIcon(notificationIconID)
			.setContentIntent(pendingNotificationIntent)
			.setWhen(System.currentTimeMillis())
			.setTicker(details)		// note: will not show up on Lollipop up except for accessibility
			.setContentTitle(title)
			.setStyle(new NotificationCompat.BigTextStyle().bigText(details));			
		if (android.os.Build.VERSION.SDK_INT >= 21)
		{
			builder.setContentText(details);
			builder.setColor(0xff0e1e43);
		}

		if (android.os.Build.VERSION.SDK_INT >= 26)
		{
			if(notificationManager != null)
			{
				NotificationChannel channel = notificationManager.getNotificationChannel(NOTIFICATION_CHANNEL_ID);
				if (channel == null)
				{
					channel = new NotificationChannel(NOTIFICATION_CHANNEL_ID, NOTICATION_CHANNEL_NAME, NotificationManager.IMPORTANCE_DEFAULT);
					channel.enableVibration(true);
					channel.enableLights(true);
					notificationManager.createNotificationChannel(channel);
				}
			}
		}
		Notification notification = builder.build();

		// Stick with the defaults
		notification.flags |= Notification.FLAG_AUTO_CANCEL;
		notification.defaults |= Notification.DEFAULT_SOUND | Notification.DEFAULT_VIBRATE;

		if(notificationManager != null)
		{
			// show the notification
			notificationManager.notify(notificationID, notification);
			
			// clear the stored notification details if they exist
			GameActivity.LocalNotificationRemoveDetails(context, notificationID);
		}
	}

	public static int getNotificationIconID(Context context)
	{
		int notificationIconID = context.getResources().getIdentifier("ic_notification_simple", "drawable", context.getPackageName());
		if (notificationIconID == 0)
		{
			notificationIconID = context.getResources().getIdentifier("ic_notification", "drawable", context.getPackageName());
		}
		if (notificationIconID == 0)
		{
			notificationIconID = context.getResources().getIdentifier("icon", "drawable", context.getPackageName());
		}
		return notificationIconID;
	}
}
