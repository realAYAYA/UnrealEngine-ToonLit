// Copyright Epic Games, Inc. All Rights Reserved.

#include "HttpConnection.h"
#include "HttpConnectionContext.h"
#include "HttpConnectionRequestReadContext.h"
#include "HttpConnectionResponseWriteContext.h"
#include "HttpRouter.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "HttpRequestHandlerIterator.h"
#include "HttpServerConstants.h"
#include "HttpServerConstantsPrivate.h"

#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Containers/Array.h"

DEFINE_LOG_CATEGORY(LogHttpConnection)

FHttpConnection::FHttpConnection(FSocket* InSocket, TSharedPtr<FHttpRouter> InRouter, uint32 InOriginPort, uint32 InConnectionId)
	: Socket(InSocket)
	,Router(InRouter)
	,OriginPort(InOriginPort)
	,ConnectionId(InConnectionId)
	,ReadContext(InSocket)
	,WriteContext(InSocket)
{
	check(nullptr != Socket);
}

FHttpConnection::~FHttpConnection()
{
	check(nullptr == Socket);
}

void FHttpConnection::Tick(float DeltaTime)
{
	const float AwaitReadTimeout = bKeepAlive ?
		ConnectionKeepAliveTimeout : ConnectionTimeout;

	switch (State)
	{
	case EHttpConnectionState::AwaitingRead:
		if (ReadContext.GetElapsedIdleTime() > AwaitReadTimeout || ReadContext.GetSecondsWaitingForReadableSocket() > AwaitReadTimeout)
		{
			Destroy(EConnectionDestroyReason::AwaitReadTimeout);
			return;
		}
		BeginRead(DeltaTime);
		break;

	case EHttpConnectionState::Reading:
		if (ReadContext.GetElapsedIdleTime() > ConnectionTimeout)
		{
			Destroy(EConnectionDestroyReason::ReadTimeout);
			return;
		}
		ContinueRead(DeltaTime);
		break;

	case EHttpConnectionState::AwaitingProcessing:
		break;

	case EHttpConnectionState::Writing:
		if (WriteContext.GetElapsedIdleTime() > ConnectionTimeout)
		{
			Destroy(EConnectionDestroyReason::WriteTimeout);
			return;
		}
		ContinueWrite(DeltaTime);
		break;

	case EHttpConnectionState::Destroyed:
		ensure(false);
		break;
	}
}

void FHttpConnection::ChangeState(EHttpConnectionState NewState)
{
	check(EHttpConnectionState::Destroyed != State);
	check(NewState != State);

	UE_LOG(LogHttpConnection, Verbose,
		TEXT("ChangingState: %d => %d"), State, NewState);
	State = NewState;
}

void FHttpConnection::TransferState(EHttpConnectionState CurrentState, EHttpConnectionState NextState)
{
	check(CurrentState == State);
	check(NextState != CurrentState);
	ChangeState(NextState);
}

void FHttpConnection::BeginRead(float DeltaTime)
{
	const FTimespan WaitTime = FTimespan::FromMilliseconds(1);
	if (!Socket->Wait(ESocketWaitConditions::WaitForRead, WaitTime))
	{
		const float SecondsPollingSocketThisFrame = WaitTime.GetTotalSeconds();
		// We don't add DeltaTime on the first attempt as only 1ms has passed since we started waiting for the socket to be reachable.
		const float SecondsPollingSocketSinceLastFrame = ReadContext.GetSecondsWaitingForReadableSocket() == 0.f ? SecondsPollingSocketThisFrame : DeltaTime + SecondsPollingSocketThisFrame;
		ReadContext.AddSecondsWaitingForReadableSocket(SecondsPollingSocketSinceLastFrame);
	}
	else
	{
		ReadContext.ResetSecondsWaitingForReadableSocket();

		// The socket is reachable, however there may not be data in the pipe
		uint32 PendingDataSize = 0;
		if (Socket->HasPendingData(PendingDataSize))
		{
			TransferState(EHttpConnectionState::AwaitingRead, EHttpConnectionState::Reading);
			LastRequestNumber++;
			ReadContext.ResetContext();
			ContinueRead(DeltaTime);
		}
		else
		{
			ReadContext.AddElapsedIdleTime(DeltaTime);
		}
	}
}

