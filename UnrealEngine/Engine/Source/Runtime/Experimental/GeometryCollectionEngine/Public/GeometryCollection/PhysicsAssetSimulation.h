// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Chaos/ChaosNotifyHandlerInterface.h"
#include "GeometryCollection/GeometryCollectionSimulationTypes.h"
#include "PhysicsInterfaceDeclaresCore.h"

class AActor;
class FSkeletalMeshPhysicsProxy;
struct FSkeletalMeshPhysicsProxyParams;
class UChaosPhysicalMaterial;
class UPhysicsAsset;
class USkeletalMeshComponent;


struct GEOMETRYCOLLECTIONENGINE_API FPhysicsAssetSimulationUtil
{
	static void BuildParams(const UObject* Caller, const AActor* OwningActor, const USkeletalMeshComponent* SkelMeshComponent, const UPhysicsAsset* PhysicsAsset, FSkeletalMeshPhysicsProxyParams& OutParams);
	static bool UpdateAnimState(const UObject* Caller, const AActor* OwningActor, const USkeletalMeshComponent* SkelMeshComponent, const float Dt, FSkeletalMeshPhysicsProxyParams& OutParams);
};
