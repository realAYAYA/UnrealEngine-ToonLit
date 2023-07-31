// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeEditorDetailCustomization_NewLandscape.h"
#include "LandscapeEditorDetailCustomization_ImportExport.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "SlateOptMacros.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Notifications/SErrorText.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "SWarningOrErrorBox.h"
#include "LandscapeEditorModule.h"
#include "LandscapeEditorObject.h"
#include "Landscape.h"
#include "LandscapeConfigHelper.h"
#include "LandscapeImportHelper.h"
#include "LandscapeSettings.h"

#include "DetailLayoutBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"
#include "PropertyCustomizationHelpers.h"

#include "SLandscapeEditor.h"
#include "Dialogs/DlgPickAssetPath.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Input/SRotatorInputBox.h"
#include "ScopedTransaction.h"
#include "DesktopPlatformModule.h"
#include "AssetRegistry/AssetRegistryModule.h"

#include "TutorialMetaData.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "LandscapeDataAccess.h"
#include "Settings/EditorExperimentalSettings.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "LandscapeSubsystem.h"
#include "SPrimaryButton.h"
#include "Widgets/Input/SSegmentedControl.h"

#define LOCTEXT_NAMESPACE "LandscapeEditor.NewLandscape"


TSharedRef<IDetailCustomization> FLandscapeEditorDetailCustomization_NewLandscape::MakeInstance()
{
	return MakeShareable(new FLandscapeEditorDetailCustomization_NewLandscape);
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FLandscapeEditorDetailCustomization_NewLandscape::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	if (!IsToolActive("NewLandscape"))
	{
		return;
	}

	IDetailCategoryBuilder& NewLandscapeCategory = DetailBuilder.EditCategory("New Landscape");

	NewLandscapeCategory.AddCustomRow(FText::GetEmpty())
	[
		SNew(SBox)
		.Padding(2.0f)
		.HAlign(HAlign_Center)
		[
			SNew(SSegmentedControl<ENewLandscapePreviewMode>)
			.Value_Lambda([this]()
			{
				FEdModeLandscape* LandscapeEdMode = GetEditorMode();
				return LandscapeEdMode ? LandscapeEdMode->NewLandscapePreviewMode : ENewLandscapePreviewMode::NewLandscape;

			})
			.OnValueChanged_Lambda([this](ENewLandscapePreviewMode Mode)
			{
				FEdModeLandscape* LandscapeEdMode = GetEditorMode();
				if (LandscapeEdMode != nullptr)
				{
					LandscapeEdMode->NewLandscapePreviewMode = Mode;
				}
			})
			+SSegmentedControl<ENewLandscapePreviewMode>::Slot(ENewLandscapePreviewMode::NewLandscape)
			.Text(LOCTEXT("NewLandscape", "Create New"))
			+ SSegmentedControl<ENewLandscapePreviewMode>::Slot(ENewLandscapePreviewMode::ImportLandscape)
			.Text(LOCTEXT("ImportLandscape", "Import from File"))
		]
		
	];

	TSharedRef<IPropertyHandle> PropertyHandle_CanHaveLayersContent = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, bCanHaveLayersContent));
	NewLandscapeCategory.AddProperty(PropertyHandle_CanHaveLayersContent).Visibility(MakeAttributeLambda([]()
	{
		const ULandscapeSettings* Settings = GetDefault<ULandscapeSettings>();
		return Settings->InRestrictiveMode() ? EVisibility::Hidden : EVisibility::Visible;
	}));

	TSharedRef<IPropertyHandle> PropertyHandle_FlipYAxis = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, bFlipYAxis));
	NewLandscapeCategory.AddProperty(PropertyHandle_FlipYAxis).Visibility(MakeAttributeLambda([]()
	{ 
		FEdModeLandscape* LandscapeEdMode = GetEditorMode();
		if (LandscapeEdMode != nullptr)
		{
			if ((LandscapeEdMode->NewLandscapePreviewMode == ENewLandscapePreviewMode::ImportLandscape) && !LandscapeEdMode->UseSingleFileImport())
			{
				return EVisibility::Visible;
			}
		}

		return EVisibility::Collapsed; 
	}));
	
	PropertyHandle_FlipYAxis->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([this]() { OnImportHeightmapFilenameChanged(); }));

	TSharedRef<IPropertyHandle> PropertyHandle_GridSize = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, WorldPartitionGridSize));
	
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode->GetWorld()->GetSubsystem<ULandscapeSubsystem>()->IsGridBased())
	{
		NewLandscapeCategory.AddProperty(PropertyHandle_GridSize);
	}
	else
	{
		DetailBuilder.HideProperty(PropertyHandle_GridSize);
	}
	TSharedRef<IPropertyHandle> PropertyHandle_HeightmapFilename = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, ImportLandscape_HeightmapFilename));
	TSharedRef<IPropertyHandle> PropertyHandle_HeightmapImportResult = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, ImportLandscape_HeightmapImportResult));
	TSharedRef<IPropertyHandle> PropertyHandle_HeightmapErrorMessage = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, ImportLandscape_HeightmapErrorMessage));
	DetailBuilder.HideProperty(PropertyHandle_HeightmapImportResult);
	DetailBuilder.HideProperty(PropertyHandle_HeightmapErrorMessage);
	PropertyHandle_HeightmapFilename->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([this, PropertyHandle_HeightmapFilename]()
	{
		FLandscapeEditorDetailCustomization_ImportExport::FormatFilename(PropertyHandle_HeightmapFilename, /*bForExport = */false);
		OnImportHeightmapFilenameChanged();
	}));
	NewLandscapeCategory.AddProperty(PropertyHandle_HeightmapFilename)
	.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateStatic(&GetVisibilityOnlyInNewLandscapeMode, ENewLandscapePreviewMode::ImportLandscape)))
	.CustomWidget()
	.NameContent()
	[
		PropertyHandle_HeightmapFilename->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(250.0f)
	.MaxDesiredWidth(0)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0,0,2,0)
		[
			SNew(SErrorText)
			.Visibility_Static(&GetHeightmapErrorVisibility, PropertyHandle_HeightmapImportResult)
			.BackgroundColor_Static(&GetHeightmapErrorColor, PropertyHandle_HeightmapImportResult)
			.ErrorText(NSLOCTEXT("UnrealEd", "Error", "!"))
			.ToolTip(
				SNew(SToolTip)
				.Text_Static(&GetPropertyValue<FText>, PropertyHandle_HeightmapErrorMessage)
			)
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		[
			SNew(SEditableTextBox)
			.Font(DetailBuilder.GetDetailFont())
			.Text_Static(&GetPropertyValueText, PropertyHandle_HeightmapFilename)
			.OnTextCommitted_Static(&SetImportHeightmapFilenameString, PropertyHandle_HeightmapFilename)
			.HintText(LOCTEXT("Import_HeightmapNotSet", "(Please specify a heightmap)"))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(1,0,0,0)
		[
			SNew(SButton)
			//.Font(DetailBuilder.GetDetailFont())
			.ContentPadding(FMargin(4, 0))
			.Text(NSLOCTEXT("UnrealEd", "GenericOpenDialog", "..."))
			.OnClicked_Static(&OnImportHeightmapFilenameButtonClicked, PropertyHandle_HeightmapFilename)
		]
	];

	NewLandscapeCategory.AddCustomRow(LOCTEXT("HeightmapResolution", "Heightmap Resolution"))
	.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateStatic(&GetVisibilityOnlyInNewLandscapeMode, ENewLandscapePreviewMode::ImportLandscape)))
	.NameContent()
	[
		SNew(SBox)
		.VAlign(VAlign_Center)
		.Padding(FMargin(2))
		[
			SNew(STextBlock)
			.Font(DetailBuilder.GetDetailFont())
			.Text(LOCTEXT("HeightmapResolution", "Heightmap Resolution"))
		]
	]
	.ValueContent()
	[
		SNew(SBox)
		.Padding(FMargin(0,0,12,0)) // Line up with the other properties due to having no reset to default button
		[
			SNew(SComboButton)
			.OnGetMenuContent(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetImportLandscapeResolutionMenu)
			.ContentPadding(2)
			.ButtonContent()
			[
				SNew(STextBlock)
				.Font(DetailBuilder.GetDetailFont())
				.Text(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetImportLandscapeResolution)
			]
		]
	];

	TSharedRef<IPropertyHandle> PropertyHandle_Material = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, NewLandscape_Material));
	NewLandscapeCategory.AddProperty(PropertyHandle_Material);

	NewLandscapeCategory.AddCustomRow(LOCTEXT("LayersLabel", "Layers"))
	.Visibility(TAttribute<EVisibility>(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetMaterialTipVisibility))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(15, 12, 0, 12)
		[
			SNew(STextBlock)
			.Font(DetailBuilder.GetDetailFont())
			.Text(LOCTEXT("Material_Tip","Hint: Assign a material to see landscape layers"))
		]
	];

	TSharedRef<IPropertyHandle> PropertyHandle_AlphamapType = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, ImportLandscape_AlphamapType));
	NewLandscapeCategory.AddProperty(PropertyHandle_AlphamapType)
	.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateStatic(&GetVisibilityOnlyInNewLandscapeMode, ENewLandscapePreviewMode::ImportLandscape)));

	TSharedRef<IPropertyHandle> PropertyHandle_Layers = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, ImportLandscape_Layers));

	TSharedRef<FDetailArrayBuilder> ArrayBuilder = MakeShared<FDetailArrayBuilder>(PropertyHandle_Layers, /*InGenerateHeader*/ true, /*InDisplayResetToDefault*/ false, /*InDisplayElementNum*/ false);
	ArrayBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FLandscapeEditorDetailCustomization_NewLandscape::GenerateLayersArrayElementWidget));
	NewLandscapeCategory.AddCustomBuilder(ArrayBuilder, /*bForAdvanced*/ false);

	TSharedRef<IPropertyHandle> PropertyHandle_Location = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, NewLandscape_Location));
	TSharedRef<IPropertyHandle> PropertyHandle_Location_X = PropertyHandle_Location->GetChildHandle("X").ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_Location_Y = PropertyHandle_Location->GetChildHandle("Y").ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_Location_Z = PropertyHandle_Location->GetChildHandle("Z").ToSharedRef();
	NewLandscapeCategory.AddProperty(PropertyHandle_Location)
	.CustomWidget()
	.NameContent()
	[
		PropertyHandle_Location->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(125.0f * 3.0f) // copied from FComponentTransformDetails
	.MaxDesiredWidth(125.0f * 3.0f)
	[
		SNew(SNumericVectorInputBox<FVector::FReal>)
		.bColorAxisLabels(true)
		.Font(DetailBuilder.GetDetailFont())
		.X_Static(&GetOptionalPropertyValue<FVector::FReal>, PropertyHandle_Location_X)
		.Y_Static(&GetOptionalPropertyValue<FVector::FReal>, PropertyHandle_Location_Y)
		.Z_Static(&GetOptionalPropertyValue<FVector::FReal>, PropertyHandle_Location_Z)
		.OnXCommitted_Static(&SetPropertyValue<FVector::FReal>, PropertyHandle_Location_X)
		.OnYCommitted_Static(&SetPropertyValue<FVector::FReal>, PropertyHandle_Location_Y)
		.OnZCommitted_Static(&SetPropertyValue<FVector::FReal>, PropertyHandle_Location_Z)
		.OnXChanged_Lambda([=](FVector::FReal NewValue) { ensure(PropertyHandle_Location_X->SetValue(NewValue, EPropertyValueSetFlags::InteractiveChange) == FPropertyAccess::Success); })
		.OnYChanged_Lambda([=](FVector::FReal NewValue) { ensure(PropertyHandle_Location_Y->SetValue(NewValue, EPropertyValueSetFlags::InteractiveChange) == FPropertyAccess::Success); })
		.OnZChanged_Lambda([=](FVector::FReal NewValue) { ensure(PropertyHandle_Location_Z->SetValue(NewValue, EPropertyValueSetFlags::InteractiveChange) == FPropertyAccess::Success); })
		.AllowSpin(true)
	];

	TSharedRef<IPropertyHandle> PropertyHandle_Rotation = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, NewLandscape_Rotation));
	TSharedRef<IPropertyHandle> PropertyHandle_Rotation_Roll  = PropertyHandle_Rotation->GetChildHandle("Roll").ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_Rotation_Pitch = PropertyHandle_Rotation->GetChildHandle("Pitch").ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_Rotation_Yaw   = PropertyHandle_Rotation->GetChildHandle("Yaw").ToSharedRef();
	NewLandscapeCategory.AddProperty(PropertyHandle_Rotation)
	.CustomWidget()
	.NameContent()
	[
		PropertyHandle_Rotation->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(125.0f * 3.0f) // copied from FComponentTransformDetails
	.MaxDesiredWidth(125.0f * 3.0f)
	[
		SNew(SNumericRotatorInputBox<FRotator::FReal>)
		.bColorAxisLabels(true)
		.Font(DetailBuilder.GetDetailFont())
		.Roll_Static(&GetOptionalPropertyValue<FRotator::FReal>, PropertyHandle_Rotation_Roll)
		.Pitch_Static(&GetOptionalPropertyValue<FRotator::FReal>, PropertyHandle_Rotation_Pitch)
		.Yaw_Static(&GetOptionalPropertyValue<FRotator::FReal>, PropertyHandle_Rotation_Yaw)
		.OnYawCommitted_Static(&SetPropertyValue<FRotator::FReal>, PropertyHandle_Rotation_Yaw) // not allowed to roll or pitch landscape
		.OnYawChanged_Lambda([=](FRotator::FReal NewValue){ ensure(PropertyHandle_Rotation_Yaw->SetValue(NewValue, EPropertyValueSetFlags::InteractiveChange) == FPropertyAccess::Success); })
		.AllowSpin(true)
	];

	TSharedRef<IPropertyHandle> PropertyHandle_Scale = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, NewLandscape_Scale));
	TSharedRef<IPropertyHandle> PropertyHandle_Scale_X = PropertyHandle_Scale->GetChildHandle("X").ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_Scale_Y = PropertyHandle_Scale->GetChildHandle("Y").ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_Scale_Z = PropertyHandle_Scale->GetChildHandle("Z").ToSharedRef();
	NewLandscapeCategory.AddProperty(PropertyHandle_Scale)
	.CustomWidget()
	.NameContent()
	[
		PropertyHandle_Scale->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(125.0f * 3.0f) // copied from FComponentTransformDetails
	.MaxDesiredWidth(125.0f * 3.0f)
	[
		SNew(SNumericVectorInputBox<FVector::FReal>)
		.bColorAxisLabels(true)
		.Font(DetailBuilder.GetDetailFont())
		.X_Static(&GetOptionalPropertyValue<FVector::FReal>, PropertyHandle_Scale_X)
		.Y_Static(&GetOptionalPropertyValue<FVector::FReal>, PropertyHandle_Scale_Y)
		.Z_Static(&GetOptionalPropertyValue<FVector::FReal>, PropertyHandle_Scale_Z)
		.OnXCommitted_Static(&SetScale, PropertyHandle_Scale_X)
		.OnYCommitted_Static(&SetScale, PropertyHandle_Scale_Y)
		.OnZCommitted_Static(&SetScale, PropertyHandle_Scale_Z)
		.OnXChanged_Lambda([=](FVector::FReal NewValue) { ensure(PropertyHandle_Scale_X->SetValue(NewValue, EPropertyValueSetFlags::InteractiveChange) == FPropertyAccess::Success); })
		.OnYChanged_Lambda([=](FVector::FReal NewValue) { ensure(PropertyHandle_Scale_Y->SetValue(NewValue, EPropertyValueSetFlags::InteractiveChange) == FPropertyAccess::Success); })
		.OnZChanged_Lambda([=](FVector::FReal NewValue) { ensure(PropertyHandle_Scale_Z->SetValue(NewValue, EPropertyValueSetFlags::InteractiveChange) == FPropertyAccess::Success); })
		.AllowSpin(true)
	];

	TSharedRef<IPropertyHandle> PropertyHandle_QuadsPerSection = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, NewLandscape_QuadsPerSection));
	NewLandscapeCategory.AddProperty(PropertyHandle_QuadsPerSection)
	.CustomWidget()
	.NameContent()
	[
		PropertyHandle_QuadsPerSection->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SComboButton)
		.OnGetMenuContent_Static(&GetSectionSizeMenu, PropertyHandle_QuadsPerSection)
		.ContentPadding(2)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Font(DetailBuilder.GetDetailFont())
			.Text_Static(&GetSectionSize, PropertyHandle_QuadsPerSection)
		]
	];

	TSharedRef<IPropertyHandle> PropertyHandle_SectionsPerComponent = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, NewLandscape_SectionsPerComponent));
	NewLandscapeCategory.AddProperty(PropertyHandle_SectionsPerComponent)
	.CustomWidget()
	.NameContent()
	[
		PropertyHandle_SectionsPerComponent->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SComboButton)
		.OnGetMenuContent_Static(&GetSectionsPerComponentMenu, PropertyHandle_SectionsPerComponent)
		.ContentPadding(2)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Font(DetailBuilder.GetDetailFont())
			.Text_Static(&GetSectionsPerComponent, PropertyHandle_SectionsPerComponent)
		]
	];

	TSharedRef<IPropertyHandle> PropertyHandle_ComponentCount = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, NewLandscape_ComponentCount));
	TSharedRef<IPropertyHandle> PropertyHandle_ComponentCount_X = PropertyHandle_ComponentCount->GetChildHandle("X").ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_ComponentCount_Y = PropertyHandle_ComponentCount->GetChildHandle("Y").ToSharedRef();
	NewLandscapeCategory.AddProperty(PropertyHandle_ComponentCount)
	.CustomWidget()
	.NameContent()
	[
		PropertyHandle_ComponentCount->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		[
			SNew(SNumericEntryBox<int32>)
			.Font(DetailBuilder.GetDetailFont())
			.MinValue(1)
			.MaxValue(32)
			.MinSliderValue(1)
			.MaxSliderValue(32)
			.AllowSpin(true)
			.UndeterminedString(NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values"))
			.Value_Static(&FLandscapeEditorDetailCustomization_Base::OnGetValue<int32>, PropertyHandle_ComponentCount_X)
			.OnValueChanged_Static(&FLandscapeEditorDetailCustomization_Base::OnValueChanged<int32>, PropertyHandle_ComponentCount_X)
			.OnValueCommitted_Static(&FLandscapeEditorDetailCustomization_Base::OnValueCommitted<int32>, PropertyHandle_ComponentCount_X)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2, 0)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Font(DetailBuilder.GetDetailFont())
			.Text(FText::FromString(FString().AppendChar(0xD7))) // Multiply sign
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		[
			SNew(SNumericEntryBox<int32>)
			.Font(DetailBuilder.GetDetailFont())
			.MinValue(1)
			.MaxValue(32)
			.MinSliderValue(1)
			.MaxSliderValue(32)
			.AllowSpin(true)
			.UndeterminedString(NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values"))
			.Value_Static(&FLandscapeEditorDetailCustomization_Base::OnGetValue<int32>, PropertyHandle_ComponentCount_Y)
			.OnValueChanged_Static(&FLandscapeEditorDetailCustomization_Base::OnValueChanged<int32>, PropertyHandle_ComponentCount_Y)
			.OnValueCommitted_Static(&FLandscapeEditorDetailCustomization_Base::OnValueCommitted<int32>, PropertyHandle_ComponentCount_Y)
		]
	];

	NewLandscapeCategory.AddCustomRow(LOCTEXT("Resolution", "Overall Resolution"))
	.RowTag("LandscapeEditor.OverallResolution")
	.NameContent()
	[
		SNew(SBox)
		.VAlign(VAlign_Center)
		.Padding(FMargin(2))
		[
			SNew(STextBlock)
			.Font(DetailBuilder.GetDetailFont())
			.Text(LOCTEXT("Resolution", "Overall Resolution"))
			.ToolTipText(TAttribute<FText>(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetOverallResolutionTooltip))
		]
	]
	.ValueContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		[
			SNew(SNumericEntryBox<int32>)
			.Font(DetailBuilder.GetDetailFont())
			.MinValue(1)
			.MaxValue(8192)
			.MinSliderValue(1)
			.MaxSliderValue(8192)
			.AllowSpin(true)
			//.MinSliderValue(TAttribute<TOptional<int32> >(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetMinLandscapeResolution))
			//.MaxSliderValue(TAttribute<TOptional<int32> >(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetMaxLandscapeResolution))
			.Value(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetLandscapeResolutionX)
			.OnValueChanged_Lambda([=](int32 NewValue)
			{
				OnChangeLandscapeResolutionX(NewValue, false);
			})
			.OnValueCommitted_Lambda([=](int32 NewValue, ETextCommit::Type)
			{
				OnChangeLandscapeResolutionX(NewValue, true);
			})
			.OnBeginSliderMovement_Lambda([=]()
			{
				bUsingSlider = true;
				GEditor->BeginTransaction(LOCTEXT("ChangeResolutionX_Transaction", "Change Landscape Resolution X"));
			})
			.OnEndSliderMovement_Lambda([=](double)
			{
				GEditor->EndTransaction();
				bUsingSlider = false;
			})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2, 0)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Font(DetailBuilder.GetDetailFont())
			.Text(FText::FromString(FString().AppendChar(0xD7))) // Multiply sign
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		.Padding(0,0,12,0) // Line up with the other properties due to having no reset to default button
		[
			SNew(SNumericEntryBox<int32>)
			.Font(DetailBuilder.GetDetailFont())
			.MinValue(1)
			.MaxValue(8192)
			.MinSliderValue(1)
			.MaxSliderValue(8192)
			.AllowSpin(true)
			//.MinSliderValue(TAttribute<TOptional<int32> >(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetMinLandscapeResolution))
			//.MaxSliderValue(TAttribute<TOptional<int32> >(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetMaxLandscapeResolution))
			.Value(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetLandscapeResolutionY)
			.OnValueChanged_Lambda([=](int32 NewValue)
			{
				OnChangeLandscapeResolutionY(NewValue, false);
			})
			.OnValueCommitted_Lambda([=](int32 NewValue, ETextCommit::Type)
			{
				OnChangeLandscapeResolutionY(NewValue, true);
			})
			.OnBeginSliderMovement_Lambda([=]()
			{
				bUsingSlider = true;
				GEditor->BeginTransaction(LOCTEXT("ChangeResolutionY_Transaction", "Change Landscape Resolution Y"));
			})
			.OnEndSliderMovement_Lambda([=](double)
			{
				GEditor->EndTransaction();
				bUsingSlider = false;
			})
		]
	];

	NewLandscapeCategory.AddCustomRow(LOCTEXT("TotalComponents", "Total Components"))
	.RowTag("LandscapeEditor.TotalComponents")
	.NameContent()
	[
		SNew(SBox)
		.VAlign(VAlign_Center)
		.Padding(FMargin(2))
		[
			SNew(STextBlock)
			.Font(DetailBuilder.GetDetailFont())
			.Text(LOCTEXT("TotalComponents", "Total Components"))
			.ToolTipText(LOCTEXT("NewLandscape_TotalComponents", "The total number of components that will be created for this landscape."))
		]
	]
	.ValueContent()
	.VAlign(VAlign_Center)
	[
		SNew(SEditableTextBox)
		.IsReadOnly(true)
		.Font(DetailBuilder.GetDetailFont())
		.Text(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetTotalComponentCount)
	];

	NewLandscapeCategory.AddCustomRow(FText::GetEmpty()).WholeRowContent()
	.VAlign(VAlign_Center)
	.HAlign(HAlign_Right)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(4,0)
		.AutoWidth()
		[
			SNew(SButton)
			.Visibility_Static(&GetVisibilityOnlyInNewLandscapeMode, ENewLandscapePreviewMode::NewLandscape)
			.Text(LOCTEXT("FillWorld", "Fill World"))
			.AddMetaData<FTutorialMetaData>(FTutorialMetaData(TEXT("FillWorldButton"), TEXT("LevelEditorToolBox")))
			.OnClicked(this, &FLandscapeEditorDetailCustomization_NewLandscape::OnFillWorldButtonClicked)
		]
		+ SHorizontalBox::Slot()
		.Padding(4, 0)
		.AutoWidth()
		[
			SNew(SButton)
			.Visibility_Static(&GetVisibilityOnlyInNewLandscapeMode, ENewLandscapePreviewMode::ImportLandscape)
			.Text(LOCTEXT("FitToData", "Fit To Data"))
			.AddMetaData<FTagMetaData>(TEXT("ImportButton"))
			.OnClicked(this, &FLandscapeEditorDetailCustomization_NewLandscape::OnFitImportDataButtonClicked)
			.IsEnabled(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetImportButtonIsEnabled)
		]
		+ SHorizontalBox::Slot()
		.Padding(4, 0)
		.AutoWidth()
		[
			SNew(SPrimaryButton)
			.Visibility_Static(&GetVisibilityOnlyInNewLandscapeMode, ENewLandscapePreviewMode::NewLandscape)
			.Text(LOCTEXT("Create", "Create"))
			.AddMetaData<FTutorialMetaData>(FTutorialMetaData(TEXT("CreateButton"), TEXT("LevelEditorToolBox")))
			.OnClicked(this, &FLandscapeEditorDetailCustomization_NewLandscape::OnCreateButtonClicked)
			.IsEnabled(this, &FLandscapeEditorDetailCustomization_NewLandscape::IsCreateButtonEnabled)
		]
		+ SHorizontalBox::Slot()
		.Padding(4, 0)
		.AutoWidth()
		[
			SNew(SButton)
			.Visibility_Static(&GetVisibilityOnlyInNewLandscapeMode, ENewLandscapePreviewMode::ImportLandscape)
			.Text(LOCTEXT("Import", "Import"))
			.OnClicked(this, &FLandscapeEditorDetailCustomization_NewLandscape::OnCreateButtonClicked)
			.IsEnabled(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetImportButtonIsEnabled)
		]
	];

	NewLandscapeCategory.AddCustomRow(FText::GetEmpty())
	.WholeRowContent()
	[
		SNew(SWarningOrErrorBox)
		.Message(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetNewLandscapeErrorText)
	]
	.Visibility(TAttribute<EVisibility>(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetNewLandscapeErrorVisibility));
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FText FLandscapeEditorDetailCustomization_NewLandscape::GetOverallResolutionTooltip() const
{
	return (GetEditorMode() && GetEditorMode()->NewLandscapePreviewMode == ENewLandscapePreviewMode::ImportLandscape)
	? LOCTEXT("ImportLandscape_OverallResolution", "Overall final resolution of the imported landscape in vertices")
	: LOCTEXT("NewLandscape_OverallResolution", "Overall final resolution of the new landscape in vertices");
}

