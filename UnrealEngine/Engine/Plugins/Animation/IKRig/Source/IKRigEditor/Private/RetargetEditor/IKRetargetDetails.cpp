// Copyright Epic Games, Inc. All Rights Reserved.

#include "RetargetEditor/IKRetargetDetails.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Animation/AnimInstance.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Framework/MultiBox/SToolBarButtonBlock.h"
#include "RetargetEditor/IKRetargetEditor.h"
#include "RetargetEditor/IKRetargetEditorController.h"
#include "RetargetEditor/IKRetargeterController.h"
#include "Retargeter/IKRetargeter.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "ScopedTransaction.h"
#include "AnimationRuntime.h"
#include "SSearchableComboBox.h"
#include "UObject/UnrealTypePrivate.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IKRetargetDetails)

#if WITH_EDITOR
#include "HAL/PlatformApplicationMisc.h"
#endif

#define LOCTEXT_NAMESPACE "IKRetargeterDetails"


FEulerTransform UIKRetargetBoneDetails::GetTransform(EIKRetargetTransformType TransformType, const bool bLocalSpace) const
{
	// editor setup?
	const FIKRetargetEditorController* Controller = EditorController.Get();
	if (!Controller)
	{
		return FEulerTransform::Identity;
	}

	// ensure we have a valid skeletal mesh
	const bool bEditingSource = EditorController->GetSourceOrTarget() == ERetargetSourceOrTarget::Source;
	UDebugSkelMeshComponent* Mesh = bEditingSource ? Controller->SourceSkelMeshComponent : Controller->TargetSkelMeshComponent;
	if (!(Mesh && Mesh->GetSkeletalMeshAsset()))
	{
		return FEulerTransform::Identity;
	}

	// ensure bone is valid
	const FReferenceSkeleton& RefSkeleton = Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
	const int32 BoneIndex = RefSkeleton.FindBoneIndex(SelectedBone);
	if (BoneIndex == INDEX_NONE)
	{
		return FEulerTransform::Identity;
	}
	
	switch(TransformType)
	{
	case EIKRetargetTransformType::Current:
		{
			if (bLocalSpace)
			{
				const TArray<FTransform>& LocalTransforms = Mesh->GetBoneSpaceTransforms();
				return LocalTransforms.IsValidIndex(BoneIndex) ? FEulerTransform(LocalTransforms[BoneIndex]) : FEulerTransform::Identity;
			}
			else
			{
				return FEulerTransform(Mesh->GetBoneTransform(BoneIndex, FTransform::Identity));
			}
		}
	
	case EIKRetargetTransformType::Reference:
		{
			if (bLocalSpace)
			{
				return FEulerTransform(RefSkeleton.GetRefBonePose()[BoneIndex]);
			}
			else
			{
				return FEulerTransform(FAnimationRuntime::GetComponentSpaceTransform(RefSkeleton, RefSkeleton.GetRefBonePose(), BoneIndex));
			}
		}

	case EIKRetargetTransformType::RelativeOffset:
		{
			// this is the only stored data we have for bone pose offsets
			const ERetargetSourceOrTarget SourceOrTarget = EditorController->GetSourceOrTarget();
			const FRotator LocalRotationDelta = EditorController->AssetController->GetRotationOffsetForRetargetPoseBone(SelectedBone, SourceOrTarget).Rotator();
			const FVector GlobalTranslationDelta = IsRootBone() ? EditorController->AssetController->GetCurrentRetargetPose(SourceOrTarget).GetRootTranslationDelta() : FVector::Zero();
			const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);

			if (bLocalSpace)
			{
				// create partial local delta transform
				FEulerTransform LocalDeltaTransform = FEulerTransform::Identity;
				LocalDeltaTransform.Rotation = LocalRotationDelta;
				// get parent global transform to calculate local translation delta
				FTransform ParentRefGlobalTransform = FTransform::Identity;
				if (ParentIndex != INDEX_NONE)
				{
					ParentRefGlobalTransform = FAnimationRuntime::GetComponentSpaceTransform(RefSkeleton, RefSkeleton.GetRefBonePose(), ParentIndex);
				}
				// now calculate local translational delta from global
				LocalDeltaTransform.SetLocation(ParentRefGlobalTransform.InverseTransformVector(GlobalTranslationDelta));
				return LocalDeltaTransform;
			}
			else
			{
				// get the CURRENT parent global transform and reference LOCAL transform to calculate the
				// current global transform of the bone without any offsets applied
				FTransform ParentGlobalTransform = FTransform::Identity;
				if (ParentIndex != INDEX_NONE)
				{
					ParentGlobalTransform = Mesh->GetBoneTransform(ParentIndex, FTransform::Identity);
				}
				FTransform LocalRefTransform = RefSkeleton.GetRefBonePose()[BoneIndex];
				FTransform GlobalTransform = LocalRefTransform * ParentGlobalTransform;
				// get global rotation plus delta
				FQuat GlobalRotationPlusDelta = GlobalTransform.GetRotation() * LocalRotationDelta.Quaternion();
				// get global delta rotation
				FQuat GlobalDeltaRotation = GlobalRotationPlusDelta * GlobalTransform.GetRotation().Inverse();
				// combine with translation delta
				return FEulerTransform(GlobalTranslationDelta, GlobalDeltaRotation.Rotator(), FVector::OneVector);
			}
		}
	default:
		checkNoEntry();
	}

	return FEulerTransform::Identity;
}

bool UIKRetargetBoneDetails::IsComponentRelative(
	ESlateTransformComponent::Type Component,
	EIKRetargetTransformType TransformType) const
{
	switch(TransformType)
	{
	case EIKRetargetTransformType::Current:
		{
			return CurrentTransformRelative[(int32)Component]; 
		}
	case EIKRetargetTransformType::Reference:
		{
			return ReferenceTransformRelative[(int32)Component]; 
		}
	case EIKRetargetTransformType::RelativeOffset:
		{
			return RelativeOffsetTransformRelative[(int32)Component];
		}
	}
	return true;
}

void UIKRetargetBoneDetails::OnComponentRelativeChanged(
	ESlateTransformComponent::Type Component,
	bool bIsRelative,
	EIKRetargetTransformType TransformType)
{
	switch(TransformType)
	{
	case EIKRetargetTransformType::Current:
		{
			CurrentTransformRelative[(int32)Component] = bIsRelative;
			break; 
		}
	case EIKRetargetTransformType::Reference:
		{
			ReferenceTransformRelative[(int32)Component] = bIsRelative;
			break; 
		}
	case EIKRetargetTransformType::RelativeOffset:
		{
			RelativeOffsetTransformRelative[(int32)Component] = bIsRelative;
			break; 
		}
	}
}

