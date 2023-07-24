// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "IAudioCodec.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealNames.h"

class ICompressedAudioInfo;

namespace Audio
{
	struct ICodec;
	struct IDecoderInput;

	class AUDIOEXTENSIONS_API ICodecRegistry
	{
		static TUniquePtr<ICodecRegistry> Instance;
	public:
		using FCodecPtr = ICodec * ;

		// Abstract Singleton.
		static ICodecRegistry& Get();

		virtual ~ICodecRegistry() = default;

		// Register each codec.
		virtual bool RegisterCodec(TUniquePtr<ICodec>&&) = 0;
		virtual bool UnregisterCodec(FCodecPtr) = 0;

		// Find exact code name. (Version can be INDEX_NONE and give the latest version).
		virtual FCodecPtr FindCodecByName(FName InName, int32 InVersion = INDEX_NONE) const = 0;

		// Find a codec given a decoders input interface
		virtual FCodecPtr FindCodecByParsingInput(IDecoderInput* InObject) const = 0;

		// Find codec by finding by family name.  (Version can be INDEX_NONE and give the latest version).
		virtual FCodecPtr FindCodecByFamilyName(FName InFamilyName, int32 InVersion = INDEX_NONE) const = 0;

		// Find default codec for a platform. (none uses host platform).
		virtual FCodecPtr FindDefaultCodec(FName InPlatformName = NAME_None) const = 0;
	};
}
