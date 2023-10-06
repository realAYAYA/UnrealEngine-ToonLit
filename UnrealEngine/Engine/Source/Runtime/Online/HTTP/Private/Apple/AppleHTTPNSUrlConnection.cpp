// Copyright Epic Games, Inc. All Rights Reserved.


#include "AppleHTTPNSUrlConnection.h"
#include "Misc/EngineVersion.h"
#include "Security/Security.h"
#include "CommonCrypto/CommonDigest.h"
#include "Foundation/Foundation.h"
#include "Misc/App.h"
#include "Misc/Base64.h"
#include "HAL/PlatformTime.h"
#include "Http.h"
#include "HttpModule.h"
#include "Apple/CFRef.h"

#if WITH_SSL
#include "Ssl.h"
#endif

/****************************************************************************
 * FAppleHttpNSUrlConnectionRequest implementation
 ***************************************************************************/


FAppleHttpNSUrlConnectionRequest::FAppleHttpNSUrlConnectionRequest()
:	Connection(nullptr)
,	bIsPayloadFile(false)
,	RequestPayloadByteLength(0)
,	ProgressBytesSent(0)
,	StartRequestTime(0.0)
,	ElapsedTime(0.0f)
,   LastReportedBytesWritten(0)
,   LastReportedBytesRead(0)
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionRequest::FAppleHttpNSUrlConnectionRequest()"));
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

#if WITH_SSL
	// Make sure the module is loaded on the game thread before being used by FHttpResponseAppleNSUrlConnectionWrapper, which will be called on the main thread
	FSslModule::Get();
#endif
}


FAppleHttpNSUrlConnectionRequest::~FAppleHttpNSUrlConnectionRequest()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionRequest::~FAppleHttpNSUrlConnectionRequest()"));
	DiscardExistingRequest();
	[Request release];
}


FString FAppleHttpNSUrlConnectionRequest::GetURL() const
{
	SCOPED_AUTORELEASE_POOL;
	NSURL* URL = Request.URL;
	if (URL != nullptr)
	{
		FString ConvertedURL(URL.absoluteString);
		UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionRequest::GetURL() - %s"), *ConvertedURL);
		return ConvertedURL;
	}
	else
	{
		UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionRequest::GetURL() - NULL"));
		return FString();
	}
}


void FAppleHttpNSUrlConnectionRequest::SetURL(const FString& URL)
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionRequest::SetURL() - %s"), *URL);
	Request.URL = [NSURL URLWithString: URL.GetNSString()];
}


FString FAppleHttpNSUrlConnectionRequest::GetURLParameter(const FString& ParameterName) const
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionRequest::GetURLParameter() - %s"), *ParameterName);

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


FString FAppleHttpNSUrlConnectionRequest::GetHeader(const FString& HeaderName) const
{
	SCOPED_AUTORELEASE_POOL;
	FString Header([Request valueForHTTPHeaderField:HeaderName.GetNSString()]);
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionRequest::GetHeader() - %s"), *Header);
	return Header;
}


void FAppleHttpNSUrlConnectionRequest::SetHeader(const FString& HeaderName, const FString& HeaderValue)
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionRequest::SetHeader() - %s / %s"), *HeaderName, *HeaderValue );
	[Request setValue: HeaderValue.GetNSString() forHTTPHeaderField: HeaderName.GetNSString()];
}

void FAppleHttpNSUrlConnectionRequest::AppendToHeader(const FString& HeaderName, const FString& AdditionalHeaderValue)
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

TArray<FString> FAppleHttpNSUrlConnectionRequest::GetAllHeaders() const
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionRequest::GetAllHeaders()"));
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


const TArray<uint8>& FAppleHttpNSUrlConnectionRequest::GetContent() const
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionRequest::GetContent()"));
	if (bIsPayloadFile)
	{
		UE_LOG(LogHttp, Warning, TEXT("FAppleHttpNSUrlConnectionRequest::GetContent() called on a request that is set up for streaming a file. Return value is an empty buffer"));
		RequestPayload.Empty();
		return RequestPayload;
	}
	else
	{
		SCOPED_AUTORELEASE_POOL;
		NSData* Body = Request.HTTPBody; // accessing HTTPBody will call copy on the value, increasing its retain count
		RequestPayload.Empty();
		RequestPayload.Append((const uint8*)Body.bytes, Body.length);
		return RequestPayload;
	}
}


