// Copyright Epic Games, Inc. All Rights Reserved.

#include "Video/Encoders/VideoEncoderNVENC.h"
#include "Video/Encoders/Configs/VideoEncoderConfigAV1.h"
#include "Video/Encoders/Configs/VideoEncoderConfigH264.h"
#include "Video/Encoders/Configs/VideoEncoderConfigH265.h"
#include "Video/Resources/Vulkan/VideoResourceVulkan.h"

#if PLATFORM_WINDOWS
#include "Video/Resources/D3D/VideoResourceD3D.h"
#endif

class FNVENCModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		TFunction<bool(TSharedRef<FAVDevice> const&, GUID)> CheckCodecSupport = [](TSharedRef<FAVDevice> const& Device, GUID CodecGUID) {
			if(!Device->HasContext<FVideoContextCUDA>())
			{
				return false;
			}

			if(!FAPI::Get<FNVENC>().IsValid())
			{
				return false;
			}

			NV_ENC_STRUCT(NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS, SessionParams);
			SessionParams.apiVersion = NVENCAPI_VERSION;
			SessionParams.deviceType = NV_ENC_DEVICE_TYPE_CUDA;
			SessionParams.device = Device->GetContext<FVideoContextCUDA>()->Raw;

			void* Encoder = nullptr;
			NVENCSTATUS Result = FAPI::Get<FNVENC>().nvEncOpenEncodeSessionEx(&const_cast<NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS&>(SessionParams), &Encoder);
			if (Result != NV_ENC_SUCCESS)
			{
				return false;
			}		

			uint32 Count = 0;
			Result = FAPI::Get<FNVENC>().nvEncGetEncodeGUIDCount(Encoder, &Count);		
			if (Result != NV_ENC_SUCCESS || Count == 0)
			{
				return false;
			}

			TArray<GUID> GUIDs;
			GUIDs.SetNumZeroed(Count);
			Result = FAPI::Get<FNVENC>().nvEncGetEncodeGUIDs(Encoder, GUIDs.GetData(), Count, &Count);	
			if (Result != NV_ENC_SUCCESS)
			{
				return false;
			}

			bool bSupported = false;
			for (uint32 i = 0; i < Count; i++) 
			{
         		if (!FMemory::Memcmp(&GUIDs[i], &CodecGUID, sizeof(GUID))) 
				{
	        		bSupported = true;
					break;
         		}
			}

			Result = FAPI::Get<FNVENC>().nvEncDestroyEncoder(Encoder);
			if (Result != NV_ENC_SUCCESS)
			{
				return false;
			}

			return bSupported;
		};

		/* Register NVENC+CUDA H264 pathway for D3D12 and Vulkan. */
		FVideoEncoder
			::RegisterPermutationsOf<FVideoEncoderNVENCCUDA>
			::With<
				FVideoResourceCUDA,
#if PLATFORM_WINDOWS
				FVideoResourceD3D12,
#endif
				FVideoResourceVulkan>
			::And<FVideoEncoderConfigNVENC, FVideoEncoderConfigH264>(
				[CheckCodecSupport](TSharedRef<FAVDevice> const& NewDevice, TSharedRef<FAVInstance> const& NewInstance)
				{
					// static local because we don't want to constantly create new encoder sessions every time we query codec support
					static bool bSupportsCodec = CheckCodecSupport(NewDevice, NV_ENC_CODEC_H264_GUID);

					return FAPI::Get<FNVENC>().IsValid() && NewDevice->HasContext<FVideoContextCUDA>() && bSupportsCodec;
				});

		/* Register NVENC+CUDA H265 pathway for D3D12 and Vulkan. */
		FVideoEncoder
			::RegisterPermutationsOf<FVideoEncoderNVENCCUDA>
			::With<
				FVideoResourceCUDA,
#if PLATFORM_WINDOWS
				FVideoResourceD3D12,
