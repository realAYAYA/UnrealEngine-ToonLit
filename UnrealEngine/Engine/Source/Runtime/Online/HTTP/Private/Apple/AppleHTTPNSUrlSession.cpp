// Copyright Epic Games, Inc. All Rights Reserved.


#include "AppleHTTPNSUrlSession.h"
#include "HttpManager.h"
#include "Misc/EngineVersion.h"
#include "Misc/App.h"
#include "Misc/Base64.h"
#include "HAL/PlatformTime.h"
#include "Http.h"
#include "HttpModule.h"

/**
 * State of a request's response
 */
enum class EAppleHttpRequestResponseState: uint8
{
	NotReady,
	Success,
	Error,
	ConnectionError
};

/**
 * Class to hold data from delegate implementation notifications.
 */

@interface FAppleHttpNSUrlSessionResponseDelegate : NSObject<NSURLSessionDataDelegate>
{
	/** Holds the payload as we receive it. */
	@public TArray<uint8> Payload;
	
	/** This stream is shared between the request and the delegate. This field is accessed through a thread out of out control 
	 * from the delegates methods and through mehthods in FAppleHttpNSUrlSessionRequest. Once we cancel or have an error this can 
	 * is nullified as soon as possible to avoid receiving more data after completion delegates were triggered */
	TSharedPtr<FArchive> ResponseBodyReceiveStream;
	
	/** critical section to properly clear stream */
	FCriticalSection ResponseStreamLock;

	/** flag meant to reduce locking on ResponseStreamLock*/
	@public BOOL bInitializedWithValidStream;

	/** Delegate invoked after processing URLSession:task:didCompleteWithError:*/
	@public FTaskCompleteDelegate TaskCompleteDelegate;
}

/** A handle for the response */
@property(retain) NSHTTPURLResponse* Response;
/** The total number of bytes written out during the request/response */
@property uint64 BytesWritten;
/** The total number of bytes received out during the request/response */
@property uint64 BytesReceived;
/** Response state */
@property EAppleHttpRequestResponseState ResponseState;

/** NSURLSessionDataDelegate delegate methods. Those are called from a thread controlled by the NSURLSession */

/** Sent periodically to notify the delegate of upload progress. */
- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didSendBodyData:(int64_t)bytesSent totalBytesSent:(int64_t)totalBytesSent totalBytesExpectedToSend:(int64_t)totalBytesExpectedToSend;
/** The task has received a response and no further messages will be received until the completion block is called. */
- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveResponse:(NSURLResponse *)response completionHandler:(void (^)(NSURLSessionResponseDisposition disposition))completionHandler;
/** Sent when data is available for the delegate to consume. Data may be discontiguous */
- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveData:(NSData *)data;
/** Sent as the last message related to a specific task.  A nil Error implies that no error occurred and this task is complete. */
- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didCompleteWithError:(nullable NSError *)error;
/** Asks the delegate if it needs to store responses in the cache. */
- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask willCacheResponse:(NSCachedURLResponse *)proposedResponse completionHandler:(void (^)(NSCachedURLResponse *cachedResponse))completionHandler;
@end

@implementation FAppleHttpNSUrlSessionResponseDelegate
@synthesize Response;
@synthesize ResponseState;
@synthesize BytesWritten;
@synthesize BytesReceived;

-(FAppleHttpNSUrlSessionResponseDelegate*) initWithResponseStream:(TSharedPtr<FArchive>)ResponseStream
{
	self = [super init];
	
	BytesWritten = 0;
	BytesReceived = 0;
	ResponseState = EAppleHttpRequestResponseState::NotReady;
	ResponseBodyReceiveStream = ResponseStream;
	bInitializedWithValidStream = (ResponseStream != nullptr);
	
	return self;
}

- (void) ClearResponseStream
{
	if (bInitializedWithValidStream)
	{
	    FScopeLock Lock(&ResponseStreamLock);
		ResponseBodyReceiveStream = nullptr;
	}
}

