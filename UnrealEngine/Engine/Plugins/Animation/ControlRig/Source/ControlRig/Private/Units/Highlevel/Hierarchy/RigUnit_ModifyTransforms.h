// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "Units/Highlevel/RigUnit_HighlevelBase.h"
#include "RigUnit_ModifyTransforms.generated.h"

UENUM()
enum class EControlRigModifyBoneMode : uint8
{
	/** Override existing local transform */
	OverrideLocal,

	/** Override existing global transform */
	OverrideGlobal,

	/** 
	 * Additive to existing local transform.
	 * Input transform is added within the bone's space.
	 */
	AdditiveLocal,

	/**
     * Additive to existing global transform.
     * Input transform is added as a global offset in the root of the hierarchy.
	 */
	AdditiveGlobal,

	/** MAX - invalid */
	Max UMETA(Hidden),
};

USTRUCT()
struct CONTROLRIG_API FRigUnit_ModifyTransforms_PerItem
{
	GENERATED_BODY()

	FRigUnit_ModifyTransforms_PerItem()
		: Item(FRigElementKey(NAME_None, ERigElementType::Bone))
	{
	}

	/**
	 * The item to set the transform for.
	 */
	UPROPERTY(EditAnywhere, meta = (Input, ExpandByDefault), Category = FRigUnit_ModifyTransforms_PerItem)
	FRigElementKey Item;

	/**
	 * The transform value to set for the given Bone.
	 */
	UPROPERTY(EditAnywhere, meta = (Input), Category = FRigUnit_ModifyTransforms_PerItem)
	FTransform Transform;
};

USTRUCT()
struct CONTROLRIG_API FRigUnit_ModifyTransforms_WorkData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FCachedRigElement> CachedItems;
};

/**
 * Modify Transforms is used to perform a change in the hierarchy by setting one or more bones' transforms
 */
USTRUCT(meta=(DisplayName="Modify Transforms", Category="Transforms", DocumentationPolicy="Strict", Keywords = "ModifyBone"))
struct CONTROLRIG_API FRigUnit_ModifyTransforms : public FRigUnit_HighlevelBaseMutable
{
	GENERATED_BODY()

	FRigUnit_ModifyTransforms()
		: Weight(1.f)
		, WeightMinimum(0.f)
		, WeightMaximum(1.f)
		, Mode(EControlRigModifyBoneMode::AdditiveLocal)
	{
		ItemToModify.Add(FRigUnit_ModifyTransforms_PerItem());
		ItemToModify[0].Item = FRigElementKey(NAME_None, ERigElementType::Bone);
	}

	virtual FRigElementKey DetermineSpaceForPin(const FString& InPinPath, void* InUserContext) const override
	{
		if (InPinPath.StartsWith(TEXT("ItemToModify")))
		{
			int32 Index = INDEX_NONE;
			FString Left, Middle, Right;
			if (InPinPath.Replace(TEXT("["), TEXT(".")).Split(TEXT("."), &Left, &Middle))
			{
				if (Middle.Replace(TEXT("]"), TEXT(".")).Split(TEXT("."), &Left, &Right))
				{
					Index = FCString::Atoi(*Left);
				}
			}

			if (ItemToModify.IsValidIndex(Index))
			{
				if (Mode == EControlRigModifyBoneMode::AdditiveLocal || Mode == EControlRigModifyBoneMode::OverrideLocal)
				{
					if (const URigHierarchy* Hierarchy = (const URigHierarchy*)InUserContext)
					{
						return Hierarchy->GetFirstParent(ItemToModify[Index].Item);
					}
				}
			}
		}
		return FRigElementKey();
	}

	virtual FTransform DetermineOffsetTransformForPin(const FString& InPinPath, void* InUserContext) const override
	{
		if (InPinPath.StartsWith(TEXT("ItemToModify")))
		{
			int32 Index = INDEX_NONE;
			FString Left, Middle, Right;
			if (InPinPath.Replace(TEXT("["), TEXT(".")).Split(TEXT("."), &Left, &Middle))
			{
				if (Middle.Replace(TEXT("]"), TEXT(".")).Split(TEXT("."), &Left, &Right))
				{
					Index = FCString::Atoi(*Left);
				}
			}

			if (ItemToModify.IsValidIndex(Index))
			{
				if (ItemToModify[Index].Item.IsValid())
				{
					if (const URigHierarchy* Hierarchy = (const URigHierarchy*)InUserContext)
					{
						if (Mode == EControlRigModifyBoneMode::AdditiveLocal)
						{
							// in the case of AdditiveLocal, we want the transient control's global transform to match the item's global transform
							// we know: transient control's global = transform value * offset transform * parent transform
							// we know: item's global transform = item's local transform from rig graph evaluation * its parent transform
							// we ensure: the item and the transient control share the same parent
							// so from our goal, transient control's global == item's global transform
							// we get: transform value * offset transform * parent transform == item's local transform from rig graph evaluation * its parent transform
							// we get: transform value * offset transform == item's local transform from rig graph evaluation
							// we get: offset transform == transform value.inverse * item's local transform from rig graph evaluation
							return ItemToModify[Index].Transform.Inverse() * Hierarchy->GetLocalTransform(ItemToModify[Index].Item);
						}
					}
				}
			}
		}

		return FTransform::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/**
	 * The items to modify.
	 */
	UPROPERTY(meta = (Input, ExpandByDefault, DefaultArraySize=1))
	TArray<FRigUnit_ModifyTransforms_PerItem> ItemToModify;

	/**
	 * At 1 this sets the transform, between 0 and 1 the transform is blended with previous results.
	 */
	UPROPERTY(meta = (Input, ClampMin=0.f, ClampMax=1.f, UIMin = 0.f, UIMax = 1.f))
	float Weight;

	/**
	 * The minimum of the weight - defaults to 0.0
	 */
	UPROPERTY(meta = (Input, Constant, ClampMin = 0.f, ClampMax = 1.f, UIMin = 0.f, UIMax = 1.f))
	float WeightMinimum;

	/**
	 * The maximum of the weight - defaults to 1.0
	 */
	UPROPERTY(meta = (Input, Constant, ClampMin = 0.f, ClampMax = 1.f, UIMin = 0.f, UIMax = 1.f))
	float WeightMaximum;

	/**
	 * Defines if the bone's transform should be set
	 * in local or global space, additive or override.
	 */
	UPROPERTY(meta = (Input, Constant))
	EControlRigModifyBoneMode Mode;

	// Used to cache the internally used bone index
	UPROPERTY(transient)
	FRigUnit_ModifyTransforms_WorkData WorkData;
};
