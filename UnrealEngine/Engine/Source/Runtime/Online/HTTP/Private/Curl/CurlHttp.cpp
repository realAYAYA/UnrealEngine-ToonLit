// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_CURL

#include "Curl/CurlHttp.h"
#include "Stats/Stats.h"
#include "Misc/App.h"
#include "HttpModule.h"
#include "Http.h"
#include "Misc/EngineVersion.h"
#include "Misc/Paths.h"
#include "Curl/CurlHttpManager.h"
#include "Misc/ScopeLock.h"
#include "HAL/FileManager.h"
#include "Internationalization/Regex.h"

#if WITH_SSL
#include "Ssl.h"

#include <openssl/ssl.h>

static int SslCertVerify(int PreverifyOk, X509_STORE_CTX* Context)
{
	if (PreverifyOk == 1)
	{
		SSL* Handle = static_cast<SSL*>(X509_STORE_CTX_get_ex_data(Context, SSL_get_ex_data_X509_STORE_CTX_idx()));
		check(Handle);

		SSL_CTX* SslContext = SSL_get_SSL_CTX(Handle);
		check(SslContext);

		FCurlHttpRequest* Request = static_cast<FCurlHttpRequest*>(SSL_CTX_get_app_data(SslContext));
		check(Request);

		const FString Domain = FPlatformHttp::GetUrlDomain(Request->GetURL());

		if (!FSslModule::Get().GetCertificateManager().VerifySslCertificates(Context, Domain))
		{
			PreverifyOk = 0;
		}
	}

	return PreverifyOk;
}

static CURLcode sslctx_function(CURL * curl, void * sslctx, void * parm)
{
	SSL_CTX* Context = static_cast<SSL_CTX*>(sslctx);
	const ISslCertificateManager& CertificateManager = FSslModule::Get().GetCertificateManager();

	CertificateManager.AddCertificatesToSslContext(Context);
	if (FCurlHttpManager::CurlRequestOptions.bVerifyPeer)
	{
		FCurlHttpRequest* Request = static_cast<FCurlHttpRequest*>(parm);
		SSL_CTX_set_verify(Context, SSL_CTX_get_verify_mode(Context), SslCertVerify);
		SSL_CTX_set_app_data(Context, Request);
	}

	/* all set to go */
	return CURLE_OK;
}
#endif //#if WITH_SSL

FCurlHttpRequest::FCurlHttpRequest()
	:	EasyHandle(nullptr)
	,	HeaderList(nullptr)
	,	bCanceled(false)
	,	bCurlRequestCompleted(false)
	,	bRedirected(false)
	,	CurlAddToMultiResult(CURLM_OK)
	,	CurlCompletionResult(CURLE_OK)
	,	CompletionStatus(EHttpRequestStatus::NotStarted)
	,	ElapsedTime(0.0f)
	,	TimeSinceLastResponse(0.0f)
	,	bAnyHttpActivity(false)
	,   BytesSent(0)
	,	TotalBytesSent(0)
	,	LastReportedBytesRead(0)
	,	LastReportedBytesSent(0)
	,   LeastRecentlyCachedInfoMessageIndex(0)
{
	checkf(FCurlHttpManager::IsInit(), TEXT("Curl request was created while the library is shutdown"));

	EasyHandle = curl_easy_init();

	// Always setup the debug function to allow for activity to be tracked
	curl_easy_setopt(EasyHandle, CURLOPT_DEBUGDATA, this);
	curl_easy_setopt(EasyHandle, CURLOPT_DEBUGFUNCTION, StaticDebugCallback);
	curl_easy_setopt(EasyHandle, CURLOPT_VERBOSE, 1L);

	curl_easy_setopt(EasyHandle, CURLOPT_BUFFERSIZE, FCurlHttpManager::CurlRequestOptions.BufferSize);

	curl_easy_setopt(EasyHandle, CURLOPT_USE_SSL, CURLUSESSL_ALL);

	// HTTP2 is linked in for newer libcurl builds and the library will use it by default.
	// There have been issues found with it use in production on long lived servers with heavy HTTP usage, for
	// that reason we're disabling its use by default in the general purpose curl request wrapper and only
	// allowing use of HTTP2 from other curl wrappers like the DerivedDataCache one.
	// Note that CURL_HTTP_VERSION_1_1 was the default for libcurl version before 7.62.0
	curl_easy_setopt(EasyHandle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

	// set certificate verification (disable to allow self-signed certificates)
	if (FCurlHttpManager::CurlRequestOptions.bVerifyPeer)
	{
		curl_easy_setopt(EasyHandle, CURLOPT_SSL_VERIFYPEER, 1L);
	}
	else
	{
		curl_easy_setopt(EasyHandle, CURLOPT_SSL_VERIFYPEER, 0L);
	}

	// allow http redirects to be followed
	curl_easy_setopt(EasyHandle, CURLOPT_FOLLOWLOCATION, 1L);

	// required for all multi-threaded handles
	curl_easy_setopt(EasyHandle, CURLOPT_NOSIGNAL, 1L);

	// associate with this just in case
	curl_easy_setopt(EasyHandle, CURLOPT_PRIVATE, this);

	const FString& ProxyAddress = FHttpModule::Get().GetProxyAddress();
	if (!ProxyAddress.IsEmpty())
	{
		// guaranteed to be valid at this point
		curl_easy_setopt(EasyHandle, CURLOPT_PROXY, TCHAR_TO_ANSI(*ProxyAddress));
	}

	if (FCurlHttpManager::CurlRequestOptions.bDontReuseConnections)
	{
		curl_easy_setopt(EasyHandle, CURLOPT_FORBID_REUSE, 1L);
	}

#if PLATFORM_LINUX && !WITH_SSL
	static const char* const CertBundlePath = []() -> const char* {
		static const char * KnownBundlePaths[] =
		{
			"/etc/pki/tls/certs/ca-bundle.crt",
			"/etc/ssl/certs/ca-certificates.crt",
			"/etc/ssl/ca-bundle.pem"
		};

		for (const char* BundlePath : KnownBundlePaths)
		{
			FString FileName(BundlePath);
			UE_LOG(LogHttp, Log, TEXT(" Libcurl: checking if '%s' exists"), *FileName);

			if (FPaths::FileExists(FileName))
			{
				return BundlePath;
			}
		}

		return nullptr;
	}();

	// set CURLOPT_CAINFO to a bundle we know exists as the default may not be present
	curl_easy_setopt(EasyHandle, CURLOPT_CAINFO, CertBundlePath);
#endif

	curl_easy_setopt(EasyHandle, CURLOPT_SSLCERTTYPE, "PEM");
#if WITH_SSL
	// unset CURLOPT_CAINFO as certs will be added via sslctx_function
	curl_easy_setopt(EasyHandle, CURLOPT_CAINFO, nullptr);
	curl_easy_setopt(EasyHandle, CURLOPT_SSL_CTX_FUNCTION, *sslctx_function);
	curl_easy_setopt(EasyHandle, CURLOPT_SSL_CTX_DATA, this);
#endif // #if WITH_SSL

	InfoMessageCache.AddDefaulted(NumberOfInfoMessagesToCache);

	// Add default headers
	const TMap<FString, FString>& DefaultHeaders = FHttpModule::Get().GetDefaultHeaders();
	for (TMap<FString, FString>::TConstIterator It(DefaultHeaders); It; ++It)
	{
		SetHeader(It.Key(), It.Value());
	}
}

FCurlHttpRequest::~FCurlHttpRequest()
{
	checkf(FCurlHttpManager::IsInit(), TEXT("Curl request was held after the library was shutdown."));
	if (EasyHandle)
	{
		// clear to prevent crashing in debug callback when this handle is part of an asynchronous curl_multi_perform()
		curl_easy_setopt(EasyHandle, CURLOPT_DEBUGDATA, nullptr);

		// cleanup the handle first (that order is used in howtos)
		curl_easy_cleanup(EasyHandle);
		EasyHandle = nullptr;
	}

	// destroy headers list
	if (HeaderList)
	{
		curl_slist_free_all(HeaderList);
		HeaderList = nullptr;
	}
}

FString FCurlHttpRequest::GetURL() const
{
	return URL;
}

FString FCurlHttpRequest::GetURLParameter(const FString& ParameterName) const
{
	TArray<FString> StringElements;

	//Parameters start after "?" in url
	FString Path, Parameters;
	if (URL.Split(TEXT("?"), &Path, &Parameters))
	{
		int32 NumElems = Parameters.ParseIntoArray(StringElements, TEXT("&"), true);
		check(NumElems == StringElements.Num());
		
		FString ParamValDelimiter(TEXT("="));
		for (int Idx = 0; Idx < NumElems; ++Idx )
		{
			FString Param, Value;
			if (StringElements[Idx].Split(ParamValDelimiter, &Param, &Value) && Param == ParameterName)
			{
				// unescape
				auto Converter = StringCast<ANSICHAR>(*Value);
				char * EscapedAnsi = (char *)Converter.Get();
				int32 EscapedLength = Converter.Length();

				int32 UnescapedLength = 0;	
				char * UnescapedAnsi = curl_easy_unescape(EasyHandle, EscapedAnsi, EscapedLength, &UnescapedLength);
				
				FString UnescapedValue(ANSI_TO_TCHAR(UnescapedAnsi));
				curl_free(UnescapedAnsi);
				
				return UnescapedValue;
			}
		}
	}

	return FString();
}

FString FCurlHttpRequest::GetHeader(const FString& HeaderName) const
{
	FString Result;

	const FString* Header = Headers.Find(HeaderName);
	if (Header != NULL)
	{
		Result = *Header;
	}
	
	return Result;
}

FString FCurlHttpRequest::CombineHeaderKeyValue(const FString& HeaderKey, const FString& HeaderValue)
{
	FString Combined;
	const TCHAR Separator[] = TEXT(": ");
	constexpr const int32 SeparatorLength = UE_ARRAY_COUNT(Separator) - 1;
	Combined.Reserve(HeaderKey.Len() + SeparatorLength + HeaderValue.Len());
	Combined.Append(HeaderKey);
	Combined.AppendChars(Separator, SeparatorLength);
	Combined.Append(HeaderValue);
	return Combined;
}

TArray<FString> FCurlHttpRequest::GetAllHeaders() const
{
	TArray<FString> Result;
	Result.Reserve(Headers.Num());
	for (const TPair<FString, FString>& It : Headers)
	{
		Result.Emplace(CombineHeaderKeyValue(It.Key, It.Value));
	}
	return Result;
}

FString FCurlHttpRequest::GetContentType() const
{
	return GetHeader(TEXT( "Content-Type" ));
}

int32 FCurlHttpRequest::GetContentLength() const
{
	return RequestPayload.IsValid() ? RequestPayload->GetContentLength() : 0;
}

const TArray<uint8>& FCurlHttpRequest::GetContent() const
{
	static const TArray<uint8> EmptyContent;
	return RequestPayload.IsValid() ? RequestPayload->GetContent() : EmptyContent;
}

void FCurlHttpRequest::SetVerb(const FString& InVerb)
{
	if (CompletionStatus == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("FCurlHttpRequest::SetVerb() - attempted to set verb on a request that is inflight"));
		return;
	}

	check(EasyHandle);
	Verb = InVerb.ToUpper();
}

