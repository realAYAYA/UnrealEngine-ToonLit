// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/Box.h"

class AActor;
class APCGWorldActor;
class ALandscape;
class ALandscapeProxy;
class UPCGComponent;
class UWorld;

namespace PCGHelpers
{
	/** Tag that will be added on every component generated through the PCG system */
	const FName DefaultPCGTag = TEXT("PCG Generated Component");
	const FName DefaultPCGDebugTag = TEXT("PCG Generated Debug Component");
	const FName DefaultPCGActorTag = TEXT("PCG Generated Actor");
	const FName MarkedForCleanupPCGTag = TEXT("PCG Marked For Cleanup");

	PCG_API int ComputeSeed(int A);
	PCG_API int ComputeSeed(int A, int B);
	PCG_API int ComputeSeed(int A, int B, int C);

	PCG_API bool IsInsideBounds(const FBox& InBox, const FVector& InPosition);
	PCG_API bool IsInsideBoundsXY(const FBox& InBox, const FVector& InPosition);

	PCG_API FBox OverlapBounds(const FBox& InBoxA, const FBox& InBoxB);

	/** Returns the bounds of InActor, intersected with the component if InActor is a partition actor */
	PCG_API FBox GetGridBounds(const AActor* InActor, const UPCGComponent* InComponent);

	PCG_API FBox GetActorBounds(const AActor* InActor, bool bIgnorePCGCreatedComponents = true);
	PCG_API FBox GetActorLocalBounds(const AActor* InActor, bool bIgnorePCGCreatedComponents = true);
	PCG_API FBox GetLandscapeBounds(const ALandscapeProxy* InLandscape);

	PCG_API ALandscape* GetLandscape(UWorld* InWorld, const FBox& InActorBounds);
	PCG_API TArray<TWeakObjectPtr<ALandscapeProxy>> GetLandscapeProxies(UWorld* InWorld, const FBox& InActorBounds);
	PCG_API TArray<TWeakObjectPtr<ALandscapeProxy>> GetAllLandscapeProxies(UWorld* InWorld);

	PCG_API bool IsRuntimeOrPIE();

	PCG_API APCGWorldActor* GetPCGWorldActor(UWorld* InWorld);

	PCG_API TArray<FString> GetStringArrayFromCommaSeparatedString(const FString& InCommaSeparatedString);

#if WITH_EDITOR
	PCG_API void GatherDependencies(UObject* Object, TSet<TObjectPtr<UObject>>& OutDependencies, int32 MaxDepth = -1);
	PCG_API void GatherDependencies(FProperty* Property, const void* InContainer, TSet<TObjectPtr<UObject>>& OutDependencies, int32 MaxDepth);
#endif
};