void FLandscapeEditorDetailCustomization_NewLandscape::SetScale(FVector::FReal NewValue, ETextCommit::Type, TSharedRef<IPropertyHandle> PropertyHandle)
{
	FVector::FReal OldValue = 0;
	PropertyHandle->GetValue(OldValue);

	if (NewValue == 0)
	{
		if (OldValue < 0)
		{
			NewValue = -1;
		}
		else
		{
			NewValue = 1;
		}
	}

	ensure(PropertyHandle->SetValue(NewValue) == FPropertyAccess::Success);

	// Make X and Y scale match
	FName PropertyName = PropertyHandle->GetProperty()->GetFName();
	if (PropertyName == "X")
	{
		TSharedRef<IPropertyHandle> PropertyHandle_Y = PropertyHandle->GetParentHandle()->GetChildHandle("Y").ToSharedRef();
		ensure(PropertyHandle_Y->SetValue(NewValue) == FPropertyAccess::Success);
	}
	else if (PropertyName == "Y")
	{
		TSharedRef<IPropertyHandle> PropertyHandle_X = PropertyHandle->GetParentHandle()->GetChildHandle("X").ToSharedRef();
		ensure(PropertyHandle_X->SetValue(NewValue) == FPropertyAccess::Success);
	}
}

TSharedRef<SWidget> FLandscapeEditorDetailCustomization_NewLandscape::GetSectionSizeMenu(TSharedRef<IPropertyHandle> PropertyHandle)
{
	FMenuBuilder MenuBuilder(true, nullptr);

	for (int32 i = 0; i < UE_ARRAY_COUNT(FLandscapeConfig::SubsectionSizeQuadsValues); i++)
	{
		MenuBuilder.AddMenuEntry(FText::Format(LOCTEXT("NxNQuads", "{0}\u00D7{0} Quads"), FText::AsNumber(FLandscapeConfig::SubsectionSizeQuadsValues[i])), FText::GetEmpty(),
			FSlateIcon(), FExecuteAction::CreateStatic(&OnChangeSectionSize, PropertyHandle, FLandscapeConfig::SubsectionSizeQuadsValues[i]));
	}

	return MenuBuilder.MakeWidget();
}

