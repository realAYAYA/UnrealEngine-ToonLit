// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Misc/App.h"

#include "Video/Encoders/VideoEncoderVT.h"
#include "Video/Encoders/Configs/VideoEncoderConfigH264.h"
#include "Video/Encoders/Configs/VideoEncoderConfigH265.h"

#include "Video/Decoders/VideoDecoderVT.h"
#include "Video/Decoders/Configs/VideoDecoderConfigVP9.h"
#include "Video/Decoders/Configs/VideoDecoderConfigH264.h"
#include "Video/Decoders/Configs/VideoDecoderConfigH265.h"

#include "Video/Resources/Metal/VideoResourceMetal.h"

THIRD_PARTY_INCLUDES_START
#include <VideoToolbox/VideoToolbox.h>
THIRD_PARTY_INCLUDES_END

class FVTModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{       
        TFunction<bool(TSharedRef<FAVDevice> const&, const TCHAR*)> CheckCodecSupport = [](TSharedRef<FAVDevice> const& Device, const TCHAR* Codec) {
			CFStringRef TargetCodec = CFStringCreateWithCString(kCFAllocatorDefault, TCHAR_TO_ANSI(Codec), kCFStringEncodingUTF8);
            CFArrayRef EncoderList = NULL;
            bool bSupportsHardware = false;

            OSStatus Status = VTCopyVideoEncoderList(NULL, &EncoderList);
            if (Status != 0) 
            {
                return false;
            }

            CFIndex EncoderCount = CFArrayGetCount(EncoderList);
            for (CFIndex i = 0; i < EncoderCount; i++) 
            {
				CFDictionaryRef EncoderDict = (CFDictionaryRef)CFArrayGetValueAtIndex(EncoderList, i);
                if (EncoderDict)
                {
					CFStringRef EncoderCodec = (CFStringRef)CFDictionaryGetValue(EncoderDict, kVTVideoEncoderList_CodecName);
		 			if (EncoderCodec != TargetCodec)
					{
						continue;
					}
					
                    CFBooleanRef bIsHardware = (CFBooleanRef)CFDictionaryGetValue(EncoderDict, kVTVideoEncoderList_IsHardwareAccelerated);
					bSupportsHardware |= bIsHardware ? (bool)CFBooleanGetValue(bIsHardware) : false;
                }
            }

            if (EncoderList) 
            {
                CFRelease(EncoderList);
            }
			
			if(TargetCodec)
			{
				CFRelease(TargetCodec);
			}

			return bSupportsHardware;
		};

        FVideoEncoder::RegisterPermutationsOf<TVideoEncoderVT<FVideoResourceMetal>>
            ::With<FVideoResourceMetal>
            ::And<FVideoEncoderConfigVT, FVideoEncoderConfigH264>(
                [CheckCodecSupport](TSharedRef<FAVDevice> const& NewDevice, TSharedRef<FAVInstance> const& NewInstance)
                {
					static bool bSupportsCodec = CheckCodecSupport(NewDevice, TEXT("H.264"));

                    return FAPI::Get<FVT>().IsValid() && bSupportsCodec;
                });

        FVideoEncoder::RegisterPermutationsOf<TVideoEncoderVT<FVideoResourceMetal>>
            ::With<FVideoResourceMetal>
            ::And<FVideoEncoderConfigVT, FVideoEncoderConfigH265>(
                [CheckCodecSupport](TSharedRef<FAVDevice> const& NewDevice, TSharedRef<FAVInstance> const& NewInstance)
                {
					static bool bSupportsCodec = CheckCodecSupport(NewDevice, TEXT("HEVC"));

                    return FAPI::Get<FVT>().IsValid() && bSupportsCodec;
                });

        VTRegisterSupplementalVideoDecoderIfAvailable(kCMVideoCodecType_VP9);

        if(VTIsHardwareDecodeSupported(kCMVideoCodecType_H264))
        {
            FVideoDecoder::RegisterPermutationsOf<TVideoDecoderVT<FVideoResourceMetal>>
                ::With<FVideoResourceMetal>
                ::And<FVideoDecoderConfigVT, FVideoDecoderConfigH264>(
                    [](TSharedRef<FAVDevice> const& NewDevice, TSharedRef<FAVInstance> const& NewInstance)
                    {
                        return FAPI::Get<FVT>().IsValid();
                    });
        }

        if(VTIsHardwareDecodeSupported(kCMVideoCodecType_HEVC))
        {
            FVideoDecoder::RegisterPermutationsOf<TVideoDecoderVT<FVideoResourceMetal>>
                ::With<FVideoResourceMetal>
                ::And<FVideoDecoderConfigVT, FVideoDecoderConfigH265>(
                    [](TSharedRef<FAVDevice> const& NewDevice, TSharedRef<FAVInstance> const& NewInstance)
                    {
                        return FAPI::Get<FVT>().IsValid();
                    });
        }

        if(VTIsHardwareDecodeSupported(kCMVideoCodecType_VP9))
        {
            FVideoDecoder::RegisterPermutationsOf<TVideoDecoderVT<FVideoResourceMetal>>
                ::With<FVideoResourceMetal>
                ::And<FVideoDecoderConfigVT, FVideoDecoderConfigVP9>(
                    [](TSharedRef<FAVDevice> const& NewDevice, TSharedRef<FAVInstance> const& NewInstance)
                    {
                        return FAPI::Get<FVT>().IsValid();
                    });
        }
	}
};

IMPLEMENT_MODULE(FVTModule, VTCodecs);
