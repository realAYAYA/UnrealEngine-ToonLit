// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "PCGPoint.h"
#include "PCGContext.h"

#include "PCGBlueprintHelpers.generated.h"

class UPCGSettings;
class UPCGData;

UCLASS()
class PCG_API UPCGBlueprintHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "PCG|Helpers")
	static int ComputeSeedFromPosition(const FVector& InPosition);

	UFUNCTION(BlueprintCallable, Category = "PCG|Helpers")
	static void SetSeedFromPosition(UPARAM(ref) FPCGPoint& InPoint);

	/** Creates a random stream from a point's seed and settings/component's seed (optional) */
	UFUNCTION(BlueprintCallable, Category = "PCG|Helpers", meta = (ScriptMethod))
	static FRandomStream GetRandomStream(const FPCGPoint& InPoint, const UPCGSettings* OptionalSettings = nullptr, const UPCGComponent* OptionalComponent = nullptr);

	UFUNCTION(BlueprintCallable, Category = "PCG|Temporary", meta = (ScriptMethod))
	static UPCGData* GetActorData(UPARAM(ref) FPCGContext& Context);

	UFUNCTION(BlueprintCallable, Category = "PCG|Temporary", meta = (ScriptMethod))
	static UPCGData* GetInputData(UPARAM(ref) FPCGContext& Context);

	UFUNCTION(BlueprintCallable, Category = "PCG|Temporary", meta = (ScriptMethod))
	static TArray<UPCGData*> GetExclusionData(UPARAM(ref) FPCGContext& Context);

	UFUNCTION(BlueprintCallable, Category = "PCG|Temporary", meta = (ScriptMethod))
	static UPCGComponent* GetComponent(UPARAM(ref) FPCGContext& Context);

	UFUNCTION(BlueprintCallable, Category = "PCG|Temporary", meta = (ScriptMethod))
	static UPCGComponent* GetOriginalComponent(UPARAM(ref) FPCGContext& Context);

	UFUNCTION(BlueprintCallable, Category = "PCG|Points", meta = (ScriptMethod))
	static void SetExtents(UPARAM(ref) FPCGPoint& InPoint, const FVector& InExtents);

	UFUNCTION(BlueprintCallable, Category = "PCG|Points", meta = (ScriptMethod))
	static FVector GetExtents(const FPCGPoint& InPoint);

	UFUNCTION(BlueprintCallable, Category = "PCG|Points", meta = (ScriptMethod))
	static void SetLocalCenter(UPARAM(ref) FPCGPoint& InPoint, const FVector& InLocalCenter);

	UFUNCTION(BlueprintCallable, Category = "PCG|Points", meta = (ScriptMethod))
	static FVector GetLocalCenter(const FPCGPoint& InPoint);

	UFUNCTION(BlueprintCallable, Category = "PCG|Points", meta = (ScriptMethod))
	static FBox GetTransformedBounds(const FPCGPoint& InPoint);

	UFUNCTION(BlueprintCallable, Category = "PCG|Helpers", meta = (ScriptMethod))
	static FBox GetActorBoundsPCG(AActor* InActor, bool bIgnorePCGCreatedComponents = true);

	UFUNCTION(BlueprintCallable, Category = "PCG|Helpers", meta = (ScriptMethod))
	static FBox GetActorLocalBoundsPCG(AActor* InActor, bool bIgnorePCGCreatedComponents = true);
};