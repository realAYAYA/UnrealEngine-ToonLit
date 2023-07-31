// Copyright Epic Games, Inc. All Rights Reserved.
package com.epicgames.unreal.download.datastructs;

import android.app.NotificationManager;
import android.content.Context;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.work.Data;
import androidx.work.WorkerParameters;

import com.epicgames.unreal.LocalNotificationReceiver;
import com.epicgames.unreal.Logger;
import com.epicgames.unreal.download.datastructs.DownloadWorkerParameterKeys;

//Helper class that stores all the needed information for a Notification in one object and handles parsing and caching defaults stored in the WorkerParameters
public class DownloadNotificationDescription
{
	//Constructor that parses good defaults from our Worker InputData
	public DownloadNotificationDescription(@NonNull Data data,@NonNull Context context, @Nullable Logger Log)
	{
		//Set values that are just raw defaults that we expect to be manually overriden
		{
			CurrentProgress = 0;
			Indeterminate = true;
		}
					
		//Load notification channel information
		{
			NotificationChannelID = data.getString(DownloadWorkerParameterKeys.NOTIFICATION_CHANNEL_ID_KEY);
			if (null == NotificationChannelID) 
			{
				NotificationChannelID = "ue-downloadworker-channel-id";
			}

			NotificationChannelName = data.getString(DownloadWorkerParameterKeys.NOTIFICATION_CHANNEL_NAME_KEY);
			if (null == NotificationChannelName) 
			{
				NotificationChannelName = "ue-downloadworker-channel";
			}

			NotificationChannelImportance = data.getInt(DownloadWorkerParameterKeys.NOTIFICATION_CHANNEL_IMPORTANCE_KEY, NotificationManager.IMPORTANCE_DEFAULT);
		}
		
		//Load notification base information
		{
			//Loads from data or defaults to a random number (that is hopefully unique) as this can NOT be set to 0 for SetForeground notifications!

			NotificationID = data.getInt(DownloadWorkerParameterKeys.NOTIFICATION_ID_KEY, DownloadWorkerParameterKeys.NOTIFICATION_DEFAULT_ID_KEY);

			if (NotificationID == 0)
			{
				if (null != Log) 
				{
					Log.error("Invalid NotificationID for notification! Will not be able to activate as a foreground service correctly!");
				}
			}
		}

		//Load notification content information
		{
			TitleText = data.getString(DownloadWorkerParameterKeys.NOTIFICATION_CONTENT_TITLE_KEY);
			if (null == TitleText) 
			{
				TitleText = "Downloading";
			}

			ContentText = data.getString(DownloadWorkerParameterKeys.NOTIFICATION_CONTENT_TEXT_KEY);
			if (null == ContentText) 
			{
				ContentText = "Download in Progress";
			}

			ContentCompleteText = data.getString(DownloadWorkerParameterKeys.NOTIFICATION_CONTENT_COMPLETE_TEXT_KEY);
			if (null == ContentCompleteText) 
			{
				ContentCompleteText = "Complete";
			}
			
			CancelText = data.getString(DownloadWorkerParameterKeys.NOTIFICATION_CONTENT_CANCEL_DOWNLOAD_TEXT_KEY);
			if (null == CancelText) 
			{
				CancelText = "Cancel";
			}
		}
		
		//Load the Cancel Icon Resource
		{
			//Flag if the user tried to set anything so we can show an error if the load fails when they weren't expecting to use the default
			boolean bTriedToSetAnything = false;
			
			String CancelIconResourceName = data.getString(DownloadWorkerParameterKeys.NOTIFICATION_RESOURCE_CANCEL_ICON_NAME);
			if (null == CancelIconResourceName) 
			{
				CancelIconResourceName = "ic_delete";
			}
			else
			{
				bTriedToSetAnything = true;
			}

			String CancelIconResourceType = data.getString(DownloadWorkerParameterKeys.NOTIFICATION_RESOURCE_CANCEL_ICON_TYPE);
			if (null == CancelIconResourceType) 
			{
				CancelIconResourceType = "drawable";
			}
			else
			{
				bTriedToSetAnything = true;
			}

			String CancelIconResourcePackage = data.getString(DownloadWorkerParameterKeys.NOTIFICATION_RESOURCE_CANCEL_ICON_PACKAGE);
			if (null == CancelIconResourcePackage) 
			{
				CancelIconResourcePackage = context.getPackageName();
			}
			else
			{
				bTriedToSetAnything = true;
			}

			CancelIconResourceID = context.getResources().getIdentifier(CancelIconResourceName, CancelIconResourceType, CancelIconResourcePackage);
			//Failed to load this default so try using a known default
			if (0 == CancelIconResourceID)
			{
				//Only an error if we set something and didn't expect the default for now
				if (bTriedToSetAnything)
				{
					if (null != Log) 
					{
						Log.error("Could not find resource for Cancel Icon using Name:" + CancelIconResourceName + " Type:" + CancelIconResourceType + " Package:" + CancelIconResourcePackage);
					}
				}
				
				//attempt to fallback to a system default
				CancelIconResourceID = android.R.drawable.ic_delete;
			
				if (0 == CancelIconResourceID)
				{
					if (null != Log)
					{
						Log.error("Unable to find any valid default cancel resource icon! Will likely crash from an invalid notification!");
					}
				}
			}
		}
		
		//Load the Small Icon Resource
		{
			//Flag if the user tried to set anything so we can show an error if the load fails when they weren't expecting to use the default
			boolean bTriedToSetAnything = false;
			
			String SmallIconResourceName = data.getString(DownloadWorkerParameterKeys.NOTIFICATION_RESOURCE_SMALL_ICON_NAME);
			if (null == SmallIconResourceName)
			{
				SmallIconResourceName = "ic_notification_simple";
			}
			else
			{
				bTriedToSetAnything = true;
			}

			String SmallIconResourceType = data.getString(DownloadWorkerParameterKeys.NOTIFICATION_RESOURCE_SMALL_ICON_TYPE);
			if (null == SmallIconResourceType)
			{
				SmallIconResourceType = "drawable";
			}
			else
			{
				bTriedToSetAnything = true;
			}

			String SmallIconResourcePackage = data.getString(DownloadWorkerParameterKeys.NOTIFICATION_RESOURCE_SMALL_ICON_PACKAGE);
			if (null == SmallIconResourcePackage)
			{
				SmallIconResourcePackage = context.getPackageName();
			}
			else
			{
				bTriedToSetAnything = true;
			}

			SmallIconResourceID = context.getResources().getIdentifier(SmallIconResourceName, SmallIconResourceType, SmallIconResourcePackage);
			//Failed to load this default so try using the LocalNotification's paths
			if (0 == SmallIconResourceID)
			{
				if (bTriedToSetAnything)
				{
					if (null != Log) 
					{
						Log.error("Could not find resource for Small Icon using Name:" + SmallIconResourceName + " Type:" + SmallIconResourceType + " Package:" + SmallIconResourcePackage);
					}
				}
				
				//attempt to fallback to the LocalNotification's method
				SmallIconResourceID = LocalNotificationReceiver.getNotificationIconID(context);
			
				if (0 == SmallIconResourceID)
				{
					if (null != Log) 
					{
						Log.error("Could not find default resource for Small Icon! Will crash from invalid notification!");
					}
				}
			}
		}
	}

	//
	//Values set by owner
	//
	public int CurrentProgress = 0;
	public boolean Indeterminate = true;
		
	//
	//Values we load defaults into from the WorkerParameters
	//
	public String NotificationChannelID = null;
	public String NotificationChannelName = null;
	public int NotificationChannelImportance = NotificationManager.IMPORTANCE_DEFAULT;
	
	public int NotificationID = 0;

	public String TitleText = null;
	public String ContentText = null;
	public String ContentCompleteText = null;
	public String CancelText = null;
	
	public int CancelIconResourceID = 0;
	public int SmallIconResourceID = 0;

	//We just care about our progress being between 0->100 as a % so this is always 100
	public final int MAX_PROGRESS = 100;
}