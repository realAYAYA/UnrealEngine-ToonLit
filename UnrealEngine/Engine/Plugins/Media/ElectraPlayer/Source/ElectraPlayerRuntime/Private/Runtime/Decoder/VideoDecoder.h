// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlayerCore.h"

#include "StreamAccessUnitBuffer.h"
#include "Decoder/DecoderBase.h"
#include "VideoDecoderResourceDelegate.h"



namespace Electra
{
	class IPlayerSessionServices;


	class IVideoDecoder : public IDecoderBase, public IAccessUnitBufferInterface, public IDecoderAUBufferDiags, public IDecoderReadyBufferDiags
	{
	public:
		static bool CanDecodeStream(const FStreamCodecInformation& InCodecInfo);

		static IVideoDecoder* Create();

		virtual ~IVideoDecoder() = default;

		virtual void SetPlayerSessionServices(IPlayerSessionServices* SessionServices) = 0;

		virtual void Open(TSharedPtrTS<FAccessUnit::CodecData> InCodecData, FParamDict&& InAdditionalOptions, const FStreamCodecInformation* InMaxStreamConfiguration) = 0;
		virtual bool Reopen(TSharedPtrTS<FAccessUnit::CodecData> InCodecData, const FParamDict& InAdditionalOptions, const FStreamCodecInformation* InMaxStreamConfiguration) = 0;
		virtual void Close() = 0;
		virtual void DrainForCodecChange() = 0;

		//! Sets a platform specific video resource delegate needed to handle decoder output.
		virtual void SetVideoResourceDelegate(TWeakPtr<IVideoDecoderResourceDelegate, ESPMode::ThreadSafe> InVideoResourceDelegate) = 0;

		//-------------------------------------------------------------------------
		// Methods from IDecoderBase
		//
		virtual void SetRenderer(TSharedPtr<IMediaRenderer, ESPMode::ThreadSafe> InRenderer) = 0;
		virtual void SuspendOrResumeDecoder(bool bSuspend, const FParamDict& InOptions) = 0;

		//-------------------------------------------------------------------------
		// Methods from IAccessUnitBufferInterface
		//
		//! Pushes an access unit to the decoder. Ownership of the access unit is transferred to the decoder.
		virtual void AUdataPushAU(FAccessUnit* AccessUnit) = 0;
		//! Notifies the decoder that there will be no further access units.
		virtual void AUdataPushEOD() = 0;
		//! Notifies the decoder that there may be further access units.
		virtual void AUdataClearEOD() = 0;
		//! Instructs the decoder to flush all pending input and all already decoded output.
		virtual void AUdataFlushEverything() = 0;

		//-------------------------------------------------------------------------
		// Methods from IDecoderAUBufferDiags
		//
		//! Registers an AU input buffer listener.
		virtual void SetAUInputBufferListener(IAccessUnitBufferListener* Listener) = 0;

		//-------------------------------------------------------------------------
		// Methods from IDecoderReadyBufferDiags
		//
		virtual void SetReadyBufferListener(IDecoderOutputBufferListener* Listener) = 0;
	};

} // namespace Electra

