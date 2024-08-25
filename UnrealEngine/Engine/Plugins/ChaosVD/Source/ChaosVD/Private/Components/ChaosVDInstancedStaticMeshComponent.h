// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/InstancedStaticMeshComponent.h"
#include "ChaosVDGeometryDataComponent.h"
#include "ChaosVDInstancedStaticMeshComponent.generated.h"

/** CVD version of a Instance Static Mesh Component that holds additional CVD data */
UCLASS(HideCategories=("Transform"), MinimalAPI)
class UChaosVDInstancedStaticMeshComponent : public UInstancedStaticMeshComponent, public IChaosVDGeometryComponent
{
	GENERATED_BODY()

	UChaosVDInstancedStaticMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
		SetCanEverAffectNavigation(false);
		bHasPerInstanceHitProxies = true;
	}
	
	// BEGIN IChaosVDGeometryDataComponent Interface
	
	virtual uint32 GetGeometryKey() const override;

	virtual TSharedPtr<FChaosVDMeshDataInstanceHandle> GetMeshDataInstanceHandle(int32 InstanceIndex) const override;

	virtual bool IsMeshReady() const override { return bIsMeshReady; }
	
	virtual void SetIsMeshReady(bool bIsReady) override { bIsMeshReady = bIsReady; }

	virtual FChaosVDMeshReadyDelegate* OnMeshReady() override { return &MeshReadyDelegate; }

	virtual FChaosVDMeshComponentEmptyDelegate* OnComponentEmpty() override { return &ComponentEmptyDelegate; }

	virtual void UpdateInstanceVisibility(const TSharedPtr<FChaosVDMeshDataInstanceHandle>& InInstanceHandle, bool bIsVisible) override;

	virtual void SetIsSelected(const TSharedPtr<FChaosVDMeshDataInstanceHandle>& InInstanceHandle, bool bIsSelected) override;

	virtual void UpdateInstanceColor(const TSharedPtr<FChaosVDMeshDataInstanceHandle>& InInstanceHandle, FLinearColor NewColor) override;
	
	virtual void UpdateInstanceWorldTransform(const TSharedPtr<FChaosVDMeshDataInstanceHandle>& InInstanceHandle, const FTransform& InTransform) override;

	virtual void SetMeshComponentAttributeFlags(EChaosVDMeshAttributesFlags Flags) override { MeshComponentAttributeFlags = static_cast<uint8>(Flags); }
	virtual EChaosVDMeshAttributesFlags GetMeshComponentAttributeFlags() const override {return static_cast<EChaosVDMeshAttributesFlags>(MeshComponentAttributeFlags); };

	virtual TArrayView<TSharedPtr<FChaosVDMeshDataInstanceHandle>> GetMeshDataInstanceHandles() override;

	virtual void Reset() override;
	// END IChaosVDGeometryDataComponent Interface

	virtual TSharedPtr<FChaosVDMeshDataInstanceHandle> AddMeshInstance(const FTransform InstanceTransform, bool bIsWorldSpace, const TSharedPtr<FChaosVDExtractedGeometryDataHandle>& InGeometryHandle, int32 ParticleID, int32 SolverID) override;
	virtual void AddMeshInstanceForHandle(TSharedPtr<FChaosVDMeshDataInstanceHandle> MeshDataHandle, const FTransform InstanceTransform, bool bIsWorldSpace, const TSharedPtr<FChaosVDExtractedGeometryDataHandle>& InGeometryHandle, int32 ParticleID, int32 SolverID) override;
	virtual void RemoveMeshInstance(TSharedPtr<FChaosVDMeshDataInstanceHandle> InHandleToRemove) override;

	virtual bool Modify(bool bAlwaysMarkDirty) override;

	virtual bool IsNavigationRelevant() const override;

protected:
	
	bool UpdateGeometryKey(uint32 NewHandleGeometryKey);

	uint8 MeshComponentAttributeFlags = 0;

	uint32 CurrentGeometryKey = 0;

	bool bIsMeshReady = false;

	UPROPERTY(Transient)
	UMaterialInterface* CurrentMaterial = nullptr;

	UPROPERTY(Transient)
	TMap<EChaosVDMaterialType, TObjectPtr<UMaterialInstanceDynamic>> CachedMaterialInstancesByID;

	FChaosVDMeshReadyDelegate MeshReadyDelegate;
	FChaosVDMeshComponentEmptyDelegate ComponentEmptyDelegate;

	TArray<TSharedPtr<FChaosVDMeshDataInstanceHandle>> CurrentInstanceHandles;
};
