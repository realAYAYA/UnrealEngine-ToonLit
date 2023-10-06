// Copyright Epic Games, Inc. All Rights Reserved.

#include "VideoDecoderInputBitstreamProcessor.h"
#include "BitstreamProcessors/VideoDecoderInputBitstreamProcessorH264.h"
#include "BitstreamProcessors/VideoDecoderInputBitstreamProcessorH265.h"
#include "BitstreamProcessors/VideoDecoderInputBitstreamProcessorVP8.h"
#include "BitstreamProcessors/VideoDecoderInputBitstreamProcessorVP9.h"
#include "IElectraDecoderOutputVideo.h"
#include "Decoder/VideoDecoderHelpers.h"
#include "StreamAccessUnitBuffer.h"

namespace Electra
{


class FVideoDecoderInputBitstreamProcessorNull : public IVideoDecoderInputBitstreamProcessor
{
public:
	virtual ~FVideoDecoderInputBitstreamProcessorNull() = default;
	void Clear() override
	{
		// Do NOT reset colorimetry or HDR info here.
		// They are one-time initialized only.
	}
	EProcessResult ProcessAccessUnitForDecoding(FBitstreamInfo& OutBSI, FAccessUnit* InOutAccessUnit) override
	{ 
		OutBSI.bIsSyncFrame = InOutAccessUnit->bIsSyncSample;
		OutBSI.bIsDiscardable = false;
		return EProcessResult::None; 
	}
	void SetPropertiesOnOutput(TSharedPtr<IElectraDecoderVideoOutput, ESPMode::ThreadSafe> InDecoderOutput, FParamDict* InOutProperties, const FBitstreamInfo& InFromBSI) override
	{
		Colorimetry.Update(COLRBox);
		HDR.UpdateFromMPEGBoxes(InDecoderOutput.IsValid() ? InDecoderOutput->GetNumberOfBits() : 0, Colorimetry, MDCVBox, CLLIBox);

		Colorimetry.UpdateParamDict(*InOutProperties);
		HDR.UpdateParamDict(*InOutProperties);
	}

	void SetColorimetryFromCOLRBox(const TArray<uint8>& InCOLRBox)
	{
		COLRBox = InCOLRBox;
	}
	void SetMasteringDisplayColorVolumeFromMDCVBox(const TArray<uint8>& InMDCVBox)
	{
		MDCVBox = InMDCVBox;
	}
	void SetContentLightLevelInfoFromCLLIBox(const TArray<uint8>& InCLLIBox)
	{
		CLLIBox = InCLLIBox;
	}
	void SetContentLightLevelFromCOLLBox(const TArray<uint8>& InCOLLBox)
	{
		if (InCOLLBox.Num() > 4)
		{
			uint8 BoxVersion = InCOLLBox[0];
			if (BoxVersion == 0)
			{
				// 'clli' box is the same as a version 0 'COLL 'coll' box.
				CLLIBox = TArray<uint8>(InCOLLBox.GetData() + 4, InCOLLBox.Num() - 4);
			}
		}
	}

private:
	TArray<uint8> COLRBox;
	TArray<uint8> MDCVBox;
	TArray<uint8> CLLIBox;

	MPEG::FColorimetryHelper Colorimetry;
	MPEG::FHDRHelper HDR;
};





TSharedPtr<IVideoDecoderInputBitstreamProcessor, ESPMode::ThreadSafe> IVideoDecoderInputBitstreamProcessor::Create(const FString& InCodec, const TMap<FString, FVariant>& InDecoderConfigOptions)
{
	auto GetVariantValueUInt8Array = [](const TMap<FString, FVariant>& InFromMap, const FString& InName) ->TArray<uint8>
	{
		const FVariant* Var = InFromMap.Find(InName);
		if (Var)
		{
			if (Var->GetType() == EVariantTypes::ByteArray)
			{
				return Var->GetValue<TArray<uint8>>();
			}
		}
		return TArray<uint8>();
	};


	if (InCodec.StartsWith(TEXT("avc")))
	{
		return IVideoDecoderInputBitstreamProcessorH264::Create(InCodec, InDecoderConfigOptions);
	}
	else if (InCodec.StartsWith(TEXT("hvc1")) || InCodec.StartsWith(TEXT("hev1")))
	{
		return IVideoDecoderInputBitstreamProcessorH265::Create(InCodec, InDecoderConfigOptions);
	}
	else if (InCodec.StartsWith(TEXT("vp08")))
	{
		return IVideoDecoderInputBitstreamProcessorVP8::Create(InCodec, InDecoderConfigOptions);
	}
	else if (InCodec.StartsWith(TEXT("vp09")))
	{
		return IVideoDecoderInputBitstreamProcessorVP9::Create(InCodec, InDecoderConfigOptions);
	}

	TSharedPtr<FVideoDecoderInputBitstreamProcessorNull, ESPMode::ThreadSafe> bsp = MakeShared<FVideoDecoderInputBitstreamProcessorNull, ESPMode::ThreadSafe>();
	bsp->SetColorimetryFromCOLRBox(GetVariantValueUInt8Array(InDecoderConfigOptions, TEXT("$colr_box")));
	bsp->SetMasteringDisplayColorVolumeFromMDCVBox(GetVariantValueUInt8Array(InDecoderConfigOptions, TEXT("$mdcv_box")));
	bsp->SetContentLightLevelFromCOLLBox(GetVariantValueUInt8Array(InDecoderConfigOptions, TEXT("$coll_box")));
	bsp->SetContentLightLevelInfoFromCLLIBox(GetVariantValueUInt8Array(InDecoderConfigOptions, TEXT("$clli_box")));
	return bsp;
}

} // namespace Electra
