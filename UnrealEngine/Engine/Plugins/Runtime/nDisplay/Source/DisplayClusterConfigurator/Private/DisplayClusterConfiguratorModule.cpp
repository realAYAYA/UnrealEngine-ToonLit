// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterConfiguratorModule.h"
#include "DisplayClusterConfiguratorStyle.h"
#include "DisplayClusterConfiguratorCommands.h"
#include "DisplayClusterConfiguratorAssetTypeActions.h"
#include "DisplayClusterConfiguratorVersionUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "PropertyEditorModule.h"
#include "Settings/DisplayClusterConfiguratorSettings.h"
#include "Views/Details/DisplayClusterRootActorDetailsCustomization.h"
#include "Views/OutputMapping/DisplayClusterConfiguratorOutputMappingCommands.h"
#include "DisplayClusterConfiguratorLog.h"

#include "Views/Details/DisplayClusterConfiguratorBaseTypeCustomization.h"
#include "Views/Details/DisplayClusterEditorPropertyReferenceTypeCustomization.h"
#include "Views/Details/DisplayClusterConfiguratorBaseDetailCustomization.h"
#include "Views/Details/Cluster/DisplayClusterConfiguratorDataDetailsCustomization.h"
#include "Views/Details/Cluster/DisplayClusterConfiguratorClusterDetailsCustomization.h"
#include "Views/Details/Cluster/DisplayClusterConfiguratorExternalImageTypeCustomization.h"
#include "Views/Details/Cluster/DisplayClusterConfiguratorGenerateMipsCustomization.h"
#include "Views/Details/Cluster/DisplayClusterConfiguratorNodeSelectionCustomization.h"
#include "Views/Details/Cluster/DisplayClusterConfiguratorClusterReferenceListCustomization.h"
#include "Views/Details/Cluster/DisplayClusterConfiguratorViewportDetailsCustomization.h"
#include "Views/Details/Cluster/DisplayClusterConfiguratorViewportRemapCustomization.h"
#include "Views/Details/Cluster/DisplayClusterConfiguratorRectangleCustomization.h"
#include "Views/Details/Components/DisplayClusterConfiguratorScreenComponentDetailsCustomization.h"
#include "Views/Details/Components/DisplayClusterICVFXCameraComponentDetailsCustomization.h"
#include "Views/Details/Media/DisplayClusterConfiguratorICVFXMediaCustomization.h"
#include "Views/Details/Media/DisplayClusterConfiguratorMediaFullFrameCustomization.h"
#include "Views/Details/Media/DisplayClusterConfiguratorMediaTileCustomization.h"
#include "Views/Details/Policies/DisplayClusterConfiguratorPolicyDetailCustomization.h"

#include "Blueprints/DisplayClusterBlueprint.h"
#include "Components/DisplayClusterScreenComponent.h"
#include "Components/DisplayClusterICVFXCameraComponent.h"
#include "Misc/DisplayClusterObjectRef.h"
#include "DisplayClusterRootActor.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "ISettingsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetTypeCategories.h"
#include "DisplayClusterConfiguratorBlueprintEditor.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "ActorFactories/ActorFactoryBlueprint.h"
#include "ScopedTransaction.h"
#include "SEditorViewportToolBarButton.h"
#include "Subsystems/PanelExtensionSubsystem.h"

#define REGISTER_PROPERTY_LAYOUT(PropertyType, CustomizationType) { \
	const FName LayoutName = PropertyType::StaticStruct()->GetFName(); \
	RegisteredPropertyLayoutNames.Add(LayoutName); \
	PropertyModule.RegisterCustomPropertyTypeLayout(LayoutName, \
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&CustomizationType::MakeInstance)); \
}

#define REGISTER_OBJECT_LAYOUT(ObjectType, CustomizationType) { \
	const FName LayoutName = ObjectType::StaticClass()->GetFName(); \
	RegisteredClassLayoutNames.Add(LayoutName); \
	PropertyModule.RegisterCustomClassLayout(LayoutName, \
		FOnGetDetailCustomizationInstance::CreateStatic(&CustomizationType::MakeInstance)); \
}

#define LOCTEXT_NAMESPACE "DisplayClusterConfigurator"

namespace UE::DisplayClusterConfiguratorModuleConstants
{
	const static FName LevelEditorViewportExtensionIdentifier = TEXT("DisplayClusterLevelViewportExtension");
}

