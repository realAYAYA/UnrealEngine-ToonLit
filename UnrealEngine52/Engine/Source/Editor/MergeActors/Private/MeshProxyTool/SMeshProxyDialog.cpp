// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshProxyTool/SMeshProxyDialog.h"
#include "Editor.h"
#include "Styling/AppStyle.h"
#include "Engine/MeshMerging.h"
#include "Engine/Selection.h"
#include "MeshProxyTool/MeshProxyTool.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Styling/SlateTypes.h"
#include "SlateOptMacros.h"
#include "UObject/UnrealType.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"


#define LOCTEXT_NAMESPACE "SMeshProxyDialog"

//////////////////////////////////////////////////////////////////////////
// SMeshProxyDialog
SMeshProxyDialog::SMeshProxyDialog()
{
    MergeStaticMeshComponentsLabel = LOCTEXT("CreateProxyMeshComponentsLabel", "Mesh components used to compute the proxy mesh:");
	SelectedComponentsListBoxToolTip = LOCTEXT("CreateProxyMeshSelectedComponentsListBoxToolTip", "The selected mesh components will be used to compute the proxy mesh");
    DeleteUndoLabel = LOCTEXT("DeleteUndo", "Insufficient mesh components found for ProxyLOD merging.");
}

SMeshProxyDialog::~SMeshProxyDialog()
{
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void  SMeshProxyDialog::Construct(const FArguments& InArgs, FMeshProxyTool* InTool)
{
	checkf(InTool != nullptr, TEXT("Invalid owner tool supplied"));
	Tool = InTool;

	SMeshProxyCommonDialog::Construct(SMeshProxyCommonDialog::FArguments());

	ProxySettings = UMeshProxySettingsObject::Get();
	SettingsView->SetObject(ProxySettings);
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SThirdPartyMeshProxyDialog::Construct(const FArguments& InArgs, FThirdPartyMeshProxyTool* InTool)
{
	Tool = InTool;
	check(Tool != nullptr);

	CuttingPlaneOptions.Add(MakeShareable(new FString(TEXT("+X"))));
	CuttingPlaneOptions.Add(MakeShareable(new FString(TEXT("+Y"))));
	CuttingPlaneOptions.Add(MakeShareable(new FString(TEXT("+Z"))));
	CuttingPlaneOptions.Add(MakeShareable(new FString(TEXT("-X"))));
	CuttingPlaneOptions.Add(MakeShareable(new FString(TEXT("-Y"))));
	CuttingPlaneOptions.Add(MakeShareable(new FString(TEXT("-Z"))));

	TextureResolutionOptions.Add(MakeShareable(new FString(TEXT("64"))));
	TextureResolutionOptions.Add(MakeShareable(new FString(TEXT("128"))));
	TextureResolutionOptions.Add(MakeShareable(new FString(TEXT("256"))));
	TextureResolutionOptions.Add(MakeShareable(new FString(TEXT("512"))));
	TextureResolutionOptions.Add(MakeShareable(new FString(TEXT("1024"))));
	TextureResolutionOptions.Add(MakeShareable(new FString(TEXT("2048"))));

	CreateLayout();
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void  SThirdPartyMeshProxyDialog::CreateLayout()
{
	int32 TextureResEntryIndex = FindTextureResolutionEntryIndex(Tool->ProxySettings.MaterialSettings.TextureSize.X);
	int32 LightMapResEntryIndex = FindTextureResolutionEntryIndex(Tool->ProxySettings.LightMapResolution);
	TextureResEntryIndex = FMath::Max(TextureResEntryIndex, 0);
	LightMapResEntryIndex = FMath::Max(LightMapResEntryIndex, 0);
		
	this->ChildSlot
	[
		SNew(SVerticalBox)
			
		+SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(10)
		[
			// Simplygon logo
			SNew(SImage)
			.Image(FAppStyle::GetBrush("MeshProxy.SimplygonLogo"))
		]
			
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				// Proxy options
				SNew(SVerticalBox)

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("OnScreenSizeLabel", "On Screen Size (pixels)"))
						.Font(FAppStyle::GetFontStyle("StandardDialog.SmallFont"))
						.ToolTipText(GetPropertyToolTipText(GET_MEMBER_NAME_CHECKED(FMeshProxySettings, ScreenSize)))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SNew(SBox)
						.HAlign(HAlign_Fill)
						.MinDesiredWidth(100.0f)
						.MaxDesiredWidth(100.0f)
						[
							SNew(SNumericEntryBox<int32>)
							.Font(FAppStyle::GetFontStyle("StandardDialog.SmallFont"))
							.MinValue(40)
							.MaxValue(1200)
							.MinSliderValue(40)
							.MaxSliderValue(1200)
							.AllowSpin(true)
							.Value(this, &SThirdPartyMeshProxyDialog::GetScreenSize)
							.OnValueChanged(this, &SThirdPartyMeshProxyDialog::ScreenSizeChanged)
						]
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("MergeDistanceLabel", "Merge Distance (pixels)"))
						.Font(FAppStyle::GetFontStyle("StandardDialog.SmallFont"))
						.ToolTipText(GetPropertyToolTipText(GET_MEMBER_NAME_CHECKED(FMeshProxySettings, MergeDistance)))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SNew(SBox)
						.HAlign(HAlign_Fill)
						.MinDesiredWidth(100.0f)
						.MaxDesiredWidth(100.0f)
						[
							SNew(SNumericEntryBox<int32>)
							.Font(FAppStyle::GetFontStyle("StandardDialog.SmallFont"))
							.MinValue(0)
							.MaxValue(300)
							.MinSliderValue(0)
							.MaxSliderValue(300)
							.AllowSpin(true)
							.Value(this, &SThirdPartyMeshProxyDialog::GetMergeDistance)
							.OnValueChanged(this, &SThirdPartyMeshProxyDialog::MergeDistanceChanged)
						]
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("TextureResolutionLabel", "Texture Resolution"))
						.Font(FAppStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SNew(STextComboBox)
						.Font(FAppStyle::GetFontStyle("StandardDialog.SmallFont"))
						.OptionsSource(&TextureResolutionOptions)
						.InitiallySelectedItem(TextureResolutionOptions[TextureResEntryIndex])
						.OnSelectionChanged(this, &SThirdPartyMeshProxyDialog::SetTextureResolution)
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("LightMapResolutionLabel", "LightMap Resolution"))
						.Font(FAppStyle::GetFontStyle("StandardDialog.SmallFont"))
						.ToolTipText(GetPropertyToolTipText(GET_MEMBER_NAME_CHECKED(FMeshProxySettings, LightMapResolution)))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SNew(STextComboBox)
						.Font(FAppStyle::GetFontStyle("StandardDialog.SmallFont"))
						.OptionsSource(&TextureResolutionOptions)
						.InitiallySelectedItem(TextureResolutionOptions[LightMapResEntryIndex])
						.OnSelectionChanged(this, &SThirdPartyMeshProxyDialog::SetLightMapResolution)
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.VAlign(VAlign_Center)
					.Padding(0.0, 0.0, 3.0, 0.0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("HardAngleLabel", "Hard Edge Angle"))
						.Font(FAppStyle::GetFontStyle("StandardDialog.SmallFont"))
						.ToolTipText(GetPropertyToolTipText(GET_MEMBER_NAME_CHECKED(FMeshProxySettings, HardAngleThreshold)))
					]
					+SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SNew(SBox)
						.HAlign(HAlign_Fill)
						.MinDesiredWidth(100.0f)
						.MaxDesiredWidth(100.0f)
						[
							SNew(SNumericEntryBox<float>)
							.Font(FAppStyle::GetFontStyle("StandardDialog.SmallFont"))
							.MinValue(0.f)
							.MaxValue(180.f)
							.MinSliderValue(0.f)
							.MaxSliderValue(180.f)
							.AllowSpin(true)
							.Value(this, &SThirdPartyMeshProxyDialog::GetHardAngleThreshold)
							.OnValueChanged(this, &SThirdPartyMeshProxyDialog::HardAngleThresholdChanged)
							.IsEnabled(this, &SThirdPartyMeshProxyDialog::HardAngleThresholdEnabled)
						]
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SCheckBox)
					.Type(ESlateCheckBoxType::CheckBox)
					.IsChecked(this, &SThirdPartyMeshProxyDialog::GetRecalculateNormals)
					.OnCheckStateChanged(this, &SThirdPartyMeshProxyDialog::SetRecalculateNormals)
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("RecalcNormalsLabel", "Recalculate Normals"))
						.Font(FAppStyle::GetFontStyle("StandardDialog.SmallFont"))
						.ToolTipText(GetPropertyToolTipText(GET_MEMBER_NAME_CHECKED(FMeshProxySettings, bRecalculateNormals)))
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SCheckBox)
					.Type(ESlateCheckBoxType::CheckBox)
					.IsChecked(this, &SThirdPartyMeshProxyDialog::GetExportNormalMap)
					.OnCheckStateChanged(this, &SThirdPartyMeshProxyDialog::SetExportNormalMap)
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ExportNormalMapLabel", "Export Normal Map"))
						.Font(FAppStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SCheckBox)
					.Type(ESlateCheckBoxType::CheckBox)
					.IsChecked(this, &SThirdPartyMeshProxyDialog::GetExportMetallicMap)
					.OnCheckStateChanged(this, &SThirdPartyMeshProxyDialog::SetExportMetallicMap)
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ExportMetallicMapLabel", "Export Metallic Map"))
						.Font(FAppStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SCheckBox)
					.Type(ESlateCheckBoxType::CheckBox)
					.IsChecked(this, &SThirdPartyMeshProxyDialog::GetExportRoughnessMap)
					.OnCheckStateChanged(this, &SThirdPartyMeshProxyDialog::SetExportRoughnessMap)
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ExportRoughnessMapLabel", "Export Roughness Map"))
						.Font(FAppStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SCheckBox)
					.Type(ESlateCheckBoxType::CheckBox)
					.IsChecked(this, &SThirdPartyMeshProxyDialog::GetExportSpecularMap)
					.OnCheckStateChanged(this, &SThirdPartyMeshProxyDialog::SetExportSpecularMap)
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ExportSpecularMapLabel", "Export Specular Map"))
						.Font(FAppStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
				]
			]
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

