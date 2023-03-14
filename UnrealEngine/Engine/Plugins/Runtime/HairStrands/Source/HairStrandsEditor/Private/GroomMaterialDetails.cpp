// Copyright Epic Games, Inc. All Rights Reserved.

#include "GroomMaterialDetails.h"
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

#define LOCTEXT_NAMESPACE "GroomMaterialDetails"

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// FGroomMaterialDetails

FGroomMaterialDetails::FGroomMaterialDetails(IGroomCustomAssetEditorToolkit* InToolkit)
: GroomDetailLayout(nullptr)
{
	if (InToolkit)
	{
		GroomAsset = InToolkit->GetCustomAsset();// InGroomAsset;
	}
	bDeleteWarningConsumed = false;
}

FGroomMaterialDetails::~FGroomMaterialDetails()
{

}

TSharedRef<IDetailCustomization> FGroomMaterialDetails::MakeInstance(IGroomCustomAssetEditorToolkit* InToolkit)
{
	return MakeShareable(new FGroomMaterialDetails(InToolkit));
}

void FGroomMaterialDetails::OnCopyMaterialList()
{
}

void FGroomMaterialDetails::OnPasteMaterialList()
{
}

bool FGroomMaterialDetails::OnCanCopyMaterialList() const
{
	return false;
}

void FGroomMaterialDetails::AddMaterials(IDetailLayoutBuilder& DetailLayout)
{
	if (!GroomAsset)
	{
		return;
	}

	// Create material list panel to let users control the materials array
	{
		FString MaterialCategoryName = FString(TEXT("Material Slots"));
		IDetailCategoryBuilder& MaterialCategory = DetailLayout.EditCategory(*MaterialCategoryName, FText::GetEmpty(), ECategoryPriority::Important);
		MaterialCategory.AddCustomRow(LOCTEXT("AddLODLevelCategories_MaterialArrayOperationAdd", "Materials Operation Add Material Slot"))
			.CopyAction(FUIAction(FExecuteAction::CreateSP(this, &FGroomMaterialDetails::OnCopyMaterialList), FCanExecuteAction::CreateSP(this, &FGroomMaterialDetails::OnCanCopyMaterialList)))
			.PasteAction(FUIAction(FExecuteAction::CreateSP(this, &FGroomMaterialDetails::OnPasteMaterialList)))
			.NameContent()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(LOCTEXT("AddLODLevelCategories_MaterialArrayOperations", "Material Slots"))
			]
		.ValueContent()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(this, &FGroomMaterialDetails::GetMaterialArrayText)
			]
		+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(2.0f, 1.0f)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.Text(LOCTEXT("AddLODLevelCategories_MaterialArrayOpAdd", "Add Material Slot"))
			.ToolTipText(LOCTEXT("AddLODLevelCategories_MaterialArrayOpAdd_Tooltip", "Add Material Slot at the end of the Material slot array. Those Material slots can be used to override a LODs section, (not the base LOD)"))
			.ContentPadding(4.0f)
			.ForegroundColor(FSlateColor::UseForeground())
			.OnClicked(this, &FGroomMaterialDetails::AddMaterialSlot)
			.IsEnabled(true)
			.IsFocusable(false)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.PlusCircle"))
			.ColorAndOpacity(FSlateColor::UseForeground())
			]
			]
			]
			];
		{
			FMaterialListDelegates MaterialListDelegates;

			MaterialListDelegates.OnGetMaterials.BindSP(this, &FGroomMaterialDetails::OnGetMaterialsForArray, 0);
			MaterialListDelegates.OnMaterialChanged.BindSP(this, &FGroomMaterialDetails::OnMaterialArrayChanged, 0);
			MaterialListDelegates.OnGenerateCustomNameWidgets.BindSP(this, &FGroomMaterialDetails::OnGenerateCustomNameWidgetsForMaterialArray);
			MaterialListDelegates.OnGenerateCustomMaterialWidgets.BindSP(this, &FGroomMaterialDetails::OnGenerateCustomMaterialWidgetsForMaterialArray, 0);
			MaterialListDelegates.OnMaterialListDirty.BindSP(this, &FGroomMaterialDetails::OnMaterialListDirty);

			//Pass an empty material list owner (owner can be use by the asset picker filter. In this case we do not need it)
			TArray<FAssetData> MaterialListOwner;
			MaterialListOwner.Add(GroomAsset);
			MaterialCategory.AddCustomBuilder(MakeShareable(new FMaterialList(MaterialCategory.GetParentLayout(), MaterialListDelegates, MaterialListOwner, false, true)));
		}
	}
}

