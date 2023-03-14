// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BackgroundHttpManagerImpl.h"
#include "Interfaces/IBackgroundHttpRequest.h"
#include "IOS/ApplePlatformBackgroundHttpRequest.h"

#include "IOS/IOSBackgroundURLSessionHandler.h"

typedef TWeakPtr<FApplePlatformBackgroundHttpRequest,ESPMode::ThreadSafe> FBackgroundHttpURLMappedRequestPtr;

DECLARE_DELEGATE(FIOSBackgroundHttpPostSessionWorkCallback);

/**
 * Manages Background Http request that are currently being processed if we are on an Apple Platform
 */
class BACKGROUNDHTTP_API FApplePlatformBackgroundHttpManager
	: public FBackgroundHttpManagerImpl
{
public:
	virtual void Initialize() override;
	virtual void Shutdown() override;
	virtual void AddRequest(const FBackgroundHttpRequestPtr Request) override;
	virtual void RemoveRequest(const FBackgroundHttpRequestPtr Request) override;
	virtual bool IsGenericImplementation() const override { return false;  }
	virtual void SetMaxActiveDownloads(int MaxActiveDownloads) override;

	/**
	* Constructor
	*/
	FApplePlatformBackgroundHttpManager();

	/**
	 * Destructor
	 */
	virtual ~FApplePlatformBackgroundHttpManager();

    //FTSTickerObjectBase implementation
    virtual bool Tick(float DeltaTime) override;
    
    //This is the identifier we expect to be associated with our NSURLDownloadSession for background downloads
    static FString BackgroundSessionIdentifier;
    
    //This value is pulled from .ini settings and is used to time out requests when we are active in the foreground
    static float ActiveTimeOutSetting;
    
	//This value is pulled from .ini setting and is used to limit the number of times a request will try and use the same
	//resume data before just ignoring that data and moving onto another CDN retry
	static int RetryResumeDataLimitSetting;

protected:
	virtual BackgroundHttpFileHashHelperRef GetFileHashHelper() override;
	virtual const BackgroundHttpFileHashHelperRef GetFileHashHelper() const override;
	
	bool AssociateWithAnyExistingUnAssociatedTasks(const FBackgroundHttpRequestPtr Request);
        
	bool CheckForExistingUnAssociatedTask(const FAppleBackgroundHttpRequestPtr Request);

    void DeletePendingRemoveRequests();
	
protected:
	//This dictionary is used to hold tasks that already existed on our Background Session when our BackgroundHttpManager was initialized. See PopulateUnAssociatedTasks and CheckForExistingUnAssociatedTask
	NSMutableDictionary<NSString*, NSURLSessionDownloadTask*>* UnAssociatedTasks;
	FRWLock UnAssociatedTasksLock;
	
	//Map to hold the associated BackgroundHttpRequest for any given URL. Multiple URLs will end up pointing to the same request in this list as it stores
    TMap<const FString, FBackgroundHttpURLMappedRequestPtr> URLToRequestMap;
	FRWLock URLToRequestMapLock;

	//Used to track if we came into certain code branches through a pathway that has initialized the manager yet or not.
	//Some of our platform callbacks can end up hitting VERY early or in background versions of our app without the engine being fully spun-up.
	static volatile bool bWasAppleBGHTTPInitialized;
	
private:
	//Checks for tasks that already exist on the BackgroundSession. Should only be called during Initialize. These tasks should really only exist 
	//due to previous application sessions starting background downloads that finished after the app closed. (IE: If the user started a patch and then our app was terminated by the OS while 
	//suspended. On next launch we will have re-associated those existing downloads to our background download session, but don't yet have a FBackgroundHttpRequest for that task.)
	//See AssociateWithAnyExistingRequest for how these Tasks are eventually claimed by an FBackgroundHttpRequest
	void PopulateUnAssociatedTasks();

	void SetupNSURLSessionResponseDelegates();
	void CleanUpNSURLSessionResponseDelegates();

    //These are used internally to pause/resume things at the task level
	void PauseAllActiveTasks();
	void ResumeTasksForBackgrounding(FIOSBackgroundHttpPostSessionWorkCallback Callback = FIOSBackgroundHttpPostSessionWorkCallback());
	bool ResumeDownloadTaskForBackgroundingIfAppropriate(NSURLSessionDownloadTask* DownloadTask, const FAppleBackgroundHttpRequestPtr Request, EBackgroundHTTPPriority LowestPriorityToQueue);

    void PauseAllUnassociatedTasks();
    void UnpauseAllUnassociatedTasks();
    
    void TickRequests(float DeltaTime);
    void TickUnassociatedTasks(float DeltaTime);
    
    void StartRequest(FAppleBackgroundHttpRequestPtr Request);
	void FinishRequest(FAppleBackgroundHttpRequestPtr Request);
	void RetryRequest(FAppleBackgroundHttpRequestPtr Request, bool bShouldIncreaseRetryCount, NSData* RetryData);
    void RemoveSessionTasksForRequest(FAppleBackgroundHttpRequestPtr Request);
    bool GenerateURLMapEntriesForRequest(FAppleBackgroundHttpRequestPtr Request);
    void RemoveURLMapEntriesForRequest(FAppleBackgroundHttpRequestPtr Request);
    
	bool IsRetryDataValid(NSData* RetryData) const;
	bool ShouldUseRequestRetryData(FAppleBackgroundHttpRequestPtr Request, NSData* RetryData) const;

	//Delegates to respond to Background Session Events
	void OnApp_EnteringForeground();
	void OnApp_EnteringBackground();
	void OnTask_DidFinishDownloadingToURL(NSURLSessionDownloadTask* Task, NSError* Error, const FString& TempFilePath);
	void OnTask_DidWriteData(NSURLSessionDownloadTask* Task, int64_t BytesWrittenSinceLastCall, int64_t TotalBytesWritten, int64_t TotalBytesExpectedToWrite);
	void OnTask_DidCompleteWithError(NSURLSessionTask* Task, NSError* Error);
	void OnSession_SessionDidFinishAllEvents(NSURLSession* Session, FIOSBackgroundDownloadCoreDelegates::FIOSBackgroundDownload_DelayedBackgroundURLSessionCompleteHandler Callback);

	FDelegateHandle OnApp_EnteringForegroundHandle;
	FDelegateHandle OnApp_EnteringBackgroundHandle;
	FDelegateHandle OnTask_DidFinishDownloadingToURLHandle;
	FDelegateHandle OnTask_DidWriteDataHandle;
	FDelegateHandle OnTask_DidCompleteWithErrorHandle;
	FDelegateHandle OnSession_SessionDidFinishAllEventsHandle;

	volatile int bHasFinishedPopulatingUnassociatedTasks;
	volatile int bIsInBackground;
    volatile int bIsIteratingThroughSessionTasks;
    
    //Need to store requests to remove at the end of our tick as we find them during iteration
    TArray<FBackgroundHttpRequestPtr> RequestsPendingRemove;
    
    /** On iOS we need to track how many Tasks we have active. This is to replace the default implementations NumCurrentlyActiveRequests **/
    volatile int NumCurrentlyActiveTasks;
	int MaxNumActualTasks;
};
