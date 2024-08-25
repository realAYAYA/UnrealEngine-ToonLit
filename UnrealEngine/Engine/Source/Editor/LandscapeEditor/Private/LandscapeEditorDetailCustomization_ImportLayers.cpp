// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeEditorDetailCustomization_ImportLayers.h"
#include "LandscapeEditorDetailCustomization_ImportExport.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "DesktopPlatformModule.h"
#include "Editor.h"
#include "LandscapeEditorModule.h"
#include "LandscapeEditorObject.h"
#include "LandscapeUtils.h"
#include "LandscapeTiledImage.h"
#include "LandscapeEditorUtils.h"
#include "SLandscapeEditor.h"

#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"

#include "PropertyCustomizationHelpers.h"

#include "Dialogs/DlgPickAssetPath.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Notifications/SErrorText.h"
#include "Widgets/SToolTip.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "LandscapeEditor.ImportLayers"

TSharedRef<IPropertyTypeCustomization> FLandscapeEditorStructCustomization_FLandscapeImportLayer::MakeInstance()
{
	return MakeShareable(new FLandscapeEditorStructCustomization_FLandscapeImportLayer);
}

bool FLandscapeEditorStructCustomization_FLandscapeImportLayer::IsImporting()
{
	if (IsToolActive("ImportExport"))
	{
		FEdModeLandscape* LandscapeEdMode = GetEditorMode();
		check(LandscapeEdMode != nullptr);
		return LandscapeEdMode->ImportExportMode == EImportExportMode::Import;
	}

	return true;
}

void FLandscapeEditorStructCustomization_FLandscapeImportLayer::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}

