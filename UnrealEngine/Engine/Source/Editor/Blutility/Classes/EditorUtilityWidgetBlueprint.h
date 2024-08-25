// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * Widget for editor utilities
 */

#pragma once

#include "Containers/Set.h"
#include "CoreMinimal.h"
#include "EditorUtilityWidget.h"
#include "HAL/Platform.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "WidgetBlueprint.h"
#include "Widgets/Docking/SDockTab.h"

#include "EditorUtilityWidgetBlueprint.generated.h"

class FSpawnTabArgs;
class SDockTab;
class SWidget;
class UBlueprint;
class UClass;
class UEditorUtilityWidget;
class UObject;
class UWorld;

enum class EAssetEditorCloseReason : uint8;
enum class EMapChangeType : uint8;

UCLASS()
class BLUTILITY_API UEditorUtilityWidgetBlueprint : public UWidgetBlueprint
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;

	TSharedRef<SDockTab> SpawnEditorUITab(const FSpawnTabArgs& SpawnTabArgs);

	/** Creates the slate widget from the UMG widget */
	TSharedRef<SWidget> CreateUtilityWidget();

	/** Recreate the tab's content on recompile */
	void RegenerateCreatedTab(UBlueprint* RecompiledBlueprint);
	
	void UpdateRespawnListIfNeeded(TSharedRef<SDockTab> TabBeingClosed);

	// UBlueprint interface
	virtual void GetReparentingRules(TSet< const UClass* >& AllowedChildrenOfClasses, TSet< const UClass* >& DisallowedChildrenOfClasses) const override;

	virtual bool AllowEditorWidget() const override { return true; }

	UEditorUtilityWidget* GetCreatedWidget() const
	{
		return CreatedUMGWidget;
	}

	void SetRegistrationName(FName InRegistrationName)
	{
		RegistrationName = InRegistrationName;
	}

	FName GetRegistrationName() const
	{
		return RegistrationName;
	}

	/** Returns the default desired tab display name that was specified for this widget */
	FText GetTabDisplayName() const;

	virtual UWidgetEditingProjectSettings* GetRelevantSettings() override;
	virtual const UWidgetEditingProjectSettings* GetRelevantSettings() const override;

public:
	static void MarkTransientRecursive(UEditorUtilityWidget* UtilityWidget);

private:
	bool IsWidgetEnabled() const;

	void ChangeTabWorld(UWorld* World, EMapChangeType MapChangeType);

private:
	// Should the widget be enabled when running PIE
	UPROPERTY(Category = Settings, EditDefaultsOnly)
	bool bIsEnabledInPIE = false;

	// Should the widget be enabled when debugging BP
	UPROPERTY(Category = Settings, EditDefaultsOnly)
	bool bIsEnabledInDebugging = false;

	FName RegistrationName;

	TWeakPtr<SDockTab> CreatedTab;

	UPROPERTY(Transient)
	TObjectPtr<UEditorUtilityWidget> CreatedUMGWidget;

};