- (void) dealloc
{
	[Response release];
	[super dealloc];
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didSendBodyData:(int64_t)bytesSent totalBytesSent:(int64_t)totalBytesSent totalBytesExpectedToSend:(int64_t)totalBytesExpectedToSend
{
	UE_LOG(LogHttp, Verbose, TEXT("URLSession:task:didSendBodyData:totalBytesSent:totalBytesExpectedToSend: totalBytesSent = %lld, totalBytesSent = %lld: %p"), totalBytesSent, totalBytesExpectedToSend, self);
	self.BytesWritten = totalBytesSent;
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveResponse:(NSURLResponse *)response completionHandler:(void (^)(NSURLSessionResponseDisposition disposition))completionHandler
{
	UE_LOG(LogHttp, Verbose, TEXT("URLSession:dataTask:didReceiveResponse:completionHandler"));
	
	self.Response = (NSHTTPURLResponse*)response;
	uint64 ExpectedResponseLength = response.expectedContentLength;
	if(!bInitializedWithValidStream && ExpectedResponseLength != NSURLResponseUnknownLength)
	{
		Payload.Empty(ExpectedResponseLength);
	}
	UE_LOG(LogHttp, Verbose, TEXT("URLSession:dataTask:didReceiveResponse:completionHandler: expectedContentLength = %lld. Length = %llu: %p"), ExpectedResponseLength, Payload.Max(), self);
	completionHandler(NSURLSessionResponseAllow);
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveData:(NSData *)data
{
	__block int64 NewBytesReceived = 0;
	if (bInitializedWithValidStream)
	{
		if (FScopeLock Lock(&ResponseStreamLock); ResponseBodyReceiveStream)
		{
			__block bool bHadError = false;
			[data enumerateByteRangesUsingBlock:^(const void *bytes, NSRange byteRange, BOOL *stop) {
				NewBytesReceived += byteRange.length;
				ResponseBodyReceiveStream->Serialize(const_cast<void*>(bytes), byteRange.length);
				bHadError = ResponseBodyReceiveStream->GetError();
				*stop = bHadError? YES : NO;
			}];
			
			if (bHadError)
			{
				[dataTask cancel];
				ResponseBodyReceiveStream = nullptr;
			}
		}
	}
	else
	{
		[data enumerateByteRangesUsingBlock:^(const void *bytes, NSRange byteRange, BOOL *stop) {
			NewBytesReceived += byteRange.length;
			Payload.Append((const uint8*)bytes, byteRange.length);
		}];
	}
	// Keep BytesReceived as a separated value to avoid concurrent accesses to Payload
	self.BytesReceived += NewBytesReceived;
	
	UE_LOG(LogHttp, Verbose, TEXT("URLSession:dataTask:didReceiveData with %llu bytes. After Append, Payload Length = %llu: %p"), NewBytesReceived, self.BytesReceived, self);
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didCompleteWithError:(nullable NSError *)error
{
	if (error == nil)
	{
		UE_LOG(LogHttp, Verbose, TEXT("URLSession:task:didCompleteWithError. Http request succeeded: %p"), self);
		self.ResponseState = EAppleHttpRequestResponseState::Success;
	}
	else
	{
		UE_LOG(LogHttp, Warning, TEXT("URLSession:task:didCompleteWithError. Http request failed - %s %s: %p"),
			   *FString([error localizedDescription]),
			   *FString([[error userInfo] objectForKey:NSURLErrorFailingURLStringErrorKey]),
			   self);
		
		// Determine if the specific error was failing to connect to the host.
		switch ([error code])
		{
			case NSURLErrorTimedOut:
			case NSURLErrorCannotFindHost:
			case NSURLErrorCannotConnectToHost:
			case NSURLErrorDNSLookupFailed:
				self.ResponseState = EAppleHttpRequestResponseState::ConnectionError;
				break;
			default:
				self.ResponseState = EAppleHttpRequestResponseState::Error;
		}
		// Log more details if verbose logging is enabled and this is an SSL error
		if (UE_LOG_ACTIVE(LogHttp, Verbose))
		{
			SecTrustRef PeerTrustInfo = reinterpret_cast<SecTrustRef>([[error userInfo] objectForKey:NSURLErrorFailingURLPeerTrustErrorKey]);
			if (PeerTrustInfo != nullptr)
			{
				SecTrustResultType TrustResult = kSecTrustResultInvalid;
				SecTrustGetTrustResult(PeerTrustInfo, &TrustResult);
				
				FString TrustResultString;
				switch (TrustResult)
				{
#define MAP_TO_RESULTSTRING(Constant) case Constant: TrustResultString = TEXT(#Constant); break;
						MAP_TO_RESULTSTRING(kSecTrustResultInvalid)
						MAP_TO_RESULTSTRING(kSecTrustResultProceed)
						MAP_TO_RESULTSTRING(kSecTrustResultDeny)
						MAP_TO_RESULTSTRING(kSecTrustResultUnspecified)
						MAP_TO_RESULTSTRING(kSecTrustResultRecoverableTrustFailure)
						MAP_TO_RESULTSTRING(kSecTrustResultFatalTrustFailure)
						MAP_TO_RESULTSTRING(kSecTrustResultOtherError)
#undef MAP_TO_RESULTSTRING
					default:
						TrustResultString = TEXT("unknown");
						break;
				}
				UE_LOG(LogHttp, Verbose, TEXT("URLSession:task:didCompleteWithError. SSL trust result: %s (%d)"), *TrustResultString, TrustResult);
			}
		}
	}
	TaskCompleteDelegate.ExecuteIfBound();
}

- (void)URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask willCacheResponse:(NSCachedURLResponse *)proposedResponse completionHandler:(void (^)(NSCachedURLResponse *cachedResponse))completionHandler
{
	// All FAppleHttpNSUrlSessionRequest use NSURLRequestReloadIgnoringLocalCacheData
	// NSURLRequestReloadIgnoringLocalCacheData disables loading of data from cache, but responses can still be stored in cache
	// Passing nil to this handler disables caching the responses
	completionHandler(nil);
}
@end

/****************************************************************************
 * FAppleHttpNSUrlSessionRequest implementation
 ***************************************************************************/

FAppleHttpNSUrlSessionRequest::FAppleHttpNSUrlSessionRequest(NSURLSession* InSession)
:   Session([InSession retain])
,   Task(nil)
,	bIsPayloadFile(false)
,	ContentBytesLength(0)
,	ElapsedTime(0.0f)
,	LastReportedBytesWritten(0)
,	LastReportedBytesRead(0)
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionRequest::FAppleHttpNSUrlSessionRequest()"));
	Request = [[NSMutableURLRequest alloc] init];
	Request.timeoutInterval = FHttpModule::Get().GetHttpTimeout();

	// Disable cache to mimic WinInet behavior
	Request.cachePolicy = NSURLRequestReloadIgnoringLocalCacheData;

	// Add default headers
	const TMap<FString, FString>& DefaultHeaders = FHttpModule::Get().GetDefaultHeaders();
	for (TMap<FString, FString>::TConstIterator It(DefaultHeaders); It; ++It)
	{
		SetHeader(It.Key(), It.Value());
	}
}

FAppleHttpNSUrlSessionRequest::~FAppleHttpNSUrlSessionRequest()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionRequest::~FAppleHttpNSUrlSessionRequest()"));
	CleanupRequest();
	[Request release];
    [Session release];
}

FString FAppleHttpNSUrlSessionRequest::GetURL() const
{
	SCOPED_AUTORELEASE_POOL;
	NSURL* URL = Request.URL;
	if (URL != nullptr)
	{
		FString ConvertedURL(URL.absoluteString);
		UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionRequest::GetURL() - %s"), *ConvertedURL);
		return ConvertedURL;
	}
	else
	{
		UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionRequest::GetURL() - NULL"));
		return FString();
	}
}

void FAppleHttpNSUrlSessionRequest::SetURL(const FString& URL)
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionRequest::SetURL() - %s"), *URL);
	Request.URL = [NSURL URLWithString: URL.GetNSString()];
}

FString FAppleHttpNSUrlSessionRequest::GetURLParameter(const FString& ParameterName) const
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionRequest::GetURLParameter() - %s"), *ParameterName);

	NSRange ParametersStart = [Request.URL.query rangeOfString:@"?"];
	if (ParametersStart.location != NSNotFound && ParametersStart.length > 0)
	{
		NSString* ParametersStr = [Request.URL.query substringFromIndex:ParametersStart.location + 1];
		NSString* ParameterNameStr = ParameterName.GetNSString();
		NSArray* Parameters = [ParametersStr componentsSeparatedByString:@"&"];
		for (NSString* Parameter in Parameters)
		{
			NSArray* KeyValue = [Parameter componentsSeparatedByString:@"="];
			NSString* Key = KeyValue[0];
			if ([Key compare:ParameterNameStr] == NSOrderedSame)
			{
				return FString(KeyValue[1]);
			}
		}
	}

	return FString();
}

