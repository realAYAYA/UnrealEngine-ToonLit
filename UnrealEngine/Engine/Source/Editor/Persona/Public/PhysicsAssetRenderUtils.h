// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPhysicsAssetRenderInterface.h"
#include "UObject/ObjectMacros.h"
#include "Math/Color.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/ShapeElem.h"
#include "SceneManagement.h"
#include "PhysicsAssetRenderUtils.generated.h"

enum class EPhysicsAssetEditorConstraintViewMode : uint8;
enum class EPhysicsAssetEditorCollisionViewMode : uint8;

class HHitProxy;
class UPhysicsAsset;
class USkeletalMeshComponent;

//////////////////////////////////////////////////////////////////////////
// FPhysicsAssetRenderSettings

/**
	Per Physics Asset parameters that determine how debug draw functions 
	should render that asset in an editor viewport.
	
	These parameters are used across different editor modes to ensure the 
	debug draw is consistent. This makes it easier to create or debug 
	physics assets whilst switching between editor modes.
*/
USTRUCT()
struct PERSONA_API FPhysicsAssetRenderSettings
{
	GENERATED_BODY()

public:

	FPhysicsAssetRenderSettings();
	void InitPhysicsAssetRenderSettings(class UMaterialInterface* InBoneUnselectedMaterial, class UMaterialInterface* InBoneNoCollisionMaterial);

	/** Accessors/helper methods */
	bool IsBodyHidden(const int32 BodyIndex) const;
	bool IsConstraintHidden(const int32 ConstraintIndex) const;
	bool AreAnyBodiesHidden() const;
	bool AreAnyConstraintsHidden() const;
	void HideBody(const int32 BodyIndex);
	void ShowBody(const int32 BodyIndex);
	void HideConstraint(const int32 ConstraintIndex);
	void ShowConstraint(const int32 ConstraintIndex);
	void ShowAllBodies();
	void ShowAllConstraints();
	void ShowAll();
	void HideAllBodies(const UPhysicsAsset* const PhysicsAsset);
	void HideAllConstraints(const UPhysicsAsset* const PhysicsAsset);
	void HideAll(const UPhysicsAsset* const PhysicsAsset);
	void ToggleShowBody(const int32 BodyIndex);
	void ToggleShowConstraint(const int32 ConstraintIndex);
	void ToggleShowAllBodies(const UPhysicsAsset* const PhysicsAsset);
	void ToggleShowAllConstraints(const UPhysicsAsset* const PhysicsAsset);

	void SetHiddenBodies(const TArray<int32>& InHiddenBodies);
	void SetHiddenConstraints(const TArray<int32>& InHiddenConstraints);

	/** Returns a set of flags describing which components of the selected constraint's transforms are being manipulated in the view port. */
	EConstraintTransformComponentFlags GetConstraintViewportManipulationFlags() const;

	/** Returns true if all the constraint transform components specified by the flags should be displayed as an offset from the default (snapped) transforms. */
	bool IsDisplayingConstraintTransformComponentRelativeToDefault(const EConstraintTransformComponentFlags ComponentFlags) const;

	/** Set how the constraint transform components specified by the flags should be displayed, in the frame of their associated physics body (false) or as an offset from the default (snapped) transforms (true). */
	void SetDisplayConstraintTransformComponentRelativeToDefault(const EConstraintTransformComponentFlags ComponentFlags, const bool bShouldDisplayRelativeToDefault);

	void ResetEditorViewportOptions();

	// Physics Asset Editor Viewport Options
	UPROPERTY()
	EPhysicsAssetEditorCollisionViewMode CollisionViewMode;

	UPROPERTY()
	EPhysicsAssetEditorConstraintViewMode ConstraintViewMode;

	// Flags that determine which parts of the constraints transforms (parent frame, child frame, position and rotation) are currently begin manipulated in the viewport.
	UPROPERTY(Transient)
	EConstraintTransformComponentFlags ConstraintViewportManipulationFlags;

	// Flags that determine which parts of the constraints transforms (parent/child position/rotation) should be displayed as an offset from the default (snapped) transforms.
	UPROPERTY()
	EConstraintTransformComponentFlags ConstraintTransformComponentDisplayRelativeToDefaultFlags;