void UIKRetargetBoneDetails::OnCopyToClipboard(
	ESlateTransformComponent::Type Component,
	EIKRetargetTransformType TransformType) const
{
	// get is local or global space
	bool bIsRelative = false;
	switch(TransformType)
	{
	case EIKRetargetTransformType::Current:
		{
			bIsRelative = CurrentTransformRelative[(int32)Component];
			break;
		}
	case EIKRetargetTransformType::Reference:
		{
			bIsRelative = ReferenceTransformRelative[(int32)Component];
			break;
		}
	case EIKRetargetTransformType::RelativeOffset:
		{
			bIsRelative = RelativeOffsetTransformRelative[(int32)Component];
			break;
		}
	default:
		checkNoEntry();
	}

	// get the transform of correct type and space
	const FEulerTransform Transform = GetTransform(TransformType, bIsRelative);
	
	FString Content;
	switch(Component)
	{
	case ESlateTransformComponent::Location:
		{
			GetContentFromData(Transform.GetLocation(), Content);
			break;
		}
	case ESlateTransformComponent::Rotation:
		{
			GetContentFromData(Transform.Rotator(), Content);
			break;
		}
	case ESlateTransformComponent::Scale:
		{
			GetContentFromData(Transform.GetScale3D(), Content);
			break;
		}
	case ESlateTransformComponent::Max:
	default:
		{
			GetContentFromData(Transform, Content);
			TBaseStructure<FTransform>::Get()->ExportText(Content, &Transform, &Transform, nullptr, PPF_None, nullptr);
			break;
		}
	}

	if(!Content.IsEmpty())
	{
		FPlatformApplicationMisc::ClipboardCopy(*Content);
	}
}

void UIKRetargetBoneDetails::OnPasteFromClipboard(
	ESlateTransformComponent::Type Component,
	EIKRetargetTransformType TransformType)
{
	// only allow editing of relative offsets in retarget poses
	if(TransformType != EIKRetargetTransformType::RelativeOffset)
	{
		return;
	}

	// must have valid controller
	UIKRetargeterController* AssetController = EditorController->AssetController;
	if(!AssetController)
	{
		return;
	}
	
	// get content of clipboard to paste
	FString Content;
	FPlatformApplicationMisc::ClipboardPaste(Content);
	if(Content.IsEmpty())
	{
		return;
	}
	
	class FRetargetPasteTransformWidgetErrorPipe : public FOutputDevice
	{
	public:

		int32 NumErrors;

		FRetargetPasteTransformWidgetErrorPipe(FIKRigLogger* InLog)	: FOutputDevice(), NumErrors(0), Log(InLog) {}

		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
		{
			if (Log)
			{
				Log->LogError(LOCTEXT("RetargetPasteError", "Error pasting transform data to bone."));
			}
			
			NumErrors++;
		}

		FIKRigLogger* Log;
	};

	FIKRigLogger* Log = nullptr;
	if (UIKRetargetProcessor* Processor = EditorController->GetRetargetProcessor())
	{
		Log = &Processor->Log;
	}
	FRetargetPasteTransformWidgetErrorPipe ErrorPipe(Log);
	
	// get the transform of correct type and space
	const bool bIsRelative = IsComponentRelative(Component, TransformType);
	FEulerTransform Transform = GetTransform(TransformType, bIsRelative);

	const ERetargetSourceOrTarget SourceOrTarget = EditorController->GetSourceOrTarget();

	// create a transaction on the asset
	FScopedTransaction Transaction(LOCTEXT("PasteTransform", "Paste Transform"));
	EditorController.Get()->AssetController->GetAsset()->Modify();
	
	switch(Component)
	{
	case ESlateTransformComponent::Location:
		{
			FVector Data = Transform.GetLocation();
			const TCHAR* Result = TBaseStructure<FVector>::Get()->ImportText(*Content, &Data, nullptr, PPF_None, &ErrorPipe, TBaseStructure<FVector>::Get()->GetName(), true);
			if (Result && ErrorPipe.NumErrors == 0)
			{
				Transform.SetLocation(Data);
				EditorController->AssetController->GetCurrentRetargetPose(SourceOrTarget).SetRootTranslationDelta(Transform.GetLocation());
			}
			break;
		}
	case ESlateTransformComponent::Rotation:
		{
			FRotator Data = Transform.Rotator();
			const TCHAR* Result = TBaseStructure<FRotator>::Get()->ImportText(*Content, &Data, nullptr, PPF_None, &ErrorPipe, TBaseStructure<FRotator>::Get()->GetName(), true);
			if (Result && ErrorPipe.NumErrors == 0)
			{
				Transform.SetRotator(Data);
				EditorController->AssetController->SetRotationOffsetForRetargetPoseBone(SelectedBone, Transform.GetRotation(), SourceOrTarget);
			}
			break;
		}
	default:
		checkNoEntry();
	}
}

TOptional<FVector::FReal> UIKRetargetBoneDetails::GetNumericValue(
	EIKRetargetTransformType TransformType,
	ESlateTransformComponent::Type Component,
	ESlateRotationRepresentation::Type Representation,
	ESlateTransformSubComponent::Type SubComponent)
{
	FEulerTransform Transform = FEulerTransform::Identity;
	
	switch(TransformType)
	{
	case EIKRetargetTransformType::Current:
		{
			Transform = GetTransform(TransformType, CurrentTransformRelative[(int32)Component]);
			break;
		}
	
	case EIKRetargetTransformType::Reference:
		{
			Transform = GetTransform(TransformType, ReferenceTransformRelative[(int32)Component]);
			break;
		}

	case EIKRetargetTransformType::RelativeOffset:
		{
			Transform = GetTransform(TransformType, RelativeOffsetTransformRelative[(int32)Component]);
			break;
		}
	default:
		checkNoEntry();
	}

	return CleanRealValue(SAdvancedTransformInputBox<FEulerTransform>::GetNumericValueFromTransform(Transform, Component, Representation, SubComponent));
}

