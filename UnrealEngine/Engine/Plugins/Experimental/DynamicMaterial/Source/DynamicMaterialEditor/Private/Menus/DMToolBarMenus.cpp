// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMToolBarMenus.h"
#include "ContentBrowserModule.h"
#include "DMBlueprintFunctionLibrary.h"
#include "DMPrivate.h"
#include "DynamicMaterialEditorModule.h"
#include "DynamicMaterialEditorSettings.h"
#include "EngineAnalytics.h"
#include "IContentBrowserSingleton.h"
#include "ISinglePropertyView.h"
#include "Material/DynamicMaterialInstance.h"
#include "Materials/Material.h"
#include "Menus/DMMenuContext.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "PropertyEditorModule.h"
#include "Slate/SDMEditor.h"
#include "Slate/SDMSlot.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "Widgets/SWidget.h"

#define LOCTEXT_NAMESPACE "FDMToolBarMenus"

namespace UE::DynamicMaterialEditor::Private
{
	static FName ToolBarEditorLayoutMenuName = TEXT("MaterialDesigner.EditorLayout");
	static FName ToolBarPreviewOptionsSectionName = TEXT("PreviewOptions");
	static FName ToolBarTooltipOptionsSectionName = TEXT("TooltipOptions");
	static FName ToolBarExportSectionName = TEXT("Export");

	void OpenMaterialEditorFromContext(UDMMenuContext* InMenuContext)
	{
		if (IsValid(InMenuContext))
		{
			if (UDynamicMaterialModelEditorOnlyData* const ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(InMenuContext->GetModel()))
			{
				if (FEngineAnalytics::IsAvailable())
				{
					FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.MaterialDesigner.OpenedGeneratedMaterial"));
				}

				ModelEditorOnlyData->OpenMaterialEditor();
			}
		}
	}

