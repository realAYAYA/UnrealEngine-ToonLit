// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeMeshClipMorphDetails.h"

#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Engine/SkeletalMesh.h"
#include "Fonts/SlateFontInfo.h"
#include "HAL/PlatformCrt.h"
#include "IDetailsView.h"
#include "Internationalization/Internationalization.h"
#include "Layout/Margin.h"
#include "Math/Transform.h"
#include "Math/TransformVectorized.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "Misc/Attribute.h"
#include "MuCO/CustomizableObject.h"
#include "MuCOE/CustomizableObjectEditorUtilities.h"
#include "MuCOE/CustomizableObjectGraph.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshClipMorph.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "ReferenceSkeleton.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Templates/Casts.h"
#include "UObject/NameTypes.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

class UCustomizableObjectNodeObject;


#define LOCTEXT_NAMESPACE "MeshClipMorphDetails"


TSharedRef<IDetailCustomization> FCustomizableObjectNodeMeshClipMorphDetails::MakeInstance()
{
	return MakeShareable(new FCustomizableObjectNodeMeshClipMorphDetails);
}


void FCustomizableObjectNodeMeshClipMorphDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	Node = nullptr;
	DetailBuilderPtr = &DetailBuilder;

	const IDetailsView* DetailsView = DetailBuilder.GetDetailsView();
	if (DetailsView->GetSelectedObjects().Num())
	{
		Node = Cast<UCustomizableObjectNodeMeshClipMorph>(DetailsView->GetSelectedObjects()[0].Get());
	}

	IDetailCategoryBuilder& BlocksCategory = DetailBuilder.EditCategory("MeshToClipAndMorph");
	DetailBuilder.HideProperty("BoneName");

	IDetailCategoryBuilder& MeshClipParametersCategory = DetailBuilder.EditCategory("MeshClipParameters");
	DetailBuilder.HideProperty("bInvertNormal");

	TSharedPtr<IPropertyHandle> ReferenceSkeletonIndexPropertyHandle = DetailBuilder.GetProperty("ReferenceSkeletonIndex");
	const FSimpleDelegate OnReferenceSkeletonIndexChangedDelegate = 
		FSimpleDelegate::CreateSP(this, &FCustomizableObjectNodeMeshClipMorphDetails::OnReferenceSkeletonIndexChanged);
	ReferenceSkeletonIndexPropertyHandle->SetOnPropertyValueChanged(OnReferenceSkeletonIndexChangedDelegate);

	if (Node)
	{
		SkeletalMesh = nullptr;
		TSharedPtr<FString> BoneToSelect;

		UCustomizableObject* CustomizableObject = Cast<UCustomizableObject>(Node->GetCustomizableObjectGraph()->GetOuter());

		if (CustomizableObject)
		{
			if (CustomizableObject->bIsChildObject)
			{
				bool bMultipleBaseObjects;
				UCustomizableObjectNodeObject* RootNode = GetRootNode(CustomizableObject, bMultipleBaseObjects);

				if (RootNode && !bMultipleBaseObjects)
				{
					TArray<UCustomizableObject*> VisitedObjects;
					UCustomizableObject* Parent = GetFullGraphRootObject(RootNode, VisitedObjects);

					if (Parent)
					{
						SkeletalMesh = VisitedObjects.Last()->GetRefSkeletalMesh(Node->ReferenceSkeletonIndex);
					}
				}
			}
			else
			{
				SkeletalMesh = CustomizableObject->GetRefSkeletalMesh(Node->ReferenceSkeletonIndex);
			}
		}

		if (SkeletalMesh)
		{
			BoneComboOptions.Empty();
				
			for (int32 i = 0; i < SkeletalMesh->GetRefSkeleton().GetNum(); ++i)
			{
				FName BoneName = SkeletalMesh->GetRefSkeleton().GetBoneName(i);
				BoneComboOptions.Add(MakeShareable(new FString(BoneName.ToString())));

				if (BoneName == Node->BoneName)
				{
					BoneToSelect = BoneComboOptions.Last();
				}
			}

            BoneComboOptions.Sort(CompareNames);

			// Add them to the parent combo box
			TSharedRef<IPropertyHandle> BoneProperty = DetailBuilder.GetProperty("BoneName");

			BlocksCategory.AddCustomRow(LOCTEXT("ClipMorphDetails_BoneName", "Bone Name"))
			[
				SNew(SProperty, BoneProperty)
				.ShouldDisplayName(false)
				.CustomWidget()
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("NoBorder"))
					.Padding(FMargin(0.0f, 0.0f, 10.0f, 0.0f))
					.HAlign(EHorizontalAlignment::HAlign_Fill)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text( LOCTEXT("ClipMorphDetails_BoneName", "Bone Name"))
							.Font(IDetailLayoutBuilder::GetDetailFont())
						]
						+SHorizontalBox::Slot().HAlign(HAlign_Fill)
						[
							SNew(STextComboBox)
							.OptionsSource(&BoneComboOptions)
							.InitiallySelectedItem(BoneToSelect)
							.OnSelectionChanged(this, &FCustomizableObjectNodeMeshClipMorphDetails::OnBoneComboBoxSelectionChanged, BoneProperty)
							.Font(IDetailLayoutBuilder::GetDetailFont())
						]
					]
				]
			];

			MeshClipParametersCategory.AddCustomRow(LOCTEXT("ClipMorphDetails_PlaneNormal", "Invert plane normal"))
			[
				SNew(SProperty, BoneProperty)
				.ShouldDisplayName(false)
				.CustomWidget()
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("NoBorder"))
					.Padding(FMargin(0.0f, 0.0f, 10.0f, 0.0f))
					.HAlign(EHorizontalAlignment::HAlign_Fill)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text( LOCTEXT("ClipMorphDetails_PlaneNormal", "Invert plane normal"))
						]
						+SHorizontalBox::Slot().HAlign(HAlign_Left)
						[
							SNew(SCheckBox)
							.OnCheckStateChanged(this, &FCustomizableObjectNodeMeshClipMorphDetails::OnInvertNormalCheckboxChanged)
							.IsChecked(this, &FCustomizableObjectNodeMeshClipMorphDetails::GetInvertNormalCheckBoxState)
							.ToolTipText(LOCTEXT("ClipMorphDetails_InvertNormal_Tooltip", "Invert normal direction of the clip plane"))
						]
					]
				]
			];
		}
	}
	else
	{
		BlocksCategory.AddCustomRow(LOCTEXT("Node", "Node"))
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ClipMorphDetails_InvertNormal_NodeNotFound", "Node not found"))
		];
	}
}


