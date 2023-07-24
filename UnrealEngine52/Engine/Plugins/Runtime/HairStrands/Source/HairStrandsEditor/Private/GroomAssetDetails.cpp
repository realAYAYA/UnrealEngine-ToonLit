// Copyright Epic Games, Inc. All Rights Reserved.

#include "GroomAssetDetails.h"
#include "Widgets/Input/SCheckBox.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "IDetailGroup.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "Widgets/SToolTip.h"
#include "IDocumentation.h"
#include "GroomComponent.h"
#include "SlateOptMacros.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Editor/UnrealEdEngine.h"
#include "EditorDirectories.h"
#include "UnrealEdGlobals.h"
#include "IDetailsView.h"
#include "MaterialList.h"
#include "PropertyCustomizationHelpers.h"
#include "Interfaces/IMainFrameModule.h"
#include "ScopedTransaction.h"
#include "Editor.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Rendering/SkeletalMeshModel.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "EditorFramework/AssetImportData.h"
#include "Logging/LogMacros.h"
#include "IHairCardGenerator.h"

#include "MeshDescription.h"
#include "MeshAttributes.h"
#include "MeshAttributeArray.h"

#include "Widgets/Input/STextComboBox.h"

#include "Widgets/Input/SNumericEntryBox.h"
#include "IDocumentation.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Input/SNumericDropDown.h"
#include "ComponentReregisterContext.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "SKismetInspector.h"
#include "PropertyEditorDelegates.h"
#include "PropertyCustomizationHelpers.h"
#include "GroomCustomAssetEditorToolkit.h"
#include "IPropertyUtilities.h"

#include "Styling/AppStyle.h"

static FLinearColor HairGroupColor(1.0f, 0.5f, 0.0f);
static FLinearColor HairLODColor(1.0f, 0.5f, 0.0f);

#define LOCTEXT_NAMESPACE "GroomRenderingDetails"

DEFINE_LOG_CATEGORY_STATIC(LogGroomAssetDetails, Log, All);

static int32 GHairCardsProcerudalResolution = 4096;
static int32 GHairCardsProcerudalResolution_LOD0 = -1;
static int32 GHairCardsProcerudalResolution_LOD1 = -1;
static int32 GHairCardsProcerudalResolution_LOD2 = -1;
static int32 GHairCardsProcerudalResolution_LOD3 = -1;
static int32 GHairCardsProcerudalResolution_LOD4 = -1;
static int32 GHairCardsProcerudalResolution_LOD5 = -1;
static int32 GHairCardsProcerudalResolution_LOD6 = -1;
static int32 GHairCardsProcerudalResolution_LOD7 = -1;
static FAutoConsoleVariableRef CVarHairCardsProcerudalResolution(TEXT("r.HairStrands.CardsAtlas.DefaultResolution"), GHairCardsProcerudalResolution, TEXT("Default cards atlas resolution."));
static FAutoConsoleVariableRef CVarHairCardsProcerudalResolution_LOD0(TEXT("r.HairStrands.CardsAtlas.DefaultResolution.LOD0"), GHairCardsProcerudalResolution_LOD0, TEXT("Default cards atlas resolution for LOD0."));
static FAutoConsoleVariableRef CVarHairCardsProcerudalResolution_LOD1(TEXT("r.HairStrands.CardsAtlas.DefaultResolution.LOD1"), GHairCardsProcerudalResolution_LOD1, TEXT("Default cards atlas resolution for LOD1."));
static FAutoConsoleVariableRef CVarHairCardsProcerudalResolution_LOD2(TEXT("r.HairStrands.CardsAtlas.DefaultResolution.LOD2"), GHairCardsProcerudalResolution_LOD2, TEXT("Default cards atlas resolution for LOD2."));
static FAutoConsoleVariableRef CVarHairCardsProcerudalResolution_LOD3(TEXT("r.HairStrands.CardsAtlas.DefaultResolution.LOD3"), GHairCardsProcerudalResolution_LOD3, TEXT("Default cards atlas resolution for LOD3."));
static FAutoConsoleVariableRef CVarHairCardsProcerudalResolution_LOD4(TEXT("r.HairStrands.CardsAtlas.DefaultResolution.LOD4"), GHairCardsProcerudalResolution_LOD4, TEXT("Default cards atlas resolution for LOD4."));
static FAutoConsoleVariableRef CVarHairCardsProcerudalResolution_LOD5(TEXT("r.HairStrands.CardsAtlas.DefaultResolution.LOD5"), GHairCardsProcerudalResolution_LOD5, TEXT("Default cards atlas resolution for LOD5."));
static FAutoConsoleVariableRef CVarHairCardsProcerudalResolution_LOD6(TEXT("r.HairStrands.CardsAtlas.DefaultResolution.LOD6"), GHairCardsProcerudalResolution_LOD6, TEXT("Default cards atlas resolution for LOD6."));
static FAutoConsoleVariableRef CVarHairCardsProcerudalResolution_LOD7(TEXT("r.HairStrands.CardsAtlas.DefaultResolution.LOD7"), GHairCardsProcerudalResolution_LOD7, TEXT("Default cards atlas resolution for LOD7."));

