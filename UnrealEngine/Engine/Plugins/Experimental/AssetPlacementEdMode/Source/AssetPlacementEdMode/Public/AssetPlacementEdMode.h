// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/DefaultEdMode.h"
#include "Elements/Framework/TypedElementHandle.h"

#include "AssetPlacementEdMode.generated.h"

class UAssetPlacementSettings;

UCLASS()
class UAssetPlacementEdMode : public UEdModeDefault
{
	GENERATED_BODY()

public:
	constexpr static const TCHAR AssetPlacementEdModeID[] = TEXT("EM_AssetPlacementEdMode");

	UAssetPlacementEdMode();
	virtual ~UAssetPlacementEdMode();

	////////////////
	// UEdMode interface
	virtual void Enter() override;
	virtual void Exit() override;
	virtual void CreateToolkit() override;
	virtual bool UsesToolkits() const override;
	virtual TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> GetModeCommands() const override;
	virtual void BindCommands() override;
	virtual bool IsSelectionAllowed(AActor* InActor, bool bInSelection) const override;
	virtual void OnToolStarted(UInteractiveToolManager* Manager, UInteractiveTool* Tool) override;
	virtual void OnToolEnded(UInteractiveToolManager* Manager, UInteractiveTool* Tool) override;
	// End of UEdMode interface
	//////////////////

	////////////////
	// UBaseLegacyWidgetEdMode interface
	virtual bool UsesPropertyWidgets() const override;
	virtual bool ShouldDrawWidget() const override;
	// End of UBaseLegacyWidgetEdMode interface
	//////////////////

	static bool IsEnabled();

protected:
	void ClearSelection();
	bool HasAnyAssetsInPalette() const;
	bool HasActiveSelection() const;
	bool IsInSelectionTool() const;
	void OnSMIsntancedElementsEnabledChanged();

	bool bIsInSelectionTool;
	TWeakObjectPtr<UAssetPlacementSettings> SettingsObjectAsPlacementSettings;
};
