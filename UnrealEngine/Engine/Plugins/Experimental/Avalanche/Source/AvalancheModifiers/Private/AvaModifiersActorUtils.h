// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Math/Box.h"
#include "Math/OrientedBox.h"
#include "Math/TransformNonVectorized.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "AvaModifiersActorUtils.generated.h"

enum class EAvaAxis : uint8;
class AActor;

/** Axis to use */
UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true"))
enum class EAvaModifiersAxis : uint8
{
	None = 0 UMETA(Hidden),
	X = 1 << 0,
	Y = 1 << 1,
	Z = 1 << 2,
};
ENUM_CLASS_FLAGS(EAvaModifiersAxis);

// All operations that can be reused or shared in modifiers should go here
struct FAvaModifiersActorUtils
{
	/** Begin Bounds */
	static FBox GetActorsBounds(const TSet<TWeakObjectPtr<AActor>>& InActors, const FTransform& InReferenceTransform, bool bInSkipHidden = false);
	static FBox GetActorsBounds(AActor* InActor, bool bInIncludeChildren, bool bInSkipHidden = false);
	static FBox GetActorBounds(const AActor* InActor);
	static FOrientedBox GetOrientedBox(const FBox& InLocalBox, const FTransform& InWorldTransform);
	static FVector GetVectorAxis(int32 InAxis);
	static bool IsAxisVectorEquals(const FVector& InVectorA, const FVector& InVectorB, int32 InCompareAxis);
	/** End Bounds */

	/** Begin Outliner */
	static bool IsActorNotIsolated(const AActor* InActor);
	/** End Outliner */
	
	static FRotator FindLookAtRotation(const FVector& InEyePosition, const FVector& InTargetPosition, const EAvaAxis InAxis, const bool bInFlipAxis);
	static bool IsActorVisible(const AActor* InActor);
};