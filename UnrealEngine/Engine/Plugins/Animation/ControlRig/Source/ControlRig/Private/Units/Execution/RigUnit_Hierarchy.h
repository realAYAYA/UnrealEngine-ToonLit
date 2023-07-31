// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "ControlRigDefines.h" 
#include "RigUnit_Hierarchy.generated.h"

USTRUCT(meta = (Abstract, NodeColor="0.462745, 1,0, 0.329412", Category = "Hierarchy"))
struct CONTROLRIG_API FRigUnit_HierarchyBase : public FRigUnit
{
	GENERATED_BODY()
};

USTRUCT(meta = (Abstract, NodeColor="0.462745, 1,0, 0.329412", Category = "Hierarchy"))
struct CONTROLRIG_API FRigUnit_HierarchyBaseMutable : public FRigUnitMutable
{
	GENERATED_BODY()
};

/**
 * Returns the item's parent
 */
USTRUCT(meta=(DisplayName="Get Parent", Keywords="Child,Parent,Root,Up,Top", Varying))
struct CONTROLRIG_API FRigUnit_HierarchyGetParent : public FRigUnit_HierarchyBase
{
	GENERATED_BODY()

	FRigUnit_HierarchyGetParent()
	{
		Child = Parent = FRigElementKey(NAME_None, ERigElementType::Bone);
		CachedChild = CachedParent = FCachedRigElement();
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey Child;

	UPROPERTY(meta = (Output))
	FRigElementKey Parent;

	// Used to cache the internally
	UPROPERTY()
	FCachedRigElement CachedChild;

	// Used to cache the internally
	UPROPERTY()
	FCachedRigElement CachedParent;
};

/**
 * Returns the item's parents
 */
USTRUCT(meta=(DisplayName="Get Parents", Keywords="Chain,Parents,Hierarchy", Varying, Deprecated = "5.0"))
struct CONTROLRIG_API FRigUnit_HierarchyGetParents : public FRigUnit_HierarchyBase
{
	GENERATED_BODY()

