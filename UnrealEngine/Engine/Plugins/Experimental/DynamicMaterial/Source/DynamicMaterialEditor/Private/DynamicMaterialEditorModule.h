// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMEDefs.h"
#include "DMObjectMaterialProperty.h"
#include "IDynamicMaterialEditorModule.h"
#include "Templates/SharedPointer.h"
#include "TickableEditorObject.h"

class AActor;
class FDMMaterialFunctionLibrary;
class FUICommandList;
class IAssetTypeActions;
class ILevelEditor;
class SDMEditor;
class SDMComponentEdit;
class SDockTab;
class SWidget;
class UDMMaterialComponent;
class UDMMaterialStageSource;
class UDynamicMaterialInstance;
class UDynamicMaterialModel;
struct FDMBuildRequestList;

DECLARE_LOG_CATEGORY_EXTERN(LogDynamicMaterialEditor, Log, All);

namespace UE::DynamicMaterialEditor
{
	constexpr bool bMultipleSlotPropertiesEnabled = false;
	constexpr bool bGlobalValuesEnabled = false;
	constexpr bool bAdvancedSlotsEnabled = false;
}

DECLARE_MULTICAST_DELEGATE(FDMOnUIValueUpdate);

/** Takes a UMaterialValue and returns the widget used to edit it. */
DECLARE_DELEGATE_RetVal_TwoParams(TSharedPtr<SWidget>, FDMCreateValueEditWidgetDelegate, const TSharedPtr<SDMComponentEdit>&, UDMMaterialValue*);

/** Creates property rows in the edit widget. */
DECLARE_DELEGATE_FourParams(FDMComponentPropertyRowGeneratorDelegate, const TSharedRef<SDMComponentEdit>&, UDMMaterialComponent*,
	TArray<FDMPropertyHandle>&, TSet<UDMMaterialComponent*>&)

struct FDMBuildRequestEntry
{
	FString AssetPath;
	bool bDirtyAssets;

	bool operator==(const FDMBuildRequestEntry& Other) const
	{
		return AssetPath == Other.AssetPath;
	}

	friend uint32 GetTypeHash(const FDMBuildRequestEntry& InEntry)
	{
		return GetTypeHash(InEntry.AssetPath);
	}
};

/**
 * Material Designer - Build your own materials in a slimline editor!
 */
class FDynamicMaterialEditorModule : public IDynamicMaterialEditorModule, public FTickableEditorObject
{
public:
	static const FName TabId;
	static FDMOnUIValueUpdate& GetOnUIValueUpdate() { return OnUIValueUpdate; }

	static FDynamicMaterialEditorModule& Get();

	static void RegisterValueEditWidgetDelegate(UClass* InClass, FDMCreateValueEditWidgetDelegate InValueEditBodyDelegate);
	template<class InValClass, class InEditClass> static void RegisterValueEditWidgetDelegate();
	static FDMCreateValueEditWidgetDelegate GetValueEditWidgetDelegate(UClass* InClass);
	static TSharedPtr<SWidget> CreateEditWidgetForValue(const TSharedPtr<SDMComponentEdit>& InComponentEditWidget, UDMMaterialValue* InValue);

	static void RegisterComponentPropertyRowGeneratorDelegate(UClass* InClass, FDMComponentPropertyRowGeneratorDelegate InComponentPropertyRowGeneratorDelegate);
	template<class InObjClass, class InGenClass> static void RegisterComponentPropertyRowGeneratorDelegate();
	static FDMComponentPropertyRowGeneratorDelegate GetComponentPropertyRowGeneratorDelegate(UClass* InClass);
	static void GeneratorComponentPropertyRows(const TSharedRef<SDMComponentEdit>& InComponentEditWidget, UDMMaterialComponent* InComponent, 
		TArray<FDMPropertyHandle>& InOutPropertyRows, TSet<UDMMaterialComponent*>& InOutProcessedObjects);

	static FDMGetObjectMaterialPropertiesDelegate GetCustomMaterialPropertyGenerator(UClass* InClass);

	/** With a provided world, the editor will bind to the MD world subsystem to receive model changes. */
	static TSharedRef<SWidget> CreateEditor(UDynamicMaterialModel* InMaterialModel, UWorld* InAssetEditorWorld);
	static TSharedRef<SWidget> CreateEmptyTabContent();

	FDynamicMaterialEditorModule();

	//~ Begin IDynamicMaterialEditorModule
	virtual void OpenEditor(UWorld* InWorld) override;
	virtual void RegisterCustomMaterialPropertyGenerator(UClass* InClass, FDMGetObjectMaterialPropertiesDelegate InGenerator) override;
	//~ End IDynamicMaterialEditorModule

	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

	void SetDynamicMaterialModel(UDynamicMaterialModel* InMaterialModel, UWorld* InWorld, bool bInInvokeTab);
	void SetDynamicMaterialObjectProperty(const FDMObjectMaterialProperty& InObjectProperty, UWorld* InWorld, bool bInInvokeTab);
	void SetDynamicMaterialInstance(UDynamicMaterialInstance* InInstance, UWorld* InWorld, bool bInInvokeTab);
	void SetDynamicMaterialActor(AActor* InActor, UWorld* InWorld, bool bInInvokeTab);
	void ClearDynamicMaterialModel(UWorld* InWorld);

	//~ Begin FTickableEditorObject
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
	virtual TStatId GetStatId() const override;
	//~ End FTickableEditorObject

	void AddBuildRequest(UObject* InToBuild, bool bInDirtyAssets);

	const TSharedRef<FUICommandList>& GetCommandList() const { return CommandList; }

protected:
	static TMap<UClass*, FDMCreateValueEditWidgetDelegate> ValueEditWidgetDelegates;
	static TMap<UClass*, FDMComponentPropertyRowGeneratorDelegate> ComponentPropertyRowGenerators;
	static TMap<UClass*, FDMGetObjectMaterialPropertiesDelegate> CustomMaterialPropertyGenerators;
	static FDMOnUIValueUpdate OnUIValueUpdate;

	TSet<FDMBuildRequestEntry> BuildRequestList;
	TSharedRef<FUICommandList> CommandList;

	void ProcessBuildRequest(UObject* InToBuild, bool bInDirtyAssets);

	void MapCommands();
	void UnmapCommands();
};

template <class InValClass, class InEditClass>
void FDynamicMaterialEditorModule::RegisterValueEditWidgetDelegate()
{
	RegisterValueEditWidgetDelegate(
		InValClass::StaticClass(),
		FDMCreateValueEditWidgetDelegate::CreateStatic(
			&InEditClass::CreateEditWidget
		)
	);
}

template <class InObjClass, class InGenClass>
void FDynamicMaterialEditorModule::RegisterComponentPropertyRowGeneratorDelegate()
{
	RegisterComponentPropertyRowGeneratorDelegate(
		InObjClass::StaticClass(),
		FDMComponentPropertyRowGeneratorDelegate::CreateSP(
			InGenClass::Get(),
			&InGenClass::AddComponentProperties
		)
	);
}
