// Copyright Epic Games, Inc. All Rights Reserved.
package com.epicgames.unreal.download.fetch;

import android.content.Context;
import android.net.Uri;

import com.epicgames.unreal.Logger;
import com.epicgames.unreal.download.datastructs.DownloadDescription;
import com.epicgames.unreal.download.datastructs.DownloadQueueDescription;
import com.epicgames.unreal.download.fetch.FetchDownloadProgressOwner;
import com.epicgames.unreal.download.DownloadProgressListener;
import com.epicgames.unreal.download.fetch.FetchEnqueueResultListener;
import com.epicgames.unreal.download.fetch.FetchRequestProgressListener;

import com.epicgames.unreal.download.UEDownloadWorker.EDownloadCompleteReason;
import com.epicgames.unreal.download.fetch.FetchRequestProgressListener.ECompleteReason;

import com.tonyodev.fetch2.CompletedDownload;
import com.tonyodev.fetch2.Download;
import com.tonyodev.fetch2.Error;
import com.tonyodev.fetch2.Fetch;
import com.tonyodev.fetch2.FetchConfiguration;
import com.tonyodev.fetch2.FetchGroup;
import com.tonyodev.fetch2.FetchListener;
import com.tonyodev.fetch2.NetworkType;
import com.tonyodev.fetch2.Priority;
import com.tonyodev.fetch2.Request;
import com.tonyodev.fetch2.exception.FetchException;
import com.tonyodev.fetch2core.Func;
import com.tonyodev.fetch2core.Func2;
import com.tonyodev.fetch2.Status;

import androidx.annotation.Nullable;
import androidx.annotation.NonNull;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Set;

import java.io.File;

//Class that handles setting up and managing Fetch2 requests
public class FetchManager implements FetchDownloadProgressOwner, FetchEnqueueResultListener
{
	public FetchManager()
	{
		RequestedDownloads =  new HashMap<String,DownloadDescription>();
	}
	
	public void StopWork(String WorkID)
	{
		Log.debug("StopWork called");

		if (IsFetchInstanceValid())
		{
			//We need to purge all partially completed downloads
			//If we don't, then on relaunching our app the BackgroundHttp system can end up assuming these downloads are finished incorrectly.
			DeleteAllInProgressRequests();

			//Freeze our FetchInstance so it stops all work until we unfreeze on resuming work
			FetchInstance.close();
		}
	}
	
	private void DeleteAllInProgressRequests()
	{
		if (!IsFetchInstanceValid())
		{
			Log.error("Call to DeleteAllInProgressRequests with an unexpected invalid FetchInstance");
			return;
		}

		//First we remove from Fetch management any completed downloads, this is so they aren't deleted by the next call
		FetchInstance.removeAllWithStatus(Status.COMPLETED);
			
		//Deletes everything still being managed by the FetchInstance
		FetchInstance.deleteAll();
	}

	public void EnqueueRequests(Context context, DownloadQueueDescription QueueDescription)
	{
		InitFetch(context);
		
		int NumDownloads = QueueDescription.DownloadDescriptions.size();
		Log.debug("EnqueueRequests called with " + NumDownloads + " DownloadDescriptions. Current RequestedDownloads Size: " + RequestedDownloads.size());

		SetVariablesFromQueueDescription(QueueDescription);
		ReconcileDownloadDescriptions(QueueDescription);
	}
		
	//Does any setup we need based on data in the QueueDescription
	public void SetVariablesFromQueueDescription(DownloadQueueDescription QueueDescription)
	{
		if (!IsFetchInstanceValid())
		{
			Log.error("Call to SetVariablesFromQueueDescription after FetchInstance is invalidated!");
			return;
		}
		
		FetchInstance.setDownloadConcurrentLimit(QueueDescription.MaxConcurrentDownloads);		
	}
		