	FRigUnit_HierarchyGetParents()
	{
		Child = FRigElementKey(NAME_None, ERigElementType::Bone);
		CachedChild = FCachedRigElement();
		Parents = CachedParents = FRigElementKeyCollection();
		bIncludeChild = false;
		bReverse = false;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey Child;

	UPROPERTY(meta = (Input))
	bool bIncludeChild;

	UPROPERTY(meta = (Input))
	bool bReverse;

	UPROPERTY(meta = (Output))
	FRigElementKeyCollection Parents;

	// Used to cache the internally
	UPROPERTY()
	FCachedRigElement CachedChild;

	// Used to cache the internally
	UPROPERTY()
	FRigElementKeyCollection CachedParents;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Returns the item's parents
 */
USTRUCT(meta=(DisplayName="Get Parents", Keywords="Chain,Parents,Hierarchy", Varying))
struct CONTROLRIG_API FRigUnit_HierarchyGetParentsItemArray : public FRigUnit_HierarchyBase
{
	GENERATED_BODY()

	FRigUnit_HierarchyGetParentsItemArray()
	{
		Child = FRigElementKey(NAME_None, ERigElementType::Bone);
		CachedChild = FCachedRigElement();
		CachedParents = FRigElementKeyCollection();
		bIncludeChild = false;
		bReverse = false;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey Child;

	UPROPERTY(meta = (Input))
	bool bIncludeChild;

	UPROPERTY(meta = (Input))
	bool bReverse;

	UPROPERTY(meta = (Output))
	TArray<FRigElementKey> Parents;

	// Used to cache the internally
	UPROPERTY()
	FCachedRigElement CachedChild;

	// Used to cache the internally
	UPROPERTY()
	FRigElementKeyCollection CachedParents;
};


/**
 * Returns the item's children
 */
USTRUCT(meta=(DisplayName="Get Children", Keywords="Chain,Children,Hierarchy", Deprecated = "4.25.0", Varying))
struct CONTROLRIG_API FRigUnit_HierarchyGetChildren : public FRigUnit_HierarchyBase
{
	GENERATED_BODY()

	FRigUnit_HierarchyGetChildren()
	{
		Parent = FRigElementKey(NAME_None, ERigElementType::Bone);
		CachedParent = FCachedRigElement();
		Children = CachedChildren = FRigElementKeyCollection();
		bIncludeParent = false;
		bRecursive = false;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey Parent;

	UPROPERTY(meta = (Input))
	bool bIncludeParent;

	UPROPERTY(meta = (Input))
	bool bRecursive;

	UPROPERTY(meta = (Output))
	FRigElementKeyCollection Children;

	// Used to cache the internally
	UPROPERTY()
	FCachedRigElement CachedParent;

	// Used to cache the internally
	UPROPERTY()
	FRigElementKeyCollection CachedChildren;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Returns the item's siblings
 */
USTRUCT(meta=(DisplayName="Get Siblings", Keywords="Chain,Siblings,Hierarchy", Varying, Deprecated = "5.0"))
struct CONTROLRIG_API FRigUnit_HierarchyGetSiblings : public FRigUnit_HierarchyBase
{
	GENERATED_BODY()

	FRigUnit_HierarchyGetSiblings()
	{
		Item = FRigElementKey(NAME_None, ERigElementType::Bone);
		CachedItem = FCachedRigElement();
		Siblings = CachedSiblings = FRigElementKeyCollection();
		bIncludeItem = false;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey Item;

	UPROPERTY(meta = (Input))
	bool bIncludeItem;

	UPROPERTY(meta = (Output))
	FRigElementKeyCollection Siblings;

	// Used to cache the internally
	UPROPERTY()
	FCachedRigElement CachedItem;

	// Used to cache the internally
	UPROPERTY()
	FRigElementKeyCollection CachedSiblings;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Returns the item's siblings
 */
USTRUCT(meta=(DisplayName="Get Siblings", Keywords="Chain,Siblings,Hierarchy", Varying))
struct CONTROLRIG_API FRigUnit_HierarchyGetSiblingsItemArray : public FRigUnit_HierarchyBase
{
	GENERATED_BODY()

	FRigUnit_HierarchyGetSiblingsItemArray()
	{
		Item = FRigElementKey(NAME_None, ERigElementType::Bone);
		CachedItem = FCachedRigElement();
		CachedSiblings = FRigElementKeyCollection();
		bIncludeItem = false;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey Item;

	UPROPERTY(meta = (Input))
	bool bIncludeItem;

	UPROPERTY(meta = (Output))
	TArray<FRigElementKey> Siblings;

	// Used to cache the internally
	UPROPERTY()
	FCachedRigElement CachedItem;

	// Used to cache the internally
	UPROPERTY()
	FRigElementKeyCollection CachedSiblings;
};

/**
 * Returns the hierarchy's pose
 */
USTRUCT(meta=(DisplayName="Get Pose Cache", Keywords="Hierarchy,Pose,State", Varying, Deprecated = "5.0"))
struct CONTROLRIG_API FRigUnit_HierarchyGetPose : public FRigUnit_HierarchyBase
{
	GENERATED_BODY()

	FRigUnit_HierarchyGetPose()
	{
		Initial = false;
		ElementType = ERigElementType::All;
		ItemsToGet = FRigElementKeyCollection();
		Pose = FRigPose();
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	bool Initial;

	UPROPERTY(meta = (Input))
	ERigElementType ElementType;

	// An optional collection to filter against
	UPROPERTY(meta = (Input))
	FRigElementKeyCollection ItemsToGet;

	UPROPERTY(meta = (Output))
	FRigPose Pose;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Returns the hierarchy's pose
 */
USTRUCT(meta=(DisplayName="Get Pose Cache", Keywords="Hierarchy,Pose,State", Varying, Category = "Pose Cache"))
struct CONTROLRIG_API FRigUnit_HierarchyGetPoseItemArray : public FRigUnit_HierarchyBase
{
	GENERATED_BODY()

	FRigUnit_HierarchyGetPoseItemArray()
	{
		Initial = false;
		ElementType = ERigElementType::All;
		Pose = FRigPose();
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	bool Initial;

	UPROPERTY(meta = (Input))
	ERigElementType ElementType;

	// An optional collection to filter against
	UPROPERTY(meta = (Input))
	TArray<FRigElementKey> ItemsToGet;

	UPROPERTY(meta = (Output))
	FRigPose Pose;
};


/**
 * Sets the hierarchy's pose
 */
USTRUCT(meta=(DisplayName="Apply Pose Cache", Keywords="Hierarchy,Pose,State", Varying, Deprecated = "5.0"))
struct CONTROLRIG_API FRigUnit_HierarchySetPose : public FRigUnit_HierarchyBaseMutable
{
	GENERATED_BODY()

	FRigUnit_HierarchySetPose()
	{
		Pose = FRigPose();
		ElementType = ERigElementType::All;
		Space = EBoneGetterSetterMode::LocalSpace;
		ItemsToSet = FRigElementKeyCollection();
		Weight = 1.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FRigPose Pose;

	UPROPERTY(meta = (Input))
	ERigElementType ElementType;

	UPROPERTY(meta = (Input))
	EBoneGetterSetterMode Space;

	// An optional collection to filter against
	UPROPERTY(meta = (Input))
	FRigElementKeyCollection ItemsToSet;

	UPROPERTY(meta = (Input))
	float Weight;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Sets the hierarchy's pose
 */
USTRUCT(meta=(DisplayName="Apply Pose Cache", Keywords="Hierarchy,Pose,State", Varying, Category = "Pose Cache"))
struct CONTROLRIG_API FRigUnit_HierarchySetPoseItemArray : public FRigUnit_HierarchyBaseMutable
{
	GENERATED_BODY()

	FRigUnit_HierarchySetPoseItemArray()
	{
		Pose = FRigPose();
		ElementType = ERigElementType::All;
		Space = EBoneGetterSetterMode::LocalSpace;
		Weight = 1.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FRigPose Pose;

	UPROPERTY(meta = (Input))
	ERigElementType ElementType;

	UPROPERTY(meta = (Input))
	EBoneGetterSetterMode Space;

	// An optional collection to filter against
	UPROPERTY(meta = (Input))
	TArray<FRigElementKey> ItemsToSet;

	UPROPERTY(meta = (Input))
	float Weight;
};

/**
* Returns true if the hierarchy pose is empty (has no items)
*/
USTRUCT(meta=(DisplayName="Is Pose Cache Empty", Keywords="Hierarchy,Pose,State", Varying, Category = "Pose Cache"))
struct CONTROLRIG_API FRigUnit_PoseIsEmpty : public FRigUnit_HierarchyBase
{
	GENERATED_BODY()

	FRigUnit_PoseIsEmpty()
	{
		Pose = FRigPose();
		IsEmpty = true;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FRigPose Pose;

	UPROPERTY(meta = (Output))
	bool IsEmpty;
};

/**
* Returns the items in the hierarchy pose
*/
USTRUCT(meta=(DisplayName="Get Pose Cache Items", Keywords="Hierarchy,Pose,State", Varying, Deprecated = "5.0"))
struct CONTROLRIG_API FRigUnit_PoseGetItems : public FRigUnit_HierarchyBase
{
	GENERATED_BODY()

	FRigUnit_PoseGetItems()
	{
		Pose = FRigPose();
		ElementType = ERigElementType::All;
		Items = FRigElementKeyCollection();
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FRigPose Pose;

	UPROPERTY(meta = (Input))
	ERigElementType ElementType;

	UPROPERTY(meta = (Output))
	FRigElementKeyCollection Items;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
* Returns the items in the hierarchy pose
*/
USTRUCT(meta=(DisplayName="Get Pose Cache Items", Keywords="Hierarchy,Pose,State", Varying, Category = "Pose Cache"))
struct CONTROLRIG_API FRigUnit_PoseGetItemsItemArray : public FRigUnit_HierarchyBase
{
	GENERATED_BODY()

	FRigUnit_PoseGetItemsItemArray()
	{
		Pose = FRigPose();
		ElementType = ERigElementType::All;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FRigPose Pose;

	UPROPERTY(meta = (Input))
	ERigElementType ElementType;

	UPROPERTY(meta = (Output))
	TArray<FRigElementKey> Items;
};

/**
* Compares two pose caches and compares their values.
*/
USTRUCT(meta=(DisplayName="Get Pose Cache Delta", Keywords="Hierarchy,Pose,State", Varying, Category = "Pose Cache"))
struct CONTROLRIG_API FRigUnit_PoseGetDelta : public FRigUnit_HierarchyBase
{
	GENERATED_BODY()

	FRigUnit_PoseGetDelta()
	{
		PoseA = PoseB = FRigPose();
		ElementType = ERigElementType::All;
		Space = EBoneGetterSetterMode::LocalSpace;
		ItemsToCompare = ItemsWithDelta = FRigElementKeyCollection();
		PositionThreshold = 0.1f;
		RotationThreshold = ScaleThreshold = CurveThreshold = 0.f;
		PosesAreEqual = false;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FRigPose PoseA;

	UPROPERTY(meta = (Input))
	FRigPose PoseB;

	// The delta threshold for a translation / position difference. 0.0 disables position differences.
	UPROPERTY(meta = (Input))
	float PositionThreshold;

	// The delta threshold for a rotation difference (in degrees). 0.0 disables rotation differences.
	UPROPERTY(meta = (Input))
	float RotationThreshold;

	// The delta threshold for a scale difference. 0.0 disables scale differences.
	UPROPERTY(meta = (Input))
	float ScaleThreshold;

	// The delta threshold for curve value difference. 0.0 disables curve differences.
	UPROPERTY(meta = (Input))
	float CurveThreshold;

	UPROPERTY(meta = (Input))
	ERigElementType ElementType;
	
	// Defines in which space transform deltas should be computed
	UPROPERTY(meta = (Input))
	EBoneGetterSetterMode Space;

	// An optional list of items to compare
	UPROPERTY(meta = (Input))
	FRigElementKeyCollection ItemsToCompare;

	UPROPERTY(meta = (Output))
	bool PosesAreEqual;

	UPROPERTY(meta = (Output))
	FRigElementKeyCollection ItemsWithDelta;

	static bool ArePoseElementsEqual(
		const FRigPoseElement& A,
		const FRigPoseElement& B,
		EBoneGetterSetterMode Space,
		float PositionU,
		float RotationU,
		float ScaleU,
		float CurveU);

	static bool AreTransformsEqual(
		const FTransform& A,
		const FTransform& B,
		float PositionU,
		float RotationU,
		float ScaleU);

	static bool AreCurvesEqual(
		float A,
		float B,
		float CurveU);
};

/**
* Returns the hierarchy's pose transform
*/
USTRUCT(meta=(DisplayName="Get Pose Cache Transform", Keywords="Hierarchy,Pose,State", Varying, Category = "Pose Cache"))
struct CONTROLRIG_API FRigUnit_PoseGetTransform : public FRigUnit_HierarchyBase
{
	GENERATED_BODY()

	FRigUnit_PoseGetTransform()
	{
		Pose = FRigPose();
		Item = FRigElementKey();
		Space = EBoneGetterSetterMode::GlobalSpace;
		Valid = false;
		Transform = FTransform::Identity;
		CurveValue = 0.f;
		CachedPoseElementIndex = CachedPoseHash = INDEX_NONE;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FRigPose Pose;

	UPROPERTY(meta = (Input))
	FRigElementKey Item;

	/**
	* Defines if the transform should be retrieved in local or global space
	*/ 
	UPROPERTY(meta = (Input))
	EBoneGetterSetterMode Space;

	UPROPERTY(meta = (Output))
	bool Valid;
	
	UPROPERTY(meta = (Output))
	FTransform Transform;

	UPROPERTY(meta = (Output))
	float CurveValue;

	UPROPERTY()
	int32 CachedPoseElementIndex;

	UPROPERTY()
	int32 CachedPoseHash;
};

/**
* Returns an array of transforms from a given hierarchy pose
*/
USTRUCT(meta=(DisplayName="Get Pose Cache Transform Array", Keywords="Hierarchy,Pose,State", Varying, Category = "Pose Cache"))
struct CONTROLRIG_API FRigUnit_PoseGetTransformArray : public FRigUnit_HierarchyBase
{
	GENERATED_BODY()

	FRigUnit_PoseGetTransformArray()
	{
		Pose = FRigPose();
		Space = EBoneGetterSetterMode::GlobalSpace;
		Valid = false;
		Transforms.Reset();
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FRigPose Pose;

	/**
	* Defines if the transform should be retrieved in local or global space
	*/ 
	UPROPERTY(meta = (Input))
	EBoneGetterSetterMode Space;

	UPROPERTY(meta = (Output))
	bool Valid;
	
	UPROPERTY(meta = (Output))
	TArray<FTransform> Transforms;
};

/**
* Returns the hierarchy's pose curve value
*/
USTRUCT(meta=(DisplayName="Get Pose Cache Curve", Keywords="Hierarchy,Pose,State", Varying, Category = "Pose Cache"))
struct CONTROLRIG_API FRigUnit_PoseGetCurve : public FRigUnit_HierarchyBase
{
	GENERATED_BODY()

	FRigUnit_PoseGetCurve()
	{
		Pose = FRigPose();
		Curve = NAME_None;
		Valid = false;
		CurveValue = 0.f;
		CachedPoseElementIndex = CachedPoseHash = INDEX_NONE;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FRigPose Pose;

	UPROPERTY(meta = (Input, CustomWidget = "CurveName"))
	FName Curve;

	UPROPERTY(meta = (Output))
	bool Valid;
	
	UPROPERTY(meta = (Output))
	float CurveValue;

	UPROPERTY()
	int32 CachedPoseElementIndex;

	UPROPERTY()
	int32 CachedPoseHash;
};

/**
* Given a pose, execute iteratively across all items in the pose
*/
USTRUCT(meta=(DisplayName="For Each Pose Cache Element", Keywords="Collection,Loop,Iterate", Icon="EditorStyle|GraphEditor.Macro.ForEach_16x", Category = "Pose Cache"))
struct CONTROLRIG_API FRigUnit_PoseLoop : public FRigUnit_HierarchyBaseMutable
{
	GENERATED_BODY()

	FRigUnit_PoseLoop()
	{
		Pose = FRigPose();
		Item = FRigElementKey();
		GlobalTransform = LocalTransform = FTransform::Identity;
		CurveValue = 0.f;
		Count = 0;
		Index = 0;
		Ratio = 0.f;
		Continue = false;
	}

	// FRigVMStruct overrides
	FORCEINLINE virtual bool IsForLoop() const override { return true; }
	FORCEINLINE virtual int32 GetNumSlices() const override { return Count; }

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FRigPose Pose;

	UPROPERTY(meta = (Singleton, Output))
	FRigElementKey Item;

	UPROPERTY(meta = (Singleton, Output))
	FTransform GlobalTransform;

	UPROPERTY(meta = (Singleton, Output))
	FTransform LocalTransform;

	UPROPERTY(meta = (Singleton, Output))
	float CurveValue;

	UPROPERTY(meta = (Singleton, Output))
	int32 Index;

	UPROPERTY(meta = (Singleton, Output))
	int32 Count;

	/**
	* Ranging from 0.0 (first item) and 1.0 (last item)
	* This is useful to drive a consecutive node with a 
	* curve or an ease to distribute a value.
	*/
	UPROPERTY(meta = (Singleton, Output))
	float Ratio;

	UPROPERTY(meta = (Singleton))
	bool Continue;

	UPROPERTY(meta = (Output))
	FControlRigExecuteContext Completed;
};
