// Copyright Epic Games, Inc. All Rights Reserved.

package com.epicgames.unreal;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.AsyncTask;

import org.json.JSONException;
import org.json.JSONObject;

import java.lang.ref.WeakReference;
import java.util.Iterator;

public class BootCompleteReceiver extends BroadcastReceiver
{
	public static Logger Log = new Logger("UE", "BootCompleteReceiver");
	
	@Override
	public void onReceive(Context context, Intent intent)
	{
		PendingResult pendingResult = goAsync();
		Task task = new Task(pendingResult, context);
		task.execute();
	}

	private static class Task extends AsyncTask<Void, Void, Void>
	{
		private PendingResult pendingResult;
		private WeakReference<Context> context;

		Task(PendingResult pendingResult, Context context)
		{
			this.pendingResult = pendingResult;
			// prevent async task context leaks
			this.context = new WeakReference<>(context.getApplicationContext());
		}

		@Override
		protected Void doInBackground(Void... params)
		{
			Context context = this.context.get();
			if (context == null)
			{
				// should technically never happen, since the application context will still exist until the receiver is recycled
				return null;
			}

			// restore any scheduled notifications
			SharedPreferences preferences = context.getSharedPreferences("LocalNotificationPreferences", Context.MODE_PRIVATE);
			if (preferences == null)
			{
				// nothing to reschedule if the preferences don't exist
				return null;
			}
			try
			{
				boolean changed = false;
				JSONObject notificationDetails = new JSONObject(preferences.getString("notificationDetails", "{}"));
				for (Iterator<String> iterator = notificationDetails.keys(); iterator.hasNext(); )
				{
					try
					{
						String key = iterator.next();
						int notificationId = Integer.parseInt(key);
						JSONObject details = notificationDetails.getJSONObject(key);
						String targetDateTime = details.getString("local-notification-targetDateTime");
						boolean localTime = details.getBoolean("local-notification-localTime");
						String title = details.getString("local-notification-title");
						String body = details.getString("local-notification-body");
						String action = details.getString("local-notification-action");
						String activationEvent = details.getString("local-notification-activationEvent");

						if (!GameActivity.LocalNotificationScheduleAtTime(context, notificationId, targetDateTime, localTime, title, body, action, activationEvent))
						{
							// if it fails, remove from details
							iterator.remove();
							changed = true;
						}
					}
					catch (NumberFormatException | JSONException e)
					{
						Log.error("Error reading notification details", e);
						iterator.remove();
						changed = true;
					}
				}

				if (changed)
				{
					SharedPreferences.Editor editor = preferences.edit();
					editor.putString("notificationDetails", notificationDetails.toString());
					editor.commit();
				}
			}
			catch (JSONException e)
			{
				Log.error("Error reading notification details", e);
			}

			return null;
		}

		@Override
		protected void onPostExecute(Void result)
		{
			super.onPostExecute(result);
			// mark receiver as ready for recycle
			pendingResult.finish();
		}
	}
}