void FLandscapeEditorDetailCustomization_NewLandscape::OnChangeSectionSize(TSharedRef<IPropertyHandle> PropertyHandle, int32 NewSize)
{
	ensure(PropertyHandle->SetValue(NewSize) == FPropertyAccess::Success);
}

FText FLandscapeEditorDetailCustomization_NewLandscape::GetSectionSize(TSharedRef<IPropertyHandle> PropertyHandle)
{
	int32 QuadsPerSection = 0;
	FPropertyAccess::Result Result = PropertyHandle->GetValue(QuadsPerSection);
	check(Result == FPropertyAccess::Success);

	if (Result == FPropertyAccess::MultipleValues)
	{
		return NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values");
	}

	return FText::Format(LOCTEXT("NxNQuads", "{0}\u00D7{0} Quads"), FText::AsNumber(QuadsPerSection));
}

TSharedRef<SWidget> FLandscapeEditorDetailCustomization_NewLandscape::GetSectionsPerComponentMenu(TSharedRef<IPropertyHandle> PropertyHandle)
{
	FMenuBuilder MenuBuilder(true, nullptr);

	for (int32 i = 0; i < UE_ARRAY_COUNT(FLandscapeConfig::NumSectionValues); i++)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Width"), FLandscapeConfig::NumSectionValues[i]);
		Args.Add(TEXT("Height"), FLandscapeConfig::NumSectionValues[i]);
		MenuBuilder.AddMenuEntry(FText::Format(FLandscapeConfig::NumSectionValues[i] == 1 ? LOCTEXT("1x1Section", "{Width}\u00D7{Height} Section") : LOCTEXT("NxNSections", "{Width}\u00D7{Height} Sections"), Args),
			FText::GetEmpty(), FSlateIcon(), FExecuteAction::CreateStatic(&OnChangeSectionsPerComponent, PropertyHandle, FLandscapeConfig::NumSectionValues[i]));
	}

	return MenuBuilder.MakeWidget();
}

