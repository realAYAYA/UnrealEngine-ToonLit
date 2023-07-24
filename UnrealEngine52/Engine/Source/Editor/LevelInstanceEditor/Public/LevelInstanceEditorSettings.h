// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/DeveloperSettings.h"
#include "LevelInstance/LevelInstanceTypes.h"
#include "LevelInstanceEditorSettings.generated.h"

UCLASS(config = Editor)
class ULevelInstanceEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	ULevelInstanceEditorSettings();

	/** List of info for all known LevelInstance template maps */
	UPROPERTY(config)
	TArray<FTemplateMapInfo> TemplateMapInfos;

	UPROPERTY(config)
	FString LevelInstanceClassName;
};

UCLASS(config = EditorPerProjectUserSettings, meta = (DisplayName = "Level Instance"))
class ULevelInstanceEditorPerProjectUserSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	ULevelInstanceEditorPerProjectUserSettings();

	static void UpdateFrom(const FNewLevelInstanceParams& Params);

	/** Gets the category for the settings, some high level grouping like, Editor, Engine, Game...etc. */
	virtual FName GetCategoryName() const override { return TEXT("ContentEditors"); }
		
	/** If false, create dialog will not be shown and last settings will be used. */
	UPROPERTY(config, EditAnywhere, Category = Create)
	bool bAlwaysShowDialog;

	UPROPERTY(config, EditAnywhere, Category = Pivot)
	ELevelInstancePivotType PivotType;
};