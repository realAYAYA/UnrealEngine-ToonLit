// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerivedDataCacheKeyFilter.h"

#include "Algo/Find.h"
#include "DerivedDataCacheKey.h"
#include "DerivedDataSharedString.h"
#include "Hash/xxhash.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"
#include "Misc/StringBuilder.h"
#include "String/Find.h"
#include "String/LexFromString.h"
#include "String/ParseTokens.h"

namespace UE::DerivedData
{

struct Private::FCacheKeyFilterState
{
	struct FTypeRate
	{
		FUtf8SharedString Type;
		uint32 Rate = 0;
	};

	TArray<FTypeRate, TInlineAllocator<2>> Types;
	uint32 DefaultRate = 0;
	uint32 Salt = 0;

	inline uint32 ApplySalt(const uint32 Hash) const
	{
		return Hash * Salt;
	}

	static inline uint32 ConvertMatchRate(const double Rate)
	{
		return uint32(0.01 * Rate * MAX_uint32);
	}

	static bool TryParseTypeRate(const FStringView TypeConfig, FTypeRate& OutTypeRate);
};

bool Private::FCacheKeyFilterState::TryParseTypeRate(const FStringView TypeConfig, FTypeRate& OutTypeRate)
{
	FStringView TypeView;
	FStringView RateView = TEXTVIEW("100");

	String::ParseTokens(TypeConfig, TEXT('@'), [&TypeView, &RateView](const FStringView TypeOrRate)
	{
		(TypeView.IsEmpty() ? TypeView : RateView) = TypeOrRate;
	}, String::EParseTokensOptions::Trim);

	double Rate;
	LexFromString(Rate, RateView);
	if (TypeView.IsEmpty() || Rate < 0.0 || Rate > 100.0)
	{
		return false;
	}

	OutTypeRate.Type = WriteToUtf8String<64>(TypeView);
	OutTypeRate.Rate = ConvertMatchRate(Rate);
	return true;
}

FCacheKeyFilter FCacheKeyFilter::Parse(const TCHAR* const Config, const TCHAR* const Prefix, const float DefaultRate)
{
	using namespace UE::DerivedData::Private;

	TArray<FCacheKeyFilterState::FTypeRate, TInlineAllocator<16>> Types;

	FString TypeConfigArray;
	for (FStringView ConfigView(Config), PrefixView(Prefix); FParse::Value(ConfigView.GetData(), Prefix, TypeConfigArray);)
	{
		const int32 PrefixIndex = String::FindFirst(ConfigView, PrefixView, ESearchCase::IgnoreCase);
		ConfigView.RightChopInline(PrefixIndex + PrefixView.Len() + TypeConfigArray.Len());
		String::ParseTokensMultiple(TypeConfigArray, {TEXT('+'), TEXT(',')}, [&Types](const FStringView TypeConfig)
		{
			FCacheKeyFilterState::FTypeRate TypeRate;
			if (FCacheKeyFilterState::TryParseTypeRate(TypeConfig, TypeRate))
			{
				Types.Emplace(TypeRate);
			}
		}, String::EParseTokensOptions::SkipEmpty | String::EParseTokensOptions::Trim);
	}

	if (Types.IsEmpty() && DefaultRate == 0.0f)
	{
		return {};
	}

	FCacheKeyFilter Filter;
	Filter.State = MakePimpl<FCacheKeyFilterState, EPimplPtrMode::DeepCopy>();
	Filter.State->Types = MoveTemp(Types);
	Filter.State->DefaultRate = FCacheKeyFilterState::ConvertMatchRate(FMath::Clamp(DefaultRate, 0.0f, 100.0f));
	Filter.SetSalt(0);
	return Filter;
}

void FCacheKeyFilter::SetSalt(uint32 Salt)
{
	// Generate a random salt in the range [1, MAX_int32].
	// Zero is invalid because the salt is multiplied with the key hash.
	// Values above MAX_int32 are avoided because FParse::Value cannot parse values in that range.
	Salt &= MAX_int32;
	while (Salt == 0)
	{
		// A new guid is the most reliable random value that the engine can generate.
		// Other options are not consistently seeded in a way that guarantees a random value when this executes.
		const FGuid Guid = FGuid::NewGuid();
		Salt = uint32(FXxHash64::HashBuffer(&Guid, sizeof(FGuid)).Hash & MAX_int32);
	}
	if (State)
	{
		State->Salt = Salt;
	}
}

uint32 FCacheKeyFilter::GetSalt() const
{
	return State ? State->Salt : 0;
}

bool FCacheKeyFilter::IsMatch(const FCacheKey& Key) const
{
	using namespace UE::DerivedData::Private;

	if (!State)
	{
		return false;
	}

	uint32 TargetRate = State->DefaultRate;

	if (!State->Types.IsEmpty())
	{
		TUtf8StringBuilder<256> Bucket;
		Bucket << Key.Bucket;
		if (Bucket.ToView().StartsWith(UTF8TEXTVIEW("Legacy")))
		{
			Bucket.RemoveAt(0, UTF8TEXTVIEW("Legacy").Len());
		}

		using FTypeRate = FCacheKeyFilterState::FTypeRate;
		if (const FTypeRate* TypeRate = Algo::FindBy(State->Types, Bucket.ToView(), &FTypeRate::Type))
		{
			TargetRate = TypeRate->Rate;
		}
	}

	if (TargetRate == 0)
	{
		return false;
	}
	if (TargetRate == MAX_uint32)
	{
		return true;
	}
	return State->ApplySalt(GetTypeHash(Key.Hash)) < TargetRate;
}

} // UE::DerivedData
