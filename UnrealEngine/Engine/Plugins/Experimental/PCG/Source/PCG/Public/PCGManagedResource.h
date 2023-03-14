// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "PCGManagedResource.generated.h"

class UActorComponent;
class UInstancedStaticMeshComponent;

/** 
* This class is used to hold resources and their mechanism to delete them on demand.
* In order to allow for some reuse (e.g. components), the Release call supports a "soft"
* release by marking them unused in order to be potentially re-used down the line.
* At the end of the generate, a call to ReleaseIfUnused will serve to finally cleanup
* what is not needed anymore.
*/
UCLASS(BlueprintType)
class PCG_API UPCGManagedResource : public UObject
{
	GENERATED_BODY()
public:
	/** Called when after a PCG component is applied to (such as after a RerunConstructionScript) */
	virtual void PostApplyToComponent();

	/** Releases/Mark Unsued the resource depending on the bHardRelease flag. Returns true if resource can be removed from the PCG component */
	virtual bool Release(bool bHardRelease, TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete);
	/** Releases resource if empty or unused. Returns true if the resource can be removed from the PCG component */
	virtual bool ReleaseIfUnused(TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete);

	virtual void MarkAsUsed() { bIsMarkedUnused = false; }
	bool IsMarkedUnused() const { return bIsMarkedUnused; }

	/** Move the given resource to a new actor. Return true if it has succeeded */
	virtual bool MoveResourceToNewActor(AActor* NewActor) { return false; };

protected:
	UPROPERTY(Transient, VisibleAnywhere, Category = GeneratedData)
	bool bIsMarkedUnused = false;
};

UCLASS(BlueprintType)
class PCG_API UPCGManagedActors : public UPCGManagedResource
{
	GENERATED_BODY()

public:
	//~Begin UObject interface
	virtual void PostEditImport() override;
	//~End UObject interface

	//~Begin UPCGManagedResource interface
	virtual void PostApplyToComponent() override;
	virtual bool Release(bool bHardRelease, TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete) override;
	virtual bool ReleaseIfUnused(TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete) override;
	virtual bool MoveResourceToNewActor(AActor* NewActor) override;
	//~End UPCGManagedResource interface

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = GeneratedData)
	TSet<TSoftObjectPtr<AActor>> GeneratedActors;
};

UCLASS(BlueprintType)
class PCG_API UPCGManagedComponent : public UPCGManagedResource
{
	GENERATED_BODY()

public:
	//~Begin UObject interface
	virtual void PostEditImport() override;
	//~End UObject interface

	//~Begin UPCGManagedResource interface
	virtual bool Release(bool bHardRelease, TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete) override;
	virtual bool ReleaseIfUnused(TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete) override;
	virtual bool MoveResourceToNewActor(AActor* NewActor) override;
	//~End UPCGManagedResource interface

	virtual void ResetComponent() { check(0); }
	virtual bool SupportsComponentReset() const { return false; }
	virtual void MarkAsUsed() override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = GeneratedData)
	TSoftObjectPtr<UActorComponent> GeneratedComponent;
};

UCLASS(BlueprintType)
class PCG_API UPCGManagedISMComponent : public UPCGManagedComponent
{
	GENERATED_BODY()

public:
	//~Begin UPCGManagedResource interface
	virtual bool ReleaseIfUnused(TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete) override;
	//~End UPCGManagedResource interface

	//~Begin UPCGManagedComponents interface
	virtual void ResetComponent() override;
	virtual bool SupportsComponentReset() const { return true; }
	//~End UPCGManagedComponents interface

	UInstancedStaticMeshComponent* GetComponent() const;
};