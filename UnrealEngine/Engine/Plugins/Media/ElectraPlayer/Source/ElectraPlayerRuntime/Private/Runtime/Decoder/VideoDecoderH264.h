// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlayerCore.h"
#include "StreamAccessUnitBuffer.h"
#include "Decoder/VideoDecoderBase.h"
#include "VideoDecoderResourceDelegate.h"

class IOptionPointerValueContainer;

namespace Electra
{
	class IPlayerSessionServices;


	/**
	 * H264 video decoder.
	**/
	class IVideoDecoderH264 : public IVideoDecoderBase
	{
	public:
		struct FSystemConfiguration
		{
			FSystemConfiguration();
			struct FThreadConfig
			{
				FMediaRunnable::Param		Decoder;					//!< Decoder thread settings.
			};
			FThreadConfig					ThreadConfig;
		};

		struct FInstanceConfiguration
		{
			FInstanceConfiguration();
			int32								MaxFrameWidth;			//!< Maximum width of any image to be decoded
			int32								MaxFrameHeight;			//!< Maximum height of any image to be decoded
			int32								ProfileIdc;				//!< IDC profile (baseline (66), main (77) or high (100))
			int32								LevelIdc;				//!< IDC profile level (eg, 30, 31, 51)
			uint32								MaxDecodedFrames;		//!<
			FSystemConfiguration::FThreadConfig	ThreadConfig;			//!< Thread configuration (defaults to values set in SystemConfiguration)

			FParamDict							AdditionalOptions;
		};

		static bool Startup(const FSystemConfiguration& InConfig);
		static void Shutdown();

		struct FStreamDecodeCapability
		{
			enum class ESupported
			{
				NotSupported,
				SoftwareOnly,
				HardwareOnly,
				HardAndSoftware
			};
			ESupported	DecoderSupportType = ESupported::NotSupported;
			int32		Width = 0;
			int32		Height = 0;
			int32		Profile = 0;
			int32		Level = 0;
			double		FPS = 0.0;
			FParamDict	AdditionalOptions;
		};
		/*
			Queries decoder support/capability for a stream with given properties.
			Can be called after Startup() but should be called only shortly before playback to ensure all relevant
			libraries are initialized.
		*/
		static bool GetStreamDecodeCapability(FStreamDecodeCapability& OutResult, const FStreamDecodeCapability& InStreamParameter);

		static IVideoDecoderH264* Create();

		virtual ~IVideoDecoderH264() = default;

		virtual void SetPlayerSessionServices(IPlayerSessionServices* SessionServices) = 0;

		virtual void Open(const FInstanceConfiguration& InConfig) = 0;
		virtual void Close() = 0;
		virtual void DrainForCodecChange() = 0;


		virtual void SetMaximumDecodeCapability(int32 MaxWidth, int32 MaxHeight, int32 MaxProfile, int32 MaxProfileLevel, const FParamDict& AdditionalOptions) = 0;

		virtual void SetResourceDelegate(const TSharedPtr<IVideoDecoderResourceDelegate, ESPMode::ThreadSafe>& ResourceDelegate) = 0;

		//-------------------------------------------------------------------------
		// Methods from IDecoderBase
		//
		virtual void SetRenderer(TSharedPtr<IMediaRenderer, ESPMode::ThreadSafe> InRenderer) = 0;


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

		//-------------------------------------------------------------------------
		// Platform specifics
		//
#if PLATFORM_ANDROID
		virtual void Android_UpdateSurface(const TSharedPtr<IOptionPointerValueContainer, ESPMode::ThreadSafe>& Surface) = 0;

		virtual void Android_SuspendOrResumeDecoder(bool bSuspend) = 0;

		/**
		 * Workarounds may be needed for certain devices that are known to have some issues with decoders.
		 * Which device is affected is outside the scope of this module and must be provided by your application.
		 * Some features are always worked around if they are known to have issues on many devices and can be
		 * enabled if you know it to work on the device you are running on.
		 * 
		 * setOutputSurface()
		 *    exists only with API level 23+ and must be enabled if you know it to be working. Is not used by
		 *    default and instead creates a new decoder instead.
		 *    To enable add "setOutputSurface" with a value of "true".
		 */
		static FParamDict& Android_Workarounds();
#endif
	};


} // namespace Electra

