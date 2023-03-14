// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "AssetTypeCategories.h"
#include "IAssetTools.h"
#include "IAssetTypeActions.h"
#include "TickableEditorObject.h"
#include "IDetailsView.h"

#include "InputEditorModule.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogEnhancedInputEditor, Log, All);

////////////////////////////////////////////////////////////////////
// FInputEditorModule

class UInputAction;
class SWindow;

class FInputEditorModule : public IModuleInterface, public FTickableEditorObject
{
public:

	// IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface interface

	// FTickableEditorObject interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(FInputEditorModule, STATGROUP_Tickables); }
	// End FTickableEditorObject interface

	static EAssetTypeCategories::Type GetInputAssetsCategory() { return InputAssetsCategory; }
	
private:
	void RegisterAssetTypeActions(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action)
	{
		AssetTools.RegisterAssetTypeActions(Action);
		CreatedAssetTypeActions.Add(Action);
	}

	void OnMainFrameCreationFinished(TSharedPtr<SWindow> InRootWindow, bool bIsNewProjectWindow);
	
	/** Automatically upgrade the current project to use Enhanced Input if it is currently set to the legacy input classes. */
	void AutoUpgradeDefaultInputClasses();

	static EAssetTypeCategories::Type InputAssetsCategory;
	
	TArray<TSharedPtr<IAssetTypeActions>> CreatedAssetTypeActions;
	
	TSharedPtr<class FSlateStyleSet> StyleSet;
};

////////////////////////////////////////////////////////////////////
// Asset factories

UCLASS()
class INPUTEDITOR_API UInputMappingContext_Factory : public UFactory
{
	GENERATED_UCLASS_BODY()
public:
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;

	/** Set the array of initial actions that the resulting IMC should be populated with */
	void SetInitialActions(TArray<TWeakObjectPtr<UInputAction>> InInitialActions);
	
protected:

	/** An array of Input Actions that the mapping context should be populated with upon creation */
	TArray<TWeakObjectPtr<UInputAction>> InitialActions;
	
};

UCLASS()
class INPUTEDITOR_API UInputAction_Factory : public UFactory
{
	GENERATED_UCLASS_BODY()
public:
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
};

UCLASS()
class INPUTEDITOR_API UPlayerMappableInputConfig_Factory : public UFactory
{
	GENERATED_UCLASS_BODY()
public:
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
};

// TODO: Add trigger/modifier factories and hook up RegisterAssetTypeActions type construction.
//
//UCLASS()
//class INPUTEDITOR_API UInputTrigger_Factory : public UBlueprintFactory
//{
//	GENERATED_UCLASS_BODY()
//};
//
//UCLASS()
//class INPUTEDITOR_API UInputModifier_Factory : public UBlueprintFactory
//{
//	GENERATED_UCLASS_BODY()
//
//	UPROPERTY(EditAnywhere, Category = DataAsset)
//	TSubclassOf<UDataAsset> DataAssetClass;
//};