void FLandscapeEditorDetailCustomization_NewLandscape::OnChangeSectionsPerComponent(TSharedRef<IPropertyHandle> PropertyHandle, int32 NewSize)
{
	ensure(PropertyHandle->SetValue(NewSize) == FPropertyAccess::Success);
}

FText FLandscapeEditorDetailCustomization_NewLandscape::GetSectionsPerComponent(TSharedRef<IPropertyHandle> PropertyHandle)
{
	int32 SectionsPerComponent = 0;
	FPropertyAccess::Result Result = PropertyHandle->GetValue(SectionsPerComponent);
	check(Result == FPropertyAccess::Success);

	if (Result == FPropertyAccess::MultipleValues)
	{
		return NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values");
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("Width"), SectionsPerComponent);
	Args.Add(TEXT("Height"), SectionsPerComponent);
	return FText::Format(SectionsPerComponent == 1 ? LOCTEXT("1x1Section", "{Width}\u00D7{Height} Section") : LOCTEXT("NxNSections", "{Width}\u00D7{Height} Sections"), Args);
}

TOptional<int32> FLandscapeEditorDetailCustomization_NewLandscape::GetLandscapeResolutionX() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		return LandscapeEdMode->GetNewLandscapeResolutionX();
	}

	return 0;
}

void FLandscapeEditorDetailCustomization_NewLandscape::OnChangeLandscapeResolutionX(int32 NewValue, bool bCommit)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		int32 NewComponentCountX = LandscapeEdMode->UISettings->CalcComponentsCount(NewValue);
		if (NewComponentCountX == LandscapeEdMode->UISettings->NewLandscape_ComponentCount.X)
		{
return;
		}

		FScopedTransaction Transaction(LOCTEXT("ChangeResolutionX_Transaction", "Change Landscape Resolution X"), !bUsingSlider && bCommit);

		LandscapeEdMode->UISettings->Modify();
		LandscapeEdMode->UISettings->NewLandscape_ComponentCount.X = NewComponentCountX;
	}
}

