// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <memory>
#include <string>
#include <vector>

#ifdef AJA_EXPORTS
#define AJA_API __declspec(dllexport)
#else
#define AJA_API __declspec(dllimport)
#endif

namespace GPUTextureTransfer
{
	class ITextureTransfer;
}

namespace AJA
{
	extern GPUTextureTransfer::ITextureTransfer* TextureTransfer;

	/* Types provided from the interface
	*****************************************************************************/
	typedef void* FDeviceScanner;
	typedef void* FDeviceInfo;
	typedef void* FAJADevice;
	typedef uint32_t FAJAVideoFormat;

	/* Logging Callbacks
	*****************************************************************************/
	using LoggingCallbackPtr = void(*)(const TCHAR* Format, ...);
	AJA_API void SetLoggingCallbacks(LoggingCallbackPtr LogInfoFunc, LoggingCallbackPtr LogWarningFunc, LoggingCallbackPtr LogErrorFunc);

	/* Pixel formats supported
	*****************************************************************************/
	enum struct EPixelFormat
	{
		PF_8BIT_YCBCR,	// As Input/Output
		PF_8BIT_ARGB,	// As Input/Output
		PF_10BIT_RGB,	// As Input/Output
		PF_10BIT_YCBCR,	// As Input/Output
	};

	/* SDI transport type
	*****************************************************************************/
	enum class ETransportType
	{
		TT_SdiSingle,
		TT_SdiSingle4kTSI,
		TT_SdiDual,
		TT_SdiQuadSQ,
		TT_SdiQuadTSI,
		TT_Hdmi,
		TT_Hdmi4kTSI,
	};


	 /* Timecode
	 *****************************************************************************/
	struct AJA_API FTimecode
	{
		FTimecode();
		bool operator== (const FTimecode& Other) const;

		uint32_t Hours;
		uint32_t Minutes;
		uint32_t Seconds;
		uint32_t Frames;
		bool bDropFrame;
	};

	enum struct ETimecodeFormat
	{
		TCF_None,
		TCF_LTC,
		TCF_VITC1,
	};

	namespace Private
	{
		class AutoDetectChannel;
		class DeviceScanner;
		class InputChannel;
		class OutputChannel;
		class SyncChannel;
		class TimecodeChannel;
		class VideoFormatsScanner;
	}

	/* AJADeviceScanner definition
	*****************************************************************************/
	class AJA_API AJADeviceScanner
	{
	public:
		const static int32_t FormatedTextSize = 64;
		using FormatedTextType = TCHAR[FormatedTextSize];

		struct AJA_API DeviceInfo
		{
			bool bIsSupported;

			bool bCanFrameStore1DoPlayback;
			bool bCanDoDualLink;
			bool bCanDo2K;
			bool bCanDo4K;
			bool bCanDo12GSdi;
			bool bCanDo12GRouting;
			bool bCanDoMultiFormat;
			bool bCanDoAlpha;
			bool bCanDo3GLevelConversion;
			bool bCanDoCustomAnc;
			bool bCanDoLtc;
			bool bCanDoLtcInRefPort;
			bool bCanDoTSI;
			bool bSupportPixelFormat8bitYCBCR;
			bool bSupportPixelFormat8bitARGB;
			bool bSupportPixelFormat10bitRGB;
			bool bSupportPixelFormat10bitYCBCR;

			uint32_t NumberOfLtcInput;
			uint32_t NumberOfLtcOutput;

			int32_t NumSdiInput;
			int32_t NumSdiOutput;
			int32_t NumHdmiInput;
			int32_t NumHdmiOutput;
		};

		AJADeviceScanner();
		~AJADeviceScanner();

		AJADeviceScanner(const AJADeviceScanner&) = delete;
		AJADeviceScanner& operator=(const AJADeviceScanner&) = delete;

		int32_t GetNumDevices() const;
		bool GetDeviceTextId(int32_t InDeviceIndex, FormatedTextType& OutTextId) const;
		bool GetDeviceInfo(int32_t InDeviceIndex, DeviceInfo& OutDeviceInfo) const;