void FAppleHttpNSUrlConnectionRequest::SetContent(const TArray<uint8>& ContentPayload)
{
	if (CompletionStatus == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("FAppleHttpNSUrlConnectionRequest::SetContent() - attempted to set content on a request that is inflight"));
		return;
	}

	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionRequest::SetContent()"));
	Request.HTTPBody = [NSData dataWithBytes:ContentPayload.GetData() length:ContentPayload.Num()];
	RequestPayloadByteLength = ContentPayload.Num();
	bIsPayloadFile = false;

	ContentData.Empty();
}


void FAppleHttpNSUrlConnectionRequest::SetContent(TArray<uint8>&& ContentPayload)
{
	if (CompletionStatus == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("FAppleHttpNSUrlConnectionRequest::SetContent() - attempted to set content on a request that is inflight"));
		return;
	}

	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionRequest::SetContent()"));
	ContentData = MoveTemp(ContentPayload);

	Request.HTTPBody = [NSData dataWithBytesNoCopy:ContentData.GetData() length:ContentData.Num() freeWhenDone:false];
	RequestPayloadByteLength = ContentData.Num();
	bIsPayloadFile = false;
}


FString FAppleHttpNSUrlConnectionRequest::GetContentType() const
{
	FString ContentType = GetHeader(TEXT("Content-Type"));
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionRequest::GetContentType() - %s"), *ContentType);
	return ContentType;
}


uint64 FAppleHttpNSUrlConnectionRequest::GetContentLength() const
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionRequest::GetContentLength() - %i"), RequestPayloadByteLength);
	return RequestPayloadByteLength;
}


void FAppleHttpNSUrlConnectionRequest::SetContentAsString(const FString& ContentString)
{
	if (CompletionStatus == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("FAppleHttpNSUrlConnectionRequest::SetContentAsString() - attempted to set content on a request that is inflight"));
		return;
	}

	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionRequest::SetContentAsString() - %s"), *ContentString);
	FTCHARToUTF8 Converter(*ContentString);

	// The extra length computation here is unfortunate, but it's technically not safe to assume the length is the same.
	Request.HTTPBody = [NSData dataWithBytes:(ANSICHAR*)Converter.Get() length:Converter.Length()];
	RequestPayloadByteLength = Converter.Length();
	bIsPayloadFile = false;

	ContentData.Empty();
}

bool FAppleHttpNSUrlConnectionRequest::SetContentAsStreamedFile(const FString& Filename)
{
	SCOPED_AUTORELEASE_POOL;
    UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionRequest::SetContentAsStreamedFile() - %s"), *Filename);

	if (CompletionStatus == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("FAppleHttpNSUrlConnectionRequest::SetContentAsStreamedFile() - attempted to set content on a request that is inflight"));
		return false;
	}

	NSString* PlatformFilename = Filename.GetNSString();

	Request.HTTPBody = nil;
	ContentData.Empty();

	struct stat FileAttrs = { 0 };
	if (stat(PlatformFilename.fileSystemRepresentation, &FileAttrs) == 0)
	{
		UE_LOG(LogHttp, VeryVerbose, TEXT("FAppleHttpNSUrlConnectionRequest::SetContentAsStreamedFile succeeded in getting the file size - %llu"), FileAttrs.st_size);
		// Under the hood, the Foundation framework unsets HTTPBody, and takes over as the stream delegate.
		// The stream itself should be unopened when passed to setHTTPBodyStream.
		Request.HTTPBodyStream = [NSInputStream inputStreamWithFileAtPath: PlatformFilename];
		RequestPayloadByteLength = FileAttrs.st_size;
		bIsPayloadFile = true;
	}
	else
	{
		UE_LOG(LogHttp, VeryVerbose, TEXT("FAppleHttpNSUrlConnectionRequest::SetContentAsStreamedFile failed to get file size"));
		Request.HTTPBodyStream = nil;
		RequestPayloadByteLength = 0;
		bIsPayloadFile = false;
	}

	return bIsPayloadFile;
}


