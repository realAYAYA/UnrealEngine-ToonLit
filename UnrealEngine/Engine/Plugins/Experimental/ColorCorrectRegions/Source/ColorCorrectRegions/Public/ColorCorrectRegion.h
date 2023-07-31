// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "Engine/Classes/Components/MeshComponent.h"
#include "Engine/Scene.h"
#include "ColorCorrectRegion.generated.h"


class UColorCorrectRegionsSubsystem;
class UBillboardComponent;


UENUM(BlueprintType)
enum class EColorCorrectRegionsType : uint8 
{
	Sphere		UMETA(DisplayName = "Sphere"),
	Box			UMETA(DisplayName = "Box"),
	Cylinder	UMETA(DisplayName = "Cylinder"),
	Cone		UMETA(DisplayName = "Cone"),
	MAX
};

UENUM(BlueprintType)
enum class EColorCorrectWindowType : uint8
{
	Square		UMETA(DisplayName = "Square"),
	Circle		UMETA(DisplayName = "Circle"),
	MAX
};

UENUM(BlueprintType)
enum class EColorCorrectRegionTemperatureType : uint8
{
	LegacyTemperature		UMETA(DisplayName = "Temperature (Legacy)"),
	WhiteBalance			UMETA(DisplayName = "White Balance"),
	ColorTemperature		UMETA(DisplayName = "Color Temperature"),
	MAX
};

UENUM(BlueprintType)
enum class EColorCorrectRegionStencilType : uint8
{
	ExcludeStencil			UMETA(DisplayName = "Exclude Selected Actors"),
	IncludeStencil			UMETA(DisplayName = "Affect Only Selected Actors"),
	MAX
};

class FStencilData
{
public:
	uint32 AssignedStencil = 0;
};

/** A state to store a copy of CCR properties to be used on render thread. */
struct FColorCorrectRenderProxy
{
	EColorCorrectRegionsType Type;
	EColorCorrectWindowType WindowType;

	/** CCR Properties */
	int32 Priority;
	float Intensity;
	float Inner;
	float Outer;
	float Falloff;
	bool Invert;
	EColorCorrectRegionTemperatureType TemperatureType;
	float Temperature;
	float Tint;
	FColorGradingSettings ColorGradingSettings;

	bool bEnablePerActorCC;
	/**
	* A list of actors that need to be excluded or included from color correction to be used on render thread.
	* This list is populated during engine tick and populated with Stencil Ids from Primitive Components of AffectedActors.
	* The main reason for this is accessing Primitive Components is not thread-safe and should be accessed on Game thread.
	*/
	TArray<uint32> StencilIds;
	EColorCorrectRegionStencilType PerActorColorCorrection;

	TWeakObjectPtr<UWorld> World;
	FVector BoxOrigin;
	FVector BoxExtent;

	FVector3f ActorLocation;
	FVector3f ActorRotation;
	FVector3f ActorScale;

	FPrimitiveComponentId FirstPrimitiveId;

	bool bIsActiveThisFrame;
};

typedef TSharedPtr<FColorCorrectRenderProxy, ESPMode::ThreadSafe> FColorCorrectRenderProxyPtr;

/**
 * An instance of Color Correction Region. Used to aggregate all active regions.
 * This actor is aggregated by ColorCorrectRegionsSubsystem which handles:
 *   - Level Loaded, Undo/Redo, Added to level, Removed from level events. 
 * AActor class itself is not aware of when it is added/removed, Undo/Redo etc in the Editor. 
 * AColorCorrectRegion reaches out to UColorCorrectRegionsSubsystem when its priority is changed, requesting regions to be sorted 
 * or during BeginPlay/EndPlay to register itself. 
 * More information in ColorCorrectRegionsSubsytem.h
 */
