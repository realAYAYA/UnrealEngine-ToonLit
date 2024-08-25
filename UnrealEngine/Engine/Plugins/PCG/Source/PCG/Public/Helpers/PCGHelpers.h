// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommon.h"
#include "PCGPoint.h"

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Math/Box.h"

#include "PCGHelpers.generated.h"

class AActor;
class APCGWorldActor;
class ALandscape;
class ALandscapeProxy;
class UPCGComponent;
class UPCGGraph;
class UPCGMetadata;
class UWorld;
struct FPCGContext;
struct FPCGPoint;

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
	PCG_API int ComputeSeedFromPosition(const FVector& InPosition);

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
	PCG_API void GatherDependencies(UObject* Object, TSet<TObjectPtr<UObject>>& OutDependencies, int32 MaxDepth = -1, const TArray<UClass*>& InExcludedClasses = {});
	PCG_API void GatherDependencies(FProperty* Property, const void* InContainer, TSet<TObjectPtr<UObject>>& OutDependencies, int32 MaxDepth, const TArray<UClass*>& InExcludedClasses = {});
#endif

	/** 
	* Check if an object is a new object and not the CDO.
	*
	* Some objects might not have the appropriate flags if they are embedded inside of other objects. 
	* Use the bCheckHierarchy flag to true to go up the object hierarchy if you want to check for this situation.
	*/
	PCG_API bool IsNewObjectAndNotDefault(const UObject* InObject, bool bCheckHierarchy = false);

	/** If hierarchical generation is enabled, returns all relevant grid sizes for the graph, otherwise returns partition grid size from world actor. */
	PCG_API bool GetGenerationGridSizes(const UPCGGraph* InGraph, const APCGWorldActor* InWorldActor, PCGHiGenGrid::FSizeArray& OutGridSizes, bool& bOutHasUnbounded);

#if WITH_EDITOR
	PCG_API void GetGeneratedActorsFolderPath(const AActor* InTargetActor, FString& OutFolderPath);
#endif

	PCG_API void AttachToParent(AActor* InActorToAttach, AActor* InParent, EPCGAttachOptions AttachOptions, const FString& GeneratedPath = FString());

	/**
	 * Finds functions on the actor matching the provided function names. Functions must be marked as CallInEditor
	 * and have parameters matching one of the provided prototypes. Some prototypes are provided in UPCGFunctionPrototypes.
	 */
	PCG_API TArray<UFunction*> FindUserFunctions(TSubclassOf<AActor> ActorClass, const TArray<FName>& FunctionNames, const TArray<const UFunction*>& FunctionPrototypes, const FPCGContext* InContext = nullptr);
};

/** Holds function prototypes used to match against actor function signatures. */
UCLASS(MinimalAPI)
class UPCGFunctionPrototypes : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static UFunction* GetPrototypeWithNoParams() { return FindObject<UFunction>(StaticClass(), TEXT("PrototypeWithNoParams")); }
	static UFunction* GetPrototypeWithPointAndMetadata() { return FindObject<UFunction>(StaticClass(), TEXT("PrototypeWithPointAndMetadata")); }

private:
	UFUNCTION()
	void PrototypeWithNoParams() {}

	UFUNCTION()
	void PrototypeWithPointAndMetadata(FPCGPoint Point, const UPCGMetadata* Metadata) {}
};