void FDisplayClusterConfiguratorModule::StartupModule()
{
	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FDisplayClusterConfiguratorModule::OnPostEngineInit);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	/*
	 * Hack for instanced property sync.
	 *
	 * We must clear CPF_EditConst for these properties. They are VisibleInstanceOnly but we are modifying them through their handles
	 * programmatically. If CPF_EditConst is present that operation will fail. We do not want them to be editable on the details panel either.
	 */
	{
		FProperty* Property = FindFProperty<FProperty>(UDisplayClusterConfigurationCluster::StaticClass(), GET_MEMBER_NAME_CHECKED(UDisplayClusterConfigurationCluster, Nodes));
		Property->ClearPropertyFlags(CPF_EditConst);
	}
	
	{
		FProperty* Property = FindFProperty<FProperty>(UDisplayClusterConfigurationClusterNode::StaticClass(), GET_MEMBER_NAME_CHECKED(UDisplayClusterConfigurationClusterNode, Viewports));
		Property->ClearPropertyFlags(CPF_EditConst);
	}

	// Create a custom menu category.
	const EAssetTypeCategories::Type AssetCategoryBit = AssetTools.RegisterAdvancedAssetCategory(
	FName(TEXT("nDisplay")), LOCTEXT("nDisplayAssetCategory", "nDisplay"));
	
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FDisplayClusterConfiguratorAssetTypeActions(AssetCategoryBit)));
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FDisplayClusterConfiguratorActorAssetTypeActions(EAssetTypeCategories::None)));

	RegisterCustomLayouts();
	RegisterSettings();
	RegisterSectionMappings();
	
	FDisplayClusterConfiguratorStyle::Get();

	FDisplayClusterConfiguratorCommands::Register();
	FDisplayClusterConfiguratorOutputMappingCommands::Register();

	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	// Register blueprint compiler -- primarily seems to be used when creating a new BP.
	IKismetCompilerInterface& KismetCompilerModule = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>(KISMET_COMPILER_MODULENAME);
	KismetCompilerModule.GetCompilers().Add(&BlueprintCompiler);

	// This is needed for actually pressing compile on the BP.
	FKismetCompilerContext::RegisterCompilerForBP(UDisplayClusterBlueprint::StaticClass(), &FDisplayClusterConfiguratorModule::GetCompilerForDisplayClusterBP);

	const UDisplayClusterConfiguratorEditorSettings* Settings = GetDefault<UDisplayClusterConfiguratorEditorSettings>();
	if (Settings->bUpdateAssetsOnStartup)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		FilesLoadedHandle = AssetRegistryModule.Get().OnFilesLoaded().AddStatic(&FDisplayClusterConfiguratorVersionUtils::UpdateBlueprintsToNewVersion);
	}
}

void FDisplayClusterConfiguratorModule::ShutdownModule()
{
	FCoreDelegates::OnPostEngineInit.RemoveAll(this);

	if (FAssetToolsModule* AssetTools = FModuleManager::GetModulePtr<FAssetToolsModule>("AssetTools"))
	{
		for (int32 IndexAction = 0; IndexAction < CreatedAssetTypeActions.Num(); ++IndexAction)
		{
			AssetTools->Get().UnregisterAssetTypeActions(CreatedAssetTypeActions[IndexAction].ToSharedRef());
		}
	}

	UnregisterSettings();
	UnregisterCustomLayouts();
	UnregisterSectionMappings();

	MenuExtensibilityManager.Reset();
	ToolBarExtensibilityManager.Reset();

	UnregisterPanelExtensions();

	IKismetCompilerInterface& KismetCompilerModule = FModuleManager::GetModuleChecked<IKismetCompilerInterface>("KismetCompiler");
	KismetCompilerModule.GetCompilers().Remove(&BlueprintCompiler);

	if (FilesLoadedHandle.IsValid() && FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().OnFilesLoaded().Remove(FilesLoadedHandle);
	}
}

void FDisplayClusterConfiguratorModule::OnPostEngineInit()
{
	RegisterPanelExtensions();
}

const FDisplayClusterConfiguratorCommands& FDisplayClusterConfiguratorModule::GetCommands() const
{
	return FDisplayClusterConfiguratorCommands::Get();
}

void FDisplayClusterConfiguratorModule::RegisterAssetTypeAction(IAssetTools& AssetTools,
	TSharedRef<IAssetTypeActions> Action)
{
	AssetTools.RegisterAssetTypeActions(Action);
	CreatedAssetTypeActions.Add(Action);
}