static uint32 GetHairCardsAtlasResolution(int32 InLODIndex, int32 PrevResolution)
{
	uint32 OutResolution = GHairCardsProcerudalResolution;

	const uint32 LODIndex = FMath::Clamp(InLODIndex, 0, 7);
	switch (LODIndex)
	{
	case 0: OutResolution = GHairCardsProcerudalResolution_LOD0 >= 0 ? GHairCardsProcerudalResolution_LOD0 : uint32(GHairCardsProcerudalResolution); break;
	case 1: OutResolution = GHairCardsProcerudalResolution_LOD1 >= 0 ? GHairCardsProcerudalResolution_LOD1 : uint32(PrevResolution * 0.5f); break;
	case 2: OutResolution = GHairCardsProcerudalResolution_LOD2 >= 0 ? GHairCardsProcerudalResolution_LOD2 : uint32(PrevResolution * 0.5f); break;
	case 3: OutResolution = GHairCardsProcerudalResolution_LOD3 >= 0 ? GHairCardsProcerudalResolution_LOD3 : uint32(PrevResolution * 0.5f); break;
	case 4: OutResolution = GHairCardsProcerudalResolution_LOD4 >= 0 ? GHairCardsProcerudalResolution_LOD4 : uint32(PrevResolution * 0.5f); break;
	case 5: OutResolution = GHairCardsProcerudalResolution_LOD5 >= 0 ? GHairCardsProcerudalResolution_LOD5 : uint32(PrevResolution * 0.5f); break;
	case 6: OutResolution = GHairCardsProcerudalResolution_LOD6 >= 0 ? GHairCardsProcerudalResolution_LOD5 : uint32(PrevResolution * 0.5f); break;
	case 7: OutResolution = GHairCardsProcerudalResolution_LOD6 >= 0 ? GHairCardsProcerudalResolution_LOD6 : uint32(PrevResolution * 0.5f); break;
	}

	const uint32 MinResolution = 128;
	const uint32 MaxResolution = 16384;
	return FMath::Clamp(OutResolution, MinResolution, MaxResolution);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// Array panel for hair strands infos
TSharedRef<SUniformGridPanel> MakeHairStrandsInfoGrid(const FSlateFontInfo& DetailFontInfo, FHairGroupInfoWithVisibility& CurrentAsset, float MaxRadius)
{
	TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(2.0f);

	// Header
	Grid->AddSlot(1, 0) // x, y
	.HAlign(HAlign_Right)
	[
		SNew(STextBlock)
		.Font(DetailFontInfo)
		.Text(LOCTEXT("HairInfo_Curves", "Curves"))
	];

	Grid->AddSlot(2, 0) // x, y
	.HAlign(HAlign_Right)
	[
		SNew(STextBlock)
		.Font(DetailFontInfo)
		.Text(LOCTEXT("HairInfo_Vertices", "Vertices"))
	];

	// Strands
	Grid->AddSlot(0, 1) // x, y
	.HAlign(HAlign_Left)
	[
		SNew(STextBlock)
		.Font(DetailFontInfo)
		.Text(LOCTEXT("HairInfo_Strands", "Strands"))
	];
	Grid->AddSlot(1, 1) // x, y
	.HAlign(HAlign_Right)
	[
		SNew(STextBlock)
		.Font(DetailFontInfo)
		.Text(FText::AsNumber(CurrentAsset.NumCurves))
	];
	Grid->AddSlot(2, 1) // x, y
	.HAlign(HAlign_Right)
	[
		SNew(STextBlock)
		.Font(DetailFontInfo)
		.Text(FText::AsNumber(CurrentAsset.NumCurveVertices))
	];

	// Guides
	Grid->AddSlot(0, 2) // x, y
	.HAlign(HAlign_Left)
	[
		SNew(STextBlock)
		.Font(DetailFontInfo)
		.Text(LOCTEXT("HairInfo_Guides", "Guides"))
	];
	Grid->AddSlot(1, 2) // x, y
	.HAlign(HAlign_Right)
	[
		SNew(STextBlock)
		.Font(DetailFontInfo)
		.Text(FText::AsNumber(CurrentAsset.NumGuides))
	];
	Grid->AddSlot(2, 2) // x, y
	.HAlign(HAlign_Right)
	[
		SNew(STextBlock)
		.Font(DetailFontInfo)
		.Text(FText::AsNumber(CurrentAsset.NumGuideVertices))
	];

	// Width (mm)
	Grid->AddSlot(0, 3) // x, y
	.HAlign(HAlign_Left)
	[
		SNew(STextBlock)
		.Font(DetailFontInfo)
		.Text(LOCTEXT("HairInfo_Width", "Max. Width"))
	];
	Grid->AddSlot(1, 3) // x, y
	.HAlign(HAlign_Right)
	[
		SNew(STextBlock)
		.Font(DetailFontInfo)
		.Text(LOCTEXT("HairInfo_GuideWidth", ""))
	];
	Grid->AddSlot(2, 3) // x, y
	.HAlign(HAlign_Right)
	[
		SNew(STextBlock)
		.Font(DetailFontInfo)
		.Text(FText::AsNumber(MaxRadius*2.0f))
	];

	// Imported Width (mm)
	Grid->AddSlot(0, 4) // x, y
	.HAlign(HAlign_Left)
	[
		SNew(STextBlock)
		.Font(DetailFontInfo)
	.Text(LOCTEXT("HairInfo_ImportedWidth", "Max. Imported Width"))
	];
	Grid->AddSlot(1, 4) // x, y
	.HAlign(HAlign_Right)
	[
		SNew(STextBlock)
		.Font(DetailFontInfo)
	.Text(LOCTEXT("HairInfo_GuideImportedWidth", ""))
	];
	Grid->AddSlot(2, 4) // x, y
	.HAlign(HAlign_Right)
	[
		SNew(STextBlock)
		.Font(DetailFontInfo)
		.Text(CurrentAsset.MaxImportedWidth > 0.f ? FText::AsNumber(CurrentAsset.MaxImportedWidth) : LOCTEXT("HairInfo_ImportedWidthDefault", "Not exported"))
	];
	return Grid;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// Array panel for hair cards infos

TSharedRef<SUniformGridPanel> MakeHairCardsInfoGrid(const FSlateFontInfo& DetailFontInfo, FHairGroupCardsInfo& CardsInfo)
{
	TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(2.0f);

	// Header
	Grid->AddSlot(0, 0) // x, y
	.HAlign(HAlign_Right)
	[
		SNew(STextBlock)
		.Font(DetailFontInfo)
		.Text(LOCTEXT("HairCardsInfo_Curves", "Cards"))
	];

	Grid->AddSlot(1, 0) // x, y
	.HAlign(HAlign_Right)
	[
		SNew(STextBlock)
		.Font(DetailFontInfo)
		.Text(LOCTEXT("HairCardsInfo_Vertices", "Vertices"))
	];

	// Strands
	Grid->AddSlot(0, 1) // x, y
	.HAlign(HAlign_Right)
	[
		SNew(STextBlock)
		.Font(DetailFontInfo)
		.Text(FText::AsNumber(CardsInfo.NumCards))
	];
	Grid->AddSlot(1, 1) // x, y
	.HAlign(HAlign_Right)
	[
		SNew(STextBlock)
		.Font(DetailFontInfo)
		.Text(FText::AsNumber(CardsInfo.NumCardVertices))
	];

	return Grid;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Details view

FGroomRenderingDetails::FGroomRenderingDetails(IGroomCustomAssetEditorToolkit* InToolkit, EMaterialPanelType Type)
{
	if (InToolkit)
	{
		GroomAsset = InToolkit->GetCustomAsset();
		Toolkit = InToolkit;
	}
	bDeleteWarningConsumed = false;
	PanelType = Type;
}

FGroomRenderingDetails::~FGroomRenderingDetails()
{

}

TSharedRef<IDetailCustomization> FGroomRenderingDetails::MakeInstance(IGroomCustomAssetEditorToolkit* InToolkit, EMaterialPanelType Type)
{
	return MakeShareable(new FGroomRenderingDetails(InToolkit, Type));
}

FName GetCategoryName(EMaterialPanelType Type)
{
	switch (Type) 
	{
	case EMaterialPanelType::Strands:		return FName(TEXT("Strands"));
	case EMaterialPanelType::Cards:			return FName(TEXT("Cards"));
	case EMaterialPanelType::Meshes:		return FName(TEXT("Meshes"));
	case EMaterialPanelType::Interpolation: return FName(TEXT("Interpolation"));
	case EMaterialPanelType::LODs:			return FName(TEXT("LODs"));
	case EMaterialPanelType::Physics:		return FName(TEXT("Physics"));
	case EMaterialPanelType::Bindings:		return FName(TEXT("Bindings"));
	}
	return FName(TEXT("Unknown"));
}

void FGroomRenderingDetails::ApplyChanges()
{
	GroomDetailLayout->ForceRefreshDetails();
}

void FGroomRenderingDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = DetailLayout.GetSelectedObjects();
	check(SelectedObjects.Num() <= 1); // The OnGenerateCustomWidgets delegate will not be useful if we try to process more than one object.

	FName CategoryName = GetCategoryName(PanelType);
	GroomDetailLayout = &DetailLayout;
	if(SelectedObjects.Num() > 0)
	{
		if (UGroomAsset* LocalGroomAsset = Cast<UGroomAsset>(SelectedObjects[0].Get()))
		{
			GroomAsset = LocalGroomAsset;

			IDetailCategoryBuilder& HairGroupCategory = DetailLayout.EditCategory(CategoryName, FText::GetEmpty(), ECategoryPriority::TypeSpecific);
			CustomizeStrandsGroupProperties(DetailLayout, HairGroupCategory);
		}
		else if (UGroomBindingAssetList* LocalGroomBindingList = Cast<UGroomBindingAssetList>(SelectedObjects[0].Get()))
		{
			GroomBindingAssetList = LocalGroomBindingList;

			IDetailCategoryBuilder& HairGroupCategory = DetailLayout.EditCategory(CategoryName, FText::GetEmpty(), ECategoryPriority::TypeSpecific);
			CustomizeStrandsGroupProperties(DetailLayout, HairGroupCategory);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Custom widget for material slot for hair rendering

void FGroomRenderingDetails::AddNewGroupButton(IDetailCategoryBuilder& FilesCategory, FProperty* Property, const FText& HeaderText)
{
	// Add a button for adding element to the hair groups array
	FilesCategory.AddCustomRow(FText::FromString(TEXT("AddGroup")))
	.ValueContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2.f, 2.f, 10.f, 2.f)
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(HeaderText)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.OnClicked(this, &FGroomRenderingDetails::OnAddGroup, Property)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.PlusCircle"))
			]
		]
	];
}

void FGroomRenderingDetails::CustomizeStrandsGroupProperties(IDetailLayoutBuilder& DetailLayout, IDetailCategoryBuilder& FilesCategory)
{
	switch (PanelType)
	{
	case EMaterialPanelType::Cards:
	{
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsInterpolation), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsRendering), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsPhysics), UGroomAsset::StaticClass()));
//		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsCards), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsMeshes), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsMaterials), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsLOD), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsInfo), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, EnableGlobalInterpolation), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairInterpolationType), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, MinLOD), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, DisableBelowMinLodStripping), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, RiggedSkeletalMesh), UGroomAsset::StaticClass()));
	}
	break;
	case EMaterialPanelType::Meshes:
	{
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsInterpolation), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsRendering), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsPhysics), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsCards), UGroomAsset::StaticClass()));
//		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsMeshes), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsMaterials), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsLOD), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsInfo), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, EnableGlobalInterpolation), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairInterpolationType), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, MinLOD), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, DisableBelowMinLodStripping), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, RiggedSkeletalMesh), UGroomAsset::StaticClass()));
	}
	break;
	case EMaterialPanelType::Strands:
	{
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsInterpolation), UGroomAsset::StaticClass()));
//		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsRendering), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsPhysics), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsCards), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsMeshes), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsMaterials), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsLOD), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsInfo), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, EnableGlobalInterpolation), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairInterpolationType), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, MinLOD), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, DisableBelowMinLodStripping), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, RiggedSkeletalMesh), UGroomAsset::StaticClass()));
	}
	break;
	case EMaterialPanelType::Physics:
	{
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsInterpolation), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsRendering), UGroomAsset::StaticClass()));
//		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsPhysics), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsCards), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsMeshes), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsMaterials), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsLOD), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsInfo), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, EnableGlobalInterpolation), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairInterpolationType), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, MinLOD), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, DisableBelowMinLodStripping), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, RiggedSkeletalMesh), UGroomAsset::StaticClass()));
	}
	break;
	case EMaterialPanelType::Interpolation:
	{
//		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsInterpolation), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsRendering), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsPhysics), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsCards), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsMeshes), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsMaterials), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsLOD), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsInfo), UGroomAsset::StaticClass()));
//		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, EnableGlobalInterpolation), UGroomAsset::StaticClass()));
//		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairInterpolationType), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, MinLOD), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, DisableBelowMinLodStripping), UGroomAsset::StaticClass()));
		//DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, RiggedSkeletalMesh), UGroomAsset::StaticClass()));
	}
	break;
	case EMaterialPanelType::LODs:
	{
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsInterpolation), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsRendering), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsPhysics), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsCards), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsMeshes), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsMaterials), UGroomAsset::StaticClass()));
//		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsLOD), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsInfo), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, EnableGlobalInterpolation), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairInterpolationType), UGroomAsset::StaticClass()));
//		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, MinLOD), UGroomAsset::StaticClass()));
//		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, DisableBelowMinLodStripping), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, RiggedSkeletalMesh), UGroomAsset::StaticClass()));
	}
	break;
	}

	switch (PanelType)
	{
		case EMaterialPanelType::Cards:		
		{
			TSharedRef<IPropertyHandle> Property = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsCards), UGroomAsset::StaticClass());
			AddNewGroupButton(FilesCategory, Property->GetProperty(), FText::FromString(TEXT("Add Card asset")));
			if (Property->IsValidHandle())
			{
				TSharedRef<FDetailArrayBuilder> PropertyBuilder = MakeShareable(new FDetailArrayBuilder(Property, false, false, false));
				PropertyBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FGroomRenderingDetails::OnGenerateElementForHairGroup, &DetailLayout));
				PropertyBuilder->SetDisplayName(FText::FromString(TEXT("Cards assets")));
				FilesCategory.AddCustomBuilder(PropertyBuilder, false);
			}
		}
		break;
		case EMaterialPanelType::Meshes:	
		{
			TSharedRef<IPropertyHandle> Property = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsMeshes), UGroomAsset::StaticClass());
			AddNewGroupButton(FilesCategory, Property->GetProperty(), FText::FromString(TEXT("Add Mesh asset")));
			if (Property->IsValidHandle())
			{
				TSharedRef<FDetailArrayBuilder> PropertyBuilder = MakeShareable(new FDetailArrayBuilder(Property, false, false, false));
				PropertyBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FGroomRenderingDetails::OnGenerateElementForHairGroup, &DetailLayout));
				PropertyBuilder->SetDisplayName(FText::FromString(TEXT("Meshes assets")));
				FilesCategory.AddCustomBuilder(PropertyBuilder, false);
			}
		}
		break;
		case EMaterialPanelType::Strands:	
		{
			TSharedRef<IPropertyHandle> Property = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsRendering), UGroomAsset::StaticClass());
			if (Property->IsValidHandle())
			{
				TSharedRef<FDetailArrayBuilder> PropertyBuilder = MakeShareable(new FDetailArrayBuilder(Property, false, false, false));
				PropertyBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FGroomRenderingDetails::OnGenerateElementForHairGroup, &DetailLayout));
				PropertyBuilder->SetDisplayName(FText::FromString(TEXT("Strands Groups")));
				FilesCategory.AddCustomBuilder(PropertyBuilder, false);
			}
		}
		break;
		case EMaterialPanelType::Physics:
		{
			TSharedRef<IPropertyHandle> Property = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsPhysics), UGroomAsset::StaticClass());
			if (Property->IsValidHandle())
			{
				TSharedRef<FDetailArrayBuilder> PropertyBuilder = MakeShareable(new FDetailArrayBuilder(Property, false, false, false));
				PropertyBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FGroomRenderingDetails::OnGenerateElementForHairGroup, &DetailLayout));
				PropertyBuilder->SetDisplayName(FText::FromString(TEXT("Physics Groups")));
				FilesCategory.AddCustomBuilder(PropertyBuilder, false);
			}
		}
		break;
		case EMaterialPanelType::Interpolation:
		{
			TSharedRef<IPropertyHandle> Property = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsInterpolation), UGroomAsset::StaticClass());
			if (Property->IsValidHandle())
			{
				TSharedRef<FDetailArrayBuilder> PropertyBuilder = MakeShareable(new FDetailArrayBuilder(Property, false, false, false));
				PropertyBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FGroomRenderingDetails::OnGenerateElementForHairGroup, &DetailLayout));
				PropertyBuilder->SetDisplayName(FText::FromString(TEXT("Interpolation Groups")));
				FilesCategory.AddCustomBuilder(PropertyBuilder, false);
			}
		}
		break;
		case EMaterialPanelType::LODs:
		{
			TSharedRef<IPropertyHandle> Property = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsLOD), UGroomAsset::StaticClass());
			if (Property->IsValidHandle())
			{
				TSharedRef<FDetailArrayBuilder> PropertyBuilder = MakeShareable(new FDetailArrayBuilder(Property, false, false, false));
				PropertyBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FGroomRenderingDetails::OnGenerateElementForHairGroup, &DetailLayout));
				PropertyBuilder->SetDisplayName(FText::FromString(TEXT("LOD Groups")));
				FilesCategory.AddCustomBuilder(PropertyBuilder, false);
			}
		}
		break;
		case EMaterialPanelType::Bindings:
		{
			TSharedRef<IPropertyHandle> Property = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomBindingAssetList, Bindings), UGroomBindingAssetList::StaticClass());
			if (Property->IsValidHandle())
			{
				TSharedRef<FDetailArrayBuilder> PropertyBuilder = MakeShareable(new FDetailArrayBuilder(Property, false, false, false));
				PropertyBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FGroomRenderingDetails::OnGenerateElementForBindingAsset, &DetailLayout));
				PropertyBuilder->SetDisplayName(FText::FromString(TEXT("Bindings")));
				FilesCategory.AddCustomBuilder(PropertyBuilder, false);
			}
		}
		break;
	}
}

