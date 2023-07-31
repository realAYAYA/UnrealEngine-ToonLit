// Copyright Epic Games, Inc. All Rights Reserved.

#include "BoneProxy.h"
#include "IPersonaPreviewScene.h"
#include "BoneControllers/AnimNode_ModifyBone.h"
#include "AnimPreviewInstance.h"
#include "ScopedTransaction.h"
#include "Animation/DebugSkelMeshComponent.h"

#define LOCTEXT_NAMESPACE "BoneProxy"

UBoneProxy::UBoneProxy()
	: bLocalLocation(true)
	, bLocalRotation(true)
	, bLocalScale(true)
	, PreviousLocation(FVector::ZeroVector)
	, PreviousRotation(FRotator::ZeroRotator)
	, PreviousScale(FVector::ZeroVector)
	, bManipulating(false)
	, bIsTickable(false)
{
}

void UBoneProxy::Tick(float DeltaTime)
{
	if (!bManipulating)
	{
		if (UDebugSkelMeshComponent* Component = SkelMeshComponent.Get())
		{
			if (Component->GetSkeletalMeshAsset() && Component->GetSkeletalMeshAsset()->IsCompiling())
			{
				//We do not want to tick if the skeletalmesh is inside a compilation
				return;
			}

			TArray<FTransform> LocalBoneTransforms = Component->GetBoneSpaceTransforms();

			int32 BoneIndex = Component->GetBoneIndex(BoneName);
			if (LocalBoneTransforms.IsValidIndex(BoneIndex))
			{
				FTransform LocalTransform = LocalBoneTransforms[BoneIndex];
				FTransform BoneTransform = Component->GetBoneTransform(BoneIndex);

				if (bLocalLocation)
				{
					Location = LocalTransform.GetLocation();
				}
				else
				{
					Location = BoneTransform.GetLocation();
				}

				if (bLocalRotation)
				{
					Rotation = LocalTransform.GetRotation().Rotator();
				}
				else
				{
					Rotation = BoneTransform.GetRotation().Rotator();
				}

				if(bLocalScale)
				{
					Scale = LocalTransform.GetScale3D();
				}
				else
				{
					Scale = BoneTransform.GetScale3D();
				}

				FTransform ReferenceTransform = Component->GetSkeletalMeshAsset()->GetRefSkeleton().GetRefBonePose()[BoneIndex];
				ReferenceLocation = ReferenceTransform.GetLocation();
				ReferenceRotation = ReferenceTransform.GetRotation().Rotator();
				ReferenceScale = ReferenceTransform.GetScale3D();
			}

			// Show mesh relative transform on the details panel so we have a way to visualize the root transform when processing root motion
			// Note that this doesn't always represent the actual transform of the root in the animation at current time but where root motion has taken us so far
			// It will not match the root transform at the current time in the animation after lopping multiple times if we are using ProcessRootMotion::Loop
			// or if we are visualizing a complex section from a montage, for example
			const FTransform MeshRelativeTransform = Component->GetRelativeTransform();
			MeshLocation = MeshRelativeTransform.GetLocation();
			MeshRotation = MeshRelativeTransform.GetRotation().Rotator();
			MeshScale = MeshRelativeTransform.GetScale3D();
		}
	}
}

bool UBoneProxy::IsTickable() const
{
	return bIsTickable;
}

TStatId UBoneProxy::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBoneProxy, STATGROUP_Tickables);
}

TOptional<FVector::FReal> UBoneProxy::GetNumericValue(
	ESlateTransformComponent::Type Component,
	ESlateRotationRepresentation::Type Representation,
	ESlateTransformSubComponent::Type SubComponent,
	ETransformType TransformType) const
{
	const FVector* LocationPtr = &Location;
	const FRotator* RotationPtr = &Rotation;
	const FVector* ScalePtr = &Scale;

	switch(TransformType)
	{
		case TransformType_Reference:
		{
			LocationPtr = &ReferenceLocation;
			RotationPtr = &ReferenceRotation;
			ScalePtr = &ReferenceScale;
			break;
		}
		case TransformType_Mesh:
		{
			LocationPtr = &MeshLocation;
			RotationPtr = &MeshRotation;
			ScalePtr = &MeshScale;
			break;
		}
	}

	const FEulerTransform Transform(*RotationPtr, *LocationPtr, *ScalePtr);
	return SAdvancedTransformInputBox<FEulerTransform>::GetNumericValueFromTransform(
		Transform,
		Component,
		Representation,
		SubComponent
		);
}

