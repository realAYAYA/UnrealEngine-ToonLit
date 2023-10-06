// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "CoreMinimal.h"

#include "BackgroundHttpFileHashHelper.h"

//Call backs called by the bellow FBackgroundURLSessionHandler so higher-level systems can respond to task updates.
class APPLICATIONCORE_API FIOSBackgroundDownloadCoreDelegates
{
public:
    DECLARE_MULTICAST_DELEGATE_ThreeParams(FIOSBackgroundDownload_DidFinishDownloadingToURL, NSURLSessionDownloadTask*, NSError*, const FString&);
    DECLARE_MULTICAST_DELEGATE_FourParams(FIOSBackgroundDownload_DidWriteData, NSURLSessionDownloadTask*, int64_t /*Bytes Written Since Last Call */, int64_t /*Total Bytes Written */, int64_t /*Total Bytes Expedted To Write */);
    DECLARE_MULTICAST_DELEGATE_TwoParams(FIOSBackgroundDownload_DidCompleteWithError, NSURLSessionTask*, NSError*);
	DECLARE_DELEGATE(FIOSBackgroundDownload_DelayedBackgroundURLSessionCompleteHandler);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FIOSBackgroundDownload_SessionDidFinishAllEvents, NSURLSession*, FIOSBackgroundDownload_DelayedBackgroundURLSessionCompleteHandler);
	
	static FIOSBackgroundDownload_DidFinishDownloadingToURL OnIOSBackgroundDownload_DidFinishDownloadingToURL;
	static FIOSBackgroundDownload_DidWriteData OnIOSBackgroundDownload_DidWriteData;
	static FIOSBackgroundDownload_DidCompleteWithError OnIOSBackgroundDownload_DidCompleteWithError;
	static FIOSBackgroundDownload_SessionDidFinishAllEvents OnIOSBackgroundDownload_SessionDidFinishAllEvents;
	static FIOSBackgroundDownload_DelayedBackgroundURLSessionCompleteHandler OnDelayedBackgroundURLSessionCompleteHandler;
};

//Interface for wrapping a NSURLSession configured to support background downloading of NSURLSessionDownloadTasks.
//This exists here as we can have to re-associate with our background session after app launch and need to re-associate with downloads
//right away before the HttpModule is loaded.
class APPLICATIONCORE_API FBackgroundURLSessionHandler
{
public:
	// Initializes a BackgroundSession with the given identifier. If the current background session already exists, returns true if the identifier matches. False if identifier doesn't match or if the session fails to create.
	static bool InitBackgroundSession(const FString& SessionIdentifier);

	//bShouldInvalidateExistingTasks determines if the session cancells all outstanding tasks immediately and cancels the session immediately or waits for them to finish and then invalidates the session
	static void ShutdownBackgroundSession(bool bShouldFinishTasksFirst = true);

	//Gets a pointer to the current background session
	static NSURLSession* GetBackgroundSession();

	static void CreateBackgroundSessionWorkingDirectory();
	
	//Function to mark if you would like for the NSURLSession to wait to call the completion handler when
	//OnIOSBackgroundDownload_SessionDidFinishAllEvents is called for you to call the passed completion handler
	//NOTE: Call DURING OnIOSBackgroundDownload_SessionDidFinishAllEvents
	static void AddDelayedBackgroundURLSessionComplete();
	
	//Function to handle calls to OnDelayedBackgroundURLSessionCompleteHandler
	//The intention is to call this for every call to AddDelayedBackgroundURLSessionComplete.
	//NOTE: Once calling this your task should be completely finished with work and ready to be backgrounded!
	static void OnDelayedBackgroundURLSessionCompleteHandlerCalled();
	
	static BackgroundHttpFileHashHelperRef GetFileHashHelper();
	
private:
	static NSURLSession* BackgroundSession;
	static FString CachedIdentifierName;
	
	static BackgroundHttpFileHashHelperRef FileHashHelper;
	
	//Used to track calls to AddDelayedBackgroundURLSessionComplete vs calls to the completion handler.
	static volatile int32 DelayedBackgroundURLSessionCompleteCount;
	
	//Used to call the stored background url session callback
	static void CallBackgroundURLSessionCompleteHandler();
};

//Delegate object associated with our above NSURLSession
@interface BackgroundDownloadDelegate : NSObject<NSURLSessionDownloadDelegate>
@end
