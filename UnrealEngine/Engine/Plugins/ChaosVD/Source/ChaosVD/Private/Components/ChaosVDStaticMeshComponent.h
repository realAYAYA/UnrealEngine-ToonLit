// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/StaticMeshComponent.h"
#include "ChaosVDGeometryDataComponent.h"
#include "ChaosVDStaticMeshComponent.generated.h"

/** CVD version of a Static Mesh Component that holds additional CVD data */
UCLASS(HideCategories=("Transform"), MinimalAPI)
class UChaosVDStaticMeshComponent : public UStaticMeshComponent, public IChaosVDGeometryComponent
{
	GENERATED_BODY()

public:
	UChaosVDStaticMeshComponent(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
		SetCanEverAffectNavigation(false);
		bNavigationRelevant = false;
	}

	// BEGIN IChaosVDGeometryDataComponent Interface

	virtual bool IsMeshReady() const override { return bIsMeshReady; }
	
	virtual void SetIsMeshReady(bool bIsReady) override { bIsMeshReady = bIsReady; }

	virtual FChaosVDMeshReadyDelegate* OnMeshReady() override { return &MeshReadyDelegate; }
	
	virtual FChaosVDMeshComponentEmptyDelegate* OnComponentEmpty() override { return &ComponentEmptyDelegate; }

	virtual uint32 GetGeometryKey() const override;
	virtual void UpdateInstanceVisibility(const TSharedPtr<FChaosVDMeshDataInstanceHandle>& InInstanceHandle, bool bIsVisible) override;

	virtual void SetIsSelected(const TSharedPtr<FChaosVDMeshDataInstanceHandle>& InInstanceHandle, bool bIsSelected) override;

	virtual bool ShouldRenderSelected() const override;
	
	virtual void UpdateInstanceColor(const TSharedPtr<FChaosVDMeshDataInstanceHandle>& InInstanceHandle, FLinearColor NewColor) override;
	virtual void UpdateInstanceWorldTransform(const TSharedPtr<FChaosVDMeshDataInstanceHandle>& InInstanceHandle, const FTransform& InTransform) override;

	virtual void SetMeshComponentAttributeFlags(EChaosVDMeshAttributesFlags Flags) override { MeshComponentAttributeFlags = static_cast<uint8>(Flags); }
	virtual EChaosVDMeshAttributesFlags GetMeshComponentAttributeFlags() const override {return static_cast<EChaosVDMeshAttributesFlags>(MeshComponentAttributeFlags); };

	virtual TSharedPtr<FChaosVDMeshDataInstanceHandle> GetMeshDataInstanceHandle(int32 InstanceIndex) const override;
	virtual TArrayView<TSharedPtr<FChaosVDMeshDataInstanceHandle>> GetMeshDataInstanceHandles() override;

	virtual void Reset() override;

	virtual TSharedPtr<FChaosVDMeshDataInstanceHandle> AddMeshInstance(const FTransform InstanceTransform, bool bIsWorldSpace, const TSharedPtr<FChaosVDExtractedGeometryDataHandle>& InGeometryHandle, int32 ParticleID, int32 SolverID) override;
	virtual void AddMeshInstanceForHandle(TSharedPtr<FChaosVDMeshDataInstanceHandle> MeshDataHandle, const FTransform InstanceTransform, bool bIsWorldSpace, const TSharedPtr<FChaosVDExtractedGeometryDataHandle>& InGeometryHandle, int32 ParticleID, int32 SolverID) override;
	virtual void RemoveMeshInstance(TSharedPtr<FChaosVDMeshDataInstanceHandle> InHandleToRemove) override;
	// END IChaosVDGeometryDataComponent Interface

protected:

	bool UpdateGeometryKey(uint32 NewHandleGeometryKey);

	/** Returns an existing material instance used by this mesh instances, or creates a new one for the provided type */
	UMaterialInstanceDynamic* GetCachedMaterialInstance(EChaosVDMaterialType Type);

	uint8 MeshComponentAttributeFlags = 0;
	uint8 CurrentGeometryKey = 0;
	bool bIsMeshReady = false;
	bool bIsOwningParticleSelected = false;
	FChaosVDMeshReadyDelegate MeshReadyDelegate;
	FChaosVDMeshComponentEmptyDelegate ComponentEmptyDelegate;

	TSharedPtr<FChaosVDMeshDataInstanceHandle> CurrentMeshDataHandle = nullptr;

	TSharedPtr<FChaosVDExtractedGeometryDataHandle> CurrentGeometryHandle = nullptr;

	UPROPERTY(Transient)
	TMap<EChaosVDMaterialType, TObjectPtr<UMaterialInstanceDynamic>> CachedMaterialInstancesByID;
};