FString FAppleHttpNSUrlSessionRequest::GetHeader(const FString& HeaderName) const
{
	SCOPED_AUTORELEASE_POOL;
	FString Header([Request valueForHTTPHeaderField:HeaderName.GetNSString()]);
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionRequest::GetHeader() - %s"), *Header);
	return Header;
}


void FAppleHttpNSUrlSessionRequest::SetHeader(const FString& HeaderName, const FString& HeaderValue)
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionRequest::SetHeader() - %s / %s"), *HeaderName, *HeaderValue );
	[Request setValue: HeaderValue.GetNSString() forHTTPHeaderField: HeaderName.GetNSString()];
}

void FAppleHttpNSUrlSessionRequest::AppendToHeader(const FString& HeaderName, const FString& AdditionalHeaderValue)
{
    if (!HeaderName.IsEmpty() && !AdditionalHeaderValue.IsEmpty())
    {
        NSDictionary* Headers = [Request allHTTPHeaderFields];
        NSString* PreviousHeaderValuePtr = [Headers objectForKey: HeaderName.GetNSString()];
        FString PreviousValue(PreviousHeaderValuePtr);
		FString NewValue;
		if (!PreviousValue.IsEmpty())
		{
			NewValue = PreviousValue + TEXT(", ");
		}
		NewValue += AdditionalHeaderValue;

        SetHeader(HeaderName, NewValue);
	}
}

