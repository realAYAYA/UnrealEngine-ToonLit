// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "Templates/Function.h"
#include "Templates/UnrealTemplate.h"
#include "VideoCommon.h"
#include "VideoDecoderAllocationTypes.h"
#include "VideoDecoderInput.h"

namespace AVEncoder
{
	class FVideoDecoderInput;
	class FVideoDecoderOutput;

	class FVideoDecoder
	{
	public:
		using CreateDecoderAllocationInterfaceCallback = TFunction<void(void* /*InOptions*/, void** /*InOutParamResult*/)>;
		using ReleaseDecoderAllocationInterfaceCallback = TFunction<void(void* /*InOptions*/, void** /*InOutParamResult*/)>;

		struct FInit
		{
			CreateDecoderAllocationInterfaceCallback	CreateDecoderAllocationInterface;
			ReleaseDecoderAllocationInterfaceCallback	ReleaseDecoderAllocationInterface;

			int32		Width;
			int32		Height;
		};

		// --- setup
		virtual bool Setup(const FInit& InInit) = 0;
		// Shuts the decoder down AND destroys it. Do not store a pointer to this decoder in a smart pointer!
		virtual void Shutdown() = 0;

		enum class EDecodeResult
		{
			Success,
			Failure
//			TryAgainLater
		};
		virtual EDecodeResult Decode(const FVideoDecoderInput* InInput) = 0;

		using OnDecodedFrameCallback = TFunction<void(const FVideoDecoderOutput* /*InDecodedFrame*/)>;

		virtual void SetOnDecodedFrame(OnDecodedFrameCallback InCallback) { OnDecodedFrame = MoveTemp(InCallback); }
		virtual void ClearOnDecodedFrame() { OnDecodedFrame = nullptr; }

	protected:
		FVideoDecoder() = default;
		virtual ~FVideoDecoder();

	protected:
		bool CreateDecoderAllocationInterface();
		void ReleaseDecoderAllocationInterface();
		EFrameBufferAllocReturn AllocateOutputFrameBuffer(FVideoDecoderAllocFrameBufferResult* OutBuffer, const FVideoDecoderAllocFrameBufferParams* InAllocParams);
		void* GetAllocationInterfaceMethods();

		struct FPlatformDecoderAllocInterface;

		OnDecodedFrameCallback	OnDecodedFrame;
		CreateDecoderAllocationInterfaceCallback	CreateDecoderAllocationInterfaceFN;
		ReleaseDecoderAllocationInterfaceCallback	ReleaseDecoderAllocationInterfaceFN;
		FPlatformDecoderAllocInterface*				PlatformDecoderAllocInterface = nullptr;
	};
}