void FCurlHttpRequest::SetURL(const FString& InURL)
{
	if (CompletionStatus == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("FCurlHttpRequest::SetURL() - attempted to set url on a request that is inflight"));
		return;
	}

	check(EasyHandle);
	URL = InURL;
}

void FCurlHttpRequest::SetContent(const TArray<uint8>& ContentPayload)
{
	SetContent(CopyTemp(ContentPayload));
}

void FCurlHttpRequest::SetContent(TArray<uint8>&& ContentPayload)
{
	if (CompletionStatus == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("FCurlHttpRequest::SetContent() - attempted to set content on a request that is inflight"));
		return;
	}

	RequestPayload = MakeUnique<FRequestPayloadInMemory>(MoveTemp(ContentPayload));
	bIsRequestPayloadSeekable = true;
}

void FCurlHttpRequest::SetContentAsString(const FString& ContentString)
{
	if (CompletionStatus == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("FCurlHttpRequest::SetContentAsString() - attempted to set content on a request that is inflight"));
		return;
	}

	int32 Utf8Length = FPlatformString::ConvertedLength<UTF8CHAR>(*ContentString, ContentString.Len());
	TArray<uint8> Buffer;
	Buffer.SetNumUninitialized(Utf8Length);
	FPlatformString::Convert((UTF8CHAR*)Buffer.GetData(), Buffer.Num(), *ContentString, ContentString.Len());
	RequestPayload = MakeUnique<FRequestPayloadInMemory>(MoveTemp(Buffer));
	bIsRequestPayloadSeekable = true;
}

bool FCurlHttpRequest::SetContentAsStreamedFile(const FString& Filename)
{
	UE_LOG(LogHttp, Verbose, TEXT("FCurlHttpRequest::SetContentAsStreamedFile() - %s"), *Filename);

	if (CompletionStatus == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("FCurlHttpRequest::SetContentAsStreamedFile() - attempted to set content on a request that is inflight"));
		return false;
	}

	FArchive* File = IFileManager::Get().CreateFileReader(*Filename);
	if (File)
	{
		RequestPayload = MakeUnique<FRequestPayloadInFileStream>(MakeShareable(File));
	}
	else
	{
		UE_LOG(LogHttp, Warning, TEXT("FCurlHttpRequest::SetContentAsStreamedFile Failed to open %s for reading"), *Filename);
		RequestPayload.Reset();
	}
	bIsRequestPayloadSeekable = false;
	return RequestPayload.IsValid();
}

bool FCurlHttpRequest::SetContentFromStream(TSharedRef<FArchive, ESPMode::ThreadSafe> Stream)
{
	UE_LOG(LogHttp, Verbose, TEXT("FCurlHttpRequest::SetContentFromStream() - %s"), *Stream->GetArchiveName());

	if (CompletionStatus == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("FCurlHttpRequest::SetContentFromStream() - attempted to set content on a request that is inflight"));
		return false;
	}

	RequestPayload = MakeUnique<FRequestPayloadInFileStream>(Stream);
	bIsRequestPayloadSeekable = false;
	return true;
}

