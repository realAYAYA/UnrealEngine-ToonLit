// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !defined(NO_UE_INCLUDES)
#include "Containers/Array.h"
#include "Containers/StringView.h"
#include "HAL/CriticalSection.h"
#include "Memory/MemoryView.h"
#endif

#include <atomic>

#if !defined(COREHTTP_API)
#	define COREHTTP_API IOSTOREONDEMAND_API
#endif

////////////////////////////////////////////////////////////////////////////////
class FIoBuffer;

namespace UE::HTTP
{

////////////////////////////////////////////////////////////////////////////////
enum class EMimeType
{
	Unknown = 0,
	Text,
	Binary,
	Json,
	Xml,
	/* UE_CUSTOM_MIME_TYPES
	CbObject,
	CbPackage,
	CompressedBuffer,
	*/
	Count
};

////////////////////////////////////////////////////////////////////////////////
enum class EStatusCodeClass
{
	Informational,
	Successful,
	Redirection,
	ClientError,
	ServerError,
	Unknown,
};

////////////////////////////////////////////////////////////////////////////////
using	FTicket = uint64;
struct	FActivity;

////////////////////////////////////////////////////////////////////////////////
class COREHTTP_API FConnectionPool
{
public:
	struct FParams
	{
		int32				SetHostFromUrl(FAnsiStringView Url);
		uint32				ConnectionCount;
		struct {
			FAnsiStringView	Name;
			uint32			Port = 80;
			/* uint8		Protocol */;
		}					Host;
		/*
		enum class ProxyType { Http, Socks4 };
		Proxy = { ip, port, type }
		 */
	};

							FConnectionPool() = default;
							FConnectionPool(const FParams& Params);
							~FConnectionPool();
							FConnectionPool(FConnectionPool&& Rhs)	{ *this = MoveTemp(Rhs); }
	FConnectionPool&		operator = (FConnectionPool&& Rhs)		{ Swap(Ptr, Rhs.Ptr); return *this; }

private:
	friend					class FEventLoop;
	class FSocketPool*		Ptr = nullptr;

private:
							FConnectionPool(const FConnectionPool&) = delete;
	FConnectionPool&		operator = (const FConnectionPool&) = delete;
};

////////////////////////////////////////////////////////////////////////////////
class COREHTTP_API FRequest
{
public:
						~FRequest();
						FRequest(FRequest&& Rhs);
	bool				IsValid() const { return Ptr != nullptr; }
	FRequest&&			Accept(EMimeType MimeType);
	FRequest&&			Accept(FAnsiStringView MimeType);
	FRequest&&			Header(FAnsiStringView Key, FAnsiStringView Value);
	void				Content(const void* Data, SIZE_T Size, EMimeType MimeType);
	void				Content(const void* Data, SIZE_T Size, FAnsiStringView MimeType);

private:
	friend				class FEventLoop;
						FRequest() = default;
	FActivity*			Ptr = nullptr;

private:
						FRequest(const FRequest&) = delete;
	FRequest&			operator = (const FRequest&) = delete;
	FRequest&			operator = (FRequest&&) = delete;
};

////////////////////////////////////////////////////////////////////////////////
class COREHTTP_API FResponse
{
public:
	EStatusCodeClass	GetStatus() const;
	uint32				GetStatusCode() const;
	FAnsiStringView 	GetStatusMessage() const;
	int64				GetContentLength() const;
	EMimeType			GetContentType() const;
	void				GetContentType(FAnsiStringView& Out) const;
	FAnsiStringView 	GetHeader(FAnsiStringView Name) const;
	void				SetDestination(FIoBuffer* Buffer);

private:
						FResponse() = delete;
						FResponse(const FResponse&) = delete;
						FResponse(FResponse&&) = delete;
	FResponse&			operator = (const FResponse&) = delete;
	FResponse&			operator = (FResponse&&) = delete;
};

////////////////////////////////////////////////////////////////////////////////
class COREHTTP_API FTicketStatus
{
public:
	enum class EId : uint8 { Response, Content, Cancelled, Error };
	EId					GetId() const;
	UPTRINT				GetParam() const;
	FTicket				GetTicket() const;
	uint32				GetIndex() const;
	FResponse&			GetResponse() const;		// if GetId() == EId::Response
	const FIoBuffer&	GetContent() const;			// if GetId() == EId::Content
	uint32				GetContentLength() const;	//  |
	const char*			GetErrorReason() const;		// if GetId() == EId::Error

private:
						FTicketStatus() = delete;
						FTicketStatus(const FTicketStatus&) = delete;
						FTicketStatus(FTicketStatus&&) = delete;
	FTicketStatus&		operator = (const FTicketStatus&) = delete;
	FTicketStatus&		operator = (FTicketStatus&&) = delete;
};

using FTicketSink = TFunction<void (const FTicketStatus&)>;

////////////////////////////////////////////////////////////////////////////////
class COREHTTP_API FEventLoop
{
public:
	struct FRequestParams
	{
		uint32	BufferSize	= 256;
	 // uint32	PageSize	= 2 << 10;
	};

	template <typename... T> [[nodiscard]] FRequest Get(T&&... t)  { return Request("GET",  Forward<T&&>(t)...); }
	template <typename... T> [[nodiscard]] FRequest Post(T&&... t) { return Request("POST", Forward<T&&>(t)...); }

							FEventLoop() = default;
							~FEventLoop();
	uint32					Tick(uint32 PollTimeoutMs=0);
	bool					IsIdle() const;
	void					Cancel(FTicket Ticket);
	[[nodiscard]] FRequest	Request(FAnsiStringView Method, FAnsiStringView Url, const FRequestParams* Params=nullptr);
	[[nodiscard]] FRequest	Request(FAnsiStringView Method, FAnsiStringView Path, FConnectionPool& Pool, const FRequestParams* Params=nullptr);
	FTicket					Send(FRequest&& Request, FTicketSink Sink, UPTRINT Param=0);

private:
	FRequest				Request(FAnsiStringView Method, FAnsiStringView Path, FActivity* Activity);
	FCriticalSection		Lock;
	std::atomic<uint64>		FreeSlots		= ~0ull;
	std::atomic<uint64>		Cancels			= 0;
	uint64					PrevFreeSlots	= ~0ull;
	TArray<FActivity*>		Pending;
	TArray<FActivity*>		Active;

private:
							FEventLoop(const FEventLoop&)	= delete;
							FEventLoop(FEventLoop&&)		= delete;
	FEventLoop&				operator = (const FEventLoop&)	= delete;
	FEventLoop&				operator = (FEventLoop&&)		= delete;
};

} // namespace UE::HTTP

/* vim: set noet : */
