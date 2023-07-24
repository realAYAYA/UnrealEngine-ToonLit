// Copyright Epic Games, Inc. All Rights Reserved.

#include "Video/Decoders/VideoDecoderNVDEC.h"
#include "Video/Decoders/Configs/VideoDecoderConfigH264.h"
#include "Video/Decoders/Configs/VideoDecoderConfigH265.h"
#include "Video/Resources/VideoResourceVulkan.h"

#if PLATFORM_WINDOWS
#include "Video/Resources/Windows/VideoResourceD3D.h"
#endif

class FNVDECModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FVideoDecoder
			::RegisterPermutationsOf<FVideoDecoderNVDEC>
			::With<
				FVideoResourceCUDA,
#if PLATFORM_WINDOWS
				FVideoResourceD3D11, 
				FVideoResourceD3D12, 
#endif
				FVideoResourceVulkan>
			::And<FVideoDecoderConfigNVDEC, FVideoDecoderConfigH264, FVideoDecoderConfigH265>(
				[](TSharedRef<FAVDevice> const& NewDevice, TSharedRef<FAVInstance> const& NewInstance)
				{
					return FAPI::Get<FNVDEC>().IsValid() && NewDevice->HasContext<FVideoContextCUDA>();
				});
	}
};

IMPLEMENT_MODULE(FNVDECModule, NVDEC);
