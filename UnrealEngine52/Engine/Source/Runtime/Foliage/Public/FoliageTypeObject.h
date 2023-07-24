// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "Templates/SubclassOf.h"
#include "FoliageType_InstancedStaticMesh.h"
#include "FoliageTypeObject.generated.h"

/** A wrapper struct used to allow the use of either FoliageType assets or FoliageType blueprint classes */
USTRUCT()
struct FOLIAGE_API FFoliageTypeObject
{
	GENERATED_USTRUCT_BODY()

	FFoliageTypeObject()
		: FoliageTypeObject(nullptr), TypeInstance(nullptr), bIsAsset(false) {}

	/** Refreshes the type instance based on the assigned object. Intended to be called after some change is made. */
	void RefreshInstance();

	/** Gets the instance of this foliage type. Creates the instance if needed. */
	const UFoliageType* GetInstance();

	/** Gets the instance of this foliage type. */
	const UFoliageType* GetInstance() const;

	/** @return Whether this would return a valid instance */
	bool ContainsValidInstance() const;

	/** @return Whether any foliage type is assigned at all */
	bool HasFoliageType() const;

	bool IsDirty() const;
	void SetClean();

#if WITH_EDITORONLY_DATA
	void PostSerialize(const FArchive& Ar);
#endif

private:
	/** The foliage type that will be spawned by the procedural foliage simulation */
	UPROPERTY(Category = ProceduralFoliageSimulation, EditAnywhere, meta=(AllowedClasses="/Script/Foliage.FoliageType_InstancedStaticMesh,/Script/Foliage.FoliageType_Actor,/Script/Engine.Blueprint", DisplayThumbnail="true", ThumbnailSize="X=40 Y=40"))
	TObjectPtr<UObject> FoliageTypeObject;

	/** The actual instance of the foliage type that is used for spawning */
	UPROPERTY(transient)
	TObjectPtr<UFoliageType> TypeInstance;

	/** Whether this contains an asset object (as opposed to a BP class) */
	UPROPERTY()
	bool bIsAsset;

	UPROPERTY()
	TSubclassOf<UFoliageType_InstancedStaticMesh> Type_DEPRECATED;
};

#if WITH_EDITORONLY_DATA
template<>
struct TStructOpsTypeTraits<FFoliageTypeObject> : public TStructOpsTypeTraitsBase2<FFoliageTypeObject>
{
	enum
	{
		WithPostSerialize = true,
	};
};
#endif