void FHttpConnection::ContinueRead(float DeltaTime)
{
	check(State == EHttpConnectionState::Reading);

	auto ReaderState = ReadContext.ReadStream(DeltaTime);

	switch (ReaderState)
	{
	case EHttpConnectionContextState::Continue:
		break;

	case EHttpConnectionContextState::Done:
		CompleteRead(ReadContext.GetRequest());
		break;

	case EHttpConnectionContextState::Error:
		HandleReadError(ReadContext.GetErrorCode(), *ReadContext.GetErrorStr());
		break;
	}
}

void FHttpConnection::CompleteRead(const TSharedPtr<FHttpServerRequest>& Request)
{
	TArray<FString>* ConnectionHeaders = Request->Headers.Find(FHttpServerHeaderKeys::CONNECTION);
	if (ConnectionHeaders)
	{
		bKeepAlive = ResolveKeepAlive(Request->HttpVersion, *ConnectionHeaders);
	}

	TWeakPtr<FHttpConnection> WeakThisPtr(AsShared());
	FHttpResultCallback OnProcessingComplete = 
		[WeakThisPtr, 
		OriginPortCapture = OriginPort,
		ConnectionIdCapture = ConnectionId, 
		LastRequestNumberCapture = LastRequestNumber,
		ResponseVersionCapture = Request->HttpVersion]
	(TUniquePtr<FHttpServerResponse>&& Response)
	{
		TSharedPtr<FHttpConnection> SharedThisPtr = WeakThisPtr.Pin();
		if (SharedThisPtr.IsValid())
		{
			UE_LOG(LogHttpConnection, Verbose,
				TEXT("EndProcessRequest\t [%d][%u]-%u : %u"), 
				OriginPortCapture, ConnectionIdCapture, LastRequestNumberCapture, Response->Code);

			// Ensure this result callback was called once
			check(EHttpConnectionState::AwaitingProcessing == SharedThisPtr->GetState());

			// Begin response flow
			Response->HttpVersion = ResponseVersionCapture;
			SharedThisPtr->BeginWrite(MoveTemp(Response), LastRequestNumberCapture);
		}
	};

	ProcessRequest(Request, OnProcessingComplete);
}

void FHttpConnection::ProcessRequest(const TSharedPtr<FHttpServerRequest>& Request, const FHttpResultCallback& OnProcessingComplete)
{
	TransferState(EHttpConnectionState::Reading, EHttpConnectionState::AwaitingProcessing);

	UE_LOG(LogHttpConnection, Verbose,
		TEXT("BeginProcessRequest\t [%d][%u]-%u : %s"), 
		OriginPort, ConnectionId, LastRequestNumber, *Request->RelativePath.GetPath());

	bool bRequestHandled = false;
	auto RequestHandlerIterator = Router->CreateRequestHandlerIterator(Request);
	while (const auto& RequestHandlerPtr = RequestHandlerIterator.Next())
	{
		(*RequestHandlerPtr).CheckCallable();
		bRequestHandled = (*RequestHandlerPtr)(*Request, OnProcessingComplete);
		if (bRequestHandled)
		{
			break;
		}
		// If this handler didn't accept, ensure they did not invoke the result callback
		check(State == EHttpConnectionState::AwaitingProcessing);
	}

	if (!bRequestHandled)
	{
		const FString& ResponseError(FHttpServerErrorStrings::NotFound);
		auto Response = FHttpServerResponse::Error(EHttpServerResponseCodes::NotFound, ResponseError);
		OnProcessingComplete(MoveTemp(Response));
	}
}

