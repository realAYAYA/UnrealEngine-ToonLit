// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interfaces/IPluginManager.h"

#include "Brushes/SlateBoxBrush.h"
#include "EdGraphUtilities.h"
#include "Editor.h"
#include "Styling/AppStyle.h"
#include "Features/IModularFeatures.h"
#include "Framework/Application/SlateApplication.h"
#include "ISettingsModule.h"
#include "LevelEditor.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Styling/SlateStyle.h"
#include "Styling/StyleColors.h"
#include "Styling/SlateTypes.h"
#include "Styling/SlateStyleRegistry.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#include "LiveLinkComponentController.h"
#include "LiveLinkComponentDetailCustomization.h"
#include "LiveLinkComponentSettings.h"
#include "LiveLinkControllerBase.h"
#include "LiveLinkControllerBaseDetailCustomization.h"
#include "LiveLinkClient.h"
#include "LiveLinkClientPanel.h"
#include "LiveLinkClientCommands.h"
#include "LiveLinkEditorPrivate.h"
#include "LiveLinkGraphPanelPinFactory.h"
#include "LiveLinkSettings.h"
#include "LiveLinkSourceSettingsDetailCustomization.h"
#include "LiveLinkSubjectKeyDetailCustomization.h"
#include "LiveLinkSubjectNameDetailCustomization.h"
#include "LiveLinkSubjectRepresentationDetailCustomization.h"
#include "LiveLinkTypes.h"
#include "LiveLinkVirtualSubject.h"
#include "LiveLinkVirtualSubjectDetailCustomization.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "VirtualSubjects/LiveLinkBlueprintVirtualSubject.h"

/**
 * Implements the LiveLinkEditor module.
 */

LLM_DEFINE_TAG(LiveLink_LiveLinkEditor);
DEFINE_LOG_CATEGORY(LogLiveLinkEditor);


#define LOCTEXT_NAMESPACE "LiveLinkModule"

static const FName LiveLinkClientTabName(TEXT("LiveLink"));
static const FName LevelEditorModuleName(TEXT("LevelEditor"));


namespace LiveLinkEditorModuleUtils
{
	FString InPluginContent(const FString& RelativePath, const ANSICHAR* Extension)
	{
		static FString ContentDir = IPluginManager::Get().FindPlugin(TEXT("LiveLink"))->GetContentDir();
		return (ContentDir / RelativePath) + Extension;
	}
}

#define IMAGE_BRUSH_SVG(RelativePath, ...) FSlateVectorImageBrush(StyleSet->RootToCoreContentDir(RelativePath, TEXT(".svg")), __VA_ARGS__)
#define IMAGE_PLUGIN_BRUSH( RelativePath, ... ) FSlateImageBrush( LiveLinkEditorModuleUtils::InPluginContent( RelativePath, ".png" ), __VA_ARGS__ )
#define IMAGE_PLUGIN_BRUSH_SVG( RelativePath, ... ) FSlateVectorImageBrush( LiveLinkEditorModuleUtils::InPluginContent( RelativePath, ".svg" ), __VA_ARGS__ )


class FLiveLinkEditorModule : public IModuleInterface
{
public:
	static TSharedPtr<FSlateStyleSet> StyleSet;

	// IModuleInterface interface

