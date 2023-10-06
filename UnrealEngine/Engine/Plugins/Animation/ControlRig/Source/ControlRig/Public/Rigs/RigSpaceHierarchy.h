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

	virtual ERigElementType GetElementType() const override
	{
		return ERigElementType::Null;
	}

	virtual FRigElementKey GetParentElementKey() const
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

	TArray<FRigSpace>::RangedForIteratorType      begin()       { return Spaces.begin(); }
	TArray<FRigSpace>::RangedForConstIteratorType begin() const { return Spaces.begin(); }
	TArray<FRigSpace>::RangedForIteratorType      end()         { return Spaces.end();   }
	TArray<FRigSpace>::RangedForConstIteratorType end() const   { return Spaces.end();   }

	FRigSpace& Add(const FName& InNewName, ERigSpaceType InSpaceType, const FName& InParentName, const FTransform& InTransform);

	// Pretty weird that this type is copy/move assignable (needed for USTRUCTs) but not copy/move constructible
	FRigSpaceHierarchy(FRigSpaceHierarchy&& InOther) = delete;
	FRigSpaceHierarchy(const FRigSpaceHierarchy& InOther) = delete;
	FRigSpaceHierarchy& operator=(FRigSpaceHierarchy&& InOther) = default;
	FRigSpaceHierarchy& operator=(const FRigSpaceHierarchy& InOther) = default;

private:
	UPROPERTY(EditAnywhere, Category = FRigSpaceHierarchy)
	TArray<FRigSpace> Spaces;

	friend struct FRigHierarchyContainer;
	friend struct FCachedRigElement;
	friend class UControlRigHierarchyModifier;
	friend class UControlRig;
};
