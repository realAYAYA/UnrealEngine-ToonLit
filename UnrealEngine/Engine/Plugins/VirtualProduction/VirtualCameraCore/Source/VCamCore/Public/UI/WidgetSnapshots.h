// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SubclassOf.h"
#include "Blueprint/UserWidget.h"
#include "WidgetSnapshots.generated.h"

/** Saves a subset of properties of a widget. */
USTRUCT()
struct FWidgetSnapshot
{
	GENERATED_BODY()

	UPROPERTY()
	TSubclassOf<UUserWidget> WidgetClass;

	/** Can be empty in case no properties were saved */
	UPROPERTY()
	TArray<uint8> SavedBinaryData;
};

/** Saves properties from widgets within a widget tree. */
USTRUCT()
struct FWidgetTreeSnapshot
{
	GENERATED_BODY()

	/**
	 * The name of the root widget if it was saved (may not have passed filters).
	 * The root widget may be re-instanced (and thus renamed).
	 */
	UPROPERTY()
	FName RootWidget = NAME_None;
	
	/** Stores every widget in the hierarchy which passed filters, even if there are no properties to save. */
	UPROPERTY()
	TMap<FName, FWidgetSnapshot> WidgetSnapshots;

	void Reset() { WidgetSnapshots.Reset(); }
	bool HasData() const { return WidgetSnapshots.Num() != 0; }
};