TOptional<int32> FLandscapeEditorDetailCustomization_NewLandscape::GetLandscapeResolutionY() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		return LandscapeEdMode->GetNewLandscapeResolutionY();
	}

	return 0;
}

void FLandscapeEditorDetailCustomization_NewLandscape::OnChangeLandscapeResolutionY(int32 NewValue, bool bCommit)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		int32 NewComponentCountY = LandscapeEdMode->UISettings->CalcComponentsCount(NewValue);
		if (NewComponentCountY == LandscapeEdMode->UISettings->NewLandscape_ComponentCount.Y)
		{
			return;
		}

		FScopedTransaction Transaction(LOCTEXT("ChangeResolutionY_Transaction", "Change Landscape Resolution Y"), !bUsingSlider && bCommit);

		LandscapeEdMode->UISettings->Modify();
		LandscapeEdMode->UISettings->NewLandscape_ComponentCount.Y = NewComponentCountY;
	}
}

TOptional<int32> FLandscapeEditorDetailCustomization_NewLandscape::GetMinLandscapeResolution() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		// Min size is one component
		return (LandscapeEdMode->UISettings->NewLandscape_SectionsPerComponent * LandscapeEdMode->UISettings->NewLandscape_QuadsPerSection + 1);
	}

	return 0;
}

