// Copyright Epic Games, Inc. All Rights Reserved.

#include "IOS/IOSBackgroundURLSessionHandler.h"

#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"

#include "HAL/PlatformFile.h"
#include "HAL/PlatformFileManager.h"

#include "IOS/IOSAppDelegate.h"

FIOSBackgroundDownloadCoreDelegates::FIOSBackgroundDownload_DidFinishDownloadingToURL FIOSBackgroundDownloadCoreDelegates::OnIOSBackgroundDownload_DidFinishDownloadingToURL;
FIOSBackgroundDownloadCoreDelegates::FIOSBackgroundDownload_DidWriteData FIOSBackgroundDownloadCoreDelegates::OnIOSBackgroundDownload_DidWriteData;
FIOSBackgroundDownloadCoreDelegates::FIOSBackgroundDownload_DidCompleteWithError FIOSBackgroundDownloadCoreDelegates::OnIOSBackgroundDownload_DidCompleteWithError;
FIOSBackgroundDownloadCoreDelegates::FIOSBackgroundDownload_SessionDidFinishAllEvents FIOSBackgroundDownloadCoreDelegates::OnIOSBackgroundDownload_SessionDidFinishAllEvents;
FIOSBackgroundDownloadCoreDelegates::FIOSBackgroundDownload_DelayedBackgroundURLSessionCompleteHandler FIOSBackgroundDownloadCoreDelegates::OnDelayedBackgroundURLSessionCompleteHandler;

NSURLSession* FBackgroundURLSessionHandler::BackgroundSession = nil;
FString FBackgroundURLSessionHandler::CachedIdentifierName = FString();
volatile int32 FBackgroundURLSessionHandler::DelayedBackgroundURLSessionCompleteCount = 0;

BackgroundHttpFileHashHelperRef FBackgroundURLSessionHandler::FileHashHelper = MakeShared<FBackgroundHttpFileHashHelper, ESPMode::ThreadSafe>();

bool FBackgroundURLSessionHandler::InitBackgroundSession(const FString& SessionIdentifier)
{
	FIOSBackgroundDownloadCoreDelegates::OnDelayedBackgroundURLSessionCompleteHandler.BindStatic(&FBackgroundURLSessionHandler::OnDelayedBackgroundURLSessionCompleteHandlerCalled);
	
	//Make sure our BackgroundHttpFileHashHelper is initialized and loaded to handle any completed requests asking for Temp file paths
	GetFileHashHelper()->LoadData();

	bool bHasSessionWithIdentifier = false;

	if (!SessionIdentifier.IsEmpty())
	{
		//if we don't already have a background session, create it
		if (nil == BackgroundSession)
		{
			//Create actual session
			{
				float BackgroundHttpTimeout = 90.0f;
				GConfig->GetFloat(TEXT("BackgroundHttp.iOSSettings"), TEXT("BackgroundReceiveTimeout"), BackgroundHttpTimeout, GEngineIni);

                float BackgroundHttpResourceTimeout = 3600.f;
                GConfig->GetFloat(TEXT("BackgroundHttp.iOSSettings"), TEXT("BackgroundHttpResourceTimeout"), BackgroundHttpResourceTimeout, GEngineIni);
                
				NSURLSessionConfiguration *config = [NSURLSessionConfiguration backgroundSessionConfigurationWithIdentifier : SessionIdentifier.GetNSString()];
				config.discretionary = NO;
				config.sessionSendsLaunchEvents = YES;
				config.shouldUseExtendedBackgroundIdleMode = YES;
				config.timeoutIntervalForRequest = BackgroundHttpTimeout;	        //Timeout for requests not getting a response from server at the iOS level. Will still work in the BG.
				config.timeoutIntervalForResource = BackgroundHttpResourceTimeout;  //Will automatically retry everytime above setting causes a timeout until this many seconds pass. Then we will get
                                                                                    //an error result. No Error for server not responding until is sent by a background task until this timeout is reached.
                //Other unused settings we might want to configure
				//HTTPMaximumConnectionsPerHost
				//allowsCellularAccess
                
                //Can't use on BG session, but may need to consider for any future FG session work:
				//waitsForConnectivity  -- This is forced true on background sessions
				//URLSession:taskIsWaitingForConnectivity: method of NSURLSessionTaskDelegate -- Can't respond to this with background sessions since the above is always forced true they never call this delegate

                //NSURLSessionConfiguration* config = [NSURLSessionConfiguration defaultSessionConfiguration];
                BackgroundSession = [NSURLSession sessionWithConfiguration:config delegate:[[BackgroundDownloadDelegate alloc]init] delegateQueue:nil];
				[BackgroundSession retain];
			}

			//test if session created
			if (nil != BackgroundSession)
			{
				CachedIdentifierName = SessionIdentifier;
				bHasSessionWithIdentifier = true;

				//Make sure our temp directory exists where we will be putting files as they complete as we might get completion callbacks right after we init this session
				CreateBackgroundSessionWorkingDirectory();
			}
		}
		//we already have one, so just see if names match
		else
		{
			bHasSessionWithIdentifier = CachedIdentifierName.Equals(SessionIdentifier);
		}
	}

	return ensureMsgf(bHasSessionWithIdentifier, TEXT("Unable to initialize background session with identifier %s"), *SessionIdentifier);
}

