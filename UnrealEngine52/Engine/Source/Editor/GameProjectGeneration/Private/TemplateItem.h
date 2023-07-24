// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "TemplateProjectDefs.h"

/** Struct describing a single template project */
struct FTemplateItem
{
	FText		Name;
	FText		Description;
	TArray<FName> Categories;

	FString		Key;
	FString		SortKey;

	TSharedPtr<FSlateBrush> Thumbnail;
	TSharedPtr<FSlateBrush> PreviewImage;

	FString		ClassTypes;
	FString		AssetTypes;

	FString		CodeProjectFile;
	UTemplateProjectDefs* CodeTemplateDefs = nullptr;
	FString		BlueprintProjectFile;
	UTemplateProjectDefs* BlueprintTemplateDefs = nullptr;

	TArray<ETemplateSetting> HiddenSettings;

	bool		bIsEnterprise = false;
	bool		bIsBlankTemplate = false;
	bool		bThumbnailAsIcon = false;
};