void UIKRetargetBoneDetails::OnNumericValueCommitted(
	ESlateTransformComponent::Type Component,
	ESlateRotationRepresentation::Type Representation,
	ESlateTransformSubComponent::Type SubComponent,
	FVector::FReal Value,
	ETextCommit::Type CommitType,
	EIKRetargetTransformType TransformType,
	bool bIsCommit)
{
	if(TransformType != EIKRetargetTransformType::RelativeOffset)
	{
		return;
	}

	UIKRetargeterController* AssetController = EditorController->AssetController;
	if(!AssetController)
	{
		return;
	}

	// ensure we have a valid skeletal mesh
	const bool bEditingSource = EditorController->GetSourceOrTarget() == ERetargetSourceOrTarget::Source;
	UDebugSkelMeshComponent* Mesh = bEditingSource ? EditorController->SourceSkelMeshComponent : EditorController->TargetSkelMeshComponent;
	if (!(Mesh && Mesh->GetSkeletalMeshAsset()))
	{
		return;
	}

	// ensure bone is valid
	const FReferenceSkeleton& RefSkeleton = Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
	const int32 BoneIndex = RefSkeleton.FindBoneIndex(SelectedBone);
	if (BoneIndex == INDEX_NONE)
	{
		return;
	}

	const ERetargetSourceOrTarget SourceOrTarget = EditorController->GetSourceOrTarget();

	switch(Component)
	{
	case ESlateTransformComponent::Location:
		{
			const bool bIsTranslationLocal = RelativeOffsetTransformRelative[0];
			FTransform CurrentGlobalOffset = FTransform::Identity;
			CurrentGlobalOffset.SetTranslation(AssetController->GetCurrentRetargetPose(SourceOrTarget).GetRootTranslationDelta());
			
			if (bIsTranslationLocal)
			{	
				// get the current LOCAL offset
				FTransform CurrentLocalOffset = CurrentGlobalOffset;
				FTransform ParentGlobalRefTransform = FTransform::Identity;
				const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
				if (ParentIndex != INDEX_NONE)
				{
					ParentGlobalRefTransform = FAnimationRuntime::GetComponentSpaceTransform(RefSkeleton, RefSkeleton.GetRefBonePose(), ParentIndex);
				}
				CurrentLocalOffset = CurrentLocalOffset.GetRelativeTransform(ParentGlobalRefTransform);

				// apply the numerical value to the local space values
				SAdvancedTransformInputBox<FTransform>::ApplyNumericValueChange(CurrentLocalOffset, Value, Component, Representation, SubComponent);

				// convert back to global space for storage in the pose
				CurrentGlobalOffset = CurrentLocalOffset * ParentGlobalRefTransform;
			}
			else
			{
				// apply the edit
				SAdvancedTransformInputBox<FTransform>::ApplyNumericValueChange(CurrentGlobalOffset, Value, Component, Representation, SubComponent);
			}
			
			// store the new transform in the retarget pose
			FScopedTransaction Transaction(LOCTEXT("EditRootTranslation", "Edit Retarget Root Pose Translation"));
			EditorController->AssetController->GetAsset()->Modify();
			EditorController->AssetController->GetCurrentRetargetPose(SourceOrTarget).SetRootTranslationDelta(CurrentGlobalOffset.GetTranslation());
			
			break;
		}
	case ESlateTransformComponent::Rotation:
		{
			const bool bIsRotationLocal = RelativeOffsetTransformRelative[1];
			const FQuat LocalRotationDelta = AssetController->GetRotationOffsetForRetargetPoseBone(SelectedBone, SourceOrTarget);
			FEulerTransform LocalDeltaTransform = FEulerTransform(FVector::ZeroVector, LocalRotationDelta.Rotator(), FVector::OneVector);
			FQuat NewLocalRotationDelta;
			
			if (bIsRotationLocal)
			{
				// rotations are stored in local space, so just apply the edit
				SAdvancedTransformInputBox<FEulerTransform>::ApplyNumericValueChange(LocalDeltaTransform, Value, Component, Representation, SubComponent);
				NewLocalRotationDelta = LocalDeltaTransform.GetRotation();
			}
			else
			{
				FTransform ParentGlobalTransform = FTransform::Identity;
				const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
				if (ParentIndex != INDEX_NONE)
				{
					ParentGlobalTransform = Mesh->GetBoneTransform(ParentIndex, FTransform::Identity);
				}
				FTransform LocalRefTransform = RefSkeleton.GetRefBonePose()[BoneIndex];
				FTransform GlobalTransform = LocalRefTransform * ParentGlobalTransform;
				
				// get reference global transform
				//FTransform GlobalTransform = Mesh->GetBoneTransform(BoneIndex, FTransform::Identity);
				// get offset global transform
				FQuat GlobalRefRotationPlusDelta = GlobalTransform.GetRotation() * LocalRotationDelta;
				// get global delta
				FQuat GlobalDeltaRotation = GlobalRefRotationPlusDelta * GlobalTransform.GetRotation().Inverse();
				// apply edit to global delta
				FEulerTransform GlobalDeltaTransform = FEulerTransform(FVector::ZeroVector, GlobalDeltaRotation.Rotator(), FVector::OneVector);
				SAdvancedTransformInputBox<FEulerTransform>::ApplyNumericValueChange(GlobalDeltaTransform, Value, Component, Representation, SubComponent);

				// convert world space delta quaternion to bone-space
				GlobalDeltaRotation = GlobalDeltaTransform.GetRotation();
				const FVector RotationAxis = GlobalDeltaRotation.GetRotationAxis();
				const FVector UnRotatedAxis = GlobalTransform.InverseTransformVector(RotationAxis);
				NewLocalRotationDelta = FQuat(UnRotatedAxis, GlobalDeltaRotation.GetAngle());
			}
			
			// store the new rotation in the retarget pose
			FScopedTransaction Transaction(LOCTEXT("EditRootRotation", "Edit Retarget Pose Rotation"));
			EditorController->AssetController->GetAsset()->Modify();
			EditorController->AssetController->SetRotationOffsetForRetargetPoseBone(SelectedBone, NewLocalRotationDelta, SourceOrTarget);
			break;
		}
	default:
		checkNoEntry();
	}
}

bool UIKRetargetBoneDetails::IsRootBone() const
{
	const FName RootBone = EditorController->AssetController->GetRetargetRootBone(EditorController->GetSourceOrTarget());
	return SelectedBone == RootBone;
}

void UIKRetargetBoneDetails::OnMultiNumericValueCommitted(
	ESlateTransformComponent::Type Component,
	ESlateRotationRepresentation::Type Representation,
	ESlateTransformSubComponent::Type SubComponent,
	FVector::FReal Value,
	ETextCommit::Type CommitType,
	EIKRetargetTransformType TransformType,
	TArrayView<UIKRetargetBoneDetails*> Bones,
	bool bIsCommit)
{
	for(UIKRetargetBoneDetails* Bone : Bones)
	{	
		Bone->OnNumericValueCommitted(Component, Representation, SubComponent, Value, CommitType, TransformType, bIsCommit);
	}
}