TArray<FString> FAppleHttpNSUrlSessionRequest::GetAllHeaders() const
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionRequest::GetAllHeaders()"));
	NSDictionary* Headers = Request.allHTTPHeaderFields;
	TArray<FString> Result;
	Result.Reserve(Headers.count);
	for (NSString* Key in Headers.allKeys)
	{
		FString ConvertedValue(Headers[Key]);
		FString ConvertedKey(Key);
		UE_LOG(LogHttp, Verbose, TEXT("Header= %s, Key= %s"), *ConvertedValue, *ConvertedKey);

		Result.Add( FString::Printf( TEXT("%s: %s"), *ConvertedKey, *ConvertedValue ) );
	}
	return Result;
}

const TArray<uint8>& FAppleHttpNSUrlSessionRequest::GetContent() const
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionRequest::GetContent()"));
	StorageForGetContent.Empty();
	if (bIsPayloadFile)
	{
		UE_LOG(LogHttp, Warning, TEXT("FAppleHttpNSUrlSessionRequest::GetContent() called on a request that is set up for streaming a file. Return value is an empty buffer"));
	}
	else
	{
		SCOPED_AUTORELEASE_POOL;
		NSData* Body = Request.HTTPBody; // accessing HTTPBody will call retain autorelease on the value, increasing its retain count
		StorageForGetContent.Append((const uint8*)Body.bytes, Body.length);
	}
	return StorageForGetContent;
}

