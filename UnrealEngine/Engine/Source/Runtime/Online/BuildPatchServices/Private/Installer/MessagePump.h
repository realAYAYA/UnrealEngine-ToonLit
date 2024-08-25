// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "BuildPatchMessage.h"

namespace BuildPatchServices
{
	/**
	 * Interface for a message pump which allows systems to bubble up event information to the Installer's public API.
	 */
	class IMessagePump
	{
	public:
		/**
		 * Virtual destructor.
		 */
		virtual ~IMessagePump() { }

		/**
		 * Sends a chunk source event message.
		 * @param Message   The message to be sent.
		 */
		virtual void SendMessage(FChunkSourceEvent Message) = 0;

		/**
		 * Sends an installation file action message.
		 * @param Message   The message to be sent.
		 */
		virtual void SendMessage(FInstallationFileAction Message) = 0;

		/**
		 * Sends out a request to resolve the uri to the chunk location
		 * @param Request   Request for the chunk location
		 * @param OnResponse   A delegate to call with the response to chunk location
		 */
		virtual void SendRequest(FChunkUriRequest Request, TFunction<void(FChunkUriResponse)> OnResponse) = 0;

		/**
		 * Dequeues received messages, pushing them to the provided handlers.
		 * NOTE: PumpMessages, RegisterMessageHandler, and UnregisterMessageHandler MUST all be called from the same thread.
		 */
		virtual void PumpMessages() = 0;

		/**
		 * Registers a message handler.
		 * @param MessageHandler    Ptr to the message handler to add. Must not be null.
		 * NOTE: PumpMessages, RegisterMessageHandler, and UnregisterMessageHandler MUST all be called from the same thread.
		 */
		virtual void RegisterMessageHandler(FMessageHandler* MessageHandler) = 0;

		/**
		 * Unregisters a message handler.
		 * @param MessageHandler    Ptr to the message handler to remove.
		 * NOTE: PumpMessages, RegisterMessageHandler, and UnregisterMessageHandler MUST all be called from the same thread.
		 */
		virtual void UnregisterMessageHandler(FMessageHandler* MessageHandler) = 0;
	};

	/**
	 * A factory for creating an IMessagePump instance.
	 */
	class FMessagePumpFactory
	{
	public:
		/**
		 * Creates an instance of IMessagePump.
		 * @return a pointer to the new IMessagePump instance created.
		 */
		static IMessagePump* Create();
	};

}