TOptional<FVector::FReal> UBoneProxy::GetMultiNumericValue(
	ESlateTransformComponent::Type Component,
	ESlateRotationRepresentation::Type Representation,
	ESlateTransformSubComponent::Type SubComponent,
	ETransformType TransformType,
	TArrayView<UBoneProxy*> BoneProxies)
{
	TOptional<FVector::FReal> FirstValue = BoneProxies[0]->GetNumericValue(Component, Representation, SubComponent, TransformType);
	if(!FirstValue.IsSet())
	{
		return FirstValue;
	}

	for(int32 Index=1;Index<BoneProxies.Num();Index++)
	{
		TOptional<FVector::FReal> Value = BoneProxies[Index]->GetNumericValue(Component, Representation, SubComponent, TransformType);
		if(!Value.IsSet())
		{
			return Value;
		}
		if(!FMath::IsNearlyEqual(FirstValue.GetValue(), Value.GetValue()))
		{
			return TOptional<FVector::FReal>();
		}
	}

	return FirstValue;
}

void UBoneProxy::OnNumericValueCommitted(
	ESlateTransformComponent::Type Component,
	ESlateRotationRepresentation::Type Representation,
	ESlateTransformSubComponent::Type SubComponent,
	FVector::FReal Value, ETextCommit::Type CommitType,
	ETransformType TransformType,
	bool bIsCommit)
{
	if(TransformType != TransformType_Bone)
	{
		return;
	}

	switch(Component)
	{
		case ESlateTransformComponent::Location:
		{
			OnPreEditChange(GET_MEMBER_NAME_CHECKED(UBoneProxy, Location), bIsCommit);
				
			switch(SubComponent)
			{
				case ESlateTransformSubComponent::X:
				{
					Location.X = Value;
					break;
				}
				case ESlateTransformSubComponent::Y:
				{
					Location.Y = Value;
					break;
				}
				case ESlateTransformSubComponent::Z:
				{
					Location.Z = Value;
					break;
				}
			}

			OnPostEditChangeProperty(GET_MEMBER_NAME_CHECKED(UBoneProxy, Location), bIsCommit);
			break;
		}
		case ESlateTransformComponent::Rotation:
		{
			OnPreEditChange(GET_MEMBER_NAME_CHECKED(UBoneProxy, Rotation), bIsCommit);
				
			switch(SubComponent)
			{
				case ESlateTransformSubComponent::Roll:
				{
					Rotation.Roll = Value;
					break;
				}
				case ESlateTransformSubComponent::Pitch:
				{
					Rotation.Pitch = Value;
					break;
				}
				case ESlateTransformSubComponent::Yaw:
				{
					Rotation.Yaw = Value;
					break;
				}
			}

			OnPostEditChangeProperty(GET_MEMBER_NAME_CHECKED(UBoneProxy, Rotation), bIsCommit);
			break;
		}
		case ESlateTransformComponent::Scale:
		{
			OnPreEditChange(GET_MEMBER_NAME_CHECKED(UBoneProxy, Scale), bIsCommit);

			switch(SubComponent)
			{
				case ESlateTransformSubComponent::X:
				{
					Scale.X = Value;
					break;
				}
				case ESlateTransformSubComponent::Y:
				{
					Scale.Y = Value;
					break;
				}
				case ESlateTransformSubComponent::Z:
				{
					Scale.Z = Value;
					break;
				}
			}

			OnPostEditChangeProperty(GET_MEMBER_NAME_CHECKED(UBoneProxy, Scale), bIsCommit);
			break;
		}
	}
}