void FBackgroundURLSessionHandler::ShutdownBackgroundSession(bool bShouldFinishTasksFirst /* = true */)
{
	if (nil != BackgroundSession)
	{
		if (bShouldFinishTasksFirst)
		{
			[BackgroundSession finishTasksAndInvalidate];
		}
		else
		{
			[BackgroundSession invalidateAndCancel];
		}

		[BackgroundSession release];
		BackgroundSession = nil;
	}

	CachedIdentifierName.Reset();
}

NSURLSession* FBackgroundURLSessionHandler::GetBackgroundSession()
{
	ensureAlwaysMsgf((nullptr != BackgroundSession), TEXT("Call to GetBackgroundSession with no valid BackgroundSession! Make sure InitBackgroundSession is called first!"));
	return BackgroundSession;
}

BackgroundHttpFileHashHelperRef FBackgroundURLSessionHandler::GetFileHashHelper()
{
	return FileHashHelper;
}

void FBackgroundURLSessionHandler::CreateBackgroundSessionWorkingDirectory()
{
	const FString& DirectoryPath = FBackgroundHttpFileHashHelper::GetTemporaryRootPath();
    
	if (ensureAlwaysMsgf(!DirectoryPath.IsEmpty(), TEXT("Invalid empty BackgroundSessionWorkingDirectoryPath!")))
	{
        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        PlatformFile.CreateDirectory(*DirectoryPath);
        
        if (!PlatformFile.DirectoryExists(*DirectoryPath))
        {
            NSString* PathAsNSString = DirectoryPath.GetNSString();
            //May not yet have UE_Log
            ensureAlwaysMsgf(false, TEXT("Warning: Error in CreateBackgroundSessionWorkingDirectory"));
        }
	}
}

void FBackgroundURLSessionHandler::CallBackgroundURLSessionCompleteHandler()
{
	NSLog(@"OnIOSBackgroundDownload_SessionDidFinishAllEvents Delegates ALL Finshed. Attempting to callback into iOS to inform session work is finished.");
	
	IOSAppDelegate* AppDelegate = [IOSAppDelegate GetDelegate];
	
	if ((nullptr != AppDelegate) && (nil != AppDelegate.BackgroundSessionEventCompleteDelegate))
	{
		//make a local copy of the completion handler to capture with the block
		void(^completionHandler)() = AppDelegate.BackgroundSessionEventCompleteDelegate;
		[completionHandler retain];
		
		//set the completion handler to nil to protect from anything else trying to use it and to release our copy
		AppDelegate.BackgroundSessionEventCompleteDelegate = nil;
		
	   //Need to issue our stored completion callback on main thread as its a UI callback
		[[NSOperationQueue mainQueue] addOperationWithBlock:^{
	
			//call the copied completion handler
			 NSLog(@"OnIOSBackgroundDownload_SessionDidFinishAllEvents Calling iOS Completion handler.");
			 completionHandler();
			
			//Release local copy so we don't leak the completion handler
			[completionHandler release];
		}];
	}
	else
	{
		NSLog(@"OnIOSBackgroundDownload_SessionDidFinishAllEvents Can not call iOS Completion Handler as it is not set");
	}
}

void FBackgroundURLSessionHandler::AddDelayedBackgroundURLSessionComplete()
{
	FPlatformAtomics::InterlockedIncrement(&DelayedBackgroundURLSessionCompleteCount);
}

void FBackgroundURLSessionHandler::OnDelayedBackgroundURLSessionCompleteHandlerCalled()
{
	int NewDecrementedCount = FPlatformAtomics::InterlockedDecrement(&DelayedBackgroundURLSessionCompleteCount);
	NSLog(@"OnIOSBackgroundDownload_SessionDidFinishAllEvents Delegate Finshed. Still pending: %d", NewDecrementedCount);
	
	if (NewDecrementedCount == 0)
	{
		CallBackgroundURLSessionCompleteHandler();
	}
}

@implementation BackgroundDownloadDelegate
	