	virtual void StartupModule() override
	{
		LLM_SCOPE_BYTAG(LiveLink_LiveLinkEditor);

		static FName LiveLinkStyle(TEXT("LiveLinkStyle"));
		StyleSet = MakeShared<FSlateStyleSet>(LiveLinkStyle);

		bHasRegisteredTabSpawners = false;

		if (FModuleManager::Get().IsModuleLoaded(LevelEditorModuleName))
		{
			RegisterTabSpawner();
		}

		ModulesChangedHandle = FModuleManager::Get().OnModulesChanged().AddRaw(this, &FLiveLinkEditorModule::ModulesChangesCallback);

		FLiveLinkClientCommands::Register();

		const FVector2D Icon8x8(8.0f, 8.0f);
		const FVector2D Icon16x16(16.0f, 16.0f);
		const FVector2D Icon40x40(40.0f, 40.0f);

		StyleSet->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
		StyleSet->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

		StyleSet->Set("LiveLinkClient.Common.Icon", new IMAGE_PLUGIN_BRUSH(TEXT("LiveLink_40x"), Icon40x40));
		StyleSet->Set("LiveLinkClient.Common.Icon.Small", new IMAGE_PLUGIN_BRUSH_SVG("Starship/LiveLink", Icon16x16));

		StyleSet->Set("ClassIcon.LiveLinkPreset",                      new IMAGE_PLUGIN_BRUSH_SVG("Starship/LiveLink", Icon16x16));
		StyleSet->Set("ClassIcon.LiveLinkFrameInterpolationProcessor", new IMAGE_PLUGIN_BRUSH_SVG("Starship/LiveLink", Icon16x16));
		StyleSet->Set("ClassIcon.LiveLinkFramePreProcessor",           new IMAGE_PLUGIN_BRUSH_SVG("Starship/LiveLink", Icon16x16));
		StyleSet->Set("ClassIcon.LiveLinkFrameTranslator",             new IMAGE_PLUGIN_BRUSH_SVG("Starship/LiveLink", Icon16x16));
		StyleSet->Set("ClassIcon.LiveLinkPreset",                      new IMAGE_PLUGIN_BRUSH_SVG("Starship/LiveLink", Icon16x16));
		StyleSet->Set("ClassIcon.LiveLinkRole",                        new IMAGE_PLUGIN_BRUSH_SVG("Starship/LiveLink", Icon16x16));
		StyleSet->Set("ClassIcon.LiveLinkVirtualSubject",              new IMAGE_PLUGIN_BRUSH_SVG("Starship/LiveLink", Icon16x16));

		StyleSet->Set("ClassThumbnail.LiveLinkPreset", new IMAGE_PLUGIN_BRUSH("LiveLink_40x", Icon40x40));

		StyleSet->Set("LiveLinkClient.Common.AddSource", new IMAGE_PLUGIN_BRUSH(TEXT("icon_AddSource_40x"), Icon40x40));
		StyleSet->Set("LiveLinkClient.Common.RemoveSource", new IMAGE_PLUGIN_BRUSH(TEXT("icon_RemoveSource_40x"), Icon40x40));
		StyleSet->Set("LiveLinkClient.Common.RemoveAllSources", new IMAGE_PLUGIN_BRUSH(TEXT("icon_RemoveSource_40x"), Icon40x40));

		StyleSet->Set("LiveLinkController.WarningIcon", new IMAGE_BRUSH_SVG("Starship/Common/alert-circle", Icon16x16, FStyleColors::Warning));
		
		FButtonStyle Button = FButtonStyle()
			.SetNormal(FSlateBoxBrush(StyleSet->RootToContentDir("Common/ButtonHoverHint.png"), FMargin(4 / 16.0f), FLinearColor(1, 1, 1, 0.15f)))
			.SetHovered(FSlateBoxBrush(StyleSet->RootToContentDir("Common/ButtonHoverHint.png"), FMargin(4 / 16.0f), FLinearColor(1, 1, 1, 0.25f)))
			.SetPressed(FSlateBoxBrush(StyleSet->RootToContentDir("Common/ButtonHoverHint.png"), FMargin(4 / 16.0f), FLinearColor(1, 1, 1, 0.30f)))
			.SetNormalPadding(FMargin(0, 0, 0, 1))
			.SetPressedPadding(FMargin(0, 1, 0, 0));
		FComboButtonStyle ComboButton = FComboButtonStyle()
			.SetButtonStyle(Button.SetNormal(FSlateNoResource()))
			.SetDownArrowImage(FSlateImageBrush(StyleSet->RootToCoreContentDir(TEXT("Common/ComboArrow.png")), Icon8x8))
			.SetMenuBorderBrush(FSlateBoxBrush(StyleSet->RootToCoreContentDir(TEXT("Old/Menu_Background.png")), FMargin(8.0f / 64.0f)))
			.SetMenuBorderPadding(FMargin(0.0f));
		StyleSet->Set("ComboButton", ComboButton);

		FEditableTextBoxStyle EditableTextStyle = FEditableTextBoxStyle()
			.SetTextStyle(FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
			.SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 9))
			.SetBackgroundImageNormal(FSlateNoResource())
			.SetBackgroundImageHovered(FSlateNoResource())
			.SetBackgroundImageFocused(FSlateNoResource())
			.SetBackgroundImageReadOnly(FSlateNoResource())
			.SetBackgroundColor(FLinearColor::Transparent)
			.SetForegroundColor(FSlateColor::UseForeground());

