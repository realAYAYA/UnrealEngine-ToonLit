// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/Quat.h"

class URigHierarchy;
struct FRigElementKey;

namespace FBIKUtil
{
	/** Utility functions */
	FVector FULLBODYIK_API GetScaledRotationAxis(const FQuat& InQuat);
	bool FULLBODYIK_API CanCrossProduct(const FVector& Vector1, const FVector& Vector2);
	bool FULLBODYIK_API GetBoneChain(URigHierarchy* Hierarchy, const FRigElementKey& Root, const FRigElementKey& Current, TArray<FRigElementKey>& ChainIndices);
};



#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
