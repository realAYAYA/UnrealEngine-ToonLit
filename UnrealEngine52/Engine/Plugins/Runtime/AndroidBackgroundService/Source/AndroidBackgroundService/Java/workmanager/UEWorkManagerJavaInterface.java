// Copyright Epic Games, Inc. All Rights Reserved.
package com.epicgames.unreal.workmanager;

import android.content.Context;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.TimeUnit;

import androidx.work.BackoffPolicy;
import androidx.work.Data;
import androidx.work.NetworkType;
import androidx.work.Operation;
import androidx.work.WorkManager;
import androidx.work.WorkRequest;
import androidx.work.OneTimeWorkRequest;
import androidx.work.Constraints;
import androidx.work.ExistingWorkPolicy;
import androidx.work.ExistingPeriodicWorkPolicy;
import androidx.work.Configuration;

import androidx.annotation.Nullable;
import androidx.annotation.NonNull;

import com.epicgames.unreal.workmanager.UEWorker;

//Helper class to manage our different work requests and callbacks
public class UEWorkManagerJavaInterface
{
	public static class FWorkRequestParametersJavaInterface
	{
		public boolean bRequireBatteryNotLow;
		public boolean bRequireCharging;
		public boolean bRequireDeviceIdle;
		public boolean bRequireWifi;
		public boolean bRequireAnyInternet;
		public boolean bAllowRoamingInternet;
		public boolean bRequireStorageNotLow;
		public boolean bStartAsForegroundService;
		public boolean bIsPeriodicWork;
		
		public long InitialStartDelayInSeconds;

		public boolean bIsRecurringWork;
		public int RepeatIntervalInMinutes;

		public boolean bUseLinearBackoffPolicy;
		public int InitialBackoffDelayInSeconds;
		
		public Map<String, Object> ExtraWorkData;
				
		public Class WorkerJavaClass;
		
		public FWorkRequestParametersJavaInterface()
		{
			// WARNING:
			//These defaults are just in here for prosterity, but in reality the defaults in the C++ UEWorkManagerNativeWrapper class
			//are what is actually used since it drives these underlying values (Although the 2 code paths SHOULD match)
			bRequireBatteryNotLow			= true;
			bRequireCharging				= false;
			bRequireDeviceIdle				= false;
			bRequireWifi					= false;
			bRequireAnyInternet				= false;
			bAllowRoamingInternet			= false;
			bRequireStorageNotLow			= false;
			bStartAsForegroundService		= false;
		
			bIsPeriodicWork					= false;
		
			InitialStartDelayInSeconds		= 0;

			bIsRecurringWork				= false;

			//default on the system is 15 min for this, even though we have it turned off want the meaningful default
			RepeatIntervalInMinutes			= 15;		

			//default if not specified is exponential backoff policy with 10s
			bUseLinearBackoffPolicy			= false;
			InitialBackoffDelayInSeconds	= 10;
			
			//by default empty HashMap, but can add to this to have these values end up in the Worker Parameter data.
			ExtraWorkData = new HashMap<String, Object>();
						
			//default to just a generic UEWorker if one isn't supplied
			WorkerJavaClass = UEWorker.class;
		}
		
		public void AndroidThunk_AddExtraWorkData(String string, Object object)
		{
			ExtraWorkData.put(string, object);
		}
		
		public void AndroidThunk_AddExtraWorkData(String string, int value)
		{
			ExtraWorkData.put(string, value);
		}
		
		public void AndroidThunk_AddExtraWorkData(String string, long value)
		{
			ExtraWorkData.put(string, value);
		}
		
		public void AndroidThunk_AddExtraWorkData(String string, float value)
		{
			ExtraWorkData.put(string, value);
		}

		public void AndroidThunk_AddExtraWorkData(String string, double value)
		{
			ExtraWorkData.put(string, value);
		}
		
		public void AndroidThunk_AddExtraWorkData(String string, boolean value)
		{
			ExtraWorkData.put(string, value);
		}
	}
	
	public static FWorkRequestParametersJavaInterface AndroidThunkJava_CreateWorkRequestParameters()
	{
		return new FWorkRequestParametersJavaInterface();
	}
	
	public static boolean AndroidThunkJava_RegisterWork(Context AppContext, String TaskID, FWorkRequestParametersJavaInterface InParams)
	{
		//See all options for constraints at: https://developer.android.com/reference/androidx/work/Constraints.Builder
		Constraints constraints = new Constraints.Builder()
			 .setRequiresBatteryNotLow(InParams.bRequireBatteryNotLow)   
			 .setRequiresCharging(InParams.bRequireCharging)
			 .setRequiresDeviceIdle(InParams.bRequireDeviceIdle)
			 .setRequiresStorageNotLow(InParams.bRequireStorageNotLow)
			 .build();

		if (InParams.bRequireWifi)
		{
			constraints.setRequiredNetworkType(NetworkType.UNMETERED);
		}
		else if (InParams.bRequireAnyInternet)
		{
			if (InParams.bAllowRoamingInternet)
			{
				constraints.setRequiredNetworkType(NetworkType.CONNECTED);
			}
			else
			{
				constraints.setRequiredNetworkType(NetworkType.NOT_ROAMING);
			}
		}
		else
		{
			constraints.setRequiredNetworkType(NetworkType.NOT_REQUIRED);
		}
				
		if (InParams.bIsRecurringWork)
		{
			// need to annoyingly duplicate a lot of the below code but using
			//WorkManager.enqueueUniquePeriodicWork(TaskID, ExistingPeriodicWorkPolicy.REPLACE ,newWorkRequest);
			return false;
			
		}
		else
		{	
			BackoffPolicy BackoffPolicyToUse = BackoffPolicy.EXPONENTIAL;
			if (InParams.bUseLinearBackoffPolicy)
			{
				BackoffPolicyToUse = BackoffPolicy.LINEAR;
			}
			
			OneTimeWorkRequest newWorkRequest =  new OneTimeWorkRequest.Builder(InParams.WorkerJavaClass)
				.setConstraints(constraints)
				.addTag(TaskID)
				.setInitialDelay(InParams.InitialStartDelayInSeconds, TimeUnit.SECONDS)
				.setBackoffCriteria(BackoffPolicyToUse, InParams.InitialBackoffDelayInSeconds, TimeUnit.SECONDS)
				.setInputData(
					new Data.Builder()
						.putString("WorkID", TaskID)
						.putAll(InParams.ExtraWorkData)
						.build())
				.build();
			
			boolean bDidQueue = true;
			Operation QueueOperationResult;
			try 
			{
				QueueOperationResult = WorkManager.getInstance(AppContext.getApplicationContext()).enqueueUniqueWork(TaskID, ExistingWorkPolicy.REPLACE, newWorkRequest);
			}
			catch(Exception exp)
			{
				exp.printStackTrace();
				bDidQueue = false;
			}
			
			//TODO TRoss -- consider listening for QueueOperationResult's result so we can flag if it actually queued or not, for now just always assume the queue went through though if we didn't catch
			return bDidQueue;
		}
	}

	public static void AndroidThunkJava_CancelWork(Context AppContext, String TaskID)
	{
		try 
		{
			WorkManager.getInstance(AppContext.getApplicationContext()).cancelUniqueWork(TaskID);
		}
		catch(Exception exp)
		{
			exp.printStackTrace();
		}		
	}
}