	private:
		Private::DeviceScanner* Scanner;
	};

	/* AJAVideoFormats definition
	*****************************************************************************/
	struct AJA_API AJAVideoFormats
	{
		struct AJA_API VideoFormatDescriptor
		{
			VideoFormatDescriptor();

			FAJAVideoFormat VideoFormatIndex;
			uint32_t FrameRateNumerator;
			uint32_t FrameRateDenominator;
			uint32_t ResolutionWidth;
			uint32_t ResolutionHeight;
			bool bIsProgressiveStandard;
			bool bIsInterlacedStandard;
			bool bIsPsfStandard;
			bool bIsVideoFormatA;
			bool bIsVideoFormatB;
			bool bIs372DualLink;
			bool bIsSD;
			bool bIsHD;
			bool bIs2K;
			bool bIs4K;

			bool bIsValid;
		};

		AJAVideoFormats(int32_t InDeviceId);
		~AJAVideoFormats();

		AJAVideoFormats(const AJAVideoFormats&) = delete;
		AJAVideoFormats& operator=(const AJAVideoFormats&) = delete;

		int32_t GetNumSupportedFormat() const;
		VideoFormatDescriptor GetSupportedFormat(int32_t InIndex) const;
		static VideoFormatDescriptor GetVideoFormat(FAJAVideoFormat InVideoFormatIndex);
		static std::string VideoFormatToString(FAJAVideoFormat InVideoFormatIndex);
		static bool VideoFormatToString(FAJAVideoFormat InVideoFormatIndex, char* OutCStr, uint32_t MaxLen);

	private:
		Private::VideoFormatsScanner* Formats;
	};

	/* AJADeviceOptions definition
	*****************************************************************************/
	struct AJA_API AJADeviceOptions
	{
		AJADeviceOptions(uint32_t InChannelIndex)
			: DeviceIndex(InChannelIndex)
			, bWantMultiFormatMode(true)
		{}

		uint32_t DeviceIndex;
		bool bWantMultiFormatMode;
	};

	/* AJASyncChannel definition
	*****************************************************************************/
	struct AJA_API IAJASyncChannelCallbackInterface
	{
		IAJASyncChannelCallbackInterface();
		virtual ~IAJASyncChannelCallbackInterface();

		virtual void OnInitializationCompleted(bool bSucceed) = 0;
	};

	struct AJA_API AJASyncChannelOptions
	{
		AJASyncChannelOptions(const TCHAR* DebugName);

		IAJASyncChannelCallbackInterface* CallbackInterface;

		ETransportType TransportType;
		uint32_t ChannelIndex; // [1...x]
		FAJAVideoFormat VideoFormatIndex;
		ETimecodeFormat TimecodeFormat;
		bool bOutput; // port is output
		bool bWaitForFrameToBeReady; // port is input and we want to wait for the image to be sent to Unreal Engine before ticking
		bool bAutoDetectFormat;
	};

	class AJA_API AJASyncChannel
	{
	public:
		AJASyncChannel(AJASyncChannel&) = delete;
		AJASyncChannel& operator=(AJASyncChannel&) = delete;

		AJASyncChannel();
		~AJASyncChannel();

	public:
		bool Initialize(const AJADeviceOptions& InDevice, const AJASyncChannelOptions& InOption);
		void Uninitialize();

		// Only available if the initialization succeeded
		bool WaitForSync() const;
		bool GetTimecode(FTimecode& OutTimecode) const;
		bool GetSyncCount(uint32_t& OutCount) const;
		bool GetVideoFormat(FAJAVideoFormat& OutVideoFormat);

	private:
		Private::SyncChannel* Channel;
	};

	/* AJATimecodeChannel definition
	*****************************************************************************/
	struct AJA_API IAJATimecodeChannelCallbackInterface
	{
		IAJATimecodeChannelCallbackInterface();
		virtual ~IAJATimecodeChannelCallbackInterface();

		virtual void OnInitializationCompleted(bool bSucceed) = 0;
	};

