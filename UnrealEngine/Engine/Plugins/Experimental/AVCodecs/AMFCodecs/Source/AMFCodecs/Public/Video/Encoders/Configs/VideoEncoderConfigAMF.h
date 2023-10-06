// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AVExtension.h"
#include "Video/VideoEncoder.h"

#include "AMF.h"

struct AMFCODECS_API FVideoEncoderConfigAMF : public FAVConfig
{
private:
	TMap<wchar_t const*, amf::AMFVariantStruct, FDefaultSetAllocator, TStringPointerMapKeyFuncs_DEPRECATED<wchar_t const*, amf::AMFVariantStruct>> Variants;

public:
	static TAVResult<AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_ENUM> ConvertRateControlMode(ERateControlMode Mode);
	static TAVResult<ERateControlMode> ConvertRateControlMode(AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_ENUM Mode);

	static FName const CodecTypeH264;
	static FName const CodecTypeH265;

	FName CodecType;

	uint32 Width = 1920;
	uint32 Height = 1080;

	bool RepeatSPSPPS = false;

	FVideoEncoderConfigAMF(EAVPreset Preset = EAVPreset::Default)
		: FAVConfig(Preset)
	{
	}

	bool operator==(FVideoEncoderConfigAMF const& Other) const
	{
		if (CodecType == Other.CodecType && Width == Other.Width && Height == Other.Height)
		{
			if (Variants.Num() != Other.Variants.Num())
			{
				return false;
			}

			for (TPair<wchar_t const*, amf::AMFVariantStruct> const& Variant : Variants)
			{
				amf::AMFVariantStruct const* OtherVariantValue = Other.Variants.Find(Variant.Key);
				if (OtherVariantValue == nullptr)
				{
					return false;
				}

				bool IsEqual = false;
				AMFVariantCompare(&Variant.Value, OtherVariantValue, &IsEqual);

				if (!IsEqual)
				{
					return false;
				}
			}
		}

		return false;
	}

	bool operator!=(FVideoEncoderConfigAMF const& Other) const
	{
		return !(*this == Other);
	}

	template <typename TVariant>
	void SetProperty(wchar_t const* Name, TVariant const& Variant)
	{
		Variants.Add(Name, static_cast<const amf::AMFVariantStruct&>(amf::AMFVariant(Variant)));
	}

	template <typename TVariant>
	bool GetProperty(wchar_t const* Name, TVariant* OutVariant) const
	{
		if (amf::AMFVariant const* const Variant = static_cast<amf::AMFVariant const*>(Variants.Find(Name)))
		{
			*OutVariant = static_cast<TVariant>(*Variant);

			return true;
		}

		return false;
	}

	bool CompareProperty(wchar_t const* Name, FVideoEncoderConfigAMF const& Other) const
	{
		if (amf::AMFVariant const* const Variant = static_cast<amf::AMFVariant const*>(Variants.Find(Name)))
		{
			amf::AMFVariantStruct const* OtherVariant = Other.Variants.Find(Name);
			if (OtherVariant == nullptr)
			{
				return false;
			}

			bool IsEqual = false;
			AMFVariantCompare(Variant, OtherVariant, &IsEqual);

			return IsEqual;
		}

		if(amf::AMFVariantStruct const* OtherVariant = Other.Variants.Find(Name); OtherVariant == nullptr)
		{
			// If the property doesn't exist in either configs then assume they're equal
			return true;
		}

		return false;
	}

	void CopyTo(amf::AMFPropertyStorage* Other) const
	{
		for (TPair<wchar_t const*, amf::AMFVariantStruct> const& Variant : Variants)
		{
			Other->SetProperty(Variant.Key, Variant.Value);
		}
	}

	void SetDifferencesOnly(FVideoEncoderConfigAMF const& NewConfig, amf::AMFPropertyStorage* Storage)
	{
		for (TPair<wchar_t const*, amf::AMFVariantStruct> const& NewSetting : NewConfig.Variants)
		{
			amf::AMFVariant OldSetting;
			bool bHasProperty = GetProperty<amf::AMFVariantStruct>(NewSetting.Key, &OldSetting);
			if (bHasProperty)
			{
				bool bIsSettingSame = false;
				AMFVariantCompare(&OldSetting, &NewSetting.Value, &bIsSettingSame);
				if (!bIsSettingSame)
				{
					Storage->SetProperty(NewSetting.Key, NewSetting.Value);
				}
			}
			else
			{
				Storage->SetProperty(NewSetting.Key, NewSetting.Value);
			}
		}
	}

	void CopyFrom(amf::AMFPropertyStorage const* Other)
	{
		int32 const VariantCount = Other->GetPropertyCount();
		for (int i = 0; i < VariantCount; ++i)
		{
			wchar_t VariantName[128];
			amf::AMFVariantStruct VariantValue;
			if (Other->GetPropertyAt(i, VariantName, sizeof(VariantName), &VariantValue) == AMF_OK)
			{
				Variants.Add(VariantName, VariantValue);
			}
		}
	}
};

template <>
FAVResult FAVExtension::TransformConfig(FVideoEncoderConfigAMF& OutConfig, struct FVideoEncoderConfig const& InConfig);

template <>
FAVResult FAVExtension::TransformConfig(struct FVideoEncoderConfig& OutConfig, FVideoEncoderConfigAMF const& InConfig);

template <>
FAVResult FAVExtension::TransformConfig(FVideoEncoderConfigAMF& OutConfig, struct FVideoEncoderConfigH264 const& InConfig);

template <>
FAVResult FAVExtension::TransformConfig(FVideoEncoderConfigAMF& OutConfig, struct FVideoEncoderConfigH265 const& InConfig);

DECLARE_TYPEID(FVideoEncoderConfigAMF, AMFCODECS_API);
