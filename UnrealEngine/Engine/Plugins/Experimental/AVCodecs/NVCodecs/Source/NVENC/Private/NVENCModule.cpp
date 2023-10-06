// Copyright Epic Games, Inc. All Rights Reserved.

#include "Video/Encoders/VideoEncoderNVENC.h"
#include "Video/Encoders/Configs/VideoEncoderConfigH264.h"
#include "Video/Encoders/Configs/VideoEncoderConfigH265.h"
#include "Video/Resources/VideoResourceVulkan.h"

#if PLATFORM_WINDOWS
#include "Video/Resources/Windows/VideoResourceD3D.h"
#endif

class FNVENCModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{

		/* Register NVENC+CUDA pathway for D3D12 and Vulkan. */
		FVideoEncoder
			::RegisterPermutationsOf<FVideoEncoderNVENCCUDA>
			::With<
				FVideoResourceCUDA,
#if PLATFORM_WINDOWS
				FVideoResourceD3D12,
#endif
				FVideoResourceVulkan>
			::And<FVideoEncoderConfigNVENC, FVideoEncoderConfigH264, FVideoEncoderConfigH265>(
				[](TSharedRef<FAVDevice> const& NewDevice, TSharedRef<FAVInstance> const& NewInstance)
				{
					return FAPI::Get<FNVENC>().IsValid() && NewDevice->HasContext<FVideoContextCUDA>();
				});

		/* Register NVENC + raw DX11 graphics device pathway for D3D11. 
		* The reason we seperate these out in this way is because D3D11 + CUDA/NVENC does not encode UE textures properly due to how they are laid out.
		*/
#if PLATFORM_WINDOWS
		FVideoEncoder
			::RegisterPermutationsOf<FVideoEncoderNVENCD3D11>
			::With<FVideoResourceD3D11>
			::And<FVideoEncoderConfigNVENC, FVideoEncoderConfigH264, FVideoEncoderConfigH265>(
				[](TSharedRef<FAVDevice> const& NewDevice, TSharedRef<FAVInstance> const& NewInstance)
				{
					return FAPI::Get<FNVENC>().IsValid() && NewDevice->HasContext<FVideoContextD3D11>();
				});
#endif // PLATFORM_WINDOWS
	
	}
};

IMPLEMENT_MODULE(FNVENCModule, NVENC);