void FDisplayClusterConfiguratorModule::RegisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Editor", "Plugins", "nDisplayEditor",
			LOCTEXT("nDisplayEditorName", "nDisplay Editor"),
			LOCTEXT("nDisplayEditorDescription", "Configure settings for the nDisplay Editor."),
			GetMutableDefault<UDisplayClusterConfiguratorEditorSettings>());
	}
}

void FDisplayClusterConfiguratorModule::UnregisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Editor", "ContentEditors", "nDisplayEditor");
	}
}

void FDisplayClusterConfiguratorModule::RegisterCustomLayouts()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	/**
	 * CLASSES
	 */
	REGISTER_OBJECT_LAYOUT(ADisplayClusterRootActor, FDisplayClusterRootActorDetailsCustomization);
	REGISTER_OBJECT_LAYOUT(UDisplayClusterConfigurationData, FDisplayClusterConfiguratorDataDetailsCustomization);
	REGISTER_OBJECT_LAYOUT(UDisplayClusterConfigurationCluster, FDisplayClusterConfiguratorClusterDetailsCustomization);
	REGISTER_OBJECT_LAYOUT(UDisplayClusterConfigurationClusterNode, FDisplayClusterConfiguratorBaseDetailCustomization);
	REGISTER_OBJECT_LAYOUT(UDisplayClusterConfigurationViewport, FDisplayClusterConfiguratorViewportDetailsCustomization);
	REGISTER_OBJECT_LAYOUT(UDisplayClusterScreenComponent, FDisplayClusterConfiguratorScreenDetailsCustomization);	
	REGISTER_OBJECT_LAYOUT(UDisplayClusterICVFXCameraComponent, FDisplayClusterICVFXCameraComponentDetailsCustomization);
	
	/**
	 * STRUCTS
	 */
	REGISTER_PROPERTY_LAYOUT(FDisplayClusterConfigurationICVFX_VisibilityList, FDisplayClusterConfiguratorBaseTypeCustomization);
	REGISTER_PROPERTY_LAYOUT(FDisplayClusterEditorPropertyReference, FDisplayClusterEditorPropertyReferenceTypeCustomization);
	REGISTER_PROPERTY_LAYOUT(FDisplayClusterConfigurationProjection, FDisplayClusterConfiguratorProjectionCustomization);
	REGISTER_PROPERTY_LAYOUT(FDisplayClusterConfigurationRenderSyncPolicy, FDisplayClusterConfiguratorRenderSyncPolicyCustomization);
	REGISTER_PROPERTY_LAYOUT(FDisplayClusterConfigurationInputSyncPolicy, FDisplayClusterConfiguratorInputSyncPolicyCustomization);
	REGISTER_PROPERTY_LAYOUT(FDisplayClusterConfigurationExternalImage, FDisplayClusterConfiguratorExternalImageTypeCustomization);
	REGISTER_PROPERTY_LAYOUT(FDisplayClusterComponentRef, FDisplayClusterConfiguratorBaseTypeCustomization);
	REGISTER_PROPERTY_LAYOUT(FDisplayClusterConfigurationOCIOProfile, FDisplayClusterConfiguratorOCIOProfileCustomization);
	REGISTER_PROPERTY_LAYOUT(FDisplayClusterConfigurationViewport_PerViewportColorGrading, FDisplayClusterConfiguratorPerViewportColorGradingCustomization);
	REGISTER_PROPERTY_LAYOUT(FDisplayClusterConfigurationViewport_PerNodeColorGrading, FDisplayClusterConfiguratorPerNodeColorGradingCustomization);
	REGISTER_PROPERTY_LAYOUT(FDisplayClusterConfigurationPostRender_GenerateMips, FDisplayClusterConfiguratorGenerateMipsCustomization);
	REGISTER_PROPERTY_LAYOUT(FDisplayClusterConfigurationClusterItemReferenceList, FDisplayClusterConfiguratorClusterReferenceListCustomization);
	REGISTER_PROPERTY_LAYOUT(FDisplayClusterConfigurationViewport_RemapData, FDisplayClusterConfiguratorViewportRemapCustomization);
	REGISTER_PROPERTY_LAYOUT(FDisplayClusterConfigurationRectangle, FDisplayClusterConfiguratorRectangleCustomization);
	REGISTER_PROPERTY_LAYOUT(FDisplayClusterConfigurationMediaICVFX, FDisplayClusterConfiguratorICVFXMediaCustomization);
	REGISTER_PROPERTY_LAYOUT(FDisplayClusterConfigurationMediaUniformTileInput,  FDisplayClusterConfiguratorMediaInputTileCustomization);
	REGISTER_PROPERTY_LAYOUT(FDisplayClusterConfigurationMediaUniformTileOutput, FDisplayClusterConfiguratorMediaOutputTileCustomization);
	REGISTER_PROPERTY_LAYOUT(FDisplayClusterConfigurationMediaInput,  FDisplayClusterConfiguratorMediaFullFrameInputCustomization);
	REGISTER_PROPERTY_LAYOUT(FDisplayClusterConfigurationMediaOutput, FDisplayClusterConfiguratorMediaFullFrameOutputCustomization);
	REGISTER_PROPERTY_LAYOUT(FDisplayClusterConfigurationMediaInputGroup,  FDisplayClusterConfiguratorMediaFullFrameInputCustomization);
	REGISTER_PROPERTY_LAYOUT(FDisplayClusterConfigurationMediaOutputGroup, FDisplayClusterConfiguratorMediaFullFrameOutputCustomization);
}

