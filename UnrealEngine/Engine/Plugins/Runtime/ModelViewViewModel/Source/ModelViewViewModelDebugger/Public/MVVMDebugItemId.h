// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UE::MVVM
{

struct FDebugItemId
{
	enum class EType : uint8
	{
		Invalid,
		View,
		ViewModel,
	} Type = EType::Invalid;
	FGuid Id;


	FDebugItemId() = default;
	FDebugItemId(EType InType, FGuid InId)
		: Type(InType), Id(InId)
	{ }

	bool operator==(const FDebugItemId& Other) const
	{
		return Other.Type == Type && Other.Id == Id;
	}
};

}