	//Goes through the DownloadDescriptions in our DownloadQueueDescription and finds DownloadDescription that
	//don't match our ActiveDownloadDescriptions entries and thus need to be reconciled (either added or modified in some way)
	private void ReconcileDownloadDescriptions(DownloadQueueDescription QueueDescription)
	{
		final int NumNewDownloadDescriptions = QueueDescription.DownloadDescriptions.size();
		for (int DescriptionIndex = 0; DescriptionIndex < NumNewDownloadDescriptions; ++DescriptionIndex)
		{
			DownloadDescription NewDownloadDescription = QueueDescription.DownloadDescriptions.get(DescriptionIndex);
			
			//Set any DownloadDescription settings that need to come from our QueueDescription
			NewDownloadDescription.ProgressListener = QueueDescription.ProgressListener;
							
			String RequestIDKey = NewDownloadDescription.RequestID;
			//Don't even have an entry for this RequestID, so it's completely new
			if (!RequestedDownloads.containsKey(RequestIDKey))
			{
				QueueNewDownloadDescription(NewDownloadDescription);
			}
			//Need to update our DownloadDescription
			else
			{
				//don't requeue completed downloads
				if (NewDownloadDescription.bHasCompleted)
				{
					Log.debug("Skipping requeue of " + NewDownloadDescription.RequestID + " as its flagged for bHasCompleted");
				}
				else
				{
					DownloadDescription ActiveDescription = RequestedDownloads.get(RequestIDKey);
					HandleChangedDownloadDescription(ActiveDescription, NewDownloadDescription);
				}
			}
		}
	}
	
	public void QueueNewDownloadDescription(DownloadDescription Description)
	{
		//We have hit this code after something else has already invalidated our FetchInstance
		if (!IsFetchInstanceValid())
		{
			Log.error("Call to QueueNewDownloadDescription after FetchInstance is invalidated! RequestID:" + Description.RequestID);
			return;
		}
		
		RequestedDownloads.put(Description.RequestID, Description);

		if (!CheckForPreviouslyCompletedDownload(Description))
		{	
			//Make sure this request wasn't previously flagged as completed before beginning new work
			if (CompletedDownloads.containsKey(Description.RequestID))
			{
				CompletedDownloads.remove(Description.RequestID);
			}

			FetchEnqueueResultListener.FetchEnqueueRequestCallback RequestCallback = new FetchEnqueueResultListener.FetchEnqueueRequestCallback(this, Description.RequestID);
			FetchEnqueueResultListener.FetchEnqueueErrorCallback ErrorCallback = new FetchEnqueueResultListener.FetchEnqueueErrorCallback(this, Description.RequestID);
		
			if (!Description.bIsCancelled)
			{
				Request FetchRequest = BuildFetchRequest(Description);
				Description.CachedFetchID = FetchRequest.getId();
			
				FetchInstance.enqueue(FetchRequest, RequestCallback, ErrorCallback);

				Log.debug("Enqueued request:" + Description.RequestID);
			}
		}
		//if we have previously completed this download, then we just want to compelte it instead of creating a new FetchRequest
		else
		{
			Log.debug("Completed RequestID:" + Description.RequestID + " as the DestinationLocation already is occupied and thus previously completed.");
			CompleteDownload(Description, ECompleteReason.Success);
		}
	}

	private boolean CheckForPreviouslyCompletedDownload(DownloadDescription Description)
	{
		File CompleteLocation = new File(Description.DestinationLocation);
		return CompleteLocation.exists();

	}

	public void PauseDownload(String RequestID, boolean bPause)
	{
		//Early out if we have invalidated our FetchInstance and thus would crash if trying to do work
		if (!IsFetchInstanceValid())
		{
			Log.error("Call to PauseDownload after FetchInstance has been invalidated. Skipping as no work can be done. RequestID: " + RequestID);
			return;
		}	

		DownloadDescription MatchedDesc = RequestedDownloads.get(RequestID);
		if (null == MatchedDesc)
		{
			Log.error("No DownloadDescription found for RequestID " + RequestID +" during PauseDownload!");
			return;
		}
		
		MatchedDesc.bIsPaused = bPause;
		if (bPause)
		{
			FetchInstance.pause(MatchedDesc.CachedFetchID);
		}
		else
		{
			FetchInstance.resume(MatchedDesc.CachedFetchID);
		}
	}

	public void CancelDownload(String RequestID)
	{
		if (!IsFetchInstanceValid())
		{
			Log.error("Call to CancelDownload with an invalid fetch instance. Skipping fetch work to avoid crashing. RequestID: " + RequestID);
			return;
		}

		DownloadDescription MatchedDesc = RequestedDownloads.get(RequestID);
		if (null == MatchedDesc)
		{
			Log.error("No DownloadDescription found for RequestID " + RequestID +" during CancelDownload!");
			return;
		}
		
		if (false == MatchedDesc.bIsCancelled)
		{
			Log.verbose("Cancelling download:" + RequestID);
			
			//Set download as complete since it was cancelled intentionally
			MatchedDesc.PreviousDownloadPercent = 100;
			CompletedDownloads.put(RequestID, MatchedDesc);
			MatchedDesc.bIsCancelled = true;

			FetchInstance.delete(MatchedDesc.CachedFetchID);
		}
	}
	