	UPROPERTY()
	float ConstraintDrawSize;

	UPROPERTY()
	float PhysicsBlend;

	UPROPERTY()
	bool bHideKinematicBodies;

	UPROPERTY()
	bool bHideSimulatedBodies;

	UPROPERTY()
	bool bRenderOnlySelectedConstraints;

	UPROPERTY()
	bool bShowCOM;

	UPROPERTY()
	bool bShowConstraintsAsPoints;

	UPROPERTY()
	bool bDrawViolatedLimits;

	// Draw colors
	UPROPERTY()
	FColor BoneUnselectedColor;

	UPROPERTY()
	FColor NoCollisionColor;

	UPROPERTY()
	FColor COMRenderColor;

	UPROPERTY()
	float COMRenderSize;

	UPROPERTY()
	float InfluenceLineLength;

	// Materials
	UPROPERTY()
	TObjectPtr<class UMaterialInterface> BoneUnselectedMaterial;

	UPROPERTY()
	TObjectPtr<class UMaterialInterface> BoneNoCollisionMaterial;

	UPROPERTY()
	TArray<int32> HiddenBodies;

	UPROPERTY()
	TArray<int32> HiddenConstraints;

};

//////////////////////////////////////////////////////////////////////////
// UPhysicsAssetRenderUtilities

/** Factory class for FPhysicsAssetRenderSettings. */
UCLASS(config=EditorPerProjectUserSettings)
class PERSONA_API UPhysicsAssetRenderUtilities : public UObject
{
	GENERATED_BODY()

public:

	UPhysicsAssetRenderUtilities();
	virtual ~UPhysicsAssetRenderUtilities();

	static void Initialise();

	/** Returns an existing render settings object or creates and returns a new one if none exists. */
	static FPhysicsAssetRenderSettings* GetSettings(const UPhysicsAsset* InPhysicsAsset);
	static FPhysicsAssetRenderSettings* GetSettings(const FString& InPhysicsAssetPathName);
	static FPhysicsAssetRenderSettings* GetSettings(const uint32 InPhysicsAssetPathNameHash);
	
	static uint32 GetPathNameHash(const UPhysicsAsset* InPhysicsAsset);
	static uint32 GetPathNameHash(const FString& InPhysicsAssetPathName);

	void OnAssetRenamed(FAssetData const& AssetInfo, const FString& InOldPhysicsAssetPathName);
	void OnAssetRemoved(UObject* Object);

private:

	void InitialiseImpl();
	FPhysicsAssetRenderSettings* GetSettingsImpl(const uint32 InPhysicsAssetPathNameHash);

	UPROPERTY()
	TMap< uint32, FPhysicsAssetRenderSettings > IdToSettingsMap;

	UPROPERTY(transient)
	TObjectPtr<class UMaterialInterface> BoneUnselectedMaterial;

	UPROPERTY(transient)
	TObjectPtr<class UMaterialInterface> BoneNoCollisionMaterial;

	IPhysicsAssetRenderInterface* PhysicsAssetRenderInterface;
};

//////////////////////////////////////////////////////////////////////////
// PhysicsAssetRender

/**
	Namespace containing functions for debug draw of Physics Assets in the editor viewport.
*/
namespace PhysicsAssetRender
{
	template< typename TReturnType > using GetPrimitiveRef = TFunctionRef< TReturnType (const int32 BodyIndex, const EAggCollisionShape::Type PrimitiveType, const int32 PrimitiveIndex, const FPhysicsAssetRenderSettings& Settings) >;
	using GetPrimitiveTransformRef = TFunctionRef< FTransform (const UPhysicsAsset* PhysicsAsset, const FTransform& BoneTM, const int32 BodyIndex, const EAggCollisionShape::Type PrimType, const int32 PrimIndex, const float Scale) >;
	using CreateBodyHitProxyFn = TFunctionRef< HHitProxy* (const int32 BodyIndex, const EAggCollisionShape::Type PrimitiveType, const int32 PrimitiveIndex) >;
	using CreateConstraintHitProxyFn = TFunctionRef< HHitProxy* (const int32 InConstraintIndex) >;

