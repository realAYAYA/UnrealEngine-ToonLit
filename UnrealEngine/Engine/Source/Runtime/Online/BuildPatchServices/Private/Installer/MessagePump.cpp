// Copyright Epic Games, Inc. All Rights Reserved.

#include "Installer/MessagePump.h"
#include "Containers/Union.h"
#include "Containers/Queue.h"
#include <atomic>

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
		// IMessagePump interface begin.
		virtual void SendMessage(FChunkSourceEvent Message) override;
		virtual void SendMessage(FInstallationFileAction Message) override;
		virtual void SendRequest(FChunkUriRequest Request, TFunction<void(FChunkUriResponse)> OnResponse) override;
		virtual void PumpMessages() override;
		virtual void RegisterMessageHandler(FMessageHandler* MessageHandler) override;
		virtual void UnregisterMessageHandler(FMessageHandler* MessageHandler) override;
		// IMessagePump interface end.

	private:
		FMessageQueue MessageQueue;
		FChunkUriQueue ChunkUriRequestQueue;
		FDefaultMessageHandler DefaultMessageHandler;
		TArray<FMessageHandler*> Handlers;
		// std::atomic will not expose integral operations with enum types.
		std::atomic<uint32> MessageRequests{0};
	};

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
		if (!EnumHasAllFlags(static_cast<EMessageRequests>(MessageRequests.load()), EMessageRequests::ChunkUriRequest))
		{
			DefaultMessageHandler.HandleRequest(Request, OnResponse);
		}
		else
		{
			ChunkUriRequestQueue.Enqueue(FChunkUriRequestResponse(MoveTemp(Request), MoveTemp(OnResponse)));
		}
	}

	void FMessagePump::PumpMessages()
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

	void FMessagePump::RegisterMessageHandler(FMessageHandler* InHandler)
	{
		MessageRequests |= static_cast<uint32>(InHandler->GetMessageRequests());
		Handlers.Add(InHandler);
	}

	void FMessagePump::UnregisterMessageHandler(FMessageHandler* InHandler)
	{
		MessageRequests = 0;
		Handlers.Remove(InHandler);
		for (FMessageHandler* Handler : Handlers)
		{
			MessageRequests |= static_cast<uint32>(Handler->GetMessageRequests());
		}
	}

	IMessagePump* FMessagePumpFactory::Create()
	{
		return new FMessagePump();
	}
}
