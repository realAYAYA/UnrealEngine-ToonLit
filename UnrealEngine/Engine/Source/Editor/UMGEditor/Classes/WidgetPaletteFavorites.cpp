// Copyright Epic Games, Inc. All Rights Reserved.

#include "WidgetPaletteFavorites.h"

#include "HAL/PlatformCrt.h"

UWidgetPaletteFavorites::UWidgetPaletteFavorites(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UWidgetPaletteFavorites::Add(const FString& InWidgetTemplateName)
{
	Favorites.AddUnique(InWidgetTemplateName);

	SaveConfig();

	OnFavoritesUpdated.Broadcast();
}

void UWidgetPaletteFavorites::Remove(const FString& InWidgetTemplateName)
{
	Favorites.RemoveSingle(InWidgetTemplateName);

	SaveConfig();

	OnFavoritesUpdated.Broadcast();
}