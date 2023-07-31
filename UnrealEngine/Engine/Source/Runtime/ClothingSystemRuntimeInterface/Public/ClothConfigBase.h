// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "ClothConfigBase.generated.h"

/**
 * Base class for simulator specific simulation controls.
 * Each cloth instance on a skeletal mesh can have a unique cloth config
 */
UCLASS(Abstract)
class CLOTHINGSYSTEMRUNTIMEINTERFACE_API UClothConfigBase : public UObject
{
	GENERATED_BODY()
public:
	UClothConfigBase();
	virtual ~UClothConfigBase();

	/** Return the self collision radius if building self collision indices is required for this config, or 0.f otherwise. */
	UE_DEPRECATED(5.0, "Use NeedsSelfCollisionData and GetSelfCollisionRadius instead.")
	virtual float NeedsSelfCollisionIndices() const { return NeedsSelfCollisionData() ? GetSelfCollisionRadius() : 0.f; }

	/** Return wherether to pre-compute self collision data. */
	virtual bool NeedsSelfCollisionData() const
	PURE_VIRTUAL(UClothConfigBase::NeedsSelfCollisionData, return false;);

	/** Return wherether to pre-compute inverse masses. */
	virtual bool NeedsInverseMasses() const
	PURE_VIRTUAL(UClothConfigBase::NeedsInverseMasses, return false;);

	/** Return wherether to pre-compute the influences. */
	virtual bool NeedsNumInfluences() const
	PURE_VIRTUAL(UClothConfigBase::NeedsNumInfluences, return false;);

	/** Return wherether to pre-compute the long range attachment tethers. */
	virtual bool NeedsTethers() const
	PURE_VIRTUAL(UClothConfigBase::NeedsTethers, return false;);

	/** Return the self collision radius to precomute self collision data. */
	virtual float GetSelfCollisionRadius() const
	PURE_VIRTUAL(UClothConfigBase::GetSelfCollisionRadius, return 0.f;);

	/** Return whether tethers need to be calculated using geodesic distances instead of eclidean. */
	virtual bool TethersUseGeodesicDistance() const
	PURE_VIRTUAL(UClothConfigBase::TethersUseGeodesicDistance, return false;);
};

/**
 * These settings are shared between all instances on a skeletal mesh
 * Deprecated, use UClothConfigBase instead.
 */
UCLASS(Abstract, Deprecated)
class CLOTHINGSYSTEMRUNTIMEINTERFACE_API UDEPRECATED_ClothSharedSimConfigBase : public UObject
{
	GENERATED_BODY()
public:
	UDEPRECATED_ClothSharedSimConfigBase() {}
	virtual ~UDEPRECATED_ClothSharedSimConfigBase() {}

	/**
	 * Return a new updated cloth shared config migrated from this current object.
	 */
	virtual UClothConfigBase* Migrate() { return nullptr; }
};
