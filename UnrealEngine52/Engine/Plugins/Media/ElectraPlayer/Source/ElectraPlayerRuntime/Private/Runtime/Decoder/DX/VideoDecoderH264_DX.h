// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Utilities/UtilsMPEGVideo.h"
#include "Decoder/VideoDecoderHelpers.h"

namespace Electra
{
	/**
	 * H264 video decoder class implementation.
	**/
	class FVideoDecoderH264 : public IVideoDecoderH264, public FMediaThread
	{
	public:
		static bool Startup(const IVideoDecoderH264::FSystemConfiguration& InConfig);
		static void Shutdown();

		static bool GetStreamDecodeCapability(FStreamDecodeCapability& OutResult, const FStreamDecodeCapability& InStreamParameter);

		FVideoDecoderH264();
		virtual ~FVideoDecoderH264();

		void TestHardwareDecoding();

		virtual void SetPlayerSessionServices(IPlayerSessionServices* SessionServices) override;

		virtual void Open(const FInstanceConfiguration& InConfig) override;
		virtual void Close() override;
		virtual void DrainForCodecChange() override;

		virtual void SetMaximumDecodeCapability(int32 MaxWidth, int32 MaxHeight, int32 MaxProfile, int32 MaxProfileLevel, const FParamDict& AdditionalOptions) override;

		virtual void SetAUInputBufferListener(IAccessUnitBufferListener* InListener) override;

		virtual void SetReadyBufferListener(IDecoderOutputBufferListener* InListener) override;

		virtual void SetRenderer(TSharedPtr<IMediaRenderer, ESPMode::ThreadSafe> InRenderer) override;

		virtual void SetResourceDelegate(const TSharedPtr<IVideoDecoderResourceDelegate, ESPMode::ThreadSafe>& ResourceDelegate) override;

		virtual void AUdataPushAU(FAccessUnit* InAccessUnit) override;
		virtual void AUdataPushEOD() override;
		virtual void AUdataClearEOD() override;
		virtual void AUdataFlushEverything() override;

	protected:
		struct FDecoderOutputBuffer
		{
			FDecoderOutputBuffer()
			{
				FMemory::Memzero(mOutputStreamInfo);
				FMemory::Memzero(mOutputBuffer);
			}
			~FDecoderOutputBuffer()
			{
				if (mOutputBuffer.pSample)
				{
					mOutputBuffer.pSample->Release();
				}
			}
			TRefCountPtr<IMFSample> DetachOutputSample()
			{
				TRefCountPtr<IMFSample> pOutputSample;
				if (mOutputBuffer.pSample)
				{
					pOutputSample = TRefCountPtr<IMFSample>(mOutputBuffer.pSample, false);
					mOutputBuffer.pSample = nullptr;
				}
				return pOutputSample;
			}
			void PrepareForProcess()
			{
				mOutputBuffer.dwStatus = 0;
				mOutputBuffer.dwStreamID = 0;
				mOutputBuffer.pEvents = nullptr;
			}
			void UnprepareAfterProcess()
			{
				if (mOutputBuffer.pEvents)
				{
					// https://docs.microsoft.com/en-us/windows/desktop/api/mftransform/nf-mftransform-imftransform-processoutput
					// The caller is responsible for releasing any events that the MFT allocates.
					mOutputBuffer.pEvents->Release();
					mOutputBuffer.pEvents = nullptr;
				}
			}
			MFT_OUTPUT_STREAM_INFO	mOutputStreamInfo;
			MFT_OUTPUT_DATA_BUFFER	mOutputBuffer;
		};

		struct FDecoderInput
		{
			~FDecoderInput()
			{
				ReleasePayload();
			}
			void ReleasePayload()
			{
				FAccessUnit::Release(AccessUnit);
				AccessUnit = nullptr;
			}

			FAccessUnit*	AccessUnit = nullptr;
			bool			bHasBeenPrepared = false;
			bool			bIsIDR = false;
			bool			bIsDiscardable = false;
			int64			PTS = 0;
			int64			EndPTS = 0;
			FTimeValue		AdjustedPTS;
			FTimeValue		AdjustedDuration;

			TArray<MPEG::FISO14496_10_seq_parameter_set_data> SPSs;
		};

		struct FDecoderFormatInfo
		{
			void Reset()
			{
				CurrentCodecData.Reset();
			}
			void UpdateFromCSD(TSharedPtr<FDecoderInput, ESPMode::ThreadSafe> AU)
			{
				if (AU->AccessUnit->AUCodecData.IsValid() && AU->AccessUnit->AUCodecData.Get() != CurrentCodecData.Get())
				{
					// Pointers are different. Is the content too?
					bool bDifferent = !CurrentCodecData.IsValid() || (CurrentCodecData.IsValid() && AU->AccessUnit->AUCodecData->CodecSpecificData != CurrentCodecData->CodecSpecificData);
					if (bDifferent)
					{
						SPSs.Empty();
						TArray<MPEG::FNaluInfo>	NALUs;
						const uint8* pD = AU->AccessUnit->AUCodecData->CodecSpecificData.GetData();
						MPEG::ParseBitstreamForNALUs(NALUs, pD, AU->AccessUnit->AUCodecData->CodecSpecificData.Num());
						for(int32 i=0; i<NALUs.Num(); ++i)
						{
							const uint8* NALU = (const uint8*)Electra::AdvancePointer(pD, NALUs[i].Offset + NALUs[i].UnitLength);
							uint8 nal_unit_type = *NALU & 0x1f;
							// SPS?
							if (nal_unit_type == 7)
							{
								MPEG::FISO14496_10_seq_parameter_set_data sps;
								if (MPEG::ParseH264SPS(sps, NALU, NALUs[i].Size))
								{
									SPSs.Emplace(MoveTemp(sps));
								}
							}
						}
					}
					CurrentCodecData = AU->AccessUnit->AUCodecData;
				}
			}
			TSharedPtr<const FAccessUnit::CodecData, ESPMode::ThreadSafe> CurrentCodecData;
			TArray<MPEG::FISO14496_10_seq_parameter_set_data> SPSs;
		};