void FCurlHttpRequest::SetHeader(const FString& HeaderName, const FString& HeaderValue)
{
	if (CompletionStatus == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("FCurlHttpRequest::SetHeader() - attempted to set header on a request that is inflight"));
		return;
	}

	Headers.Add(HeaderName, HeaderValue);
}

void FCurlHttpRequest::AppendToHeader(const FString& HeaderName, const FString& AdditionalHeaderValue)
{
	if (CompletionStatus == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("FCurlHttpRequest::AppendToHeader() - attempted to append to header on a request that is inflight"));
		return;
	}

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

FString FCurlHttpRequest::GetVerb() const
{
	return Verb;
}

size_t FCurlHttpRequest::StaticUploadCallback(void* Ptr, size_t SizeInBlocks, size_t BlockSizeInBytes, void* UserData)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FCurlHttpRequest_StaticUploadCallback);
	check(Ptr);
	check(UserData);

	// dispatch
	FCurlHttpRequest* Request = reinterpret_cast<FCurlHttpRequest*>(UserData);
	return Request->UploadCallback(Ptr, SizeInBlocks, BlockSizeInBytes);
}

int FCurlHttpRequest::StaticSeekCallback(void* UserData, curl_off_t Offset, int Origin)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FCurlHttpRequest_StaticSeekCallback);
	check(UserData);

	// dispatch
	FCurlHttpRequest* Request = reinterpret_cast<FCurlHttpRequest*>(UserData);
	return Request->SeekCallback(Offset, Origin);
}

size_t FCurlHttpRequest::StaticReceiveResponseHeaderCallback(void* Ptr, size_t SizeInBlocks, size_t BlockSizeInBytes, void* UserData)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FCurlHttpRequest_StaticReceiveResponseHeaderCallback);
	check(Ptr);
	check(UserData);

	// dispatch
	FCurlHttpRequest* Request = reinterpret_cast<FCurlHttpRequest*>(UserData);
	return Request->ReceiveResponseHeaderCallback(Ptr, SizeInBlocks, BlockSizeInBytes);	
}

size_t FCurlHttpRequest::StaticReceiveResponseBodyCallback(void* Ptr, size_t SizeInBlocks, size_t BlockSizeInBytes, void* UserData)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FCurlHttpRequest_StaticReceiveResponseBodyCallback);
	check(Ptr);
	check(UserData);

	// dispatch
	FCurlHttpRequest* Request = reinterpret_cast<FCurlHttpRequest*>(UserData);
	return Request->ReceiveResponseBodyCallback(Ptr, SizeInBlocks, BlockSizeInBytes);	
}

size_t FCurlHttpRequest::StaticDebugCallback(CURL * Handle, curl_infotype DebugInfoType, char * DebugInfo, size_t DebugInfoSize, void* UserData)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FCurlHttpRequest_StaticDebugCallback);
	check(Handle);
	check(UserData);

	// dispatch
	FCurlHttpRequest* Request = reinterpret_cast<FCurlHttpRequest*>(UserData);
	return Request->DebugCallback(Handle, DebugInfoType, DebugInfo, DebugInfoSize);
}

size_t FCurlHttpRequest::ReceiveResponseHeaderCallback(void* Ptr, size_t SizeInBlocks, size_t BlockSizeInBytes)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FCurlHttpRequest_ReceiveResponseHeaderCallback);
	check(Response.IsValid());
	
	TimeSinceLastResponse = 0.0f;
	if (Response.IsValid())
	{
		uint32 HeaderSize = SizeInBlocks * BlockSizeInBytes;
		if (HeaderSize > 0 && HeaderSize <= CURL_MAX_HTTP_HEADER)
		{
			TArray<char> AnsiHeader;
			AnsiHeader.AddUninitialized(HeaderSize + 1);

			FMemory::Memcpy(AnsiHeader.GetData(), Ptr, HeaderSize);
			AnsiHeader[HeaderSize] = 0;

			FString Header(ANSI_TO_TCHAR(AnsiHeader.GetData()));
			// kill \n\r
			Header = Header.Replace(TEXT("\n"), TEXT(""));
			Header = Header.Replace(TEXT("\r"), TEXT(""));

			UE_LOG(LogHttp, Verbose, TEXT("%p: Received response header '%s'."), this, *Header);

			FString HeaderKey, HeaderValue;
			if (Header.Split(TEXT(":"), &HeaderKey, &HeaderValue))
			{
				HeaderValue.TrimStartInline();
				if (!HeaderKey.IsEmpty() && !HeaderValue.IsEmpty() && !bRedirected)
				{
					//Store the content length so OnRequestProgress() delegates have something to work with
					if (HeaderKey == TEXT("Content-Length"))
					{
						Response->ContentLength = FCString::Atoi(*HeaderValue);
					}
					Response->NewlyReceivedHeaders.Enqueue(TPair<FString, FString>(MoveTemp(HeaderKey), MoveTemp(HeaderValue)));
				}
			}
			else
			{
				long HttpCode = 0;
				if (CURLE_OK == curl_easy_getinfo(EasyHandle, CURLINFO_RESPONSE_CODE, &HttpCode))
				{
					bRedirected = (HttpCode >= 300 && HttpCode < 400);
				}
			}
			return HeaderSize;
		}
		else
		{
			UE_LOG(LogHttp, Warning, TEXT("%p: Could not process response header for request - header size (%d) is invalid."), this, HeaderSize);
		}
	}
	else
	{
		UE_LOG(LogHttp, Warning, TEXT("%p: Could not download response header for request - response not valid."), this);
	}

	return 0;
}

size_t FCurlHttpRequest::ReceiveResponseBodyCallback(void* Ptr, size_t SizeInBlocks, size_t BlockSizeInBytes)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FCurlHttpRequest_ReceiveResponseBodyCallback);
	check(Response.IsValid());
	  
	TimeSinceLastResponse = 0.0f;
	if (Response.IsValid())
	{
		uint32 SizeToDownload = SizeInBlocks * BlockSizeInBytes;

		UE_LOG(LogHttp, Verbose, TEXT("%p: ReceiveResponseBodyCallback: %d bytes out of %d received. (SizeInBlocks=%d, BlockSizeInBytes=%d, Response->TotalBytesRead=%d, Response->GetContentLength()=%d, SizeToDownload=%d (<-this will get returned from the callback))"),
			this,
			static_cast<int32>(Response->TotalBytesRead.GetValue() + SizeToDownload), Response->GetContentLength(),
			static_cast<int32>(SizeInBlocks), static_cast<int32>(BlockSizeInBytes), Response->TotalBytesRead.GetValue(), Response->GetContentLength(), static_cast<int32>(SizeToDownload)
			);

		// note that we can be passed 0 bytes if file transmitted has 0 length
		if (SizeToDownload > 0)
		{
			Response->Payload.AddUninitialized(SizeToDownload);

			// save
			FMemory::Memcpy(static_cast<uint8*>(Response->Payload.GetData()) + Response->TotalBytesRead.GetValue(), Ptr, SizeToDownload);
			Response->TotalBytesRead.Add(SizeToDownload);

			return SizeToDownload;
		}
	}
	else
	{
		UE_LOG(LogHttp, Warning, TEXT("%p: Could not download response data for request - response not valid."), this);
	}

	return 0;	// request will fail with write error if we had non-zero bytes to download
}