void FHttpConnection::BeginWrite(TUniquePtr<FHttpServerResponse>&& Response, uint32 RequestNumber)
{
	// Ensure the passed-in request number is the one we expect
	check(RequestNumber == LastRequestNumber);

	ChangeState(EHttpConnectionState::Writing);

	if (bKeepAlive)
	{
		FString KeepAliveTimeoutStr = FString::Printf(TEXT("timeout=%f"), ConnectionKeepAliveTimeout);
		TArray<FString> KeepAliveTimeoutValue = { MoveTemp(KeepAliveTimeoutStr) };
		Response->Headers.Add(FHttpServerHeaderKeys::KEEP_ALIVE, MoveTemp(KeepAliveTimeoutValue));
	}

	WriteContext.ResetContext(MoveTemp(Response));
	ContinueWrite(0.0f);
}

void FHttpConnection::ContinueWrite(float DeltaTime)
{
	check(State == EHttpConnectionState::Writing);

	auto WriterState = WriteContext.WriteStream(DeltaTime);
	switch (WriterState)
	{
	case EHttpConnectionContextState::Continue:
		break;

	case EHttpConnectionContextState::Done:
		CompleteWrite();
		break;

	case EHttpConnectionContextState::Error:
		HandleWriteError(*WriteContext.GetErrorStr());
		break;
	}
}

void FHttpConnection::CompleteWrite()
{
	check(EHttpConnectionState::Writing == State);

	if (bKeepAlive && !bGracefulDestroyRequested)
	{
		ChangeState(EHttpConnectionState::AwaitingRead);
	}
	else
	{
		Destroy(EConnectionDestroyReason::WriteComplete);
	}
}

void FHttpConnection::RequestDestroy(bool bGraceful)
{
	if (EHttpConnectionState::Destroyed == State)
	{
		return;
	}

	bGracefulDestroyRequested = bGraceful;

	// If we aren't gracefully destroying, or we are otherwise already 
	// awaiting a read operation (not started yet), destroy() immediately.
	if (!bGracefulDestroyRequested || State == EHttpConnectionState::AwaitingRead)
	{
		Destroy(EConnectionDestroyReason::DestroyRequest);
	}
}

void FHttpConnection::Destroy(EConnectionDestroyReason Reason)
{
	check(State != EHttpConnectionState::Destroyed);
	ChangeState(EHttpConnectionState::Destroyed);

	if (Socket)
	{
		ISocketSubsystem *SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (SocketSubsystem)
		{
			SocketSubsystem->DestroySocket(Socket);
		}
		Socket = nullptr;

		UE_LOG(LogHttpConnection, Verbose, TEXT("ConnectionDestroyed, Reason: %s"), LexToString(Reason));
	}
}

void FHttpConnection::HandleReadError(EHttpServerResponseCodes ErrorCode, const TCHAR* ErrorCodeStr)
{
	UE_LOG(LogHttpConnection, Error, TEXT("%s"), ErrorCodeStr);

	// Forcibly Reply
	bKeepAlive = false;
	auto Response = FHttpServerResponse::Error(ErrorCode, ErrorCodeStr);
	BeginWrite(MoveTemp(Response), LastRequestNumber);
}

void FHttpConnection::HandleWriteError(const TCHAR* ErrorCodeStr)
{
	UE_LOG(LogHttpConnection, Error, TEXT("%s"), ErrorCodeStr);

	// Forcibly Close
	bKeepAlive = false;
	Destroy(EConnectionDestroyReason::WriteError);
}

bool FHttpConnection::ResolveKeepAlive(HttpVersion::EHttpServerHttpVersion HttpVersion, const TArray<FString>& ConnectionHeaders)
{
	const bool bDefaultKeepAlive = HttpVersion == HttpVersion::EHttpServerHttpVersion::HTTP_VERSION_1_1;

	if (bDefaultKeepAlive)
	{
		return !ConnectionHeaders.Contains(TEXT("close"));
	}
	else
	{
		return ConnectionHeaders.Contains(TEXT("Keep-Alive"));
	}
	return true;
}
