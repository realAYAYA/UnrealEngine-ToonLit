// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utilities/StringHelpers.h"

namespace Electra
{
	namespace StringHelpers
	{


		int32 FindFirstOf(const FString& InString, const FString& SplitAt, int32 FirstPos)
		{
			if (SplitAt.Len() == 1)
			{
				// Speical version for only one split character
				const TCHAR& FindMe = SplitAt[0];
				for (int32 i = FirstPos; i < InString.Len(); ++i)
				{
					if (InString[i] == FindMe)
					{
						return i;
					}
				}
			}
			else
			{
				for (int32 i = FirstPos; i < InString.Len(); ++i)
				{
					for (int32 j = 0; j < SplitAt.Len(); ++j)
					{
						if (InString[i] == SplitAt[j])
						{
							return i;
						}
					}
				}
			}
			return INDEX_NONE;
		}


		int32 FindFirstNotOf(const FString& InString, const FString& InNotOfChars, int32 FirstPos)
		{
			for (int32 i = FirstPos; i < InString.Len(); ++i)
			{
				bool bFoundCharFromNotOfChars = false;
				for (int32 j = 0; j < InNotOfChars.Len(); ++j)
				{
					if (InString[i] == InNotOfChars[j])
					{
						// We found a character from the "NOT of" list. Check next...
						bFoundCharFromNotOfChars = true;
						break;
					}
				}
				if (!bFoundCharFromNotOfChars)
				{
					// We did not find any of the characters. This is what we are looking for and return the index of the first "not of" character
					return i;
				}
			}
			return INDEX_NONE;
		}


		int32 FindLastNotOf(const FString& InString, const FString& InNotOfChars, int32 InStartPos)
		{
			InStartPos = FMath::Min(InStartPos, InString.Len() - 1);
			for (int32 i = InStartPos; i >= 0; --i)
			{
				bool bFoundCharFromNotOfChars = false;
				for (int32 j = 0; j < InNotOfChars.Len(); ++j)
				{
					if (InString[i] == InNotOfChars[j])
					{
						// We found a character from the "NOT of" list. Check next...
						bFoundCharFromNotOfChars = true;
						break;
					}
				}
				if (!bFoundCharFromNotOfChars)
				{
					// We did not find any of the characters. This is what we are looking for and return the index of the first "not of" character
					return i;
				}
			}
			return INDEX_NONE;
		}


		void SplitByDelimiter(TArray<FString>& OutSplits, const FString& InString, const FString& SplitAt)
		{
			if (InString.Len())
			{
				int32 FirstPos = 0;
				while (1)
				{
					int32 SplitPos = InString.Find(SplitAt, ESearchCase::IgnoreCase, ESearchDir::FromStart, FirstPos);
					FString subs = InString.Mid(FirstPos, SplitPos == INDEX_NONE ? MAX_int32 : SplitPos - FirstPos);
					if (subs.Len())
					{
						OutSplits.Push(subs);
					}
					FirstPos = SplitPos + SplitAt.Len();
					if (SplitPos == INDEX_NONE || FirstPos >= InString.Len())
					{
						break;
					}
				}
			}
		}


		bool StringEquals(const TCHAR * const s1, const TCHAR * const s2)
		{ 
			return FPlatformString::Strcmp(s1, s2) == 0; 
		}

		bool StringStartsWith(const TCHAR * const s1, const TCHAR * const s2, SIZE_T n)
		{ 
			return FPlatformString::Strncmp(s1, s2, n) == 0; 
		}

		void StringToArray(TArray<uint8>& OutArray, const FString& InString)
		{
			FTCHARToUTF8 cnv(*InString);
			int32 Len = cnv.Length();
			OutArray.AddUninitialized(Len);
			FMemory::Memcpy(OutArray.GetData(), cnv.Get(), Len);
		}

		FString ArrayToString(const TArray<uint8>& InArray)
		{
			FUTF8ToTCHAR cnv((const ANSICHAR*)InArray.GetData(), InArray.Num());
			FString UTF8Text(cnv.Length(), cnv.Get());
			//return (TCHAR*)FUTF8ToTCHAR((const ANSICHAR*)InArray.GetData(), InArray.Num()).Get();
			//FString UTF8Text(InArray.Num(), (TCHAR*)FUTF8ToTCHAR((const ANSICHAR*)InArray.GetData(), InArray.Num()).Get());
			return MoveTemp(UTF8Text);
		}


	} // namespace StringHelpers
} // namespace Electra