#endif
				FVideoResourceVulkan>
			::And<FVideoEncoderConfigNVENC, FVideoEncoderConfigH265>(
				[CheckCodecSupport](TSharedRef<FAVDevice> const& NewDevice, TSharedRef<FAVInstance> const& NewInstance)
				{
					// static local because we don't want to constantly create new encoder sessions every time we query codec support
					static bool bSupportsCodec = CheckCodecSupport(NewDevice, NV_ENC_CODEC_HEVC_GUID);

					return FAPI::Get<FNVENC>().IsValid() && NewDevice->HasContext<FVideoContextCUDA>() && bSupportsCodec;
				});

		/* Register NVENC+CUDA AV1 pathway for D3D12 and Vulkan. */
		FVideoEncoder
			::RegisterPermutationsOf<FVideoEncoderNVENCCUDA>
			::With<
				FVideoResourceCUDA,
#if PLATFORM_WINDOWS
				FVideoResourceD3D12,
#endif
				FVideoResourceVulkan>
			::And<FVideoEncoderConfigNVENC, FVideoEncoderConfigAV1>(
				[CheckCodecSupport](TSharedRef<FAVDevice> const& NewDevice, TSharedRef<FAVInstance> const& NewInstance)
				{
					// static local because we don't want to constantly create new encoder sessions every time we query codec support
					static bool bSupportsCodec = CheckCodecSupport(NewDevice, NV_ENC_CODEC_AV1_GUID);

					return FAPI::Get<FNVENC>().IsValid() && NewDevice->HasContext<FVideoContextCUDA>() && bSupportsCodec;
				});

		/* Register NVENC + raw DX11 graphics device pathway for D3D11. 
		* The reason we seperate these out in this way is because D3D11 + CUDA/NVENC does not encode UE textures properly due to how they are laid out.
		*/
#if PLATFORM_WINDOWS
		FVideoEncoder
			::RegisterPermutationsOf<FVideoEncoderNVENCD3D11>
			::With<FVideoResourceD3D11>
			::And<FVideoEncoderConfigNVENC, FVideoEncoderConfigH264>(
				[CheckCodecSupport](TSharedRef<FAVDevice> const& NewDevice, TSharedRef<FAVInstance> const& NewInstance)
				{
					// static local because we don't want to constantly create new encoder sessions every time we query codec support
					static bool bSupportsCodec = CheckCodecSupport(NewDevice, NV_ENC_CODEC_H264_GUID);

					return FAPI::Get<FNVENC>().IsValid() && NewDevice->HasContext<FVideoContextD3D11>() && bSupportsCodec;
				});

		FVideoEncoder
			::RegisterPermutationsOf<FVideoEncoderNVENCD3D11>
			::With<FVideoResourceD3D11>
			::And<FVideoEncoderConfigNVENC, FVideoEncoderConfigH265>(
				[CheckCodecSupport](TSharedRef<FAVDevice> const& NewDevice, TSharedRef<FAVInstance> const& NewInstance)
				{
					// static local because we don't want to constantly create new encoder sessions every time we query codec support
					static bool bSupportsCodec = CheckCodecSupport(NewDevice, NV_ENC_CODEC_HEVC_GUID);

					return FAPI::Get<FNVENC>().IsValid() && NewDevice->HasContext<FVideoContextD3D11>() && bSupportsCodec;
				});

		FVideoEncoder
			::RegisterPermutationsOf<FVideoEncoderNVENCD3D11>
			::With<FVideoResourceD3D11>
			::And<FVideoEncoderConfigNVENC, FVideoEncoderConfigAV1>(
				[CheckCodecSupport](TSharedRef<FAVDevice> const& NewDevice, TSharedRef<FAVInstance> const& NewInstance)
				{
					// static local because we don't want to constantly create new encoder sessions every time we query codec support
					static bool bSupportsCodec = CheckCodecSupport(NewDevice, NV_ENC_CODEC_AV1_GUID);

					return FAPI::Get<FNVENC>().IsValid() && NewDevice->HasContext<FVideoContextD3D11>() && bSupportsCodec;
				});
#endif // PLATFORM_WINDOWS
	
	}
};

IMPLEMENT_MODULE(FNVENCModule, NVENC);