-(void)URLSession:(NSURLSession *)session downloadTask : (NSURLSessionDownloadTask *)downloadTask didFinishDownloadingToURL : (NSURL *)location;
{
	//Go ahead and try and move the file out of IOS system temporary storage and into our own background download specific
	//temp storage that won't be deleted by iOS
	//We do this here because we need to handle this case even if nothing has subscribed to the delegate yet
	
	NSURL* destinationURL = nil;
	NSError* downloadError = [downloadTask error];
    const bool didTaskError = (downloadError != nullptr);
    
    //Get path on disk we need to move the completed iOS temp file to
    const FString TaskURL([[[downloadTask currentRequest] URL] absoluteString]);
	const FString& TempFileName = FBackgroundURLSessionHandler::GetFileHashHelper()->FindOrAddTempFilenameMappingForURL(TaskURL);
	const FString& DestinationPath = FBackgroundHttpFileHashHelper::GetFullPathOfTempFilename(TempFileName);
	
	if (!didTaskError && (nil != location))
	{
        //Convert UFS path to external path so we can pass it into iOS function to move iOS temp file
        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        FString ConvertedPath = PlatformFile.ConvertToAbsolutePathForExternalAppForWrite(*DestinationPath);
        
		destinationURL = [NSURL fileURLWithPath:ConvertedPath.GetNSString()];
        NSLog(@"Attempting to move iOS temp file to BackgroundHttp temp folder | iOSLocation:%s | DestinationLocation:%s", [[location path] UTF8String], [DestinationPath.GetNSString() UTF8String]);
        
		[[NSFileManager defaultManager] moveItemAtURL:location toURL:destinationURL error:&downloadError];
	}
    
	FIOSBackgroundDownloadCoreDelegates::OnIOSBackgroundDownload_DidFinishDownloadingToURL.Broadcast(downloadTask, downloadError, DestinationPath);
}

-(void)URLSession:(NSURLSession *)session downloadTask : (NSURLSessionDownloadTask *)downloadTask didWriteData:(int64_t)bytesWritten totalBytesWritten:(int64_t)totalBytesWritten totalBytesExpectedToWrite:(int64_t)totalBytesExpectedToWrite;
{	
	FIOSBackgroundDownloadCoreDelegates::OnIOSBackgroundDownload_DidWriteData.Broadcast(downloadTask, bytesWritten, totalBytesWritten, totalBytesExpectedToWrite);
}

-(void)URLSession:(NSURLSession *)session task : (NSURLSessionTask *)task didCompleteWithError : (NSError *)error;
{
    NSString* TaskURLString = [[[task currentRequest] URL] absoluteString];
    NSLog(@"Task ending with error. TaskURL:%s", [TaskURLString UTF8String]);

    FIOSBackgroundDownloadCoreDelegates::OnIOSBackgroundDownload_DidCompleteWithError.Broadcast(task, error);
}

-(void)URLSessionDidFinishEventsForBackgroundURLSession:(NSURLSession *)session;
{
	//Save any dirty hash data now that we have done all work for a given session
	//this way any newly completed downloads will have their URL mapping saved correctly
	FBackgroundURLSessionHandler::GetFileHashHelper()->SaveData();
	
	//Check if we have this delegate setup. If not our app may have not been loaded or we were evicted.
	const bool bHasURLSessionDelegateBeenSetup = FIOSBackgroundDownloadCoreDelegates::OnIOSBackgroundDownload_SessionDidFinishAllEvents.IsBound();
	
	if (bHasURLSessionDelegateBeenSetup)
	{
		NSLog(@"Calling into OnIOSBackgroundDownload_SessionDidFinishAllEvents Waiting for Delegates to complete.");
		
		//We call this here and then the completion handler ourselves below this. That way we delay the complete until all systems responding to this delegate have received the broadcast.
		FBackgroundURLSessionHandler::AddDelayedBackgroundURLSessionComplete();
		
		//First process other systems doing things for all events processing on URL Session
		FIOSBackgroundDownloadCoreDelegates::OnIOSBackgroundDownload_SessionDidFinishAllEvents.Broadcast(session, FIOSBackgroundDownloadCoreDelegates::OnDelayedBackgroundURLSessionCompleteHandler);
		
		//Now call the completion handler to erase the delay we had above
		FBackgroundURLSessionHandler::OnDelayedBackgroundURLSessionCompleteHandlerCalled();
	}
	//Handle restarting any suspended tasks. These can exist because we didn't resume these before our app was evicted.
	else
	{
		NSLog(@"Resuming all tasks regardless of request state as we have no registered handler of OnIOSBackgroundDownload_SessionDidFinishAllEvents");
		
		[session getTasksWithCompletionHandler : ^ (NSArray<__kindof NSURLSessionDataTask*>* DataTasks, NSArray<__kindof NSURLSessionUploadTask*>* UploadTasks, NSArray<__kindof NSURLSessionDownloadTask*>* DownloadTasks)
		{
			for (NSURLSessionDownloadTask* DownloadTask : DownloadTasks)
			{
				if ([DownloadTask state] == NSURLSessionTaskStateSuspended)
				{
					NSLog(@"Resuming Task for URL:%s", [[[[DownloadTask currentRequest] URL] absoluteString] UTF8String]);
					[DownloadTask resume];
				}
			}
			
			//Go ahead and call the completion handler now that we have resumed everything.
			FBackgroundURLSessionHandler::OnDelayedBackgroundURLSessionCompleteHandlerCalled();
		}];
	}
}

@end
