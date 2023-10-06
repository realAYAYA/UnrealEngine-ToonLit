// Copyright Epic Games, Inc. All Rights Reserved.

#include "ElectraDecodersUtils.h"


namespace ElectraDecodersUtil
{

namespace
{
	inline void LexFromStringHex(int32& OutValue, const TCHAR* Buffer)
	{ 
		OutValue = FCString::Strtoi(Buffer, nullptr, 16);
	}
	
	inline void LexFromStringHexU64(uint64& OutValue, const TCHAR* Buffer)
	{
		OutValue = FCString::Strtoui64(Buffer, nullptr, 16);
	}

}



bool ParseMimeTypeWithCodec(FMimeTypeVideoCodecInfo& OutInfo, const FString& InMimeType)
{
	return false;
}

bool ParseMimeTypeWithCodec(FMimeTypeAudioCodecInfo& OutInfo, const FString& InMimeType)
{
	return false;
}

bool ParseCodecMP4A(FMimeTypeAudioCodecInfo& OutInfo, const FString& InCodecFormat)
{
	if (!InCodecFormat.StartsWith("mp4a"))
	{
		return false;
	}
	OutInfo.Codec = TEXT("mp4a");
	// Object and profile follow?
	if (InCodecFormat.Len() > 6 && InCodecFormat[4] == TCHAR('.'))
	{
		// mp4a.xx.d is recognized.
		FString OT, Profile;
		int32 DotPos = InCodecFormat.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromStart, 5);
		OT = InCodecFormat.Mid(5, DotPos != INDEX_NONE ? DotPos - 5 : DotPos);
		Profile = InCodecFormat.Mid(DotPos != INDEX_NONE ? DotPos + 1 : DotPos);
		OutInfo.ObjectType = FCString::Strtoi(*OT, nullptr, 16);
		int32 ProfileValue = 0;
		LexFromString(ProfileValue, *Profile);
		OutInfo.Profile = ProfileValue;
	}
	return true;
}

bool ParseCodecH264(FMimeTypeVideoCodecInfo& OutInfo, const FString& InCodecFormat)
{
	if (InCodecFormat.StartsWith("avc"))
	{
		// avc1 and avc3 (inband SPS/PPS) are recognized.
		if (InCodecFormat.Len() > 3)
		{
			// avc1 or avc3 only!
			if (InCodecFormat[3] != TCHAR('1') && InCodecFormat[3] != TCHAR('3'))
			{
				return false;
			}
			OutInfo.Codec = TEXT("avc");

			// Profile and level follow?
			if (InCodecFormat.Len() > 5 && InCodecFormat[4] == TCHAR('.'))
			{
				FString Temp;
				int32 TempValue;
				// We recognize the expected format avcC.xxyyzz and for legacy reasons also avcC.xx.zz
				if (InCodecFormat.Len() == 11)
				{
					Temp = InCodecFormat.Mid(5, 2);
					LexFromStringHex(TempValue, *Temp);
					OutInfo.Profile = TempValue;
					Temp = InCodecFormat.Mid(7, 2);
					LexFromStringHex(TempValue, *Temp);
					//ProfileConstraints = TempValue;
					Temp = InCodecFormat.Mid(9, 2);
					LexFromStringHex(TempValue, *Temp);
					OutInfo.Level = TempValue;
				}
				else if (InCodecFormat.Len() == 10 && InCodecFormat[7] == TCHAR('.'))
				{
					Temp = InCodecFormat.Mid(5, 2);
					LexFromStringHex(TempValue, *Temp);
					OutInfo.Profile = TempValue;
					Temp = InCodecFormat.Mid(8, 2);
					LexFromStringHex(TempValue, *Temp);
					OutInfo.Level = TempValue;
				}
				else
				{
					return false;
				}
			}
		}
		return true;
	}
	return false;
}

