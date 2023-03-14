// Copyright Epic Games, Inc. All Rights Reserved.
package com.epicgames.unreal.download.fetch;

import androidx.annotation.NonNull;

import com.tonyodev.fetch2.Error;
import com.tonyodev.fetch2.Request;
import com.tonyodev.fetch2core.Func;

//Interface describing a class that wants to receive results from a Fetch2 enqueue call
public interface FetchEnqueueResultListener
{
	//Function called on this FetchEnqueueResultListener by the FetchEnqueueRequestCallback
	public void OnFetchEnqueueRequestCallback(@NonNull String RequestID, @NonNull Request EnqueuedRequest);
	
	//Function called on this FetchEnqueueResultListener by the FetchEnqueueErrorCallback
	public void OnFetchEnqueueErrorCallback(@NonNull String RequestID, @NonNull Error EnqueueError);
	
	//Helper class passed in to the Fetch.enqueue command to pass back the results of the enqueue operation on success.
	public class FetchEnqueueRequestCallback implements Func<Request>
	{
		public FetchEnqueueRequestCallback(@NonNull FetchEnqueueResultListener Owner, @NonNull String CachedRequestID)
		{
			this.Owner = Owner;
			this.CachedRequestID = CachedRequestID;
		}

		@Override
		public void call(@NonNull Request result)
		{
			if (Owner != null) 
			{
				Owner.OnFetchEnqueueRequestCallback(CachedRequestID, result);
			}			
			Owner = null;
		}

		FetchEnqueueResultListener Owner = null;
		String CachedRequestID = null;
	}

	//Helper class passed in to the Fetch.enqueue command to pass back the results of the enqueue operation on error
	public class FetchEnqueueErrorCallback implements Func<Error>
	{
		public FetchEnqueueErrorCallback(@NonNull FetchEnqueueResultListener Owner, @NonNull String CachedRequestID)
		{
			this.Owner = Owner;
			this.CachedRequestID = CachedRequestID;
		}

		@Override
		public void call(@NonNull Error error)
		{
			if (Owner != null) 
			{
				Owner.OnFetchEnqueueErrorCallback(CachedRequestID, error);
			}
			Owner = null;
		}
		
		FetchEnqueueResultListener Owner = null;
		String CachedRequestID = null;
	}
}