// Copyright Epic Games, Inc. All Rights Reserved.

#include "Video/Decoders/VideoDecoderNVDEC.h"

#include "HAL/PlatformProcess.h"

#include "NV12_to_BGRA8.cuh"
#include "P010_to_ABGR10.cuh"

FVideoDecoderNVDEC::~FVideoDecoderNVDEC()
{
	Close();
}

bool FVideoDecoderNVDEC::IsOpen() const
{
	return bIsOpen;
}

FAVResult FVideoDecoderNVDEC::Open(TSharedRef<FAVDevice> const& NewDevice, TSharedRef<FAVInstance> const& NewInstance)
{
	Close();

	TVideoDecoder::Open(NewDevice, NewInstance);

	bIsOpen = true;

	return EAVResult::Success;
}

void FVideoDecoderNVDEC::Close()
{
	if (Decoder != nullptr)
	{
		FCUDAContextScope const ContextGuard(GetDevice()->GetContext<FVideoContextCUDA>()->Raw);

		CUresult const Result = FAPI::Get<FNVDEC>().cuvidDestroyDecoder(Decoder);
		if (Result != CUDA_SUCCESS)
		{
			FAVResult::Log(EAVResult::ErrorDestroying, TEXT("Failed to destroy NVENC encoder"), TEXT("NVDEC"), Result);
		}

		Decoder = nullptr;
	}

	bIsOpen = false;
}

bool FVideoDecoderNVDEC::IsInitialized() const
{
	return IsOpen() && Decoder != nullptr;
}

FAVResult FVideoDecoderNVDEC::ApplyConfig()
{
	if (IsOpen())
	{
		FVideoDecoderConfigNVDEC const& PendingConfig = GetPendingConfig();
		if (AppliedConfig != PendingConfig)
		{
			if (IsInitialized())
			{
				/*
				TODO(Andrew) Can be reconfigured? See https://docs.nvidia.com/video-technologies/video-codec-sdk/nvenc-video-encoder-api-prog-guide/#reconfigure-api
				if (AppliedConfig.maxEncodeWidth == PendingConfig.maxEncodeWidth
					&& AppliedConfig.maxEncodeHeight == PendingConfig.maxEncodeHeight
					&& AppliedConfig.enablePTD == PendingConfig.enablePTD
					&& AppliedConfig.enableEncodeAsync == PendingConfig.enableEncodeAsync
					&& AppliedConfig.encodeConfig->gopLength == PendingConfig.encodeConfig->gopLength
					&& AppliedConfig.encodeConfig->frameIntervalP == PendingConfig.encodeConfig->frameIntervalP
					&& AppliedConfig.encodeConfig->encodeCodecConfig.h264Config.idrPeriod == PendingConfig.encodeConfig->encodeCodecConfig.h264Config.idrPeriod)
				{
					NV_ENC_STRUCT(NV_ENC_RECONFIGURE_PARAMS, ReconfigureParams);
					FMemory::Memcpy(&ReconfigureParams.reInitEncodeParams, &PendingConfig, sizeof(NV_ENC_INITIALIZE_PARAMS));
					ReconfigureParams.forceIDR = AppliedConfig.encodeWidth != PendingConfig.encodeWidth || AppliedConfig.encodeHeight != PendingConfig.encodeHeight;

					NVENCSTATUS const Result = FAPI::Get<FNVENC_Context>().nvEncReconfigureEncoder(Encoder, &ReconfigureParams);
					if (Result != NV_ENC_SUCCESS)
					{
						return FAVResult(EAVResult::Error, FString::Printf(TEXT("Failed to update NVENC encoder configuration: %s"), *FAPI::Get<FNVENC_Context>().GetErrorString(Encoder, Result)));
					}
				}
				else
				{
					// TODO: Destroy and recreate with original session
					unimplemented();
				}
				*/
			}

			if (!IsInitialized())
			{
				FCUDAContextScope const ContextGuard(GetDevice()->GetContext<FVideoContextCUDA>()->Raw);

				CUresult const Result = FAPI::Get<FNVDEC>().cuvidCreateDecoder(&Decoder, &const_cast<FVideoDecoderConfigNVDEC&>(PendingConfig));
				if (Result != CUDA_SUCCESS)
				{
					Close();

					return FAVResult(EAVResult::ErrorCreating, TEXT("Failed to create decoder"), TEXT("NVDEC"), Result);
				}
			}
		}

		return TVideoDecoder::ApplyConfig();
	}

	return FAVResult(EAVResult::ErrorInvalidState, TEXT("Decoder not open"), TEXT("NVDEC"));
}