	private Request BuildFetchRequest(DownloadDescription Description)
	{
		String URL = GetNextURL(Description);

		//Want to download the file to the DestinationLocation with the TempFileExtension appended. This gets removed when the file is finished
		Uri DownloadUri = Uri.parse(GetTempDownloadDestination(Description));

		Request FetchRequest = new Request(URL, DownloadUri);
		FetchRequest.setPriority(GetFetchPriority(Description));
		FetchRequest.setTag(Description.RequestID);
		FetchRequest.setGroupId(Description.GroupID);
		FetchRequest.setAutoRetryMaxAttempts(Description.IndividualURLRetryCount);
		
		//For now we don't specify this on the fetch request because its already specified on the WorkManager worker, which should stop all our
		//downloading when it stops work anyway.
		FetchRequest.setNetworkType(NetworkType.ALL);
			
		return FetchRequest;
	}
	
	private void HandleChangedDownloadDescription(DownloadDescription OldDescription, DownloadDescription NewDescription)
	{
		if (!IsFetchInstanceValid())
		{
			Log.error("Call to HandleChangedDownloadDescription with an invalid fetch instance. Skipping to avoid crashing.");
			return;
		}

		Log.verbose("Updating download description as it has changed:" + OldDescription.RequestID);
		CopyStateToNewDescription(OldDescription, NewDescription);

		//Handle the change by cancelling and recreating the fetch2 download
		RecreateDownloadByTagFunc RecreateFunc = new RecreateDownloadByTagFunc(this, NewDescription);
		FetchInstance.getDownloadsByTag(NewDescription.RequestID, RecreateFunc);
	}
	
	//Copies over the non-serialized values that track download state from our old descrition to our new description so that changing
	//serialized values doesn't wipe out existing downlod behavior
	private void CopyStateToNewDescription(DownloadDescription OldDescription, DownloadDescription NewDescription)
	{
		NewDescription.CurrentRetryCount = OldDescription.CurrentRetryCount;
		NewDescription.CachedFetchID = OldDescription.CachedFetchID;
		NewDescription.bIsPaused = OldDescription.bIsPaused;
		NewDescription.PreviousDownloadedBytes = OldDescription.PreviousDownloadedBytes;
		NewDescription.PreviousDownloadPercent = OldDescription.PreviousDownloadPercent;
		
		//if this has shown back up, we don't want to copy bIsCancelled because we want to re-queue this download
		NewDescription.bIsCancelled = false;

		//Purposefully don't copy DownloadProgressListener as we set this based on the new QueueDescription already and likely
		//don't want to keep the old one (although in current code they match).
		//NewDescription.DownloadProgressListener ProgressListener = OldDescription.DownloadProgressListener;
	}
	
	private boolean IsFetchInstanceValid()
	{
		return ((null != FetchInstance)	&& (false == FetchInstance.isClosed()));
	}
	
	private String GetNextURL(DownloadDescription Description)
	{
		if (IsOutOfRetries(Description))
		{
			return null;
		}
		
		final int NumURLs = Description.URLs.size();
		final int URLIndexToUse = Description.CurrentRetryCount % NumURLs;
		
		return Description.URLs.get(URLIndexToUse);
	}
	
	private boolean IsOutOfRetries(DownloadDescription Description)
	{
		return ((Description.CurrentRetryCount >= Description.MaxRetryCount) && Description.MaxRetryCount >= 0);
	}
	
	private Priority GetFetchPriority(DownloadDescription Description)
	{
		switch (Description.RequestPriority)
		{
		case 0:
			return Priority.NORMAL;
		case 1:
			return Priority.HIGH; 
		case -1:
			return Priority.LOW;
		default:
			return Priority.NORMAL;
		}
	}
	