size_t FCurlHttpRequest::UploadCallback(void* Ptr, size_t SizeInBlocks, size_t BlockSizeInBytes)
{
	TimeSinceLastResponse = 0.0f;

	size_t MaxBufferSize = SizeInBlocks * BlockSizeInBytes;
	size_t SizeAlreadySent = static_cast<size_t>(BytesSent.GetValue());
	size_t SizeSentThisTime = RequestPayload->FillOutputBuffer(Ptr, MaxBufferSize, SizeAlreadySent);
	BytesSent.Add(SizeSentThisTime);
	TotalBytesSent.Add(SizeSentThisTime);

	UE_LOG(LogHttp, Verbose, TEXT("%p: UploadCallback: %d bytes out of %d sent (%d bytes total sent). (SizeInBlocks=%d, BlockSizeInBytes=%d, SizeToSendThisTime=%d (<-this will get returned from the callback))"),
		this,
		static_cast< int32 >(BytesSent.GetValue()),
		RequestPayload->GetContentLength(),
		static_cast< int32 >(TotalBytesSent.GetValue()),
		static_cast< int32 >(SizeInBlocks),
		static_cast< int32 >(BlockSizeInBytes),
		static_cast< int32 >(SizeSentThisTime)
		);

	return SizeSentThisTime;
}

int FCurlHttpRequest::SeekCallback(curl_off_t Offset, int Origin)
{
	// Only support seeking to the very beginning
	if (bIsRequestPayloadSeekable && Origin == SEEK_SET && Offset == 0)
	{
		UE_LOG(LogHttp, Log, TEXT("%p: SeekCallback: Resetting to the beginning. We had uploaded %d bytes"),
			this,
			static_cast<int32>(BytesSent.GetValue()));
		BytesSent.Reset();
		bIsRequestPayloadSeekable = false; // Do not attempt to re-seek
		return CURL_SEEKFUNC_OK;
	}
	UE_LOG(LogHttp, Warning, TEXT("%p: SeekCallback: Failed to seek to Offset=%lld, Origin=%d %s"), 
		this,
		(int64)(Offset),
		Origin, 
		bIsRequestPayloadSeekable ? TEXT("not implemented") : TEXT("seek disabled"));
	return CURL_SEEKFUNC_CANTSEEK;
}

size_t FCurlHttpRequest::DebugCallback(CURL * Handle, curl_infotype DebugInfoType, char * DebugInfo, size_t DebugInfoSize)
{
	check(Handle == EasyHandle);	// make sure it's us
#if CURL_ENABLE_DEBUG_CALLBACK
	switch(DebugInfoType)
	{
		case CURLINFO_TEXT:
			{
				// in this case DebugInfo is a C string (see http://curl.haxx.se/libcurl/c/debug.html)
				// C string is not null terminated:  https://curl.haxx.se/libcurl/c/CURLOPT_DEBUGFUNCTION.html

				// Truncate at 1023 characters. This is just an arbitrary number based on a buffer size seen in
				// the libcurl code.
				DebugInfoSize = FMath::Min(DebugInfoSize, (size_t)1023);

				// Calculate the actual length of the string due to incorrect use of snprintf() in lib/vtls/openssl.c.
				char* FoundNulPtr = (char*)memchr(DebugInfo, 0, DebugInfoSize);
				int CalculatedSize = FoundNulPtr != nullptr ? FoundNulPtr - DebugInfo : DebugInfoSize;

				FString DebugText(CalculatedSize, static_cast<const ANSICHAR*>(DebugInfo));

				DebugText.ReplaceInline(TEXT("\n"), TEXT(""), ESearchCase::CaseSensitive);
				DebugText.ReplaceInline(TEXT("\r"), TEXT(""), ESearchCase::CaseSensitive);
				UE_LOG(LogHttp, VeryVerbose, TEXT("%p: '%s'"), this, *DebugText);
				const FScopeLock CacheLock(&InfoMessageCacheCriticalSection);
				if (InfoMessageCache.Num() > 0)
				{
					InfoMessageCache[LeastRecentlyCachedInfoMessageIndex] = MoveTemp(DebugText);
					LeastRecentlyCachedInfoMessageIndex = (LeastRecentlyCachedInfoMessageIndex + 1) % InfoMessageCache.Num();
				}
			}
			break;

		case CURLINFO_HEADER_IN:
			UE_LOG(LogHttp, VeryVerbose, TEXT("%p: Received header (%d bytes)"), this, DebugInfoSize);
			break;

		case CURLINFO_HEADER_OUT:
			{
				if (UE_LOG_ACTIVE(LogHttp, VeryVerbose))
				{
					// C string is not null terminated:  https://curl.haxx.se/libcurl/c/CURLOPT_DEBUGFUNCTION.html

					// Scan for \r\n\r\n.  According to some code in tool_cb_dbg.c, special processing is needed for
					// CURLINFO_HEADER_OUT blocks when containing both headers and data (which may be binary).
					//
					// Truncate at 1023 characters. This is just an arbitrary number based on a buffer size seen in
					// the libcurl code.
					int RecalculatedSize = FMath::Min(DebugInfoSize, (size_t)1023);
					for (int Index = 0; Index <= RecalculatedSize - 4; ++Index)
					{
						if (DebugInfo[Index] == '\r' && DebugInfo[Index + 1] == '\n'
								&& DebugInfo[Index + 2] == '\r' && DebugInfo[Index + 3] == '\n')
						{
							RecalculatedSize = Index;
							break;
						}
					}

					// As lib/http.c states that CURLINFO_HEADER_OUT may contain binary data, only print it if
					// the header data is readable.
					bool bIsPrintable = true;
					for (int Index = 0; Index < RecalculatedSize; ++Index)
					{
						unsigned char Ch = DebugInfo[Index];
						if (!isprint(Ch) && !isspace(Ch))
						{
							bIsPrintable = false;
							break;
						}
					}

					if (bIsPrintable)
					{
						FString DebugText(RecalculatedSize, static_cast<const ANSICHAR*>(DebugInfo));

						DebugText.ReplaceInline(TEXT("\n"), TEXT(""), ESearchCase::CaseSensitive);
						DebugText.ReplaceInline(TEXT("\r"), TEXT(""), ESearchCase::CaseSensitive);
						UE_LOG(LogHttp, VeryVerbose, TEXT("%p: Sent header (%d bytes) - %s"), this, RecalculatedSize, *DebugText);
					}
					else
					{
						UE_LOG(LogHttp, VeryVerbose, TEXT("%p: Sent header (%d bytes) - contains binary data"), this, RecalculatedSize);
					}
					}
			}
			break;

		case CURLINFO_DATA_IN:
			UE_LOG(LogHttp, VeryVerbose, TEXT("%p: Received data (%d bytes)"), this, DebugInfoSize);
			break;

		case CURLINFO_DATA_OUT:
			UE_LOG(LogHttp, VeryVerbose, TEXT("%p: Sent data (%d bytes)"), this, DebugInfoSize);
			break;

		case CURLINFO_SSL_DATA_IN:
			UE_LOG(LogHttp, VeryVerbose, TEXT("%p: Received SSL data (%d bytes)"), this, DebugInfoSize);
			break;

		case CURLINFO_SSL_DATA_OUT:
			UE_LOG(LogHttp, VeryVerbose, TEXT("%p: Sent SSL data (%d bytes)"), this, DebugInfoSize);
			break;

		default:
			UE_LOG(LogHttp, VeryVerbose, TEXT("%p: DebugCallback: Unknown DebugInfoType=%d (DebugInfoSize: %d bytes)"), this, (int32)DebugInfoType, DebugInfoSize);
			break;
	}
#endif // CURL_ENABLE_DEBUG_CALLBACK

	switch (DebugInfoType)
	{
		case CURLINFO_HEADER_IN:
		case CURLINFO_HEADER_OUT:
		case CURLINFO_DATA_IN:
		case CURLINFO_DATA_OUT:
		case CURLINFO_SSL_DATA_IN:
		case CURLINFO_SSL_DATA_OUT:
			TimeSinceLastResponse = 0.0f;
			bAnyHttpActivity = true;
			break;
		default:
			break;
	}
	
	return 0;
}