void FAppleHttpNSUrlSessionRequest::SetContent(const TArray<uint8>& ContentPayload)
{
	if (CompletionStatus == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("FAppleHttpNSUrlSessionRequest::SetContent() - attempted to set content on a request that is inflight"));
		return;
	}

	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionRequest::SetContent()"));
	Request.HTTPBody = [NSData dataWithBytes:ContentPayload.GetData() length:ContentPayload.Num()];
	ContentBytesLength = ContentPayload.Num();
	bIsPayloadFile = false;
}

void FAppleHttpNSUrlSessionRequest::SetContent(TArray<uint8>&& ContentPayload)
{
	if (CompletionStatus == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("FAppleHttpNSUrlSessionRequest::SetContent() - attempted to set content on a request that is inflight"));
		return;
	}

	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionRequest::SetContent()"));

	// We cannot use NSData dataWithBytesNoCopy:length:freeWhenDone: and keep the data in this instance because we don't have control
	// over the lifetime of the request copy that NSURLSessionTask keeps
	Request.HTTPBody = [NSData dataWithBytes:ContentPayload.GetData() length:ContentPayload.Num()];
	ContentBytesLength = ContentPayload.Num();
	bIsPayloadFile = false;

	// Clear argument content since client code probably expects that
	ContentPayload.Empty();
}

FString FAppleHttpNSUrlSessionRequest::GetContentType() const
{
	FString ContentType = GetHeader(TEXT("Content-Type"));
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionRequest::GetContentType() - %s"), *ContentType);
	return ContentType;
}

uint64 FAppleHttpNSUrlSessionRequest::GetContentLength() const
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionRequest::GetContentLength() - %i"), ContentBytesLength);
	return ContentBytesLength;
}

void FAppleHttpNSUrlSessionRequest::SetContentAsString(const FString& ContentString)
{
	if (CompletionStatus == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("FAppleHttpNSUrlSessionRequest::SetContentAsString() - attempted to set content on a request that is inflight"));
		return;
	}

	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionRequest::SetContentAsString() - %s"), *ContentString);
	FTCHARToUTF8 Converter(*ContentString);

	// The extra length computation here is unfortunate, but it's technically not safe to assume the length is the same.
	Request.HTTPBody = [NSData dataWithBytes:(ANSICHAR*)Converter.Get() length:Converter.Length()];
	ContentBytesLength = Converter.Length();
	bIsPayloadFile = false;
}

bool FAppleHttpNSUrlSessionRequest::SetContentAsStreamedFile(const FString& Filename)
{
	SCOPED_AUTORELEASE_POOL;
    UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionRequest::SetContentAsStreamedFile() - %s"), *Filename);

	if (CompletionStatus == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("FAppleHttpNSUrlSessionRequest::SetContentAsStreamedFile() - attempted to set content on a request that is inflight"));
		return false;
	}

	NSString* PlatformFilename = Filename.GetNSString();

	Request.HTTPBody = nil;

	struct stat FileAttrs = { 0 };
	if (stat(PlatformFilename.fileSystemRepresentation, &FileAttrs) == 0)
	{
		UE_LOG(LogHttp, VeryVerbose, TEXT("FAppleHttpNSUrlSessionRequest::SetContentAsStreamedFile succeeded in getting the file size - %lld"), FileAttrs.st_size);
		// Under the hood, the Foundation framework unsets HTTPBody, and takes over as the stream delegate.
		// The stream itself should be unopened when passed to setHTTPBodyStream.
		Request.HTTPBodyStream = [NSInputStream inputStreamWithFileAtPath: PlatformFilename];
		ContentBytesLength = FileAttrs.st_size;
		bIsPayloadFile = true;
	}
	else
	{
		UE_LOG(LogHttp, Warning, TEXT("FAppleHttpNSUrlSessionRequest::SetContentAsStreamedFile failed to get file size"));
		Request.HTTPBodyStream = nil;
		ContentBytesLength = 0;
		bIsPayloadFile = false;
	}

	return bIsPayloadFile;
}

bool FAppleHttpNSUrlSessionRequest::SetContentFromStream(TSharedRef<FArchive, ESPMode::ThreadSafe> Stream)
{
	UE_LOG(LogHttp, Warning, TEXT("FAppleHttpNSUrlSessionRequest::SetContentFromStream is not implemented"));
	return false;
}