bool FAppleHttpNSUrlConnectionRequest::SetContentFromStream(TSharedRef<FArchive, ESPMode::ThreadSafe> Stream)
{
	UE_LOG(LogHttp, Warning, TEXT("FAppleHttpNSUrlConnectionRequest::SetContentFromStream is not implemented"));
	return false;
}

bool FAppleHttpNSUrlConnectionRequest::SetResponseBodyReceiveStream(TSharedRef<FArchive> Stream)
{
	UE_LOG(LogHttp, Warning, TEXT("FAppleHttpNSUrlConnectionRequest::SetResponseBodyReceiveStream is not implemented"));
	return false;
}

FString FAppleHttpNSUrlConnectionRequest::GetVerb() const
{
	FString ConvertedVerb(Request.HTTPMethod);
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionRequest::GetVerb() - %s"), *ConvertedVerb);
	return ConvertedVerb;
}


void FAppleHttpNSUrlConnectionRequest::SetVerb(const FString& Verb)
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionRequest::SetVerb() - %s"), *Verb);
	Request.HTTPMethod = Verb.GetNSString();
}

void FAppleHttpNSUrlConnectionRequest::SetTimeout(float InTimeoutSecs)
{
	Request.timeoutInterval = InTimeoutSecs;
}

void FAppleHttpNSUrlConnectionRequest::ClearTimeout()
{
	Request.timeoutInterval = FHttpModule::Get().GetHttpTimeout();
}

TOptional<float> FAppleHttpNSUrlConnectionRequest::GetTimeout() const
{
	return TOptional<float>(Request.timeoutInterval);
}

bool FAppleHttpNSUrlConnectionRequest::ProcessRequest()
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionRequest::ProcessRequest()"));

	if (!PreCheck() || !StartRequest())
	{
		// Ensure we run on game thread
		if (!IsInGameThread())
		{
			FHttpModule::Get().GetHttpManager().AddGameThreadTask([StrongThis = StaticCastSharedRef<FAppleHttpNSUrlConnectionRequest>(AsShared())]()
			{
				StrongThis->FinishedRequest();
			});
		}
		else
		{
			FinishedRequest();
		}

		return false;
	}

	return true;
}

bool FAppleHttpNSUrlConnectionRequest::StartRequest()
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionRequest::StartRequest()"));
	bool bStarted = false;

	// set the content-length and user-agent
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

	LastReportedBytesWritten = 0;
	LastReportedBytesRead = 0;

	DiscardExistingRequest();

	Response = MakeShareable( new FAppleHttpNSUrlConnectionResponse( *this ) );

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
	// Create the connection, tell it to run in the main run loop, and kick it off.
	Connection = [[NSURLConnection alloc] initWithRequest:Request delegate:Response->ResponseWrapper startImmediately:NO];
#pragma clang diagnostic pop
	if (Connection != nullptr && Response->ResponseWrapper != nullptr)
	{
		CompletionStatus = EHttpRequestStatus::Processing;
		[Connection scheduleInRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];
		[Connection start];
		UE_LOG(LogHttp, Verbose, TEXT("[Connection start]"));

		bStarted = true;
		// Add to global list while being processed so that the ref counted request does not get deleted
		FHttpModule::Get().GetHttpManager().AddRequest(SharedThis(this));
	}
	else
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. Could not initialize Internet connection."));
		CompletionStatus = EHttpRequestStatus::Failed_ConnectionError;
	}
	StartRequestTime = FPlatformTime::Seconds();
	// reset the elapsed time.
	ElapsedTime = 0.0f;

	return bStarted;
}

void FAppleHttpNSUrlConnectionRequest::FinishedRequest()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionRequest::FinishedRequest()"));
	ElapsedTime = (float)(FPlatformTime::Seconds() - StartRequestTime);
	if( Response.IsValid() && Response->IsReady() && !Response->HadError())
	{
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
		if (Response.IsValid() && [Response->ResponseWrapper bIsHostConnectionFailure])
		{
			CompletionStatus = EHttpRequestStatus::Failed_ConnectionError;
		}

		Response = nullptr;
	}

	// Clean up session/request handles that may have been created
	CleanupRequest();

	OnProcessRequestComplete().ExecuteIfBound(SharedThis(this), Response, Response != nullptr);
}

