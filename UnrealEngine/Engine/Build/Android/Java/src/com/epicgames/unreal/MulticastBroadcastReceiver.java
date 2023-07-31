package com.epicgames.unreal;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import java.util.List;

public class MulticastBroadcastReceiver extends BroadcastReceiver
{
	public static Logger Log = new Logger("UE", "BroadcastReceiver");

	@Override
	public void onReceive(Context context, Intent intent)
	{
		String action = intent.getAction();
		Log.debug("MulticastBroadcastReceiver onReceive intent " + action);

		if (action == null)
		{
			return;
		}

		String className = this.getClass().getName();

		List<ResolveInfo> installReceivers = context.getPackageManager().queryBroadcastReceivers(new Intent(action), 0);
		for (ResolveInfo resolveInfo : installReceivers)
		{
			if (resolveInfo.activityInfo.packageName.equals(context.getPackageName()) && !resolveInfo.activityInfo.name.equals(className))
			{
				Log.debug("Broadcasting intent " + action + " to " + resolveInfo.activityInfo.name);
				try
				{
					BroadcastReceiver broadcastReceiver = (BroadcastReceiver)Class.forName(resolveInfo.activityInfo.name).newInstance();
					broadcastReceiver.onReceive(context, intent);
				}
				catch (Throwable e)
				{
					Log.debug("Broadcasting intent " + action + " failed for " + resolveInfo.activityInfo.name);
				}
			}
		}
	}
}