FReply FGroomRenderingDetails::OnAddGroup(FProperty* Property)
{
	check(GroomAsset);
	switch (PanelType)
	{
	case EMaterialPanelType::Cards:
	{
		FScopedTransaction Transaction(FText::FromString(TEXT("AddCardsGroup")));
		GroomAsset->HairGroupsCards.AddDefaulted();

		const int32 LODCount = GroomAsset->HairGroupsCards.Num();
		if (LODCount > 1)
		{
			const FHairGroupsCardsSourceDescription& Prev = GroomAsset->HairGroupsCards[LODCount - 2];
			FHairGroupsCardsSourceDescription& Current = GroomAsset->HairGroupsCards[LODCount - 1];

			Current.SourceType = Prev.SourceType;
			Current.GroupIndex = Prev.GroupIndex;
			Current.LODIndex = FMath::Min(Prev.LODIndex + 1, 7);

			// Prefill the LOD setting with basic preset
			Current.ProceduralSettings.TextureSettings.AtlasMaxResolution  = GetHairCardsAtlasResolution(Current.LODIndex, Prev.ProceduralSettings.TextureSettings.AtlasMaxResolution);
			Current.ProceduralSettings.TextureSettings.PixelPerCentimeters = Prev.ProceduralSettings.TextureSettings.PixelPerCentimeters * 0.75f;
		}
		else
		{
			FHairGroupsCardsSourceDescription& Current = GroomAsset->HairGroupsCards[LODCount - 1];
			Current.ProceduralSettings.TextureSettings.AtlasMaxResolution  = GetHairCardsAtlasResolution(0,0);
			Current.LODIndex = 0;
		}

		FPropertyChangedEvent PropertyChangedEvent(Property, EPropertyChangeType::ArrayAdd);
		GroomAsset->PostEditChangeProperty(PropertyChangedEvent);
	}
	break;
	case EMaterialPanelType::Meshes:
	{
		FScopedTransaction Transaction(FText::FromString(TEXT("AddMeshesGroup")));
		GroomAsset->HairGroupsMeshes.AddDefaulted();

		FPropertyChangedEvent PropertyChangedEvent(Property, EPropertyChangeType::ArrayAdd);
		GroomAsset->PostEditChangeProperty(PropertyChangedEvent);
	}
	break;
	}

	return FReply::Handled();
}

FReply FGroomRenderingDetails::OnSelectBinding(int32 BindingIndex, FProperty* Property)
{
	check(GroomBindingAssetList);

	// If user click twice onto the same binding index, we disable the binding;
	BindingIndex = Toolkit->GetActiveBindingIndex() == BindingIndex ? -1 : BindingIndex;
	Toolkit->PreviewBinding(BindingIndex);
	ApplyChanges();
	return FReply::Handled();
}

FName& FGroomRenderingDetails::GetMaterialSlotName(int32 GroupIndex)
{
	check(GroomAsset);
	switch (PanelType)
	{
		case EMaterialPanelType::Cards:		return GroomAsset->HairGroupsCards[GroupIndex].MaterialSlotName;
		case EMaterialPanelType::Meshes:	return GroomAsset->HairGroupsMeshes[GroupIndex].MaterialSlotName;
		case EMaterialPanelType::Strands:	return GroomAsset->HairGroupsRendering[GroupIndex].MaterialSlotName;
	}

	static FName Default;
	return Default;
}

const FName& FGroomRenderingDetails::GetMaterialSlotName(int32 GroupIndex) const
{
	static const FName Default(TEXT("Invalid"));
	check(GroomAsset);
	switch (PanelType)
	{
	case EMaterialPanelType::Cards:		return GroupIndex < GroomAsset->HairGroupsCards.Num() ? GroomAsset->HairGroupsCards[GroupIndex].MaterialSlotName : Default;
	case EMaterialPanelType::Meshes:	return GroupIndex < GroomAsset->HairGroupsMeshes.Num() ? GroomAsset->HairGroupsMeshes[GroupIndex].MaterialSlotName : Default;
	case EMaterialPanelType::Strands:	return GroupIndex < GroomAsset->HairGroupsRendering.Num() ? GroomAsset->HairGroupsRendering[GroupIndex].MaterialSlotName : Default;
	}

	return Default;
}

int32 FGroomRenderingDetails::GetGroupCount() const
{
	check(GroomAsset);
	switch (PanelType)
	{
	case EMaterialPanelType::Cards:			return GroomAsset->HairGroupsCards.Num();
	case EMaterialPanelType::Meshes:		return GroomAsset->HairGroupsMeshes.Num();
	case EMaterialPanelType::Strands:		return GroomAsset->HairGroupsRendering.Num();
	case EMaterialPanelType::Physics:		return GroomAsset->HairGroupsPhysics.Num();
	case EMaterialPanelType::Interpolation:	return GroomAsset->HairGroupsInterpolation.Num();
	case EMaterialPanelType::LODs:			return GroomAsset->HairGroupsLOD.Num();
	}

	return 0;
}

FReply FGroomRenderingDetails::OnRemoveGroupClicked(int32 GroupIndex, FProperty* Property)
{
	check(GroomAsset);
	switch (PanelType)
	{
	case EMaterialPanelType::Cards:		
	{
		if (GroupIndex < GroomAsset->HairGroupsCards.Num())
		{
			FScopedTransaction Transaction(FText::FromString(TEXT("RemoveCardsGroup")));
			GroomAsset->HairGroupsCards.RemoveAt(GroupIndex);

			FPropertyChangedEvent PropertyChangedEvent(Property, EPropertyChangeType::ArrayRemove);
			GroomAsset->PostEditChangeProperty(PropertyChangedEvent);
		}
	}
	break;
	case EMaterialPanelType::Meshes:
	{
		if (GroupIndex < GroomAsset->HairGroupsMeshes.Num())
		{
			FScopedTransaction Transaction(FText::FromString(TEXT("RemoveMeshesGroup")));
			GroomAsset->HairGroupsMeshes.RemoveAt(GroupIndex);

			FPropertyChangedEvent PropertyChangedEvent(Property, EPropertyChangeType::ArrayRemove);
			GroomAsset->PostEditChangeProperty(PropertyChangedEvent);
		}
	}
	break;
	}

	return FReply::Handled();
}

void FGroomRenderingDetails::SetMaterialSlot(int32 GroupIndex, int32 MaterialIndex)
{
	if (!GroomAsset || GroupIndex < 0)
	{
		return;
	}

	int32 GroupCount = GetGroupCount();
	if (MaterialIndex == INDEX_NONE)
	{
		GetMaterialSlotName(GroupIndex) = NAME_None;
	}
	else if (GroupIndex < GroupCount && MaterialIndex >= 0 && MaterialIndex < GroomAsset->HairGroupsMaterials.Num())
	{
		GetMaterialSlotName(GroupIndex) = GroomAsset->HairGroupsMaterials[MaterialIndex].SlotName;
	}

	GroomAsset->MarkMaterialsHasChanged();
}