	struct AJA_API AJATimecodeChannelOptions
	{
		AJATimecodeChannelOptions(const TCHAR* DebugName);

		IAJATimecodeChannelCallbackInterface* CallbackInterface;

		bool bUseDedicatedPin;

		//Timecode read from dedicated pin
		bool bReadTimecodeFromReferenceIn;
		uint32_t LTCSourceIndex; //[1...x]
		uint32_t LTCFrameRateNumerator;
		uint32_t LTCFrameRateDenominator;

		//Timecode read from input channels
		ETransportType TransportType;
		uint32_t ChannelIndex; // [1...x]
		FAJAVideoFormat VideoFormatIndex;
		ETimecodeFormat TimecodeFormat;
		bool bAutoDetectFormat;
	};

	class AJA_API AJATimecodeChannel
	{
	public:
		AJATimecodeChannel(AJATimecodeChannel&) = delete;
		AJATimecodeChannel& operator=(AJATimecodeChannel&) = delete;

		AJATimecodeChannel();
		~AJATimecodeChannel();

	public:
		bool Initialize(const AJADeviceOptions& InDevice, const AJATimecodeChannelOptions& InOption);
		void Uninitialize();

		// Only available if the initialization succeeded
		bool GetTimecode(FTimecode& OutTimecode) const;

	private:
		Private::TimecodeChannel* Channel;
	};

	/* AJAInputFrameData definition
	*****************************************************************************/
	struct AJA_API AJAInputFrameData
	{
		AJAInputFrameData();

		FTimecode Timecode;
		uint32_t FramesDropped; // frame dropped by the AJA
	};

	struct AJA_API AJAOutputFrameData : AJAInputFrameData
	{
		AJAOutputFrameData();

		uint32_t FramesLost; // frame ready by the game but not sent to AJA
	};

	struct AJA_API AJAAncillaryFrameData
	{
		AJAAncillaryFrameData();

		uint8_t* AncBuffer;
		uint32_t AncBufferSize;
		uint8_t* AncF2Buffer;
		uint32_t AncF2BufferSize;
	};

	struct AJA_API AJAAudioFrameData
	{
		AJAAudioFrameData();

		uint8_t* AudioBuffer;
		uint32_t AudioBufferSize;
		uint32_t NumChannels;
		uint32_t AudioRate;
		uint32_t NumSamples;
	};

	struct AJA_API AJAVideoFrameData
	{
		AJAVideoFrameData();
		FAJAVideoFormat VideoFormatIndex;
		uint8_t* VideoBuffer;
		uint32_t VideoBufferSize;
		uint32_t Stride;
		uint32_t Width;
		uint32_t Height;
		EPixelFormat PixelFormat;
		bool bIsProgressivePicture;
	};

	struct AJA_API AJARequestInputBufferData
	{
		AJARequestInputBufferData();
		bool bIsProgressivePicture;
		uint32_t AncBufferSize;
		uint32_t AncF2BufferSize;
		uint32_t AudioBufferSize;
		uint32_t VideoBufferSize;
	};

	struct AJA_API AJARequestedInputBufferData
	{
		AJARequestedInputBufferData();
		uint8_t* AncBuffer;
		uint8_t* AncF2Buffer;
		uint8_t* AudioBuffer;
		uint8_t* VideoBuffer;
	};

	struct AJA_API IAJAInputOutputChannelCallbackInterface : IAJASyncChannelCallbackInterface
	{
		IAJAInputOutputChannelCallbackInterface();

		virtual bool OnRequestInputBuffer(const AJARequestInputBufferData& RequestBuffer, AJARequestedInputBufferData& OutRequestedBuffer) = 0;
		virtual bool OnInputFrameReceived(const AJAInputFrameData& InFrameData, const AJAAncillaryFrameData& InAncillaryFrame, const AJAAudioFrameData& InAudioFrame, const AJAVideoFrameData& InVideoFrame) = 0;
		virtual void OnOutputFrameStarted() { }
		virtual bool OnOutputFrameCopied(const AJAOutputFrameData& InFrameData) = 0;
		virtual void OnCompletion(bool bSucceed) = 0;
		virtual void OnFormatChange(FAJAVideoFormat VideoFormat) {}
	};