void FAppleHttpNSUrlConnectionRequest::DiscardExistingRequest()
{
	if (Connection != nil)
	{
		if (CompletionStatus == EHttpRequestStatus::Processing)
		{
			[Connection cancel];
		}
		[Connection release];
		Connection = nil;
	}
	Response = nullptr;
}

void FAppleHttpNSUrlConnectionRequest::CleanupRequest()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionRequest::CleanupRequest()"));

	if(CompletionStatus == EHttpRequestStatus::Processing)
	{
		CancelRequest();
	}

	if(Connection != nullptr)
	{
		[Connection release];
		Connection = nullptr;
	}
}


void FAppleHttpNSUrlConnectionRequest::CancelRequest()
{
	
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionRequest::CancelRequest()"));
	if(Connection != nullptr)
	{
		[Connection cancel];
	}

	// Ensure we run on game thread
	if (!IsInGameThread())
	{
		FHttpModule::Get().GetHttpManager().AddGameThreadTask([StrongThis = StaticCastSharedRef<FAppleHttpNSUrlConnectionRequest>(AsShared())]()
		{
			StrongThis->FinishedRequest();
		});
	}
	else
	{
		FinishedRequest();
	}
}

const FHttpResponsePtr FAppleHttpNSUrlConnectionRequest::GetResponse() const
{
	return Response;
}

void FAppleHttpNSUrlConnectionRequest::Tick(float DeltaSeconds)
{
	if (Response.IsValid() && (CompletionStatus == EHttpRequestStatus::Processing || Response->HadError()))
	{
		if (OnRequestProgress().IsBound())
		{
			const uint64 BytesWritten = Response->GetNumBytesWritten();
			const uint64 BytesRead = Response->GetNumBytesReceived();
			if (BytesWritten != LastReportedBytesWritten || BytesRead != LastReportedBytesRead)
			{
				OnRequestProgress().ExecuteIfBound(SharedThis(this), BytesWritten, BytesRead);
				LastReportedBytesWritten = BytesWritten;
				LastReportedBytesRead = BytesRead;
			}
		}
		if (Response->IsReady())
		{
			FinishedRequest();
		}
	}
}

float FAppleHttpNSUrlConnectionRequest::GetElapsedTime() const
{
	return ElapsedTime;
}


/****************************************************************************
 * FHttpResponseAppleNSUrlConnectionWrapper implementation
 ***************************************************************************/

@implementation FHttpResponseAppleNSUrlConnectionWrapper
@synthesize Response;
@synthesize bIsReady;
@synthesize bHadError;
@synthesize bIsHostConnectionFailure;
@synthesize BytesWritten;


-(FHttpResponseAppleNSUrlConnectionWrapper*) init
{
	UE_LOG(LogHttp, Verbose, TEXT("-(FHttpResponseAppleNSUrlConnectionWrapper*) init"));
	self = [super init];
	bIsReady = false;
	bHadError = false;
	bIsHostConnectionFailure = false;
	
	return self;
}

- (void)dealloc
{
	[Response release];
	[super dealloc];
}

-(void) connection:(NSURLConnection *)connection didSendBodyData:(NSInteger)bytesWritten totalBytesWritten:(NSInteger)totalBytesWritten totalBytesExpectedToWrite:(NSInteger)totalBytesExpectedToWrite
{
	UE_LOG(LogHttp, Verbose, TEXT("didSendBodyData:(NSInteger)bytesWritten totalBytes:Written:(NSInteger)totalBytesWritten totalBytesExpectedToWrite:(NSInteger)totalBytesExpectedToWrite"));
	self.BytesWritten = totalBytesWritten;
	UE_LOG(LogHttp, Verbose, TEXT("didSendBodyData: totalBytesWritten = %llu, totalBytesExpectedToWrite = %llu: %p"), totalBytesWritten, totalBytesExpectedToWrite, self);
}

-(void) connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)response
{
	UE_LOG(LogHttp, Verbose, TEXT("didReceiveResponse:(NSURLResponse *)response"));
	self.Response = (NSHTTPURLResponse*)response;
	
	// presize the payload container if possible
	Payload.Empty([response expectedContentLength] != NSURLResponseUnknownLength ? [response expectedContentLength] : 0);
	UE_LOG(LogHttp, Verbose, TEXT("didReceiveResponse: expectedContentLength = %llu. Length = %llu: %p"), [response expectedContentLength], Payload.Max(), self);
}