TSharedRef<SWidget> FGroomRenderingDetails::OnGenerateStrandsMaterialMenuPicker(int32 GroupIndex)
{
	if (GroomAsset == nullptr)
	{
		return SNullWidget::NullWidget;
	}

	const int32 MaterialCount = GroomAsset->HairGroupsMaterials.Num();
	if(MaterialCount == 0)
	{
		return SNullWidget::NullWidget;
	}
	FMenuBuilder MenuBuilder(true, NULL);

	// Default material
	{
		int32 MaterialIt = INDEX_NONE;
		FText DefaultString = FText::FromString(TEXT("Default"));
		FUIAction Action(FExecuteAction::CreateSP(this, &FGroomRenderingDetails::SetMaterialSlot, GroupIndex, MaterialIt));
		MenuBuilder.AddMenuEntry(DefaultString, FText::GetEmpty(), FSlateIcon(), Action);
	}

	// Add a menu item for material
	for (int32 MaterialIt = 0; MaterialIt < MaterialCount; ++MaterialIt)
	{
		FText MaterialString = FText::FromString(FString::FromInt(MaterialIt) + TEXT(" - ") + GroomAsset->HairGroupsMaterials[MaterialIt].SlotName.ToString());
		FUIAction Action(FExecuteAction::CreateSP(this, &FGroomRenderingDetails::SetMaterialSlot, GroupIndex, MaterialIt));
		MenuBuilder.AddMenuEntry(MaterialString, FText::GetEmpty(), FSlateIcon(), Action);
	}

	return MenuBuilder.MakeWidget();
}

FText FGroomRenderingDetails::GetStrandsMaterialName(int32 GroupIndex) const
{
	const FName& MaterialSlotName = GetMaterialSlotName(GroupIndex);
	const int32 MaterialIndex = GroomAsset->GetMaterialIndex(MaterialSlotName);
	FText MaterialString = FText::FromString(TEXT("Default"));
	if (MaterialIndex != INDEX_NONE)
	{
		MaterialString = FText::FromString(FString::FromInt(MaterialIndex) + TEXT(" - ") + GroomAsset->HairGroupsMaterials[MaterialIndex].SlotName.ToString());
	}
	return MaterialString;
}

TSharedRef<SWidget> FGroomRenderingDetails::OnGenerateStrandsMaterialPicker(int32 GroupIndex, IDetailLayoutBuilder* DetailLayoutBuilder)
{
	return CreateMaterialSwatch(DetailLayoutBuilder->GetThumbnailPool(), GroupIndex);
}

bool FGroomRenderingDetails::IsStrandsMaterialPickerEnabled(int32 GroupIndex) const
{
	return true;
}


template <typename T>
bool AssignIfDifferent(T& Dest, const T& Src, bool bSetValue)
{
	const bool bHasChanged = Dest != Src;
	if (bHasChanged && bSetValue)
	{
		Dest = Src;
	}
	return bHasChanged;
}
	
#define HAIR_RESET0(GroomMemberName, StructTypeName, MemberName)										{ if (PropertyName == GET_MEMBER_NAME_CHECKED(StructTypeName, MemberName)) { bHasChanged = AssignIfDifferent(GroomAsset->GroomMemberName[GroupIndex].MemberName, Default.MemberName, bSetValue); } }
#define HAIR_RESET1(GroomMemberName, StructTypeName, StructMemberName, MemberName)						{ if (PropertyName == GET_MEMBER_NAME_CHECKED(StructTypeName, MemberName)) { bHasChanged = AssignIfDifferent(GroomAsset->GroomMemberName[GroupIndex].StructMemberName.MemberName, Default.MemberName, bSetValue); } }
#define HAIR_RESET2(GroomMemberName, StructTypeName, StructMemberName, SubStructMemberName, MemberName)	{ if (PropertyName == GET_MEMBER_NAME_CHECKED(StructTypeName, MemberName)) { bHasChanged = AssignIfDifferent(GroomAsset->GroomMemberName[GroupIndex].StructMemberName.SubStructMemberName.MemberName, Default.MemberName, bSetValue); } }

