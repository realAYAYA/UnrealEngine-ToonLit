// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/ChaosDebugName.h"
#include "CoreTypes.h"

namespace Chaos
{
	FString FSharedDebugName::DefaultName = FString(TEXT("NoName"));

#if CHAOS_DEBUG_NAME
	FSharedDebugName::FSharedDebugName(const FString& S)
		: Name(TSharedPtr<FString, ESPMode::ThreadSafe>(new FString(S)))
	{
	}

	FSharedDebugName::FSharedDebugName(FString&& S)
		: Name(TSharedPtr<FString, ESPMode::ThreadSafe>(new FString(MoveTemp(S))))
	{
	}

	inline bool FSharedDebugName::IsValid() const
	{ 
		return Name.IsValid();
	}

	const FString& FSharedDebugName::Value() const
	{
		if (Name.IsValid())
		{
			return *Name.Get();
		}
		return DefaultName;
	}

#endif
}