// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlayerCore.h"
#include "StreamTypes.h"
#include "Utilities/StringHelpers.h"
#include "Utilities/Utilities.h"

namespace Electra
{

	FString FStreamCodecInformation::GetMimeType() const
	{
		if (!MimeType.IsEmpty())
		{
			return MimeType;
		}
		switch(GetCodec())
		{
			case ECodec::H264:
				return FString(TEXT("video/mp4"));
			case ECodec::H265:
				return FString(TEXT("video/mp4"));
			case ECodec::AAC:
				return FString(TEXT("audio/mp4"));
			case ECodec::EAC3:
				return FString(TEXT("audio/mp4"));
			case ECodec::WebVTT:
			case ECodec::TTML:
			case ECodec::TX3G:
			case ECodec::OtherSubtitle:
				return FString(TEXT("application/mp4"));
			default:
				return FString(TEXT("application/octet-stream"));
		}
	}

	FString FStreamCodecInformation::GetMimeTypeWithCodec() const
	{
		return GetMimeType() + FString::Printf(TEXT("; codecs=\"%s\""), *GetCodecSpecifierRFC6381());
	}

	FString FStreamCodecInformation::GetMimeTypeWithCodecAndFeatures() const
	{
		if (GetStreamType() == EStreamType::Video && GetResolution().Width && GetResolution().Height)
		{
			return GetMimeTypeWithCodec() + FString::Printf(TEXT("; resolution=%dx%d"), GetResolution().Width, GetResolution().Height);
		}
		return GetMimeTypeWithCodec();
	}