bool FGroomRenderingDetails::CommonResetToDefault(TSharedPtr<IPropertyHandle> ChildHandle, int32 GroupIndex, int32 LODIndex, bool bSetValue)
{
	bool bHasChanged = false;
	if (ChildHandle == nullptr || GroomAsset == nullptr || GroupIndex < 0)
	{
		return bHasChanged;
	}

	FName PropertyName = ChildHandle->GetProperty()->GetFName();

	// For cards & meshes the incoming index is actually the cards/mesh description index, not the group index
	// For the rest, the group index refers to the actual group index.
	const bool bIsCardDescIndexValid = GroupIndex < GroomAsset->HairGroupsCards.Num();
	const bool bIsMeshDescIndexValid = GroupIndex < GroomAsset->HairGroupsMeshes.Num();
	const bool bIsGroupIndexValid = GroupIndex < GroomAsset->GetNumHairGroups();
		
	// Hair strands
	if (bIsGroupIndexValid)
	{
		{
			FHairGeometrySettings Default;
			HAIR_RESET1(HairGroupsRendering, FHairGeometrySettings, GeometrySettings, HairWidth);
			HAIR_RESET1(HairGroupsRendering, FHairGeometrySettings, GeometrySettings, HairRootScale);
			HAIR_RESET1(HairGroupsRendering, FHairGeometrySettings, GeometrySettings, HairTipScale);
		}

		{
			FHairShadowSettings Default;
			HAIR_RESET1(HairGroupsRendering, FHairShadowSettings, ShadowSettings, bVoxelize);
			HAIR_RESET1(HairGroupsRendering, FHairShadowSettings, ShadowSettings, bUseHairRaytracingGeometry);
			HAIR_RESET1(HairGroupsRendering, FHairShadowSettings, ShadowSettings, HairRaytracingRadiusScale);
			HAIR_RESET1(HairGroupsRendering, FHairShadowSettings, ShadowSettings, HairRaytracingRadiusScale);
		}

		{
			FHairAdvancedRenderingSettings Default;
			HAIR_RESET1(HairGroupsRendering, FHairAdvancedRenderingSettings, AdvancedSettings, bScatterSceneLighting);
			HAIR_RESET1(HairGroupsRendering, FHairAdvancedRenderingSettings, AdvancedSettings, bUseStableRasterization);
		}
	}

	// Interpolation
	if (bIsGroupIndexValid)
	{
		{
			FHairDecimationSettings Default;
			HAIR_RESET1(HairGroupsInterpolation, FHairDecimationSettings, DecimationSettings, CurveDecimation);
			HAIR_RESET1(HairGroupsInterpolation, FHairDecimationSettings, DecimationSettings, VertexDecimation);
		}

		{
			FHairInterpolationSettings Default;
			HAIR_RESET1(HairGroupsInterpolation, FHairInterpolationSettings, InterpolationSettings, bOverrideGuides);
			HAIR_RESET1(HairGroupsInterpolation, FHairInterpolationSettings, InterpolationSettings, HairToGuideDensity);
			HAIR_RESET1(HairGroupsInterpolation, FHairInterpolationSettings, InterpolationSettings, bRandomizeGuide);
			HAIR_RESET1(HairGroupsInterpolation, FHairInterpolationSettings, InterpolationSettings, bUseUniqueGuide);
		}

		{
			FHairDeformationSettings Default;
			HAIR_RESET1(HairGroupsInterpolation, FHairDeformationSettings, RiggingSettings, NumCurves);
			HAIR_RESET1(HairGroupsInterpolation, FHairDeformationSettings, RiggingSettings, NumPoints);
			HAIR_RESET1(HairGroupsInterpolation, FHairDeformationSettings, RiggingSettings, bEnableRigging);
		}
	}

	// LODs
	if (bIsGroupIndexValid)
	{
		{
			FHairGroupsLOD Default;
			HAIR_RESET0(HairGroupsLOD, FHairGroupsLOD, ClusterWorldSize);
			HAIR_RESET0(HairGroupsLOD, FHairGroupsLOD, ClusterScreenSizeScale);
		}

		if (LODIndex>=0 && LODIndex < GroomAsset->HairGroupsLOD[GroupIndex].LODs.Num())
		{
			FHairLODSettings Default;
			HAIR_RESET1(HairGroupsLOD, FHairLODSettings, LODs[LODIndex], CurveDecimation);
			HAIR_RESET1(HairGroupsLOD, FHairLODSettings, LODs[LODIndex], VertexDecimation);
			HAIR_RESET1(HairGroupsLOD, FHairLODSettings, LODs[LODIndex], AngularThreshold);
			HAIR_RESET1(HairGroupsLOD, FHairLODSettings, LODs[LODIndex], ScreenSize);
			HAIR_RESET1(HairGroupsLOD, FHairLODSettings, LODs[LODIndex], ThicknessScale);
			HAIR_RESET1(HairGroupsLOD, FHairLODSettings, LODs[LODIndex], GeometryType);
		}
	}

	// Cards
	if (bIsCardDescIndexValid)
	{
		{
			FHairGroupsCardsSourceDescription Default;
			HAIR_RESET0(HairGroupsCards, FHairGroupsCardsSourceDescription, SourceType);
			HAIR_RESET0(HairGroupsCards, FHairGroupsCardsSourceDescription, ProceduralMesh);
			HAIR_RESET0(HairGroupsCards, FHairGroupsCardsSourceDescription, ImportedMesh);
			HAIR_RESET0(HairGroupsCards, FHairGroupsCardsSourceDescription, GroupIndex);
			HAIR_RESET0(HairGroupsCards, FHairGroupsCardsSourceDescription, LODIndex);
		}

		{
			FHairGroupCardsTextures Default;
			HAIR_RESET1(HairGroupsCards, FHairGroupCardsTextures, Textures, DepthTexture);
			HAIR_RESET1(HairGroupsCards, FHairGroupCardsTextures, Textures, CoverageTexture);
			HAIR_RESET1(HairGroupsCards, FHairGroupCardsTextures, Textures, TangentTexture);
			HAIR_RESET1(HairGroupsCards, FHairGroupCardsTextures, Textures, AttributeTexture);
			HAIR_RESET1(HairGroupsCards, FHairGroupCardsTextures, Textures, MaterialTexture);
			HAIR_RESET1(HairGroupsCards, FHairGroupCardsTextures, Textures, AuxilaryDataTexture);
		}
	}

	// Meshes
	if (bIsMeshDescIndexValid)
	{
		{
			FHairGroupsMeshesSourceDescription Default;
			HAIR_RESET0(HairGroupsMeshes, FHairGroupsMeshesSourceDescription, ImportedMesh);
			HAIR_RESET0(HairGroupsMeshes, FHairGroupsMeshesSourceDescription, GroupIndex);
			HAIR_RESET0(HairGroupsMeshes, FHairGroupsMeshesSourceDescription, LODIndex);
		}

		{
			FHairGroupCardsTextures Default;
			HAIR_RESET1(HairGroupsMeshes, FHairGroupCardsTextures, Textures, DepthTexture);
			HAIR_RESET1(HairGroupsMeshes, FHairGroupCardsTextures, Textures, CoverageTexture);
			HAIR_RESET1(HairGroupsMeshes, FHairGroupCardsTextures, Textures, TangentTexture);
			HAIR_RESET1(HairGroupsMeshes, FHairGroupCardsTextures, Textures, AttributeTexture);
			HAIR_RESET1(HairGroupsMeshes, FHairGroupCardsTextures, Textures, MaterialTexture);
			HAIR_RESET1(HairGroupsMeshes, FHairGroupCardsTextures, Textures, AuxilaryDataTexture);
		}
	}

	// Physics
	if (bIsGroupIndexValid)
	{
		{
			FHairSolverSettings Default;
			HAIR_RESET1(HairGroupsPhysics, FHairSolverSettings, SolverSettings, EnableSimulation);
			HAIR_RESET1(HairGroupsPhysics, FHairSolverSettings, SolverSettings, NiagaraSolver);
			HAIR_RESET1(HairGroupsPhysics, FHairSolverSettings, SolverSettings, CustomSystem);
			HAIR_RESET1(HairGroupsPhysics, FHairSolverSettings, SolverSettings, SubSteps);
			HAIR_RESET1(HairGroupsPhysics, FHairSolverSettings, SolverSettings, IterationCount);
		}

		{
			FHairExternalForces Default;
			HAIR_RESET1(HairGroupsPhysics, FHairExternalForces, ExternalForces, GravityVector);
			HAIR_RESET1(HairGroupsPhysics, FHairExternalForces, ExternalForces, AirDrag);
			HAIR_RESET1(HairGroupsPhysics, FHairExternalForces, ExternalForces, AirVelocity);
		}

		{
			FHairBendConstraint Default;
			HAIR_RESET2(HairGroupsPhysics, FHairBendConstraint, MaterialConstraints, BendConstraint, SolveBend);
			HAIR_RESET2(HairGroupsPhysics, FHairBendConstraint, MaterialConstraints, BendConstraint, ProjectBend);
			HAIR_RESET2(HairGroupsPhysics, FHairBendConstraint, MaterialConstraints, BendConstraint, BendDamping);
			HAIR_RESET2(HairGroupsPhysics, FHairBendConstraint, MaterialConstraints, BendConstraint, BendStiffness);
//			HAIR_RESET2(HairGroupsPhysics, FHairBendConstraint, MaterialConstraints, BendConstraint, BendScale);
		}

		{
			FHairStretchConstraint Default;
			HAIR_RESET2(HairGroupsPhysics, FHairStretchConstraint, MaterialConstraints, StretchConstraint, SolveStretch);
			HAIR_RESET2(HairGroupsPhysics, FHairStretchConstraint, MaterialConstraints, StretchConstraint, ProjectStretch);
			HAIR_RESET2(HairGroupsPhysics, FHairStretchConstraint, MaterialConstraints, StretchConstraint, StretchDamping);
			HAIR_RESET2(HairGroupsPhysics, FHairStretchConstraint, MaterialConstraints, StretchConstraint, StretchStiffness);
//			HAIR_RESET2(HairGroupsPhysics, FHairStretchConstraint, MaterialConstraints, StretchConstraint, StretchScale);
		}

		{
			FHairCollisionConstraint Default;
			HAIR_RESET2(HairGroupsPhysics, FHairCollisionConstraint, MaterialConstraints, CollisionConstraint, SolveCollision);
			HAIR_RESET2(HairGroupsPhysics, FHairCollisionConstraint, MaterialConstraints, CollisionConstraint, ProjectCollision);
			HAIR_RESET2(HairGroupsPhysics, FHairCollisionConstraint, MaterialConstraints, CollisionConstraint, StaticFriction);
			HAIR_RESET2(HairGroupsPhysics, FHairCollisionConstraint, MaterialConstraints, CollisionConstraint, KineticFriction);
			HAIR_RESET2(HairGroupsPhysics, FHairCollisionConstraint, MaterialConstraints, CollisionConstraint, StrandsViscosity);
			HAIR_RESET2(HairGroupsPhysics, FHairCollisionConstraint, MaterialConstraints, CollisionConstraint, GridDimension);
			HAIR_RESET2(HairGroupsPhysics, FHairCollisionConstraint, MaterialConstraints, CollisionConstraint, CollisionRadius);
//			HAIR_RESET2(HairGroupsPhysics, FHairCollisionConstraint, MaterialConstraints, CollisionConstraint, RadiusScale);
		}
		

		{
			FHairStrandsParameters Default;
			HAIR_RESET1(HairGroupsPhysics, FHairStrandsParameters, StrandsParameters, StrandsSize);
			HAIR_RESET1(HairGroupsPhysics, FHairStrandsParameters, StrandsParameters, StrandsDensity);
			HAIR_RESET1(HairGroupsPhysics, FHairStrandsParameters, StrandsParameters, StrandsSmoothing);
			HAIR_RESET1(HairGroupsPhysics, FHairStrandsParameters, StrandsParameters, StrandsThickness);
//			HAIR_RESET1(HairGroupsPhysics, FHairStrandsParameters, StrandsParameters, ThicknessScale);
		}
	}

	if (bSetValue && bHasChanged)
	{
		FPropertyChangedEvent PropertyChangedEvent(ChildHandle->GetProperty(), EPropertyChangeType::ValueSet);
		GroomAsset->PostEditChangeProperty(PropertyChangedEvent);
	}
	return bHasChanged;
}

bool FGroomRenderingDetails::ShouldResetToDefault(TSharedPtr<IPropertyHandle> ChildHandle, int32 GroupIndex, int32 LODIndex)
{
	return CommonResetToDefault(ChildHandle, GroupIndex, LODIndex, false);
}

void FGroomRenderingDetails::ResetToDefault(TSharedPtr<IPropertyHandle> ChildHandle, int32 GroupIndex, int32 LODIndex)
{
	CommonResetToDefault(ChildHandle, GroupIndex, LODIndex, true);
}

IDetailPropertyRow& FGroomRenderingDetails::AddPropertyWithCustomReset(TSharedPtr<IPropertyHandle>& PropertyHandle, IDetailChildrenBuilder& Builder, int32 GroupIndex, int32 LODIndex)
{	
	FIsResetToDefaultVisible IsResetVisible = FIsResetToDefaultVisible::CreateSP(this, &FGroomRenderingDetails::ShouldResetToDefault, GroupIndex, LODIndex);
	FResetToDefaultHandler ResetHandler = FResetToDefaultHandler::CreateSP(this, &FGroomRenderingDetails::ResetToDefault, GroupIndex, LODIndex);
	FResetToDefaultOverride ResetOverride = FResetToDefaultOverride::Create(IsResetVisible, ResetHandler);
	return Builder.AddProperty(PropertyHandle.ToSharedRef()).OverrideResetToDefault(ResetOverride);
}

void FGroomRenderingDetails::ExpandStructForLOD(TSharedRef<IPropertyHandle>& PropertyHandle, IDetailChildrenBuilder& ChildrenBuilder, int32 GroupIndex, int32 LODIndex, bool bOverrideReset)
{

	uint32 ChildrenCount = 0;
	PropertyHandle->GetNumChildren(ChildrenCount);
	for (uint32 ChildIt = 0; ChildIt < ChildrenCount; ++ChildIt)
	{
		TSharedPtr<IPropertyHandle> ChildHandle = PropertyHandle->GetChildHandle(ChildIt);
		const FName ChildPropertyName = ChildHandle->GetProperty()->GetFName();

		// If the geometry type is not strands, then bypass the display of the strands related property
		if (GroomAsset->GetGeometryType(GroupIndex, LODIndex) != EGroomGeometryType::Strands && 
			(ChildPropertyName == GET_MEMBER_NAME_CHECKED(FHairLODSettings, CurveDecimation)  ||
			 ChildPropertyName == GET_MEMBER_NAME_CHECKED(FHairLODSettings, VertexDecimation) ||
			 ChildPropertyName == GET_MEMBER_NAME_CHECKED(FHairLODSettings, AngularThreshold) ||
			 ChildPropertyName == GET_MEMBER_NAME_CHECKED(FHairLODSettings, ThicknessScale)))
		{
			continue;
		}

		if (bOverrideReset)
		{
			FIsResetToDefaultVisible IsResetVisible = FIsResetToDefaultVisible::CreateSP(this, &FGroomRenderingDetails::ShouldResetToDefault, GroupIndex, LODIndex);
			FResetToDefaultHandler ResetHandler = FResetToDefaultHandler::CreateSP(this, &FGroomRenderingDetails::ResetToDefault, GroupIndex, LODIndex);
			FResetToDefaultOverride ResetOverride = FResetToDefaultOverride::Create(IsResetVisible, ResetHandler);
			ChildrenBuilder.AddProperty(ChildHandle.ToSharedRef()).OverrideResetToDefault(ResetOverride);
		}
		else
		{
			ChildrenBuilder.AddProperty(ChildHandle.ToSharedRef());
		}
	}
}

