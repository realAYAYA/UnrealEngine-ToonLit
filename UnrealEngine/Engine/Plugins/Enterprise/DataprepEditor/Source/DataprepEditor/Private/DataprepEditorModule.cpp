// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataprepEditorModule.h"

#include "AssetTypeActions_DataprepAsset.h"
#include "AssetTypeActions_DataprepAssetInterface.h"
#include "DataprepAssetProducers.h"
#include "DataprepEditor.h"
#include "DataprepEditorUtils.h"
#include "DataprepEditorStyle.h"
#include "Widgets/DataprepGraph/SDataprepGraphEditor.h"
#include "Widgets/DataprepWidgets.h"
#include "Widgets/SDataprepEditorViewport.h"
#include "Widgets/SDataprepProducersWidget.h"

#include "Developer/AssetTools/Public/IAssetTools.h"
#include "Developer/AssetTools/Public/AssetToolsModule.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ToolMenus.h"
#include "UObject/StrongObjectPtr.h"

#include "Widgets/SNullWidget.h"

// Temporary include remove when the new graph is in place
#include "BlueprintNodes/K2Node_DataprepAction.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "SchemaActions/DataprepSchemaActionUtils.h"
#include "DataprepActionAsset.h"


const FName DataprepEditorAppIdentifier = FName(TEXT("DataprepEditorApp"));

EAssetTypeCategories::Type IDataprepEditorModule::DataprepCategoryBit;

#define LOCTEXT_NAMESPACE "DataprepEditorModule"

class FDataprepEditorModule : public IDataprepEditorModule
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
		FDataprepEditorStyle::Initialize();

		MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
		ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

		// Register asset type actions for DataPrepRecipe class
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

		// Register Dataprep category to group together asset type actions related to Dataprep
		DataprepCategoryBit = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("Dataprep")), LOCTEXT("DataprepAssetCategory", "Dataprep"));

		TSharedPtr<FAssetTypeActions_DataprepAssetInterface> DataprepAssetInterfaceTypeAction = MakeShared<FAssetTypeActions_DataprepAssetInterface>();
		AssetTools.RegisterAssetTypeActions(DataprepAssetInterfaceTypeAction.ToSharedRef());
		AssetTypeActionsArray.Add(DataprepAssetInterfaceTypeAction);

		TSharedPtr<FAssetTypeActions_DataprepAsset> DataprepAssetTypeAction = MakeShareable(new FAssetTypeActions_DataprepAsset);
		AssetTools.RegisterAssetTypeActions(DataprepAssetTypeAction.ToSharedRef());
		AssetTypeActionsArray.Add(DataprepAssetTypeAction);

		TSharedPtr<FAssetTypeActions_DataprepAssetInstance> DataprepAssetInstanceTypeAction = MakeShareable(new FAssetTypeActions_DataprepAssetInstance);
		AssetTools.RegisterAssetTypeActions(DataprepAssetInstanceTypeAction.ToSharedRef());
		AssetTypeActionsArray.Add(DataprepAssetInstanceTypeAction);

		// Register the details customizer
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked< FPropertyEditorModule >( TEXT("PropertyEditor") );
		PropertyModule.RegisterCustomClassLayout( UDataprepAssetProducers::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic( &FDataprepAssetProducersDetails::MakeDetails ) );

		SDataprepGraphEditor::RegisterFactories();

		SDataprepEditorViewport::LoadDefaultSettings();

		UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FDataprepEditorModule::RegisterMenus));

		FDataprepEditorUtils::RegisterBlueprintCallbacks(this);
	}

	virtual void ShutdownModule() override
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);

		FKismetEditorUtilities::UnregisterAutoBlueprintNodeCreation(this);

		SDataprepEditorViewport::ReleaseDefaultMaterials();

		SDataprepGraphEditor::UnRegisterFactories();

		MenuExtensibilityManager.Reset();
		ToolBarExtensibilityManager.Reset();

		// Unregister asset type actions
		if (FModuleManager::Get().IsModuleLoaded(TEXT("AssetTools")))
		{
			IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
			for (TSharedPtr<FAssetTypeActions_Base>& AssetTypeActions : AssetTypeActionsArray)
			{
				AssetTools.UnregisterAssetTypeActions(AssetTypeActions.ToSharedRef());
			}
		}
		AssetTypeActionsArray.Empty();

		FDataprepEditorStyle::Shutdown();

		// Register the details customizer
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked< FPropertyEditorModule >( TEXT("PropertyEditor") );
		PropertyModule.UnregisterCustomClassLayout( TEXT("DataprepAssetProducers") );
	}

	void RegisterMenus()
	{
		// Allow cleanup when module unloads
		FToolMenuOwnerScoped OwnerScoped(this);
		{
			UToolMenu* Menu = UToolMenus::Get()->RegisterMenu("DataprepEditor.AssetContextMenu");
			Menu->AddSection("AssetActions", LOCTEXT("AssetActionsMenuHeading", "Asset Actions"));
		}
		{
			UToolMenu* Menu = UToolMenus::Get()->RegisterMenu("DataprepEditor.SceneOutlinerContextMenu");
			Menu->AddSection("SceneOutlinerActions", LOCTEXT("SceneOutlinerMenuHeading", "Scene Outliner"));
		}
	}

	/** Gets the extensibility managers for outside entities to extend datasmith data prep editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() override { return MenuExtensibilityManager; }
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() override { return ToolBarExtensibilityManager; }

	virtual TSharedRef<SWidget> CreateDataprepProducersWidget(UDataprepAssetProducers* AssetProducers) override
	{
		return AssetProducers ? SNew(SDataprepProducersWidget, AssetProducers) : SNullWidget::NullWidget;
	}

	virtual TSharedRef<SWidget> CreateDataprepDetailsView(UObject* ObjectToDetail) override
	{
		if(ObjectToDetail)
		{
			return SNew(SDataprepDetailsView).Object( ObjectToDetail );
		}

		return SNullWidget::NullWidget;
	}

private:
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;
	TArray<TSharedPtr<FAssetTypeActions_Base>> AssetTypeActionsArray;
};

IMPLEMENT_MODULE(FDataprepEditorModule, DataprepEditor)

#undef LOCTEXT_NAMESPACE
