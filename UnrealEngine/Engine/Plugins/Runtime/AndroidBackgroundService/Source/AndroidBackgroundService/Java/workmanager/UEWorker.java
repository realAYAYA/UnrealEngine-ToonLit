// Copyright Epic Games, Inc. All Rights Reserved.
package com.epicgames.unreal.workmanager;

import android.content.Context;
import android.util.Log;

import androidx.annotation.Nullable;
import androidx.annotation.NonNull;

import androidx.work.Worker;
import androidx.work.WorkerParameters;

import java.util.Set;

import com.epicgames.unreal.Logger;

import com.epicgames.unreal.GameActivity;

import com.google.common.util.concurrent.ListenableFuture;

//Base implementation for custom UE Workers
public class UEWorker extends Worker
{
	public Logger Log = new Logger("UE", "UEWorker");
	
	public UEWorker(@NonNull Context context,@NonNull WorkerParameters params)
	{
		super(context, params);
	}

	//Our UE wrappers on the base WorkManager Worker class' functions. Final as we want to force overwritting OnWorkerStart, OnWorkerStop 
	@NonNull
	@Override
	public final Result doWork()
	{
		Log.debug("doWork Starting for Worker:" + GetWorkID());

		try
		{
			InitWorker();
			doWork_Internal(GetWorkID());
		}
		catch(Exception exp)
		{
			Log.error("Exception hit during doWork for Worker:" + GetWorkID());

			//Only want to fail on exception if we haven't already recieved a result from UE4.
			//Want to honor the UE4 result even if we hit an exception after it finished since it represents expected behavior
            if (!bReceivedResult)
            {
                SetWorkResult_Failure();
            }

			//Always print stack trace for exception
			exp.printStackTrace();
		}
		finally
		{
			Log.debug("doWork ending for Worker:" + GetWorkID() + " with CachedResult:" + CachedResult);

			//bubble up to UE that our Worker is finished working
			CallNativeOnWorkerStop(GetWorkID());

			//Should be set by either the exception or through a callback to this object from the UE native code
			return CachedResult;
		}
	}

	private void doWork_Internal(String WorkID)
	{
		OnWorkerStart(WorkID);

		if (!bReceivedResult)
		{
			Log.warn("OnWorkerStart completed without explicitly setting a work result through SetWorkResult methods! Will end up retrying by default! WorkID: " + WorkID);
		}
	}
	
	@NonNull
	@Override
	public final void onStopped()
	{
		OnWorkerStopped(GetWorkID());
	}

	//
	//These functions are designed to be overridden by custom workers to handle the actual task of doing their work.
	//
	
	//Always called before OnWorkerStart so that we can handle any init needed before doing our work
	public void InitWorker()
	{
		//by default we set this to retry as if an unexpected error occurs and we don't get a result from UE4 code we should retry later
		CachedResult = Result.retry();
        bReceivedResult = false;

		//Ensure GameActivity has loaded its static initializers as this is where the UE lib is loaded 
		//as well as when JNI loads the native functions in it otherwise we will crash with a JNI error 
		//when attempting to call native functions that JNI can't find
		GameActivity.ForceLoadThisClass();
	}
	
	//Called when we are starting our actual work
	public void OnWorkerStart(String WorkID)
	{
		Log.verbose("OnWorkerStart for WorkID: " + WorkID);
		CallNativeOnWorkerStart(WorkID);
	}
	
	//Called if the Android system is shutting down our Worker to give us a chance to clean up and set failure/retry results if needed.
	public void OnWorkerStopped(String WorkID)
	{
		Log.verbose("OnWorkerStopped for WorkID: " + WorkID);
		CallNativeOnWorkerStop(WorkID);
	}

	//Grabs our WorkID from the InputData supplied to our Worker
	@NonNull
	public String GetWorkID()
	{
		String ReturnedString;

		String WorkID = getInputData().getString("WorkID");
		if (WorkID == null)
		{
			ReturnedString = "Error_NoWorkID";
			Log.error("Invalid UEWorker queued! No WorkID supplied in the input data!");
		}
		else
		{
			ReturnedString = WorkID;
		}

		return ReturnedString;
	}

	public boolean DidReceiveResult()
	{
		return bReceivedResult;
	}

	public boolean DidWorkEndInSuccess()
	{
		//If we haven't received a result yet, just always return false
		if (!bReceivedResult)
		{
			return false;
		}
		
		return (CachedResult == Result.success());
	}
	
	public boolean DidWorkEndInFailure()
	{
		//If we haven't received a result yet, just always return false
		if (!bReceivedResult)
		{
			return false;
		}
		
		return (CachedResult == Result.failure());
	}
	
	public boolean DidWorkEndInRetry()
	{
		//If we haven't received a result yet, just always return false
		if (!bReceivedResult)
		{
			return false;
		}
		
		return (CachedResult == Result.retry());
	}
	
	//If our work will run again in a retry or not. This is different from !DidWorkEndInRetry as if we haven't received a result yet, the answer is false.
	public boolean IsWorkEndTerminal()
	{
		if (!bReceivedResult)
		{
			return false;
		}
		
		return !DidWorkEndInRetry();
	}

	//
	// The following functions are callbacks we expect to have called during our work to determine how the system handles that work
	// 
	
	//Work will be considered successful and complete
	public void SetWorkResult_Success()
    {
		if (!bReceivedResult)
		{
			CachedResult = Result.success();
			bReceivedResult = true;
		}
		else
		{
			//Prioritize teh failure or retry state over the success as those are more likely to require special attention that we don't want to stamp out.
			Log.error("Call to SetWorkResult_Success ignored as work result was already previously set! Should be careful to only call once!");
		}
    }
	
	//Work will be considered unsucessful and need to be retried based on the WorkRequest's retry rules. Note that any Work that have pre-requisites on this Work will not be able to be started until it succeeds! 
	//WARNING: If the goal is to have a re-curring task that should be accomplished through the WorkRequest parameters and not by calling Retry for the above stated reason!
    public void SetWorkResult_Retry()
    {
		if (bReceivedResult)
		{
			Log.error("Call to SetWorkResult_Retry after a previous call to a SetWorkResult function was already done! This will stomp the previous result!");
		}
		
		CachedResult = Result.retry();
		bReceivedResult = true;
    }

	//Work will be considered unsuccesfull and complete
    public void SetWorkResult_Failure()
    {
		if (bReceivedResult)
		{
			Log.error("Call to SetWorkResult_Failure after a previous call to a SetWorkResult function was already done! This will stomp the previous result!");
		}
		
        CachedResult = Result.failure();
        bReceivedResult = true;
    }

	//overridable code to call the respective native OnWorkerStart for this worker
	public void CallNativeOnWorkerStart(String WorkID)
	{
		nativeAndroidBackgroundServicesOnWorkerStart(WorkID);
	}

	//overridable code to call the respective native OnWorkerStop for this worker
	public void CallNativeOnWorkerStop(String WorkID)
	{
		nativeAndroidBackgroundServicesOnWorkerStop(WorkID);
	}

	//Native functions used to bubble up to native UE code
	public native void nativeAndroidBackgroundServicesOnWorkerStart(String WorkID);
	public native void nativeAndroidBackgroundServicesOnWorkerStop(String WorkID);

	//by default if we don't get an answer we should retry as something weird happened that prevented us from
    //doing anything significant in our native call
    protected Result CachedResult;

    //flags if we are still using the original cached result or if the native code successfully set a result
    protected volatile boolean bReceivedResult;
}