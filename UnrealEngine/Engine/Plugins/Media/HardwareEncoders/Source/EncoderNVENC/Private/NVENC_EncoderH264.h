// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NVENC_Common.h"
#include "VideoEncoderFactory.h"
#include "VideoEncoderInputImpl.h"
#include "HAL/Event.h"

namespace AVEncoder
{
	class FVideoEncoderNVENC_H264 : public FVideoEncoder
	{

	public:
		virtual ~FVideoEncoderNVENC_H264() override; 

		bool Setup(TSharedRef<FVideoEncoderInput> InputFrameFactory, const FLayerConfig& InitConfig) override;
		void Shutdown() override;

		// Query whether or not encoder is supported and available
		static bool GetIsAvailable(const FVideoEncoderInput& InFrameFactory, FVideoEncoderInfo& OutEncoderInfo);

		// Register encoder with video encoder factory
		static void Register(FVideoEncoderFactory& InFactory);

		void Encode(const TSharedPtr<FVideoEncoderInputFrame> InFrame, const FEncodeOptions& EncodeOptions) override;
		void Flush();

	protected:
		FLayer* CreateLayer(uint32 InLayerIndex, const FLayerConfig& InLayerConfig) override;
		void DestroyLayer(FLayer* layer) override;

	private:
		FVideoEncoderNVENC_H264();

		class FNVENCLayer : public FLayer
		{

		public:
			struct FInputOutput
			{
				TSharedPtr<AVEncoder::FVideoEncoderInputFrameImpl> SourceFrame = nullptr;
				void* InputTexture = nullptr;
				uint32 Width = 0;
				uint32 Height = 0;
				uint32 Pitch = 0;
				NV_ENC_BUFFER_FORMAT BufferFormat = NV_ENC_BUFFER_FORMAT_UNDEFINED;
				NV_ENC_REGISTERED_PTR RegisteredInput = nullptr;
				NV_ENC_INPUT_PTR MappedInput = nullptr;
				NV_ENC_PIC_PARAMS PicParams = {};
				NV_ENC_OUTPUT_PTR OutputBitstream = nullptr;
				const void* BitstreamData = nullptr;
				uint32 BitstreamDataSize = 0;
				NV_ENC_PIC_TYPE PictureType = NV_ENC_PIC_TYPE_UNKNOWN;
				uint32 FrameAvgQP = 0;
				uint64 TimeStamp;
				uint64 SubmitTimeCycles;
			};

			FNVENCLayer(uint32 layerIdx, FLayerConfig const& config, FVideoEncoderNVENC_H264& encoder);
			~FNVENCLayer();
			bool Setup();
			bool CreateSession();
			bool CreateInitialConfig();
			int GetCapability(NV_ENC_CAPS CapsToQuery) const;
			FString GetError(NVENCSTATUS ForStatus) const;
			void MaybeReconfigure();
			void UpdateConfig();
			void UpdateLastEncodedQP(uint32 InLastEncodedQP);
			void Encode(const TSharedPtr<FVideoEncoderInputFrame> InFrame, const FEncodeOptions& EncodeOptions);
			void EncodeBuffer(FInputOutput* Buffer);
			void ProcessEncodedBuffer(FInputOutput* Buffer);
			void Flush();
			void Shutdown();
			void UpdateBitrate(uint32 InMaxBitRate, uint32 InTargetBitRate);
			void UpdateResolution(uint32 InMaxBitRate, uint32 InTargetBitRate);
			FInputOutput* GetOrCreateBuffer(const TSharedPtr<FVideoEncoderInputFrameImpl> InFrame);
			FInputOutput* CreateBuffer();
			void DestroyBuffer(FInputOutput* InBuffer);
			bool RegisterInputTexture(FInputOutput* InBuffer);
			bool UnregisterInputTexture(FInputOutput* InBuffer);
			bool MapInputTexture(FInputOutput* InBuffer);
			bool UnmapInputTexture(FInputOutput* InBuffer);
			bool LockOutputBuffer(FInputOutput* InBuffer);
			bool UnlockOutputBuffer(FInputOutput* InBuffer);
			void CreateResourceDIRECTX(FInputOutput* InBuffer, NV_ENC_REGISTER_RESOURCE& RegisterParam, FIntPoint TextureSize);
			void CreateResourceCUDAARRAY(FInputOutput* InBuffer, NV_ENC_REGISTER_RESOURCE& RegisterParam, FIntPoint TextureSize);

		public:
			FVideoEncoderNVENC_H264& Encoder;
			FNVENCCommon& NVENC;
			GUID CodecGUID;
			uint32 LayerIndex;
			void* NVEncoder = nullptr;
			NV_ENC_INITIALIZE_PARAMS EncoderInitParams;
			NV_ENC_CONFIG EncoderConfig;
			FDateTime LastKeyFrameTime = 0;
			bool bForceNextKeyframe = false;
			uint32 LastEncodedQP = 0;

		private:
			FThreadSafeBool bIsProcessingFrame = false;
			FInputOutput* InputOutputBuffer = nullptr;
		};

		FNVENCCommon& NVENC;
		EVideoFrameFormat FrameFormat = EVideoFrameFormat::Undefined;

		// this could be a TRefCountPtr<ID3D11Device>, CUcontext or void*
		void* EncoderDevice;
	};

} // namespace AVEncoder