	/* AJAInputOutputChannelOptions definition
	*****************************************************************************/
	enum class EAJAReferenceType
	{
		EAJA_REFERENCETYPE_EXTERNAL,
		EAJA_REFERENCETYPE_FREERUN,
		EAJA_REFERENCETYPE_INPUT,
	};

	struct AJA_API AJAInputOutputChannelOptions
	{
		AJAInputOutputChannelOptions(const TCHAR* DebugName, uint32_t InChannelIndex);

		IAJAInputOutputChannelCallbackInterface* CallbackInterface;

		uint32_t NumberOfAudioChannel;
		ETransportType TransportType;
		uint32_t ChannelIndex; // [1...x]
		uint32_t SynchronizeChannelIndex; // [1...x]
		uint32_t KeyChannelIndex; // [1...x] for output
		uint32_t OutputNumberOfBuffers; // [1...x] supported but not suggested (min of 2 is suggested)
		FAJAVideoFormat VideoFormatIndex;
		EPixelFormat PixelFormat;
		ETimecodeFormat TimecodeFormat;
		EAJAReferenceType OutputReferenceType;
		uint32_t BurnTimecodePercentY;

		union
		{
			struct 
			{
				uint32_t bUseAutoCirculating : 1;
				uint32_t bOutput : 1; // port is output
				uint32_t bUseKey : 1; // output will also sent the key on OutputKeyPortIndex
				uint32_t bOutputInterlacedFieldsTimecodeNeedToMatch : 1; // when trying to find the odd field that correspond to the even field, the 2 timecode need to match
				uint32_t bUseAncillary : 1; // enable ANC system
				uint32_t bUseAudio : 1; // enable audio input/output
				uint32_t bUseVideo : 1; // enable video input/output
				uint32_t bBurnTimecode : 1; // burn the timecode to the input or output image
				uint32_t bDisplayWarningIfDropFrames : 1;
				uint32_t bConvertOutputLevelAToB : 1; // enable video output 3G level A to convert it to 3G level B
				uint32_t bTEST_OutputInterlaced : 1; // when outputting, warn if the field 1 and field 2 are the same color. It except one of the line to be white and the other line to be not white
				uint32_t bUseGPUDMA : 1; // Whether to use GPU Direct when outputting.
				uint32_t bAutoDetectFormat : 1; // Whether to autodetect format on input.
			};
			uint32_t Options;
		};
	};

	/* AJAInputChannel definition
	*****************************************************************************/
	class AJA_API AJAInputChannel
	{
	public:
		AJAInputChannel(AJAInputChannel&) = delete;
		AJAInputChannel& operator=(AJAInputChannel&) = delete;

		AJAInputChannel();
		virtual ~AJAInputChannel();

	public:
		bool Initialize(const AJADeviceOptions& InDevice, const AJAInputOutputChannelOptions& Options);
		void Uninitialize();

		// Only available if the initialization succeeded
		uint32_t GetFrameDropCount() const;
		const AJAInputOutputChannelOptions& GetOptions() const;
		const AJADeviceOptions& GetDeviceOptions() const;

	private:
		Private::InputChannel* Channel;
	};

	/* AJAOutputFrameBufferData definition
	*****************************************************************************/
	struct AJA_API AJAOutputFrameBufferData
	{
		AJAOutputFrameBufferData();

		static const uint32_t InvalidFrameIdentifier;

		FTimecode Timecode;
		uint32_t FrameIdentifier;
		bool bEvenFrame;
	};

	/* AJAOutputChannel definition
	*****************************************************************************/
	class AJA_API AJAOutputChannel
	{
	public:
		AJAOutputChannel(AJAOutputChannel&) = delete;
		AJAOutputChannel& operator=(AJAOutputChannel&) = delete;

		AJAOutputChannel();
		virtual ~AJAOutputChannel();