void FLandscapeEditorStructCustomization_FLandscapeImportLayer::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TSharedRef<IPropertyHandle> PropertyHandle_LayerName = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLandscapeImportLayer, LayerName)).ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_LayerInfo = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLandscapeImportLayer, LayerInfo)).ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_SourceFilePath = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLandscapeImportLayer, SourceFilePath)).ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_ExportFilePath = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLandscapeImportLayer, ExportFilePath)).ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_ThumbnailMIC = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLandscapeImportLayer, ThumbnailMIC)).ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_ImportResult = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLandscapeImportLayer, ImportResult)).ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_ErrorMessage = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLandscapeImportLayer, ErrorMessage)).ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_Selected = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLandscapeImportLayer, bSelected)).ToSharedRef();
	
	PropertyHandle_SourceFilePath->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([PropertyHandle_SourceFilePath]()
	{
		FLandscapeEditorDetailCustomization_ImportExport::FormatFilename(PropertyHandle_SourceFilePath, /*bForExport = */false);
		FLandscapeEditorStructCustomization_FLandscapeImportLayer::OnImportWeightmapFilenameChanged();
	}));

	PropertyHandle_ExportFilePath->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([PropertyHandle_ExportFilePath]()
	{
		FLandscapeEditorDetailCustomization_ImportExport::FormatFilename(PropertyHandle_ExportFilePath, /*bForExport = */true);
	}));

	FName LayerName;
	FText LayerNameText;
	FPropertyAccess::Result Result = PropertyHandle_LayerName->GetValue(LayerName);
	checkSlow(Result == FPropertyAccess::Success);
	LayerNameText = FText::FromName(LayerName);
	if (Result == FPropertyAccess::MultipleValues)
	{
		LayerName = NAME_None;
		LayerNameText = NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values");
	}

	UObject* ThumbnailMIC = nullptr;
	Result = PropertyHandle_ThumbnailMIC->GetValue(ThumbnailMIC);
	checkSlow(Result == FPropertyAccess::Success);

	auto CreateFilenameWidget = [&](TSharedRef<IPropertyHandle> PropertyHandle_Filepath, bool bImport)
	{
		return SNew(SHorizontalBox)
			.Visibility_Static(&GetImportExportVisibility, bImport)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 2, 0)
			[
				SNew(SErrorText)
				.Visibility_Static(&GetErrorVisibility, PropertyHandle_ImportResult)
				.BackgroundColor_Static(&GetErrorColor, PropertyHandle_ImportResult)
				.ErrorText(NSLOCTEXT("UnrealEd", "Error", "!"))
				.ToolTip(
					SNew(SToolTip)
					.Text_Static(&GetErrorText, PropertyHandle_ErrorMessage)
				)
			]
			+ SHorizontalBox::Slot()
			[
				PropertyHandle_Filepath->CreatePropertyValueWidget()
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(1, 0, 0, 0)
			[
				SNew(SButton)
				.ContentPadding(FMargin(4, 0))
				.Text(NSLOCTEXT("UnrealEd", "GenericOpenDialog", "..."))
				.OnClicked_Static(&FLandscapeEditorStructCustomization_FLandscapeImportLayer::OnLayerFilenameButtonClicked, PropertyHandle_Filepath)
			];
	};

	ChildBuilder.AddCustomRow(LayerNameText)
	.NameContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		.Padding(FMargin(2))
		[
			SNew(SCheckBox)
			.Visibility_Static(GetImportLayerSelectionVisibility)
			.IsEnabled_Static(&FLandscapeEditorStructCustomization_FLandscapeImportLayer::IsValidLayerInfo, PropertyHandle_LayerInfo)
			.IsChecked_Static(&FLandscapeEditorStructCustomization_FLandscapeImportLayer::GetImportLayerSelectedCheckState, PropertyHandle_Selected, PropertyHandle_LayerInfo)
			.OnCheckStateChanged_Static(&FLandscapeEditorStructCustomization_FLandscapeImportLayer::OnImportLayerSelectedCheckStateChanged, PropertyHandle_Selected)
			.ToolTipText_Static(&FLandscapeEditorStructCustomization_FLandscapeImportLayer::GetImportLayerSelectedToolTip, PropertyHandle_Selected, PropertyHandle_LayerInfo)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(FMargin(2))
		[
			SNew(SLandscapeAssetThumbnail, ThumbnailMIC, StructCustomizationUtils.GetThumbnailPool().ToSharedRef())
			.ThumbnailSize(FIntPoint(48, 48))
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		.VAlign(VAlign_Center)
		.Padding(FMargin(2))
		[
			SNew(STextBlock)
			.Font(StructCustomizationUtils.GetRegularFont())
			.Text(LayerNameText)
		]
	]
	.ValueContent()
	.MinDesiredWidth(250.0f) // copied from SPropertyEditorAsset::GetDesiredWidth
	.MaxDesiredWidth(0)
	[
		SNew(SBox)
		.VAlign(VAlign_Center)
		.Padding(FMargin(0, 0, 12, 0)) // Line up with the other properties due to having no reset to default button
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				.Visibility_Static(&FLandscapeEditorStructCustomization_FLandscapeImportLayer::GetLayerInfoAssignVisibility)
				+ SHorizontalBox::Slot()
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(ULandscapeLayerInfoObject::StaticClass())
					.PropertyHandle(PropertyHandle_LayerInfo)
					.OnShouldFilterAsset_Static(&ShouldFilterLayerInfo, LayerName)
					.AllowCreate(false)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SComboButton)
					.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
					.HasDownArrow(false)
					.ContentPadding(4.0f)
					.ForegroundColor(FSlateColor::UseForeground())
					.IsFocusable(false)
					.ToolTipText(LOCTEXT("Target_Create", "Create Layer Info"))
					.Visibility_Static(&GetImportLayerCreateVisibility, PropertyHandle_LayerInfo)
					.OnGetMenuContent_Static(&OnGetImportLayerCreateMenu, PropertyHandle_LayerInfo, LayerName)
					.ButtonContent()
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("LandscapeEditor.Target_Create"))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SVerticalBox)
				.Visibility_Static(&FLandscapeEditorStructCustomization_FLandscapeImportLayer::GetImportLayerVisibility)
				.IsEnabled_Static(&FLandscapeEditorStructCustomization_FLandscapeImportLayer::IsImportLayerSelected, PropertyHandle_Selected, PropertyHandle_LayerInfo)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					CreateFilenameWidget(PropertyHandle_SourceFilePath, true)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					CreateFilenameWidget(PropertyHandle_ExportFilePath, false)
				]
			]
		]
	];
}