-(void) connection:(NSURLConnection *)connection didReceiveData:(NSData *)data
{
	Payload.Append((const uint8*)[data bytes], [data length]);
	UE_LOG(LogHttp, Verbose, TEXT("didReceiveData with %llu bytes. After Append, Payload Length = %llu: %p"), [data length], Payload.Num(), self);
}


-(void) connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
	self.bIsReady = YES;
	self.bHadError = YES;
	UE_LOG(LogHttp, Warning, TEXT("didFailWithError. Http request failed - %s %s: %p"), 
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
			self.bIsHostConnectionFailure = YES;
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
			UE_LOG(LogHttp, Verbose, TEXT("didFailWithError. SSL trust result: %s (%d)"), *TrustResultString, TrustResult);
		}
	}
}

#if WITH_SSL
-(void) connection:(NSURLConnection *)connection willSendRequestForAuthenticationChallenge: (NSURLAuthenticationChallenge *)challenge
{
	// CC gives the actual key, but strips the ASN.1 header... which means
	// we can't calulate a proper SPKI hash without reconstructing it. sigh.
	static const unsigned char rsa2048Asn1Header[] =
	{
		0x30, 0x82, 0x01, 0x22, 0x30, 0x0d, 0x06, 0x09,
		0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01,
		0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0f, 0x00
	};
	static const unsigned char rsa4096Asn1Header[] =
	{
		0x30, 0x82, 0x02, 0x22, 0x30, 0x0d, 0x06, 0x09,
		0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01,
		0x01, 0x05, 0x00, 0x03, 0x82, 0x02, 0x0f, 0x00
	};
	static const unsigned char ecdsaSecp256r1Asn1Header[] =
	{
		0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86,
		0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x08, 0x2a,
		0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03,
		0x42, 0x00
	};
	static const unsigned char ecdsaSecp384r1Asn1Header[] =
	{
		0x30, 0x76, 0x30, 0x10, 0x06, 0x07, 0x2a, 0x86,
		0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x05, 0x2b,
		0x81, 0x04, 0x00, 0x22, 0x03, 0x62, 0x00
	};

    if (ensure(ISslCertificateManager::PUBLIC_KEY_DIGEST_SIZE == CC_SHA256_DIGEST_LENGTH))
    {
        // we only care about challenges to the received certificate chain
        if ([challenge.protectionSpace.authenticationMethod isEqualToString:NSURLAuthenticationMethodServerTrust])
        {
            SecTrustRef RemoteTrust = challenge.protectionSpace.serverTrust;
            FString RemoteHost = FString(UTF8_TO_TCHAR([challenge.protectionSpace.host UTF8String]));
            if ((RemoteTrust == NULL) || (RemoteHost.IsEmpty()))
            {
                UE_LOG(LogHttp, Error, TEXT("failed certificate pinning validation: could not parse parameters during certificate pinning evaluation"));
                [challenge.sender cancelAuthenticationChallenge: challenge];
                return;
            }

			if (!SecTrustEvaluateWithError(RemoteTrust, nil))
			{
				UE_LOG(LogHttp, Error, TEXT("failed certificate pinning validation: default certificate trust evaluation failed for domain '%s'"), *RemoteHost);
				[challenge.sender cancelAuthenticationChallenge: challenge];
				return;
			}
            // look at all certs in the remote chain and calculate the SHA256 hash of their DER-encoded SPKI
            // the chain starts with the server's cert itself, so walk backwards to optimize for roots first
            TArray<TArray<uint8, TFixedAllocator<ISslCertificateManager::PUBLIC_KEY_DIGEST_SIZE>>> CertDigests;
            
            CFArrayRef Certificates = SecTrustCopyCertificateChain(RemoteTrust);
            if (Certificates == nil)
            {
                UE_LOG(LogHttp, Error, TEXT("No certificate could be copied in the certificate chain used to evaluate trust."));
                return;
            }
            
            CFIndex CertificateCount = CFArrayGetCount(Certificates);
            for (int i = 0; i < CertificateCount; ++i)
            {
                SecCertificateRef Cert = (SecCertificateRef)CFArrayGetValueAtIndex(Certificates, i);
                
                // this is not great, but the only way to extract a public key from a SecCertificateRef
                // is to create an individual SecTrustRef for each cert that only contains itself and then
                // evaluate that against an empty X509 policy.
                TCFRef<SecTrustRef> CertTrust;
                TCFRef<SecPolicyRef> TrustPolicy = SecPolicyCreateBasicX509();
                SecTrustCreateWithCertificates(Cert, TrustPolicy, CertTrust.GetForAssignment());
                SecTrustEvaluateWithError(CertTrust, nil);
                TCFRef<SecKeyRef> CertPubKey;

				CertPubKey = SecTrustCopyKey(CertTrust);

				TCFRef<CFDataRef> CertPubKeyData = SecKeyCopyExternalRepresentation(CertPubKey, NULL);
                if (!CertPubKeyData)
                {
                    UE_LOG(LogHttp, Warning, TEXT("could not extract public key from certificate %i for domain '%s'; skipping!"), i, *RemoteHost);
                    continue;
                }
                
				// we got the key. now we have to figure out what type of key it is; thanks, CommonCrypto.
                TCFRef<CFDictionaryRef> CertPubKeyAttr = SecKeyCopyAttributes(CertPubKey);
                NSString *CertPubKeyType = static_cast<NSString *>(CFDictionaryGetValue(CertPubKeyAttr, kSecAttrKeyType));
                NSNumber *CertPubKeySize = static_cast<NSNumber *>(CFDictionaryGetValue(CertPubKeyAttr, kSecAttrKeySizeInBits));
                char *CertPubKeyASN1Header;
                uint8_t CertPubKeyASN1HeaderSize = 0;
                if ([CertPubKeyType isEqualToString: (NSString *)kSecAttrKeyTypeRSA])
                {
                    switch ([CertPubKeySize integerValue])
                    {
                        case 2048:
                            UE_LOG(LogHttp, VeryVerbose, TEXT("found 2048 bit RSA pubkey"));
                            CertPubKeyASN1Header = (char *)rsa2048Asn1Header;
                            CertPubKeyASN1HeaderSize = sizeof(rsa2048Asn1Header);
                            break;
                        case 4096:
                            UE_LOG(LogHttp, VeryVerbose, TEXT("found 4096 bit RSA pubkey"));
                            CertPubKeyASN1Header = (char *)rsa4096Asn1Header;
                            CertPubKeyASN1HeaderSize = sizeof(rsa4096Asn1Header);
                            break;
                        default:
                            UE_LOG(LogHttp, Log, TEXT("unsupported RSA key length %i for certificate %i for domain '%s'; skipping!"), [CertPubKeySize integerValue], i, *RemoteHost);
                            continue;
                    }
                }
                else if ([CertPubKeyType isEqualToString: (NSString *)kSecAttrKeyTypeECSECPrimeRandom])
                {
                    switch ([CertPubKeySize integerValue])
                    {
                        case 256:
                            UE_LOG(LogHttp, VeryVerbose, TEXT("found 256 bit ECDSA pubkey"));
                            CertPubKeyASN1Header = (char *)ecdsaSecp256r1Asn1Header;
                            CertPubKeyASN1HeaderSize = sizeof(ecdsaSecp256r1Asn1Header);
                            break;
                        case 384:
                            UE_LOG(LogHttp, VeryVerbose, TEXT("found 384 bit ECDSA pubkey"));
                            CertPubKeyASN1Header = (char *)ecdsaSecp384r1Asn1Header;
                            CertPubKeyASN1HeaderSize = sizeof(ecdsaSecp384r1Asn1Header);
                            break;
                        default:
                            UE_LOG(LogHttp, Log, TEXT("unsupported ECDSA key length %i for certificate %i for domain '%s'; skipping!"), [CertPubKeySize integerValue], i, *RemoteHost);
                            continue;
                    }
                }
                else {
                    UE_LOG(LogHttp, Log, TEXT("unsupported key type (not RSA or ECDSA) for certificate %i for domain '%s'; skipping!"), i, *RemoteHost);
                    continue;
                }
				
                UE_LOG(LogHttp, VeryVerbose, TEXT("constructed key header: [%d] %s"), CertPubKeyASN1HeaderSize, UTF8_TO_TCHAR([[[NSData dataWithBytes:CertPubKeyASN1Header length:CertPubKeyASN1HeaderSize] description] UTF8String]));
                UE_LOG(LogHttp, VeryVerbose, TEXT("current pubkey: [%d] %s"), [(NSData*)CertPubKeyData length], UTF8_TO_TCHAR([[[NSData dataWithBytes:[(NSData*)CertPubKeyData bytes] length:[(NSData*)CertPubKeyData length]] description] UTF8String]));
                
                // smash 'em together to get a proper key with an ASN.1 header
                NSMutableData *ReconstructedPubKey = [NSMutableData data];
                [ReconstructedPubKey appendBytes:CertPubKeyASN1Header length:CertPubKeyASN1HeaderSize];
                [ReconstructedPubKey appendData:CertPubKeyData];
                UE_LOG(LogHttp, VeryVerbose, TEXT("reconstructed key: [%d] %s"), [ReconstructedPubKey length], UTF8_TO_TCHAR([[ReconstructedPubKey description] UTF8String]));
                
                TArray<uint8, TFixedAllocator<ISslCertificateManager::PUBLIC_KEY_DIGEST_SIZE>> CertCalcDigest;
                CertCalcDigest.AddUninitialized(CC_SHA256_DIGEST_LENGTH);
                if (!CC_SHA256([ReconstructedPubKey bytes], (CC_LONG)[ReconstructedPubKey length], CertCalcDigest.GetData()))
                {
                    UE_LOG(LogHttp, Warning, TEXT("could not calculate SHA256 digest of public key %d for domain '%s'; skipping!"), i, *RemoteHost);
                }
                else
                {
                    CertDigests.Add(CertCalcDigest);
                    UE_LOG(LogHttp, Verbose, TEXT("added SHA256 digest to list for evaluation: domain: '%s' digest: [%d] %s"), *RemoteHost, CertCalcDigest.Num(), UTF8_TO_TCHAR([[[NSData dataWithBytes:CertCalcDigest.GetData() length:CertCalcDigest.Num()] description] UTF8String]));
                }
            }
            
            //finally, see if any of the pubkeys in the chain match any of our pinned pubkey hashes
            if (CertDigests.Num() <= 0 || !FSslModule::Get().GetCertificateManager().VerifySslCertificates(CertDigests, RemoteHost))
            {
                // we could not validate any of the provided certs in chain with the pinned hashes for this host
                // so we tell the sender to cancel (which cancels the pending connection)
                UE_LOG(LogHttp, Error, TEXT("failed certificate pinning validation: no SPKI hashes in request matched pinned hashes for domain '%s' (was provided %d certificates in request)"), *RemoteHost, CertDigests.Num());
                [challenge.sender cancelAuthenticationChallenge:challenge];
                return;
            }
        }
    }
    else
    {
        UE_LOG(LogHttp, Error, TEXT("failed certificate pinning validation: SslCertificateManager is using non-SHA256 SPKI hashes [expected %d bytes, got %d bytes]"), CC_SHA256_DIGEST_LENGTH, ISslCertificateManager::PUBLIC_KEY_DIGEST_SIZE);
        [challenge.sender cancelAuthenticationChallenge:challenge];
        return;
    }
    
    // if we got this far, pinning validation either succeeded or was disabled (or this was checking for client auth, etc.)
    // so tell the connection to keep going with whatever else it was trying to validate
    UE_LOG(LogHttp, Verbose, TEXT("certificate public key pinning either succeeded, is disabled, or challenge was not a server trust; continuing with auth"));
    [challenge.sender performDefaultHandlingForAuthenticationChallenge: challenge];
}
#endif