void FGroomRenderingDetails::ExpandStruct(TSharedPtr<IPropertyHandle>& PropertyHandle, IDetailChildrenBuilder& ChildrenBuilder, int32 GroupIndex, int32 LODIndex, bool bOverrideReset)
{

	uint32 ChildrenCount = 0;
	PropertyHandle->GetNumChildren(ChildrenCount);
	for (uint32 ChildIt = 0; ChildIt < ChildrenCount; ++ChildIt)
	{
		TSharedPtr<IPropertyHandle> ChildHandle = PropertyHandle->GetChildHandle(ChildIt);
		if (bOverrideReset)
		{
			FIsResetToDefaultVisible IsResetVisible = FIsResetToDefaultVisible::CreateSP(this, &FGroomRenderingDetails::ShouldResetToDefault, GroupIndex, LODIndex);
			FResetToDefaultHandler ResetHandler     = FResetToDefaultHandler::CreateSP(this, &FGroomRenderingDetails::ResetToDefault, GroupIndex, LODIndex);
			FResetToDefaultOverride ResetOverride   = FResetToDefaultOverride::Create(IsResetVisible, ResetHandler);
			ChildrenBuilder.AddProperty(ChildHandle.ToSharedRef()).OverrideResetToDefault(ResetOverride);
		}
		else
		{
			ChildrenBuilder.AddProperty(ChildHandle.ToSharedRef());
		}
	}
}

void FGroomRenderingDetails::ExpandStruct(TSharedRef<IPropertyHandle>& PropertyHandle, IDetailChildrenBuilder& ChildrenBuilder, int32 GroupIndex, int32 LODIndex, bool bOverrideReset)
{
	uint32 ChildrenCount = 0;
	PropertyHandle->GetNumChildren(ChildrenCount);
	for (uint32 ChildIt = 0; ChildIt < ChildrenCount; ++ChildIt)
	{
		TSharedPtr<IPropertyHandle> ChildHandle = PropertyHandle->GetChildHandle(ChildIt);
		if (bOverrideReset)
		{
			FIsResetToDefaultVisible IsResetVisible = FIsResetToDefaultVisible::CreateSP(this, &FGroomRenderingDetails::ShouldResetToDefault, GroupIndex, LODIndex);
			FResetToDefaultHandler ResetHandler = FResetToDefaultHandler::CreateSP(this, &FGroomRenderingDetails::ResetToDefault, GroupIndex, LODIndex);
			FResetToDefaultOverride ResetOverride = FResetToDefaultOverride::Create(IsResetVisible, ResetHandler);
			ChildrenBuilder.AddProperty(ChildHandle.ToSharedRef()).OverrideResetToDefault(ResetOverride);
		}
		else
		{
			ChildrenBuilder.AddProperty(ChildHandle.ToSharedRef());
		}
	}
}

FReply FGroomRenderingDetails::OnRemoveLODClicked(int32 GroupIndex, int32 LODIndex, FProperty* Property)
{
	check(GroomAsset);
	if (GroupIndex < GroomAsset->HairGroupsLOD.Num() && LODIndex >= 0 && LODIndex < GroomAsset->HairGroupsLOD[GroupIndex].LODs.Num() && GroomAsset->HairGroupsLOD[GroupIndex].LODs.Num() > 1)
	{
		FScopedTransaction Transaction(FText::FromString(TEXT("RemoveLOD")));

		GroomAsset->HairGroupsLOD[GroupIndex].LODs.RemoveAt(LODIndex);

		FPropertyChangedEvent PropertyChangedEvent(Property, EPropertyChangeType::ArrayRemove);
		GroomAsset->PostEditChangeProperty(PropertyChangedEvent);
	}
	return FReply::Handled();
}

FReply FGroomRenderingDetails::OnAddLODClicked(int32 GroupIndex, FProperty* Property)
{
	const int32 MaxLODCount = 8; 
	if (GroupIndex < GroomAsset->HairGroupsLOD.Num() && GroomAsset->HairGroupsLOD[GroupIndex].LODs.Num() < MaxLODCount)
	{
		FScopedTransaction Transaction(FText::FromString(TEXT("AddLOD")));

		GroomAsset->HairGroupsLOD[GroupIndex].LODs.AddDefaulted();
		const int32 LODCount = GroomAsset->HairGroupsLOD[GroupIndex].LODs.Num();
		if (LODCount > 1)
		{
			const FHairLODSettings& PrevLODSettings = GroomAsset->HairGroupsLOD[GroupIndex].LODs[LODCount-2];
			FHairLODSettings& LODSettings = GroomAsset->HairGroupsLOD[GroupIndex].LODs[LODCount-1];

			// Prefill the LOD setting with basic preset
			LODSettings.VertexDecimation = PrevLODSettings.VertexDecimation * 0.5f;
			LODSettings.AngularThreshold = PrevLODSettings.AngularThreshold * 2.f;
			LODSettings.CurveDecimation = PrevLODSettings.CurveDecimation * 0.5f;
			LODSettings.ScreenSize = PrevLODSettings.ScreenSize * 0.5f;
			LODSettings.GeometryType = PrevLODSettings.GeometryType;
		}

		FPropertyChangedEvent PropertyChangedEvent(Property, EPropertyChangeType::ArrayAdd);
		GroomAsset->PostEditChangeProperty(PropertyChangedEvent);
	}
	return FReply::Handled();
}

// Hair_TODO: rename into OnReloadCards
FReply FGroomRenderingDetails::OnRefreshCards(int32 DescIndex, FProperty* Property)
{
	if (DescIndex < GroomAsset->HairGroupsCards.Num() && GroomAsset->HairGroupsCards[DescIndex].SourceType == EHairCardsSourceType::Procedural)
	{
		FScopedTransaction Transaction(FText::FromString(TEXT("RefreshCards")));
		
		FPropertyChangedEvent PropertyChangedEvent(Property);
		GroomAsset->PostEditChangeProperty(PropertyChangedEvent);
	}
	return FReply::Handled();
}

// Hair_TODO: rename into OnGenerageCards
FReply FGroomRenderingDetails::OnSaveCards(int32 DescIndex, FProperty* Property)
{
	if (DescIndex < GroomAsset->HairGroupsCards.Num())
	{
		GroomAsset->SaveProceduralCards(DescIndex);
	}
	return FReply::Handled();
}

FReply FGroomRenderingDetails::OnGenerateCardDataUsingPlugin(int32 GroupIndex)
{
	if (GroomAsset && GroomAsset->HairGroupsCards.IsValidIndex(GroupIndex))
	{
		TArray<IHairCardGenerator*> HairCardGeneratorPlugins = IModularFeatures::Get().GetModularFeatureImplementations<IHairCardGenerator>(IHairCardGenerator::ModularFeatureName);
		if (HairCardGeneratorPlugins.Num() > 0)
		{
			UE_CLOG(HairCardGeneratorPlugins.Num() > 1, LogGroomAssetDetails, Warning, TEXT("There are more than one available hair-card generator options. Defaulting to the first one found."));

			const FScopedTransaction Transaction(LOCTEXT("GenerateHairCardsTransaction", "Generate hair cards."));

			// Use a copy so we can only apply changes on success
			FHairGroupsCardsSourceDescription HairCardsCopy = GroomAsset->HairGroupsCards[GroupIndex];
			// Clear fields that are supposed to be set by the generation (in case it leaves any unset, and we don't cary over old settings)
			HairCardsCopy.Textures = FHairGroupCardsTextures();

			const bool bSuccess = HairCardGeneratorPlugins[0]->GenerateHairCardsForLOD(GroomAsset, HairCardsCopy);
			if (bSuccess)
			{
				GroomAsset->Modify();
				GroomAsset->HairGroupsCards[GroupIndex] = HairCardsCopy;
			}
		}
	}
	return FReply::Handled();
}

void FGroomRenderingDetails::AddLODSlot(TSharedRef<IPropertyHandle>& LODHandle, IDetailChildrenBuilder& ChildrenBuilder, int32 GroupIndex, int32 LODIndex)
{	
	ExpandStruct(LODHandle, ChildrenBuilder, GroupIndex, LODIndex, true);
}

void FGroomRenderingDetails::OnGenerateElementForLODs(TSharedRef<IPropertyHandle> StructProperty, int32 LODIndex, IDetailChildrenBuilder& ChildrenBuilder, IDetailLayoutBuilder* DetailLayout, int32 GroupIndex)
{
	const FSlateFontInfo DetailFontInfo = IDetailLayoutBuilder::GetDetailFont();
	FProperty* Property = StructProperty->GetProperty();

	const FLinearColor LODColorBlock = GetHairGroupDebugColor(GroupIndex) * 0.25f;
	const FLinearColor LODNameColor(FLinearColor::White);
	static const FSlateBrush* GenericBrush = FCoreStyle::Get().GetBrush("GenericWhiteBox");
	float OtherMargin = 2.0f;

	ChildrenBuilder.AddCustomRow(LOCTEXT("HairInfo_Separator", "Separator"))
	.WholeRowContent()
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(GenericBrush)
			.ColorAndOpacity(LODColorBlock)
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(OtherMargin)
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ColorAndOpacity(LODNameColor)
				.Text(FText::Format(LOCTEXT("LOD", "LOD {0}"), FText::AsNumber(LODIndex)))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
				.OnClicked(this, &FGroomRenderingDetails::OnRemoveLODClicked, GroupIndex, LODIndex, Property)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.Delete"))
				]
			]
			
		]
	];

	// Rename the array entry name by its group name and adds all its existing properties
	StructProperty->SetPropertyDisplayName(LOCTEXT("LODProperties", "LOD Properties"));
	ExpandStructForLOD(StructProperty, ChildrenBuilder, GroupIndex, LODIndex, true); ///
}

TSharedRef<SWidget> FGroomRenderingDetails::MakeGroupNameButtonCustomization(int32 GroupIndex, FProperty* Property)
{
	switch (PanelType)
	{
	case EMaterialPanelType::LODs:
	{
		return SNew(SButton)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.OnClicked(this, &FGroomRenderingDetails::OnAddLODClicked, GroupIndex, Property)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.PlusCircle"))
			];
	}
	break;
	case EMaterialPanelType::Cards:
	{
		return SNew(SButton)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.OnClicked(this, &FGroomRenderingDetails::OnRemoveGroupClicked, GroupIndex, Property)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.Delete"))
			];
	}
	break;
	case EMaterialPanelType::Meshes:
	{
		return SNew(SButton)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.OnClicked(this, &FGroomRenderingDetails::OnRemoveGroupClicked, GroupIndex, Property)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.Delete"))
			];
	}
	break;
	}

	return SNullWidget::NullWidget;
}