	void ExportMaterialInstanceFromInstance(TWeakObjectPtr<UDynamicMaterialInstance> InMaterialInstanceWeak)
	{
		if (UDynamicMaterialInstance* MaterialInstance = InMaterialInstanceWeak.Get())
		{
			FSaveAssetDialogConfig SaveAssetDialogConfig;
			SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("SaveAssetDialogTitle", "Save Asset As");
			SaveAssetDialogConfig.DefaultPath = "/Game";
			SaveAssetDialogConfig.DefaultAssetName = MaterialInstance->GetName();
			SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;

			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);

			UDMBlueprintFunctionLibrary::ExportMaterialInstance(MaterialInstance->GetMaterialModel(), SaveObjectPath);

			if (FEngineAnalytics::IsAvailable())
			{
				FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.MaterialDesigner.ExportedMaterialInstance"));
			}
		}
	}

	void ExportMaterialModelFromModel(TWeakObjectPtr<UDynamicMaterialModel> InMaterialModelWeak)
	{
		if (UDynamicMaterialModel* MaterialModel = InMaterialModelWeak.Get())
		{
			UMaterial* GeneratedMaterial = MaterialModel->GetGeneratedMaterial();

			if (!GeneratedMaterial)
			{
				UE::DynamicMaterialEditor::Private::LogError(TEXT("Failed to find a generated material to export."));
				return;
			}

			FSaveAssetDialogConfig SaveAssetDialogConfig;
			SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("SaveAssetDialogTitle", "Save Asset As");
			SaveAssetDialogConfig.DefaultPath = "/Game";
			SaveAssetDialogConfig.DefaultAssetName = GeneratedMaterial->GetName();
			SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;

			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			const FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);

			if (SaveObjectPath.Len() == 0)
			{
				return;
			}

			UDMBlueprintFunctionLibrary::ExportGeneratedMaterial(MaterialModel, SaveObjectPath);

			if (FEngineAnalytics::IsAvailable())
			{
				FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.MaterialDesigner.ExportedGeneratedMaterial"));
			}
		}
	}

	void AddToolBarBoolOptionMenuEntry(FToolMenuSection& InSection, const FName& InPropertyName, const FUIAction InAction)
	{
		const FProperty* const OptionProperty = UDynamicMaterialEditorSettings::StaticClass()->FindPropertyByName(InPropertyName);

		if (ensure(OptionProperty))
		{
			InSection.AddMenuEntry(NAME_None,
				OptionProperty->GetDisplayNameText(),
				OptionProperty->GetToolTipText(),
				FSlateIcon(),
				InAction, EUserInterfaceActionType::ToggleButton);
		}
	}

	void AddToolBarIntOptionMenuEntry(FToolMenuSection& InSection, const FName& InPropertyName, 
		TAttribute<bool> InIsEnabledAttribute = TAttribute<bool>(),
		TAttribute<EVisibility> InVisibilityAttribute = TAttribute<EVisibility>())
	{
		InSection.AddDynamicEntry(NAME_None,
			FNewToolMenuSectionDelegate::CreateLambda(
				[InPropertyName, InIsEnabledAttribute, InVisibilityAttribute](FToolMenuSection& InSection)
				{
					const FProperty* const OptionProperty = UDynamicMaterialEditorSettings::StaticClass()->FindPropertyByName(InPropertyName);
					FText DisplayName = FText::GetEmpty();
					FText Tooltip = FText::GetEmpty();

					if (ensure(OptionProperty))
					{
						DisplayName = OptionProperty->GetDisplayNameText();
						Tooltip = OptionProperty->GetToolTipText();
					}

					FPropertyEditorModule& PropertyEditor = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

					FSinglePropertyParams SinglePropertyParams;
					SinglePropertyParams.NamePlacement = EPropertyNamePlacement::Hidden;

					TSharedRef<ISinglePropertyView> SinglePropertyView = PropertyEditor.CreateSingleProperty(UDynamicMaterialEditorSettings::Get(), InPropertyName, SinglePropertyParams).ToSharedRef();
					SinglePropertyView->SetToolTipText(Tooltip);
					SinglePropertyView->SetEnabled(InIsEnabledAttribute);
					SinglePropertyView->SetVisibility(InVisibilityAttribute);

					InSection.AddEntry(FToolMenuEntry::InitWidget(NAME_None,
						SNew(SBox)
						.HAlign(HAlign_Fill)
						[
							SNew(SBox)
								.WidthOverride(80.0f)
								.HAlign(HAlign_Right)
								[
									SinglePropertyView
								]
						],
						DisplayName));
				})
		);
	}

	void AddToolbarExportMenu(UToolMenu* InMenu)
	{
		if (!IsValid(InMenu) || InMenu->ContainsSection(ToolBarExportSectionName))
		{
			return;
		}

		const UDMMenuContext* const MenuContext = InMenu->FindContext<UDMMenuContext>();

		if (!MenuContext)
		{
			return;
		}

		UDynamicMaterialModel* const MaterialModel = MenuContext->GetModel();

		if (!MaterialModel)
		{
			return;
		}

		UDynamicMaterialInstance* Instance = MaterialModel->GetDynamicMaterialInstance();

		if (!Instance)
		{
			return;
		}

		const bool bAllowInstanceExport = IsValid(Instance->GetOuter()) && !Instance->GetOuter()->IsA<UPackage>();
		const bool bAllowMaterialExport = IsValid(MaterialModel->GetGeneratedMaterial());

		if (!bAllowInstanceExport && !bAllowMaterialExport)
		{
			return;
		}

		FToolMenuSection& NewSection = InMenu->AddSection("Export", LOCTEXT("ExportSection", "Export Material"));

		if (bAllowInstanceExport)
		{
			NewSection.AddMenuEntry(NAME_None,
				LOCTEXT("ExportMaterialInstance", "Export Material Designer Instance"),
				LOCTEXT("ExportMaterialInstanceTooltip", "Export the material instance to an asset."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateStatic(
					&UE::DynamicMaterialEditor::Private::ExportMaterialInstanceFromInstance,
					TWeakObjectPtr<UDynamicMaterialInstance>(Instance)
				))
			);
		}

		if (bAllowMaterialExport)
		{
			NewSection.AddMenuEntry(NAME_None,
				LOCTEXT("ExportGeneratedMaterial", "Export Generated Material"),
				LOCTEXT("ExportGeneratedMaterialTooltip", "Export the generated material to an asset."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateStatic(
					&UE::DynamicMaterialEditor::Private::ExportMaterialModelFromModel,
					TWeakObjectPtr<UDynamicMaterialModel>(MaterialModel))
				)
			);
		}
	}

	void AddToolBarTooltipOptionsSection(UToolMenu* InMenu)
	{
		if (!IsValid(InMenu) || InMenu->ContainsSection(ToolBarTooltipOptionsSectionName))
		{
			return;
		}

		FToolMenuSection& NewSection = InMenu->AddSection(ToolBarTooltipOptionsSectionName, LOCTEXT("TooltipOptionsSection", "Tooltip Options"));

		AddToolBarBoolOptionMenuEntry(NewSection,
			GET_MEMBER_NAME_CHECKED(UDynamicMaterialEditorSettings, bShowTooltipPreview),
			FUIAction(
				FExecuteAction::CreateLambda(
					[]()
					{
						UDynamicMaterialEditorSettings* Settings = UDynamicMaterialEditorSettings::Get();
						Settings->bShowTooltipPreview = !Settings->bShowTooltipPreview;
						Settings->SaveConfig();
					}),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda(
					[]()
					{
						return UDynamicMaterialEditorSettings::Get()->bShowTooltipPreview;
					})
			)
		);

		TAttribute<bool> AreTooltipPreviewsEnabledAttribute = TAttribute<bool>::CreateLambda(
			[]()
			{
				return UDynamicMaterialEditorSettings::Get()->bShowTooltipPreview;
			});

		AddToolBarIntOptionMenuEntry(
			NewSection,
			GET_MEMBER_NAME_CHECKED(UDynamicMaterialEditorSettings, TooltipTextureSize),
			AreTooltipPreviewsEnabledAttribute
		);
	}

	void AddToolBarPreviewOptionsSection(UToolMenu* InMenu)
	{
		if (!IsValid(InMenu) || InMenu->ContainsSection(ToolBarPreviewOptionsSectionName))
		{
			return;
		}

		FToolMenuSection& NewSection = InMenu->AddSection(ToolBarPreviewOptionsSectionName, LOCTEXT("PreviewOptionsSection", "Preview Options"));

		AddToolBarIntOptionMenuEntry(NewSection,
			GET_MEMBER_NAME_CHECKED(UDynamicMaterialEditorSettings, LayerPreviewSize)
		);

		NewSection.AddSeparator(NAME_None);

		AddToolBarIntOptionMenuEntry(NewSection,
			GET_MEMBER_NAME_CHECKED(UDynamicMaterialEditorSettings, DetailsPreviewSize)
		);

		NewSection.AddSeparator(NAME_None);
	}

	void AddToolBarAdvancedSection(UToolMenu* InMenu)
	{
		FToolMenuSection& NewSection = InMenu->AddSection("AdvancedSettings", LOCTEXT("AdvancedSettingsSection", "Advanced Settings"));

		InMenu->AddDynamicSection(NAME_None, FNewToolMenuDelegate::CreateStatic(&AddToolBarPreviewOptionsSection));

		InMenu->AddDynamicSection(NAME_None, FNewToolMenuDelegate::CreateStatic(&AddToolBarTooltipOptionsSection));

		NewSection.AddSeparator(NAME_None);

		NewSection.AddMenuEntry(NAME_None,
			LOCTEXT("ResetAllSettingsToDefaults", "Reset All To Defaults"),
			LOCTEXT("ResetAllSettingsToDefaultsTooltip", "Resets all the Material Designer settings to their default values."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateUObject(
				UDynamicMaterialEditorSettings::Get(), 
				&UDynamicMaterialEditorSettings::ResetAllLayoutSettings)
			)
		);
	}

	void AddToolBarEditorLayoutMenu(UToolMenu* InMenu)
	{
		AddToolbarExportMenu(InMenu);

		FToolMenuSection& NewSection = InMenu->AddSection("MaterialDesigner", LOCTEXT("MaterialDesignerSection", "MaterialDesigner"));

		UDMMenuContext* MenuContext = InMenu->FindContext<UDMMenuContext>();

		NewSection.AddMenuEntry(NAME_None,
			LOCTEXT("OpenInUEMaterialEditor", "Open in UE Material Editor..."),
			LOCTEXT("OpenInUEMaterialEditorTooltip", "Opens the currently editing generated Material Designer Instance material in the Unreal Engine material editor."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(
				&OpenMaterialEditorFromContext,
				MenuContext
			))
		);

		NewSection.AddSubMenu(
			"AdvancedSettings",
			LOCTEXT("AdvancedSettingsSubMenu", "Advanced Settings"),
			LOCTEXT("AdvancedSettingsSubMenu_ToolTip", "Display advanced Material Designer settings"),
			FNewToolMenuDelegate::CreateStatic(&AddToolBarAdvancedSection)
		);

		NewSection.AddMenuEntry(NAME_None,
			LOCTEXT("OpenSettings", "Material Designer Settings..."),
			LOCTEXT("OpenSettingsTooltip", "Opens the Material Designer settings window."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "FoliageEditMode.Settings"),
			FUIAction(FExecuteAction::CreateUObject(
				UDynamicMaterialEditorSettings::Get(),
				&UDynamicMaterialEditorSettings::OpenEditorSettingsWindow)
			)
		);
	}
}