void UBoneProxy::OnMultiNumericValueCommitted(
	ESlateTransformComponent::Type Component,
	ESlateRotationRepresentation::Type Representation,
	ESlateTransformSubComponent::Type SubComponent,
	FVector::FReal Value,
	ETextCommit::Type CommitType,
	ETransformType TransformType,
	TArrayView<UBoneProxy*> BoneProxies,
	bool bIsCommit)
{
	for(UBoneProxy* BoneProxy : BoneProxies)
	{
		BoneProxy->OnNumericValueCommitted(Component, Representation, SubComponent, Value, CommitType, TransformType, bIsCommit);
	}
}

bool UBoneProxy::DiffersFromDefault(ESlateTransformComponent::Type Component, ETransformType TransformType) const
{
	if(TransformType == TransformType_Bone)
	{
		switch(Component)
		{
			case ESlateTransformComponent::Location:
			{
				return !Location.Equals(ReferenceLocation);
			}
			case ESlateTransformComponent::Rotation:
			{
				return !Rotation.Equals(ReferenceRotation);
			}
			case ESlateTransformComponent::Scale:
			{
				return !Scale.Equals(ReferenceScale);
			}
			default:
			{
				return DiffersFromDefault(ESlateTransformComponent::Location, TransformType) ||
					DiffersFromDefault(ESlateTransformComponent::Rotation, TransformType) ||
						DiffersFromDefault(ESlateTransformComponent::Scale, TransformType);
			}
		}
	}
	return false;
}

void UBoneProxy::ResetToDefault(ESlateTransformComponent::Type InComponent, ETransformType TransformType)
{
	if(TransformType == TransformType_Bone)
	{
		if (UDebugSkelMeshComponent* Component = SkelMeshComponent.Get())
		{
			if (Component->PreviewInstance && Component->AnimScriptInstance == Component->PreviewInstance)
			{
				int32 BoneIndex = Component->GetBoneIndex(BoneName);
				if (BoneIndex != INDEX_NONE && BoneIndex < Component->GetNumComponentSpaceTransforms())
				{
					Component->PreviewInstance->SetFlags(RF_Transactional);
					Component->PreviewInstance->Modify();

					FAnimNode_ModifyBone& ModifyBone = Component->PreviewInstance->ModifyBone(BoneName);

					switch(InComponent)
					{
						case ESlateTransformComponent::Location:
						{
							ModifyBone.Translation = ReferenceLocation;
							break;
						}
						case ESlateTransformComponent::Rotation:
						{
							ModifyBone.Rotation = ReferenceRotation;
							break;
						}
						case ESlateTransformComponent::Scale:
						{
							ModifyBone.Scale = ReferenceScale;
							break;
						}
						default:
						{
							ModifyBone.Translation = ReferenceLocation;
							ModifyBone.Rotation = ReferenceRotation;
							ModifyBone.Scale = ReferenceScale;
							break;
						}
					}

					if(ModifyBone.Translation.Equals(ReferenceLocation) &&
						ModifyBone.Rotation.Equals(ReferenceRotation) &&
						ModifyBone.Scale.Equals(ReferenceScale))
					{
						Component->PreviewInstance->RemoveBoneModification(BoneName);
					}
				}
			}
		}
	}
}

void UBoneProxy::PreEditChange(FEditPropertyChain& PropertyAboutToChange)
{
	if (UDebugSkelMeshComponent* Component = SkelMeshComponent.Get())
	{
		if (Component->PreviewInstance && Component->AnimScriptInstance == Component->PreviewInstance)
		{
			bManipulating = true;

			Component->PreviewInstance->Modify();

			if (PropertyAboutToChange.GetActiveMemberNode()->GetValue()->GetFName() == GET_MEMBER_NAME_CHECKED(UBoneProxy, Location))
			{
				PreviousLocation = Location;
			}
			else if (PropertyAboutToChange.GetActiveMemberNode()->GetValue()->GetFName() == GET_MEMBER_NAME_CHECKED(UBoneProxy, Rotation))
			{
				PreviousRotation = Rotation;
			}
			else if (PropertyAboutToChange.GetActiveMemberNode()->GetValue()->GetFName() == GET_MEMBER_NAME_CHECKED(UBoneProxy, Scale))
			{
				PreviousScale = Scale;
			}
		}
	}
}

