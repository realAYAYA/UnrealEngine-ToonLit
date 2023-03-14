// Copyright Epic Games, Inc. All Rights Reserved.

#include "Installer/MessagePump.h"
#include "Containers/Union.h"
#include "Containers/Queue.h"

namespace BuildPatchServices
{
	// Union of all possible message types.
	typedef TUnion<FChunkSourceEvent, FInstallationFileAction> FMessageUnion;

	// Queue type for messages.
	typedef TQueue<FMessageUnion, EQueueMode::Mpsc> FMessageQueue;

	// Tuple of the request and response for finding a chunk uri location
	typedef TTuple<FChunkUriRequest, TFunction<void(FChunkUriResponse)>> FChunkUriRequestResponse;

	// Queue of request responses of chunk we are looking for
	typedef TQueue<FChunkUriRequestResponse, EQueueMode::Mpsc> FChunkUriQueue;
}

namespace MessagePumpHelpers
{
	template<typename Type>
	bool TryPump(const TArray<BuildPatchServices::FMessageHandler*>& Handlers, const BuildPatchServices::FMessageUnion& MessageUnion)
	{
		if (MessageUnion.HasSubtype<Type>())
		{
			for (BuildPatchServices::FMessageHandler* Handler : Handlers)
			{
				Handler->HandleMessage(MessageUnion.GetSubtype<Type>());
			}
			return true;
		}
		return false;
	}
}

namespace BuildPatchServices
{
	class FMessagePump
		: public IMessagePump
	{
	public:
		FMessagePump();
		~FMessagePump();

		// IMessagePump interface begin.
		virtual void SendMessage(FChunkSourceEvent Message) override;
		virtual void SendMessage(FInstallationFileAction Message) override;
		virtual void SendRequest(FChunkUriRequest Request, TFunction<void(FChunkUriResponse)> OnResponse) override;
		virtual void PumpMessages(const TArray<FMessageHandler*>& Handlers) override;
		// IMessagePump interface end.

	private:
		FMessageQueue MessageQueue;
		FChunkUriQueue ChunkUriRequestQueue;
	};

	FMessagePump::FMessagePump()
	{
	}

	FMessagePump::~FMessagePump()
	{
	}

	void FMessagePump::SendMessage(FChunkSourceEvent Message)
	{
		MessageQueue.Enqueue(FMessageUnion(MoveTemp(Message)));
	}

	void FMessagePump::SendMessage(FInstallationFileAction Message)
	{
		MessageQueue.Enqueue(FMessageUnion(MoveTemp(Message)));
	}

	void FMessagePump::SendRequest(FChunkUriRequest Request, TFunction<void(FChunkUriResponse)> OnResponse)
	{
		ChunkUriRequestQueue.Enqueue(FChunkUriRequestResponse(MoveTemp(Request), MoveTemp(OnResponse)));
	}

	void FMessagePump::PumpMessages(const TArray<FMessageHandler*>& Handlers)
	{
		FMessageUnion MessageUnion;
		while (MessageQueue.Dequeue(MessageUnion))
		{
			if (MessagePumpHelpers::TryPump<FChunkSourceEvent>(Handlers, MessageUnion))
			{
				continue;
			}
			else if (MessagePumpHelpers::TryPump<FInstallationFileAction>(Handlers, MessageUnion))
			{
				continue;
			}
		}

		FDefaultMessageHandler DefaultMessageHandler;
		FChunkUriRequestResponse ChunkUriRequestResponse;
		while (ChunkUriRequestQueue.Dequeue(ChunkUriRequestResponse))
		{
			bool bHandled = false;
			for (BuildPatchServices::FMessageHandler* Handler : Handlers)
			{
				if (Handler->HandleRequest(ChunkUriRequestResponse.Get<0>(), ChunkUriRequestResponse.Get<1>()))
				{
					bHandled = true;
					break;
				}
			}
			if (!bHandled)
			{
				DefaultMessageHandler.HandleRequest(ChunkUriRequestResponse.Get<0>(), ChunkUriRequestResponse.Get<1>());
			}
		}
	}

	IMessagePump* FMessagePumpFactory::Create()
	{
		return new FMessagePump();
	}
}
