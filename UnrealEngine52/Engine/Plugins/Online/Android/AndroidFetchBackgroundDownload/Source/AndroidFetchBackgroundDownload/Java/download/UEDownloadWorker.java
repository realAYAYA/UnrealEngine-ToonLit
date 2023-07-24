// Copyright Epic Games, Inc. All Rights Reserved.
package com.epicgames.unreal.download;

import android.content.Intent;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.net.Uri;
import android.os.Build;

import androidx.annotation.Nullable;
import androidx.annotation.NonNull;

import androidx.core.app.NotificationCompat;
import androidx.work.Data;
import androidx.work.ForegroundInfo;
import androidx.work.WorkManager;
import androidx.work.WorkerParameters;

import java.io.File;

import com.epicgames.unreal.GameActivity;

import com.epicgames.unreal.Logger;
import com.epicgames.unreal.workmanager.UEWorker;

import com.epicgames.unreal.download.datastructs.DownloadNotificationDescription;
import com.epicgames.unreal.download.DownloadProgressListener;
import com.epicgames.unreal.download.datastructs.DownloadQueueDescription;
import com.epicgames.unreal.download.datastructs.DownloadWorkerParameterKeys;
import com.epicgames.unreal.download.fetch.FetchManager;

import com.tonyodev.fetch2.Download;
import com.tonyodev.fetch2.Error;
import com.tonyodev.fetch2.Request;
import com.tonyodev.fetch2.exception.FetchException;
import static com.tonyodev.fetch2.util.FetchUtils.canRetryDownload;

import java.util.concurrent.TimeUnit;

import static android.content.Context.NOTIFICATION_SERVICE;

//Helper class to manage our different work requests and callbacks
public class UEDownloadWorker extends UEWorker implements DownloadProgressListener
{	
	public enum EDownloadCompleteReason
	{
		Success,
		Error,
		OutOfRetries
	}
	
	public UEDownloadWorker(Context context, WorkerParameters params)
	{
		super(context,params);
		
		//Overwrite the default log to have a more specific log identifier tag
		Log = new Logger("UE", "UEDownloadWorker");
	}
	
	@Override
	public void InitWorker()
	{
		super.InitWorker();
		
		if (null == mFetchManager)
		{
			mFetchManager = new FetchManager();	
		}
		
		//Make sure we have a CancelIntent we can use to cancel this job (passed into notifications, etc)
		if (null == CancelIntent) 
		{
			CancelIntent = WorkManager.getInstance(getApplicationContext())
				.createCancelPendingIntent(getId());
		}
		
		//Generate our NotificationDescription so that we load important data from our InputData() to control notification content
		if (null == NotificationDescription)
		{
			NotificationDescription = new DownloadNotificationDescription(getInputData(), getApplicationContext(), Log);
		}

		bHasEnqueueHappened = false;
		bForceStopped = false;
	}
	
	@Override
	public void OnWorkerStart(String WorkID)
	{
		Log.debug("OnWorkerStart Beginning for " + WorkID);

		//TODO: TRoss this should be based on some WorkerParameter and handled in UEWorker
		//Set this as an important task so that it continues even when the app closes, etc.
		//Do this immediately as we only have limited time to call this after worker start
		setForegroundAsync(CreateForegroundInfo(NotificationDescription));

		super.OnWorkerStart(WorkID);
		
		if (mFetchManager == null)
		{
			Log.error("OnWorkerStart called without a valid FetchInstance! Failing Worker and completing!");
			SetWorkResult_Failure();
			return;
		}
		
		//Setup downloads in mFetchManager
		QueueDescription = new DownloadQueueDescription(getInputData(), getApplicationContext(), Log);
		QueueDescription.ProgressListener = this;

       //Have to have parsed some DownloadDescriptions to have any meaningful work to do
		if ((QueueDescription == null) || (QueueDescription.DownloadDescriptions.size() == 0))
		{
			Log.error("Invalid QueueDescription! No DownloadDescription list for queued UEDownloadWorker!");
			SetWorkResult_Failure();
			return;
		}

		//Kick off our enqueue request with the FetchManager
		mFetchManager.EnqueueRequests(getApplicationContext(),QueueDescription);

		//Enter actual loop until work is finished
		Log.verbose("Entering OnWorkerStart Loop waiting for Fetch2");
		try 
		{
			while (bReceivedResult == false)
			{
				Tick(QueueDescription);
				Thread.sleep(500);
			}
		} 
		catch (InterruptedException e) 
		{
			Log.error("Exception trying to sleep thread. Setting work result to retry and shutting down");
			e.printStackTrace();
			
			SetWorkResult_Retry();
		}
		finally
		{
			CleanUp(WorkID);

			Log.debug("Finishing OnWorkerStart. CachedResult:" + CachedResult + " bReceivedResult:" + bReceivedResult);
		}
	}
	