bool FCurlHttpRequest::SetupRequest()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FCurlHttpRequest_SetupRequest);
	check(EasyHandle);

	// Disabled http request processing
	if (!FHttpModule::Get().IsHttpEnabled())
	{
		UE_LOG(LogHttp, Verbose, TEXT("Http disabled. Skipping request. url=%s"), *GetURL());
		return false;
	}
	// Prevent overlapped requests using the same instance
	else if (CompletionStatus == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. Still processing last request."));
		return false;
	}
	// Nothing to do without a valid URL
	else if (URL.IsEmpty())
	{
		UE_LOG(LogHttp, Log, TEXT("Cannot process HTTP request: URL is empty"));
		return false;
	}
	if ((GetVerb().IsEmpty() || GetVerb().Equals(TEXT("GET"), ESearchCase::IgnoreCase))
		&& (RequestPayload.IsValid() && RequestPayload->GetContentLength() > 0))
	{
		UE_LOG(LogHttp, Warning, TEXT("An HTTP Get request cannot contain a payload."));
		return false;
	}

	// set up request

	if (!RequestPayload.IsValid())
	{
		RequestPayload = MakeUnique<FRequestPayloadInMemory>(TArray<uint8>());
		bIsRequestPayloadSeekable = true;
	}

	bCurlRequestCompleted = false;
	bCanceled = false;
	CurlAddToMultiResult = CURLM_OK;

	// default no verb to a GET
	if (Verb.IsEmpty())
	{
		Verb = TEXT("GET");
	}

	UE_LOG(LogHttp, Verbose, TEXT("%p: URL='%s'"), this, *URL);
	UE_LOG(LogHttp, Verbose, TEXT("%p: Verb='%s'"), this, *Verb);
	UE_LOG(LogHttp, Verbose, TEXT("%p: Custom headers are %s"), this, Headers.Num() ? TEXT("present") : TEXT("NOT present"));
	UE_LOG(LogHttp, Verbose, TEXT("%p: Payload size=%d"), this, RequestPayload->GetContentLength());

	if (GetHeader(TEXT("User-Agent")).IsEmpty())
	{
		SetHeader(TEXT("User-Agent"), FPlatformHttp::GetDefaultUserAgent());
	}

	// content-length should be present http://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html#sec4.4
	if (GetHeader(TEXT("Content-Length")).IsEmpty())
	{
		SetHeader(TEXT("Content-Length"), FString::Printf(TEXT("%d"), RequestPayload->GetContentLength()));
	}

	// Remove "Expect: 100-continue" since this is supposed to cause problematic behavior on Amazon ELB (and WinInet doesn't send that either)
	// (also see http://www.iandennismiller.com/posts/curl-http1-1-100-continue-and-multipartform-data-post.html , http://the-stickman.com/web-development/php-and-curl-disabling-100-continue-header/ )
	if (GetHeader(TEXT("Expect")).IsEmpty())
	{
		SetHeader(TEXT("Expect"), TEXT(""));
	}

	return true;
}

