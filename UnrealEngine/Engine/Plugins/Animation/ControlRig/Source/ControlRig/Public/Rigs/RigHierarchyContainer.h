// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigHierarchyDefines.h"
#include "RigHierarchyPose.h"
#include "RigBoneHierarchy.h"
#include "RigSpaceHierarchy.h"
#include "RigControlHierarchy.h"
#include "RigCurveContainer.h"
#include "RigHierarchyCache.h"
#include "RigInfluenceMap.h"
#include "RigHierarchyContainer.generated.h"

class UControlRig;
struct FRigHierarchyContainer;

USTRUCT()
struct CONTROLRIG_API FRigHierarchyContainer
{
public:

	GENERATED_BODY()

	FRigHierarchyContainer();
	FRigHierarchyContainer(const FRigHierarchyContainer& InOther);
	FRigHierarchyContainer& operator= (const FRigHierarchyContainer& InOther);

	UPROPERTY()
	FRigBoneHierarchy BoneHierarchy;

	UPROPERTY()
	FRigSpaceHierarchy SpaceHierarchy;

	UPROPERTY()
	FRigControlHierarchy ControlHierarchy;

	UPROPERTY()
	FRigCurveContainer CurveContainer;

private:

	TArray<FRigElementKey> ImportFromText(const FRigHierarchyCopyPasteContent& InData);

	friend class SRigHierarchy;
	friend class URigHierarchyController;
};

// this struct is still here for backwards compatibility - but not used anywhere
USTRUCT()
struct CONTROLRIG_API FRigHierarchyRef
{
	GENERATED_BODY()
};