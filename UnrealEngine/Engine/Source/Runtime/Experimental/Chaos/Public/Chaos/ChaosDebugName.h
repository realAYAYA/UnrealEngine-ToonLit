// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/UnrealString.h"
#include "Templates/SharedPointer.h"

#ifndef CHAOS_DEBUG_NAME
#define CHAOS_DEBUG_NAME 0
#endif

namespace Chaos
{
	/**
	* A wrapper around shared pointer to a string that compiles away in shipping buiilds,
	* but Get() always returns a valid string reference to make writing logs a little easier
	*/
	class CHAOS_API FSharedDebugName
	{
	public:
		FSharedDebugName() = default;
		FSharedDebugName(const FSharedDebugName& Other) = default;
		FSharedDebugName(FSharedDebugName&& Other) = default;

		FSharedDebugName(const FString& S);
		FSharedDebugName(FString&& S);

		bool IsValid() const;
		const FString& Value() const;

	private:
#if CHAOS_DEBUG_NAME
		TSharedPtr<FString, ESPMode::ThreadSafe> Name;
#endif
		static FString DefaultName;
	};

#if !CHAOS_DEBUG_NAME
	inline FSharedDebugName::FSharedDebugName(const FString& S) {}
	inline FSharedDebugName::FSharedDebugName(FString&& S) {}
	inline bool FSharedDebugName::IsValid() const { return false; }
	inline const FString& FSharedDebugName::Value() const { return DefaultName; }
#endif
}