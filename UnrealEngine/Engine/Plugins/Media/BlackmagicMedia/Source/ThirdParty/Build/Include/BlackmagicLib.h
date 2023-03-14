// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifdef _WINDOWS
	#ifdef UEBLACKMAGICDESIGN_EXPORTS
		#define UEBLACKMAGICDESIGN_API __declspec(dllexport)
	#else
		#define UEBLACKMAGICDESIGN_API
	#endif
#else // _WINDOWS
	#ifdef UEBLACKMAGICDESIGN_EXPORTS
		#define UEBLACKMAGICDESIGN_API __attribute__ ((visibility ("default")))
	#else
		#define UEBLACKMAGICDESIGN_API
	#endif
#endif // _WINDOWS

#include "BlackmagicReferencePtr.h"

namespace GPUTextureTransfer
{
	class ITextureTransfer;
}

namespace BlackmagicDesign
{
	extern GPUTextureTransfer::ITextureTransfer* TextureTransfer;

	using LoggingCallbackPtr = void(*)(const TCHAR* Format, ...);
	using FBlackmagicVideoFormat = int32_t; //BMDDisplayMode

	enum class EPixelFormat
	{
		pf_8Bits,
		pf_10Bits
	};

	enum class EPixelColor
	{
		YCbCr,
		RGB,
	};

	enum class EFieldDominance
	{
		Progressive,
		Interlaced,
		ProgressiveSegmentedFrame,
	};

	enum class EAudioBitDepth
	{
		Signed_16Bits,
		Signed_32Bits
	};

	enum class EAudioChannelConfiguration : uint8_t
	{
		Channels_2 = 2,
		Channels_8 = 8,
		Channels_16 = 16
	};

	enum class EAudioSampleRate : uint32_t
	{
		SR_48kHz = 48000
	};

	enum class ERHI : uint8_t
	{
		Invalid,
		D3D11,
		D3D12,
		Cuda,
		Vulkan
	};

	namespace Private
	{
		class DeviceScanner;
		class VideoFormatsScanner;
	}

	/* FUniqueIdentifier definition
	*****************************************************************************/
	struct UEBLACKMAGICDESIGN_API FUniqueIdentifier
	{
		FUniqueIdentifier();
		explicit FUniqueIdentifier(int32_t InIdentifier);
		bool IsValid() const;
		bool operator== (const FUniqueIdentifier& rhs) const;

	private:
		int32_t Identifier;
	};

	/* FTimecode definition
	 * limited to 30fps
	*****************************************************************************/
	struct UEBLACKMAGICDESIGN_API FTimecode
	{
		FTimecode();
		bool operator== (const FTimecode& Other) const;

		uint32_t Hours;
		uint32_t Minutes;
		uint32_t Seconds;
		uint32_t Frames;
		bool bIsDropFrame;
	};

	enum struct ETimecodeFormat
	{
		TCF_None,
		TCF_LTC,
		TCF_VITC1,
		TCF_Auto
	};

	enum struct ELinkConfiguration
	{
		SingleLink,
		DualLink,
		QuadLinkTSI,
		QuadLinkSqr,
	};
 
	/* FFormatInfo definition
	 * Information about a given frame desc
	*****************************************************************************/
	struct UEBLACKMAGICDESIGN_API FFormatInfo
	{
		/** Framerate */
		uint32_t FrameRateNumerator;
		uint32_t FrameRateDenominator;

		/** Image Width & Weight in texels */
		uint32_t Width;
		uint32_t Height;

		EFieldDominance FieldDominance;
		FBlackmagicVideoFormat DisplayMode; // Unique identifier that represent all that combination for the device

		bool operator==(FFormatInfo& Other) const;

	};
	
	/* FAudioFormat definition
	*****************************************************************************/
	struct UEBLACKMAGICDESIGN_API FAudioFormat
	{
		EAudioBitDepth BitDepth;
		EAudioChannelConfiguration ChannelConfigration;
		bool operator==(FAudioFormat& Other) const;
	};

	/* FChannelInfo definition
	*****************************************************************************/
	struct UEBLACKMAGICDESIGN_API FChannelInfo
	{
		int32_t DeviceIndex;

		bool operator==(FChannelInfo& Other) const;
	};

	/* FInputChannelOptions definition
	*****************************************************************************/
	struct UEBLACKMAGICDESIGN_API FInputChannelOptions
	{
		FInputChannelOptions();

		FFormatInfo FormatInfo;
		int32_t CallbackPriority;

		bool bReadVideo;
		EPixelFormat PixelFormat;
		
		ETimecodeFormat TimecodeFormat;

		bool bReadAudio;
		int32_t NumberOfAudioChannel;

		bool bUseTheDedicatedLTCInput;
		bool bAutoDetect;
	};

	/* FOutputChannelOptions definition
	*****************************************************************************/
	struct UEBLACKMAGICDESIGN_API FOutputChannelOptions
	{
		FOutputChannelOptions();

		FFormatInfo FormatInfo;
		int32_t CallbackPriority;
		EPixelFormat PixelFormat;