TOptional<int32> FLandscapeEditorDetailCustomization_NewLandscape::GetMaxLandscapeResolution() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		// Max size is either whole components below 8192 verts, or 32 components
		const int32 QuadsPerComponent = (LandscapeEdMode->UISettings->NewLandscape_SectionsPerComponent * LandscapeEdMode->UISettings->NewLandscape_QuadsPerSection);
		//return (float)(FMath::Min(32, FMath::FloorToInt(8191 / QuadsPerComponent)) * QuadsPerComponent);
		return (8191 / QuadsPerComponent) * QuadsPerComponent + 1;
	}

	return 0;
}

FText FLandscapeEditorDetailCustomization_NewLandscape::GetTotalComponentCount() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		return FText::AsNumber(LandscapeEdMode->UISettings->NewLandscape_ComponentCount.X * LandscapeEdMode->UISettings->NewLandscape_ComponentCount.Y);
	}

	return FText::FromString(TEXT("---"));
}


EVisibility FLandscapeEditorDetailCustomization_NewLandscape::GetVisibilityOnlyInNewLandscapeMode(ENewLandscapePreviewMode value)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		if (LandscapeEdMode->NewLandscapePreviewMode == value)
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

void FLandscapeEditorDetailCustomization_NewLandscape::GenerateLayersArrayElementWidget(TSharedRef<IPropertyHandle> InPropertyHandle, int32 InArrayIndex, IDetailChildrenBuilder& InChildrenBuilder)
{
	InChildrenBuilder.AddProperty(InPropertyHandle)
	.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetLayerVisibility, InPropertyHandle)));
}

EVisibility FLandscapeEditorDetailCustomization_NewLandscape::GetLayerVisibility(TSharedRef<IPropertyHandle> InPropertyHandle) const
{
	const FEdModeLandscape* EdMode = GetEditorMode();
	TArray<void*> RawData;
	InPropertyHandle->AccessRawData(RawData);

	if ((EdMode != nullptr) && !RawData.IsEmpty() && (RawData[0] != nullptr))
	{
		FLandscapeImportLayer* ImportLayer = reinterpret_cast<FLandscapeImportLayer*>(RawData[0]);

		if ((EdMode->NewLandscapePreviewMode != ENewLandscapePreviewMode::ImportLandscape) && (ImportLayer->LayerName == ALandscapeProxy::VisibilityLayer->LayerName))
		{
			return EVisibility::Hidden;
		}
	}

	return EVisibility::Visible;
}

bool FLandscapeEditorDetailCustomization_NewLandscape::IsCreateButtonEnabled() const
{
	const FEdModeLandscape* EdMode = GetEditorMode();
	
	if (EdMode != nullptr)
	{
		return EdMode->IsLandscapeResolutionCompliant();
	}

	return true;
}

EVisibility FLandscapeEditorDetailCustomization_NewLandscape::GetNewLandscapeErrorVisibility() const
{
	return IsCreateButtonEnabled() ? EVisibility::Hidden : EVisibility::Visible;
}

FText FLandscapeEditorDetailCustomization_NewLandscape::GetNewLandscapeErrorText() const
{
	const FEdModeLandscape* EdMode = GetEditorMode();

	if (EdMode != nullptr)
	{
		return EdMode->GetLandscapeResolutionErrorText();
	}

	return FText::GetEmpty();
}

