// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeMeshReshapeSelectionDetails.h"

#include "BoneSelectionWidget.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "DetailWidgetRow.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Text.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshMorph.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshReshape.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshReshapeCommon.h"
#include "MuCOE/Nodes/CustomizableObjectNodeSkeletalMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTable.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "ReferenceSkeleton.h"
#include "Templates/Casts.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"
#include "Widgets/DeclarativeSyntaxSupport.h"


#define LOCTEXT_NAMESPACE "CustomizableObjectDetails"


TSharedRef<IPropertyTypeCustomization> FMeshReshapeBonesReferenceCustomization::MakeInstance()
{
	return MakeShareable(new FMeshReshapeBonesReferenceCustomization());
}

void FMeshReshapeBonesReferenceCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	constexpr bool bRecurse = false;
	BoneNameProperty = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMeshReshapeBoneReference, BoneName), bRecurse);

	TSharedPtr<IPropertyHandle> Parent = StructPropertyHandle->GetParentHandle();
	if (Parent)
	{
		ParentArrayProperty = Parent->AsArray();
	}

	if (BoneNameProperty->IsValidHandle())
	{
		HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MaxDesiredWidth(0.0f)
		[
			SAssignNew(SelectionWidget, SBoneSelectionWidget)
			.ToolTipText(StructPropertyHandle->GetToolTipText())
			.OnBoneSelectionChanged(this, &FMeshReshapeBonesReferenceCustomization::OnBoneSelectionChanged)
			.OnGetSelectedBone(this, &FMeshReshapeBonesReferenceCustomization::GetSelectedBone)
			.OnGetReferenceSkeleton(this, &FMeshReshapeBonesReferenceCustomization::GetReferenceSkeleton)
		];

		DefaultColor = SelectionWidget->GetColorAndOpacity();
	}
}

USkeletalMesh* FMeshReshapeBonesReferenceCustomization::GetSkeletalMesh(TSharedPtr<IPropertyHandle> BoneNamePropertyHandle) const 
{
	if (!BoneNamePropertyHandle)
	{
		return nullptr;
	}

	TArray<UObject*> Objects;
	BoneNamePropertyHandle->GetOuterObjects(Objects);

	USkeletalMesh* TargetSkeletalMesh = nullptr;
	
	for (const UObject* Object : Objects)
	{
		UEdGraphPin* MeshPin = nullptr;

		if (const UCustomizableObjectNodeMeshMorph* NodeMorph = Cast<UCustomizableObjectNodeMeshMorph>(Object))
		{
			MeshPin = NodeMorph->MeshPin();
		}
		else if (const UCustomizableObjectNodeMeshReshape* NodeReshape = Cast<UCustomizableObjectNodeMeshReshape>(Object))
		{
			MeshPin = NodeReshape->BaseMeshPin();
		}
		else
		{
			checkf(false, TEXT("node not implemented"));
		}

		if (MeshPin)
		{
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*MeshPin))
			{
				if (const UEdGraphPin* SourceMeshPin = FindMeshBaseSource(*ConnectedPin, false))
				{
					if (const UCustomizableObjectNodeSkeletalMesh* SkeletalMeshNode = Cast<UCustomizableObjectNodeSkeletalMesh>(SourceMeshPin->GetOwningNode()))
					{
						return SkeletalMeshNode->SkeletalMesh;
					}
					else if (UCustomizableObjectNodeTable* TableNode = Cast<UCustomizableObjectNodeTable>(SourceMeshPin->GetOwningNode()))
					{
						return TableNode->GetColumnDefaultAssetByType<USkeletalMesh>(SourceMeshPin);
					}
				}
			}
		}
	}

	return nullptr;
}

FName FMeshReshapeBonesReferenceCustomization::GetSelectedBone(bool& bMultipleValues) const
{
	if (BoneNameProperty.IsValid())
	{
		FString OutText;

		FPropertyAccess::Result Result = BoneNameProperty->GetValueAsFormattedString(OutText);

		return FName(*OutText);
	}

	return FName();
}

const struct FReferenceSkeleton& FMeshReshapeBonesReferenceCustomization::GetReferenceSkeleton() const
{
	static FReferenceSkeleton DummySkeleton;

	USkeletalMesh* SkeletalMesh = GetSkeletalMesh(BoneNameProperty);
	return SkeletalMesh ? SkeletalMesh->GetRefSkeleton() : DummySkeleton;
}

void FMeshReshapeBonesReferenceCustomization::OnBoneSelectionChanged(FName Name)
{
	FMeshReshapeBoneReference NewBone;
	NewBone.BoneName = Name;

	if (!ParentArrayProperty)
	{
		BoneNameProperty->SetValue(Name);
		return;
	}

	uint32 NumParentElems = 0;
	FPropertyAccess::Result Result = ParentArrayProperty->GetNumElements(NumParentElems);
	check(Result == FPropertyAccess::Result::Success);

	// Find if already set.
	bool BoneNameFound = false;
	for (uint32 I = 0; I < NumParentElems; ++I)
	{
		TSharedPtr<IPropertyHandle> ElemProperty = ParentArrayProperty->GetElement(I);
		check(ElemProperty);
	
		constexpr bool bRecurse = false;	
		TSharedPtr<IPropertyHandle> ElemBoneNameProperty = 
				ElemProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMeshReshapeBoneReference, BoneName), bRecurse);

		check(ElemBoneNameProperty);

		FName BoneNameValue;
		Result = ElemBoneNameProperty->GetValue(BoneNameValue);

		check(Result == FPropertyAccess::Result::Success);

		if (BoneNameValue == NewBone.BoneName)
		{
			BoneNameFound = true;
			break;
		}	
	}


	// TODO: This behavior does not seem to be correct, if the option is repeated the value is not set
	// only logging a warning to the console, which might not be open and could make dificult to see what 
	// is the problem. 
	BoneNameProperty->SetValue(Name);
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

#undef LOCTEXT_NAMESPACE