	private void Tick(DownloadQueueDescription QueueDescription)
	{
		//Skip any tick logic if we have already gotten a result or our download is finished as that means we are just pending our worker stopping
		//Also want to ensure enough time has passed that we have sent off our Enqueues to the FetchManager
		if (!bReceivedResult && bHasEnqueueHappened && !bForceStopped)
		{
			mFetchManager.RequestGroupProgressUpdate(QueueDescription.DownloadGroupID,  this);
			mFetchManager.RequestCheckDownloadsStillActive(this);

			nativeAndroidBackgroundDownloadOnTick();
		}
	}
	
	@Override
	public void OnWorkerStopped(String WorkID)
	{	
		bForceStopped = true;

		Log.debug("OnWorkerStopped called for " + WorkID);
		super.OnWorkerStopped(WorkID);
		
		CleanUp(WorkID);

		Log.debug("OnWorkerStopped Ending for " + WorkID + " CachedResult:" + CachedResult + " bReceivedResult:" + bReceivedResult);
	}
	
	public void CleanUp(String WorkID)
	{
		//Call stop work to make sure Fetch stops doing work while 
		mFetchManager.StopWork(WorkID);
		
		//Clean up our DownloadDescriptionList file if our work is not going to re-run ever
		if (ShouldCleanupDownloadDescriptorJSONFile())
		{
			Data data = getInputData();
			if (null != data)
			{
				String DownloadDescriptionListString = DownloadQueueDescription.GetDownloadDescriptionListFileName(data, Log);
				if (null != DownloadDescriptionListString)
				{
					File DeleteFile = new File(DownloadDescriptionListString);
					if (DeleteFile.exists())
					{
						DeleteFile.delete();
						Log.debug("Deleted DownloadDescriptorJSONFile " + DownloadDescriptionListString + " in CleanUp");
					}
				}
			}
		}
	}
	
	public void UpdateNotification(int CurrentProgress, boolean Indeterminate)
	{
		if (null != NotificationDescription)
		{
			NotificationDescription.CurrentProgress = CurrentProgress;
			NotificationDescription.Indeterminate = Indeterminate;

			setForegroundAsync(CreateForegroundInfo(NotificationDescription));
		}
		else
		{
			Log.error("Unexpected NULL NotificationDescripton during UpdateNotification!");
		}
	}
	
	@NonNull
	private ForegroundInfo CreateForegroundInfo(DownloadNotificationDescription Description) 
	{		
		Context context = getApplicationContext();
		NotificationManager notificationManager = GetNotificationManager(context);
		
		CreateNotificationChannel(context, notificationManager, Description);

		//Setup Notification Text Values (ContentText and ContentInfo)
		boolean bIsComplete = (Description.CurrentProgress >= Description.MAX_PROGRESS);
		String NotificationTextToUse = null;
		if (!bIsComplete)
		{
			NotificationTextToUse = String.format(Description.ContentText, Description.CurrentProgress);
		}
		else
		{
			NotificationTextToUse = Description.ContentCompleteText;
		}
		
		//Setup Opening UE app if clicked
		PendingIntent pendingNotificationIntent = null;
		{
			Intent notificationIntent = new Intent(context, GameActivity.class);
			
			// launch if closed but resume if running
			notificationIntent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);

			notificationIntent.putExtra("localNotificationID" , Description.NotificationID);
			notificationIntent.putExtra("localNotificationAppLaunched" , true);

			pendingNotificationIntent = PendingIntent.getActivity(context, Description.NotificationID, notificationIntent, PendingIntent.FLAG_IMMUTABLE);
		}

		Notification notification = new NotificationCompat.Builder(context, Description.NotificationChannelID)
			.setContentTitle(Description.TitleText)
			.setTicker(Description.TitleText)
			.setContentText(NotificationTextToUse)
			.setContentIntent(pendingNotificationIntent)
			.setProgress(Description.MAX_PROGRESS,Description.CurrentProgress, Description.Indeterminate)
			.setOngoing(true)
			.setOnlyAlertOnce (true)
			.setSmallIcon(Description.SmallIconResourceID)
			.addAction(Description.CancelIconResourceID, Description.CancelText, CancelIntent)
			.build();
		