void FLandscapeEditorStructCustomization_FLandscapeImportLayer::OnImportWeightmapFilenameChanged()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode && LandscapeEdMode->UISettings)
	{
		LandscapeEdMode->UISettings->OnImportWeightmapFilenameChanged();
	}
}

FReply FLandscapeEditorStructCustomization_FLandscapeImportLayer::OnLayerFilenameButtonClicked(TSharedRef<IPropertyHandle> PropertyHandle_LayerFilename)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (!LandscapeEdMode)
	{
		return FReply::Handled();
	}

	ILandscapeEditorModule& LandscapeEditorModule = FModuleManager::GetModuleChecked<ILandscapeEditorModule>("LandscapeEditor");
	const bool bIsImporting = IsImporting();
	const FString DialogTypeString = bIsImporting  ? LandscapeEditorModule.GetWeightmapImportDialogTypeString() : LandscapeEditorModule.GetWeightmapExportDialogTypeString();
	const FString DialogTitle = bIsImporting ? LOCTEXT("ImportLayer", "Import Layer").ToString() : LOCTEXT("ExportLayer", "Export Layer").ToString();

	TOptional<FString> OptionalExportImportPath = LandscapeEditorUtils::GetImportExportFilename(DialogTitle, LandscapeEdMode->UISettings->LastImportPath, DialogTypeString, bIsImporting);
	if (OptionalExportImportPath.IsSet())
	{
		const FString& Filename = OptionalExportImportPath.GetValue();

		ensure(PropertyHandle_LayerFilename->SetValue(Filename) == FPropertyAccess::Success);
		LandscapeEdMode->UISettings->LastImportPath = FPaths::GetPath(Filename);
	}

	return FReply::Handled();
}

bool FLandscapeEditorStructCustomization_FLandscapeImportLayer::ShouldFilterLayerInfo(const FAssetData& AssetData, FName LayerName)
{
	const FName LayerNameMetaData = AssetData.GetTagValueRef<FName>("LayerName");
	if (!LayerNameMetaData.IsNone())
	{
		return LayerNameMetaData != LayerName;
	}

	ULandscapeLayerInfoObject* LayerInfo = CastChecked<ULandscapeLayerInfoObject>(AssetData.GetAsset());
	return LayerInfo->LayerName != LayerName;
}

EVisibility FLandscapeEditorStructCustomization_FLandscapeImportLayer::GetImportLayerSelectionVisibility()
{
	return IsToolActive("ImportExport") ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FLandscapeEditorStructCustomization_FLandscapeImportLayer::GetImportLayerCreateVisibility(TSharedRef<IPropertyHandle> PropertyHandle_LayerInfo)
{
	if (IsToolActive("ImportExport"))
	{
		return EVisibility::Collapsed;
	}

	
	return IsValidLayerInfo(PropertyHandle_LayerInfo) ? EVisibility::Collapsed : EVisibility::Visible;
	

	return EVisibility::Collapsed;
}

bool FLandscapeEditorStructCustomization_FLandscapeImportLayer::IsValidLayerInfo(TSharedRef<IPropertyHandle> PropertyHandle_LayerInfo)
{
	UObject* LayerInfoAsUObject = nullptr;
	return (PropertyHandle_LayerInfo->GetValue(LayerInfoAsUObject) != FPropertyAccess::Fail && LayerInfoAsUObject != nullptr);
}

EVisibility FLandscapeEditorStructCustomization_FLandscapeImportLayer::GetImportExportVisibility(bool bImport)
{
	return IsImporting() == bImport ? EVisibility::Visible : EVisibility::Collapsed;
}

TSharedRef<SWidget> FLandscapeEditorStructCustomization_FLandscapeImportLayer::OnGetImportLayerCreateMenu(TSharedRef<IPropertyHandle> PropertyHandle_LayerInfo, FName LayerName)
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.AddMenuEntry(LOCTEXT("Target_Create_Blended", "Weight-Blended Layer (normal)"), FText(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateStatic(&OnImportLayerCreateClicked, PropertyHandle_LayerInfo, LayerName, false)));

	MenuBuilder.AddMenuEntry(LOCTEXT("Target_Create_NoWeightBlend", "Non Weight-Blended Layer"), FText(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateStatic(&OnImportLayerCreateClicked, PropertyHandle_LayerInfo, LayerName, true)));

	return MenuBuilder.MakeWidget();
}