	private void InitFetch(Context context)
	{
		Log.debug("InitFetch called");

		//Make sure any existing FetchInstance is in a correct state (either null and ready to be created, or open and unfrozen ready to do work
		if (FetchInstance != null)
		{
			//If we previously closed our FetchInstance just remove it and recreate it bellow
			if (FetchInstance.isClosed())
			{
				FetchInstance = null;
			}
			//If our FetchInstance exists and isn't closed, its very likely frozen and needs to be unfrozen
			else
			{
				Log.debug("InitFetch has existing non-closed FetchInstance. Unfreezing and deleting all in progress requests");

				//If we are just unfreezing existing Fetch work, lets delete everything that wasn't finished by the previous work before unfreezing
				//This prevents errors in resuming where we no longer want a particular download or we fail to resume the work
				DeleteAllInProgressRequests();

				//Now unfreeze (and hopefully have nothing really running)
				FetchInstance.unfreeze();
			}
		}

		if (FetchInstance == null)
		{
			//TODO TRoss: Pull these values from the worker's getInputData
			FetchInstance = Fetch.Impl.getInstance(new FetchConfiguration.Builder(context)
				.setNamespace(context.getPackageName())
				.enableRetryOnNetworkGain(true)
				.setProgressReportingInterval(200)
				.build());

			//if we are creating our FetchInstance, make sure our FetchListener is also recreated and attached
			FetchListener = null;
		}
					
		if (!IsFetchInstanceValid())
		{
			Log.error("Unexpected invalid FetchInstance after completing InitFetch!");
		}
		else
		{
			//Add our FetchListener
			if (null == FetchListener)
			{
				FetchListener = new FetchRequestProgressListener(this);
				FetchInstance.addListener(FetchListener);
			}
		}
	}
	
	//Helper class to avoid use of Delegates as our current compile source target is 7 and thus delegates are not supported.
	//Cancels and then recreates downloads found by a given tag.
	private class RecreateDownloadByTagFunc extends CancelDownloadByTagFunc
	{
		@Override
		public void call(List<Download> MatchingDownloads)
		{
			if (IsValid()) 
			{
				//First cancel the download
				super.call(MatchingDownloads);
				
				//Now just have the fetch manager owner recreate the download with the supplied data
				Owner.QueueNewDownloadDescription(RecreateDescription);
			}
			else
			{
				Log.debug("Aborting RecreateDownloadByTagFunc as FetchInstance has been invalidated");
			}
		}
		
		public RecreateDownloadByTagFunc(FetchManager Owner, DownloadDescription RecreateDescription)
		{
			this.Owner = Owner;
			this.RecreateDescription = RecreateDescription;
		}
		
		private boolean IsValid()
		{
			return ((null != Owner) && (null != RecreateDescription) && IsFetchInstanceValid());
		}
		
		private FetchManager Owner;
		private	DownloadDescription RecreateDescription;
	}

	//Helper class to avoid use of Delegates as our current compile source target is 7 and thus delegates are not supported.
	//Cancels downloads found by a given tag.
	private class CancelDownloadByTagFunc implements Func<List<Download>>
	{
		@Override
		public void call(List<Download> MatchingDownloads)
		{
			if (MatchingDownloads != null)
			{
				for (int DownloadIndex = 0; DownloadIndex < MatchingDownloads.size(); ++DownloadIndex)
				{
					if (!IsFetchInstanceValid())
					{
						Log.debug("Aborting CancelDownloadByTagFunc as FetchInstance has been invalidated.");
						break;
					}
					
					Download FoundDownload = MatchingDownloads.get(DownloadIndex);
					FetchInstance.cancel(FoundDownload.getId());
				}
			}
		}
	}
	
	//Function that allows us to catch any errors where the FetchAPI has stopped all work and thus should bubble up that all downloads are complete
	//Employs a non-synchronous callback, so check will not be instant
	public void RequestCheckDownloadsStillActive(DownloadProgressListener ProgressListener)
	{
		if (!IsFetchInstanceValid())
		{
			Log.error("RequestCheckDownloadsStillActive called while FetchInstance is invalid! Skipping request.");
			return;
		}
		
		CheckForActiveDownloadsFunc HasActiveDownloadFunc = new CheckForActiveDownloadsFunc(this, ProgressListener);
		FetchInstance.hasActiveDownloads(true /*includeAddedDownloads*/, HasActiveDownloadFunc);
	}

	public void RequestGroupProgressUpdate(int GroupID, DownloadProgressListener ListenerToUpdate)
	{
		//For now just assume each download is roughly the same size.
		//TODO TRoss, we should pass in the expected download amount potentially for each download and actually compute these values
		//so that we can do more accurate % rather then larger files "slowing down" the progress bar progression.
		
		int TotalProgress = 0;
		int TotalDownloadsInGroup = 0;
		
		ArrayList<String> DownloadKeys = new ArrayList<String>(RequestedDownloads.keySet());
		for (int DescIndex = 0; DescIndex < DownloadKeys.size(); ++DescIndex)
		{
			DownloadDescription FoundDesc = RequestedDownloads.get(DownloadKeys.get(DescIndex));
			
			if (FoundDesc.GroupID == GroupID)
			{
				++TotalDownloadsInGroup;
				TotalProgress += FoundDesc.PreviousDownloadPercent;
			}
		}
				
		//just get the raw average of this to send back
		int TotalToSend = TotalProgress / TotalDownloadsInGroup;
		boolean bIsIndeterminate = (TotalProgress == 0);
		
		//Make sure we cap at 100% progress
		if (TotalToSend > 100)
		{
			TotalToSend = 100;
		}
		ListenerToUpdate.OnDownloadGroupProgress(GroupID, TotalToSend, bIsIndeterminate);
	}

