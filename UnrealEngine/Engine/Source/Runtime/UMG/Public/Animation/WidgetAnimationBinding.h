// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "WidgetAnimationBinding.generated.h"

class UUserWidget;
class UWidgetTree;

/**
 * A single object bound to a UMG sequence.
 */
USTRUCT()
struct FWidgetAnimationBinding
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName WidgetName;

	UPROPERTY()
	FName SlotWidgetName;

	UPROPERTY()
	FGuid AnimationGuid;

	UPROPERTY()
	bool bIsRootWidget = false;

public:

	/**
	 * Locates a runtime object to animate from the provided tree of widgets.
	 * @return the runtime object to animate or null if not found
	 */
	UMG_API UObject* FindRuntimeObject(const UWidgetTree& WidgetTree, UUserWidget& UserWidget) const;

	/**
	 * Compares two widget animation bindings for equality.
	 *
	 * @param X The first binding to compare.
	 * @param Y The second binding to compare.
	 * @return true if the bindings are equal, false otherwise.
	 */
	friend bool operator==(const FWidgetAnimationBinding& X, const FWidgetAnimationBinding& Y)
	{
		return (X.WidgetName == Y.WidgetName) && (X.SlotWidgetName == Y.SlotWidgetName) && (X.AnimationGuid == Y.AnimationGuid) && (X.bIsRootWidget == Y.bIsRootWidget);
	}

	/**
	 * Serializes a widget animation binding from/to the given archive.
	 *
	 * @param Ar The archive to serialize to/from.
	 * @param Binding the binding to serialize.
	 */
	friend FArchive& operator<<(FArchive& Ar, FWidgetAnimationBinding& Binding)
	{
		Ar << Binding.WidgetName;
		Ar << Binding.SlotWidgetName;
		Ar << Binding.AnimationGuid;
		Ar << Binding.bIsRootWidget;
		return Ar;
	}

	friend void operator<<(FStructuredArchive::FSlot Slot, FWidgetAnimationBinding& Binding)
	{
		FStructuredArchive::FRecord Record = Slot.EnterRecord();
		Record << SA_VALUE(TEXT("WidgetName"), Binding.WidgetName);
		Record << SA_VALUE(TEXT("SlotWidgetName"), Binding.SlotWidgetName);
		Record << SA_VALUE(TEXT("AnimationGuid"), Binding.AnimationGuid);
		Record << SA_VALUE(TEXT("bIsRootWidget"), Binding.bIsRootWidget);
	}
};