bool ParseCodecH265(FMimeTypeVideoCodecInfo& OutInfo, const FString& InCodecFormat)
{
	if (InCodecFormat.StartsWith("hvc1") || InCodecFormat.StartsWith("hev1"))
	{
		// hvc1 and hev1 (inband VPS/SPS/PPS) are recognized.
		if (InCodecFormat.Len() > 4)
		{
			OutInfo.Codec = TEXT("hevc");

			FString oti = InCodecFormat;
			FString Temp;
			int32 DotPos;
			if (oti.FindChar(TCHAR('.'), DotPos))
			{
				int32 general_profile_space = 0;
				int32 general_tier_flag = 0;
				int32 general_profile_idc = 0;
				int32 general_level_idc = 0;
				uint32 general_profile_compatibility_flag = 0;
				uint64 contraint_flags = 0;
				oti.RightChopInline(DotPos + 1);
				// optional general_profile_space
				if (oti[0] == TCHAR('A') || oti[0] == TCHAR('B') || oti[0] == TCHAR('C'))
				{
					general_profile_space = oti[0] - TCHAR('A') + 1;
					oti.RightChopInline(1);
				}
				else if (oti[0] == TCHAR('a') || oti[0] == TCHAR('b') || oti[0] == TCHAR('c'))
				{
					general_profile_space = oti[0] - TCHAR('a') + 1;
					oti.RightChopInline(1);
				}
				// general_profile_idc
				if (oti.FindChar(TCHAR('.'), DotPos))
				{
					Temp = oti.Left(DotPos);
					oti.RightChopInline(DotPos + 1);
					LexFromString(general_profile_idc, *Temp);
				}
				// general_profile_compatibility_flags
				if (oti.FindChar(TCHAR('.'), DotPos))
				{
					Temp = oti.Left(DotPos);
					oti.RightChopInline(DotPos + 1);
					LexFromString(general_profile_compatibility_flag, *Temp);
				}
				// general_tier_flag
				if (oti[0] != TCHAR('L') && oti[0] != TCHAR('H') && oti[0] != TCHAR('l') && oti[0] != TCHAR('h'))
				{
					return false;
				}
				else if (oti[0] == TCHAR('H') || oti[0] == TCHAR('h'))
				{
					general_tier_flag = 1;
				}
				oti.RightChopInline(1);
				// constraint_flags
				FString ConstraintFlags;
				if (oti.FindChar(TCHAR('.'), DotPos))
				{
					ConstraintFlags = oti.Mid(DotPos + 1);
					oti.LeftInline(DotPos);
					ConstraintFlags.ReplaceInline(TEXT("."), TEXT(""));
					ConstraintFlags += TEXT("000000000000");
					ConstraintFlags.LeftInline(12);
					LexFromStringHexU64(contraint_flags, *ConstraintFlags);
				}
				// general_level_idc
				LexFromString(general_level_idc, *oti);

				OutInfo.Profile = general_profile_idc;
				OutInfo.Level = general_level_idc;
				OutInfo.ProfileSpace = general_profile_space;
				OutInfo.CompatibilityFlags = BitReverse32(general_profile_compatibility_flag);
				OutInfo.Tier = general_tier_flag;
				OutInfo.Constraints = contraint_flags;
				return true;
			}
		}
	}
	return false;
}

bool ParseCodecVP8(FMimeTypeVideoCodecInfo& OutInfo, const FString& InCodecFormat, const TArray<uint8>& InvpcCBox)
{
	if (InCodecFormat.StartsWith("vp08"))
	{
		FString oti = InCodecFormat;
		if (!InvpcCBox.IsEmpty())
		{
			// Enough data to represent the `vpcC` box, and is it of version 1?
			if (InvpcCBox.Num() < 12 || InvpcCBox[0] != 1)
			{
				return false;
			}

			OutInfo.Profile = InvpcCBox[4];
			OutInfo.Level = InvpcCBox[5];
			OutInfo.NumBitsLuma = InvpcCBox[6] >> 4;
			return true;
		}
		if (oti.Len() > 4)
		{
			OutInfo.Codec = TEXT("vp08");
			FString Temp;
			int32 DotPos;
			if (oti.FindChar(TCHAR('.'), DotPos))
			{
				int32 Components[8] {0}, NumComponents=0;
				oti.RightChopInline(DotPos + 1);
				while(oti.Len() && NumComponents<UE_ARRAY_COUNT(Components))
				{
					if (oti.FindChar(TCHAR('.'), DotPos))
					{
						Temp = oti.Left(DotPos);
						oti.RightChopInline(DotPos + 1);
						LexFromString(Components[NumComponents++], *Temp);
					}
					else
					{
						LexFromString(Components[NumComponents++], *oti);
						break;
					}
				}
				OutInfo.Profile = Components[0];
				OutInfo.Level = Components[1];
				OutInfo.NumBitsLuma = Components[2];
				return true;
			}
		}
	}
	return false;
}