void FDisplayClusterConfiguratorModule::UnregisterCustomLayouts()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	for (const FName& LayoutName : RegisteredClassLayoutNames)
	{
		PropertyModule.UnregisterCustomClassLayout(LayoutName);
	}
	
	for (const FName& LayoutName : RegisteredPropertyLayoutNames)
	{
		PropertyModule.UnregisterCustomPropertyTypeLayout(LayoutName);
	}

	RegisteredPropertyLayoutNames.Empty();
}

void FDisplayClusterConfiguratorModule::RegisterSectionMappings()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	
	// Root Actor
	{
		const TSharedRef<FPropertySection> Section = PropertyModule.FindOrCreateSection(ADisplayClusterRootActor::StaticClass()->GetFName(),
			DisplayClusterConfigurationStrings::categories::ViewportsCategory, LOCTEXT("Viewports", "Viewports"));
		Section->AddCategory(DisplayClusterConfigurationStrings::categories::ViewportsCategory);
	}
	{
		const TSharedRef<FPropertySection> Section = PropertyModule.FindOrCreateSection(ADisplayClusterRootActor::StaticClass()->GetFName(),
			DisplayClusterConfigurationStrings::categories::ICVFXCategory, LOCTEXT("In-Camera VFX", "In-Camera VFX"));
		Section->AddCategory(DisplayClusterConfigurationStrings::categories::ICVFXCategory);
	}
	{
		const TSharedRef<FPropertySection> Section = PropertyModule.FindOrCreateSection(ADisplayClusterRootActor::StaticClass()->GetFName(),
			DisplayClusterConfigurationStrings::categories::ColorGradingCategory, LOCTEXT("Color Grading", "Color Grading"));
		Section->AddCategory(DisplayClusterConfigurationStrings::categories::ColorGradingCategory);
	}
	{
		const TSharedRef<FPropertySection> Section = PropertyModule.FindOrCreateSection(ADisplayClusterRootActor::StaticClass()->GetFName(),
			DisplayClusterConfigurationStrings::categories::OCIOCategory, LOCTEXT("OCIO", "OCIO"));
		Section->AddCategory(DisplayClusterConfigurationStrings::categories::OCIOCategory);
	}
	{
		const TSharedRef<FPropertySection> Section = PropertyModule.FindOrCreateSection(ADisplayClusterRootActor::StaticClass()->GetFName(),
			DisplayClusterConfigurationStrings::categories::LightcardCategory, LOCTEXT("Light Cards", "Light Cards"));
		Section->AddCategory(DisplayClusterConfigurationStrings::categories::LightcardCategory);
	}
	{
		const TSharedRef<FPropertySection> Section = PropertyModule.FindOrCreateSection(ADisplayClusterRootActor::StaticClass()->GetFName(),
			DisplayClusterConfigurationStrings::categories::PreviewCategory, LOCTEXT("Editor Preview", "Editor Preview"));
		Section->AddCategory(DisplayClusterConfigurationStrings::categories::PreviewCategory);
	}
	{
		const TSharedRef<FPropertySection> Section = PropertyModule.FindOrCreateSection(ADisplayClusterRootActor::StaticClass()->GetFName(),
			DisplayClusterConfigurationStrings::categories::PreviewInGameCategory, LOCTEXT("Preview In Game", "Preview In Game"));
		Section->AddCategory(DisplayClusterConfigurationStrings::categories::PreviewInGameCategory);
	}

	// ICVFX Component
	{
		const TSharedRef<FPropertySection> Section = PropertyModule.FindOrCreateSection(UDisplayClusterICVFXCameraComponent::StaticClass()->GetFName(),
		DisplayClusterConfigurationStrings::categories::ICVFXCategory, LOCTEXT("InnerFrustumSectionLabel", "Inner Frustum"));
		Section->AddCategory(DisplayClusterConfigurationStrings::categories::ICVFXCategory);
	}
	{
		const TSharedRef<FPropertySection> Section = PropertyModule.FindOrCreateSection(UDisplayClusterICVFXCameraComponent::StaticClass()->GetFName(),
			DisplayClusterConfigurationStrings::categories::ICVFXCameraCategory, LOCTEXT("InnerFrustumCameraSectionLabel", "Camera"));
		Section->AddCategory(DisplayClusterConfigurationStrings::categories::ICVFXCameraCategory);
	}
	{
		const TSharedRef<FPropertySection> Section = PropertyModule.FindOrCreateSection(UDisplayClusterICVFXCameraComponent::StaticClass()->GetFName(),
		DisplayClusterConfigurationStrings::categories::CameraColorGradingCategory, LOCTEXT("InnerFrustumColorGradingLabel", "Color Grading"));
		Section->AddCategory(DisplayClusterConfigurationStrings::categories::CameraColorGradingCategory);
	}
	{
		const TSharedRef<FPropertySection> Section = PropertyModule.FindOrCreateSection(UDisplayClusterICVFXCameraComponent::StaticClass()->GetFName(),
			DisplayClusterConfigurationStrings::categories::OCIOCategory, LOCTEXT("OCIO", "OCIO"));
		Section->AddCategory(DisplayClusterConfigurationStrings::categories::OCIOCategory);
	}
	{
		const TSharedRef<FPropertySection> Section = PropertyModule.FindOrCreateSection(UDisplayClusterICVFXCameraComponent::StaticClass()->GetFName(),
			DisplayClusterConfigurationStrings::categories::MediaCategory, LOCTEXT("Media", "Media"));
		Section->AddCategory(DisplayClusterConfigurationStrings::categories::MediaCategory);
	}
	{
		const TSharedRef<FPropertySection> Section = PropertyModule.FindOrCreateSection(UDisplayClusterICVFXCameraComponent::StaticClass()->GetFName(),
			DisplayClusterConfigurationStrings::categories::ChromaKeyCategory, LOCTEXT("Chromakey", "Chromakey"));
		Section->AddCategory(DisplayClusterConfigurationStrings::categories::ChromaKeyCategory);
	}
}