template <typename DataType>
void UIKRetargetBoneDetails::GetContentFromData(const DataType& InData, FString& Content) const
{
	TBaseStructure<DataType>::Get()->ExportText(Content, &InData, &InData, nullptr, PPF_None, nullptr);
}

TOptional<FVector::FReal> UIKRetargetBoneDetails::CleanRealValue(TOptional<FVector::FReal> InValue)
{
	// remove insignificant decimal noise and sign bit if value near zero
	if (InValue.IsSet() && FMath::IsNearlyZero(InValue.GetValue(), UE_KINDA_SMALL_NUMBER))
	{
		InValue = 0.0f;
	}
	
	return InValue;
}

// ------------------------------------------- BEGIN FIKRetargetBoneDetailCustomization -------------------------------

void FIKRetargetBoneDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized = DetailBuilder.GetSelectedObjects();
	Bones.Reset();
	for (const TWeakObjectPtr<UObject>& Object : ObjectsBeingCustomized)
	{
		if (UIKRetargetBoneDetails* Bone = Cast<UIKRetargetBoneDetails>(Object.Get()))
		{
			Bones.Add(Bone);
		}
	}
	
	if (Bones.IsEmpty())
	{
		return;
	}

	if (!Bones[0]->EditorController.IsValid())
	{
		return;
	}
	
	const FIKRetargetEditorController& Controller = *Bones[0]->EditorController.Get();
	const UIKRetargeterController* AssetController = Controller.AssetController;
	
	const bool bIsEditingPose = Controller.IsEditingPose();

	const FName CurrentRootName = AssetController->GetRetargetRootBone(Controller.GetSourceOrTarget());
	const bool bIsRootSelected =  Bones[0]->SelectedBone == CurrentRootName;

	FIKRetargetTransformUIData UIData;
	GetTransformUIData( bIsEditingPose, DetailBuilder, UIData);

	TSharedPtr<SSegmentedControl<EIKRetargetTransformType>> TransformChoiceWidget =
		SSegmentedControl<EIKRetargetTransformType>::Create(
			UIData.TransformTypes,
			UIData.ButtonLabels,
			UIData.ButtonTooltips,
			UIData.VisibleTransforms
		);

	DetailBuilder.EditCategory(TEXT("Selection")).SetSortOrder(1);

	IDetailCategoryBuilder& CategoryBuilder = DetailBuilder.EditCategory(TEXT("Transforms"));
	CategoryBuilder.SetSortOrder(2);
	CategoryBuilder.AddCustomRow(FText::FromString(TEXT("TransformType")))
	.ValueContent()
	.MinDesiredWidth(375.f)
	.MaxDesiredWidth(375.f)
	.HAlign(HAlign_Left)
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			TransformChoiceWidget.ToSharedRef()
		]
	];

	SAdvancedTransformInputBox<FTransform>::FArguments TransformWidgetArgs = SAdvancedTransformInputBox<FTransform>::FArguments()
	.ConstructLocation(!bIsEditingPose || (bIsEditingPose && bIsRootSelected))
	.ConstructRotation(true)
	.ConstructScale(!bIsEditingPose)
	.DisplayRelativeWorld(true)
	.DisplayScaleLock(false)
	.AllowEditRotationRepresentation(true)
	.Font(IDetailLayoutBuilder::GetDetailFont())
	.UseQuaternionForRotation(true);

	TArrayView<UIKRetargetBoneDetails*> BonesView = TArrayView<UIKRetargetBoneDetails*>(Bones);
	
	for(int32 PropertyIndex=0;PropertyIndex<UIData.Properties.Num();PropertyIndex++)
	{
		const EIKRetargetTransformType TransformType = UIData.TransformTypes[PropertyIndex];

		// only enable editing of the relative offset transform type while in edit mode
		const bool bIsEditable = bIsEditingPose && TransformType == EIKRetargetTransformType::RelativeOffset;
		
		TransformWidgetArgs.IsEnabled(bIsEditable);
		// edit transform
		if(bIsEditable)
		{
			TransformWidgetArgs.OnNumericValueChanged_Static(
				&UIKRetargetBoneDetails::OnMultiNumericValueCommitted,
				ETextCommit::Default,
				TransformType,
				BonesView,
				false);
		}

		// get/set relative
		TransformWidgetArgs.OnGetIsComponentRelative_Lambda( [bIsEditable, BonesView, TransformType](ESlateTransformComponent::Type InComponent)
		{
			return BonesView.ContainsByPredicate( [&](const TObjectPtr<UIKRetargetBoneDetails> Bone)
			{
				return Bone->IsComponentRelative(InComponent, TransformType);
			} );
		})
		.OnIsComponentRelativeChanged_Lambda( [bIsEditable, BonesView, TransformType](ESlateTransformComponent::Type InComponent, bool bIsRelative)
		{
			for (const TObjectPtr<UIKRetargetBoneDetails> Bone: BonesView)
			{
				Bone->OnComponentRelativeChanged(InComponent, bIsRelative, TransformType);
			}
		} );

		TransformWidgetArgs.OnGetNumericValue_Lambda([BonesView, TransformType](
		ESlateTransformComponent::Type Component,
		ESlateRotationRepresentation::Type Representation,
		ESlateTransformSubComponent::Type SubComponent) -> TOptional<FVector::FReal>
		{
			TOptional<FVector::FReal> FirstValue = BonesView[0]->GetNumericValue(TransformType, Component, Representation, SubComponent);
			
			if (FirstValue)
			{
				for (int32 Index = 1; Index < BonesView.Num(); Index++)
				{
					const TOptional<FVector::FReal> CurrentValue = BonesView[Index]->GetNumericValue(TransformType, Component, Representation, SubComponent);
					if (CurrentValue.IsSet())
					{
						// using a more permissive precision to avoid "Multiple Values" in details panel caused
						// by floating point noise introduced by normal rotation calculations
						constexpr double EDITING_PRECISION = 1.e-2;
						if(! FMath::IsNearlyEqual(FirstValue.GetValue(), CurrentValue.GetValue(), EDITING_PRECISION))
						{
							return TOptional<FVector::FReal>();
						}
					}
				}
			}
			return FirstValue;
		});

		// copy/paste bones transforms
		TransformWidgetArgs.OnCopyToClipboard_UObject(Bones[0], &UIKRetargetBoneDetails::OnCopyToClipboard, TransformType);
		TransformWidgetArgs.OnPasteFromClipboard_UObject(Bones[0], &UIKRetargetBoneDetails::OnPasteFromClipboard, TransformType);

		TransformWidgetArgs.Visibility_Lambda([TransformChoiceWidget, TransformType]() -> EVisibility
		{
			return TransformChoiceWidget->HasValue(TransformType) ? EVisibility::Visible : EVisibility::Collapsed;
		});

		SAdvancedTransformInputBox<FTransform>::ConstructGroupedTransformRows(
			CategoryBuilder, 
			UIData.ButtonLabels[PropertyIndex], 
			UIData.ButtonTooltips[PropertyIndex], 
			TransformWidgetArgs);
	}
}