bool FAppleHttpNSUrlSessionRequest::SetResponseBodyReceiveStream(TSharedRef<FArchive> Stream)
{
	ResponseBodyReceiveStream = Stream;
	return true;
}

FString FAppleHttpNSUrlSessionRequest::GetVerb() const
{
	FString ConvertedVerb(Request.HTTPMethod);
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionRequest::GetVerb() - %s"), *ConvertedVerb);
	return ConvertedVerb;
}

void FAppleHttpNSUrlSessionRequest::SetVerb(const FString& Verb)
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionRequest::SetVerb() - %s"), *Verb);
	Request.HTTPMethod = Verb.GetNSString();
}

void FAppleHttpNSUrlSessionRequest::SetTimeout(float InTimeoutSecs)
{
	Request.timeoutInterval = InTimeoutSecs;
}

void FAppleHttpNSUrlSessionRequest::ClearTimeout()
{
	Request.timeoutInterval = FHttpModule::Get().GetHttpTimeout();
}

TOptional<float> FAppleHttpNSUrlSessionRequest::GetTimeout() const
{
	return TOptional<float>(Request.timeoutInterval);
}

bool FAppleHttpNSUrlSessionRequest::ProcessRequest()
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionRequest::ProcessRequest()"));

	if (!PreCheck() || !StartRequest())
	{
		FinishRequestNotInHttpManager();
		return false;
	}

	return true;
}

bool FAppleHttpNSUrlSessionRequest::StartRequest()
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionRequest::StartRequest()"));
	bool bStarted = false;

	// set the content-length and user-agent (it is possible that the OS ignores this value)
	if(GetContentLength() > 0)
	{
		[Request setValue:[NSString stringWithFormat:@"%llu", GetContentLength()] forHTTPHeaderField:@"Content-Length"];
	}

	const FString UserAgent = GetHeader("User-Agent");
	if(UserAgent.IsEmpty())
	{
		NSString* Tag = FPlatformHttp::GetDefaultUserAgent().GetNSString();
		[Request setValue:Tag forHTTPHeaderField:@"User-Agent"];
	}

	CleanupRequest();

	LastReportedBytesWritten = 0;
	LastReportedBytesRead = 0;
	ElapsedTime = 0.0f;
	Response = nullptr;

	Task = [Session dataTaskWithRequest: Request];
	
	if (Task != nil)
	{
		bStarted = true;

		CompletionStatus = EHttpRequestStatus::Processing;

		Response = MakeShared<FAppleHttpNSUrlSessionResponse>(*this);

		// Both Task and Response keep a strong reference to the delegate
		Task.delegate = Response->ResponseDelegate;

		//Setup delegates before starting the request
		FHttpModule::Get().GetHttpManager().AddThreadedRequest(SharedThis(this));

		[[Task retain] resume];
		UE_LOG(LogHttp, Verbose, TEXT("[NSURLSessionTask resume]"));
	}
	else
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. Could not initialize Internet connection."));
		CompletionStatus = EHttpRequestStatus::Failed_ConnectionError;
	}

	return bStarted;
}

void FAppleHttpNSUrlSessionRequest::FinishRequest()
{
	check(IsInGameThread() || DelegateThreadPolicy == EHttpRequestDelegateThreadPolicy::CompleteOnHttpThread);

	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionRequest::FinishRequest()"));

	// Clean up session/request handles that may have been created
	CleanupRequest();

	bool bSuccess = false;
	if (Response.IsValid() && Response->IsReady() && !Response->HadError())
	{
		bSuccess = true;
		UE_LOG(LogHttp, Verbose, TEXT("Request succeeded"));
		CompletionStatus = EHttpRequestStatus::Succeeded;

		// TODO: Try to broadcast OnHeaderReceived when we receive headers instead of here at the end
		BroadcastResponseHeadersReceived();
	}
	else
	{
		UE_LOG(LogHttp, Verbose, TEXT("Request failed"));
		FString URL([[Request URL] absoluteString]);
		CompletionStatus = EHttpRequestStatus::Failed;
		if (Response.IsValid() && Response->HadConnectionError())
		{
			CompletionStatus = EHttpRequestStatus::Failed_ConnectionError;
			Response = nullptr;
		}
	}

	OnProcessRequestComplete().ExecuteIfBound(SharedThis(this), Response, bSuccess);
}

