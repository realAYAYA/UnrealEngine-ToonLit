// Copyright Epic Games, Inc. All Rights Reserved.
package com.epicgames.unreal.download;

import com.epicgames.unreal.download.UEDownloadWorker.EDownloadCompleteReason;

//Interface for a class that wants to be notified when a download completes
public interface DownloadProgressListener
{
	public void OnDownloadProgress(String RequestID, long BytesWrittenSinceLastCall, long TotalBytesWritten);
	public void OnDownloadGroupProgress(int GroupID, int Progress, boolean Indeterminate);
	public void OnDownloadComplete(String RequestID, String CompleteLocation, EDownloadCompleteReason CompleteReason);
	public void OnAllDownloadsComplete(boolean bDidAllRequestsSucceed);
	public void OnDownloadEnqueued(String RequestID, boolean bEnqueueSuccess);
}