bool FCurlHttpRequest::SetupRequestHttpThread()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FCurlHttpRequest_SetupRequestHttpThread);
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FCurlHttpRequest_SetupRequest_SLIST_FREE_HEADERS);
		curl_slist_free_all(HeaderList);
		HeaderList = nullptr;
	}

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FCurlHttpRequest_SetupRequest_EASY_SETOPT);

		curl_easy_setopt(EasyHandle, CURLOPT_URL, TCHAR_TO_ANSI(*URL));

		if (!FCurlHttpManager::CurlRequestOptions.LocalHostAddr.IsEmpty())
		{
			// Set the local address to use for making these requests
			CURLcode ErrCode = curl_easy_setopt(EasyHandle, CURLOPT_INTERFACE, TCHAR_TO_ANSI(*FCurlHttpManager::CurlRequestOptions.LocalHostAddr));
		}

		bool bUseReadFunction = false;

		// set up verb (note that Verb is expected to be uppercase only)
		if (Verb == TEXT("POST"))
		{
			// If we don't pass any other Content-Type, RequestPayload is assumed to be URL-encoded by this time
			// In the case of using a streamed file, you must explicitly set the Content-Type, because RequestPayload->IsURLEncoded returns false.
			check(!GetHeader(TEXT("Content-Type")).IsEmpty() || RequestPayload->IsURLEncoded());
			curl_easy_setopt(EasyHandle, CURLOPT_POST, 1L);
			curl_easy_setopt(EasyHandle, CURLOPT_POSTFIELDS, NULL);
#if WITH_CURL_XCURL
			curl_easy_setopt(EasyHandle, CURLOPT_INFILESIZE, RequestPayload->GetContentLength());
#else
			curl_easy_setopt(EasyHandle, CURLOPT_POSTFIELDSIZE, RequestPayload->GetContentLength());
#endif
			bUseReadFunction = true;
		}
		else if (Verb == TEXT("PUT") || Verb == TEXT("PATCH"))
		{
			curl_easy_setopt(EasyHandle, CURLOPT_UPLOAD, 1L);
			curl_easy_setopt(EasyHandle, CURLOPT_INFILESIZE, RequestPayload->GetContentLength());
			if (Verb != TEXT("PUT"))
			{
				curl_easy_setopt(EasyHandle, CURLOPT_CUSTOMREQUEST, TCHAR_TO_UTF8(*Verb));
			}

			bUseReadFunction = true;
		}
		else if (Verb == TEXT("GET"))
		{
			// technically might not be needed unless we reuse the handles
			curl_easy_setopt(EasyHandle, CURLOPT_HTTPGET, 1L);
		}
		else if (Verb == TEXT("HEAD"))
		{
			curl_easy_setopt(EasyHandle, CURLOPT_NOBODY, 1L);
		}
		else if (Verb == TEXT("DELETE"))
		{
			// If we don't pass any other Content-Type, RequestPayload is assumed to be URL-encoded by this time
			// (if we pass, don't check here and trust the request)
			check(!GetHeader(TEXT("Content-Type")).IsEmpty() || RequestPayload->IsURLEncoded());

			curl_easy_setopt(EasyHandle, CURLOPT_POST, 1L);
			curl_easy_setopt(EasyHandle, CURLOPT_CUSTOMREQUEST, "DELETE");
			curl_easy_setopt(EasyHandle, CURLOPT_POSTFIELDSIZE, RequestPayload->GetContentLength());
			bUseReadFunction = true;
		}
		else
		{
			UE_LOG(LogHttp, Fatal, TEXT("Unsupported verb '%s', can be perhaps added with CURLOPT_CUSTOMREQUEST"), *Verb);
			UE_DEBUG_BREAK();
		}

		if (bUseReadFunction)
		{
			BytesSent.Reset();
			TotalBytesSent.Reset();
			curl_easy_setopt(EasyHandle, CURLOPT_READDATA, this);
			curl_easy_setopt(EasyHandle, CURLOPT_READFUNCTION, StaticUploadCallback);
		}

		// set up header function to receive response headers
		curl_easy_setopt(EasyHandle, CURLOPT_HEADERDATA, this);
		curl_easy_setopt(EasyHandle, CURLOPT_HEADERFUNCTION, StaticReceiveResponseHeaderCallback);

		// set up write function to receive response payload
		curl_easy_setopt(EasyHandle, CURLOPT_WRITEDATA, this);
		curl_easy_setopt(EasyHandle, CURLOPT_WRITEFUNCTION, StaticReceiveResponseBodyCallback);

		// set up headers

		// Empty string here tells Curl to list all supported encodings, allowing servers to send compressed content.
		if (FCurlHttpManager::CurlRequestOptions.bAcceptCompressedContent)
		{
			curl_easy_setopt(EasyHandle, CURLOPT_ACCEPT_ENCODING, "");
		}

		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FCurlHttpRequest_SetupRequest_SLIST_APPEND_HEADERS);

			TArray<FString> AllHeaders = GetAllHeaders();
			const int32 NumAllHeaders = AllHeaders.Num();
			for (int32 Idx = 0; Idx < NumAllHeaders; ++Idx)
			{
				const bool bCanLogHeaderValue = !AllHeaders[Idx].Contains(TEXT("Authorization"));
				if (bCanLogHeaderValue)
				{
					UE_LOG(LogHttp, Verbose, TEXT("%p: Adding header '%s'"), this, *AllHeaders[Idx]);
				}

				curl_slist* NewHeaderList = curl_slist_append(HeaderList, TCHAR_TO_UTF8(*AllHeaders[Idx]));
				if (!NewHeaderList)
				{
					if (bCanLogHeaderValue)
					{
						UE_LOG(LogHttp, Warning, TEXT("Failed to append header '%s'"), *AllHeaders[Idx]);
					}
					else
					{
						UE_LOG(LogHttp, Warning, TEXT("Failed to append header 'Authorization'"));
					}
				}
				else
				{
					HeaderList = NewHeaderList;
				}
			}
		}

		if (HeaderList)
		{
			curl_easy_setopt(EasyHandle, CURLOPT_HTTPHEADER, HeaderList);
		}

		// Set connection timeout in seconds
		int32 HttpConnectionTimeout = FHttpModule::Get().GetHttpConnectionTimeout();
		if (HttpConnectionTimeout >= 0)
		{
			curl_easy_setopt(EasyHandle, CURLOPT_CONNECTTIMEOUT, HttpConnectionTimeout);
		}

		if (FCurlHttpManager::CurlRequestOptions.bAllowSeekFunction && bIsRequestPayloadSeekable)
		{
			curl_easy_setopt(EasyHandle, CURLOPT_SEEKDATA, this);
			curl_easy_setopt(EasyHandle, CURLOPT_SEEKFUNCTION, StaticSeekCallback);
		}
	}

#if !WITH_CURL_XCURL
	{
		//Tracking the locking in the CURLOPT_SHARE branch of the curl_easy_setopt implementation
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FCurlHttpRequest_SetupRequest_EASY_CURLOPT_SHARE);

		curl_easy_setopt(EasyHandle, CURLOPT_SHARE, FCurlHttpManager::GShareHandle);
	}
#endif

	UE_LOG(LogHttp, Log, TEXT("%p: Starting %s request to URL='%s'"), this, *Verb, *URL);

	// Response object to handle data that comes back after starting this request
	Response = MakeShared<FCurlHttpResponse, ESPMode::ThreadSafe>(*this);

	return true;
}

bool FCurlHttpRequest::ProcessRequest()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FCurlHttpRequest_ProcessRequest);
	check(EasyHandle);

	// Clear out response. If this is a re-used request, Response could point to a stale response until SetupRequestHttpThread is called
	Response = nullptr;

	bool bStarted = false;
	if (!FHttpModule::Get().GetHttpManager().IsDomainAllowed(URL))
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. URL '%s' is not using an allowed domain. %p"), *URL, this);
	}
	else if (!SetupRequest())
	{
		UE_LOG(LogHttp, Warning, TEXT("Could not perform game thread setup, processing HTTP request failed. Increase verbosity for additional information."));
	}
	else
	{
		// Clear the info cache log so we don't output messages from previous requests when reusing/retrying a request
		const FScopeLock CacheLock(&InfoMessageCacheCriticalSection);
		for (FString& Line : InfoMessageCache)
		{
			Line.Reset();
		}

		bStarted = true;
	}

	if (!bStarted)
	{
		if (!IsInGameThread())
		{
			// Always finish on the game thread
			FHttpModule::Get().GetHttpManager().AddGameThreadTask([StrongThis = StaticCastSharedRef<FCurlHttpRequest>(AsShared())]()
			{
				StrongThis->FinishedRequest();
			});
			return true;
		}
		else
		{
			// Cleanup and call delegate
			FinishedRequest();
		}
	}
	else
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_CurlHttpAddThreadedRequest);
		// Mark as in-flight to prevent overlapped requests using the same object
		CompletionStatus = EHttpRequestStatus::Processing;
		// Add to global list while being processed so that the ref counted request does not get deleted
		FHttpModule::Get().GetHttpManager().AddThreadedRequest(SharedThis(this));

		UE_LOG(LogHttp, Verbose, TEXT("%p: request (easy handle:%p) has been added to threaded queue for processing"), this, EasyHandle);
	}

	return bStarted;
}

bool FCurlHttpRequest::StartThreadedRequest()
{
	// reset timeout
	ElapsedTime = 0.0f;
	TimeSinceLastResponse = 0.0f;
	bAnyHttpActivity = false;
	
	UE_LOG(LogHttp, Verbose, TEXT("%p: request (easy handle:%p) has started threaded processing"), this, EasyHandle);

	return true;
}