		EAudioSampleRate AudioSampleRate;
		EAudioBitDepth AudioBitDepth;
		EAudioChannelConfiguration NumAudioChannels;

		uint32_t NumberOfBuffers;

		ETimecodeFormat TimecodeFormat;
		ELinkConfiguration LinkConfiguration;

		bool bOutputKey;
		bool bOutputVideo;
		bool bOutputAudio;
		bool bOutputInterlacedFieldsTimecodeNeedToMatch;
		bool bLogDropFrames;
		bool bUseGPUDMA;
		bool bScheduleInDifferentThread;
	};

	/* IInputEventCallback definition
	*****************************************************************************/
	struct UEBLACKMAGICDESIGN_API IInputEventCallback
	{
		struct UEBLACKMAGICDESIGN_API FFrameReceivedInfo
		{
			FFrameReceivedInfo();

			bool bHasInputSource;

			int64_t FrameNumber;

			// Timecode
			bool bHaveTimecode;
			FTimecode Timecode;

			// Video
			void* VideoBuffer;
			int32_t VideoWidth;
			int32_t VideoHeight;
			int32_t VideoPitch;
			EPixelFormat PixelFormat;
			EFieldDominance FieldDominance;

			// Audio
			void* AudioBuffer;
			int32_t AudioBufferSize;
			int32_t NumberOfAudioChannel;
			int32_t AudioRate;
		};

		virtual ~IInputEventCallback();

		virtual void AddRef() = 0;
		virtual void Release() = 0;

		virtual void OnInitializationCompleted(bool bSuccess) = 0;
		virtual void OnShutdownCompleted() = 0;

		virtual void OnFrameReceived(const FFrameReceivedInfo&) = 0;
		virtual void OnFrameFormatChanged(const FFormatInfo& NewFormat) = 0;
		virtual void OnInterlacedOddFieldEvent(long long FrameNumber) = 0;
	};

	/* IOutputEventCallback definition
	*****************************************************************************/
	struct UEBLACKMAGICDESIGN_API IOutputEventCallback
	{
		struct UEBLACKMAGICDESIGN_API FFrameSentInfo
		{
			FFrameSentInfo();

			uint32_t FramesLost;
			uint32_t FramesDropped;
		};

		virtual ~IOutputEventCallback();

		virtual void AddRef() = 0;
		virtual void Release() = 0;

		virtual void OnInitializationCompleted(bool bSuccess) = 0;
		virtual void OnShutdownCompleted() = 0;

		virtual void OnOutputFrameCopied(const FFrameSentInfo& InFrameInfo) = 0;
		virtual void OnPlaybackStopped() = 0;
		virtual void OnInterlacedOddFieldEvent() = 0;
	};

	struct UEBLACKMAGICDESIGN_API FBaseFrameData
	{
		FTimecode Timecode;
		uint32_t FrameIdentifier = 0;
		bool bEvenFrame = true;
	};

	struct UEBLACKMAGICDESIGN_API FFrameDescriptor : public FBaseFrameData
	{
		uint8_t* VideoBuffer = nullptr;
		int32_t VideoWidth = 0;
		int32_t VideoHeight = 0;
	};

	struct UEBLACKMAGICDESIGN_API FFrameDescriptor_GPUDMA : public FBaseFrameData
	{
		void* RHITexture = nullptr;
	};

	struct UEBLACKMAGICDESIGN_API FAudioSamplesDescriptor
	{
		uint8_t* AudioBuffer = nullptr;
		int32_t AudioBufferLength = 0;
		int32_t NumAudioSamples = 0;

		FTimecode Timecode;
		uint32_t FrameIdentifier = 0;
	};

	struct UEBLACKMAGICDESIGN_API FInitializeDMAArgs
	{
		ERHI RHI = ERHI::Invalid;
		void* RHIDevice = nullptr;
		void* RHICommandQueue = nullptr;

		// Begin Vulkan Only
		void* VulkanInstance = nullptr;
		uint8_t RHIDeviceUUID[16] = { 0 };
		// End Vulkan Only
	};

	struct UEBLACKMAGICDESIGN_API FRegisterDMABufferArgs
	{
		void* Buffer = nullptr;
		uint32_t Stride = 0;
		uint32_t Height = 0;
		uint32_t Width = 0;
	};

	struct UEBLACKMAGICDESIGN_API FRegisterDMATextureArgs
	{
		void* RHITexture = nullptr;
		void* RHIResourceMemory = nullptr; // Vulkan only
	};

	/* BlackmagicDeviceScanner definition
	*****************************************************************************/
	class UEBLACKMAGICDESIGN_API BlackmagicDeviceScanner
	{
	public:
		const static int32_t FormatedTextSize = 64;
		using FormatedTextType = TCHAR[FormatedTextSize];

		struct UEBLACKMAGICDESIGN_API DeviceInfo
		{
			bool bIsSupported;
			bool bCanDoCapture;
			bool bCanDoPlayback;
			bool bCanDoFullDuplex;
			bool bCanDoDualLink;
			bool bCanDoQuadLink;
			bool bCanDoQuadSquareLink;
			bool bHasGenlockReferenceInput;
			bool bHasLTCTimecodeInput;
			bool bCanAutoDetectInputFormat;
			bool bSupportInternalKeying;
			bool bSupportExternalKeying;