void FAppleHttpNSUrlSessionRequest::CleanupRequest()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionRequest::CleanupRequest()"));
	
	if (Response != nullptr)
	{
		Response->CleanSharedObjects();
	}

	if(Task != nil)
	{
		if (CompletionStatus == EHttpRequestStatus::Processing)
		{
			[Task cancel];
		}
		[Task release];
		Task = nil;
	}
}

void FAppleHttpNSUrlSessionRequest::CancelRequest()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionRequest::CancelRequest()"));

	if (Task != nil)
	{
		[Task cancel];
	}
}

const FHttpResponsePtr FAppleHttpNSUrlSessionRequest::GetResponse() const
{
	return Response;
}

void FAppleHttpNSUrlSessionRequest::Tick(float DeltaSeconds)
{
	if (Response.IsValid() && (CompletionStatus == EHttpRequestStatus::Processing || Response->HadError()))
	{
		if (OnRequestProgress().IsBound())
		{
			const int32 BytesWritten = Response->GetNumBytesWritten();
			const int32 BytesRead = Response->GetNumBytesReceived();
			if (BytesWritten != LastReportedBytesWritten || BytesRead != LastReportedBytesRead)
			{
				OnRequestProgress().ExecuteIfBound(SharedThis(this), BytesWritten, BytesRead);
				LastReportedBytesWritten = BytesWritten;
				LastReportedBytesRead = BytesRead;
			}
		}
	}
}

float FAppleHttpNSUrlSessionRequest::GetElapsedTime() const
{
	return ElapsedTime;
}

bool FAppleHttpNSUrlSessionRequest::StartThreadedRequest()
{
	return true;
}

bool FAppleHttpNSUrlSessionRequest::IsThreadedRequestComplete()
{
	TSharedPtr<FAppleHttpNSUrlSessionResponse> AliveResponse = Response;
	return (AliveResponse.IsValid() && AliveResponse->IsReady());
}

void FAppleHttpNSUrlSessionRequest::TickThreadedRequest(float DeltaSeconds)
{
	ElapsedTime += DeltaSeconds;
}

/****************************************************************************
 * FAppleHttpNSUrlSessionResponse implementation
 **************************************************************************/

FAppleHttpNSUrlSessionResponse::FAppleHttpNSUrlSessionResponse(const FAppleHttpNSUrlSessionRequest& InRequest)
	: Request( InRequest )
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionResponse::FAppleHttpNSUrlSessionResponse()"));
	ResponseDelegate = [[FAppleHttpNSUrlSessionResponseDelegate alloc] initWithResponseStream: InRequest.ResponseBodyReceiveStream];
}

FAppleHttpNSUrlSessionResponse::~FAppleHttpNSUrlSessionResponse()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionResponse::~FAppleHttpNSUrlSessionResponse()"));
	
	[ResponseDelegate release];
	ResponseDelegate = nil;
}

void FAppleHttpNSUrlSessionResponse::SetInternalTaskCompleteDelegate(FTaskCompleteDelegate&& Delegate)
{	
	ResponseDelegate->TaskCompleteDelegate = MoveTemp(Delegate);
}

void FAppleHttpNSUrlSessionResponse::CleanSharedObjects()
{
	[ResponseDelegate ClearResponseStream];
}

FString FAppleHttpNSUrlSessionResponse::GetURL() const
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionResponse::GetURL()"));
	return FString(Request.Request.URL.query);
}