void FGroomMaterialDetails::ApplyChanges()
{
	GroomDetailLayout->ForceRefreshDetails();
}

FText FGroomMaterialDetails::GetMaterialSlotNameText(int32 MaterialIndex) const
{	
	if (IsMaterialValid(MaterialIndex))
	{
		return FText::FromName(GroomAsset->HairGroupsMaterials[MaterialIndex].SlotName);
	}

	return LOCTEXT("HairMaterial_InvalidIndex", "Invalid Material Index");
}

void FGroomMaterialDetails::CustomizeDetails( IDetailLayoutBuilder& DetailLayout )
{
	const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = DetailLayout.GetSelectedObjects();
	check(SelectedObjects.Num()<=1); // The OnGenerateCustomWidgets delegate will not be useful if we try to process more than one object.

	GroomAsset = SelectedObjects.Num() > 0 ? Cast<UGroomAsset>(SelectedObjects[0].Get()) : nullptr;

	// Hide all properties
	{
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsInterpolation), UGroomAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsRendering), UGroomAsset::StaticClass()));
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
	}

	GroomDetailLayout = &DetailLayout;
	AddMaterials(DetailLayout);
}

void FGroomMaterialDetails::OnGetMaterialsForArray(class IMaterialListBuilder& OutMaterials, int32 LODIndex)
{
	if (!GroomAsset)
		return;

	for (int32 MaterialIndex = 0; MaterialIndex < GroomAsset->HairGroupsMaterials.Num(); ++MaterialIndex)
	{
		OutMaterials.AddMaterial(MaterialIndex, GroomAsset->HairGroupsMaterials[MaterialIndex].Material, true);
	}
}

void FGroomMaterialDetails::OnMaterialArrayChanged(UMaterialInterface* NewMaterial, UMaterialInterface* PrevMaterial, int32 SlotIndex, bool bReplaceAll, int32 LODIndex)
{
	if (!GroomAsset)
		return;

	// Whether or not we made a transaction and need to end it
	bool bMadeTransaction = false;

	FProperty* MaterialProperty = FindFProperty<FProperty>(UGroomAsset::StaticClass(), "HairGroupsMaterials");
	check(MaterialProperty);
	GroomAsset->PreEditChange(MaterialProperty);
	check(GroomAsset->HairGroupsMaterials.Num() > SlotIndex)

	if (NewMaterial != PrevMaterial)
	{
		GEditor->BeginTransaction(LOCTEXT("GroomEditorMaterialChanged", "Groom editor: material changed"));
		bMadeTransaction = true;
		GroomAsset->Modify();
		GroomAsset->HairGroupsMaterials[SlotIndex].Material = NewMaterial;

		// Add a default name to the material slot if this slot was manually add and there is no name yet
		if (NewMaterial != nullptr && GroomAsset->HairGroupsMaterials[SlotIndex].SlotName == NAME_None)
		{
			if (GroomAsset->HairGroupsMaterials[SlotIndex].SlotName == NAME_None)
			{					
				GroomAsset->HairGroupsMaterials[SlotIndex].SlotName = NewMaterial->GetFName();
			}

			//Ensure the imported material slot name is unique
			if (GroomAsset->HairGroupsMaterials[SlotIndex].SlotName == NAME_None)
			{
				UGroomAsset* LocalGroomAsset = GroomAsset;
				auto IsMaterialNameUnique = [&LocalGroomAsset, SlotIndex](const FName TestName)
				{
					for (int32 MaterialIndex = 0; MaterialIndex < LocalGroomAsset->HairGroupsMaterials.Num(); ++MaterialIndex)
					{
						if (MaterialIndex == SlotIndex)
						{
							continue;
						}
						if (LocalGroomAsset->HairGroupsMaterials[MaterialIndex].SlotName == TestName)
						{
							return false;
						}
					}
					return true;
				};
				int32 MatchNameCounter = 0;
				//Make sure the name is unique for imported material slot name
				bool bUniqueName = false;
				FString MaterialSlotName = NewMaterial->GetName();
				while (!bUniqueName)
				{
					bUniqueName = true;
					if (!IsMaterialNameUnique(FName(*MaterialSlotName)))
					{
						bUniqueName = false;
						MatchNameCounter++;
						MaterialSlotName = NewMaterial->GetName() + TEXT("_") + FString::FromInt(MatchNameCounter);
					}
				}
				GroomAsset->HairGroupsMaterials[SlotIndex].SlotName = FName(*MaterialSlotName);
			}
		}
	}

	FPropertyChangedEvent PropertyChangedEvent(MaterialProperty);
	GroomAsset->PostEditChangeProperty(PropertyChangedEvent);

	if (bMadeTransaction)
	{
		// End the transation if we created one
		GEditor->EndTransaction();
		// Redraw viewports to reflect the material changes 
		GUnrealEd->RedrawLevelEditingViewports();
	}
}