bool ParseCodecVP9(FMimeTypeVideoCodecInfo& OutInfo, const FString& InCodecFormat, const TArray<uint8>& InvpcCBox)
{
	if (InCodecFormat.StartsWith("vp09"))
	{
		FString oti = InCodecFormat;
		if (!InvpcCBox.IsEmpty())
		{
			// Enough data to represent the `vpcC` box, and is it of version 1?
			if (InvpcCBox.Num() < 12 || InvpcCBox[0] != 1)
			{
				return false;
			}

			OutInfo.Extras[0] = OutInfo.Profile = InvpcCBox[4];
			OutInfo.Extras[1] = OutInfo.Level = InvpcCBox[5];
			OutInfo.Extras[2] = OutInfo.NumBitsLuma = InvpcCBox[6] >> 4;
			OutInfo.Extras[3] = (InvpcCBox[6] >> 1) & 7;
			OutInfo.Extras[4] = InvpcCBox[7];
			OutInfo.Extras[5] = InvpcCBox[8];
			OutInfo.Extras[6] = InvpcCBox[9];
			OutInfo.Extras[7] = InvpcCBox[6] & 1;
			return true;
		}
		if (oti.Len() > 4)
		{
			OutInfo.Codec = TEXT("vp09");
			FString Temp;
			int32 DotPos;
			if (oti.FindChar(TCHAR('.'), DotPos))
			{
				int32 Components[8] {0}, NumComponents=0;
				oti.RightChopInline(DotPos + 1);
				while(oti.Len() && NumComponents<UE_ARRAY_COUNT(Components))
				{
					if (oti.FindChar(TCHAR('.'), DotPos))
					{
						Temp = oti.Left(DotPos);
						oti.RightChopInline(DotPos + 1);
						LexFromString(Components[NumComponents++], *Temp);
					}
					else
					{
						LexFromString(Components[NumComponents++], *oti);
						break;
					}
				}
				OutInfo.Extras[0] = OutInfo.Profile = Components[0];
				OutInfo.Extras[1] = OutInfo.Level = Components[1];
				OutInfo.Extras[2] = OutInfo.NumBitsLuma = Components[2];
				OutInfo.Extras[3] = Components[3];
				OutInfo.Extras[4] = Components[4];
				OutInfo.Extras[5] = Components[5];
				OutInfo.Extras[6] = Components[6];
				OutInfo.Extras[7] = Components[7];
				return true;
			}
		}
	}
	return false;
}


int64 GetVariantValueSafeI64(const TMap<FString, FVariant>& InFromMap, const FString& InName, int64 InDefaultValue)
{
	int64 V = InDefaultValue;
	const FVariant* Var = InFromMap.Find(InName);
	if (Var)
	{
		if (Var->GetType() == EVariantTypes::Int32)
		{
			V = Var->GetValue<int32>();
		}
		else if (Var->GetType() == EVariantTypes::Int64)
		{
			V = Var->GetValue<int64>();
		}
		else if (Var->GetType() == EVariantTypes::UInt64)
		{
			V = (int64) Var->GetValue<uint64>();
		}
		else if (Var->GetType() == EVariantTypes::UInt32)
		{
			V = (int64) Var->GetValue<uint32>();
		}
		else if (Var->GetType() == EVariantTypes::Int16)
		{
			V = Var->GetValue<int16>();
		}
		else if (Var->GetType() == EVariantTypes::Int8)
		{
			V = Var->GetValue<int8>();
		}
		else if (Var->GetType() == EVariantTypes::UInt16)
		{
			V = Var->GetValue<uint16>();
		}
		else if (Var->GetType() == EVariantTypes::UInt8)
		{
			V = Var->GetValue<uint8>();
		}
		else if (Var->GetType() == EVariantTypes::Float)
		{
			V = (int64) Var->GetValue<float>();
		}
		else if (Var->GetType() == EVariantTypes::Double)
		{
			V = (int64) Var->GetValue<double>();
		}
		else if (Var->GetType() == EVariantTypes::Bool)
		{
			V = Var->GetValue<bool>() ? 1 : 0;
		}
	}
	return V;
}