void FIKRetargetBoneDetailCustomization::GetTransformUIData(
	const bool bIsEditingPose,
	const IDetailLayoutBuilder& DetailBuilder,
	FIKRetargetTransformUIData& OutData) const
{
	// read-only
	static const TArray<EIKRetargetTransformType> ReadOnlyTransformTypes
	{
		EIKRetargetTransformType::Current,
		EIKRetargetTransformType::Reference
	};
	static const TArray<FText> ReadOnlyButtonLabels
	{
		LOCTEXT("CurrentTransform", "Current"),
		LOCTEXT("ReferenceTransform", "Reference")
	};
	static const TArray<FText> ReadOnlyButtonTooltips
	{
		LOCTEXT("CurrentBoneTransformTooltip", "The current transform of the bone."),
		LOCTEXT("ReferenceBoneTransformTooltip", "The reference transform of the bone.")
	};
	static TAttribute<TArray<EIKRetargetTransformType>> ReadOnlyVisibleTransforms =
		TArray<EIKRetargetTransformType>({EIKRetargetTransformType::Current});


	// editable
	static const TArray<EIKRetargetTransformType> EditableTransformTypes
	{
		EIKRetargetTransformType::RelativeOffset,
		EIKRetargetTransformType::Reference
	};
	static const TArray<FText> EditableButtonLabels
	{
		LOCTEXT("EditableRelativeOffsetTransform", "Relative Offset"),
		LOCTEXT("EditableReferenceTransform", "Reference")
	};
	static const TArray<FText> EditableButtonTooltips
	{
		LOCTEXT("RelativeOffsetBoneTransformTooltip", "The offset transform in the current retarget pose, relative to the reference pose."),
		LOCTEXT("EditableReferenceBoneTransformTooltip", "The reference transform of the bone.")
	};
	static TAttribute<TArray<EIKRetargetTransformType>> EditableVisibleTransforms =
		TArray<EIKRetargetTransformType>({EIKRetargetTransformType::RelativeOffset});


	if (bIsEditingPose)
	{
		OutData.TransformTypes = EditableTransformTypes;
		OutData.ButtonLabels = EditableButtonLabels;
		OutData.ButtonTooltips = EditableButtonTooltips;
		OutData.VisibleTransforms = EditableVisibleTransforms;

		OutData.Properties.Append(
			{
			DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UIKRetargetBoneDetails, OffsetTransform)),
			DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UIKRetargetBoneDetails, ReferenceTransform))
			});
	}
	else
	{
		OutData.TransformTypes = ReadOnlyTransformTypes;
		OutData.ButtonLabels = ReadOnlyButtonLabels;
		OutData.ButtonTooltips = ReadOnlyButtonTooltips;
		OutData.VisibleTransforms = ReadOnlyVisibleTransforms;

		OutData.Properties.Append(
			{
			DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UIKRetargetBoneDetails, CurrentTransform)),
			DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UIKRetargetBoneDetails, ReferenceTransform))
			});
	}
}

void FRetargetChainSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized = DetailBuilder.GetSelectedObjects();
	ChainSettingsObjects.Reset();
	for (const TWeakObjectPtr<UObject>& Object : ObjectsBeingCustomized)
	{
		if (URetargetChainSettings* ChainSettings = Cast<URetargetChainSettings>(Object.Get()))
		{
			ChainSettingsObjects.Add(ChainSettings);
		}
	}
	
	if (ChainSettingsObjects.IsEmpty())
	{
		return;
	}

	Controller = ChainSettingsObjects[0]->EditorController;
	if (!Controller.IsValid())
	{
		return;
	}

	// hide the settings category and build our own
	DetailBuilder.HideCategory("Settings");
	DetailBuilder.HideCategory("Chain Mapping");

	// determine the current setup state of the chain (is it ready for FK and/or IK?)
	// FK: it can only run FK retarget if it's mapped to a source chain
	// IK: can only run IK if FK is setup AND it's mapped to a goal that is connected to a solver (in the target IK rig)
	auto CheckChainSetupForIKAndFK([this](
		const TObjectPtr<URetargetChainSettings> ChainSettings,
		bool& OutFKSetup,
		bool& OutIKSetup,
		bool& OutIKConnected)
	{
		OutFKSetup = true;
		OutIKSetup = true;
		OutIKConnected = true;
		
		if (ChainSettings->SourceChain == NAME_None || ChainSettings->TargetChain == NAME_None)
		{
			OutFKSetup = false;
			OutIKSetup = false;
			OutIKConnected = false;
			return;
		}

		const FName ChainGoal = Controller->AssetController->GetChainGoal(ChainSettings.Get());
		if (ChainGoal == NAME_None)
		{
			OutIKSetup = false;
		}

		if (!Controller->AssetController->IsChainGoalConnectedToASolver(ChainGoal))
		{
			OutIKConnected = false;
		}
	});

	// get the FK and IK status of the first selected chain
	bool bFirstChainFKSetup;
	bool bFirstChainIKSetup;
	bool bFirstChainIKConnected;
	CheckChainSetupForIKAndFK(ChainSettingsObjects[0].Get(), bFirstChainFKSetup, bFirstChainIKSetup, bFirstChainIKConnected);

	// check to see if there is a mix of chains with valid FK or IK setups
	bool bIsFKSetupMultipleValues = false;
	bool bIsIKSetupMultipleValues = false;
	bool bIsIKConnectedMultipleValues = false;
	for (const TWeakObjectPtr<URetargetChainSettings> ChainSettings : ChainSettingsObjects)
	{
		bool bIsFKSetup;
		bool bIsIKSetup;
		bool bIsIKConnected;
		CheckChainSetupForIKAndFK(ChainSettings.Get(), bIsFKSetup, bIsIKSetup, bIsIKConnected);
		bIsFKSetupMultipleValues = bIsFKSetup != bFirstChainFKSetup ? true : bIsFKSetupMultipleValues;
		bIsIKSetupMultipleValues = bIsIKSetup != bFirstChainIKSetup ? true : bIsIKSetupMultipleValues;
		bIsIKConnectedMultipleValues = bIsIKConnected != bFirstChainIKConnected ? true : bIsIKConnectedMultipleValues;
	}

	// are we editing multiple chains?
	const bool bEditingMultipleChains = ChainSettingsObjects.Num() > 1;
	TObjectPtr<URetargetChainSettings> PrimaryChainSettingsObject = ChainSettingsObjects[0].Get();

	// get source chain options for combobox
	SourceChainOptions.Reset();
	SourceChainOptions.Add(MakeShareable(new FString(TEXT("None"))));
	if (const UIKRigDefinition* SourceIKRig = Controller->AssetController->GetIKRig(ERetargetSourceOrTarget::Source))
	{
		const TArray<FBoneChain>& Chains = SourceIKRig->GetRetargetChains();
		for (const FBoneChain& BoneChain : Chains)
		{
			SourceChainOptions.Add(MakeShareable(new FString(BoneChain.ChainName.ToString())));
		}
	}
	
	// replace the "Settings" struct property
	const FText ChainTitleName = bEditingMultipleChains ?  LOCTEXT("MultipleChains_Label", "Multiple Chain") : FText::FromName(PrimaryChainSettingsObject->TargetChain);
	const FText ChainCategoryTitle = FText::Format( LOCTEXT("ChainSettingsHeaderLabel", "{0} Settings"), ChainTitleName);
	IDetailCategoryBuilder& SettingsCategory = DetailBuilder.EditCategory("Chain Settings", ChainCategoryTitle);
	
	// add row to select source chain to map to
	SettingsCategory.AddCustomRow(LOCTEXT("ChainStatus_Label", "Status"))
	.NameContent()
	[
		SNew(SBox)
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT( "SoureChain_Label", "Source Chain" ))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
	]
	.ValueContent()
	.MinDesiredWidth(166.0f)
	[
		SNew(SSearchableComboBox)
		.ToolTipText(LOCTEXT("SourceChainOptionsToolTip", "Select source chain to map to this target chain."))
		.OptionsSource(&SourceChainOptions)
		.OnGenerateWidget_Lambda([](TSharedPtr<FString> InItem)
		{
			return SNew(STextBlock).Text(FText::FromString(*InItem.Get()));
		})
		.OnSelectionChanged_Lambda([this](TSharedPtr<FString> InString, ESelectInfo::Type SelectInfo)
		{
			const FName SourceChainName = FName(*InString.Get());
			for (TWeakObjectPtr<URetargetChainSettings> ChainMapSettings : ChainSettingsObjects)
			{
				Controller->AssetController->SetSourceChainForTargetChain(ChainMapSettings.Get(), SourceChainName);
			}
		})
		[
			SNew(STextBlock)
			.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
			.Text_Lambda([PrimaryChainSettingsObject]()
			{
				return FText::FromName(PrimaryChainSettingsObject->SourceChain);
			})
		]
	];

	// FK settings
	const FText FKNotMappedWarning = LOCTEXT( "FKDisabled_Label", "Disabled: No source chain specified." );
	const bool bFKEditingEnabled = bFirstChainFKSetup && !bIsFKSetupMultipleValues;
	AddSettingsSection(
		DetailBuilder,
		SettingsCategory,
		GET_MEMBER_NAME_STRING_CHECKED(FTargetChainSettings, FK),
		FName("FK"),
		LOCTEXT("FKGroup_Label", "FK"),
		FTargetChainFKSettings::StaticStruct(),
		GET_MEMBER_NAME_STRING_CHECKED(FTargetChainFKSettings, EnableFK),
		bFKEditingEnabled,
		FKNotMappedWarning);

	// IK settings
	const FText NoIKGoalWarning = LOCTEXT( "IKDisabled_Label", "Disabled: No IK Goal specified." );
	const FText IKNotConnectedWarning = LOCTEXT( "IKDisconnected_Label", "Disabled: IK Goal is not connected to any solvers." );
	const FText IKEnabledMultipleValuesWarning = LOCTEXT( "IKDisabledMultipleValues_Label", "Some selected chain(s) do not have an IK Goal." );
	const bool bIKIsConnected = bFirstChainIKConnected && !bIsIKConnectedMultipleValues;
	const bool bIKEditingEnabled = bFirstChainIKSetup && !bIsIKSetupMultipleValues;
	FText IKWarningToUse = FText();
	if (!bFKEditingEnabled)
	{
		IKWarningToUse = FKNotMappedWarning;
	}
	else if (!bIKEditingEnabled)
	{
		IKWarningToUse = NoIKGoalWarning;
	}
	else if (!bIKIsConnected)
	{
		IKWarningToUse = IKNotConnectedWarning;
	}
	
	AddSettingsSection(
		DetailBuilder,
		SettingsCategory,
		GET_MEMBER_NAME_STRING_CHECKED(FTargetChainSettings, IK),
		FName("IK"),
		LOCTEXT("IKGroup_Label", "IK"),
		FTargetChainIKSettings::StaticStruct(),
		GET_MEMBER_NAME_STRING_CHECKED(FTargetChainIKSettings, EnableIK),
		bIKEditingEnabled && bIKIsConnected,
		IKWarningToUse);

	// Plant settings
	AddSettingsSection(
		DetailBuilder,
		SettingsCategory,
		GET_MEMBER_NAME_STRING_CHECKED(FTargetChainSettings, SpeedPlanting),
		FName("Speed Planting"),
		LOCTEXT("PlantingGroup_Label", "Speed Planting"),
		FTargetChainSpeedPlantSettings::StaticStruct(),
		GET_MEMBER_NAME_STRING_CHECKED(FTargetChainSpeedPlantSettings, EnableSpeedPlanting),
		bIKEditingEnabled,
		IKWarningToUse);
}

