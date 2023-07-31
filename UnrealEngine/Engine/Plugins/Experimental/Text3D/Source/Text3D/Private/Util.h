// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

struct FPart;

using FPartPtr = TSharedPtr<FPart>;
using FPartConstPtr = TSharedPtr<const FPart>;

class FAvailableExpandsFar final : public TMap<FPartPtr, float>
{
public:
	void Add(const FPartConstPtr Edge, const float Value)
	{
		TMap<FPartPtr, float>::Add(ConstCastSharedPtr<FPart>(Edge), Value);
	}

	void Remove(const FPartConstPtr Edge)
	{
		TMap<FPartPtr, float>::Remove(ConstCastSharedPtr<FPart>(Edge));
	}
};

class FContourBase : public TArray<FPartPtr>
{
public:
	int32 Find(const FPartConstPtr Edge)
	{
		return TArray<FPartPtr>::Find(ConstCastSharedPtr<FPart>(Edge));
	}
};
