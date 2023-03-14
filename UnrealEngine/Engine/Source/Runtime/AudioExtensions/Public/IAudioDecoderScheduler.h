// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "HAL/Platform.h"
#include "Templates/SharedPointer.h"

namespace Audio
{
	struct IDecoderInput;
	struct IDecoderOutput;
		
	class FActiveDecodeHandle
	{
		uint32 Id;
	public:
		FActiveDecodeHandle(uint32 InId) 
			: Id(InId) 
		{}
		bool IsValid() const { return Id != 0; }
	};

	struct IDecoderScheduler
	{
		static IDecoderScheduler& Get();

		struct FStartDecodeArgs
		{
			uint32 StartingFrame = 0;
			TSharedPtr<IDecoderInput> InputObject;
			TSharedPtr<IDecoderOutput> OutputObject;
		};

		virtual FActiveDecodeHandle StartDecode(
			FStartDecodeArgs&& ) = 0;
	};
}