// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintEditor.h"

class UMVVMViewModelBlueprint;

/**
 * Viewmodel blueprint editor (extends Blueprint editor)
 */
class MODELVIEWVIEWMODELEDITOR_API FMVVMViewModelBlueprintEditor : public FBlueprintEditor
{
private:
	using Super = FBlueprintEditor;


public:
	FMVVMViewModelBlueprintEditor() = default;
	//virtual ~FMVVMViewModelBlueprintEditor();

	void InitViewModelBlueprintEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, const TArray<UBlueprint*>& InBlueprints, bool bShouldOpenInDefaultsMode);

	//~ Begin FBlueprintEditor interface
	//virtual void Tick(float DeltaTime) override;
	//virtual void PostUndo(bool bSuccessful) override;
	//virtual void PostRedo(bool bSuccessful) override;
	//virtual void Compile() override;
	//~ End FBlueprintEditor interface

	// ~ Begin IToolkit interface
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual void InitToolMenuContext(FToolMenuContext& MenuContext) override;
	// ~ End IToolkit interface

	/** @return The viewmodel blueprint currently being edited in this editor */
	UMVVMViewModelBlueprint* GetViewModelBlueprintObj() const;

protected:
	//~ Begin FBlueprintEditor interface
	virtual void RegisterApplicationModes(const TArray<UBlueprint*>& Blueprints, bool bShouldOpenInDefaultsMode, bool bNewlyCreated = false) override;
	virtual FGraphAppearanceInfo GetGraphAppearance(class UEdGraph* Graph) const override;
	virtual bool NewDocument_IsVisibleForType(ECreatedDocumentType GraphType) const override;
	virtual bool IsSectionVisible(NodeSectionID::Type SectionID) const override;
	//~ End FBlueprintEditor interface
};