bool FVideoDecoderNVDEC::GetCapability(CUVIDDECODECAPS& CapsToQuery) const
{
	if (IsOpen())
	{
		FCUDAContextScope const ContextGuard(GetDevice()->GetContext<FVideoContextCUDA>()->Raw);

		CUresult const Result = FAPI::Get<FNVDEC>().cuvidGetDecoderCaps(&CapsToQuery);
		if (Result != CUDA_SUCCESS)
		{
			FAVResult::Log(EAVResult::Warning, TEXT("Failed to query for NVENC capability"), TEXT("NVDEC"), Result);

			return false;
		}

		return CapsToQuery.bIsSupported > 0;
	}

	return FAVResult(EAVResult::ErrorInvalidState, TEXT("Decoder not open"), TEXT("NVDEC"));
}

FAVResult FVideoDecoderNVDEC::SendPacket(FVideoPacket const& Packet)
{
	if (!IsOpen())
	{
		return FAVResult(EAVResult::ErrorInvalidState, TEXT("Decoder not open"), TEXT("NVDEC"));
	}

	FVideoDecoderConfigNVDEC& PendingConfig = EditPendingConfig();

	TArray<FVideoDecoderConfigNVDEC::FParsedPicture> Pictures;

	FAVResult Result = PendingConfig.Parse(GetInstance().ToSharedRef(), Packet, Pictures);
	if (Result.IsNotSuccess())
	{
		return Result;
	}

	if (Pictures.Num() == 0)
	{
		return EAVResult::PendingInput;
	}

	for (auto& Picture : Pictures)
	{
		if (FramesCount + 1 >= static_cast<int64>(Picture.DecodeCreateInfo.ulNumDecodeSurfaces))
		{
			return FAVResult(EAVResult::PendingOutput, TEXT("Decode buffer full"), TEXT("NVDEC"));
		}

		CUVIDPROCPARAMS MapParams = {};

		FMemory::Memcpy(&static_cast<CUVIDDECODECREATEINFO&>(PendingConfig), &Picture.DecodeCreateInfo, sizeof(CUVIDDECODECREATEINFO));

		Result = ApplyConfig();
		if (Result.IsNotSuccess())
		{
			return Result;
		}

		CUresult const CUResult = FAPI::Get<FNVDEC>().cuvidDecodePicture(Decoder, &Picture);
		if (CUResult != CUDA_SUCCESS)
		{
			return FAVResult(EAVResult::Error, TEXT("Failed to decode frame"), TEXT("NVDEC"), CUResult);
		}

		MapParams.progressive_frame = PendingConfig.DeinterlaceMode;
		MapParams.second_field = Picture.second_field;
		MapParams.top_field_first = Picture.bottom_field_flag != 0;

		Frames.Enqueue({ Picture.CurrPicIdx, AppliedConfig.ulTargetWidth, AppliedConfig.ulTargetHeight, GetAppliedConfig().OutputFormat, MapParams });
		++FramesCount;
	}

	return EAVResult::Success;
}