- (NSCachedURLResponse *)connection:(NSURLConnection *)connection willCacheResponse:(NSCachedURLResponse *)cachedResponse
{
	// All FAppleHttpNSUrlConnectionRequest use NSURLRequestReloadIgnoringLocalCacheData
	// NSURLRequestReloadIgnoringLocalCacheData disables loading of data from cache, but responses can still be stored in cache
	// Returning nil in this delegate disables caching the responses
	return nil;
}

-(void) connectionDidFinishLoading:(NSURLConnection *)connection
{
	UE_LOG(LogHttp, Verbose, TEXT("connectionDidFinishLoading: %p"), self);
	self.bIsReady = YES;
}

- (TArray<uint8>&)getPayload
{
	return Payload;
}

-(uint64)getBytesWritten
{
	return self.BytesWritten;
}

@end


/****************************************************************************
 * FAppleHttpNSUrlConnectionResponse implementation
 **************************************************************************/

FAppleHttpNSUrlConnectionResponse::FAppleHttpNSUrlConnectionResponse(const FAppleHttpNSUrlConnectionRequest& InRequest)
	: Request( InRequest )
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionResponse::FAppleHttpNSUrlConnectionResponse()"));
	ResponseWrapper = [[FHttpResponseAppleNSUrlConnectionWrapper alloc] init];
}


