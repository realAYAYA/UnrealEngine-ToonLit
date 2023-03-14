// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

namespace BuildPatchServices
{
	/**
	 * A message describing an event that occurred for a chunk source.
	 */
	struct FChunkSourceEvent
	{
		// Describes the event type.
		enum class EType : uint32
		{
			// Access was lost to the source.
			AccessLost = 0,
			// Access has been regained after being lost.
			AccessRegained,
		};

		// The type of event that occurred.
		EType Event;
		// The location context for the source, could be cloud root, install location, chunkdb file etc.
		FString Location;
	};

	/**
	 * A message describing an action taken to an installation file.
	 */
	struct FInstallationFileAction
	{
		// Describes the action type.
		enum class EType : uint32
		{
			// The file was removed.
			Removed = 0,
			// The file was added.
			Added,
			// The file was updated.
			Updated,
		};

		// The type of action that occurred.
		EType Action;
		// The filename affected, relative to the install location.
		FString Filename;
	};

	/**
	 * A Request for a chunk
	 */
	struct FChunkUriRequest
	{
		FString CloudDirectory;
		FString RelativePath;
	};

	/**
	 * A Response containing the actual location of the chunk
	 */
	struct FChunkUriResponse
	{
		FString Uri;
	};

	/**
	 * Base class of a message handler, this should be inherited from and passed to an installer to receive messages that you want to handle.
	 */
	class FMessageHandler
	{
	public:
		FMessageHandler() {}
		virtual ~FMessageHandler() {}

		/**
		 * Handles a chunk source event message.
		 * @param Message   The message to be handled.
		 */
		virtual void HandleMessage(const FChunkSourceEvent& Message) {}

		/**
		 * Handles an installation file action message.
		 * @param Message   The message to be handled.
		 */
		virtual void HandleMessage(const FInstallationFileAction& Message) {}

		/**
		 * Handles responding to a chunk Uri request
		 * @param Request   The request for a chunk
		 * @param OnResponse   The function to callback once the chunk has been found
		 */
		virtual bool HandleRequest(const FChunkUriRequest& Request, TFunction<void(FChunkUriResponse)> OnResponse) { return false; }
	};

	class FDefaultMessageHandler
		: public FMessageHandler
	{
	public:
		virtual bool HandleRequest(const FChunkUriRequest& Request, TFunction<void(FChunkUriResponse)> OnResponse) override
		{
			OnResponse({ Request.CloudDirectory / Request.RelativePath });
			return true;
		}
	};
}
