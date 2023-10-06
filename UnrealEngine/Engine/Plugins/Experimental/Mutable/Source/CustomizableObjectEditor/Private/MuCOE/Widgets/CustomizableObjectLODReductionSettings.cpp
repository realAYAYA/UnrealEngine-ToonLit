// Copyright Epic Games, Inc. All Rights Reserved.

#include "CustomizableObjectLODReductionSettings.h"

#include "BoneSelectionWidget.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Engine/SkeletalMesh.h"
#include "MuCOE/CustomizableObjectGraph.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObject.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "CustomizableObjectDetails"


TSharedRef<IPropertyTypeCustomization> FCustomizableObjectLODReductionSettings::MakeInstance()
{
	return MakeShareable(new FCustomizableObjectLODReductionSettings());
}


void FCustomizableObjectLODReductionSettings::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	constexpr bool bRecurse = false;

	BoneNameProperty = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FBoneToRemove, BoneName), bRecurse);
	IncludeBoneProperty = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FBoneToRemove, bOnlyRemoveChildren), bRecurse);

	TSharedPtr<IPropertyHandle> Parent = StructPropertyHandle->GetParentHandle();
	if (Parent)
	{
		ParentArrayProperty = Parent->AsArray();
	}

	if (BoneNameProperty.IsValid() && BoneNameProperty->IsValidHandle() 
		&& IncludeBoneProperty.IsValid() && IncludeBoneProperty->IsValidHandle())
	{
		ObjectNode = GetObjectNode();

		bool bOnlyRemoveChildrenProperty = false;
		IncludeBoneProperty->GetValue(bOnlyRemoveChildrenProperty);

		HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MaxDesiredWidth(0.0f)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 1.0f, 0.0, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(0.0f,5.0f,5.0f,0.0f)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("BoneSelector_Text", "Bone:"))
					.ToolTipText(LOCTEXT("BoneSelector_TooltipText", "Select the bone to remove. Bone children will be removed too."))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(SelectionWidget, SBoneSelectionWidget)
					.ToolTipText(StructPropertyHandle->GetToolTipText())
					.OnBoneSelectionChanged(this, &FCustomizableObjectLODReductionSettings::OnBoneSelectionChanged)
					.OnGetSelectedBone(this, &FCustomizableObjectLODReductionSettings::GetSelectedBone)
					.OnGetReferenceSkeleton(this, &FCustomizableObjectLODReductionSettings::GetReferenceSkeleton)
				]
			]
			
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f,5.0f,0.0,3.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f,2.5f,5.0f,0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("BoneCheckBox_Text", "Remove Only Children:"))
					.ToolTipText(LOCTEXT("BoneCheckBox_TooltipText","If true, only the bone children of the selected bone will be removed."))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SCheckBox)
					.OnCheckStateChanged(this, &FCustomizableObjectLODReductionSettings::OnBoneCheckBoxChanged)
					.IsChecked(bOnlyRemoveChildrenProperty ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
				]
			]
		];

		DefaultColor = SelectionWidget->GetColorAndOpacity();
	}
}


UCustomizableObjectNodeObject* FCustomizableObjectLODReductionSettings::GetObjectNode()
{
	if (!BoneNameProperty->IsValidHandle())
	{
		return nullptr;
	}

	TArray<UObject*> Objects;
	BoneNameProperty->GetOuterObjects(Objects);

	if (Objects.Num())
	{
		return Cast<UCustomizableObjectNodeObject>(Objects[0]);
	}

	return nullptr;
}


FName FCustomizableObjectLODReductionSettings::GetSelectedBone(bool& bMultipleValues) const
{
	if (BoneNameProperty->IsValidHandle())
	{
		FString OutText;
	
		FPropertyAccess::Result Result = BoneNameProperty->GetValueAsFormattedString(OutText);
	
		return FName(*OutText);
	}

	return FName();
}


const struct FReferenceSkeleton& FCustomizableObjectLODReductionSettings::GetReferenceSkeleton() const
{
	static FReferenceSkeleton DummySkeleton;
	USkeletalMesh* SkeletalMesh = nullptr;

	if (ObjectNode)
	{
		if (const UCustomizableObject* CustomizableObject = Cast<UCustomizableObject>(ObjectNode->GetCustomizableObjectGraph()->GetOuter()))
		{
			if (CustomizableObject->ReferenceSkeletalMeshes.IsValidIndex(ObjectNode->CurrentComponent))
			{
				SkeletalMesh = CustomizableObject->ReferenceSkeletalMeshes[ObjectNode->CurrentComponent];
			}
		}
	}

	return SkeletalMesh ? SkeletalMesh->GetRefSkeleton() : DummySkeleton;
}


void FCustomizableObjectLODReductionSettings::OnBoneSelectionChanged(FName Name)
{
	if (!ParentArrayProperty)
	{
		BoneNameProperty->SetValue(Name);
		IncludeBoneProperty->SetValue(false);
		return;
	}
	
	uint32 NumParentElems = 0;
	FPropertyAccess::Result Result = ParentArrayProperty->GetNumElements(NumParentElems);
	check(Result == FPropertyAccess::Result::Success);
	
	// Find if already set.
	bool BoneNameFound = false;
	for (uint32 ElementIndex = 0; ElementIndex < NumParentElems; ++ElementIndex)
	{
		TSharedPtr<IPropertyHandle> ElemProperty = ParentArrayProperty->GetElement(ElementIndex);
		check(ElemProperty);
	
		constexpr bool bRecurse = false;
		TSharedPtr<IPropertyHandle> ElemBoneNameProperty =
			ElemProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FBoneToRemove, BoneName), bRecurse);
	
		check(ElemBoneNameProperty);
	
		FName BoneNameValue;
		Result = ElemBoneNameProperty->GetValue(BoneNameValue);
	
		check(Result == FPropertyAccess::Result::Success);
	
		if (BoneNameValue == Name)
		{
			BoneNameFound = true;
			break;
		}
	}
	
	// TODO: This behavior does not seem to be correct, if the option is repeated the value is not set
	// only logging a warning to the console, which might not be open and could make dificult to see what 
	// is the problem. 
	BoneNameProperty->SetValue(Name);
	IncludeBoneProperty->SetValue(false);

	if (BoneNameFound)
	{
		const FLinearColor WarningColor = FLinearColor::Yellow;
		SelectionWidget->SetColorAndOpacity(WarningColor);
	}
	else
	{
		SelectionWidget->SetColorAndOpacity(DefaultColor);
	}
}

void FCustomizableObjectLODReductionSettings::OnBoneCheckBoxChanged(ECheckBoxState NewState)
{
	bool bValue = NewState == ECheckBoxState::Checked;

	if (IncludeBoneProperty->IsValidHandle())
	{
		IncludeBoneProperty->SetValue(bValue);
	}
}

#undef LOCTEXT_NAMESPACE