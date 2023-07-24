// Copyright Epic Games, Inc. All Rights Reserved.

#include "HttpIXMLSupport.h"

#if PLATFORM_HOLOLENS

#include "HttpIXML.h"
#include <string>
#include <sstream>

#include "Microsoft/AllowMicrosoftPlatformTypes.h"

// --------------------------------------------------------------------------------------
// Name: HttpCallback::~HttpCallback
// Desc: Destructor
// --------------------------------------------------------------------------------------
HttpCallback::~HttpCallback()
{
	// remove the completion EVENT object.
	if ( CompleteSignal )
	{
		::CloseHandle(CompleteSignal);
	}
}

// --------------------------------------------------------------------------------------
// Name: HttpCallback::RuntimeClassInitialize
// Desc: Used by WRL to instance the COM object
// --------------------------------------------------------------------------------------
STDMETHODIMP HttpCallback::RuntimeClassInitialize()
{
	// Create the "complete" event
	HANDLE EventHandle = ::CreateEventEx(	nullptr,                     // security attributes
											nullptr,                     // name of the event
											CREATE_EVENT_MANUAL_RESET,   // flags (starts nonsignaled, manual ResetEvent())
											EVENT_ALL_ACCESS );          // desired access
	if ( EventHandle == nullptr )
	{
		return HRESULT_FROM_WIN32( GetLastError() );
	}

	CompleteSignal = EventHandle;
	return S_OK;
}

// --------------------------------------------------------------------------------------
// Name: HttpCallback::OnRedirect
//
// Desc: The requested URI was redirected by the HTTP server to a new URI.
//
// Arguments:
//     XHR         - The interface pointer of originating IXMLHTTPRequest2 object.
//     pRedirectUrl - The new URL to for the request.
// --------------------------------------------------------------------------------------
IFACEMETHODIMP HttpCallback::OnRedirect( IXMLHTTPRequest2* XHR, const wchar_t* RedirectUrl )
{
	UNREFERENCED_PARAMETER(XHR);
	UNREFERENCED_PARAMETER(RedirectUrl);

	// If the URI was redirected by the HTTP server to a new URI, do nothing.

	return S_OK;
};

// --------------------------------------------------------------------------------------
// Name: HttpCallback::OnHeadersAvailable
//
// Desc: The HTTP Headers have been downloaded and are ready for parsing. The string that is
//       returned is owned by this function and should be copied or deleted before exit.
//
// Arguments:
//     XHR       - The interface pointer of originating IXMLHTTPRequest2 object.
//     Status	  - The value of HTTP status code, e.g. 200, 404
//     pwszStatus - The description text of HTTP status code.
// --------------------------------------------------------------------------------------
IFACEMETHODIMP HttpCallback::OnHeadersAvailable( IXMLHTTPRequest2* XHR, DWORD Status, const wchar_t *StatusString )
{
	UNREFERENCED_PARAMETER(StatusString);
	HRESULT hr = S_OK;

	// We need a pointer to the originating HttpRequest object, otherwise this
	// makes no sense.
	if ( XHR == NULL )
	{
		return E_INVALIDARG;
	}

	// Get all response headers. We could equally select a single header using:
	//
	//     hr = XHR->GetResponseHeader(L"Content-Length", &pwszContentLength);
	//
	wchar_t* headers = nullptr;
	hr = XHR->GetAllResponseHeaders( &headers );
	if ( SUCCEEDED( hr ) )
	{
		// take a copy of the header data to the local wstring.
		AllHeaders += headers;
		hr = S_OK;
	}

	// The header string that was passed in needs to be deleted here.
	if ( headers != nullptr )
	{
		::CoTaskMemFree( headers );
		headers = nullptr;
	}

	// copy the http status for later use.
	HttpStatus = Status;

	return hr;
}

// --------------------------------------------------------------------------------------
// Name: HttpCallback::OnDataAvailable
//
// Desc: Part of the HTTP Data payload is available, we can start processing it
//       here or copy it off and wait for the whole request to finish loading.
//
// Arguments:
//    XHR            - Pointer to the originating IXMLHTTPRequest2 object.
//    ResponseStream - Pointer to the input stream, which may only be part of the
//                      whole stream.
// --------------------------------------------------------------------------------------
IFACEMETHODIMP HttpCallback::OnDataAvailable( IXMLHTTPRequest2 *XHR, ISequentialStream *ResponseStream )
{
	UNREFERENCED_PARAMETER(XHR);

	// add the contents of the stream to our running result.
	LastResult = ReadDataFromStream(ResponseStream);

	return LastResult;
}