void FCurlHttpRequest::FinishRequest()
{
	FinishedRequest();
}

bool FCurlHttpRequest::IsThreadedRequestComplete()
{
	if (bCanceled)
	{
		return true;
	}
	
	if (bCurlRequestCompleted && ElapsedTime >= FHttpModule::Get().GetHttpDelayTime())
	{
		return true;
	}

	if (CurlAddToMultiResult != CURLM_OK)
	{
		return true;
	}

	const float HttpTimeout = GetTimeoutOrDefault();
	bool bTimedOut = (HttpTimeout > 0 && TimeSinceLastResponse >= HttpTimeout);
#if CURL_ENABLE_NO_TIMEOUTS_OPTION
	static const bool bNoTimeouts = FParse::Param(FCommandLine::Get(), TEXT("NoTimeouts"));
	bTimedOut = bTimedOut && !bNoTimeouts;
#endif
	if (bTimedOut)
	{
		UE_LOG(LogHttp, Warning, TEXT("%p: HTTP request timed out after %0.2f seconds URL=%s"), this, TimeSinceLastResponse, *GetURL());
		return true;
	}

	return false;
}

void FCurlHttpRequest::TickThreadedRequest(float DeltaSeconds)
{
	ElapsedTime += DeltaSeconds;
	TimeSinceLastResponse += DeltaSeconds;
}

void FCurlHttpRequest::CancelRequest()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FCurlHttpRequest_CancelRequest);

	if (bCanceled)
	{
		return;
	}

	bCanceled = true;
	UE_LOG(LogHttp, Verbose, TEXT("%p: HTTP request canceled.  URL=%s"), this, *GetURL());

	FHttpManager& HttpManager = FHttpModule::Get().GetHttpManager();
	if (HttpManager.IsValidRequest(this))
	{
		HttpManager.CancelThreadedRequest(SharedThis(this));
	}
	else if (!IsInGameThread())
	{
		// Always finish on the game thread
		FHttpModule::Get().GetHttpManager().AddGameThreadTask([StrongThis = StaticCastSharedRef<FCurlHttpRequest>(AsShared())]()
		{
			StrongThis->FinishedRequest();
		});
	}
	else
	{
		// Finish immediately
		FinishedRequest();
	}
}

EHttpRequestStatus::Type FCurlHttpRequest::GetStatus() const
{
	return CompletionStatus;
}

const FHttpResponsePtr FCurlHttpRequest::GetResponse() const
{
	return Response;
}

void FCurlHttpRequest::Tick(float DeltaSeconds)
{
	CheckProgressDelegate();
	BroadcastNewlyReceivedHeaders();
}

void FCurlHttpRequest::CheckProgressDelegate()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FCurlHttpRequest_CheckProgressDelegate);
	const int32 CurrentBytesRead = Response.IsValid() ? Response->TotalBytesRead.GetValue() : 0;
	const int32 CurrentBytesSent = BytesSent.GetValue();

	const bool bProcessing = CompletionStatus == EHttpRequestStatus::Processing;
	const bool bBytesSentChanged = (CurrentBytesSent != LastReportedBytesSent);
	const bool bBytesReceivedChanged = (Response.IsValid() && CurrentBytesRead != LastReportedBytesRead);
	const bool bProgressChanged = bBytesSentChanged || bBytesReceivedChanged;
	if (bProcessing && bProgressChanged)
	{
		LastReportedBytesSent = CurrentBytesSent;
		if (Response.IsValid())
		{
			LastReportedBytesRead = CurrentBytesRead;
		}
		// Update response progress
		OnRequestProgress().ExecuteIfBound(SharedThis(this), LastReportedBytesSent, LastReportedBytesRead);
	}
}

void FCurlHttpRequest::BroadcastNewlyReceivedHeaders()
{
	check(IsInGameThread());
	if (Response.IsValid())
	{
		// Process the headers received on the HTTP thread and merge them into our master list and then broadcast the new headers
		TPair<FString, FString> NewHeader;
		while (Response->NewlyReceivedHeaders.Dequeue(NewHeader))
		{
			const FString& HeaderKey = NewHeader.Key;
			const FString& HeaderValue = NewHeader.Value;

			FString NewValue;
			FString* PreviousValue = Response->Headers.Find(HeaderKey);
			if (PreviousValue != nullptr && !PreviousValue->IsEmpty())
			{
				constexpr const int32 SeparatorLength = 2; // Length of ", "
				NewValue = MoveTemp(*PreviousValue);
				NewValue.Reserve(NewValue.Len() + SeparatorLength + HeaderValue.Len());
				NewValue += TEXT(", ");
			}
			NewValue += HeaderValue;
			Response->Headers.Add(HeaderKey, MoveTemp(NewValue));

			OnHeaderReceived().ExecuteIfBound(SharedThis(this), NewHeader.Key, NewHeader.Value);
		}
	}
}