	//
	// FetchDownloadProgressOwner Implementation
	//
	@Override
	public void OnDownloadQueued(@NonNull Download download)
	{
		Log.verbose("OnDownloadQueued: " + GetRequestID(download));

		//Treat this as just an un-pause for now to make sure we handle cases where Fetch resumes downloads that we paused (This can happen when retrying for networkgain as an example)
		OnDownloadChangePauseState(download, false);
	}

	@Override
	public void OnDownloadProgress(@NonNull Download download, boolean indeterminate, long downloadedBytesPerSecond, long etaInMilliSeconds)
	{
		String DownloadRequestID = GetRequestID(download);

		Log.verbose("OnDownloadProgress: " + DownloadRequestID + " bytes/s:" + downloadedBytesPerSecond + " eta:" + etaInMilliSeconds);

		DownloadDescription MatchedDownload = RequestedDownloads.get(DownloadRequestID);
		if (null == MatchedDownload)
		{
			Log.error("OnDownloadProgress called from DownloadProgressOwner implementation with a download that doesn't match any DownloadDesc! Download's Tag: " + DownloadRequestID);
			return;
		}
		
		if (HasValidProgressCallback(MatchedDownload))
		{
			long TotalDownloadedSinceLastCall = 0;
			
			long TotalDownloaded = download.getDownloaded();
			//If our TotalDownload size has gone down, we likely had an error and restarted the download since the previous update.
			//Don't want to return a negative value, so just use 0 for now
			if (MatchedDownload.PreviousDownloadedBytes > TotalDownloaded)
			{
				TotalDownloadedSinceLastCall = 0;
			}
			else
			{
				TotalDownloadedSinceLastCall = (TotalDownloaded - MatchedDownload.PreviousDownloadedBytes);
			}

			MatchedDownload.PreviousDownloadPercent = download.getProgress();
			MatchedDownload.ProgressListener.OnDownloadProgress(GetRequestID(download), TotalDownloadedSinceLastCall, TotalDownloaded);
		}
		else
		{
			Log.error("DownloadDescription tied to download does not have a valid ProgressListener callback! RequestID:" + MatchedDownload.RequestID);
		}
	}

	@Override
	public void OnDownloadChangePauseState(@NonNull Download download, boolean bIsPaused)
	{
		String DownloadRequestID = GetRequestID(download);

		DownloadDescription MatchedDownload = RequestedDownloads.get(DownloadRequestID);
		if (null == MatchedDownload)
		{
			Log.error("OnDownloadChangePauseState called from DownloadProgressOwner implementation with a download that doesn't match any DownloadDesc! Download's Tag: " + DownloadRequestID);
			return;
		}
		
		if (!IsFetchInstanceValid())
		{
			Log.error("Call to OnDownloadChangePauseState with an invalid fetch instance. Skipping fetch work to avoid crashing. RequestID: " + DownloadRequestID);
			return;
		}

		//the DownloadDescription is always definitive, so make sure the new download state matches our expectations
		if (bIsPaused != MatchedDownload.bIsPaused)
		{
			Log.verbose("OnDownloadChangePauseState required Fetch action. RequestID:" + DownloadRequestID + " New bIsPaused:" + MatchedDownload.bIsPaused + " Old Fetch bIsPaused:" + bIsPaused);			
			
			if (MatchedDownload.bIsPaused)
			{
				FetchInstance.pause(download.getId());
			}
			else
			{
				FetchInstance.resume(download.getId());
			}
		}
	}

	@Override
	public void OnDownloadGroupProgress(@NonNull FetchGroup Group, DownloadProgressListener ProgressListener)
	{
		if (null == ProgressListener)
		{
			Log.error("Call to OnDownloadGroupProgress with an invalid DownloadProgressListener!");
			return;
		}
		
		int Progress = Group.getGroupDownloadProgress();
		boolean bIsIndeterminate = (Progress > 0);
		
		ProgressListener.OnDownloadGroupProgress(Group.getId(), Progress, bIsIndeterminate);
	}
	