	public:
		bool Initialize(const AJADeviceOptions& InDevice, const AJAInputOutputChannelOptions& Options);
		void Uninitialize();

		// Set a new buffer that will be copied to the AJA.
		bool SetAncillaryFrameData(const AJAOutputFrameBufferData& InFrameData, uint8_t* AncillaryBuffer, uint32_t AncillaryBufferSize);
		bool SetAudioFrameData(const AJAOutputFrameBufferData& InFrameData, uint8_t* AudioBuffer, uint32_t AudioBufferSize);
		bool SetVideoFrameData(const AJAOutputFrameBufferData& InFrameData, uint8_t* VideoBuffer, uint32_t VideoBufferSize);
		bool SetVideoFrameData(const AJAOutputFrameBufferData& InFrameData, void* RHITexture);

		bool GetOutputDimension(uint32_t& OutWidth, uint32_t& OutHeight) const;
		int32_t GetNumAudioSamplesPerFrame(const AJAOutputFrameBufferData& InFrameData) const;

	private:
		Private::OutputChannel* Channel;
	};


	/* AJAAutoDetectChannelCallbackInterface definition
	*****************************************************************************/
	struct AJA_API IAJAAutoDetectCallbackInterface 
	{
		IAJAAutoDetectCallbackInterface();
		virtual ~IAJAAutoDetectCallbackInterface();

		virtual void OnCompletion(bool bSucceed) = 0;
	};

	/* AJAAutoDetectChannel definition
	*****************************************************************************/
	class AJA_API AJAAutoDetectChannel
	{
	public:
		struct AutoDetectChannelData
		{
			AutoDetectChannelData();

			FAJAVideoFormat DetectedVideoFormat;
			uint32_t DeviceIndex; // [0...x]
			uint32_t ChannelIndex; // [1...x]
			ETimecodeFormat TimecodeFormat;
		};

	public:
		AJAAutoDetectChannel(AJAAutoDetectChannel&) = delete;
		AJAAutoDetectChannel& operator=(AJAAutoDetectChannel&) = delete;

		AJAAutoDetectChannel();
		virtual ~AJAAutoDetectChannel();

	public:
		bool Initialize(IAJAAutoDetectCallbackInterface* InCallbackInterface);
		void Uninitialize();

		int32_t GetNumOfChannelData() const;
		AutoDetectChannelData GetChannelData(int32_t Index) const;

	private:
		Private::AutoDetectChannel* AutoChannel;
	};

	enum class ERHI : uint8_t
	{
		Invalid,
		D3D11,
		D3D12,
		Cuda,
		Vulkan
	};

	struct AJA_API FInitializeDMAArgs
	{
		ERHI RHI = ERHI::Invalid;
		void* RHIDevice = nullptr;
		void* RHICommandQueue = nullptr;

		// Begin Vulkan Only
		void* VulkanInstance = nullptr;
		uint8_t RHIDeviceUUID[16] = { 0 };
		// End Vulkan Only
	};

	struct AJA_API FRegisterDMABufferArgs
	{
		void* Buffer = nullptr;
		uint32_t Stride = 0;
		uint32_t Height = 0;
		uint32_t Width = 0;
		uint8_t BytesPerPixel = 0;
	};

	struct AJA_API FRegisterDMATextureArgs
	{
		void* RHITexture = nullptr;
		void* RHIResourceMemory = nullptr; // Vulkan only
	};

	AJA_API bool InitializeDMA(const FInitializeDMAArgs& Args);
	AJA_API void UninitializeDMA();
	AJA_API bool RegisterDMATexture(const FRegisterDMATextureArgs& Args);
	AJA_API bool UnregisterDMATexture(void* RHITexture);
	AJA_API bool RegisterDMABuffer(const FRegisterDMABufferArgs& InArgs);
	AJA_API bool UnregisterDMABuffer(void* InBuffer);
	AJA_API bool LockDMATexture(void* RHITexture);
	AJA_API bool UnlockDMATexture(void* RHITexture);
}