void UBoneProxy::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if(PropertyChangedEvent.Property != nullptr)
	{
		if (UDebugSkelMeshComponent* Component = SkelMeshComponent.Get())
		{
			if (Component->PreviewInstance && Component->AnimScriptInstance == Component->PreviewInstance)
			{
				bManipulating = (PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive);

				int32 BoneIndex = Component->GetBoneIndex(BoneName);
				if (BoneIndex != INDEX_NONE && BoneIndex < Component->GetNumComponentSpaceTransforms())
				{
					FTransform BoneTransform = Component->GetBoneTransform(BoneIndex);
					FMatrix BoneLocalCoordSystem = Component->GetBoneTransform(BoneIndex).ToMatrixNoScale().RemoveTranslation();
					Component->PreviewInstance->SetFlags(RF_Transactional);
					Component->PreviewInstance->Modify();
					FAnimNode_ModifyBone& ModifyBone = Component->PreviewInstance->ModifyBone(BoneName);
					FTransform ModifyBoneTransform(ModifyBone.Rotation, ModifyBone.Translation, ModifyBone.Scale);
					FTransform BaseTransform = BoneTransform.GetRelativeTransformReverse(ModifyBoneTransform);

					if (PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UBoneProxy, Location))
					{
						FVector Delta = (Location - PreviousLocation);
						if (!Delta.IsNearlyZero())
						{
							if (bLocalLocation)
							{
								Delta = BoneLocalCoordSystem.TransformPosition(Delta);
							}

							FVector BoneSpaceDelta = BaseTransform.TransformVector(Delta);
							ModifyBone.Translation += BoneSpaceDelta;
						}
					}
					else if (PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UBoneProxy, Rotation))
					{
						FRotator Delta = (Rotation - PreviousRotation);
						if (!Delta.IsNearlyZero())
						{					
							if (bLocalRotation)
							{
								// get delta in current coord space
								Delta = (BoneLocalCoordSystem.Inverse() * FRotationMatrix(Delta) * BoneLocalCoordSystem).Rotator();
							}

							FVector RotAxis;
							float RotAngle;
							Delta.Quaternion().ToAxisAndAngle(RotAxis, RotAngle);

							FVector4 BoneSpaceAxis = BaseTransform.TransformVectorNoScale(RotAxis);

							//Calculate the new delta rotation
							FQuat NewDeltaQuat(BoneSpaceAxis, RotAngle);
							NewDeltaQuat.Normalize();

							FRotator NewRotation = (ModifyBoneTransform * FTransform(NewDeltaQuat)).Rotator();
							ModifyBone.Rotation = NewRotation;
						}
					}
					else if (PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UBoneProxy, Scale))
					{
						FVector Delta = (Scale - PreviousScale);
						if (!Delta.IsNearlyZero())
						{
							ModifyBone.Scale += Delta;
						}
					}
				}
			}
		}
	}
}

void UBoneProxy::OnPreEditChange(FName PropertyName, bool bIsCommit)
{
	FEditPropertyChain PropertyChain;
	FProperty* Property = FindFProperty<FProperty>(GetClass(), PropertyName);
	PropertyChain.AddHead(Property);
	PropertyChain.SetActiveMemberPropertyNode(Property);
	PreEditChange(PropertyChain);
}

void UBoneProxy::OnPostEditChangeProperty(FName PropertyName, bool bIsCommit)
{
	FProperty* Property = FindFProperty<FProperty>(GetClass(), PropertyName);
	FPropertyChangedEvent ChangedEvent(Property, bIsCommit ? EPropertyChangeType::Unspecified : EPropertyChangeType::Interactive);
	PostEditChangeProperty(ChangedEvent);
}

#undef LOCTEXT_NAMESPACE
