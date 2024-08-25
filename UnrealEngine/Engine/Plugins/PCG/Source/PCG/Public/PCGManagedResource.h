// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommon.h"
#include "PCGCrc.h"

#include "ISMPartition/ISMComponentDescriptor.h"

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

	/** Releases/Mark Unused the resource depending on the bHardRelease flag. Returns true if resource can be removed from the PCG component */
	virtual bool Release(bool bHardRelease, TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete);
	/** Releases resource if empty or unused. Returns true if the resource can be removed from the PCG component */
	virtual bool ReleaseIfUnused(TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete);

	/** Returns whether a resource can be used - generally true except for resources marked as transient (from loading) */
	virtual bool CanBeUsed() const;

	/** Marks the resources as being kept and changed through generation */
	virtual void MarkAsUsed() { ensure(CanBeUsed()); bIsMarkedUnused = false; }
	/** Marks the resource as being reused as-is during the generation */
	virtual void MarkAsReused() { bIsMarkedUnused = false; }
	bool IsMarkedUnused() const { return bIsMarkedUnused; }

	/** Move the given resource to a new actor. Return true if it has succeeded */
	virtual bool MoveResourceToNewActor(AActor* NewActor) { return false; };
	virtual bool MoveResourceToNewActor(AActor* NewActor, const AActor* ExpectedPreviousOwner) { return MoveResourceToNewActor(NewActor); }

	static bool DebugForcePurgeAllResourcesOnGenerate();

	const FPCGCrc& GetCrc() const { return Crc; }
	void SetCrc(const FPCGCrc& InCrc) { Crc = InCrc; }

#if WITH_EDITOR
	virtual void ChangeTransientState(EPCGEditorDirtyMode NewEditingMode);
	virtual void MarkTransientOnLoad() { bMarkedTransientOnLoad = true; }

	bool IsMarkedTransientOnLoad() const { return bMarkedTransientOnLoad; }
#endif // WITH_EDITOR

protected:
	UPROPERTY(VisibleAnywhere, Category = GeneratedData)
	FPCGCrc Crc;

	UPROPERTY(Transient, VisibleAnywhere, Category = GeneratedData)
	bool bIsMarkedUnused = false;

#if WITH_EDITORONLY_DATA
	// Resources on a Load-as-preview component are marked as 'transient on load'; these resources must not be affected in any
	//  permanent way in order to make sure they are not serialized in a different state if their outer is saved.
	// These resources will generally have a different Release path, and will be managed differently from the PCG component as well.
	// Note that this flag will be reset if there is a transient state change originating from the component, which might trigger resource deletion, flags change, etc.
	UPROPERTY(VisibleAnywhere, Category = GeneratedData, meta = (NoResetToDefault))
	bool bMarkedTransientOnLoad = false;
#endif
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
	virtual void MarkAsUsed() override;
	virtual void MarkAsReused() override;

#if WITH_EDITOR
	virtual void ChangeTransientState(EPCGEditorDirtyMode NewEditingMode) override;
#endif
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
	virtual bool MoveResourceToNewActor(AActor* NewActor, const AActor* ExpectedPreviousOwner) override;

#if WITH_EDITOR
	virtual void ChangeTransientState(EPCGEditorDirtyMode NewEditingMode) override;
	/** Hides the content of the component in a transient way (such as unregistering) */
	virtual void HideComponent();
#endif
	//~End UPCGManagedResource interface

	virtual void ResetComponent() { check(0); }
	virtual bool SupportsComponentReset() const { return false; }
	virtual void MarkAsUsed() override;
	virtual void MarkAsReused() override;
	virtual void ForgetComponent() { GeneratedComponent.Reset(); }

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = GeneratedData)
	TSoftObjectPtr<UActorComponent> GeneratedComponent;
};

UCLASS(BlueprintType)
class PCG_API UPCGManagedISMComponent : public UPCGManagedComponent
{
	GENERATED_BODY()

public:
	//~Begin UObject interface
	virtual void PostLoad() override;
	//~End UObject interface

	//~Begin UPCGManagedResource interface
	virtual bool ReleaseIfUnused(TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete) override;
	//~End UPCGManagedResource interface

	//~Begin UPCGManagedComponents interface
	virtual void ResetComponent() override;
	virtual bool SupportsComponentReset() const override{ return true; }
	virtual void MarkAsUsed() override;
	virtual void MarkAsReused() override;
	virtual void ForgetComponent() override;
	//~End UPCGManagedComponents interface

	UInstancedStaticMeshComponent* GetComponent() const;
	void SetComponent(UInstancedStaticMeshComponent* InComponent);

	void SetDescriptor(const FISMComponentDescriptor& InDescriptor);
	const FISMComponentDescriptor& GetDescriptor() const { return Descriptor; }

	void SetRootLocation(const FVector& InRootLocation);

	uint64 GetSettingsUID() const { return SettingsUID; }
	void SetSettingsUID(uint64 InSettingsUID) { SettingsUID = InSettingsUID; }

protected:
	UPROPERTY()
	bool bHasDescriptor = false;

	UPROPERTY()
	FISMComponentDescriptor Descriptor;

	UPROPERTY()
	bool bHasRootLocation = false;

	UPROPERTY()
	FVector RootLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	uint64 SettingsUID = -1; // purposefully a value that will never happen in data

	// Cached raw pointer to ISM component
	mutable UInstancedStaticMeshComponent* CachedRawComponentPtr = nullptr;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