		void InternalDecoderDestroy();
		void DecoderCreate();
		bool DecoderSetInputType();
		bool DecoderSetOutputType();
		bool DecoderVerifyStatus();
		void StartThread();
		void StopThread();
		void WorkerThread();

		bool CreateDecodedImagePool();
		void DestroyDecodedImagePool();

		void NotifyReadyBufferListener(bool bHaveOutput);

		void SetupBufferAcquisitionProperties();

		bool AcquireOutputBuffer(bool bForNonDisplay);
		bool ConvertDecodedImage(const TRefCountPtr<IMFSample>& DecodedSample);
		bool FindAndUpdateDecoderInput(TSharedPtrTS<FDecoderInput>& OutMatchingInput, int64 InPTSFromDecoder);

		void PrepareAU(TSharedPtrTS<FDecoderInput> InAccessUnit);

		bool Decode(TSharedPtrTS<FDecoderInput> InAccessUnit, bool bResolutionChanged);
		bool PerformFlush();
		bool DecodeDummy(TSharedPtrTS<FDecoderInput> InAccessUnit);

		void ReturnUnusedFrame();

		void PostError(int32 ApiReturnValue, const FString& Message, uint16 Code, UEMediaError Error = UEMEDIA_ERROR_OK);
		void LogMessage(IInfoLog::ELevel Level, const FString& Message);
		bool FallbackToSwDecoding(FString Reason);
		bool ReconfigureForSwDecoding(FString Reason);
		bool Configure();
		bool StartStreaming();

		void HandleApplicationHasEnteredForeground();
		void HandleApplicationWillEnterBackground();

		// Per platform specialization
		virtual bool InternalDecoderCreate() = 0;
		virtual bool CreateDecoderOutputBuffer() = 0;
		virtual bool PreInitDecodeOutputForSW(const FIntPoint& Dim) = 0;
		virtual bool SetupDecodeOutputData(const FIntPoint& ImageDim, const TRefCountPtr<IMFSample>& DecodedOutputSample, FParamDict* OutputBufferSampleProperties) = 0;
		virtual void PlatformTick() {}

		FInstanceConfiguration								Config;

		FMediaEvent											ApplicationRunningSignal;
		FMediaEvent											ApplicationSuspendConfirmedSignal;

		FMediaEvent											TerminateThreadSignal;
		FMediaEvent											FlushDecoderSignal;
		FMediaEvent											DecoderFlushedSignal;
		bool												bThreadStarted;
		bool												bDrainForCodecChange;

		IPlayerSessionServices*								PlayerSessionServices;

		TSharedPtr<IMediaRenderer, ESPMode::ThreadSafe>		Renderer;

		TWeakPtr<IVideoDecoderResourceDelegate, ESPMode::ThreadSafe> ResourceDelegate;

		TAccessUnitQueue<TSharedPtrTS<FDecoderInput>>		NextAccessUnits;
		FStreamCodecInformation								NewSampleInfo;
		FStreamCodecInformation								CurrentSampleInfo;
		TSharedPtrTS<FDecoderInput>							CurrentAccessUnit;

		FMediaCriticalSection								ListenerMutex;
		IAccessUnitBufferListener*							InputBufferListener;
		IDecoderOutputBufferListener*						ReadyBufferListener;

		TRefCountPtr<IMFTransform>							DecoderTransform;
		TRefCountPtr<IMFMediaType>							CurrentOutputMediaType;
		MFT_OUTPUT_STREAM_INFO								DecoderOutputStreamInfo;
		bool												bIsHardwareAccelerated;
		bool												bRequiresReconfigurationForSW;
		int32												NumFramesInDecoder;
		bool												bDecoderFlushPending;
		bool												bError;
		TArray<TSharedPtrTS<FDecoderInput>>					InDecoderInput;

		FDecoderFormatInfo									CurrentStreamFormatInfo;
		MPEG::FColorimetryHelper							Colorimetry;

		TUniquePtr<FDecoderOutputBuffer>					CurrentDecoderOutputBuffer;
		IMediaRenderer::IBuffer*							CurrentRenderOutputBuffer;
		FParamDict											BufferAcquireOptions;
		bool												bHaveDecoder;
		int32												MaxDecodeBufferSize;

		FIntPoint											MaxDecodeDim;

	public:
		static FSystemConfiguration							SystemConfig;

#ifdef ELECTRA_ENABLE_SWDECODE
		static bool bDidCheckHWSupport;
		static bool bIsHWSupported;
#endif
	};

}