FVector FindBoneLocation(int32 BoneIndex, USkeletalMesh* SkeletalMesh)
{
	const TArray<FTransform>& BoneArray = SkeletalMesh->GetRefSkeleton().GetRefBonePose();

	int32 ParentIndex = BoneIndex;
	FVector Location = FVector::ZeroVector;

	while (ParentIndex >= 0)
	{
		Location = BoneArray[ParentIndex].TransformPosition(Location);
		ParentIndex = SkeletalMesh->GetRefSkeleton().GetParentIndex(ParentIndex);
	}

	return Location;
}


void FCustomizableObjectNodeMeshClipMorphDetails::OnBoneComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo, TSharedRef<IPropertyHandle> BoneProperty)
{
	for (int OptionIndex = 0; OptionIndex < BoneComboOptions.Num(); ++OptionIndex)
	{
		if (BoneComboOptions[OptionIndex] == Selection)
		{
			//TArray<FVector> AffectedBoneLocations;

			//if (SkeletalMesh)
			//{
			//	const TArray<FTransform>& BoneArray = SkeletalMesh->RefSkeleton.GetRefBonePose();
			//	int32 BoneIndex = SkeletalMesh->RefSkeleton.FindBoneIndex(FName(**Selection));
			//	
			//	if (BoneIndex >= 0)
			//	{
			//		// Get tranform from skeleton root to selected bone
			//		int32 ParentIndex = BoneIndex;
			//		FTransform RootToBoneTransform = FTransform::Identity;

			//		while (ParentIndex >= 0)
			//		{
			//			RootToBoneTransform = RootToBoneTransform * BoneArray[ParentIndex];
			//			ParentIndex = SkeletalMesh->RefSkeleton.GetParentIndex(ParentIndex);
			//		}

			//		// Now that we have the transform from root to the selected bone, go down from this bone to the end of the bone chain hanging from it building a bounding box
			//		int32 CurrentBone = BoneIndex;

			//		FVector SelectedBoneLocationWithTransform = RootToBoneTransform.TransformPosition(FVector::ZeroVector);
			//		FVector SelectedBoneLocationFromSelection = BoneLocations[OptionIndex];
			//		check(FVector::DistSquared(SelectedBoneLocationWithTransform, SelectedBoneLocationFromSelection) < 0.01f);

			//		while (CurrentBone >= 0)
			//		{
			//			FVector CurrentBoneLocation = RootToBoneTransform.TransformPosition(FVector::ZeroVector);
			//			AffectedBoneLocations.Add(CurrentBoneLocation);

			//			check(FVector::DistSquared(CurrentBoneLocation, FindBoneLocation(CurrentBone, SkeletalMesh)) < 0.01f);

			//			int32 NextBone = -1;

			//			for (int32 i = 0; i < SkeletalMesh->RefSkeleton.GetNum(); ++i)
			//			{
			//				if (SkeletalMesh->RefSkeleton.GetParentIndex(i) == CurrentBone)
			//				{
			//					NextBone = i;
			//					break;
			//				}
			//			}

			//			CurrentBone = NextBone;

			//			if (CurrentBone >= 0)
			//			{
			//				RootToBoneTransform = BoneArray[CurrentBone] * RootToBoneTransform;
			//			}
			//		}
			//	}
			//}

			FVector Location = FVector::ZeroVector;
			FVector Direction = FVector::ForwardVector;

			if (SkeletalMesh)
			{
				const TArray<FTransform>& BoneArray = SkeletalMesh->GetRefSkeleton().GetRefBonePose();
				int32 ParentIndex = SkeletalMesh->GetRefSkeleton().FindBoneIndex(FName(**Selection));

				FVector ChildLocation = FVector::ForwardVector;

				for (int32 i = 0; i < SkeletalMesh->GetRefSkeleton().GetNum(); ++i)
				{
					if (SkeletalMesh->GetRefSkeleton().GetParentIndex(i) == ParentIndex)
					{
						ChildLocation = BoneArray[i].TransformPosition(FVector::ZeroVector);
						break;
					}
				}

				//Direction = ChildLocation;

				while (ParentIndex >= 0)
				{
					Location = BoneArray[ParentIndex].TransformPosition(Location);
					ChildLocation = BoneArray[ParentIndex].TransformPosition(ChildLocation);
					//Direction = BoneArray[ParentIndex].TransformVector(Direction);
					ParentIndex = SkeletalMesh->GetRefSkeleton().GetParentIndex(ParentIndex);
				}

				Direction = (ChildLocation - Location).GetSafeNormal();
			}
			 
			BoneProperty->SetValue(*BoneComboOptions[OptionIndex].Get());
			
			if (Node)
			{
				Node->Origin = Location;
				Node->Normal = Direction;
			}

			return;
		}
	}

	BoneProperty->SetValue(FString());
}


void FCustomizableObjectNodeMeshClipMorphDetails::OnInvertNormalCheckboxChanged(ECheckBoxState CheckBoxState)
{
	if (Node == nullptr)
	{
		return;
	}

	switch (CheckBoxState)
	{
		case ECheckBoxState::Checked:
		{
			Node->bInvertNormal = true;
			Node->Normal *= -1.0f;
			break;
		}
		case ECheckBoxState::Unchecked:
		{
			Node->bInvertNormal = false;
			Node->Normal *= -1.0f;
			break;
		}
	}

	if (Node->bLocalStartOffset)
	{
		Node->StartOffset.Z *= -1;
		Node->StartOffset.X *= -1;
	}
}


ECheckBoxState FCustomizableObjectNodeMeshClipMorphDetails::GetInvertNormalCheckBoxState() const
{
	if (Node == nullptr)
	{
		return ECheckBoxState::Unchecked;
	}

	return Node->bInvertNormal ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}


void FCustomizableObjectNodeMeshClipMorphDetails::OnReferenceSkeletonIndexChanged()
{
	if (Node)
	{
		Node->BoneName = FName();
	}
	DetailBuilderPtr->ForceRefreshDetails();
}


#undef LOCTEXT_NAMESPACE
