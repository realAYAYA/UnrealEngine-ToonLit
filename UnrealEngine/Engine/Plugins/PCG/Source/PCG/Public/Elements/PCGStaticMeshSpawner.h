// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"

#include "Metadata/PCGObjectPropertyOverride.h"

#include "Engine/CollisionProfile.h"

#include "PCGStaticMeshSpawner.generated.h"

class UPCGInstanceDataPackerBase;
class UPCGMeshSelectorBase;
class UPCGSpatialData;
struct FPCGContext;
struct FPCGMeshInstanceList;
struct FPCGPackedCustomData;
struct FPCGStaticMeshSpawnerContext;

class UStaticMesh;

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), meta = (PrioritizeCategories = "Settings"))
class UPCGStaticMeshSpawnerSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPCGStaticMeshSpawnerSettings(const FObjectInitializer &ObjectInitializer);

#if WITH_EDITOR
	// ~Begin UPCGSettings interface
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("StaticMeshSpawner")); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spawner; }
	virtual void ApplyDeprecation(UPCGNode* InOutNode) override;
#endif
	

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return Super::DefaultPointInputPinProperties(); }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return Super::DefaultPointOutputPinProperties(); }
	virtual FPCGElementPtr CreateElement() const override;
	// ~End UPCGSettings interface

public:
	// ~Begin UObject interface
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	// ~End UObject interface
#endif

	UFUNCTION(BlueprintCallable, Category = Settings)
	PCG_API void SetMeshSelectorType(TSubclassOf<UPCGMeshSelectorBase> InMeshSelectorType);

	UFUNCTION(BlueprintCallable, Category = Settings)
	PCG_API void SetInstancePackerType(TSubclassOf<UPCGInstanceDataPackerBase> InInstancePackerType);

public:
	/** Defines the method of mesh selection per input data */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, NoClear, Category = MeshSelector)
	TSubclassOf<UPCGMeshSelectorBase> MeshSelectorType;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Instanced, Category = MeshSelector)
	TObjectPtr<UPCGMeshSelectorBase> MeshSelectorParameters;

	/** Defines the method of custom data packing for spawned (H)ISMCs */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = InstanceDataPacker)
	TSubclassOf<UPCGInstanceDataPackerBase> InstanceDataPackerType;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Instanced, Category = InstanceDataPacker)
	TObjectPtr<UPCGInstanceDataPackerBase> InstanceDataPackerParameters = nullptr;

	/** Map an attribute directly to an ISM Descriptor property, the value of which will be overriden when generated.
	 * Note: Currently only enabled using SelectByAttribute mesh selection.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FPCGObjectPropertyOverrideDescription> StaticMeshComponentPropertyOverrides;

	/** Attribute name to store mesh SoftObjectPaths inside if the output pin is connected. Note: Will overwrite existing data if the attribute name already exists. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FName OutAttributeName = NAME_None;

	/** Sets the BoundsMin and BoundsMax attributes of each point to reflect the StaticMesh spawned at its location */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bApplyMeshBoundsToPoints = true;

	UPROPERTY(BlueprintReadWrite, Category = Settings, meta = (PCG_Overridable))
	TSoftObjectPtr<AActor> TargetActor;

	/** Specify a list of functions to be called on the target actor after instances are spawned. Functions need to be parameter-less and with "CallInEditor" flag enabled. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FName> PostProcessFunctionNames;

	/** Meshes/Materials will be synchronously loaded before spawning instead of asynchronously. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Debug")
	bool bSynchronousLoad = false;

	/** Opt-in option to silence errors when the property override attributes are not found. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Advanced")
	bool bSilenceOverrideAttributeNotFoundErrors = false;

protected:
	void RefreshMeshSelector();
	void RefreshInstancePacker();
};

class FPCGStaticMeshSpawnerElement : public IPCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override;
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual FPCGContext* CreateContext() override;
	virtual bool PrepareDataInternal(FPCGContext* Context) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual void AbortInternal(FPCGContext* Context) const override;
	void SpawnStaticMeshInstances(FPCGStaticMeshSpawnerContext* Context, const FPCGMeshInstanceList& InstanceList, AActor* TargetActor, const FPCGPackedCustomData& PackedCustomData) const;
};
