// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigHierarchyDefines.h"
#include "RigHierarchyPose.h"
#include "ReferenceSkeleton.h"
#include "RigBoneHierarchy.generated.h"

class UControlRig;
struct FRigHierarchyContainer;

USTRUCT()
struct CONTROLRIG_API FRigBone: public FRigElement
{
	GENERATED_BODY()

	FRigBone()
		: FRigElement()
		, ParentName(NAME_None)
		, ParentIndex(INDEX_NONE)
		, InitialTransform(FTransform::Identity)
		, GlobalTransform(FTransform::Identity)
		, LocalTransform(FTransform::Identity)
		, Dependents()
		, Type(ERigBoneType::Imported)
	{
	}
	virtual ~FRigBone() {}

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = FRigElement)
	FName ParentName;

	UPROPERTY(transient)
	int32 ParentIndex;

	/* Initial global transform that is saved in this rig */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = FRigElement)
	FTransform InitialTransform;

	UPROPERTY(BlueprintReadOnly, transient, EditAnywhere, Category = FRigElement)
	FTransform GlobalTransform;

	UPROPERTY(BlueprintReadOnly, transient, EditAnywhere, Category = FRigElement)
	FTransform LocalTransform;

	/** dependent list - direct dependent for child or anything that needs to update due to this */
	UPROPERTY(transient)
	TArray<int32> Dependents;

	/** the source of the bone to differentiate procedurally generated, imported etc */
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = FRigElement)
	ERigBoneType Type;

	FORCEINLINE virtual ERigElementType GetElementType() const override
	{
		return ERigElementType::Bone;
	}

	FORCEINLINE virtual FRigElementKey GetParentElementKey(bool bForce = false) const
	{
		return FRigElementKey(ParentName, GetElementType());
	}
};

USTRUCT()
struct CONTROLRIG_API FRigBoneHierarchy
{
	GENERATED_BODY()

	FRigBoneHierarchy();

	FORCEINLINE TArray<FRigBone>::RangedForIteratorType      begin()       { return Bones.begin(); }
	FORCEINLINE TArray<FRigBone>::RangedForConstIteratorType begin() const { return Bones.begin(); }
	FORCEINLINE TArray<FRigBone>::RangedForIteratorType      end()         { return Bones.end();   }
	FORCEINLINE TArray<FRigBone>::RangedForConstIteratorType end() const   { return Bones.end();   }

	FRigBone& Add(const FName& InNewName, const FName& InParentName, ERigBoneType InType, const FTransform& InInitTransform, const FTransform& InLocalTransform, const FTransform& InGlobalTransform);
	
private:

	// disable copy constructor
	FRigBoneHierarchy(const FRigBoneHierarchy& InOther) {}

	UPROPERTY(EditAnywhere, Category = FRigBoneHierarchy)
	TArray<FRigBone> Bones;

	friend struct FRigHierarchyContainer;
	friend struct FCachedRigElement;
	friend class UControlRigHierarchyModifier;
	friend class UControlRig;
};