FAVResult FVideoDecoderNVDEC::ReceiveFrame(TResolvableVideoResource<FVideoResourceCUDA>& InOutResource)
{
	if (IsOpen())
	{
		FFrame Frame;
		if (Frames.Peek(Frame))
		{
			FCUDAContextScope const ContextGuard(GetDevice()->GetContext<FVideoContextCUDA>()->Raw);

			CUresult Result = CUDA_SUCCESS;
			CUVIDGETDECODESTATUS DecodeStatus = {};

			do
			{
				Result = FAPI::Get<FNVDEC>().cuvidGetDecodeStatus(Decoder, Frame.SurfaceIndex, &DecodeStatus);
				if (Result != CUDA_SUCCESS)
				{
					return FAVResult(EAVResult::Error, TEXT("Could not retrieve decoder status"), TEXT("NVDEC"));
				}
				FPlatformProcess::Sleep(0.5f / 1000.0f); // HACK (aidan.possemiers) surely we can avoid a busy wait here
			} while (DecodeStatus.decodeStatus == cuvidDecodeStatus_InProgress);

			if (DecodeStatus.decodeStatus < cuvidDecodeStatus_Success)
			{
				return FAVResult(EAVResult::PendingInput, TEXT("Send more frames"), TEXT("NVDEC"), DecodeStatus.decodeStatus);
			}

			Frames.Pop();
			--FramesCount;

			if (DecodeStatus.decodeStatus > cuvidDecodeStatus_Success)
			{
				FAVResult::Log(EAVResult::Warning, TEXT("Error when decoding frame"), TEXT("NVDEC"), DecodeStatus.decodeStatus);
			}
			
			// Map the decoder output buffer
			CUdeviceptr MapSurface = 0;
			uint32 MapPitch = 0;

			Result = FAPI::Get<FNVDEC>().cuvidMapVideoFrame(Decoder, Frame.SurfaceIndex, &MapSurface, &MapPitch, &Frame.MapParams);
			if (Result != CUDA_SUCCESS)
			{
				return FAVResult(EAVResult::ErrorMapping, TEXT("Failed to map decoded frame"), TEXT("NVDEC"), Result);
			}

			FVideoDescriptor ResourceDescriptor = {};

#if 0 // Old path with automatic conversion
			TFunction<CUresult(CUdeviceptr, CUarray, uint32, uint32, uint32)> ConversionFunction;

			// TODO (aidan) handle conversion to non-RGB textures
			switch (Frame.SurfaceFormat)
			{
			case cudaVideoSurfaceFormat_NV12:
				ResourceDescriptor = FVideoDescriptor(EVideoFormat::BGRA, Frame.Width, Frame.Height);
				ConversionFunction = NV12_to_BGRA8;
				break;
			case cudaVideoSurfaceFormat_P016:
				ResourceDescriptor = FVideoDescriptor(EVideoFormat::ABGR10, Frame.Width, Frame.Height);
				ConversionFunction = P010_to_ABGR10; // TODO
				break;
			/*case cudaVideoSurfaceFormat_YUV444:
				ResourceDescriptor = FVideoDescriptor(EVideoFormat::YUV444, Frame.Width, Frame.Height);
				break;
			case cudaVideoSurfaceFormat_YUV444_16Bit:
				ResourceDescriptor = FVideoDescriptor(EVideoFormat::YUV444_16, Frame.Width, Frame.Height);*/
			default:
				return FAVResult(EAVResult::ErrorUnsupported, FString::Printf(TEXT("Currently do not support the recieved OutputFormat (%d)"), Frame.SurfaceFormat));
			}

			if (!InOutResource.Resolve(GetDevice(), ResourceDescriptor))
			{
				return FAVResult(EAVResult::ErrorResolving, TEXT("Failed to resolve frame resource"), TEXT("NVDEC"));
			}

			InOutResource->Lock();

			Result = ConversionFunction(MapSurface, InOutResource->GetRaw(), InOutResource->GetWidth(), InOutResource->GetHeight(), MapPitch);

			InOutResource->Unlock();

			if (Result != CUDA_SUCCESS)
			{
				return FAVResult(EAVResult::Error, TEXT("Failed to run CUDA kernal on decoded frame"), TEXT("NVDEC"), Result);
			}

			// START DEBUG

#if DEBUG_DUMP_TO_DISK

			TArray64<uint8> OutData;

			InOutResource->ReadData(OutData);

			FString SaveName = FString::Printf(TEXT("%s/DumpOutput/image%05d.%s"), *FPaths::ProjectSavedDir(), Frame.SurfaceIndex, Frame.SurfaceFormat == cudaVideoSurfaceFormat_NV12 ? TEXT("nv12") : TEXT("p016"));

			FFileHelper::SaveArrayToFile(OutData, *SaveName);

#endif

			// END DEBUG
#else // new code path just returns in the format that the decoder outputs and is up to the user to transform it
			switch (Frame.SurfaceFormat)
			{
				case cudaVideoSurfaceFormat_NV12:
					ResourceDescriptor = FVideoDescriptor(EVideoFormat::NV12, Frame.Width, Frame.Height);
					break;
				case cudaVideoSurfaceFormat_P016:
					ResourceDescriptor = FVideoDescriptor(EVideoFormat::P010, Frame.Width, Frame.Height);
					break;
				case cudaVideoSurfaceFormat_YUV444:
					ResourceDescriptor = FVideoDescriptor(EVideoFormat::YUV444, Frame.Width, Frame.Height);
					break;
				case cudaVideoSurfaceFormat_YUV444_16Bit:
					ResourceDescriptor = FVideoDescriptor(EVideoFormat::YUV444_16, Frame.Width, Frame.Height);
					break;
				default:
					return FAVResult(EAVResult::ErrorUnsupported, FString::Printf(TEXT("Currently do not support the recieved OutputFormat (%d)"), Frame.SurfaceFormat));
			}

			if (!InOutResource.Resolve(GetDevice(), ResourceDescriptor))
			{
				return FAVResult(EAVResult::ErrorResolving, TEXT("Failed to resolve frame resource"), TEXT("NVDEC"));
			}

			// Do copy into the VideoResource
			InOutResource->CopyFrom(MapSurface, MapPitch);

			// TODO (aidan.possemiers) this is messy as we may unmap the texture before the copy is done
			// from what I can see there is no way to easily unmap this one the copy is done
			Result = FAPI::Get<FNVDEC>().cuvidUnmapVideoFrame(Decoder, MapSurface);
			if (Result != CUDA_SUCCESS)
			{
				return FAVResult(EAVResult::ErrorUnmapping, TEXT("Failed to unmap decoded frame"), TEXT("NVDEC"), Result);
			}
			

#endif // end else !0		

			return EAVResult::Success;
		}

		return EAVResult::PendingInput;
	}

	return FAVResult(EAVResult::ErrorInvalidState, TEXT("Decoder not open"), TEXT("NVDEC"));
}