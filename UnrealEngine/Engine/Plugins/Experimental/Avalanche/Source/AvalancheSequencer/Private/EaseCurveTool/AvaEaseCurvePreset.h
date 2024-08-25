// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "EaseCurveTool/AvaEaseCurveTangents.h"
#include "Templates/SharedPointer.h"
#include "AvaEaseCurvePreset.generated.h"

USTRUCT()
struct FAvaEaseCurvePreset
{
	GENERATED_BODY()

	FAvaEaseCurvePreset() {}
	FAvaEaseCurvePreset(const FString& InName, const FString& InCategory, const FAvaEaseCurveTangents& InTangents)
		: Name(InName), Category(InCategory), Tangents(InTangents)
	{}

	FORCEINLINE bool operator==(const FAvaEaseCurvePreset& InRhs) const
	{
		return Name.Equals(InRhs.Name) && Category.Equals(InRhs.Category);
	}
	FORCEINLINE bool operator!=(const FAvaEaseCurvePreset& InRhs) const
	{
		return !(*this == InRhs);
	}
	FORCEINLINE bool operator<(const FAvaEaseCurvePreset& InRhs) const
	{
		// Order from soft to hard based on curve length
		return Tangents.CalculateCurveLength() < InRhs.Tangents.CalculateCurveLength();
	}

	UPROPERTY(Config, EditAnywhere, Category = "EaseCurvePreset")
	FString Name;

	UPROPERTY(Config, EditAnywhere, Category = "EaseCurvePreset")
	FString Category;

	UPROPERTY(Config, EditAnywhere, Category = "EaseCurvePreset")
	FAvaEaseCurveTangents Tangents;
};