		StyleSet->Set("EditableTextBox", EditableTextStyle);

		FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());

		RegisterSettings();
		RegisterCustomizations();
		RegisterAutoGeneratedDefaultEvents();
	}

	void ModulesChangesCallback(FName ModuleName, EModuleChangeReason ReasonForChange)
	{
		if (ReasonForChange == EModuleChangeReason::ModuleLoaded && ModuleName == LevelEditorModuleName)
		{
			RegisterTabSpawner();
		}
	}

	virtual void ShutdownModule() override
	{
		LLM_SCOPE_BYTAG(LiveLink_LiveLinkEditor);

		UnregisterCustomizations();

		UnregisterTabSpawner();

		FModuleManager::Get().OnModulesChanged().Remove(ModulesChangedHandle);

		if (LevelEditorTabManagerChangedHandle.IsValid() && FModuleManager::Get().IsModuleLoaded(LevelEditorModuleName))
		{
			FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(LevelEditorModuleName);
			LevelEditorModule.OnTabManagerChanged().Remove(LevelEditorTabManagerChangedHandle);
		}

		StyleSet.Reset();
	}

	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}

	static TSharedRef<SDockTab> SpawnLiveLinkTab(const FSpawnTabArgs& SpawnTabArgs, TSharedPtr<FSlateStyleSet> InStyleSet)
	{
		FLiveLinkClient* Client = &IModularFeatures::Get().GetModularFeature<FLiveLinkClient>(FLiveLinkClient::ModularFeatureName);

		const TSharedRef<SDockTab> MajorTab =
			SNew(SDockTab)
			.TabRole(ETabRole::NomadTab);

		MajorTab->SetContent(SNew(SLiveLinkClientPanel, Client));

		return MajorTab;
	}

