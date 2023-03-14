// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigHierarchyDefines.h"
#include "RigHierarchyPose.h"
#include "RigSpaceHierarchy.generated.h"

class UControlRig;
struct FRigHierarchyContainer;

UENUM(BlueprintType)
enum class ERigSpaceType : uint8
{
	/** Not attached to anything */
	Global,

	/** Attached to a bone */
	Bone,

	/** Attached to a control */
	Control,

	/** Attached to a space*/
	Space
};

USTRUCT()
struct CONTROLRIG_API FRigSpace : public FRigElement
{
	GENERATED_BODY()

	FRigSpace()
		: FRigElement()
		, SpaceType(ERigSpaceType::Global)
		, ParentName(NAME_None)
		, ParentIndex(INDEX_NONE)
		, InitialTransform(FTransform::Identity)
		, LocalTransform(FTransform::Identity)
	{
	}
	virtual ~FRigSpace() {}

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = FRigElement)
	ERigSpaceType SpaceType;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = FRigElement)
	FName ParentName;

	UPROPERTY(BlueprintReadOnly, transient, Category = FRigElement)
	int32 ParentIndex;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = FRigElement)
	FTransform InitialTransform;

	UPROPERTY(BlueprintReadOnly, transient, EditAnywhere, Category = FRigElement)
	FTransform LocalTransform;

	FORCEINLINE virtual ERigElementType GetElementType() const override
	{
		return ERigElementType::Null;
	}

	FORCEINLINE virtual FRigElementKey GetParentElementKey() const
	{
		switch (SpaceType)
		{
			case ERigSpaceType::Bone:
			{
				return FRigElementKey(ParentName, ERigElementType::Bone);
			}
			case ERigSpaceType::Control:
			{
				return FRigElementKey(ParentName, ERigElementType::Control);
			}
			case ERigSpaceType::Space:
			{
				return FRigElementKey(ParentName, ERigElementType::Null);
			}
			default:
			{
				break;
			}
		}
		return FRigElementKey();
	}
};

USTRUCT()
struct CONTROLRIG_API FRigSpaceHierarchy
{
	GENERATED_BODY()

	FRigSpaceHierarchy();

	FORCEINLINE TArray<FRigSpace>::RangedForIteratorType      begin()       { return Spaces.begin(); }
	FORCEINLINE TArray<FRigSpace>::RangedForConstIteratorType begin() const { return Spaces.begin(); }
	FORCEINLINE TArray<FRigSpace>::RangedForIteratorType      end()         { return Spaces.end();   }
	FORCEINLINE TArray<FRigSpace>::RangedForConstIteratorType end() const   { return Spaces.end();   }

	FRigSpace& Add(const FName& InNewName, ERigSpaceType InSpaceType, const FName& InParentName, const FTransform& InTransform);
	
private:

	// disable copy constructor
	FRigSpaceHierarchy(const FRigSpaceHierarchy& InOther) {}

	UPROPERTY(EditAnywhere, Category = FRigSpaceHierarchy)
	TArray<FRigSpace> Spaces;

	friend struct FRigHierarchyContainer;
	friend struct FCachedRigElement;
	friend class UControlRigHierarchyModifier;
	friend class UControlRig;
};