	bool FStreamCodecInformation::ParseFromRFC6381(const FString& CodecOTI)
	{
		if (CodecOTI.StartsWith("avc"))
		{
			// avc1 and avc3 (inband SPS/PPS) are recognized.
			StreamType = EStreamType::Video;
			CodecSpecifier = CodecOTI;
			Codec = ECodec::H264;
			if (CodecOTI.Len() > 3)
			{
				// avc 1 or 3 only.
				if (CodecOTI[3] != TCHAR('1') && CodecOTI[3] != TCHAR('3'))
				{
					return false;
				}
				// Profile and level follow?
				if (CodecOTI.Len() > 5 && CodecOTI[4] == TCHAR('.'))
				{
					FString Temp;
					int32 TempValue;
					// We recognize the expected format avcC.xxyyzz and for legacy reasons also avcC.xx.zz
					if (CodecOTI.Len() == 11)
					{
						Temp = CodecOTI.Mid(5, 2);
						LexFromStringHex(TempValue, *Temp);
						SetProfile(TempValue);
						Temp = CodecOTI.Mid(7, 2);
						LexFromStringHex(TempValue, *Temp);
						SetProfileConstraints(TempValue);
						Temp = CodecOTI.Mid(9, 2);
						LexFromStringHex(TempValue, *Temp);
						SetProfileLevel(TempValue);
					}
					else if (CodecOTI.Len() == 10 && CodecOTI[7] == TCHAR('.'))
					{
						Temp = CodecOTI.Mid(5, 2);
						LexFromStringHex(TempValue, *Temp);
						SetProfile(TempValue);
						Temp = CodecOTI.Mid(8, 2);
						LexFromStringHex(TempValue, *Temp);
						SetProfileLevel(TempValue);
						// Change the string to the expected format.
						SetCodecSpecifierRFC6381(FString::Printf(TEXT("avc%c.%02x00%02x"), CodecOTI[3], GetProfile(), GetProfileLevel()));
					}
					else
					{
						return false;
					}
				}
			}
			return true;
		}
		else if (CodecOTI.StartsWith("hvc") || CodecOTI.StartsWith("hev"))
		{
			FString oti = CodecOTI;
			FString Temp;
			// hvc1 and hev1 (inband VPS/SPS/PPS) are recognized.
			StreamType = EStreamType::Video;
			CodecSpecifier = CodecOTI;
			Codec = ECodec::H265;

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
				SetProfileSpace(general_profile_space);
				SetProfileCompatibilityFlags(Utils::BitReverse32(general_profile_compatibility_flag));
				SetProfileTier(general_tier_flag);
				SetProfile(general_profile_idc);
				SetProfileLevel(general_level_idc);
				SetProfileConstraints(contraint_flags);
				return true;
			}
			return false;
		}
		else if (CodecOTI.StartsWith("mp4a"))
		{
			StreamType = EStreamType::Audio;
			CodecSpecifier = CodecOTI;
			Codec = ECodec::AAC;
			// Object and profile follow?
			if (CodecOTI.Len() > 6 && CodecOTI[4] == TCHAR('.'))
			{
				// mp4a.40.d is recognized.
				FString OT, Profile;
				int32 DotPos = CodecOTI.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromStart, 5);
				OT = CodecOTI.Mid(5, DotPos != INDEX_NONE ? DotPos - 5 : DotPos);
				Profile = CodecOTI.Mid(DotPos != INDEX_NONE ? DotPos + 1 : DotPos);
				if (!OT.Equals(TEXT("40")))
				{
					return false;
				}
				int32 ProfileValue = 0;
				LexFromString(ProfileValue, *Profile);
				SetProfile(ProfileValue);
				// AAC-LC, AAC-HE (SBR), AAC-HEv2 (PS)
				if (!(ProfileValue == 2 || ProfileValue == 5 || ProfileValue == 29))
				{
					return false;
				}
			}
			return true;
		}
		else if (CodecOTI.StartsWith("ec-3") || CodecOTI.StartsWith("ec+3") || CodecOTI.StartsWith("ec3") || CodecOTI.StartsWith("eac3"))
		{
			StreamType = EStreamType::Audio;
			CodecSpecifier = CodecOTI;
			Codec = ECodec::EAC3;
			// Presently not supported.
			return false;
		}
		else if (CodecOTI.Equals(TEXT("wvtt")))
		{
			StreamType = EStreamType::Subtitle;
			CodecSpecifier = CodecOTI;
			Codec = ECodec::WebVTT;
			return true;
		}
		// This is indicating one of the many TTML variants (eg. IMSC1, SMPTE-TT, EBU-TT) and profiles (eg. stpp.ttml.im1t)
		//	See: https://www.w3.org/TR/ttml-profile-registry/#registry-profile-designator-specifications
		else if (CodecOTI.StartsWith(TEXT("stpp")))
		{
			StreamType = EStreamType::Subtitle;
			CodecSpecifier = CodecOTI;
			Codec = ECodec::TTML;
			return true;
		}
		else if (CodecOTI.Equals(TEXT("tx3g")))
		{
			StreamType = EStreamType::Subtitle;
			CodecSpecifier = CodecOTI;
			Codec = ECodec::TX3G;
			return true;
		}
		else
		{
			StreamType = EStreamType::Unsupported;
			CodecSpecifier = CodecOTI;
			Codec = ECodec::Unknown;
			return false;
		}
	}

	FString FStreamCodecInformation::GetCodecName() const
	{
		switch(Codec)
		{
			case FStreamCodecInformation::ECodec::H264:
				return FString(TEXT("avc"));
			case FStreamCodecInformation::ECodec::H265:
				return FString(TEXT("hevc"));
			case FStreamCodecInformation::ECodec::AAC:
				return FString(TEXT("aac"));
			case FStreamCodecInformation::ECodec::EAC3:
				return FString(TEXT("eac3"));
			case FStreamCodecInformation::ECodec::WebVTT:
				return FString(TEXT("wvtt"));
			case FStreamCodecInformation::ECodec::TTML:
				return FString(TEXT("stpp"));
			case FStreamCodecInformation::ECodec::TX3G:
				return FString(TEXT("tx3g"));
			case FStreamCodecInformation::ECodec::OtherSubtitle:
				return FString(TEXT("subt"));
			default:
				return FString(TEXT("unknown"));
		}
	}





	bool FCodecSelectionPriorities::Initialize(const FString& ConfigurationString)
	{
		ClassPriorities.Empty();
		if (ConfigurationString.Len() && !ParseInternal(ConfigurationString))
		{
			ClassPriorities.Empty();
			return false;
		}
		return true;
	}
	bool FCodecSelectionPriorities::ParseInternal(const FString& ConfigurationString)
	{
		auto SkipWhiteSpaces = [](StringHelpers::FStringIterator& it) -> void
		{
			while(it && TChar<TCHAR>::IsWhitespace(*it))
			{
				++it;
			}
		};

		auto ParsePriority = [](int32& OutPrio, StringHelpers::FStringIterator& it, bool bInClass) -> bool
		{
			int64 Prio = 0;
			bool bEmpty = true;
			while(it && TChar<TCHAR>::IsDigit(*it))
			{
				bEmpty = false;
				Prio *= 10;
				Prio += *it - TCHAR('0');
				++it;
			}
			while(it && TChar<TCHAR>::IsWhitespace(*it))
			{
				++it;
			}
			// Did we end the priority properly?
			if (!bEmpty && (!it || *it == TCHAR(',') || (bInClass && *it == TCHAR('{'))))
			{
				OutPrio = Prio;
				return true;
			}
			// Unexpected next character. Fail!
			return false;
		};

		const TCHAR* const CommaDelimiter = TEXT(",");
		StringHelpers::FStringIterator it(ConfigurationString);
		while(it)
		{
			FClassPriority ClassPriority;
			SkipWhiteSpaces(it);
			while(it && *it != TCHAR('=') && *it != TCHAR('{') && *it != TCHAR(','))
			{
				ClassPriority.Prefix += *it++;
			}
			if (ClassPriority.Prefix.Len() == 0)
			{
				return false;
			}

			// Is the next char assigning a priority?
			if (it && *it == TCHAR('='))
			{
				// Get the class priority
				++it;
				if (!ParsePriority(ClassPriority.Priority, it, true))
				{
					return false;
				}
			}
			// If no priority then there must now be a group for stream specific priorities.
			else if (!it || *it != TCHAR('{'))
			{
				return false;
			}
			// Do stream specific priorities follow?
			if (it && *it == TCHAR('{'))
			{
				int32 GroupStart = it.GetIndex();
				// Look for the end of the group.
				while(it && *it != TCHAR('}'))
				{
					++it;
				}
				if (!it || *it != TCHAR('}'))
				{
					return false;
				}
				++it;
				FString Group = ConfigurationString.Mid(GroupStart+1, it.GetIndex()-GroupStart-2);
				TArray<FString> StreamPriorities;
				Group.ParseIntoArray(StreamPriorities, CommaDelimiter, true);
				if (StreamPriorities.Num() == 0)
				{
					return false;
				}
				for(auto &sp : StreamPriorities)
				{
					FStreamPriority StreamPriority;
					StringHelpers::FStringIterator spIt(sp);
					while(spIt)
					{
						SkipWhiteSpaces(spIt);
						while(spIt && *spIt != TCHAR('=') && *spIt != TCHAR('{') && *spIt != TCHAR(','))
						{
							StreamPriority.Prefix += *spIt++;
						}
						if (StreamPriority.Prefix.Len() == 0)
						{
							return false;
						}
						if (spIt && *spIt == TCHAR('='))
						{
							++spIt;
							if (!ParsePriority(StreamPriority.Priority, spIt, false))
							{
								return false;
							}
						}
						else
						{
							return false;
						}
					}
					ClassPriority.StreamPriorities.Emplace(MoveTemp(StreamPriority));
				}
			}
			// Either there's a comma separating successive entries or we are done.
			SkipWhiteSpaces(it);
			if (it && *it != TCHAR(','))
			{
				return false;
			}
			++it;
			ClassPriorities.Emplace(MoveTemp(ClassPriority));
		}
		return true;
	}
	
	int32 FCodecSelectionPriorities::GetClassPriority(const FString& CodecSpecifierRFC6381) const
	{
		for(auto &CodecClass : ClassPriorities)
		{
			if (CodecSpecifierRFC6381.StartsWith(CodecClass.Prefix, ESearchCase::IgnoreCase))
			{
				return CodecClass.Priority;
			}
		}
		return -1;
	}
	
	int32 FCodecSelectionPriorities::GetStreamPriority(const FString& CodecSpecifierRFC6381) const
	{
		for(auto &CodecClass : ClassPriorities)
		{
			if (CodecSpecifierRFC6381.StartsWith(CodecClass.Prefix, ESearchCase::IgnoreCase))
			{
				for(auto &CodecStream : CodecClass.StreamPriorities)
				{
					if (CodecSpecifierRFC6381.StartsWith(CodecStream.Prefix, ESearchCase::IgnoreCase))
					{
						return CodecStream.Priority;
					}
				}
			}
		}
		return -1;
	}

} // namespace Electra