	//This version is the DownloadProgressListener version of this function! See CompleteDownload for FetchManager implementation
	@Override
	public void OnDownloadComplete(@NonNull Download download, ECompleteReason completeReason)
	{
		String DownloadRequestID = GetRequestID(download);
		Log.debug("OnDownloadComplete: " + DownloadRequestID + " Reason:" + completeReason);

		DownloadDescription MatchedDownload = RequestedDownloads.get(DownloadRequestID);
		if (null == MatchedDownload)
		{
			Log.error("OnDownloadComplete called from DownloadProgressOwner implementation with a download that doesn't match any DownloadDesc! Download's Tag: " + DownloadRequestID);
			return;
		}
		
		CompleteDownload(MatchedDownload, completeReason);
	}

	//
	// FetchEnqueueResultListener Implementation
	//
	@Override
	public void OnFetchEnqueueRequestCallback(@NonNull String RequestID, @NonNull Request EnqueuedRequest)
	{
		Log.verbose("Enqueued Request Success. ID:" + RequestID);
		
		DownloadDescription MatchingDescription = RequestedDownloads.get(RequestID);
		if (null == MatchingDescription)
		{
			Log.error("OnFetchEnqueueRequestCallback called on request is missing from our RequestedDownloads list! RequestID:" + RequestID);
			return;
		}

		MatchingDescription.ProgressListener.OnDownloadEnqueued(RequestID, true);
	}

	@Override
	public void OnFetchEnqueueErrorCallback(@NonNull String RequestID, @NonNull Error EnqueueError)
	{
		Log.error("Error Enqueing Request! " + RequestID + "Error: " + EnqueueError);

		DownloadDescription MatchingDescription = RequestedDownloads.get(RequestID);
		if (null == MatchingDescription)
		{
			Log.error("OnFetchEnqueueErrorCallback called on request is missing from our RequestedDownloads list! RequestID:" + RequestID);
			return;
		}

		MatchingDescription.ProgressListener.OnDownloadEnqueued(RequestID, false);
	}
	
	//if UEDOwnloadableWorker is calling into it, the only thing it should need is the RequestID
	public void RetryDownload(String RequestID)
	{
		RetryDownload(RequestID, null);
	}
	
	private void RetryDownload(String RequestID, @Nullable Download FetchDownload)
	{
		DownloadDescription MatchingDescription = RequestedDownloads.get(RequestID);
		if (null == MatchingDescription)
		{
			Log.error("RetryDownload called on invalid download that was never requested! RequestID:" + RequestID);
			return;
		}

		if (!IsFetchInstanceValid())
		{
			Log.debug("Call to RetryDownload with an invalid fetch instance. Skipping fetch work and completing to avoid crashing. RequestID:" + RequestID);
			CompleteDownload(MatchingDescription, FetchRequestProgressListener.ECompleteReason.Error);
			return;
		}

		
		MatchingDescription.CurrentRetryCount++;

		if (IsOutOfRetries(MatchingDescription))
		{
			CompleteDownload(MatchingDescription, FetchRequestProgressListener.ECompleteReason.OutOfRetries);
			return;
		}
		
		//if we already know the associated FetchDownload we can just pass it through
		if (null != FetchDownload)
		{
			RetryDownload_Internal(MatchingDescription, FetchDownload);
		}
		//We don't know the associated Fetch download, so lets query Fetch for it first to see if it exists
		else
		{
			//Setup a callback to RetryDownload_Internal once we try and get our Download from Fetch
			FetchInstance.getDownload(MatchingDescription.CachedFetchID, new RetryDownloadFunc(MatchingDescription));
		}
	}
	
	private void RetryDownload_Internal(@NonNull DownloadDescription DownloadDesc, @Nullable Download RetryDownload)
	{
		if (!IsFetchInstanceValid())
		{
			Log.debug("Call to RetryDownload_Internal with an invalid fetch instance. Skipping fetch work and completing to avoid crashing. RequestID:" + DownloadDesc.RequestID);
			CompleteDownload(DownloadDesc, FetchRequestProgressListener.ECompleteReason.Error);
			return;
		}
		
		if (null != RetryDownload)
		{
			//Remove existing download from Fetch
			FetchInstance.remove(RetryDownload.getId());
			
			//Remove partial download file when switching URLs and wait to recreate until the callback is finished for the delete
			FetchInstance.delete(RetryDownload.getId(),new RecreateCallbackWithDownload(DownloadDesc), new RecreateCallbackWithError(DownloadDesc));
		}
		else
		{
			QueueNewDownloadDescription(DownloadDesc);
		}
	}

