// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"

#include "WidgetPaletteFavorites.generated.h"

UCLASS(config = EditorPerProjectUserSettings)
class UMGEDITOR_API UWidgetPaletteFavorites : public UObject
{
	GENERATED_UCLASS_BODY()
public:

	void Add(const FString& InWidgetTemplateName);
	void Remove(const FString& InWidgetTemplateName);

	TArray<FString> GetFavorites() const { return Favorites; }
	
	DECLARE_MULTICAST_DELEGATE(FOnFavoritesUpdated)

	/** Fires after the list of favorites is updated */
	FOnFavoritesUpdated OnFavoritesUpdated;

private:
	UPROPERTY(config)
	TArray<FString> Favorites;
};