FString FAppleHttpNSUrlSessionResponse::GetURLParameter(const FString& ParameterName) const
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionResponse::GetURLParameter()"));

	NSString* ParameterNameStr = ParameterName.GetNSString();
	NSArray* Parameters = [[[Request.Request URL] query] componentsSeparatedByString:@"&"];
	for (NSString* Parameter in Parameters)
	{
		NSArray* KeyValue = [Parameter componentsSeparatedByString:@"="];
		NSString* Key = [KeyValue objectAtIndex:0];
		if ([Key compare:ParameterNameStr] == NSOrderedSame)
		{
			return FString([[KeyValue objectAtIndex:1] stringByRemovingPercentEncoding]);
		}
	}
	return FString();
}

FString FAppleHttpNSUrlSessionResponse::GetHeader(const FString& HeaderName) const
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionResponse::GetHeader()"));
	NSString* ConvertedHeaderName = HeaderName.GetNSString();
	return FString([ResponseDelegate.Response.allHeaderFields objectForKey:ConvertedHeaderName]);
}

TArray<FString> FAppleHttpNSUrlSessionResponse::GetAllHeaders() const
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionResponse::GetAllHeaders()"));

	NSDictionary* Headers = ResponseDelegate.Response.allHeaderFields;
	TArray<FString> Result;
	Result.Reserve([Headers count]);
	for (NSString* Key in [Headers allKeys])
	{
		FString ConvertedValue([Headers objectForKey:Key]);
		FString ConvertedKey(Key);
		Result.Add( FString::Printf( TEXT("%s: %s"), *ConvertedKey, *ConvertedValue ) );
	}
	return Result;
}

FString FAppleHttpNSUrlSessionResponse::GetContentType() const
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionResponse::GetContentType()"));

	return GetHeader( TEXT( "Content-Type" ) );
}

uint64 FAppleHttpNSUrlSessionResponse::GetContentLength() const
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionResponse::GetContentLength()"));
	
	return ResponseDelegate.Response.expectedContentLength;
}

const TArray<uint8>& FAppleHttpNSUrlSessionResponse::GetContent() const
{
	if( !IsReady() )
	{
		UE_LOG(LogHttp, Warning, TEXT("Payload is incomplete. Response still processing. %p"), &Request);
	}
	else
	{
		UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionResponse::GetContent() - Num: %i"), ResponseDelegate->Payload.Num());
	}
	return ResponseDelegate->Payload;
}

FString FAppleHttpNSUrlSessionResponse::GetContentAsString() const
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionResponse::GetContentAsString()"));

	// Fill in our data.
	const TArray<uint8>& Payload = GetContent();

	TArray<uint8> ZeroTerminatedPayload;
	ZeroTerminatedPayload.AddZeroed( Payload.Num() + 1 );
	FMemory::Memcpy( ZeroTerminatedPayload.GetData(), Payload.GetData(), Payload.Num() );

	return UTF8_TO_TCHAR( ZeroTerminatedPayload.GetData() );
}

int32 FAppleHttpNSUrlSessionResponse::GetResponseCode() const
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlSessionResponse::GetResponseCode()"));

	return ResponseDelegate.Response.statusCode;
}

bool FAppleHttpNSUrlSessionResponse::IsReady() const
{
	return (ResponseDelegate.ResponseState != EAppleHttpRequestResponseState::NotReady);
}

bool FAppleHttpNSUrlSessionResponse::HadError() const
{
	switch(ResponseDelegate.ResponseState)
	{
		case EAppleHttpRequestResponseState::Error:
		case EAppleHttpRequestResponseState::ConnectionError:
			return true;
		default:
			return false;
	}
}

bool FAppleHttpNSUrlSessionResponse::HadConnectionError() const
{
	return (ResponseDelegate.ResponseState == EAppleHttpRequestResponseState::ConnectionError);
}

const uint64 FAppleHttpNSUrlSessionResponse::GetNumBytesReceived() const
{
	return ResponseDelegate.BytesReceived;
}

const uint64 FAppleHttpNSUrlSessionResponse::GetNumBytesWritten() const
{
	return ResponseDelegate.BytesWritten;
}