	private class RetryDownloadFunc implements Func2<Download>
	{
		public RetryDownloadFunc (DownloadDescription CachedDownloadDescription)
		{
			this.CachedDownloadDescription = CachedDownloadDescription;
		}

		@Override
		public void call(@Nullable Download MatchingDownload)
		{
			RetryDownload_Internal(CachedDownloadDescription, MatchingDownload);
		}

		private DownloadDescription CachedDownloadDescription = null;
	}

	
	private class RecreateCallbackBase
	{
		public RecreateCallbackBase(DownloadDescription CachedDownloadDescription)
		{
			this.CachedDownloadDescription = CachedDownloadDescription;
		}
		private void call_internal()
		{
			if (null != CachedDownloadDescription) 
			{
				QueueNewDownloadDescription(CachedDownloadDescription);
			}
		}
		
		private DownloadDescription CachedDownloadDescription = null;
	}
	private class RecreateCallbackWithDownload extends RecreateCallbackBase implements Func<Download>
	{
		public RecreateCallbackWithDownload(DownloadDescription CachedDownloadDescription)
		{
			super(CachedDownloadDescription);
		}
		
		@Override
		public void call(@NonNull Download CancelledDownload)
		{
			super.call_internal();
		}
	}
	private class RecreateCallbackWithError extends RecreateCallbackBase implements Func<Error>
	{
		public RecreateCallbackWithError(DownloadDescription CachedDownloadDescription)
		{
			super(CachedDownloadDescription);
		}
		
		@Override
		public void call(@Nullable Error CancelError)
		{
			super.call_internal();
		}
	}

	//Helper class to handle routing calls from the FetchInstance's hasActiveDownloads function back to our FetchManager
	private class CheckForActiveDownloadsFunc implements Func<Boolean>
	{
		@Override
		public void call(Boolean bHasActiveDownloads)
		{
			if (null != bHasActiveDownloads)
			{
				if (IsValid())
				{
					Owner.HandleHasActiveDownloadResult(bHasActiveDownloads, ProgressListener);
				}
			}
		}

		public CheckForActiveDownloadsFunc(FetchManager Owner, DownloadProgressListener ProgressListener)
		{
			this.Owner = Owner;
			this.ProgressListener = ProgressListener;
		}

		private boolean IsValid()
		{
			return ((null != Owner) && (null != ProgressListener));
		}

		private FetchManager Owner;
		private DownloadProgressListener ProgressListener;
	}
	
	private String GetTempDownloadDestination(DownloadDescription DownloadDesc)
	{
		return DownloadDesc.DestinationLocation + TempFileExtension;
	}

	private void HandleHasActiveDownloadResult(boolean bHasActiveDownloads, DownloadProgressListener ProgressListener)
	{
		if (!bHasActiveDownloads)
		{
			Log.error("No active downloads in FetchInstance, but still waiting on downloads to finish! Acting as if all downloads completed.");
			//Send a failure download complete notification since we have an unexpected error where we are still trying to do work
			//without any active downloads in our FetchManager
			SendAllDownloadsCompleteNotification(ProgressListener, true);
		}
	}

