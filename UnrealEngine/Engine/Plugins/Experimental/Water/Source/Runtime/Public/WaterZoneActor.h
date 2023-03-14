// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "WaterZoneActor.generated.h"

class UWaterMeshComponent;
class UBoxComponent;
class AWaterBody;
class UWaterBodyComponent;

enum class EWaterZoneRebuildFlags
{
	None = 0,
	UpdateWaterInfoTexture = (1 << 1),
	UpdateWaterMesh = (1 << 2),
	UpdateWaterBodyLODSections = (1 << 3),
	All = (~0),
};
ENUM_CLASS_FLAGS(EWaterZoneRebuildFlags);

UCLASS(Blueprintable, HideCategories=(Physics, Replication, Input, Collision))
class WATER_API AWaterZone : public AActor
{
	GENERATED_UCLASS_BODY()
public:
	UWaterMeshComponent* GetWaterMeshComponent() { return WaterMesh; }
	const UWaterMeshComponent* GetWaterMeshComponent() const { return WaterMesh; }

	void MarkForRebuild(EWaterZoneRebuildFlags Flags);
	void Update();
		
	/** Execute a predicate function on each valid water body within the water zone. Predicate should return false for early exit. */
	void ForEachWaterBodyComponent(TFunctionRef<bool(UWaterBodyComponent*)> Predicate) const;

	void AddWaterBodyComponent(UWaterBodyComponent* WaterBodyComponent);
	void RemoveWaterBodyComponent(UWaterBodyComponent* WaterBodyComponent);

	FVector2D GetZoneExtent() const { return ZoneExtent; }
	void SetZoneExtent(FVector2D NewExtents);

	void SetRenderTargetResolution(FIntPoint NewResolution);
	FIntPoint GetRenderTargetResolution() const { return RenderTargetResolution; }

	uint32 GetVelocityBlurRadius() const { return VelocityBlurRadius; }

	virtual void BeginPlay() override;
	virtual void PostLoadSubobjects(FObjectInstancingGraph* OuterInstanceGraph) override;
	virtual void PostLoad() override;
	virtual void PostRegisterAllComponents() override;

	FVector2f GetWaterHeightExtents() const { return WaterHeightExtents; }
	float GetGroundZMin() const { return GroundZMin; }

	int32 GetOverlapPriority() const { return OverlapPriority; }

	UPROPERTY(Transient, DuplicateTransient, VisibleAnywhere, BlueprintReadOnly, Category = Water)
	TObjectPtr<UTextureRenderTarget2D> WaterInfoTexture;

	FVector GetTessellatedWaterMeshCenter() const;
	void SetTessellatedWaterMeshCenter(FVector NewCenter) { TessellatedWaterMeshCenter = NewCenter;}

	FVector GetTessellatedWaterMeshExtent() const;

	uint32 GetNonTessellatedLODScale() const { return NonTessellatedLODSectionScale; }
	float GetNonTessellatedLODSectionSize() const;

	bool IsNonTessellatedLODMeshEnabled() const { return bEnableNonTessellatedLODMesh; }

private:

	/**
	 * Enqueue's a command on the Water Scene View Extension to re-render the water info on the next frame.
	 * Returns false if the water info cannot be rendered this frame due to one of the dependencies not being ready yet (ie. a material under On-Demand-Shader-Compilation)
	 */
	bool UpdateWaterInfoTexture();

	/**
	 * Updates the list of owned water bodies causing any overlapping water bodies which do no have an owning water zone to register themselves to this water zone.
	 * Returns true if any any change to the list of owned water bodies occurred
	 */
	bool UpdateOverlappingWaterBodies();

	void OnExtentChanged();

#if WITH_EDITOR
	void OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh);
	
	UFUNCTION(CallInEditor, Category = Debug)
	void ForceUpdateWaterInfoTexture();

	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditUndo() override;
	virtual void PostEditImport() override;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	/** Called when the Bounds component is modified. Updates the value of ZoneExtent to match the new bounds */
	void OnBoundsComponentModified();
#endif // WITH_EDITOR

private:

	UPROPERTY(Transient, Category = Water, VisibleAnywhere)
	TArray<TWeakObjectPtr<UWaterBodyComponent>> OwnedWaterBodies;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Water, meta = (AllowPrivateAccess = "true"))
	FIntPoint RenderTargetResolution;

	/** The water mesh component */
	UPROPERTY(VisibleAnywhere, Category = Water, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWaterMeshComponent> WaterMesh;

	/** Width of the zone bounding box */
	UPROPERTY(Category = Water, EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	FVector2D ZoneExtent;

	/** Offsets the height above the water zone at which the WaterInfoTexture is rendered. This is applied after computing the maximum Z of all the water bodies within the zone. */
	UPROPERTY(Category = Water, EditAnywhere, AdvancedDisplay, meta = (AllowPrivateAccess = "true"))
	float CaptureZOffset = 64.f;

	/** Determines if the WaterInfoTexture should be 16 or 32 bits per channel */
	UPROPERTY(Category = Water, EditAnywhere, AdvancedDisplay, meta = (AllowPrivateAccess = "true"))
	bool bHalfPrecisionTexture = true;

	/** Radius of the velocity blur in the finalize water info pass */
	UPROPERTY(Category = Water, EditAnywhere, AdvancedDisplay)
	int32 VelocityBlurRadius = 1;

	/** Area around the camera covered by the tessellated water mesh when LOD is enabled. */
	UPROPERTY(Category = NonTessellatedLOD, EditAnywhere, meta = (EditCondition = "bEnableNonTessellatedLODMesh"))
	FVector TessellatedWaterMeshExtent;

	UPROPERTY(Category = NonTessellatedLOD, EditAnywhere, meta = (ClampMin = "1", DisplayName = "Non Tessellated LOD Section Scale", EditCondition = "bEnableNonTessellatedLODMesh"))
	uint32 NonTessellatedLODSectionScale = 4;

	/** Higher number is higher priority. If Water Zones overlap and a water body does not have a manual water zone override, this priority will be used when automatically assigning the zone. */
	UPROPERTY(Category = Water, EditAnywhere, AdvancedDisplay)
	int32 OverlapPriority = 0;

	UPROPERTY(Category = NonTessellatedLOD, EditAnywhere, meta = (DisplayName = "Enable Non-Tessellated LOD Mesh"))
	bool bEnableNonTessellatedLODMesh = false;

	bool bNeedsWaterInfoRebuild = true;

	bool bNeedsNonTessellatedMeshRebuild = true;

	FVector2f WaterHeightExtents;
	float GroundZMin;

	FVector TessellatedWaterMeshCenter;

#if WITH_EDITORONLY_DATA
	/** A manipulatable box for visualizing/editing the water zone bounds */
	UPROPERTY(Transient)
	TObjectPtr<UBoxComponent> BoundsComponent;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AWaterBody>> SelectedWaterBodies;

	UPROPERTY(Transient)
	TObjectPtr<UBillboardComponent> ActorIcon;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> WaterVelocityTexture_DEPRECATED;
#endif // WITH_EDITORONLY_DATA
};