void FLandscapeEditorStructCustomization_FLandscapeImportLayer::OnImportLayerCreateClicked(TSharedRef<IPropertyHandle> PropertyHandle_LayerInfo, FName LayerName, bool bNoWeightBlend)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (!LandscapeEdMode)
	{
		return;
	}
	
	// Hack as we don't have a direct world pointer in the EdMode...
	ULevel* Level = LandscapeEdMode->CurrentGizmoActor->GetWorld()->GetCurrentLevel();

	// Build default layer object name and package name
	FName LayerObjectName;
	FString PackageName = UE::Landscape::GetLayerInfoObjectPackageName(Level, LayerName, LayerObjectName);

	TSharedRef<SDlgPickAssetPath> NewLayerDlg =
		SNew(SDlgPickAssetPath)
		.Title(LOCTEXT("CreateNewLayerInfo", "Create New Landscape Layer Info Object"))
		.DefaultAssetPath(FText::FromString(PackageName));

	if (NewLayerDlg->ShowModal() != EAppReturnType::Cancel)
	{
		PackageName = NewLayerDlg->GetFullAssetPath().ToString();
		LayerObjectName = FName(*NewLayerDlg->GetAssetName().ToString());

		UPackage* Package = CreatePackage(*PackageName);
		ULandscapeLayerInfoObject* LayerInfo = NewObject<ULandscapeLayerInfoObject>(Package, LayerObjectName, RF_Public | RF_Standalone | RF_Transactional);
		LayerInfo->LayerName = LayerName;
		LayerInfo->bNoWeightBlend = bNoWeightBlend;

		const UObject* LayerInfoAsUObject = LayerInfo; // HACK: If SetValue took a reference to a const ptr (T* const &) or a non-reference (T*) then this cast wouldn't be necessary
		ensure(PropertyHandle_LayerInfo->SetValue(LayerInfoAsUObject) == FPropertyAccess::Success);

		// Notify the asset registry
		FAssetRegistryModule::AssetCreated(LayerInfo);

		// Mark the package dirty...
		Package->MarkPackageDirty();

		// Show in the content browser
		TArray<UObject*> Objects;
		Objects.Add(LayerInfo);
		GEditor->SyncBrowserToObjects(Objects);
	}
	
}