	private void CompleteDownload(DownloadDescription DownloadDesc, ECompleteReason CompleteReason)
	{
		if (false == HasValidProgressCallback(DownloadDesc))
		{
			Log.error("Call to CompleteDownload with an invalid DownloadDescription! RequestID:" + DownloadDesc.RequestID);
			return;
		}
		
		EDownloadCompleteReason CompleteReasonToSend = ConvertCompleteReasonForDownload(CompleteReason);
		
		//Mark download as complete
		DownloadDesc.PreviousDownloadPercent = 100;
		CompletedDownloads.put(DownloadDesc.RequestID, DownloadDesc);

		//Only bubble up to the UEDownloadWorker non-intenional completes as we initiated any other completes internally
		if (IsCompleteReasonIntentional(CompleteReason))
		{
			Log.verbose("Call to CompleteDownload taking no action as complete reason was intentional! RequestID:" + DownloadDesc.RequestID + " CompleteReason:" + CompleteReason);
			return;
		}

		//Test if we were successful, and if so lets try and move our successful download to the non-temp location
		if (CompleteReason == FetchRequestProgressListener.ECompleteReason.Success)
		{
			String TempDownloadLoc = GetTempDownloadDestination(DownloadDesc);
			File CompletedFile = new File(TempDownloadLoc);
			File NewFileAtDestinationLocation = new File(DownloadDesc.DestinationLocation);

			if (NewFileAtDestinationLocation.exists())
			{
				Log.debug("Completed download that already exists! Completing with original file " + DownloadDesc.DestinationLocation);
				
				if (CompletedFile.exists())
				{
					Log.error("Completed download that already existed was also re-downloaded! Should not happen! Deleting dupicate tempfile at " + TempDownloadLoc);
					CompletedFile.delete();
				}
			}
			else
			{
				//Attempt rename, and if it fails we need to treat this download as an error
				if (!CompletedFile.renameTo(NewFileAtDestinationLocation))
				{
					Log.error("Failed to rename " + TempDownloadLoc + " to " + DownloadDesc.DestinationLocation + " ! Download has Errored.");
				
					//Overwrite our successful complete reason with an error.
					CompleteReasonToSend = EDownloadCompleteReason.Error;
					FailedDownloads.put(DownloadDesc.RequestID, DownloadDesc);
				}
			}
		}
		else
		{
			FailedDownloads.put(DownloadDesc.RequestID, DownloadDesc);
		}

		//Flag any successful complete so we don't redownload it on retries
		if (CompleteReasonToSend == EDownloadCompleteReason.Success)
		{
			DownloadDesc.bHasCompleted = true;
		}

		DownloadDesc.ProgressListener.OnDownloadComplete(DownloadDesc.RequestID, DownloadDesc.DestinationLocation, CompleteReasonToSend);		
		CheckForAllDownloadsComplete(DownloadDesc.ProgressListener);
	}
	
	private EDownloadCompleteReason ConvertCompleteReasonForDownload(ECompleteReason CompleteReason)
	{
		switch (CompleteReason)
		{
			case Error:
				return EDownloadCompleteReason.Error; 
			case Success:
				return EDownloadCompleteReason.Success;
			case OutOfRetries:
				return EDownloadCompleteReason.OutOfRetries;
			default:
				return EDownloadCompleteReason.Error;
		}
	}
	
	private void CheckForAllDownloadsComplete(DownloadProgressListener ProgressListener)
	{
		if (CompletedDownloads.size() < RequestedDownloads.size())
		{
			return;
		}

		if (CompletedDownloads.size() > RequestedDownloads.size())
		{
			Log.error("Error in CompleteDownload logic! CompletedDownloads.size:" + CompletedDownloads.size() + " RequstedDownloads.size():" + RequestedDownloads.size());
		}
		
		SendAllDownloadsCompleteNotification(ProgressListener, false);
	}

	private void SendAllDownloadsCompleteNotification(DownloadProgressListener ProgressListener, boolean bForceFailure)
	{
		boolean bDidAllSucceed = ((bForceFailure == false) && (FailedDownloads.isEmpty()));
		ProgressListener.OnAllDownloadsComplete(bDidAllSucceed);
	}
	
	private String GetRequestID(Download download)
	{
		if (null != download)
		{
			return download.getTag();
		}
		
		return "";
	}

	private boolean HasValidProgressCallback(DownloadDescription DownloadDesc)
	{
		return ((null != DownloadDesc) && (null != DownloadDesc.ProgressListener));
	}
	//Used to determine if the reason a download completed was because of action we took. IE: Cancelling, deleting, removing, etc that happened through calls to the FetchManager.
	//Or if it was a change of status by hitting an error, succeeding, etc.
	private boolean IsCompleteReasonIntentional(FetchRequestProgressListener.ECompleteReason CompleteReason)
	{
		switch (CompleteReason)
		{
			//All cases that are intentional should be here
			case Cancelled:
			case Deleted:
			case Removed:
				return true;
			//by default assume its not intentional
			default:
				return false;
		}
	}
	//private boolean ShouldRetryRequest(@NonNull Request 
	
	private Fetch FetchInstance = null;

	private FetchRequestProgressListener FetchListener = null;
	
	private HashMap<String, DownloadDescription> RequestedDownloads = new HashMap<String, DownloadDescription>();
	private HashMap<String, DownloadDescription> CompletedDownloads = new HashMap<String, DownloadDescription>();
	private HashMap<String, DownloadDescription> FailedDownloads = new HashMap<String, DownloadDescription>();

	public String TempFileExtension = ".fetchtemp";
	
	public Logger Log = new Logger("UE", "FetchManager");
}