uint64 GetVariantValueSafeU64(const TMap<FString, FVariant>& InFromMap, const FString& InName, uint64 InDefaultValue)
{
	uint64 V = InDefaultValue;
	const FVariant* Var = InFromMap.Find(InName);
	if (Var)
	{
		if (Var->GetType() == EVariantTypes::Int32)
		{
			V = Var->GetValue<int32>();
		}
		else if (Var->GetType() == EVariantTypes::Int64)
		{
			V = (uint64) Var->GetValue<int64>();
		}
		else if (Var->GetType() == EVariantTypes::UInt64)
		{
			V = Var->GetValue<uint64>();
		}
		else if (Var->GetType() == EVariantTypes::UInt32)
		{
			V = Var->GetValue<uint32>();
		}
		else if (Var->GetType() == EVariantTypes::Int16)
		{
			V = Var->GetValue<int16>();
		}
		else if (Var->GetType() == EVariantTypes::Int8)
		{
			V = Var->GetValue<int8>();
		}
		else if (Var->GetType() == EVariantTypes::UInt16)
		{
			V = Var->GetValue<uint16>();
		}
		else if (Var->GetType() == EVariantTypes::UInt8)
		{
			V = Var->GetValue<uint8>();
		}
		else if (Var->GetType() == EVariantTypes::Float)
		{
			V = (uint64) Var->GetValue<float>();
		}
		else if (Var->GetType() == EVariantTypes::Double)
		{
			V = (uint64) Var->GetValue<double>();
		}
		else if (Var->GetType() == EVariantTypes::Bool)
		{
			V = Var->GetValue<bool>() ? 1 : 0;
		}
	}
	return V;
}


double GetVariantValueSafeDouble(const TMap<FString, FVariant>& InFromMap, const FString& InName, double InDefaultValue)
{
	double V = InDefaultValue;
	const FVariant* Var = InFromMap.Find(InName);
	if (Var)
	{
		if (Var->GetType() == EVariantTypes::Int32)
		{
			V = (double) Var->GetValue<int32>();
		}
		else if (Var->GetType() == EVariantTypes::Int64)
		{
			V = (double) Var->GetValue<int64>();
		}
		else if (Var->GetType() == EVariantTypes::UInt32)
		{
			V = (double) Var->GetValue<uint32>();
		}
		else if (Var->GetType() == EVariantTypes::Int16)
		{
			V = (double) Var->GetValue<int16>();
		}
		else if (Var->GetType() == EVariantTypes::Int8)
		{
			V = (double) Var->GetValue<int8>();
		}
		else if (Var->GetType() == EVariantTypes::UInt16)
		{
			V = (double) Var->GetValue<uint16>();
		}
		else if (Var->GetType() == EVariantTypes::UInt8)
		{
			V = (double) Var->GetValue<uint8>();
		}
		else if (Var->GetType() == EVariantTypes::Float)
		{
			V = (double) Var->GetValue<float>();
		}
		else if (Var->GetType() == EVariantTypes::Double)
		{
			V = (double) Var->GetValue<double>();
		}
		else if (Var->GetType() == EVariantTypes::Bool)
		{
			V = Var->GetValue<bool>() ? 1.0 : 0.0;
		}
	}
	return V;
}


TArray<uint8> GetVariantValueUInt8Array(const TMap<FString, FVariant>& InFromMap, const FString& InName)
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
}

FString GetVariantValueFString(const TMap<FString, FVariant>& InFromMap, const FString& InName)
{
	const FVariant* Var = InFromMap.Find(InName);
	if (Var)
	{
		if (Var->GetType() == EVariantTypes::String)
		{
			return Var->GetValue<FString>();
		}
	}
	return FString();
}

}