FReply FGroomMaterialDetails::AddMaterialSlot()
{
	if (!GroomAsset)
	{
		return FReply::Handled();
	}

	FScopedTransaction Transaction(LOCTEXT("PersonaAddMaterialSlotTransaction", "Persona editor: Add material slot"));
	GroomAsset->Modify();
	FHairGroupsMaterial NewMaterial;
	NewMaterial.SlotName = FName(TEXT("Material"));

	// Build a unique name
	FName SlotName = NewMaterial.SlotName;
	uint32 UniqueId = 0;
	bool bHasUniqueName = true;
	do
	{
		bHasUniqueName = true;
		for (const FHairGroupsMaterial& Group : GroomAsset->HairGroupsMaterials)
		{
			if (Group.SlotName == SlotName)
			{
				bHasUniqueName = false;				
				FString NewSlotName = NewMaterial.SlotName.ToString() + FString::FromInt(++UniqueId);
				SlotName = FName(*NewSlotName);
				break;
			}
		}
	} while (!bHasUniqueName);

	// Add new material
	NewMaterial.SlotName = SlotName;
	GroomAsset->HairGroupsMaterials.Add(NewMaterial);
	GroomAsset->PostEditChange();

	return FReply::Handled();
}

FText FGroomMaterialDetails::GetMaterialArrayText() const
{
	FString MaterialArrayText = TEXT(" Material Slots");
	int32 SlotNumber = 0;
	if (GroomAsset)
	{
		SlotNumber = GroomAsset->HairGroupsMaterials.Num();
	}
	MaterialArrayText = FString::FromInt(SlotNumber) + MaterialArrayText;
	return FText::FromString(MaterialArrayText);
}

FText FGroomMaterialDetails::GetMaterialNameText(int32 MaterialIndex) const
{
	if (IsMaterialValid(MaterialIndex))
	{
		return FText::FromName(GroomAsset->HairGroupsMaterials[MaterialIndex].SlotName);
	}
	return FText::FromName(NAME_None);
}

void FGroomMaterialDetails::OnMaterialNameCommitted(const FText& InValue, ETextCommit::Type CommitType, int32 MaterialIndex)
{
	FName NewSlotName = FName(*(InValue.ToString()));
	if (IsMaterialValid(MaterialIndex) && NewSlotName != GroomAsset->HairGroupsMaterials[MaterialIndex].SlotName)
	{
		FScopedTransaction ScopeTransaction(LOCTEXT("PersonaMaterialSlotNameChanged", "Persona editor: Material slot name change"));

		FProperty* ChangedProperty = FindFProperty<FProperty>(UGroomAsset::StaticClass(), "HairGroupsMaterials");
		check(ChangedProperty);
		GroomAsset->PreEditChange(ChangedProperty);

		// Rename group which were using the old slot name
		FName PreviousSlotName = GroomAsset->HairGroupsMaterials[MaterialIndex].SlotName;
		for (FHairGroupsRendering& Group : GroomAsset->HairGroupsRendering)
		{
			if (PreviousSlotName == Group.MaterialSlotName)
			{
				Group.MaterialSlotName = NewSlotName;
			}
		}
		for (FHairGroupsCardsSourceDescription& Group : GroomAsset->HairGroupsCards)
		{
			if (PreviousSlotName == Group.MaterialSlotName)
			{
				Group.MaterialSlotName = NewSlotName;
			}
		}
		for (FHairGroupsMeshesSourceDescription& Group : GroomAsset->HairGroupsMeshes)
		{
			if (PreviousSlotName == Group.MaterialSlotName)
			{
				Group.MaterialSlotName = NewSlotName;
			}
		}

		GroomAsset->HairGroupsMaterials[MaterialIndex].SlotName = NewSlotName;

		FPropertyChangedEvent PropertyUpdateStruct(ChangedProperty);
		GroomAsset->PostEditChangeProperty(PropertyUpdateStruct);
	}
}

TSharedRef<SWidget> FGroomMaterialDetails::OnGenerateCustomNameWidgetsForMaterialArray(UMaterialInterface* Material, int32 MaterialIndex)
{
	return SNew(SVerticalBox);
}