FReply FLandscapeEditorDetailCustomization_NewLandscape::OnCreateButtonClicked()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr && 
		LandscapeEdMode->GetWorld() != nullptr && 
		LandscapeEdMode->GetWorld()->GetCurrentLevel()->bIsVisible)
	{
		ULandscapeEditorObject* UISettings = LandscapeEdMode->UISettings;
		const int32 ComponentCountX = UISettings->NewLandscape_ComponentCount.X;
		const int32 ComponentCountY = UISettings->NewLandscape_ComponentCount.Y;
		const int32 QuadsPerComponent = UISettings->NewLandscape_SectionsPerComponent * UISettings->NewLandscape_QuadsPerSection;
		const int32 SizeX = ComponentCountX * QuadsPerComponent + 1;
		const int32 SizeY = ComponentCountY * QuadsPerComponent + 1;

		TArray<FLandscapeImportLayerInfo> MaterialImportLayers;
		ELandscapeImportResult LayerImportResult = LandscapeEdMode->NewLandscapePreviewMode == ENewLandscapePreviewMode::NewLandscape ? UISettings->CreateNewLayersInfo(MaterialImportLayers) : UISettings->CreateImportLayersInfo(MaterialImportLayers);

		if (LayerImportResult == ELandscapeImportResult::Error)
		{
			return FReply::Handled();
		}

		TMap<FGuid, TArray<uint16>> HeightDataPerLayers;
		TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerDataPerLayers;

		TArray<uint16> OutHeightData;
		if (LandscapeEdMode->NewLandscapePreviewMode == ENewLandscapePreviewMode::NewLandscape)
		{
			UISettings->InitializeDefaultHeightData(OutHeightData);
		}
		else
		{
			UISettings->ExpandImportData(OutHeightData, MaterialImportLayers);
		}

		HeightDataPerLayers.Add(FGuid(), OutHeightData);
		// ComputeHeightData will also modify/expand material layers data, which is why we create MaterialLayerDataPerLayers after calling ComputeHeightData
		MaterialLayerDataPerLayers.Add(FGuid(), MoveTemp(MaterialImportLayers));

		FScopedTransaction Transaction(LOCTEXT("Undo", "Creating New Landscape"));

		const FVector Offset = FTransform(UISettings->NewLandscape_Rotation, FVector::ZeroVector, UISettings->NewLandscape_Scale).TransformVector(FVector(-ComponentCountX * QuadsPerComponent / 2, -ComponentCountY * QuadsPerComponent / 2, 0));
		
		ALandscape* Landscape = LandscapeEdMode->GetWorld()->SpawnActor<ALandscape>(UISettings->NewLandscape_Location + Offset, UISettings->NewLandscape_Rotation);
		Landscape->bCanHaveLayersContent = UISettings->bCanHaveLayersContent;
		Landscape->LandscapeMaterial = UISettings->NewLandscape_Material.Get();
		Landscape->SetActorRelativeScale3D(UISettings->NewLandscape_Scale);

		// automatically calculate a lighting LOD that won't crash lightmass (hopefully)
		// < 2048x2048 -> LOD0
		// >=2048x2048 -> LOD1
		// >= 4096x4096 -> LOD2
		// >= 8192x8192 -> LOD3
		Landscape->StaticLightingLOD = FMath::DivideAndRoundUp(FMath::CeilLogTwo((SizeX * SizeY) / (2048 * 2048) + 1), (uint32)2);

		FString ReimportHeightmapFilePath;
		if (LandscapeEdMode->NewLandscapePreviewMode == ENewLandscapePreviewMode::ImportLandscape)
		{
			ReimportHeightmapFilePath = UISettings->ImportLandscape_HeightmapFilename;
		}

		Landscape->Import(FGuid::NewGuid(), 0, 0, SizeX - 1, SizeY - 1, UISettings->NewLandscape_SectionsPerComponent, UISettings->NewLandscape_QuadsPerSection, HeightDataPerLayers, *ReimportHeightmapFilePath, MaterialLayerDataPerLayers, UISettings->ImportLandscape_AlphamapType);

		ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
		check(LandscapeInfo);

		FActorLabelUtilities::SetActorLabelUnique(Landscape, ALandscape::StaticClass()->GetName());

		LandscapeInfo->UpdateLayerInfoMap(Landscape);

		// Import doesn't fill in the LayerInfo for layers with no data, do that now
		const TArray<FLandscapeImportLayer>& ImportLandscapeLayersList = UISettings->ImportLandscape_Layers;
		const ULandscapeSettings* Settings = GetDefault<ULandscapeSettings>();
		TSoftObjectPtr<ULandscapeLayerInfoObject> DefaultLayerInfoObject = Settings->GetDefaultLayerInfoObject().LoadSynchronous();

		for (int32 i = 0; i < ImportLandscapeLayersList.Num(); i++)
		{
			ULandscapeLayerInfoObject* LayerInfo = ImportLandscapeLayersList[i].LayerInfo;
			FName LayerName = ImportLandscapeLayersList[i].LayerName;

			// If DefaultLayerInfoObject is set and LayerInfo does not exist, we will try to create the new LayerInfo by cloning DefaultLayerInfoObject. Except for VisibilityLayer which doesn't require an asset.
			if (DefaultLayerInfoObject.IsValid() && (LayerInfo == nullptr) && (LayerName != ALandscapeProxy::VisibilityLayer->LayerName))
			{
				LayerInfo = Landscape->CreateLayerInfo(*LayerName.ToString(), DefaultLayerInfoObject.Get());

				if (LayerInfo != nullptr)
				{
					LayerInfo->LayerUsageDebugColor = LayerInfo->GenerateLayerUsageDebugColor();
					LayerInfo->MarkPackageDirty();
				}
			}

			if (LayerInfo != nullptr)
			{
				if (LandscapeEdMode->NewLandscapePreviewMode == ENewLandscapePreviewMode::ImportLandscape)
				{
					Landscape->EditorLayerSettings.Add(FLandscapeEditorLayerSettings(LayerInfo, ImportLandscapeLayersList[i].SourceFilePath));
				}
				else
				{
					Landscape->EditorLayerSettings.Add(FLandscapeEditorLayerSettings(LayerInfo));
				}

				int32 LayerInfoIndex = LandscapeInfo->GetLayerInfoIndex(ImportLandscapeLayersList[i].LayerName);
				if (ensure(LayerInfoIndex != INDEX_NONE))
				{
					FLandscapeInfoLayerSettings& LayerSettings = LandscapeInfo->Layers[LayerInfoIndex];
					LayerSettings.LayerInfoObj = LayerInfo;
				}
			}
		}

		LandscapeEdMode->UpdateLandscapeList();
		LandscapeEdMode->SetLandscapeInfo(LandscapeInfo);
		LandscapeEdMode->CurrentToolTarget.TargetType = ELandscapeToolTargetType::Heightmap;
		LandscapeEdMode->SetCurrentTargetLayer(NAME_None, nullptr);
		LandscapeEdMode->SetCurrentTool("Select"); // change tool so switching back to the manage mode doesn't give "New Landscape" again
		LandscapeEdMode->SetCurrentTool("Sculpt"); // change to sculpting mode and tool
		LandscapeEdMode->SetCurrentLayer(0);

		LandscapeEdMode->GetWorld()->GetSubsystem<ULandscapeSubsystem>()->ChangeGridSize(LandscapeInfo, UISettings->WorldPartitionGridSize);

		if (LandscapeEdMode->CurrentToolTarget.LandscapeInfo.IsValid())
		{
			ALandscapeProxy* LandscapeProxy = LandscapeEdMode->CurrentToolTarget.LandscapeInfo->GetLandscapeProxy();
			LandscapeProxy->OnMaterialChangedDelegate().AddRaw(LandscapeEdMode, &FEdModeLandscape::OnLandscapeMaterialChangedDelegate);
		}
	}

	return FReply::Handled();
}

FReply FLandscapeEditorDetailCustomization_NewLandscape::OnFillWorldButtonClicked()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		FScopedTransaction Transaction(LOCTEXT("FillWorld_Transaction", "Landscape Fill World"));

		LandscapeEdMode->UISettings->Modify();

		LandscapeEdMode->UISettings->NewLandscape_Location = FVector::ZeroVector;
		const int32 QuadsPerComponent = LandscapeEdMode->UISettings->NewLandscape_SectionsPerComponent * LandscapeEdMode->UISettings->NewLandscape_QuadsPerSection;
		LandscapeEdMode->UISettings->NewLandscape_ComponentCount.X = FMath::CeilToInt(WORLD_MAX / QuadsPerComponent / LandscapeEdMode->UISettings->NewLandscape_Scale.X);
		LandscapeEdMode->UISettings->NewLandscape_ComponentCount.Y = FMath::CeilToInt(WORLD_MAX / QuadsPerComponent / LandscapeEdMode->UISettings->NewLandscape_Scale.Y);

		const ULandscapeSettings* Settings = GetDefault<ULandscapeSettings>();
		if (Settings->IsLandscapeResolutionRestricted())
		{
			auto ClampComponentCount = [Settings, &QuadsPerComponent](int32& ComponentCount)
			{
				const float MaxResolution = Settings->GetSideResolutionLimit();
				ComponentCount = FMath::Clamp(ComponentCount, 1, FMath::Min(32, FMath::FloorToInt((MaxResolution - 1) / QuadsPerComponent)));
			};

			ClampComponentCount(LandscapeEdMode->UISettings->NewLandscape_ComponentCount.X);
			ClampComponentCount(LandscapeEdMode->UISettings->NewLandscape_ComponentCount.Y);
		}
		else
		{
			LandscapeEdMode->UISettings->NewLandscape_ClampSize();
		}
	}

	return FReply::Handled();
}

