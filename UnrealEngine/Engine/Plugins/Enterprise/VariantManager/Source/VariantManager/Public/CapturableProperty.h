// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "PropertyPath.h"
#include "PropertyValue.h" // For ToCapturedPropSegmentArray

#include "CapturableProperty.generated.h"

enum class EPropertyValueCategory : uint8;

// Describes a property path that can be captured. It just exposes a display name but
// uses internal data in order to be able to capture exception properties, like materials
USTRUCT(BlueprintType)
struct FCapturableProperty
{
	GENERATED_USTRUCT_BODY()

	FCapturableProperty(){};
	FCapturableProperty(const FString& InDisplayName, const FPropertyPath& InPropPath, const TArray<FString>& InComponentNames, bool InChecked, FName InPropertySetterName, EPropertyValueCategory InCaptureType)
		: DisplayName(InDisplayName)
		, Prop(InPropPath)
		, Checked(InChecked)
		, CaptureType(InCaptureType)
		, ComponentNames(InComponentNames)
		, PropertySetterName(InPropertySetterName)
	{
	}

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="VariantManager")
	FString DisplayName;

	FPropertyPath Prop;
	bool Checked;
	EPropertyValueCategory CaptureType;
	TArray<FString> ComponentNames; // Same length as Prop, but will be empty for non component ObjectProperties
	FName PropertySetterName;

	TArray<FCapturedPropSegment> ToCapturedPropSegmentArray()
	{
		int32 NumSegs = Prop.GetNumProperties();
		check(ComponentNames.Num() == NumSegs);

		TArray<FCapturedPropSegment> Result;
		Result.Reserve(NumSegs);

		for (int32 Index = 0; Index < NumSegs; Index++)
		{
			const FPropertyInfo& PropInfo = Prop.GetPropertyInfo(Index);

			FString& ComponentName = ComponentNames[Index];

			// If we have a component name, forget the index. This makes it easy to search by name later
			FCapturedPropSegment NewSegment;
			NewSegment.PropertyName = PropInfo.Property->GetName();
			NewSegment.PropertyIndex = ComponentName.IsEmpty() ? PropInfo.ArrayIndex : 0;
			NewSegment.ComponentName = ComponentName;

			Result.Add(NewSegment);
		}

		return Result;
	}
};