UCLASS(Blueprintable, NotPlaceable, Abstract)
class COLORCORRECTREGIONS_API AColorCorrectRegion : public AActor
{
	GENERATED_UCLASS_BODY()
public:
	/** Region type. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Color Correction")
	EColorCorrectRegionsType Type;

	/** 
	* Render priority/order. The higher the number the later region will be applied. 
	* A region with Priority 1 will be rendered before a region with Priority 10. 
	* This property is hidden if priority is determined by distance from the camera (When Window CCR is being used). 
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Color Correction")
	int32 Priority;

	/** Color correction intensity. Clamped to 0-1 range. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Color Correction", meta = (UIMin = 0.0, UIMax = 1.0))
	float Intensity;

	/** Inner of the region. Swapped with Outer in case it is higher than Outer. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Color Correction", meta = (UIMin = 0.0, UIMax = 1.0))
	float Inner;

	/** Outer of the region. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Color Correction", meta = (UIMin = 0.0, UIMax = 1.0))
	float Outer;

	/** Falloff. Softening the region. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Color Correction", meta = (UIMin = 0.0))
	float Falloff;

	/** Invert region. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Color Correction")
	bool Invert;

	/** Type of algorithm to be used to control color temperature or white balance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Color Correction")
	EColorCorrectRegionTemperatureType TemperatureType;

	/** Color correction temperature. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Color Correction", meta = (UIMin = "1500.0", UIMax = "15000.0"))
	float Temperature;

	/** Color temperature tint. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Color Correction", meta=(UIMin = "-1.0", UIMax = "1.0"))
	float Tint;

	/** Color correction settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Color Correction")
	FColorGradingSettings ColorGradingSettings;

	/** Enable/Disable color correction provided by this region. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Color Correction")
	bool Enabled;

	/** Enables or disabled per actor color correction. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Per Actor CC")
	bool bEnablePerActorCC;

	/** Controls in which way the below targets will be affected by color correction. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Per Actor CC", meta = (editcondition = "bEnablePerActorCC", DisplayName = "Per Actor CC Mode"))
	EColorCorrectRegionStencilType PerActorColorCorrection;

	/** List of actors that get affected or ignored by Per actor CC. Effect depends on the above option. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Per Actor CC", meta = (editcondition = "bEnablePerActorCC", DisplayName = "Actor Selection"))
	TSet<TSoftObjectPtr<AActor>> AffectedActors;

	/** 
	* This is a temporary property that is used to keep track of managed actors for per actor CC.
	* PerAffectedActorStencilData is managed by FColorCorrectRegionsStencilManager and CCR Subsystem
	* Its lifetime is aligned with selected level and gets reset often.
	*/
	TMap<TSoftObjectPtr<AActor>, TSharedPtr<FStencilData>> PerAffectedActorStencilData;

#if WITH_EDITORONLY_DATA

	/** Billboard component for this actor. */
	UPROPERTY(Transient)
	TObjectPtr<UBillboardComponent> SpriteComponent;

#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	/** Called when any of the properties are changed. */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

#endif

	/** To handle play in Editor, PIE and Standalone. These methods aggregate objects in play mode similarly to 
	* Editor methods in FColorCorrectRegionsSubsystem
	*/
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void BeginDestroy() override;

	virtual void TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction);
	virtual bool ShouldTickIfViewportsOnly() const;

	/** 
	* We have to manage the lifetime of the region ourselves, because EndPlay is not guaranteed to be called 
	* and BeginDestroy could be called from GC when it is too late.
	*/
	void Cleanup();

	/**
	* Gets a full state for rendering. 
	*/
	FColorCorrectRenderProxyPtr GetCCProxy_RenderThread()
	{
		check(IsInRenderingThread());
		return ColorCorrectRenderProxy;
	};

private:

#if WITH_METADATA
	/** Creates an icon for CCR/CCW to be clicked on in Editor. */
	void CreateIcon();

#endif // WITH_METADATA

	/**
	* AffectedActors property change could potentially invoke a Dialog Window, which should be displayed on Game Thread.
	*/
	void HandleAffectedActorsPropertyChange();

	/**
	* Copy state required for rendering to be consumed by Scene view extension.
	*/
	void TransferState();

private:
	UColorCorrectRegionsSubsystem* ColorCorrectRegionsSubsystem;

	/** A copy of all properties required by render thread to process this CCR. */
	FColorCorrectRenderProxyPtr ColorCorrectRenderProxy;

	bool bActorListIsDirty;
	uint8 ActorListChangeType;

	FCriticalSection StateCopyCriticalSecion;

	// This is for optimization purposes that would let us check assigned actors component's stencil ids ever few once in a while.
	float TimeWaited = 0;

};

/** 
 * NotPlaceable prohibits this actor from appearing in Place Actors menu by default. We add that manually with an icon attached.
*/
/**
 * A placeable Color Correction Regions actor that replaces previous implementation (blueprint). 
 * Color Correction Regions allow users to adjust color of anything that is within it (or outside, if Invert option is selected). 
 */
UCLASS(Blueprintable, NotPlaceable)
class COLORCORRECTREGIONS_API AColorCorrectionRegion : public AColorCorrectRegion
{
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> MeshComponents;

public:
	/** Swaps meshes for different CCR. */
	void SetMeshVisibilityForRegionType();

#if WITH_EDITOR
	/** Called when any of the properties are changed. */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual FName GetCustomIconName() const override;
#endif
};
