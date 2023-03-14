// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"

#if PLATFORM_HOLOLENS


#include "Containers/UnrealString.h"
#include "Http.h"
#include "Microsoft/AllowMicrosoftPlatformTypes.h"
#include <msxml6.h>
#include <wrl.h>
#include <Windows.h>

using namespace Microsoft::WRL;

// default buffer size and HTTP request size
static const int MAX_HTTP_BUFFER_SIZE = 8192;

//------------------------------------------------------------------------------
// Name: HttpCallback
// Desc: Implement the IXMLHTTPRequest2Callback functions with
//       basic error reporting and an Event signaling when the request is
//       complete.
//------------------------------------------------------------------------------
class HttpCallback : public Microsoft::WRL::RuntimeClass<RuntimeClassFlags<ClassicCom>, IXMLHTTPRequest2Callback>
{
public:
	// Required functions
	STDMETHODIMP OnRedirect( IXMLHTTPRequest2* XHR, const WCHAR* RedirectUrl );
	STDMETHODIMP OnHeadersAvailable( IXMLHTTPRequest2* XHR, DWORD Status, const WCHAR *StatusString );
	STDMETHODIMP OnDataAvailable( IXMLHTTPRequest2* XHR, ISequentialStream* ResponseStream );
	STDMETHODIMP OnResponseReceived( IXMLHTTPRequest2* XHR, ISequentialStream* ResponseStream );
	STDMETHODIMP OnError( IXMLHTTPRequest2* XHR, HRESULT InError );

	STDMETHODIMP RuntimeClassInitialize();
	friend HRESULT MakeAndInitialize<HttpCallback,HttpCallback>( HttpCallback ** );

	HttpCallback() : HttpStatus(0), LastResult(S_OK), CompleteSignal(nullptr), AllHeaders(), ContentData(), ContentRead(0) {}
	~HttpCallback();

	//. Accessors
	uint32			GetHttpStatus()		{ return HttpStatus; }
	TArray<uint8>&	GetContent()		{ return ContentData; }
	FString			GetAllHeaders()		{ return AllHeaders; }
	HRESULT			GetErrorCode()		{ return LastResult; }

	// Helper functions
	HRESULT			ReadDataFromStream( ISequentialStream *Stream );
	BOOL			IsFinished();
	BOOL			WaitForFinish();

private:
	uint32			HttpStatus;
	HRESULT			LastResult;
	HANDLE			CompleteSignal;
	FString			AllHeaders;
	TArray<uint8>	ContentData;
	uint64			ContentRead;
};


// ----------------------------------------------------------------------------
// Name: RequestStream
// Desc: Encapsulates a request data stream. It inherits ISequentialStream,
// which the IXMLHTTPRequest2 class uses to read from our buffer. It also 
// inherits IDispatch, which the IXMLHTTPRequest2 interface on Durango requires 
// (unlike on Windows, where only ISequentialStream is necessary).
// ----------------------------------------------------------------------------
class RequestStream : public Microsoft::WRL::RuntimeClass<RuntimeClassFlags<ClassicCom>, ISequentialStream, IDispatch>
{
public:

	RequestStream();
	~RequestStream();

	// ISequentialStream
	STDMETHODIMP Open( LPCSTR Buffer, ULONG BufferSize );
	STDMETHODIMP Read( void *Data, ULONG ByteCount, ULONG *NumReadBytes );
	STDMETHODIMP Write( const void *Data,  ULONG ByteCount, ULONG *Written );

	//Helper
	STDMETHODIMP_(ULONGLONG) Size();

	//IUnknown
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();
	STDMETHODIMP QueryInterface( REFIID InRIID, void **OutObj );

	//IDispatch
	STDMETHODIMP GetTypeInfoCount( uint32 FAR*  Info );
	STDMETHODIMP GetTypeInfo( uint32 Info, LCID InLCID, ITypeInfo FAR* FAR* TInfo );
	STDMETHODIMP GetIDsOfNames( REFIID InRIID, OLECHAR FAR* FAR* Names, uint32 NameCount, LCID InLCID, DISPID FAR* DispId );
	STDMETHODIMP Invoke(
			DISPID			DispIdMember, 
			REFIID			InRIID, 
			LCID			InLCID, 
			WORD			InFlags,
			DISPPARAMS FAR*	DispParams,
			VARIANT FAR*	VarResult,
			EXCEPINFO FAR*	ExcepInfo, 
			uint32 FAR*		ArgErr
		);

protected:
	LONG    RefCount;
	CHAR*   StreamBuffer;
	size_t  BuffSize;
	size_t  BuffIndex;
};


#include "Microsoft/HideMicrosoftPlatformTypes.h"

#endif
