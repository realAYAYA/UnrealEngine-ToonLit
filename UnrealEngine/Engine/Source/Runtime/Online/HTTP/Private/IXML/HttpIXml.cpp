// Copyright Epic Games, Inc. All Rights Reserved.

#include "HttpIXML.h"
#include "HttpManager.h"
#include "HAL/FileManager.h"

#if PLATFORM_HOLOLENS

#define CHECK_SUCCESS(a)  { bool success = SUCCEEDED( (a) ); check( success ); }

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
FHttpRequestIXML::FHttpRequestIXML()
	: RequestStatus( EHttpRequestStatus::NotStarted )
	, XHR( nullptr )
	, XHRCallback( nullptr )
	, ElapsedTime(0)
	, HttpCB(nullptr)
{
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
FHttpRequestIXML::~FHttpRequestIXML()
{
	// ComPtr<> smart pointers should handle releasing the COM objects.
}
//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
FString FHttpRequestIXML::GetURL() const
{
	return URL;
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
FString FHttpRequestIXML::GetURLParameter(const FString& ParameterName) const
{
	check(false);
	return TEXT("Not yet implemented");
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
FString FHttpRequestIXML::GetHeader(const FString& HeaderName) const
{
	const FString* Header = Headers.Find(HeaderName);
	return Header != NULL ? *Header : TEXT("");
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
TArray<FString> FHttpRequestIXML::GetAllHeaders() const
{
	TArray<FString> Result;
	for (TMap<FString, FString>::TConstIterator It(Headers); It; ++It)
	{
		Result.Add(It.Key() + TEXT(": ") + It.Value());
	}
	return Result;
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
FString FHttpRequestIXML::GetContentType() const
{
	return GetHeader(TEXT("Content-Type"));
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
int32 FHttpRequestIXML::GetContentLength() const
{
	return Payload->GetContentLength();
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
const TArray<uint8>& FHttpRequestIXML::GetContent() const
{
	return Payload->GetContent();
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
FString FHttpRequestIXML::GetVerb() const
{
	return Verb;
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
void FHttpRequestIXML::SetVerb(const FString& InVerb)
{
	Verb = InVerb;
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
void FHttpRequestIXML::SetURL(const FString& InURL)
{
	URL = InURL;
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
void FHttpRequestIXML::SetContent(const TArray<uint8>& ContentPayload)
{
	Payload = MakeUnique<FRequestPayloadInMemory>(ContentPayload);
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
void FHttpRequestIXML::SetContent(TArray<uint8>&& ContentPayload)
{
	Payload = MakeUnique<FRequestPayloadInMemory>(MoveTemp(ContentPayload));
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
void FHttpRequestIXML::SetContentAsString(const FString& ContentString)
{
	if ( ContentString.Len() )
	{
		int32 Utf8Length = FPlatformString::ConvertedLength<UTF8CHAR>(*ContentString, ContentString.Len());
		TArray<uint8> Buffer;
		Buffer.SetNumUninitialized(Utf8Length);
		FPlatformString::Convert((UTF8CHAR*)Buffer.GetData(), Buffer.Num(), *ContentString, ContentString.Len());
		Payload = MakeUnique<FRequestPayloadInMemory>(MoveTemp(Buffer));
	}
}

bool FHttpRequestIXML::SetContentAsStreamedFile(const FString& Filename)
{
	UE_LOG(LogHttp, Verbose, TEXT("FCurlHttpRequest::SetContentAsStreamedFile() - %s"), *Filename);

	if (RequestStatus == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("FCurlHttpRequest::SetContentAsStreamedFile() - attempted to set content on a request that is inflight"));
		return false;
	}

	FArchive* File = IFileManager::Get().CreateFileReader(*Filename);
	if (File)
	{
		Payload = MakeUnique<FRequestPayloadInFileStream>(MakeShareable(File));
		return true;
	}
	else
	{
		UE_LOG(LogHttp, Warning, TEXT("FCurlHttpRequest::SetContentAsStreamedFile Failed to open %s for reading"), *Filename);
		Payload.Reset();
		return false;
	}
}

bool FHttpRequestIXML::SetContentFromStream(TSharedRef<FArchive, ESPMode::ThreadSafe> Stream)
{
	UE_LOG(LogHttp, Verbose, TEXT("FCurlHttpRequest::SetContentFromStream() - %s"), *Stream->GetArchiveName());

	if (RequestStatus == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("FCurlHttpRequest::SetContentFromStream() - attempted to set content on a request that is inflight"));
		return false;
	}

	Payload = MakeUnique<FRequestPayloadInFileStream>(Stream);
	return true;
}


//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
void FHttpRequestIXML::SetHeader(const FString& HeaderName, const FString& HeaderValue)
{
	Headers.Add(HeaderName, HeaderValue);
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
void FHttpRequestIXML::AppendToHeader(const FString& HeaderName, const FString& AdditionalHeaderValue)
{
	if (!HeaderName.IsEmpty() && !AdditionalHeaderValue.IsEmpty())
	{
		FString* PreviousValue = Headers.Find(HeaderName);
		FString NewValue;
		if (PreviousValue != nullptr && !PreviousValue->IsEmpty())
		{
			NewValue = (*PreviousValue) + TEXT(", ");
		}
		NewValue += AdditionalHeaderValue;

		SetHeader(HeaderName, NewValue);
	}
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
bool FHttpRequestIXML::ProcessRequest()
{
	uint32 Result = 0;

	// Are we already processing?
	if (RequestStatus == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. Still processing last request."));
	}
	// Nothing to do without a URL
	else if (URL.IsEmpty())
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. No URL was specified."));
	}
	else
	{
		Result = CreateRequest();

		if (SUCCEEDED(Result))
		{
			Result = ApplyHeaders();

			if (SUCCEEDED(Result))
			{
				RequestStatus = EHttpRequestStatus::Processing;
				Response = MakeShareable( new FHttpResponseIXML(*this, HttpCB) );

				// Try to start the connection and send the Http request
				Result = SendRequest();

				if (SUCCEEDED(Result))
				{
					// Add to global list while being processed so that the ref counted request does not get deleted
					FHttpModule::Get().GetHttpManager().AddRequest(SharedThis(this));
				}
				else
				{
					// No response since connection failed
					Response = NULL;

					// Cleanup and call delegate
					if (!IsInGameThread())
					{
						FHttpModule::Get().GetHttpManager().AddGameThreadTask([StrongThis = StaticCastSharedRef<FHttpRequestIXML>(AsShared())]()
						{
							StrongThis->FinishedRequest();
						});
					}
					else
					{
						FinishedRequest();
					}
				}
			}
		}
		else
		{
			UE_LOG(LogHttp, Warning, TEXT("CreateRequest failed with error code %d URL=%s"), Result, *URL);
		}

	}

	return SUCCEEDED(Result);
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------

uint32 FHttpRequestIXML::CreateRequest()
{
	// Create the IXmlHttpRequest2 object.
	CHECK_SUCCESS( ::CoCreateInstance( __uuidof(FreeThreadedXMLHTTP60),
		nullptr,
		CLSCTX_SERVER,
		__uuidof(IXMLHTTPRequest2),
		&XHR ) );

	// Create the IXmlHttpRequest2Callback object and initialize it.
	CHECK_SUCCESS( Microsoft::WRL::Details::MakeAndInitialize<HttpCallback>( &HttpCB ) );
	CHECK_SUCCESS( HttpCB.As( &XHRCallback ) );

	// Open a connection for an HTTP GET request.
	// NOTE: This is where the IXMLHTTPRequest2 object gets given a
	// pointer to the IXMLHTTPRequest2Callback object.
	return XHR->Open( 
		*Verb,				// HTTP method
		*URL,					// URL string as wchar*
		XHRCallback.Get(),	// callback object from a ComPtr<>
		NULL,					// username
		NULL,					// password
		NULL,					// proxy username
		NULL );					// proxy password
};

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------

uint32 FHttpRequestIXML::ApplyHeaders()
{
	uint32 hr = S_OK;

	for ( auto It = Headers.CreateConstIterator(); It; ++It )
	{
		FString HeaderName  = It.Key();
		FString HeaderValue = It.Value();

		hr = XHR->SetRequestHeader( *HeaderName, *HeaderValue );

		if( FAILED( hr ) )
		{
			break;
		}
	}

	return hr;
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------

uint32 FHttpRequestIXML::SendRequest()
{
	uint32 hr = E_FAIL;

	if( Payload )
	{
		uint32 SizeInBytes = Payload->GetContentLength();

		// Create and open a new runtime class
		SendStream = Make<RequestStream>();
		LPCSTR PayloadChars = (LPCSTR)Payload->GetContent().GetData();
		SendStream->Open( PayloadChars, (ULONG) SizeInBytes );

		hr = XHR->Send(
			SendStream.Get(),        // body message as an ISequentialStream*
			SendStream->Size() );    // count of bytes in the stream.
	}
	else
	{
		hr = XHR->Send( NULL, 0 );
	}

	// The HTTP Request runs asynchronously from here...
	return hr;
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
void FHttpRequestIXML::CancelRequest()
{
	check ( XHR );

	XHR->Abort();
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
EHttpRequestStatus::Type FHttpRequestIXML::GetStatus() const
{
	return RequestStatus;
}

const FHttpResponsePtr FHttpRequestIXML::GetResponse() const
{
	return Response;
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
void FHttpRequestIXML::Tick(float DeltaSeconds)
{
	// IXML requests may need the app message pump operational
	// in order to progress.  If the core engine loop is not
	// running then we'll just pump messages ourselves.
	if (!GIsRunning)
	{
		FPlatformMisc::PumpMessages(true);
	}

	// keep track of elapsed seconds
	ElapsedTime += DeltaSeconds;
	const float HttpTimeout = GetTimeoutOrDefault();
	if (HttpTimeout > 0 &&
		ElapsedTime >= HttpTimeout)
	{
		UE_LOG(LogHttp, Warning, TEXT("Timeout processing Http request. %p"),
			this);

		// finish it off since it is timeout
		FinishedRequest();
	}

	// No longer waiting for a response and done processing it
	if (RequestStatus == EHttpRequestStatus::Processing &&
		Response.IsValid() &&
		HttpCB &&
		HttpCB->IsFinished() )
	{
		FinishedRequest();
	}
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
void FHttpRequestIXML::FinishedRequest()
{
	// Clean up session/request handles that may have been created
	CleanupRequest();

	// Remove from global list since processing is now complete
	FHttpModule::Get().GetHttpManager().RemoveRequest(SharedThis(this));

	if (Response.IsValid() &&
		Response->Succeeded() )
	{
		// Mark last request attempt as completed successfully
		RequestStatus = EHttpRequestStatus::Succeeded;
		// Call delegate with valid request/response objects
		OnProcessRequestComplete().ExecuteIfBound(SharedThis(this),Response,true);
	}
	else
	{
		// Mark last request attempt as completed but failed
		RequestStatus = EHttpRequestStatus::Failed;
		// Call delegate with failure
		OnProcessRequestComplete().ExecuteIfBound(SharedThis(this),Response,false);
	}
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
void FHttpRequestIXML::CleanupRequest()
{
}

float FHttpRequestIXML::GetElapsedTime() const
{
	return ElapsedTime;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//	
// FHttpResponseIXML
//
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
FHttpResponseIXML::FHttpResponseIXML(FHttpRequestIXML& InRequest, ComPtr<HttpCallback> InHttpCB)
	: Request( InRequest )
	, HttpCB( InHttpCB )
{
	check( HttpCB );
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------
FHttpResponseIXML::~FHttpResponseIXML()
{
	// ComPtr<> smart pointers should handle releasing the COM objects.
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------

bool FHttpResponseIXML::Succeeded()
{
	check ( HttpCB );

	if ( HttpCB->IsFinished() )
	{
		if (   ( HttpCB->GetHttpStatus() >= 200 )
			&& ( HttpCB->GetHttpStatus() <  300 ) )
		{
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------

FString FHttpResponseIXML::GetURL() const
{
	return Request.GetURL();
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------

FString FHttpResponseIXML::GetURLParameter(const FString& ParameterName) const
{
	return Request.GetURLParameter( ParameterName );
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------

FString FHttpResponseIXML::GetHeader(const FString& HeaderName) const
{
	FString SingleHeader;
	PWSTR SingleHeaderPtr;

	if( SUCCEEDED( Request.XHR->GetResponseHeader( *HeaderName, &SingleHeaderPtr )))
	{
		SingleHeader = SingleHeaderPtr;
	}

	return SingleHeader;
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------

TArray<FString> FHttpResponseIXML::GetAllHeaders() const
{
	TArray<FString> AllHeaders;
	PWSTR AllHeadersPtr;

	if( SUCCEEDED( Request.XHR->GetAllResponseHeaders( &AllHeadersPtr )))
	{
		FString AllHeadersString = FString( AllHeadersPtr );
		while( AllHeadersString.Contains(TEXT("\r\n")) )
		{
			FString Header, RestOfHeaders;
			AllHeadersString.Split(TEXT("\r\n"), &Header, &RestOfHeaders);

			if( !Header.IsEmpty() &&  Header.Contains(TEXT(":")) )
			{
				AllHeaders.Add( Header );
			}
			AllHeadersString = RestOfHeaders;
		}
	}

	return AllHeaders;
}

//-----------------------------------------------------------------------------
//	
//----------------------------------------------------------->-----------------

FString FHttpResponseIXML::GetContentType() const
{
	return GetHeader(TEXT("Content-Type"));
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------

int32 FHttpResponseIXML::GetContentLength() const
{
	check ( HttpCB );
	
	return HttpCB->GetContent().Num();
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------

const TArray<uint8>& FHttpResponseIXML::GetContent() const
{
	check ( HttpCB );
	
	return HttpCB->GetContent();
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------

int32 FHttpResponseIXML::GetResponseCode() const
{
	check ( HttpCB );

	return HttpCB->GetHttpStatus();
}

//-----------------------------------------------------------------------------
//	
//-----------------------------------------------------------------------------

FString FHttpResponseIXML::GetContentAsString() const
{
	TArray<uint8> ZeroTerminatedPayload(GetContent());
	ZeroTerminatedPayload.Add(0);
	return UTF8_TO_TCHAR(ZeroTerminatedPayload.GetData());
}

//-----------------------------------------------------------------------------
//	End of file

#endif // PLATFORM_HOLOLENS