int32 SThirdPartyMeshProxyDialog::FindTextureResolutionEntryIndex(int32 InResolution) const
{
	FString ResolutionStr = TTypeToString<int32>::ToString(InResolution);
	
	int32 Result = TextureResolutionOptions.IndexOfByPredicate([&](const TSharedPtr<FString>& Entry)
	{
		return (ResolutionStr == *Entry);
	});

	return Result;
}

FText SThirdPartyMeshProxyDialog::GetPropertyToolTipText(const FName& PropertyName) const
{
	FProperty* Property = FMeshProxySettings::StaticStruct()->FindPropertyByName(PropertyName);
	if (Property)
	{
		return Property->GetToolTipText();
	}
	
	return FText::GetEmpty();
}

//Screen size
TOptional<int32> SThirdPartyMeshProxyDialog::GetScreenSize() const
{
	return Tool->ProxySettings.ScreenSize;
}

void SThirdPartyMeshProxyDialog::ScreenSizeChanged(int32 NewValue)
{
	Tool->ProxySettings.ScreenSize = NewValue;
}

//Recalculate normals
ECheckBoxState SThirdPartyMeshProxyDialog::GetRecalculateNormals() const
{
	return Tool->ProxySettings.bRecalculateNormals ? ECheckBoxState::Checked: ECheckBoxState::Unchecked;
}

