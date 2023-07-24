// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Internationalization/TextKey.h"

namespace UE::SharedMemoryMedia
{
	// When the implementation requires spin waiting on a fence or polling shared system memory,
	// it will sleep by the amount below in a loop until the condition is met or a given timout occurs.
	constexpr float SpinWaitTimeSeconds = 50 * 1e-6;

	/** 
	 * This defines the number of textures used for communication. Having more that one allows for overlapping
	 * sends and minimize waits on frame acks for texture re-use. Use 3 for best performance, or 2 for
	 * a potential compromise with resource usage.
	 */
	constexpr int32 SenderNumBuffers = 3;

	// Guid that identifies the SharedMemoryMediaPlayer.
	static const FGuid PlayerGuid = FGuid(0xAF8C5107, 0x13CF992C, 0x9FD4EBFE, 0x2E3E049F);

	/**
	 * Used to generate a Guid based on the UniqueName that sender and receiver uses to open the same
	 * shared system memory used for IPC. There will be one shard memory region per buffer index.
	 * There are SenderNumBuffers buffer indices.
	 * A given frame number uses a buffer index defined by:
	 *   BufferIdx = FrameNumber % SenderNumBuffers.
	 */
	static FGuid GenerateSharedMemoryGuid(FString UniqueName, uint32 BufferIdx)
	{
		// Avoid the hard to spot user error of differing spaces in strings.
		UniqueName.RemoveSpacesInline();

		const uint32 UniqueNameHash = TextKeyUtil::HashString(UniqueName);
		return FGuid(0xDA90FCAF, 0x7034A2AA, UniqueNameHash, 0xEB12036B + BufferIdx);
	};

	// Convenience ZeroGuid value.
	static const FGuid ZeroGuid = FGuid(0, 0, 0, 0);

}

struct FSharedMemoryMediaTextureDescription
{
	uint32 Width = 0;
	uint32 Height = 0;
	uint32 BytesPerPixel = 0;
	uint32 Stride = 0;
	EPixelFormat Format = EPixelFormat::PF_Unknown;
	FGuid Guid = UE::SharedMemoryMedia::ZeroGuid;
	bool bSrgb = false;

	bool IsEquivalentTo(const FSharedMemoryMediaTextureDescription& Other) const
	{
		return Width == Other.Width
			&& Height == Other.Height
			&& BytesPerPixel == Other.BytesPerPixel
			&& Stride == Other.Stride
			&& Format == Other.Format
			&& bSrgb == Other.bSrgb
			;
	}
};

/**
 * This structure represents the IPC between sender and receiver of the shared memory media.
 */
struct FSharedMemoryMediaFrameMetadata
{
	static constexpr uint32 MAGIC = 0x534D4D4D; // SMMM - Shared Memory Media Metadata

	// Data that the sender wants to communicate
	struct FSender
	{
		// Used to determine that the rest of the metadata is valid.
		uint32 Magic = MAGIC;

		// Each shared cross gpu texture is assigned a Guid. The player can use this to locate and open them.
		FGuid TextureGuid;

		// This is the sender's frame number of the pixel data in the texture.
		uint32 FrameNumber = 0;
	} Sender;

	// Data that the receiver wants to communicate (although KeepAliveShiftRegister is shifted by the Sender)
	struct FReceiver
	{
		// Frame number that the receiver is done reading. This signals the sender that it can re-use the texture.
		uint32 FrameNumberAcked = 0;

		/** 
		 * The receiver sets to ones this register. This signals the sender that it should wait for the related frame number ack before re-using the texture.
		 * The sender will shift this register to the right each frame, as a way to time out the session in case the receiver disappears.
		 */
		uint8 KeepAliveShiftRegister = 0;
	} Receiver;
};