TSharedRef<SWidget> FDMToolBarMenus::MakeEditorLayoutMenu(const TSharedPtr<SDMEditor>& InEditorWidget)
{
	using namespace UE::DynamicMaterialEditor::Private;

	if (!UToolMenus::Get()->IsMenuRegistered(ToolBarEditorLayoutMenuName))
	{
		UToolMenu* const NewToolMenu = UDMMenuContext::GenerateContextMenuDefault(ToolBarEditorLayoutMenuName);

		if (!NewToolMenu)
		{
			return SNullWidget::NullWidget;
		}

		FToolMenuSection& NewSection = NewToolMenu->AddDynamicSection(
			"MaterialDesigner", 
			FNewToolMenuDelegate::CreateStatic(&AddToolBarEditorLayoutMenu)
		);
	}

	TSharedPtr<SDMSlot> SlotWidget;
	if (InEditorWidget.IsValid())
	{
		TArray<TSharedPtr<SDMSlot>> SlotWidgets = InEditorWidget->GetSlotWidgets();

		if (SlotWidgets.IsEmpty() == false)
		{
			SlotWidget = SlotWidgets[0];
		}
	}

	FToolMenuContext MenuContext(FDynamicMaterialEditorModule::Get().GetCommandList(), TSharedPtr<FExtender>(), UDMMenuContext::CreateSlot(SlotWidget));

	return UToolMenus::Get()->GenerateWidget(ToolBarEditorLayoutMenuName, MenuContext);
}

#undef LOCTEXT_NAMESPACE
