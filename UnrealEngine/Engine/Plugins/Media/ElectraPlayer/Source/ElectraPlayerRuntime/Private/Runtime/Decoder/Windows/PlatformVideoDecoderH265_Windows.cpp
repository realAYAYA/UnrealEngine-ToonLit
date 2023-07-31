// Copyright Epic Games, Inc. All Rights Reserved.

#include "Decoder/Windows/PlatformVideoDecoderH265.h"

#if ELECTRA_PLATFORM_HAS_H265_DECODER
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/WindowsHWrapper.h"
#include "HAL/LowLevelMemTracker.h"

THIRD_PARTY_INCLUDES_START
#include "mftransform.h"
#include "mfapi.h"
#include "mferror.h"
#include "mfidl.h"
THIRD_PARTY_INCLUDES_END

#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace Electra
{
	#if ELECTRA_PLATFORM_HAS_H265_DECODER
	namespace
	{
		static bool HaveMFTHEVCDecoder()
		{
			static int32 HaveHEVCDecoder = -1;
			if (HaveHEVCDecoder < 0)
			{
				MFT_REGISTER_TYPE_INFO		MediaInputInfo { MFMediaType_Video , MFVideoFormat_HEVC };
				TRefCountPtr<IMFAttributes>	Attributes;
				TRefCountPtr<IMFTransform>	Decoder;
				IMFActivate**				ActivateObjects = nullptr;
				UINT32						NumActivateObjects = 0;
				HRESULT						res;

				HaveHEVCDecoder = 0;
				if (MFTEnumEx(MFT_CATEGORY_VIDEO_DECODER, MFT_ENUM_FLAG_SYNCMFT, &MediaInputInfo, nullptr, &ActivateObjects, &NumActivateObjects) == S_OK)
				{
					if (NumActivateObjects)
					{
						IMFTransform* NewDecoder = nullptr;
						res = ActivateObjects[0]->ActivateObject(IID_PPV_ARGS(&NewDecoder));
						if (res == S_OK && NewDecoder)
						{
							HaveHEVCDecoder = 1;
						}
						if (NewDecoder)
						{
							NewDecoder->Release();
						}
						for(UINT32 i=0; i<NumActivateObjects; ++i)
						{
							ActivateObjects[i]->Release();
						}
						CoTaskMemFree(ActivateObjects);
					}
				}
			}
			return HaveHEVCDecoder == 1;
		}
	}
	#endif


	bool FPlatformVideoDecoderH265::GetPlatformStreamDecodeCapability(IVideoDecoderH265::FStreamDecodeCapability& OutResult, const IVideoDecoderH265::FStreamDecodeCapability& InStreamParameter)
	{
		#if ELECTRA_PLATFORM_HAS_H265_DECODER
			bool bHaveMFTHEVCDecoder = HaveMFTHEVCDecoder();
			if (bHaveMFTHEVCDecoder)
			{
				// Set the output same as the input. Modifications are made below.
				OutResult = InStreamParameter;
				OutResult.DecoderSupportType = IVideoDecoderH265::FStreamDecodeCapability::ESupported::NotSupported;

				// Global query?
				if (InStreamParameter.Width <= 0 && InStreamParameter.Height <= 0)
				{
					OutResult.Width = 3840;
					OutResult.Height = 2160;
					OutResult.Profile = 1;
					OutResult.Level = 153;
					OutResult.FPS = 60.0;
					OutResult.DecoderSupportType = IVideoDecoderH265::FStreamDecodeCapability::ESupported::HardwareOnly;
				}
				else
				{
					// For now we handle only 8 bit encodings. If Main-10 profile, or only Main-10 profile compatibility is signaled we refuse this stream.
					if (InStreamParameter.Profile > 1 || (InStreamParameter.CompatibilityFlags & 0x60000000) == 0x20000000)
					{
						return true;
					}

					// TBD - What limits should apply here, exactly?
					if (InStreamParameter.FPS > 0.0 && InStreamParameter.FPS <= 60.0 &&
						InStreamParameter.Level <= 153)		// Level 5.1
					{
						int32 maxMB = (3840 / 16) * (2160 / 16) * 60;
						int32 numMB = (int32)(((InStreamParameter.Width + 15) / 16) * ((InStreamParameter.Height + 15) / 16) * (InStreamParameter.FPS > 0.0 ? InStreamParameter.FPS : 30.0));
						OutResult.DecoderSupportType = numMB <= maxMB ? IVideoDecoderH265::FStreamDecodeCapability::ESupported::HardwareOnly : IVideoDecoderH265::FStreamDecodeCapability::ESupported::NotSupported;
					}
				}
				return true;
			}
		#endif
		return false;
	}

} // namespace Electra