void FDisplayClusterConfiguratorModule::UnregisterSectionMappings()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor") && FSlateApplication::IsInitialized())
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RemoveSection(ADisplayClusterRootActor::StaticClass()->GetFName(), DisplayClusterConfigurationStrings::categories::ViewportsCategory);
		PropertyModule.RemoveSection(ADisplayClusterRootActor::StaticClass()->GetFName(), DisplayClusterConfigurationStrings::categories::ICVFXCategory);
		PropertyModule.RemoveSection(ADisplayClusterRootActor::StaticClass()->GetFName(), DisplayClusterConfigurationStrings::categories::ColorGradingCategory);
		PropertyModule.RemoveSection(ADisplayClusterRootActor::StaticClass()->GetFName(), DisplayClusterConfigurationStrings::categories::OCIOCategory);
		PropertyModule.RemoveSection(ADisplayClusterRootActor::StaticClass()->GetFName(), DisplayClusterConfigurationStrings::categories::LightcardCategory);

		PropertyModule.RemoveSection(UDisplayClusterICVFXCameraComponent::StaticClass()->GetFName(), DisplayClusterConfigurationStrings::categories::ICVFXCategory);
		PropertyModule.RemoveSection(UDisplayClusterICVFXCameraComponent::StaticClass()->GetFName(), DisplayClusterConfigurationStrings::categories::CameraColorGradingCategory);
		PropertyModule.RemoveSection(UDisplayClusterICVFXCameraComponent::StaticClass()->GetFName(), DisplayClusterConfigurationStrings::categories::OCIOCategory);
		PropertyModule.RemoveSection(UDisplayClusterICVFXCameraComponent::StaticClass()->GetFName(), DisplayClusterConfigurationStrings::categories::ChromaKeyCategory);
	}
}