private:

	void RegisterTabSpawner()
	{
		if (bHasRegisteredTabSpawners)
		{
			UnregisterTabSpawner();
		}

		FTabSpawnerEntry& SpawnerEntry = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(LiveLinkClientTabName, FOnSpawnTab::CreateStatic(&FLiveLinkEditorModule::SpawnLiveLinkTab, StyleSet))
			.SetDisplayName(LOCTEXT("LiveLinkTabTitle", "Live Link"))
			.SetTooltipText(LOCTEXT("LiveLinkTabTooltipText", "Open the Live Link streaming manager tab."))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorVirtualProductionCategory())
			.SetIcon(FSlateIcon(StyleSet->GetStyleSetName(), "LiveLinkClient.Common.Icon.Small"));

		bHasRegisteredTabSpawners = true;
	}

	void UnregisterTabSpawner()
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(LiveLinkClientTabName);
		bHasRegisteredTabSpawners = false;
	}

	void RegisterSettings()
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (SettingsModule != nullptr)
		{
			SettingsModule->RegisterSettings("Project", "Plugins", "LiveLink",
				LOCTEXT("LiveLinkSettingsName", "Live Link"),
				LOCTEXT("LiveLinkDescription", "Configure the Live Link plugin."),
				GetMutableDefault<ULiveLinkSettings>()
			);

			SettingsModule->RegisterSettings("Project", "Plugins", "LiveLinkComponent",
				LOCTEXT("LiveLinkComponentSettingsName", "Live Link Component"),
				LOCTEXT("LiveLinkComponentDescription", "Configure the Live Link Component."),
				GetMutableDefault<ULiveLinkComponentSettings>()
			);
		}
	}

	void UnregisterSettings()
	{
		// unregister settings
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Plugins", "LiveLinkComponent");
			SettingsModule->UnregisterSettings("Project", "Plugins", "LiveLink");
		}
	}

	void RegisterCustomizations()
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyEditorModule.RegisterCustomClassLayout(ULiveLinkVirtualSubject::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FLiveLinkVirtualSubjectDetailCustomization::MakeInstance));
		PropertyEditorModule.RegisterCustomPropertyTypeLayout(FLiveLinkSubjectKey::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FLiveLinkSubjectKeyDetailCustomization::MakeInstance));
		PropertyEditorModule.RegisterCustomPropertyTypeLayout(FLiveLinkSubjectRepresentation::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FLiveLinkSubjectRepresentationDetailCustomization::MakeInstance));
		PropertyEditorModule.RegisterCustomPropertyTypeLayout(FLiveLinkSubjectName::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FLiveLinkSubjectNameDetailCustomization::MakeInstance));
		PropertyEditorModule.RegisterCustomClassLayout(ULiveLinkSourceSettings::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FLiveLinkSourceSettingsDetailCustomization::MakeInstance));
		PropertyEditorModule.RegisterCustomClassLayout(ULiveLinkComponentController::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FLiveLinkComponentDetailCustomization::MakeInstance));
		PropertyEditorModule.RegisterCustomClassLayout(ULiveLinkControllerBase::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FLiveLinkControllerBaseDetailCustomization::MakeInstance));

		LiveLinkGraphPanelPinFactory = MakeShared<FLiveLinkGraphPanelPinFactory>();
		FEdGraphUtilities::RegisterVisualPinFactory(LiveLinkGraphPanelPinFactory);
	}

	void UnregisterCustomizations()
	{
		if (UObjectInitialized() && !IsEngineExitRequested())
		{
			FEdGraphUtilities::UnregisterVisualPinFactory(LiveLinkGraphPanelPinFactory);
			FPropertyEditorModule* PropertyEditorModule = FModuleManager::Get().GetModulePtr<FPropertyEditorModule>("PropertyEditor");
			if (PropertyEditorModule)
			{
				PropertyEditorModule->UnregisterCustomClassLayout(ULiveLinkComponentController::StaticClass()->GetFName());
				PropertyEditorModule->UnregisterCustomClassLayout(ULiveLinkControllerBase::StaticClass()->GetFName());
				PropertyEditorModule->UnregisterCustomClassLayout(ULiveLinkSourceSettings::StaticClass()->GetFName());
				PropertyEditorModule->UnregisterCustomPropertyTypeLayout(FLiveLinkSubjectName::StaticStruct()->GetFName());
				PropertyEditorModule->UnregisterCustomPropertyTypeLayout(FLiveLinkSubjectRepresentation::StaticStruct()->GetFName());
				PropertyEditorModule->UnregisterCustomPropertyTypeLayout(FLiveLinkSubjectKey::StaticStruct()->GetFName());
				PropertyEditorModule->UnregisterCustomClassLayout(ULiveLinkVirtualSubject::StaticClass()->GetFName());
			}
		}
	}

	// Set up default nodes for live link blueprint classes
	void RegisterAutoGeneratedDefaultEvents()
	{
		FKismetEditorUtilities::RegisterAutoGeneratedDefaultEvent(this, ULiveLinkBlueprintVirtualSubject::StaticClass(), "OnInitialize");
		FKismetEditorUtilities::RegisterAutoGeneratedDefaultEvent(this, ULiveLinkBlueprintVirtualSubject::StaticClass(), "OnUpdate");
	}

private:
	FDelegateHandle LevelEditorTabManagerChangedHandle;
	FDelegateHandle ModulesChangedHandle;

	TSharedPtr<FLiveLinkGraphPanelPinFactory> LiveLinkGraphPanelPinFactory;

	// Track if we have registered
	bool bHasRegisteredTabSpawners;
};

TSharedPtr<FSlateStyleSet> FLiveLinkEditorModule::StyleSet;

IMPLEMENT_MODULE(FLiveLinkEditorModule, LiveLinkEditor);


TSharedPtr< class ISlateStyle > FLiveLinkEditorPrivate::GetStyleSet()
{
	return FLiveLinkEditorModule::StyleSet;
}

#undef LOCTEXT_NAMESPACE