			uint32_t NumberOfSubDevices;
			uint32_t DevicePersistentId;
			uint32_t ProfileId;
			uint32_t DeviceGroupId;
			uint32_t SubDeviceIndex;
		};

		BlackmagicDeviceScanner();
		~BlackmagicDeviceScanner();

		BlackmagicDeviceScanner(const BlackmagicDeviceScanner&) = delete;
		BlackmagicDeviceScanner& operator=(const BlackmagicDeviceScanner&) = delete;

		int32_t GetNumDevices() const;
		bool GetDeviceTextId(int32_t InDeviceIndex, FormatedTextType& OutTextId) const;
		bool GetDeviceInfo(int32_t InDeviceIndex, DeviceInfo& OutDeviceInfo) const;

	private:
		Private::DeviceScanner* Scanner;
	};

	/* BlackmagicVideoFormats definition
	*****************************************************************************/
	struct UEBLACKMAGICDESIGN_API BlackmagicVideoFormats
	{
		struct UEBLACKMAGICDESIGN_API VideoFormatDescriptor
		{
			VideoFormatDescriptor();

			FBlackmagicVideoFormat VideoFormatIndex;
			uint32_t FrameRateNumerator;
			uint32_t FrameRateDenominator;
			uint32_t ResolutionWidth;
			uint32_t ResolutionHeight;
			bool bIsProgressiveStandard;
			bool bIsInterlacedStandard;
			bool bIsPsfStandard;
			bool bIsSD;
			bool bIsHD;
			bool bIs2K;
			bool bIs4K;
			bool bIs8K;

			bool bIsValid;
		};

		BlackmagicVideoFormats(int32_t InDeviceId, bool bForOutput);
		~BlackmagicVideoFormats();

		BlackmagicVideoFormats(const BlackmagicVideoFormats&) = delete;
		BlackmagicVideoFormats& operator=(const BlackmagicVideoFormats&) = delete;

		int32_t GetNumSupportedFormat() const;
		VideoFormatDescriptor GetSupportedFormat(int32_t InIndex) const;

	private:
		Private::VideoFormatsScanner* Formats;
	};

	 /* Configure Logging
	 *****************************************************************************/
	UEBLACKMAGICDESIGN_API void SetLoggingCallbacks(LoggingCallbackPtr LogInfoFunc, LoggingCallbackPtr LogWarningFunc, LoggingCallbackPtr LogErrorFunc, LoggingCallbackPtr LogVerboseFunc);

	 /* Initialization
	 *****************************************************************************/
	UEBLACKMAGICDESIGN_API bool ApiInitialization();
	UEBLACKMAGICDESIGN_API void ApiUninitialization();

	 /* Register/Unregister
	 *****************************************************************************/
	UEBLACKMAGICDESIGN_API FUniqueIdentifier RegisterCallbackForChannel(const FChannelInfo& InChannelInfo, const FInputChannelOptions& InChannelOptions, ReferencePtr<IInputEventCallback> InCallback);
	UEBLACKMAGICDESIGN_API void UnregisterCallbackForChannel(const FChannelInfo& InChannelInfo, FUniqueIdentifier InIdentifier);
	
	UEBLACKMAGICDESIGN_API FUniqueIdentifier RegisterOutputChannel(const FChannelInfo& InChannelInfo, const FOutputChannelOptions& InChannelOptions, ReferencePtr<IOutputEventCallback> InCallback);
	UEBLACKMAGICDESIGN_API void UnregisterOutputChannel(const FChannelInfo& InChannelInfo, FUniqueIdentifier InIdentifier, bool bCallCompleted);
	UEBLACKMAGICDESIGN_API bool SendVideoFrameData(const FChannelInfo& InChannelInfo, const FFrameDescriptor& InFrame);
	UEBLACKMAGICDESIGN_API bool SendVideoFrameData(const FChannelInfo& InChannelInfo, FFrameDescriptor_GPUDMA& InFrame);
	UEBLACKMAGICDESIGN_API bool SendAudioSamples(const FChannelInfo& InChannelInfo, const FAudioSamplesDescriptor& InSamples);

	UEBLACKMAGICDESIGN_API bool InitializeDMA(const FInitializeDMAArgs& Args);
	UEBLACKMAGICDESIGN_API void UninitializeDMA();
	UEBLACKMAGICDESIGN_API bool RegisterDMATexture(const FRegisterDMATextureArgs& Args);
	UEBLACKMAGICDESIGN_API bool UnregisterDMATexture(void* InRHITexture);
	UEBLACKMAGICDESIGN_API bool RegisterDMABuffer(const FRegisterDMABufferArgs& InArgs);
	UEBLACKMAGICDESIGN_API bool UnregisterDMABuffer(void* InBuffer);
	UEBLACKMAGICDESIGN_API bool LockDMATexture(void* InRHITexture);
	UEBLACKMAGICDESIGN_API bool UnlockDMATexture(void* InRHITexture);
};