void FDisplayClusterConfiguratorModule::RegisterPanelExtensions()
{
	if (GEditor)
	{
		FDisplayClusterConfiguratorBlueprintEditor::RegisterPanelExtensionFactory();
	
		if (UPanelExtensionSubsystem* PanelExtensionSubsystem = GEditor->GetEditorSubsystem<UPanelExtensionSubsystem>())
		{
			if (!PanelExtensionSubsystem->IsPanelFactoryRegistered(UE::DisplayClusterConfiguratorModuleConstants::LevelEditorViewportExtensionIdentifier))
			{
				FPanelExtensionFactory LevelViewportToolbarExtensionWidget;
				LevelViewportToolbarExtensionWidget.CreateExtensionWidget = FPanelExtensionFactory::FCreateExtensionWidget::CreateStatic(&FDisplayClusterConfiguratorModule::OnExtendLevelEditorViewportToolbar);
				LevelViewportToolbarExtensionWidget.Identifier = UE::DisplayClusterConfiguratorModuleConstants::LevelEditorViewportExtensionIdentifier;

				PanelExtensionSubsystem->RegisterPanelFactory(TEXT("LevelViewportToolBar.LeftExtension"), LevelViewportToolbarExtensionWidget);
			}
		}
	}
}

void FDisplayClusterConfiguratorModule::UnregisterPanelExtensions()
{
	if (GEditor)
	{
		FDisplayClusterConfiguratorBlueprintEditor::UnregisterPanelExtensionFactory();

		if (UPanelExtensionSubsystem* PanelExtensionSubsystem = GEditor->GetEditorSubsystem<UPanelExtensionSubsystem>())
		{
			PanelExtensionSubsystem->UnregisterPanelFactory(UE::DisplayClusterConfiguratorModuleConstants::LevelEditorViewportExtensionIdentifier);
		}
	}
}

TSharedPtr<FKismetCompilerContext> FDisplayClusterConfiguratorModule::GetCompilerForDisplayClusterBP(UBlueprint* BP,
                                                                                                     FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompileOptions)
{
	return TSharedPtr<FKismetCompilerContext>(new FDisplayClusterConfiguratorKismetCompilerContext(CastChecked<UDisplayClusterBlueprint>(BP), InMessageLog, InCompileOptions));
}

TSharedRef<SWidget> FDisplayClusterConfiguratorModule::OnExtendLevelEditorViewportToolbar(FWeakObjectPtr ExtensionContext)
{
	return SNew(SEditorViewportToolBarButton)	
	.ButtonType(EUserInterfaceActionType::Button)
	.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("EditorViewportToolBar.WarningButton"))
	.OnClicked_Static(OnViewportsFrozenWarningClicked)
	.Visibility_Static(GetViewportsFrozenWarningVisibility)
	.ToolTipText(LOCTEXT("DisplayClusterViewportsFrozenOff_ToolTip", "nDisplay viewports are frozen. Click to unfreeze the viewports."))
	.Content()
	[
		SNew(STextBlock)
		.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("SmallText"))
		.Text(LOCTEXT("DisplayClusterViewportsFrozen", "nDisplay Viewports Frozen"))
	];
}

FReply FDisplayClusterConfiguratorModule::OnViewportsFrozenWarningClicked()
{
	if (GEditor)
	{
		FScopedTransaction Transaction(LOCTEXT("UnfreezeViewports", "Unfreeze viewports"));
		
		for (TActorIterator<ADisplayClusterRootActor> It(GEditor->GetEditorWorldContext().World()); It; ++It)
		{
			It->SetFreezeOuterViewports(false);
		}
	}

	return FReply::Handled();
}

EVisibility FDisplayClusterConfiguratorModule::GetViewportsFrozenWarningVisibility()
{
	if (GEditor)
	{
		if (const UWorld* World = GEditor->GetEditorWorldContext().World())
		{
			for (TActorIterator<ADisplayClusterRootActor> It(World); It; ++It)
			{
				const UDisplayClusterConfigurationData* ConfigData = It->GetConfigData();
				if (ConfigData && ConfigData->StageSettings.bFreezeRenderOuterViewports)
				{
					return EVisibility::Visible;
				}
			}
		}
	}

	return EVisibility::Collapsed;
}

IMPLEMENT_MODULE(FDisplayClusterConfiguratorModule, DisplayClusterConfigurator);

#undef LOCTEXT_NAMESPACE