static FName GetGroupName(const UGroomAsset* GroomAsset, int32 GroupIndex)
{
	if (GroomAsset && GroupIndex >= 0 && GroupIndex < GroomAsset->HairGroupsInfo.Num())
	{
		return GroomAsset->HairGroupsInfo[GroupIndex].GroupName;
	}
	return NAME_None;
}

TSharedRef<SWidget> GetGroupNameWidget(const UGroomAsset* GroomAsset, int32 GroupIndex, const FLinearColor& GroupColor)
{
	FName GroupName = GetGroupName(GroomAsset, GroupIndex);
	if (GroupName != NAME_None)
	{
		return SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ColorAndOpacity(GroupColor)
			.Text(FText::Format(LOCTEXT("GroupWithName", "Group ID {0} - {1}"), FText::AsNumber(GroupIndex), FText::FromName(GroupName)));
	}
	else
	{
		return SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ColorAndOpacity(GroupColor)
			.Text(FText::Format(LOCTEXT("GroupWithoutName", "Group ID {0}"), FText::AsNumber(GroupIndex)));
	}
}

TSharedRef<SWidget> FGroomRenderingDetails::MakeGroupNameCustomization(int32 GroupIndex, const FLinearColor& GroupTextColor)
{
	switch (PanelType)
	{
	case EMaterialPanelType::Cards:
	{
		return SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ColorAndOpacity(GroupTextColor)
			.Text(FText::Format(LOCTEXT("Cards", "Cards {0} "), FText::AsNumber(GroupIndex)));
	}
	break;
	case EMaterialPanelType::Meshes:
	{
		return SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ColorAndOpacity(GroupTextColor)
			.Text(FText::Format(LOCTEXT("Meshes", "Meshes {0} "), FText::AsNumber(GroupIndex)));
	}
	break;
	default:
	{
		return GetGroupNameWidget(GroomAsset, GroupIndex, GroupTextColor);
	}
	break;
	}

	return SNullWidget::NullWidget;
}

// Hair group custom display
void FGroomRenderingDetails::OnGenerateElementForHairGroup(TSharedRef<IPropertyHandle> StructProperty, int32 GroupIndex, IDetailChildrenBuilder& ChildrenBuilder, IDetailLayoutBuilder* DetailLayout)
{
	const FSlateFontInfo DetailFontInfo = IDetailLayoutBuilder::GetDetailFont();
	const FLinearColor GroupColorBlock = GetHairGroupDebugColor(GroupIndex) * 0.75f;
	const FLinearColor GroupNameColor(FLinearColor::White);

	FProperty* Property = StructProperty->GetProperty();

	static const FSlateBrush* GenericBrush = FCoreStyle::Get().GetBrush("GenericWhiteBox");

	float OtherMargin = 2.0f;
	float RightMargin = 2.0f;
	if (PanelType != EMaterialPanelType::LODs && PanelType != EMaterialPanelType::Cards && PanelType != EMaterialPanelType::Meshes)
	{
		RightMargin = 10.0f;
	}

	ChildrenBuilder.AddCustomRow(LOCTEXT("HairInfo_Separator", "Separator"))
	.WholeRowContent()
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(GenericBrush)
			.ColorAndOpacity(GroupColorBlock)
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(OtherMargin, OtherMargin, RightMargin, OtherMargin)
			[
				MakeGroupNameCustomization(GroupIndex, GroupNameColor)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				MakeGroupNameButtonCustomization(GroupIndex, Property)
			]
			
		]
	];

	if (GroomAsset != nullptr && GroupIndex>=0 && GroupIndex < GroomAsset->HairGroupsInfo.Num() && (PanelType == EMaterialPanelType::Strands || PanelType == EMaterialPanelType::Interpolation))
	{
		ChildrenBuilder.AddCustomRow(LOCTEXT("HairStrandsInfo_Array", "HairStrandsInfo"))
		.ValueContent()
		.HAlign(HAlign_Fill)
		[
			MakeHairStrandsInfoGrid(DetailFontInfo, GroomAsset->HairGroupsInfo[GroupIndex], GroomAsset->HairGroupsData[GroupIndex].Strands.BulkData.MaxRadius)
		];
	}

	if (GroomAsset != nullptr && GroupIndex >= 0 && GroupIndex < GroomAsset->HairGroupsCards.Num() && (PanelType == EMaterialPanelType::Cards))
	{
		ChildrenBuilder.AddCustomRow(LOCTEXT("HairCardsInfo_Array", "HairCardsInfo"))
		.ValueContent()
		.HAlign(HAlign_Fill)
		[
			MakeHairCardsInfoGrid(DetailFontInfo, GroomAsset->HairGroupsCards[GroupIndex].CardsInfo)
		];
	}

	// Display a material picker for strands/cards/meshes panel. This material picker allows to select material among the one valid for this current asset, i.e., 
	// materials which have been added by the user within the material panel
	if (PanelType == EMaterialPanelType::Strands || PanelType == EMaterialPanelType::Cards || PanelType == EMaterialPanelType::Meshes)
	{
		FResetToDefaultOverride ResetToDefaultOverride = FResetToDefaultOverride::Create(
			FIsResetToDefaultVisible::CreateSP(this, &FGroomRenderingDetails::GetReplaceVisibility),
			FResetToDefaultHandler::CreateSP(this, &FGroomRenderingDetails::OnResetToBaseClicked)
		);

		ChildrenBuilder.AddCustomRow(LOCTEXT("HairGroup_Material", "Material"))
		.OverrideResetToDefault(ResetToDefaultOverride)
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("HairRenderingGroup_Label_Material", "Material"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.IsEnabled(this, &FGroomRenderingDetails::IsStrandsMaterialPickerEnabled, GroupIndex)
		]
		.ValueContent()
		.MinDesiredWidth(250.f)
		.MaxDesiredWidth(0.0f) // no maximum
		[
			OnGenerateStrandsMaterialPicker(GroupIndex, DetailLayout)
		];
	}

	// Rename the array entry name by its group name and adds all its existing properties
	StructProperty->SetPropertyDisplayName(LOCTEXT("GroupProperties", "Properties"));

	uint32 ChildrenCount = 0;
	StructProperty->GetNumChildren(ChildrenCount);
	for (uint32 ChildIt = 0; ChildIt < ChildrenCount; ++ChildIt)
	{
		TSharedPtr<IPropertyHandle> ChildHandle = StructProperty->GetChildHandle(ChildIt);
		FName PropertyName = ChildHandle->GetProperty()->GetFName();

		switch (PanelType)
		{
		case EMaterialPanelType::Strands:
		{
			if (PropertyName == GET_MEMBER_NAME_CHECKED(FHairGroupsRendering, GeometrySettings) ||
				PropertyName == GET_MEMBER_NAME_CHECKED(FHairGroupsRendering, ShadowSettings) ||
				PropertyName == GET_MEMBER_NAME_CHECKED(FHairGroupsRendering, AdvancedSettings))
			{
				ExpandStruct(ChildHandle, ChildrenBuilder, GroupIndex, -1, true);
			}
			else 
			{
				ChildrenBuilder.AddProperty(ChildHandle.ToSharedRef());
			}
		}
		break;
		case EMaterialPanelType::Cards:
		{
			if (PropertyName == GET_MEMBER_NAME_CHECKED(FHairGroupsCardsSourceDescription, CardsInfo))
			{
				// Not node display
			}
			else
			{
				IDetailPropertyRow& PropertyRow = AddPropertyWithCustomReset(ChildHandle, ChildrenBuilder, GroupIndex, -1);

				auto CustomizeMeshPropertyRow = [](IDetailPropertyRow& PropertyRow, bool bEnable)->FDetailWidgetRow&
				{
					TSharedPtr<SWidget> NameWidget;
					TSharedPtr<SWidget> ValueWidget;
					FDetailWidgetRow Row;
					PropertyRow.GetDefaultWidgets(NameWidget, ValueWidget, Row);

					ValueWidget->SetEnabled(bEnable);

					return PropertyRow.CustomWidget()
						.NameContent()
						[
							PropertyRow.GetPropertyHandle()->CreatePropertyNameWidget(
								LOCTEXT("HairCardsMeshProperty", "Mesh"),
								LOCTEXT("HairCardsMeshTooltop",  "")
							)
						]
						.ValueContent()
						[
							ValueWidget.ToSharedRef()
						];
				};

				TWeakObjectPtr<UGroomAsset> GroomAssetPtr = GroomAsset;
				auto ShouldShowProceduralProperties = [GroomAssetPtr, GroupIndex]()
				{
					return GroomAssetPtr.IsValid() && GroomAssetPtr->HairGroupsCards.IsValidIndex(GroupIndex) &&
						GroomAssetPtr->HairGroupsCards[GroupIndex].SourceType == EHairCardsSourceType::Procedural;
				};
				TAttribute<EVisibility> ProceduralPropertyVisibility = TAttribute<EVisibility>::CreateLambda([ShouldShowProceduralProperties]()
					{
						return ShouldShowProceduralProperties() ? EVisibility::Visible : EVisibility::Collapsed;
					}
				);
				
				if (PropertyName == GET_MEMBER_NAME_CHECKED(FHairGroupsCardsSourceDescription, ProceduralMesh))
				{
					PropertyRow.Visibility(ProceduralPropertyVisibility);
					CustomizeMeshPropertyRow(PropertyRow, /*bEnable =*/false);
				}
				else if (PropertyName == GET_MEMBER_NAME_CHECKED(FHairGroupsCardsSourceDescription, ProceduralSettings))
				{
					PropertyRow.Visibility(TAttribute<EVisibility>::CreateLambda([ShouldShowProceduralProperties]()
						{
							TArray<IHairCardGenerator*> HairCardGeneratorPlugins = IModularFeatures::Get().GetModularFeatureImplementations<IHairCardGenerator>(IHairCardGenerator::ModularFeatureName);
							return HairCardGeneratorPlugins.IsEmpty() && ShouldShowProceduralProperties() ? EVisibility::Visible : EVisibility::Collapsed;
						}
					));
				}
				else if (PropertyName == GET_MEMBER_NAME_CHECKED(FHairGroupsCardsSourceDescription, ImportedMesh))
				{
					TAttribute<EVisibility> NonProceduralPropertyVisibility = TAttribute<EVisibility>::CreateLambda([ShouldShowProceduralProperties]()
						{
							return ShouldShowProceduralProperties() ? EVisibility::Collapsed : EVisibility::Visible;
						}
					);
					PropertyRow.Visibility(NonProceduralPropertyVisibility);

					CustomizeMeshPropertyRow(PropertyRow, /*bEnable =*/true);
				}
				else if (PropertyName == GET_MEMBER_NAME_CHECKED(FHairGroupsCardsSourceDescription, SourceType))
				{
					TSharedPtr<SWidget> NameWidget;
					TSharedPtr<SWidget> ValueWidget;
					FDetailWidgetRow Row;
					PropertyRow.GetDefaultWidgets(NameWidget, ValueWidget, Row);

					TSharedRef<SHorizontalBox> ProceduralGenButtons = SNew(SHorizontalBox).Visibility(ProceduralPropertyVisibility);
					TArray<IHairCardGenerator*> HairCardGeneratorPlugins = IModularFeatures::Get().GetModularFeatureImplementations<IHairCardGenerator>(IHairCardGenerator::ModularFeatureName);
					if (HairCardGeneratorPlugins.Num() > 0)
					{
						FText RegenToolTipText = LOCTEXT("GenerateNewCardsTooltip", "Generate new card assets (meshes and textures) using the current procedural settings. NOTE: This will overwrite preexisting card assets for this LOD.");
						ProceduralGenButtons->AddSlot()
							.AutoWidth()
							[
								SNew(SButton)
									.VAlign(VAlign_Center)
									.HAlign(HAlign_Center)
									.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
									.ToolTipText(RegenToolTipText)
									.OnClicked(this, &FGroomRenderingDetails::OnGenerateCardDataUsingPlugin, GroupIndex)
									[
										SNew(SImage)
											// @TODO: Need a specialized icon for this?
											.Image(FAppStyle::GetBrush("ContentBrowser.AssetActions.ReimportAsset"))
											.RenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(90.0f))))
											.RenderTransformPivot(FVector2D(0.5f, 0.5f))
									]
							];
					}
					else
					{
						FText ToolTipTextForGeneration(FText::FromString(TEXT("Generate procedural cards data (meshes and textures) based on current procedural settings. Cards generation needs to run prior to the (re)loading of the cards data.")));
						FText ToolTipTextForReloading(FText::FromString(TEXT("(Re)Load generated cards data (meshes and textures) into the groom asset. The data need to be generated with the save/generated button prior to reloading.")));

						ProceduralGenButtons->AddSlot()
							.AutoWidth()
							[
								SNew(SButton)
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Center)
								.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
								.ToolTipText(ToolTipTextForGeneration)
								.OnClicked(this, &FGroomRenderingDetails::OnSaveCards, GroupIndex, Property)
								[
									SNew(SImage)
									.Image(FAppStyle::GetBrush("AssetEditor.SaveAsset"))
								]
							];

						ProceduralGenButtons->AddSlot()
							.AutoWidth()
							[
								SNew(SButton)
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Center)
								.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
								.ToolTipText(ToolTipTextForReloading)
								.OnClicked(this, &FGroomRenderingDetails::OnRefreshCards, GroupIndex, Property)
								[
									SNew(SImage)
									.Image(FAppStyle::GetBrush("Icons.Refresh"))
								]
							];
					}

					PropertyRow.CustomWidget()
						.NameContent()
						[
							NameWidget.ToSharedRef()
						]
						.ValueContent()
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								ValueWidget.ToSharedRef()
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								ProceduralGenButtons
							]
						];
				}
			}
		}
		break;
		case EMaterialPanelType::Meshes:
		{
			AddPropertyWithCustomReset(ChildHandle, ChildrenBuilder, GroupIndex, -1);
		}
		break;
		case EMaterialPanelType::Interpolation:
		{
			if (PropertyName == GET_MEMBER_NAME_CHECKED(FHairGroupsInterpolation, DecimationSettings) ||
				PropertyName == GET_MEMBER_NAME_CHECKED(FHairGroupsInterpolation, InterpolationSettings))
			{
				ExpandStruct(ChildHandle, ChildrenBuilder, GroupIndex, -1, true);
			}
			else
			{
				AddPropertyWithCustomReset(ChildHandle, ChildrenBuilder, GroupIndex, -1);
			}
		}
		break;
		case EMaterialPanelType::LODs:
		{
			if (PropertyName == GET_MEMBER_NAME_CHECKED(FHairGroupsLOD, LODs))
			{
				// Add a custom builder for each LOD arrays within each group. This way we can customize this 'nested/inner' array
				TSharedRef<FDetailArrayBuilder> PropertyBuilder = MakeShareable(new FDetailArrayBuilder(ChildHandle.ToSharedRef(), false, false, false));
				PropertyBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FGroomRenderingDetails::OnGenerateElementForLODs, DetailLayout, GroupIndex));
				PropertyBuilder->SetDisplayName(FText::FromString(TEXT("LODs")));
				ChildrenBuilder.AddCustomBuilder(PropertyBuilder);
			}
			else
			{
				AddPropertyWithCustomReset(ChildHandle, ChildrenBuilder, GroupIndex, -1);
			}
		}
		break;
		case EMaterialPanelType::Physics:
		{
			AddPropertyWithCustomReset(ChildHandle, ChildrenBuilder, GroupIndex, -1);
		}
		break;
		case EMaterialPanelType::Bindings:
		{
			AddPropertyWithCustomReset(ChildHandle, ChildrenBuilder, GroupIndex, -1);
		}
		break;
		default:
		{
			ChildrenBuilder.AddProperty(ChildHandle.ToSharedRef());
		}
		break;
		}

	}
}