		return new ForegroundInfo(Description.NotificationID,notification);
	}

	//Gets the Notification Manager through the appropriate method based on build version
	public NotificationManager GetNotificationManager(@NonNull Context context)
	{
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M)
		{
			return context.getSystemService(NotificationManager.class);
		}
		else
		{
			return (NotificationManager)context.getSystemService(NOTIFICATION_SERVICE);
		}
	}

	private void CreateNotificationChannel(Context context, NotificationManager notificationManager, DownloadNotificationDescription Description)
	{
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
		{
			if (notificationManager != null)
			{
				//Don't create if it already exists
				NotificationChannel Channel = notificationManager.getNotificationChannel(Description.NotificationChannelID);
				if (Channel == null)
				{
					Channel = new NotificationChannel(Description.NotificationChannelID, Description.NotificationChannelName, Description.NotificationChannelImportance);
					notificationManager.createNotificationChannel(Channel);
				}
			}
		}
	}
	
	public boolean ShouldCleanupDownloadDescriptorJSONFile()
	{
		return (IsWorkEndTerminal());
	}
	
	//
	// DownloadCompletionListener Implementation
	//
	@Override
	public void OnDownloadProgress(String RequestID, long BytesWrittenSinceLastCall, long TotalBytesWritten)
	{
		nativeAndroidBackgroundDownloadOnProgress(RequestID, BytesWrittenSinceLastCall, TotalBytesWritten);
	}
	
	@Override
	public void OnDownloadGroupProgress(int GroupID, int Progress, boolean Indeterminate)
	{
		//For now all downloads are in the same GroupID, but in the future we will want a notification for each group ID 
		//and to upgate them separately here.
		UpdateNotification(Progress, Indeterminate);
	}
	
	@Override
	public void OnDownloadComplete(String RequestID, String CompleteLocation, EDownloadCompleteReason CompleteReason)
	{
		boolean bWasSuccess = (CompleteReason == EDownloadCompleteReason.Success);
		nativeAndroidBackgroundDownloadOnComplete(RequestID, CompleteLocation, bWasSuccess);
	}
	
	@Override
	public void OnAllDownloadsComplete(boolean bDidAllRequestsSucceed)
	{	
		//If UE code has already provided a resolution then we do not need to handle this OnAllDownloadsComplete notification as 
		//this UEDownloadWorker is already in the process of stopping work. Also if we have already been force stopped, don't send
		//this to avoid sending this queued reply after we have already stopped work (possible to queue this during the tick before our force stop)
		if (!bReceivedResult && !bForceStopped)
		{
			UpdateNotification(100, false);
		
			nativeAndroidBackgroundDownloadOnAllComplete(bDidAllRequestsSucceed);
		
			//If UE code didn't provide a result for the work in the above callback(IE: Engine isn't running yet, we are completely in background, etc.) 
			//then we need to still flag this Worker as completed and shutdown now that our task is finished
			if (!bReceivedResult)
			{
				if (bDidAllRequestsSucceed)
				{
					SetWorkResult_Success();
				}
				//by default if UE didn't give us a behavior, lets just retry the download through the worker if one of the downloads failed
				else
				{
					SetWorkResult_Retry();
				}
			}
		}
	}

	@Override
	public void OnDownloadEnqueued(String RequestID, boolean bEnqueueSuccess)
	{
		if (bEnqueueSuccess)
		{
			Log.verbose("Enqueue success:%s" + RequestID);
		}
		else
		{
			Log.debug("Enqueue failure, retrying request:" + RequestID);
			mFetchManager.RetryDownload(RequestID);
		}

		bHasEnqueueHappened = true;
	}
	
	//Want to call our DownloadWorker version of OnWorkerStart
	@Override
	public void CallNativeOnWorkerStart(String WorkID)
	{
		nativeAndroidBackgroundDownloadOnWorkerStart(WorkID);
	}

	@Override
	public void CallNativeOnWorkerStop(String WorkID)
	{
		nativeAndroidBackgroundDownloadOnWorkerStop(WorkID);
	}

	//
	// Functions called by our UE c++ code on this object
	//
	public void PauseRequest(String RequestID)
	{
		mFetchManager.PauseDownload(RequestID, true);
	}
	
	public void ResumeRequest(String RequestID)
	{
		mFetchManager.PauseDownload(RequestID, false);
	}
	
	public void CancelRequest(String RequestID)
	{
		mFetchManager.CancelDownload(RequestID);
	}
	
	//Native functions used to bubble up progress to native UE code
	public native void nativeAndroidBackgroundDownloadOnWorkerStart(String WorkID);
	public native void nativeAndroidBackgroundDownloadOnWorkerStop(String WorkID);
	public native void nativeAndroidBackgroundDownloadOnProgress(String TaskID, long BytesWrittenSinceLastCall, long TotalBytesWritten);
	public native void nativeAndroidBackgroundDownloadOnComplete(String TaskID, String CompleteLocation, boolean bWasSuccess);
	public native void nativeAndroidBackgroundDownloadOnAllComplete(boolean bDidAllRequestsSucceed);
	public native void nativeAndroidBackgroundDownloadOnTick();
	
	private boolean bForceStopped = false;
	private DownloadQueueDescription QueueDescription = null;
	private volatile boolean bHasEnqueueHappened = false;
	static FetchManager mFetchManager;
	private PendingIntent CancelIntent = null;
	private DownloadNotificationDescription NotificationDescription = null;
}