void SThirdPartyMeshProxyDialog::SetRecalculateNormals(ECheckBoxState NewValue)
{
	Tool->ProxySettings.bRecalculateNormals = (NewValue == ECheckBoxState::Checked);
}

//Hard Angle Threshold
bool SThirdPartyMeshProxyDialog::HardAngleThresholdEnabled() const
{
	if(Tool->ProxySettings.bRecalculateNormals)
	{
		return true;
	}

	return false;
}

TOptional<float> SThirdPartyMeshProxyDialog::GetHardAngleThreshold() const
{
	return Tool->ProxySettings.HardAngleThreshold;
}

void SThirdPartyMeshProxyDialog::HardAngleThresholdChanged(float NewValue)
{
	Tool->ProxySettings.HardAngleThreshold = NewValue;
}

//Merge Distance
TOptional<int32> SThirdPartyMeshProxyDialog::GetMergeDistance() const
{
	return UE::LWC::FloatToIntCastChecked<int32>(Tool->ProxySettings.MergeDistance);
}

void SThirdPartyMeshProxyDialog::MergeDistanceChanged(int32 NewValue)
{
	Tool->ProxySettings.MergeDistance = (float)NewValue;
}

//Texture Resolution
void SThirdPartyMeshProxyDialog::SetTextureResolution(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	int32 Resolution = 512;
	TTypeFromString<int32>::FromString(Resolution, **NewSelection);
	FIntPoint TextureSize(Resolution, Resolution);
	
	Tool->ProxySettings.MaterialSettings.TextureSize = TextureSize;
}