FReply FLandscapeEditorDetailCustomization_NewLandscape::OnFitImportDataButtonClicked()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		LandscapeEdMode->UISettings->ChooseBestComponentSizeForImport();
	}

	return FReply::Handled();
}

bool FLandscapeEditorDetailCustomization_NewLandscape::GetImportButtonIsEnabled() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		if (LandscapeEdMode->UISettings->ImportLandscape_HeightmapImportResult == ELandscapeImportResult::Error ||
			LandscapeEdMode->UISettings->ImportLandscape_HeightmapFilename.IsEmpty() ||
			LandscapeEdMode->UISettings->GetImportLandscapeData().IsEmpty())
		{
			return false;
		}

		for (int32 i = 0; i < LandscapeEdMode->UISettings->ImportLandscape_Layers.Num(); ++i)
		{
			if (LandscapeEdMode->UISettings->ImportLandscape_Layers[i].ImportResult == ELandscapeImportResult::Error)
			{
				return false;
			}
		}

		return IsCreateButtonEnabled();
	}
	
	return false;
}

EVisibility FLandscapeEditorDetailCustomization_NewLandscape::GetHeightmapErrorVisibility(TSharedRef<IPropertyHandle> PropertyHandle_HeightmapImportResult)
{
	ELandscapeImportResult HeightmapImportResult;
	FPropertyAccess::Result Result = PropertyHandle_HeightmapImportResult->GetValue((uint8&)HeightmapImportResult);

	if (Result == FPropertyAccess::Fail)
	{
		return EVisibility::Collapsed;
	}

	if (Result == FPropertyAccess::MultipleValues)
	{
		return EVisibility::Visible;
	}

	if (HeightmapImportResult != ELandscapeImportResult::Success)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

FSlateColor FLandscapeEditorDetailCustomization_NewLandscape::GetHeightmapErrorColor(TSharedRef<IPropertyHandle> PropertyHandle_HeightmapImportResult)
{
	ELandscapeImportResult HeightmapImportResult;
	FPropertyAccess::Result Result = PropertyHandle_HeightmapImportResult->GetValue((uint8&)HeightmapImportResult);

	if (Result == FPropertyAccess::Fail ||
		Result == FPropertyAccess::MultipleValues)
	{
		return FCoreStyle::Get().GetColor("ErrorReporting.BackgroundColor");
	}

	switch (HeightmapImportResult)
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

void FLandscapeEditorDetailCustomization_NewLandscape::SetImportHeightmapFilenameString(const FText& NewValue, ETextCommit::Type CommitInfo, TSharedRef<IPropertyHandle> PropertyHandle_HeightmapFilename)
{
	FString HeightmapFilename = NewValue.ToString();
	ensure(PropertyHandle_HeightmapFilename->SetValue(HeightmapFilename) == FPropertyAccess::Success);
}

void FLandscapeEditorDetailCustomization_NewLandscape::OnImportHeightmapFilenameChanged()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		LandscapeEdMode->UISettings->OnImportHeightmapFilenameChanged();
	}
}

FReply FLandscapeEditorDetailCustomization_NewLandscape::OnImportHeightmapFilenameButtonClicked(TSharedRef<IPropertyHandle> PropertyHandle_HeightmapFilename)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	check(LandscapeEdMode != nullptr);

	// Prompt the user for the Filenames
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform != nullptr)
	{
		ILandscapeEditorModule& LandscapeEditorModule = FModuleManager::GetModuleChecked<ILandscapeEditorModule>("LandscapeEditor");
		const TCHAR* FileTypes = LandscapeEditorModule.GetHeightmapImportDialogTypeString();

		TArray<FString> OpenFilenames;
		bool bOpened = DesktopPlatform->OpenFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			NSLOCTEXT("UnrealEd", "Import", "Import").ToString(),
			LandscapeEdMode->UISettings->LastImportPath,
			TEXT(""),
			FileTypes,
			EFileDialogFlags::None,
			OpenFilenames);

		if (bOpened)
		{
			ensure(PropertyHandle_HeightmapFilename->SetValue(OpenFilenames[0]) == FPropertyAccess::Success);
			LandscapeEdMode->UISettings->LastImportPath = FPaths::GetPath(OpenFilenames[0]);
		}
	}

	return FReply::Handled();

}

TSharedRef<SWidget> FLandscapeEditorDetailCustomization_NewLandscape::GetImportLandscapeResolutionMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		for (int32 i = 0; i < LandscapeEdMode->UISettings->HeightmapImportDescriptor.ImportResolutions.Num(); i++)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Width"), LandscapeEdMode->UISettings->HeightmapImportDescriptor.ImportResolutions[i].Width);
			Args.Add(TEXT("Height"), LandscapeEdMode->UISettings->HeightmapImportDescriptor.ImportResolutions[i].Height);
			MenuBuilder.AddMenuEntry(FText::Format(LOCTEXT("ImportResolution_Format", "{Width}\u00D7{Height}"), Args), FText(), FSlateIcon(), FExecuteAction::CreateSP(this, &FLandscapeEditorDetailCustomization_NewLandscape::OnChangeImportLandscapeResolution, i));
		}
	}

	return MenuBuilder.MakeWidget();
}

void FLandscapeEditorDetailCustomization_NewLandscape::OnChangeImportLandscapeResolution(int32 Index)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		LandscapeEdMode->UISettings->OnChangeImportLandscapeResolution(Index);
	}
}

FText FLandscapeEditorDetailCustomization_NewLandscape::GetImportLandscapeResolution() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		const int32 Width = LandscapeEdMode->UISettings->ImportLandscape_Width;
		const int32 Height = LandscapeEdMode->UISettings->ImportLandscape_Height;
		if (Width != 0 && Height != 0)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Width"), Width);
			Args.Add(TEXT("Height"), Height);
			return FText::Format(LOCTEXT("ImportResolution_Format", "{Width}\u00D7{Height}"), Args);
		}
		else
		{
			return LOCTEXT("ImportResolution_Invalid", "(invalid)");
		}
	}

	return FText::GetEmpty();
}

EVisibility FLandscapeEditorDetailCustomization_NewLandscape::GetMaterialTipVisibility() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		if (LandscapeEdMode->UISettings->ImportLandscape_Layers.Num() == 0)
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