EVisibility FLandscapeEditorStructCustomization_FLandscapeImportLayer::GetLayerInfoAssignVisibility()
{
	return IsToolActive("ImportExport") ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility FLandscapeEditorStructCustomization_FLandscapeImportLayer::GetImportLayerVisibility()
{
	if (IsToolActive("ImportExport"))
	{
		return EVisibility::Visible;
	}

	if (IsToolActive("NewLandscape"))
	{
		FEdModeLandscape* LandscapeEdMode = GetEditorMode();
		if (LandscapeEdMode != nullptr)
		{
			if (LandscapeEdMode->NewLandscapePreviewMode == ENewLandscapePreviewMode::ImportLandscape)
			{
				return EVisibility::Visible;
			}
		}
	}

	return EVisibility::Collapsed;
}

EVisibility FLandscapeEditorStructCustomization_FLandscapeImportLayer::GetErrorVisibility(TSharedRef<IPropertyHandle> PropertyHandle_ImportResult)
{
	ELandscapeImportResult WeightmapImportResult;
	FPropertyAccess::Result Result = PropertyHandle_ImportResult->GetValue((uint8&)WeightmapImportResult);

	if (Result == FPropertyAccess::Fail ||
		Result == FPropertyAccess::MultipleValues)
	{
		return EVisibility::Visible;
	}

	if (WeightmapImportResult != ELandscapeImportResult::Success)
	{
		return EVisibility::Visible;
	}
	return EVisibility::Collapsed;
}

FSlateColor FLandscapeEditorStructCustomization_FLandscapeImportLayer::GetErrorColor(TSharedRef<IPropertyHandle> PropertyHandle_ImportResult)
{
	ELandscapeImportResult WeightmapImportResult;
	FPropertyAccess::Result Result = PropertyHandle_ImportResult->GetValue((uint8&)WeightmapImportResult);

	if (Result == FPropertyAccess::Fail ||
		Result == FPropertyAccess::MultipleValues)
	{
		return FCoreStyle::Get().GetColor("ErrorReporting.BackgroundColor");
	}

	switch (WeightmapImportResult)
	{
	case ELandscapeImportResult::Success:
		return FCoreStyle::Get().GetColor("InfoReporting.BackgroundColor");
	case ELandscapeImportResult::Warning:
		return FCoreStyle::Get().GetColor("ErrorReporting.WarningBackgroundColor");
	case ELandscapeImportResult::Error:
		return FCoreStyle::Get().GetColor("ErrorReporting.BackgroundColor");
	default:
		check(0);
		return FSlateColor();
	}
}

FText FLandscapeEditorStructCustomization_FLandscapeImportLayer::GetErrorText(TSharedRef<IPropertyHandle> PropertyHandle_ErrorMessage)
{
	FText ErrorMessage;
	FPropertyAccess::Result Result = PropertyHandle_ErrorMessage->GetValue(ErrorMessage);
	if (Result == FPropertyAccess::Fail)
	{
		return LOCTEXT("Import_LayerUnknownError", "Unknown Error");
	}
	else if (Result == FPropertyAccess::MultipleValues)
	{
		return NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values");
	}

	return ErrorMessage;
}

bool FLandscapeEditorStructCustomization_FLandscapeImportLayer::IsImportLayerSelected(TSharedRef<IPropertyHandle> PropertyHandle_Selected, TSharedRef<IPropertyHandle> PropertyHandle_LayerInfo)
{
	if (IsToolActive("ImportExport"))
	{
		// Need a valid layer info to import/export
		if (!IsValidLayerInfo(PropertyHandle_LayerInfo))
		{
			return false;
		}

		bool bSelected;
		FPropertyAccess::Result Result = PropertyHandle_Selected->GetValue(bSelected);
		return Result == FPropertyAccess::Success ? bSelected : false;
	}

	return true;
}

ECheckBoxState FLandscapeEditorStructCustomization_FLandscapeImportLayer::GetImportLayerSelectedCheckState(TSharedRef<IPropertyHandle> PropertyHandle_Selected, TSharedRef<IPropertyHandle> PropertyHandle_LayerInfo)
{
	return IsImportLayerSelected(PropertyHandle_Selected, PropertyHandle_LayerInfo) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

FText FLandscapeEditorStructCustomization_FLandscapeImportLayer::GetImportLayerSelectedToolTip(TSharedRef<IPropertyHandle> PropertyHandle_Selected, TSharedRef<IPropertyHandle> PropertyHandle_LayerInfo)
{
	if (!IsValidLayerInfo(PropertyHandle_LayerInfo))
	{
		return LOCTEXT("InvalidLayerInfo", "This layer doesn't have a valid LayerInfo object assigned.");
	}

	return FText();
}

void FLandscapeEditorStructCustomization_FLandscapeImportLayer::OnImportLayerSelectedCheckStateChanged(ECheckBoxState CheckState, TSharedRef<IPropertyHandle> PropertyHandle_Selected)
{
	ensure(PropertyHandle_Selected->SetValue(CheckState == ECheckBoxState::Checked, EPropertyValueSetFlags::NotTransactable) == FPropertyAccess::Success);
}

#undef LOCTEXT_NAMESPACE