	/** Debug draw Physics Asset bodies and constraints using the default callbacks */
	PERSONA_API void DebugDraw(class USkeletalMeshComponent* const SkeletalMeshComponent, class UPhysicsAsset* const PhysicsAsset, FPrimitiveDrawInterface* PDI);
	
	/** Debug draw Physics Asset bodies using the supplied custom callbacks */
	PERSONA_API void DebugDrawBodies(class USkeletalMeshComponent* const SkeletalMeshComponent, class UPhysicsAsset* const PhysicsAsset, FPrimitiveDrawInterface* PDI, GetPrimitiveRef< FColor > GetPrimitiveColor, GetPrimitiveRef< class UMaterialInterface* > GetPrimitiveMaterial, GetPrimitiveTransformRef GetPrimitiveTransform, CreateBodyHitProxyFn CreateHitProxy);
	
	/** Debug draw Physics Asset constraints using the supplied custom callbacks */
	PERSONA_API void DebugDrawConstraints(class USkeletalMeshComponent* const SkeletalMeshComponent, class UPhysicsAsset* const PhysicsAsset, FPrimitiveDrawInterface* PDI, TFunctionRef< bool(const uint32) > IsConstraintSelected, const bool bRunningSimulation, CreateConstraintHitProxyFn CreateHitProxy);
	
	/** Default callbacks used by DebugDraw */
	PERSONA_API FTransform GetPrimitiveTransform(const UPhysicsAsset* PhysicsAsset, const FTransform& BoneTM, const int32 BodyIndex, const EAggCollisionShape::Type PrimType, const int32 PrimIndex, const float Scale);
	PERSONA_API FColor GetPrimitiveColor(const int32 BodyIndex, const EAggCollisionShape::Type PrimitiveType, const int32 PrimitiveIndex, const FPhysicsAssetRenderSettings& Settings);
	PERSONA_API class UMaterialInterface* GetPrimitiveMaterial(const int32 BodyIndex, const EAggCollisionShape::Type PrimitiveType, const int32 PrimitiveIndex, const FPhysicsAssetRenderSettings& Settings);
}

class FPhysicsAssetRenderInterface : public IPhysicsAssetRenderInterface
{
public:
	virtual void DebugDraw(USkeletalMeshComponent* const SkeletalMeshComponent, UPhysicsAsset* const PhysicsAsset, FPrimitiveDrawInterface* PDI) override;
	virtual void DebugDrawBodies(USkeletalMeshComponent* const SkeletalMeshComponent, UPhysicsAsset* const PhysicsAsset, FPrimitiveDrawInterface* PDI, const FColor& PrimitiveColorOverride) override;
	virtual void DebugDrawConstraints(USkeletalMeshComponent* const SkeletalMeshComponent, UPhysicsAsset* const PhysicsAsset, FPrimitiveDrawInterface* PDI) override;

	virtual void SaveConfig() override;

	virtual void ToggleShowAllBodies(UPhysicsAsset* const PhysicsAsset) override;
	virtual void ToggleShowAllConstraints(UPhysicsAsset* const PhysicsAsset) override;
	virtual bool AreAnyBodiesHidden(UPhysicsAsset* const PhysicsAsset) override;
	virtual bool AreAnyConstraintsHidden(UPhysicsAsset* const PhysicsAsset) override;

	/** Returns a set of flags describing which components of the selected constraint's transforms are being manipulated in the viewport. */
	virtual EConstraintTransformComponentFlags GetConstraintViewportManipulationFlags(class UPhysicsAsset* const PhysicsAsset) override;

	/** Returns true if the constraint transform component specified by the flags should be displayed as an offset from the default (snapped) transforms. */
	virtual bool IsDisplayingConstraintTransformComponentRelativeToDefault(class UPhysicsAsset* const PhysicsAsset, const EConstraintTransformComponentFlags ComponentFlags) override;

	/** Change how the constraint transform component specified by the flags should be displayed, in the frame of their associated physics body or as an offset from the default (snapped) transforms. */
	virtual void SetDisplayConstraintTransformComponentRelativeToDefault(class UPhysicsAsset* const PhysicsAsset, const EConstraintTransformComponentFlags ComponentFlags, const bool bShouldDisplayRelativeToDefault) override;
};