TSharedRef<SWidget> FGroomMaterialDetails::OnGenerateCustomMaterialWidgetsForMaterialArray(UMaterialInterface* Material, int32 MaterialIndex, int32 LODIndex)
{
	bool bMaterialIsUsed = GroomAsset && GroomAsset->IsMaterialUsed(MaterialIndex);

	return
		SNew(SMaterialSlotWidget, MaterialIndex, bMaterialIsUsed)
		.MaterialName(this, &FGroomMaterialDetails::GetMaterialNameText, MaterialIndex)
		.OnMaterialNameCommitted(this, &FGroomMaterialDetails::OnMaterialNameCommitted, MaterialIndex)
		.CanDeleteMaterialSlot(this, &FGroomMaterialDetails::CanDeleteMaterialSlot, MaterialIndex)
		.OnDeleteMaterialSlot(this, &FGroomMaterialDetails::OnDeleteMaterialSlot, MaterialIndex);
}

bool FGroomMaterialDetails::IsMaterialValid(int32 MaterialIndex) const
{
	return GroomAsset && MaterialIndex >= 0 && MaterialIndex < GroomAsset->HairGroupsMaterials.Num();
}

bool FGroomMaterialDetails::CanDeleteMaterialSlot(int32 MaterialIndex) const
{
	if (!GroomAsset)
	{
		return false;
	}

	return !GroomAsset->IsMaterialUsed(MaterialIndex);
}
	
void FGroomMaterialDetails::OnDeleteMaterialSlot(int32 MaterialIndex)
{
	if (!GroomAsset || !CanDeleteMaterialSlot(MaterialIndex))
	{
		return;
	}

	if (!bDeleteWarningConsumed)
	{
		EAppReturnType::Type Answer = FMessageDialog::Open(EAppMsgType::OkCancel, LOCTEXT("FPersonaMeshDetails_DeleteMaterialSlot", "WARNING - Deleting a material slot can break the game play blueprint or the game play code. All indexes after the delete slot will change"));
		if (Answer == EAppReturnType::Cancel)
		{
			return;
		}
		bDeleteWarningConsumed = true;
	}
	GroomAsset->HairGroupsMaterials.RemoveAt(MaterialIndex);

	FScopedTransaction Transaction(LOCTEXT("PersonaOnDeleteMaterialSlotTransaction", "Persona editor: Delete material slot"));
	GroomAsset->Modify();
}

bool FGroomMaterialDetails::OnMaterialListDirty()
{
	bool ForceMaterialListRefresh = false;
	return ForceMaterialListRefresh;
}

TSharedRef<SWidget> FGroomMaterialDetails::OnGenerateCustomNameWidgetsForSection(int32 LodIndex, int32 SectionIndex)
{
	bool IsSectionChunked = false;
	TSharedRef<SVerticalBox> SectionWidget = SNew(SVerticalBox);
	SectionWidget->AddSlot()
		.AutoHeight()
		.Padding(0, 2, 0, 0);
	return SectionWidget;
}

TSharedRef<SWidget> FGroomMaterialDetails::OnGenerateCustomSectionWidgetsForSection(int32 LODIndex, int32 SectionIndex)
{
	TSharedRef<SVerticalBox> SectionWidget = SNew(SVerticalBox);
	SectionWidget->AddSlot()
	.AutoHeight()
	.Padding(0, 2, 0, 0);
	return SectionWidget;
}

EVisibility FGroomMaterialDetails::ShowEnabledSectionDetail(int32 LodIndex, int32 SectionIndex) const
{
	return EVisibility::All;
}

EVisibility FGroomMaterialDetails::ShowDisabledSectionDetail(int32 LodIndex, int32 SectionIndex) const
{
	return EVisibility::All;
}

void FGroomMaterialDetails::OnMaterialSelectedChanged(ECheckBoxState NewState, int32 MaterialIndex)
{

}

ECheckBoxState FGroomMaterialDetails::IsIsolateMaterialEnabled(int32 MaterialIndex) const
{
	ECheckBoxState State = ECheckBoxState::Unchecked;
	return State;
}

void FGroomMaterialDetails::OnMaterialIsolatedChanged(ECheckBoxState NewState, int32 MaterialIndex)
{

}

ECheckBoxState FGroomMaterialDetails::IsSectionSelected(int32 SectionIndex) const
{
	ECheckBoxState State = ECheckBoxState::Unchecked;
	return State;
}

void FGroomMaterialDetails::OnSectionSelectedChanged(ECheckBoxState NewState, int32 SectionIndex)
{

}

ECheckBoxState FGroomMaterialDetails::IsIsolateSectionEnabled(int32 SectionIndex) const
{
	ECheckBoxState State = ECheckBoxState::Unchecked;
	return State;
}

void FGroomMaterialDetails::OnSectionIsolatedChanged(ECheckBoxState NewState, int32 SectionIndex)
{

}


#undef LOCTEXT_NAMESPACE