FAppleHttpNSUrlConnectionResponse::~FAppleHttpNSUrlConnectionResponse()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionResponse::~FAppleHttpNSUrlConnectionResponse()"));
	[ResponseWrapper getPayload].Empty();

	[ResponseWrapper release];
	ResponseWrapper = nil;
}


FString FAppleHttpNSUrlConnectionResponse::GetURL() const
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionResponse::GetURL()"));
	return FString(Request.Request.URL.query);
}


FString FAppleHttpNSUrlConnectionResponse::GetURLParameter(const FString& ParameterName) const
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionResponse::GetURLParameter()"));

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


FString FAppleHttpNSUrlConnectionResponse::GetHeader(const FString& HeaderName) const
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionResponse::GetHeader()"));

	NSString* ConvertedHeaderName = HeaderName.GetNSString();
	return FString([[[ResponseWrapper Response] allHeaderFields] objectForKey:ConvertedHeaderName]);
}


TArray<FString> FAppleHttpNSUrlConnectionResponse::GetAllHeaders() const
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionResponse::GetAllHeaders()"));

	NSDictionary* Headers = [GetResponseObj() allHeaderFields];
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


FString FAppleHttpNSUrlConnectionResponse::GetContentType() const
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionResponse::GetContentType()"));

	return GetHeader( TEXT( "Content-Type" ) );
}