// --------------------------------------------------------------------------------------
// Name: HttpCallback::OnResponseReceived
//
// Desc: Called when the entire body has been received.
//       At this point the application can begin processing the data by calling
//       ISequentialStream::Read on the ResponseStream or store a reference to
//       the ISequentialStream for later processing.
//
// Arguments:
//    XHR            - Pointer to the originating IXMLHTTPRequest2 object.
//    ResponseStream - Pointer to the complete input stream.
// --------------------------------------------------------------------------------------
IFACEMETHODIMP HttpCallback::OnResponseReceived( IXMLHTTPRequest2 *XHR, ISequentialStream *ResponseStream )
{
	UNREFERENCED_PARAMETER(XHR);
	UNREFERENCED_PARAMETER(ResponseStream);

	//. Shrink the content to the actual amount read
	ContentData.SetNum( ContentRead );

	// set the completion event to "triggered".
	SetEvent(CompleteSignal);

	return LastResult;
}

// --------------------------------------------------------------------------------------
// Name: HttpCallback::OnError
// Desc: Handle errors that have occurred during the HTTP request.
// Arguments:
//    XHR - The interface pointer of IXMLHTTPRequest2 object.
//    InError - The errocode for the httprequest.
// --------------------------------------------------------------------------------------
IFACEMETHODIMP HttpCallback::OnError( IXMLHTTPRequest2 *XHR, HRESULT InError )
{
	UNREFERENCED_PARAMETER(XHR);

	// The Request is complete, but broken.
	SetEvent(CompleteSignal);
	LastResult = InError;

	//. Patch HTTP status code to be something useful, based on error code
	switch ( InError )
	{
		case E_ACCESSDENIED:	HttpStatus = EHttpResponseCodes::Denied;	break;

		default:	break;
	}

	return S_OK;
}

// --------------------------------------------------------------------------------------
// Name: HttpCallback::ReadFromStream
// Desc: Demonstrate how to read from the HTTP response stream.
// Arguments:
//    Stream - the data stream read form the http response.
// --------------------------------------------------------------------------------------
HRESULT HttpCallback::ReadDataFromStream( ISequentialStream *Stream )
{
	if (Stream == NULL)
	{
		return E_INVALIDARG;
	}

	ULONG bytesRead = 0;

	do
	{
		//. TODO can we determine size upfront?
		ContentData.SetNum( ContentRead + MAX_HTTP_BUFFER_SIZE );

		//. Read directly into content 
		HRESULT hr = Stream->Read( &ContentData[ContentRead], 
									MAX_HTTP_BUFFER_SIZE - 1, 
									&bytesRead );

		//. Exit if we are done reading, or on error
		if ( FAILED(hr) || bytesRead == 0 )
		{
			return hr;
		}

		ContentRead += bytesRead;

	} while ( true );

	//. Should never be reached
	return E_FAIL;
}

// --------------------------------------------------------------------------------------
// Name: HttpCallback::IsFinished
// Desc: Non-blocking test for completion of the HTTP request.
// --------------------------------------------------------------------------------------
BOOL HttpCallback::IsFinished()
{
	// execute a non-blocking wait of zero time.
	uint32 Error = ::WaitForSingleObject( CompleteSignal, 0 );

	if ( Error == WAIT_FAILED )
	{
		LastResult = HRESULT_FROM_WIN32( GetLastError() );
		return FALSE;
	}
	else if ( Error != WAIT_OBJECT_0 )
	{
		// every other state including WAIT_TIMEOUT is a false result.
		return FALSE;
	}

	// Event was signalled, success.
	return TRUE;
}