void SThirdPartyMeshProxyDialog::SetLightMapResolution(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	int32 Resolution = 256;
	TTypeFromString<int32>::FromString(Resolution, **NewSelection);
		
	Tool->ProxySettings.LightMapResolution = Resolution;
}

ECheckBoxState SThirdPartyMeshProxyDialog::GetExportNormalMap() const
{
	return Tool->ProxySettings.MaterialSettings.bNormalMap ? ECheckBoxState::Checked :  ECheckBoxState::Unchecked;
}

void SThirdPartyMeshProxyDialog::SetExportNormalMap(ECheckBoxState NewValue)
{
	Tool->ProxySettings.MaterialSettings.bNormalMap = (NewValue == ECheckBoxState::Checked);
}

ECheckBoxState SThirdPartyMeshProxyDialog::GetExportMetallicMap() const
{
	return Tool->ProxySettings.MaterialSettings.bMetallicMap ? ECheckBoxState::Checked :  ECheckBoxState::Unchecked;
}

void SThirdPartyMeshProxyDialog::SetExportMetallicMap(ECheckBoxState NewValue)
{
	Tool->ProxySettings.MaterialSettings.bMetallicMap = (NewValue == ECheckBoxState::Checked);
}

ECheckBoxState SThirdPartyMeshProxyDialog::GetExportRoughnessMap() const
{
	return Tool->ProxySettings.MaterialSettings.bRoughnessMap ? ECheckBoxState::Checked :  ECheckBoxState::Unchecked;
}

void SThirdPartyMeshProxyDialog::SetExportRoughnessMap(ECheckBoxState NewValue)
{
	Tool->ProxySettings.MaterialSettings.bRoughnessMap = (NewValue == ECheckBoxState::Checked);
}

ECheckBoxState SThirdPartyMeshProxyDialog::GetExportSpecularMap() const
{
	return Tool->ProxySettings.MaterialSettings.bSpecularMap ? ECheckBoxState::Checked :  ECheckBoxState::Unchecked;
}

void SThirdPartyMeshProxyDialog::SetExportSpecularMap(ECheckBoxState NewValue)
{
	Tool->ProxySettings.MaterialSettings.bSpecularMap = (NewValue == ECheckBoxState::Checked);
}


#undef LOCTEXT_NAMESPACE