uint64 FAppleHttpNSUrlConnectionResponse::GetContentLength() const
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionResponse::GetContentLength()"));

	return ResponseWrapper.Response.expectedContentLength;
}


const TArray<uint8>& FAppleHttpNSUrlConnectionResponse::GetContent() const
{
	if( !IsReady() )
	{
		UE_LOG(LogHttp, Warning, TEXT("Payload is incomplete. Response still processing. %p"), &Request);
	}
	else
	{
		Payload = [ResponseWrapper getPayload];
		UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionResponse::GetContent() - Num: %i"), [ResponseWrapper getPayload].Num());
	}

	return Payload;
}


FString FAppleHttpNSUrlConnectionResponse::GetContentAsString() const
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionResponse::GetContentAsString()"));

	// Fill in our data.
	GetContent();

	TArray<uint8> ZeroTerminatedPayload;
	ZeroTerminatedPayload.AddZeroed( Payload.Num() + 1 );
	FMemory::Memcpy( ZeroTerminatedPayload.GetData(), Payload.GetData(), Payload.Num() );

	return UTF8_TO_TCHAR( ZeroTerminatedPayload.GetData() );
}


int32 FAppleHttpNSUrlConnectionResponse::GetResponseCode() const
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionResponse::GetResponseCode()"));

	return [GetResponseObj() statusCode];
}


NSHTTPURLResponse* FAppleHttpNSUrlConnectionResponse::GetResponseObj() const
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionResponse::GetResponseObj()"));

	return [ResponseWrapper Response];
}


bool FAppleHttpNSUrlConnectionResponse::IsReady() const
{
	bool Ready = [ResponseWrapper bIsReady];

	if( Ready )
	{
		UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionResponse::IsReady()"));
	}

	return Ready;
}

bool FAppleHttpNSUrlConnectionResponse::HadError() const
{
	bool bHadError = [ResponseWrapper bHadError];
	
	if( bHadError )
	{
		UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpNSUrlConnectionResponse::HadError()"));
	}
	
	return bHadError;
}

const uint64 FAppleHttpNSUrlConnectionResponse::GetNumBytesReceived() const
{
	return [ResponseWrapper getPayload].Num();
}

const uint64 FAppleHttpNSUrlConnectionResponse::GetNumBytesWritten() const
{
    uint64 NumBytesWritten = [ResponseWrapper getBytesWritten];
    return NumBytesWritten;
}