// Hair binding display
void FGroomRenderingDetails::OnGenerateElementForBindingAsset(TSharedRef<IPropertyHandle> StructProperty, int32 BindingIndex, IDetailChildrenBuilder& ChildrenBuilder, IDetailLayoutBuilder* DetailLayout)
{
	FProperty* Property = StructProperty->GetProperty();
	ChildrenBuilder.AddProperty(StructProperty);

	const FLinearColor Color = BindingIndex == Toolkit->GetActiveBindingIndex() ? FLinearColor::White : FLinearColor(0.1f, 0.1f, 0.1f, 1.f);

	ChildrenBuilder.AddCustomRow(FText::FromString(TEXT("Preview")))
	.ValueContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.OnClicked(this, &FGroomRenderingDetails::OnSelectBinding, BindingIndex, Property)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.Visible"))
				.ColorAndOpacity(Color)
			]
		]
	];
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FGroomRenderingDetails::OnSetObject(const FAssetData& AssetData)
{

}

FString FGroomRenderingDetails::OnGetObjectPath(int32 GroupIndex) const
{
	if (!GroomAsset || GroupIndex < 0)
		return FString();

	int32 MaterialIndex = INDEX_NONE;
	check(GroomAsset);
	switch (PanelType)
	{
	case EMaterialPanelType::Cards:
	{
		if (GroupIndex < GroomAsset->HairGroupsCards.Num())
		{
			MaterialIndex = GroomAsset->GetMaterialIndex(GroomAsset->HairGroupsCards[GroupIndex].MaterialSlotName);
		}
	}
	break;
	case EMaterialPanelType::Meshes:
	{
		if (GroupIndex < GroomAsset->HairGroupsMeshes.Num())
		{
			MaterialIndex = GroomAsset->GetMaterialIndex(GroomAsset->HairGroupsMeshes[GroupIndex].MaterialSlotName);
		}
	}
	break;
	case EMaterialPanelType::Strands:
	{
		if (GroupIndex < GroomAsset->HairGroupsRendering.Num())
		{
			MaterialIndex = GroomAsset->GetMaterialIndex(GroomAsset->HairGroupsRendering[GroupIndex].MaterialSlotName);
		}
	}
	break;
	}

	if (MaterialIndex == INDEX_NONE)
		return FString();
	
	return GroomAsset->HairGroupsMaterials[MaterialIndex].Material->GetPathName();
}

/**
 * Called to get the visibility of the replace button
 */
bool FGroomRenderingDetails::GetReplaceVisibility(TSharedPtr<IPropertyHandle> PropertyHandle) const
{
	return false;
}

/**
 * Called when reset to base is clicked
 */
void FGroomRenderingDetails::OnResetToBaseClicked(TSharedPtr<IPropertyHandle> PropertyHandle)
{

}

TSharedRef<SWidget> FGroomRenderingDetails::CreateMaterialSwatch( const TSharedPtr<FAssetThumbnailPool>& ThumbnailPool/*, const TArray<FAssetData>& OwnerAssetDataArray*/, int32 GroupIndex)
{
	FIntPoint ThumbnailSize(64, 64);

	const bool bDisplayCompactSize = false;
	return
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding( 0.0f )
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Fill)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew( SObjectPropertyEntryBox )
					.EnableContentPicker(false)
					.DisplayUseSelected(false)
					.ObjectPath(this, &FGroomRenderingDetails::OnGetObjectPath, GroupIndex)
					.AllowedClass(UMaterialInterface::StaticClass())
					.OnObjectChanged(this, &FGroomRenderingDetails::OnSetObject)
					.ThumbnailPool(ThumbnailPool)
					.DisplayCompactSize(bDisplayCompactSize)
					//.OwnerAssetDataArray(OwnerAssetDataArray)
					.CustomContentSlot()
					[
						SNew(SComboButton)
						.IsEnabled(this, &FGroomRenderingDetails::IsStrandsMaterialPickerEnabled, GroupIndex)
						.OnGetMenuContent(this, &FGroomRenderingDetails::OnGenerateStrandsMaterialMenuPicker, GroupIndex)
						.VAlign(VAlign_Center)
						.ContentPadding(2)
						.ButtonContent()
						[
							SNew(STextBlock)
							.Font(IDetailLayoutBuilder::GetDetailFont())
							.Text(this, &FGroomRenderingDetails::GetStrandsMaterialName, GroupIndex)
						]
					]
				]
			]
		];
}

#undef LOCTEXT_NAMESPACE