// --------------------------------------------------------------------------------------
// Name: HttpCallback::WaitForFinish
// Desc: Blocking wait for completion of the HTTP request.
// --------------------------------------------------------------------------------------
BOOL HttpCallback::WaitForFinish()
{
	// execute a blocking wait on the completion Event.
	uint32 Error = ::WaitForSingleObject( CompleteSignal, INFINITE );
	if ( Error == WAIT_FAILED )
	{
		LastResult = HRESULT_FROM_WIN32( GetLastError() );
		return FALSE;
	}
	else if ( Error != WAIT_OBJECT_0 )
	{
		// every other state including WAIT_TIMEOUT is a false result.
		return FALSE;
	}

	return TRUE;
}

//--------------------------------------------------------------------------------------
// Name: RequestStream::RequestStream
// Desc: Constructor
//--------------------------------------------------------------------------------------
RequestStream::RequestStream()
	: RefCount(1)
	, StreamBuffer(nullptr)
{
}

//--------------------------------------------------------------------------------------
// Name: RequestStream::~RequestStream
// Desc: Destructor
//--------------------------------------------------------------------------------------
RequestStream::~RequestStream()
{
	delete StreamBuffer;
}

//--------------------------------------------------------------------------------------
//  Name: RequestStream::Open
//  Desc: Opens the buffer populated with the supplied data
//--------------------------------------------------------------------------------------
STDMETHODIMP RequestStream::Open( LPCSTR InBuffer, ULONG BufferSize )
{
	HRESULT hr = S_OK;

	if ( InBuffer == nullptr )
	{
		return E_INVALIDARG;
	}

	BuffSize = BufferSize;
	BuffIndex = 0;

	// Create a buffer to store a copy of the request (and include space for the null 
	// terminator, as generally this method can accept the result of strlen() for 
	// BufferSize). This buffer is deleted in the destructor.
	StreamBuffer = new char[ BufferSize + 1 ];
	if ( StreamBuffer == nullptr )
	{
		return E_OUTOFMEMORY;
	}

	memcpy_s( StreamBuffer, BuffSize + 1, InBuffer, BuffSize );

	return hr;
}

//--------------------------------------------------------------------------------------
//  Name: RequestStream::Size
//  Desc: Returns the size of the buffer
//--------------------------------------------------------------------------------------
STDMETHODIMP_(ULONGLONG) RequestStream::Size()
{
	return BuffSize;
}

//--------------------------------------------------------------------------------------
//  Name: RequestStream::Read
//  Desc: ISequentialStream overload: Reads data from the buffer
//--------------------------------------------------------------------------------------
STDMETHODIMP RequestStream::Read( void *Data, ULONG ByteCount, ULONG *NumReadBytes )
{
	HRESULT hr = S_OK;

	if ( Data == nullptr )
	{
		return E_INVALIDARG;
	}

	BYTE*		pbOutput = reinterpret_cast<BYTE*>( Data );
	const BYTE* pbInput  = reinterpret_cast<BYTE*>( &StreamBuffer[BuffIndex] );

	ULONG bytes_to_copy  = ByteCount;
	ULONG max_available  = BuffSize - BuffIndex;

	if ( bytes_to_copy > max_available )
	{
		bytes_to_copy = max_available;
		hr = S_FALSE;
	}

	if ( bytes_to_copy > 0 )
	{
		memcpy_s( pbOutput, ByteCount, pbInput, bytes_to_copy );
		BuffIndex += bytes_to_copy;
	}

	*NumReadBytes = bytes_to_copy;

	return hr;
}

//--------------------------------------------------------------------------------------
//  Name: RequestStream::Write
//  Desc: ISequentialStream overload: Writes to the buffer. Not implmented, as the buffer is "read only"
//--------------------------------------------------------------------------------------
STDMETHODIMP RequestStream::Write( const void *Data, ULONG ByteCount, ULONG *Written )
{
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(ByteCount);
	UNREFERENCED_PARAMETER(Written);

	return E_NOTIMPL;
}

//--------------------------------------------------------------------------------------
//  Name: RequestStream::QueryInterface
//  Desc: IUnknown overload: Queries for a particular interface
//--------------------------------------------------------------------------------------
STDMETHODIMP RequestStream::QueryInterface( REFIID InRIID, void **OutObj )
{
	if ( OutObj == nullptr )
	{ 
		return E_INVALIDARG;
	}

	*OutObj = nullptr;

	HRESULT hr = S_OK;
	void *Object = nullptr;

	if (InRIID == IID_IUnknown)
	{
		Object = static_cast<IUnknown *>((IDispatch*)this);
	}
	else if (InRIID == IID_IDispatch)
	{
		Object = static_cast<IDispatch *>(this);
	}
	else if (InRIID == IID_ISequentialStream)
	{
		Object = static_cast<ISequentialStream *>(this);
	}
	else 
	{
		return E_NOINTERFACE;
	}

	AddRef();

	*OutObj = Object;
	Object = nullptr;

	return hr;
} 

