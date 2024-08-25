// Copyright Epic Games, Inc. All Rights Reserved.

#include "HttpConnectionResponseWriteContext.h"
#include "HttpServerResponse.h"
#include "HttpServerHttpVersion.h"
#include "HttpServerConstantsPrivate.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

DEFINE_LOG_CATEGORY(LogHttpConnectionResponseWriteContext);

FHttpConnectionResponseWriteContext::FHttpConnectionResponseWriteContext(FSocket* InSocket)
	: Socket(InSocket)
{
}

void FHttpConnectionResponseWriteContext::ResetContext(TUniquePtr<FHttpServerResponse>&& Resp)
{
	Response = MoveTemp(Resp);
	ElapsedIdleTime = 0.0f;
	ErrorBuilder.Empty();

	HeaderBytesWritten = 0;
	BodyBytesWritten = 0;
	HeaderBytes.Empty();

	if (Response)
	{
		// Affix Content-Length Header
		TArray<FString> ContentLengthValue = { FString::FromInt(Response->Body.Num()) };
		Response->Headers.Add(UE_HTTP_SERVER_HEADER_KEYS_CONTENT_LENGTH, MoveTemp(ContentLengthValue));

		// Serialize Headers
		HeaderBytes.Append(SerializeHeadersUtf8(Response->HttpVersion, Response->Code, Response->Headers));
	}
}

EHttpConnectionContextState FHttpConnectionResponseWriteContext::WriteStream(float DeltaTime)
{
	ElapsedIdleTime += DeltaTime;

	int32 BytesWritten = 0;

	if (!IsWriteHeaderComplete())
	{
		const uint8* DataOffset = HeaderBytes.GetData() + HeaderBytesWritten;
		int32 DataLen = HeaderBytes.Num() - HeaderBytesWritten;
		if (!WriteBytes(DataOffset, DataLen, BytesWritten))
		{
			AddError(UE_HTTP_SERVER_ERROR_STR_SOCKET_SEND_FAILURE);
			return EHttpConnectionContextState::Error;
		}
		HeaderBytesWritten += BytesWritten;
	}

	if (IsWriteHeaderComplete() && !IsWriteBodyComplete())
	{
		const uint8* DataOffset = Response->Body.GetData() + BodyBytesWritten;
		int32 DataLen = Response->Body.Num() - BodyBytesWritten;
		if (!WriteBytes(DataOffset, DataLen, BytesWritten))
		{
			AddError(UE_HTTP_SERVER_ERROR_STR_SOCKET_SEND_FAILURE);
			return EHttpConnectionContextState::Error;
		}
		BodyBytesWritten += BytesWritten;
	}

	if (IsWriteHeaderComplete() && IsWriteBodyComplete())
	{
		ensure(BodyBytesWritten+HeaderBytesWritten == HeaderBytes.Num()+Response->Body.Num());
		return EHttpConnectionContextState::Done;
	}

	return EHttpConnectionContextState::Continue;
}

bool FHttpConnectionResponseWriteContext::WriteBytes(const uint8* Bytes, int32 BytesLen, int32 &OutBytesWritten)
{
	OutBytesWritten = 0;
	bool bWriteSuccess = Socket->Send(Bytes, BytesLen, OutBytesWritten);
	if (!bWriteSuccess)
	{
		ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		const ESocketErrors Err = SocketSubsystem->GetLastErrorCode();
		if (Err == SE_EWOULDBLOCK || Err == SE_TRY_AGAIN)
		{
			bWriteSuccess = true;
			OutBytesWritten = 0;
		}
		else
		{
			UE_LOG(LogHttpConnectionResponseWriteContext, Warning, TEXT("WriteBytes sent %d/%d bytes"), OutBytesWritten, BytesLen);
		}
	}

	if (OutBytesWritten > 0)
	{
		UE_LOG(LogHttpConnectionResponseWriteContext, Verbose,
			TEXT("ElapsedIdleTime\t %f"), ElapsedIdleTime);
		ElapsedIdleTime = 0.0f;
	}

	return bWriteSuccess;
}

bool FHttpConnectionResponseWriteContext::IsWriteHeaderComplete() const
{
	return HeaderBytesWritten >= HeaderBytes.Num();
}

bool FHttpConnectionResponseWriteContext::IsWriteBodyComplete() const
{
	check(Response);
	return BodyBytesWritten >= Response->Body.Num();
}

TArray<uint8> FHttpConnectionResponseWriteContext::SerializeHeadersUtf8(HttpVersion::EHttpServerHttpVersion HttpVersion, EHttpServerResponseCodes ResponseCode, const TMap<FString, TArray<FString>>& HeadersMap)
{
	FString ResponseHeaderStr = FString::Printf(TEXT("%s %d\r\n"),  
		*HttpVersion::ToString(HttpVersion), ResponseCode);

	for (const auto& KeyValuePair : HeadersMap)
	{
		const FString& HeaderKey = KeyValuePair.Key;
		const TArray<FString>& HeaderValues = KeyValuePair.Value;
		for (const auto& HeaderValue : HeaderValues)
		{
			const FString HeaderItem = FString::Printf(TEXT("%s: %s\r\n"), *HeaderKey, *HeaderValue);
			ResponseHeaderStr.Append(HeaderItem);
		}
	}
	ResponseHeaderStr.Append(TEXT("\r\n"));

	TArray<uint8> HeaderRawBytes;
	FTCHARToUTF8 HeaderUtf8(*ResponseHeaderStr);
	const uint8* HeaderUtf8Bytes = reinterpret_cast<const uint8*>(HeaderUtf8.Get());
	HeaderRawBytes.Append(HeaderUtf8Bytes, HeaderUtf8.Length());
	return HeaderRawBytes;
}