void FRetargetChainSettingsCustomization::AddSettingsSection(
	const IDetailLayoutBuilder& DetailBuilder,
	IDetailCategoryBuilder& SettingsCategory,
	const FString& StructPropertyName,
	const FName& GroupName,
	const FText& LocalizedGroupName,
	const UScriptStruct* SettingsClass,
	const FString& EnabledPropertyName,
	const bool& bIsSectionEnabled,
	const FText& DisabledMessage) const
{
	// create a group of all the properties in a settings struct,
	// with a single "Enable X" property in the header of the details row
	
	const FString SettingsPropertyPath = GET_MEMBER_NAME_STRING_CHECKED(URetargetChainSettings, Settings);
	const FString SettingsStructPropertyPath = FString::Printf(TEXT("%s.%s"), *SettingsPropertyPath, *StructPropertyName);
	const FString EnablePropertyPath = FString::Printf(TEXT("%s.%s"), *SettingsStructPropertyPath, *EnabledPropertyName);
	const TSharedRef<IPropertyHandle> IsEnabledProperty = DetailBuilder.GetProperty(*EnablePropertyPath);

	IDetailGroup& Group = SettingsCategory.AddGroup(GroupName, LocalizedGroupName, false, true);

	if (bIsSectionEnabled)
	{
		Group.HeaderProperty(IsEnabledProperty).CustomWidget()
			.NameContent()
			.VAlign(VAlign_Center)
			[
				SNew( STextBlock )
				.Text( FText::FromName(GroupName) )
				.Font( FAppStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
				.ToolTipText( IsEnabledProperty->GetToolTipText() )
			]
			.ValueContent()
			[
				SNew(SCheckBox)
				.ToolTipText(IsEnabledProperty->GetToolTipText())
				.IsChecked_Lambda([IsEnabledProperty]() -> ECheckBoxState
				{
					bool IsChecked;
					IsEnabledProperty.Get().GetValue(IsChecked);
					return IsChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([this, IsEnabledProperty](ECheckBoxState State)
				{
					IsEnabledProperty->SetValue(State == ECheckBoxState::Checked);
					Controller->HandleRetargeterNeedsInitialized(Controller->AssetController->GetAsset());
				})
			];
	}
	else
	{
		Group.HeaderRow()
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(LocalizedGroupName)
		]
		.ValueContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(DisabledMessage)
		];
	}
	
	const TAttribute<bool> EditCondition = TAttribute<bool>::Create([IsEnabledProperty, bIsSectionEnabled]()
	{
		bool bIsEnabledPropertyTrue = true;
		IsEnabledProperty->GetValue(bIsEnabledPropertyTrue);
		return bIsSectionEnabled && bIsEnabledPropertyTrue;
	});
	
	for (TFieldIterator<FProperty> It(SettingsClass); It; ++It)
	{
		if (It->GetName() == EnabledPropertyName)
		{
			continue;
		}
		const FString PropertyPath = FString::Printf(TEXT("%s.%s"), *SettingsStructPropertyPath, *It->GetName());
		Group.AddPropertyRow(DetailBuilder.GetProperty(*PropertyPath)).EditCondition(EditCondition, nullptr);
	}
}

void FRetargetRootSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized = DetailBuilder.GetSelectedObjects();
	if (ObjectsBeingCustomized.Num() < 0)
	{
		return;
	}
	
	RootSettingsObject = Cast<URetargetRootSettings>(ObjectsBeingCustomized[0].Get());
	if (!RootSettingsObject.IsValid())
	{
		return;
	}

	Controller = RootSettingsObject->EditorController;
	if (!Controller.IsValid())
	{
		return;
	}

	const FString StructPropertyPath = GET_MEMBER_NAME_STRING_CHECKED(URetargetRootSettings, Settings);

	auto AddPropertyRowToGroup = [&StructPropertyPath, &DetailBuilder](IDetailGroup& Group, const FString& PropertyName)
	{
		const FString PropertyPath = FString::Printf(TEXT("%s.%s"), *StructPropertyPath, *PropertyName);
		Group.AddPropertyRow(DetailBuilder.GetProperty(*PropertyPath));
	};

	DetailBuilder.HideCategory("Settings");

	// replace the "Settings" struct property
	IDetailCategoryBuilder& SettingsCategory = DetailBuilder.EditCategory(FName("Root Settings"));

	// alpha group
	IDetailGroup& AlphaGroup = SettingsCategory.AddGroup("Alpha", LOCTEXT("Alpha_Label", "Alpha"), false, true);
	AddPropertyRowToGroup(AlphaGroup,GET_MEMBER_NAME_STRING_CHECKED(FTargetRootSettings, RotationAlpha));
	AddPropertyRowToGroup(AlphaGroup,GET_MEMBER_NAME_STRING_CHECKED(FTargetRootSettings, TranslationAlpha));

	// offset group
	IDetailGroup& OffsetGroup = SettingsCategory.AddGroup("Offsets", LOCTEXT("OffsetRoot_Label", "Offsets"), false, true);
	AddPropertyRowToGroup(OffsetGroup,GET_MEMBER_NAME_STRING_CHECKED(FTargetRootSettings, TranslationOffset));
	AddPropertyRowToGroup(OffsetGroup,GET_MEMBER_NAME_STRING_CHECKED(FTargetRootSettings, RotationOffset));

	// scale group
	IDetailGroup& ScaleGroup = SettingsCategory.AddGroup("Scale Translation", LOCTEXT("ScaleRoot_Label", "Scale Translation"), false, true);
	AddPropertyRowToGroup(ScaleGroup,GET_MEMBER_NAME_STRING_CHECKED(FTargetRootSettings, ScaleHorizontal));
	AddPropertyRowToGroup(ScaleGroup,GET_MEMBER_NAME_STRING_CHECKED(FTargetRootSettings, ScaleVertical));

	// blend to source group
	IDetailGroup& BlendToSourceGroup = SettingsCategory.AddGroup("Blend To Source", LOCTEXT("BlendToSource_Label", "Blend to Source"), false, true);
	AddPropertyRowToGroup(BlendToSourceGroup,GET_MEMBER_NAME_STRING_CHECKED(FTargetRootSettings, BlendToSource));
	AddPropertyRowToGroup(BlendToSourceGroup,GET_MEMBER_NAME_STRING_CHECKED(FTargetRootSettings, BlendToSourceWeights));

	// affect IK directionally
	IDetailGroup& AffectIKGroup = SettingsCategory.AddGroup("Affect IK", LOCTEXT("AffectIK_Label", "Affect IK"), false, true);
	AddPropertyRowToGroup(AffectIKGroup,GET_MEMBER_NAME_STRING_CHECKED(FTargetRootSettings, AffectIKHorizontal));
	AddPropertyRowToGroup(AffectIKGroup,GET_MEMBER_NAME_STRING_CHECKED(FTargetRootSettings, AffectIKVertical));
}

void FRetargetGlobalSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized = DetailBuilder.GetSelectedObjects();
	if (ObjectsBeingCustomized.Num() < 0)
	{
		return;
	}
	
	GlobalSettingsObject = Cast<UIKRetargetGlobalSettings>(ObjectsBeingCustomized[0].Get());
	if (!GlobalSettingsObject.IsValid())
	{
		return;
	}

	Controller = GlobalSettingsObject->EditorController;
	if (!Controller.IsValid())
	{
		return;
	}

	const FString StructPropertyPath = GET_MEMBER_NAME_STRING_CHECKED(URetargetRootSettings, Settings);

	auto GetPropertyHandle = [&DetailBuilder, &StructPropertyPath](const FString& PropertyName)
	{
		const FString EnableWarpingPropertyName = GET_MEMBER_NAME_STRING_CHECKED(FRetargetGlobalSettings, bWarping);
		const FString PropertyPath = FString::Printf(TEXT("%s.%s"), *StructPropertyPath, *PropertyName);
		return DetailBuilder.GetProperty(*PropertyPath);
	};

	DetailBuilder.HideCategory("Settings");

	// replace the "Settings" struct property
	IDetailCategoryBuilder& SettingsCategory = DetailBuilder.EditCategory(FName("Global Settings"));

	// phases group
	IDetailGroup& AlphaGroup = SettingsCategory.AddGroup("Phases", LOCTEXT("Phases_Label", "Global Retarget Phases"), false, true);
	AlphaGroup.AddPropertyRow(GetPropertyHandle(GET_MEMBER_NAME_STRING_CHECKED(FRetargetGlobalSettings, bEnableRoot)));
	AlphaGroup.AddPropertyRow(GetPropertyHandle(GET_MEMBER_NAME_STRING_CHECKED(FRetargetGlobalSettings, bEnableFK)));
	AlphaGroup.AddPropertyRow(GetPropertyHandle(GET_MEMBER_NAME_STRING_CHECKED(FRetargetGlobalSettings, bEnableIK)));

	// stride warping group
	const FText WarpingTitleLabel = LOCTEXT("Warping_Label", "Stride Warping");
	IDetailGroup& WarpingGroup = SettingsCategory.AddGroup("Warping", WarpingTitleLabel, false, true);

	// header with "enable" checkbox that disables whole group
	const TSharedRef<IPropertyHandle> EnableWarpingPropertyHandle = GetPropertyHandle(GET_MEMBER_NAME_STRING_CHECKED(FRetargetGlobalSettings, bWarping));
	WarpingGroup.HeaderProperty(EnableWarpingPropertyHandle).DisplayName(WarpingTitleLabel);
	
	const TAttribute<bool> EditCondition = TAttribute<bool>::Create([EnableWarpingPropertyHandle]()
	{
		bool bIsEnabledPropertyTrue = true;
		EnableWarpingPropertyHandle->GetValue(bIsEnabledPropertyTrue);
		return bIsEnabledPropertyTrue;
	});
	
	// add all the warping parameters
	WarpingGroup.AddPropertyRow(GetPropertyHandle(GET_MEMBER_NAME_STRING_CHECKED(FRetargetGlobalSettings, ForwardDirection))).EditCondition(EditCondition,nullptr);
	WarpingGroup.AddPropertyRow(GetPropertyHandle(GET_MEMBER_NAME_STRING_CHECKED(FRetargetGlobalSettings, DirectionSource))).EditCondition(EditCondition,nullptr);

	// get target chain options for combobox
	TargetChainOptions.Reset();
	TargetChainOptions.Add(MakeShareable(new FString(TEXT("None"))));
	if (const UIKRigDefinition* TargetIKRig = Controller->AssetController->GetIKRig(ERetargetSourceOrTarget::Target))
	{
		const TArray<FBoneChain>& Chains = TargetIKRig->GetRetargetChains();
		for (const FBoneChain& BoneChain : Chains)
		{
			TargetChainOptions.Add(MakeShareable(new FString(BoneChain.ChainName.ToString())));
		}
	}
	
	// add row to target chain to use for body direction
	const TSharedRef<IPropertyHandle> DirectionChainPropertyHandle = GetPropertyHandle(GET_MEMBER_NAME_STRING_CHECKED(FRetargetGlobalSettings, DirectionChain));
	WarpingGroup.AddPropertyRow(DirectionChainPropertyHandle).CustomWidget()
	.IsEnabled(GlobalSettingsObject->Settings.DirectionSource == EWarpingDirectionSource::Chain)
	.NameContent()
	[
		SNew(SBox)
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT( "DirectionChainName_Label", "Direction Chain" ))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
	]
	.ValueContent()
	.MinDesiredWidth(166.0f)
	[
		SNew(SSearchableComboBox)
		.ToolTipText(LOCTEXT("DirectionChainOptionsToolTip", "Select target chain (usually the Spine) to help define the forward direction of the character. This is especially useful for creatures with a horizontal body, like quadrupeds."))
		.OptionsSource(&TargetChainOptions)
		.OnGenerateWidget_Lambda([](TSharedPtr<FString> InItem)
		{
			return SNew(STextBlock).Text(FText::FromString(*InItem.Get()));
		})
		.OnSelectionChanged_Lambda([this](TSharedPtr<FString> InString, ESelectInfo::Type SelectInfo)
		{
			const FName TargetChainName = FName(*InString.Get());
			GlobalSettingsObject->Settings.DirectionChain = TargetChainName;
		})
		[
			SNew(STextBlock)
			.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
			.Text_Lambda([this]()
			{
				return FText::FromName(GlobalSettingsObject->Settings.DirectionChain);
			})
		]
	];
	
	WarpingGroup.AddPropertyRow(GetPropertyHandle(GET_MEMBER_NAME_STRING_CHECKED(FRetargetGlobalSettings, WarpForwards))).EditCondition(EditCondition,nullptr);
	WarpingGroup.AddPropertyRow(GetPropertyHandle(GET_MEMBER_NAME_STRING_CHECKED(FRetargetGlobalSettings, WarpSplay))).EditCondition(EditCondition,nullptr);
	WarpingGroup.AddPropertyRow(GetPropertyHandle(GET_MEMBER_NAME_STRING_CHECKED(FRetargetGlobalSettings, SidewaysOffset))).EditCondition(EditCondition,nullptr);
}

#undef LOCTEXT_NAMESPACE