//--------------------------------------------------------------------------------------
//  Name: RequestStream::AddRef
//  Desc: IUnknown: Increments the reference count
//--------------------------------------------------------------------------------------
STDMETHODIMP_(ULONG) RequestStream::AddRef()
{ 
	return ::_InterlockedIncrement(&RefCount);
} 

//--------------------------------------------------------------------------------------
//  Name: RequestStream::Release
//  Desc: IUnknown overload: Decrements the reference count, possibly deletes the instance
//--------------------------------------------------------------------------------------
STDMETHODIMP_(ULONG) RequestStream::Release()
{
	ULONG ulRefCount = ::_InterlockedDecrement( &RefCount );

	if ( 0 == ulRefCount )
	{
		delete this;
	}

	return ulRefCount;
}

//--------------------------------------------------------------------------------------
//  Name: RequestStream::GetTypeInfoCount
//  Desc: IDispatch overload: IXMLHTTPRequest2 expects a complete IDispatch interface,
//  but doesn't actually make use of this.
//--------------------------------------------------------------------------------------
HRESULT RequestStream::GetTypeInfoCount( uint32* Info )
{
	if (Info)
	{
		*Info = 0;
	}

	return E_NOTIMPL;
}

//--------------------------------------------------------------------------------------
//  Name: RequestStream::GetTypeInfo
//  Desc: IDispatch overload: IXMLHTTPRequest2 expects a complete IDispatch interface,
//  but doesn't actually make use of this.
//--------------------------------------------------------------------------------------
HRESULT RequestStream::GetTypeInfo( uint32 Info, LCID  InLCID, ITypeInfo** TInfo)
{
	if (TInfo)
	{
		*TInfo = nullptr;
	}

	UNREFERENCED_PARAMETER(InLCID);
	UNREFERENCED_PARAMETER(TInfo);

	return E_NOTIMPL;
}

//--------------------------------------------------------------------------------------
//  Name: RequestStream::GetIDsOfNames
//  Desc: IDispatch overload: IXMLHTTPRequest2 expects a complete IDispatch interface,
//  but doesn't actually make use of this.
//--------------------------------------------------------------------------------------
HRESULT RequestStream::GetIDsOfNames( REFIID InRIID, OLECHAR** Names, 
									 uint32 NameCount, LCID InLCID, DISPID* DispId)
{
	UNREFERENCED_PARAMETER(InRIID);
	UNREFERENCED_PARAMETER(Names);
	UNREFERENCED_PARAMETER(NameCount);
	UNREFERENCED_PARAMETER(InLCID);
	UNREFERENCED_PARAMETER(DispId);

	return DISP_E_UNKNOWNNAME;
}

//--------------------------------------------------------------------------------------
//  Name: RequestStream::Invoke
//  Desc: IDispatch overload: IXMLHTTPRequest2 expects a complete IDispatch interface,
//  but doesn't actually make use of this.
//--------------------------------------------------------------------------------------
HRESULT RequestStream::Invoke( 
	DISPID DispIdMember, REFIID InRIID, LCID InLCID, WORD InFlags,
	DISPPARAMS* DispParams, VARIANT* VarResult, EXCEPINFO* ExcepInfo, 
	uint32* ArgErr)
{
	UNREFERENCED_PARAMETER(DispIdMember);
	UNREFERENCED_PARAMETER(InRIID);
	UNREFERENCED_PARAMETER(InLCID);
	UNREFERENCED_PARAMETER(InFlags);
	UNREFERENCED_PARAMETER(DispParams);
	UNREFERENCED_PARAMETER(VarResult);
	UNREFERENCED_PARAMETER(ExcepInfo);
	UNREFERENCED_PARAMETER(ArgErr);

	return S_OK;
}

// --------------------------------------------------------------------------------------

#include "Microsoft/HideMicrosoftPlatformTypes.h"

#endif