void FCurlHttpRequest::FinishedRequest()
{
	check(IsInGameThread());
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FCurlHttpRequest_FinishedRequest);

	curl_easy_setopt(EasyHandle, CURLOPT_SHARE, nullptr);
	
	CheckProgressDelegate();
	// if completed, get more info
	if (bCurlRequestCompleted)
	{
		if (Response.IsValid())
		{
			Response->bSucceeded = (CURLE_OK == CurlCompletionResult);

			// get the information
			long HttpCode = 0;
			if (CURLE_OK == curl_easy_getinfo(EasyHandle, CURLINFO_RESPONSE_CODE, &HttpCode))
			{
				Response->HttpCode = HttpCode;
			}

			double ContentLengthDownload = 0.0;
			if (CURLE_OK == curl_easy_getinfo(EasyHandle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &ContentLengthDownload) && ContentLengthDownload >= 0.0)
			{
				Response->ContentLength = static_cast< int32 >(ContentLengthDownload);
			}
			else
			{
				// If curl did not know how much we downloaded, or we were missing a Content-Length header (Chunked request), set our ContentLength as the amount we downloaded
				Response->ContentLength = Response->TotalBytesRead.GetValue();
			}

			if (Response->HttpCode <= 0 && URL.StartsWith(TEXT("Http"), ESearchCase::IgnoreCase))
			{
				UE_LOG(LogHttp, Warning, TEXT("%p: invalid HTTP response code received. URL: %s, HTTP code: %d, content length: %d, actual payload size: %d"),
					this, *GetURL(), Response->HttpCode, Response->ContentLength, Response->Payload.Num());
				Response->bSucceeded = false;
			}
		}
	}
	
	// if just finished, mark as stopped async processing
	if (Response.IsValid())
	{
		BroadcastNewlyReceivedHeaders();
		Response->bIsReady = true;
	}

	if (Response.IsValid() &&
		Response->bSucceeded)
	{
		const bool bDebugServerResponse = Response->GetResponseCode() >= 500 && Response->GetResponseCode() <= 503;

		// log info about error responses to identify failed downloads
		if (UE_LOG_ACTIVE(LogHttp, Verbose) ||
			bDebugServerResponse)
		{
			if (bDebugServerResponse)
			{
				UE_LOG(LogHttp, Warning, TEXT("%p: request has been successfully processed. URL: %s, HTTP code: %d, content length: %d, actual payload size: %d, elapsed: %.2fs"),
					this, *GetURL(), Response->HttpCode, Response->ContentLength, Response->Payload.Num(), ElapsedTime);
			}
			else
			{
				UE_LOG(LogHttp, Log, TEXT("%p: request has been successfully processed. URL: %s, HTTP code: %d, content length: %d, actual payload size: %d, elapsed: %.2fs"),
					this, *GetURL(), Response->HttpCode, Response->ContentLength, Response->Payload.Num(), ElapsedTime);
			}

			TArray<FString> AllHeaders = Response->GetAllHeaders();
			for (TArray<FString>::TConstIterator It(AllHeaders); It; ++It)
			{
				const FString& HeaderStr = *It;
				if (!HeaderStr.StartsWith(TEXT("Authorization")) && !HeaderStr.StartsWith(TEXT("Set-Cookie")))
				{
					if (bDebugServerResponse)
					{
						UE_LOG(LogHttp, Warning, TEXT("%p Response Header %s"), this, *HeaderStr);
					}
					else
					{
						UE_LOG(LogHttp, Verbose, TEXT("%p Response Header %s"), this, *HeaderStr);
					}
				}
			}
		}

		// Mark last request attempt as completed successfully
		CompletionStatus = EHttpRequestStatus::Succeeded;
		// Broadcast any headers we haven't broadcast yet
		BroadcastNewlyReceivedHeaders();
		// Call delegate with valid request/response objects
		OnProcessRequestComplete().ExecuteIfBound(SharedThis(this),Response,true);
	}
	else
	{
		if (bCanceled)
		{
			UE_LOG(LogHttp, Warning, TEXT("%p: request was cancelled"), this);
		}
		else if (CurlAddToMultiResult != CURLM_OK)
		{
			UE_LOG(LogHttp, Warning, TEXT("%p: request failed, libcurl multi error: %d (%s)"), this, (int32)CurlAddToMultiResult, ANSI_TO_TCHAR(curl_multi_strerror(CurlAddToMultiResult)));
		}
		else
		{
			UE_LOG(LogHttp, Warning, TEXT("%p: request failed, libcurl error: %d (%s)"), this, (int32)CurlCompletionResult, ANSI_TO_TCHAR(curl_easy_strerror(CurlCompletionResult)));
		}

		if (!bCanceled)
		{
			const FScopeLock CacheLock(&InfoMessageCacheCriticalSection);
			for (int32 i = 0; i < InfoMessageCache.Num(); ++i)
			{
				if (InfoMessageCache[(LeastRecentlyCachedInfoMessageIndex + i) % InfoMessageCache.Num()].Len() > 0)
				{
					UE_LOG(LogHttp, Warning, TEXT("%p: libcurl info message cache %d (%s)"), this, (LeastRecentlyCachedInfoMessageIndex + i) % InfoMessageCache.Num(), *(InfoMessageCache[(LeastRecentlyCachedInfoMessageIndex + i) % NumberOfInfoMessagesToCache]));
				}
			}
		}

		// Mark last request attempt as completed but failed
		if (bCanceled)
		{
			CompletionStatus = EHttpRequestStatus::Failed;
		}
		else if (bCurlRequestCompleted)
		{
			switch (CurlCompletionResult)
			{
			case CURLE_COULDNT_CONNECT:
			case CURLE_COULDNT_RESOLVE_PROXY:
			case CURLE_COULDNT_RESOLVE_HOST:
			case CURLE_SSL_CONNECT_ERROR:
				// report these as connection errors (safe to retry)
				CompletionStatus = EHttpRequestStatus::Failed_ConnectionError;
				break;
			default:
				CompletionStatus = EHttpRequestStatus::Failed;
			}
		}
		else
		{
			if (bAnyHttpActivity)
			{
				CompletionStatus = EHttpRequestStatus::Failed;
			}
			else
			{
				CompletionStatus = EHttpRequestStatus::Failed_ConnectionError;
			}
		}
		// Call delegate with failure
		OnProcessRequestComplete().ExecuteIfBound(SharedThis(this), Response, false);

		//Delegate needs to know about the errors -- so clear out Response (since connection failed) afterwards...
		Response = nullptr;
	}
}

float FCurlHttpRequest::GetElapsedTime() const
{
	return ElapsedTime;
}


// FCurlHttpRequest

FCurlHttpResponse::FCurlHttpResponse(FCurlHttpRequest& InRequest)
	:	Request(InRequest)
	,	TotalBytesRead(0)
	,	HttpCode(EHttpResponseCodes::Unknown)
	,	ContentLength(0)
	,	bIsReady(0)
	,	bSucceeded(0)
{
}

FCurlHttpResponse::~FCurlHttpResponse()
{	
}

FString FCurlHttpResponse::GetURL() const
{
	return Request.GetURL();
}

FString FCurlHttpResponse::GetURLParameter(const FString& ParameterName) const
{
	return Request.GetURLParameter(ParameterName);
}

FString FCurlHttpResponse::GetHeader(const FString& HeaderName) const
{
	FString Result;
	if (!bIsReady)
	{
		UE_LOG(LogHttp, Warning, TEXT("Can't get cached header [%s]. Response still processing. %p"),
			*HeaderName, &Request);
	}
	else
	{
		const FString* Header = Headers.Find(HeaderName);
		if (Header != NULL)
		{
			Result = *Header;
		}
	}
	return Result;
}

TArray<FString> FCurlHttpResponse::GetAllHeaders() const
{
	TArray<FString> Result;
	if (!bIsReady)
	{
		UE_LOG(LogHttp, Warning, TEXT("Can't get cached headers. Response still processing. %p"),&Request);
	}
	else
	{
		Result.Reserve(Headers.Num());
		for (const TPair<FString, FString>& It : Headers)
		{
			Result.Emplace(FCurlHttpRequest::CombineHeaderKeyValue(It.Key, It.Value));
		}
	}
	return Result;
}

FString FCurlHttpResponse::GetContentType() const
{
	return GetHeader(TEXT("Content-Type"));
}

int32 FCurlHttpResponse::GetContentLength() const
{
	return ContentLength;
}

const TArray<uint8>& FCurlHttpResponse::GetContent() const
{
	if (!bIsReady)
	{
		UE_LOG(LogHttp, Warning, TEXT("Payload is incomplete. Response still processing. %p"),&Request);
	}
	return Payload;
}

int32 FCurlHttpResponse::GetResponseCode() const
{
	return HttpCode;
}

FString FCurlHttpResponse::GetContentAsString() const
{
	// Content is NOT null-terminated; we need to specify lengths here
	FUTF8ToTCHAR TCHARData(reinterpret_cast<const ANSICHAR*>(Payload.GetData()), Payload.Num());
	return FString(TCHARData.Length(), TCHARData.Get());
}

#endif //WITH_CURL
