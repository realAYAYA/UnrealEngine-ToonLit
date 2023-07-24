// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Algo/Transform.h"
#include "Templates/SubclassOf.h"

#include "HLODBuilder.generated.h"

class AActor;
class UActorComponent;

ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogHLODBuilder, Log, All);

/**
 * Base class for all HLOD Builder settings
 */
UCLASS()
class ENGINE_API UHLODBuilderSettings : public UObject
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR
public:
	/**
	 * Hash the class settings.
	 * This is used to detect settings changes and trigger an HLOD rebuild if necessarry.
	 */
	virtual uint32 GetCRC() const { return 0; }
#endif // WITH_EDITOR
};


/**
 * Provide context for the HLOD generation
 */
struct FHLODBuildContext
{
	/** World for which HLODs are being built */
	UWorld*	World;

	/** Actors that will be represented by this HLOD */
	TArray<AActor*> SourceActors;

	/** Outer to use for generated assets */
	UObject* AssetsOuter;

	/** Base name to use for generated assets */
	FString	AssetsBaseName;

	// Location of this HLOD actor in the world
	FVector WorldPosition;

	/** Minimum distance at which the HLOD is expected to be displayed */
	double MinVisibleDistance;
};


/**
 * Base class for all HLOD builders
 * This class takes as input a group of components, and should return component(s) that will be included in the HLOD actor.
 */
UCLASS(Abstract)
class ENGINE_API UHLODBuilder : public UObject
{
	 GENERATED_UCLASS_BODY()

#if WITH_EDITOR
public:
	/**
	 * Provide builder settings before a Build.
	 */
	void SetHLODBuilderSettings(const UHLODBuilderSettings* InHLODBuilderSettings);

	/**
	 * Build an HLOD representation of the input actors.
	 * Components returned by this method needs to be properly outered & assigned to your target (HLOD) actor.
	 */
	TArray<UActorComponent*> Build(const FHLODBuildContext& InHLODBuildContext) const;

	UE_DEPRECATED(5.2, "Use Build() method that takes a single FHLODBuildContext parameter. SourceActors are now part of the HLOD build context object.")
	TArray<UActorComponent*> Build(const FHLODBuildContext& InHLODBuildContext, const TArray<AActor*>& InSourceActors) const { return Build(InHLODBuildContext); }

	/**
	 * Return the setting subclass associated with this builder.
	 */
	virtual TSubclassOf<UHLODBuilderSettings> GetSettingsClass() const;

	/**
	 * Should return true if components generated from this builder need a warmup phase before being made visible.
	 * If your components are using virtual textures or Nanite meshes, this should return true, as it will be necessary
	 * to warmup the VT & Nanite caches before transitionning to HLOD. Otherwise, it's likely the initial first frames
	 * could show a low resolution texture or mesh.
	 */
	virtual bool RequiresWarmup() const;

	/**
	 * For a given component, compute a unique hash from the properties that are relevant to HLOD generation.
	 * Used to detect changes to the source components of an HLOD.
	 * The base version can only support hashing of static mesh components. HLOD builder subclasses
	 * should override this method and compute the hash of component types they support as input.
	 */
	virtual uint32 ComputeHLODHash(const UActorComponent* InSourceComponent) const;

	/**
	 * Components created with this method need to be properly outered & assigned to your target actor.
	 */
	virtual TArray<UActorComponent*> Build(const FHLODBuildContext& InHLODBuildContext, const TArray<UActorComponent*>& InSourceComponents) const PURE_VIRTUAL(UHLODBuilder::Build, return {};);

	/**
	 * From a set of actors, compute a unique hash from their properties that are relevant to HLOD generation.
	 * Used to detect changes to the source actors and trigger an HLOD rebuild if necessarry.
	 */
	static uint32 ComputeHLODHash(const TArray<AActor*>& InSourceActors);

protected:
	virtual bool ShouldIgnoreBatchingPolicy() const { return false; }

	static TArray<UActorComponent*> BatchInstances(const TArray<UActorComponent*>& InSubComponents);

	template <typename TComponentClass>
	static inline TArray<TComponentClass*> FilterComponents(const TArray<UActorComponent*>& InSourceComponents)
	{
		TArray<TComponentClass*> FilteredComponents;
		FilteredComponents.Reserve(InSourceComponents.Num());
		Algo::TransformIf(InSourceComponents, FilteredComponents, [](UActorComponent* SourceComponent) { return Cast<TComponentClass>(SourceComponent); }, [](UActorComponent* SourceComponent) { return Cast<TComponentClass>(SourceComponent); });
		if (InSourceComponents.Num() != FilteredComponents.Num())
		{
			UE_LOG(LogHLODBuilder, Warning, TEXT("Excluding %d components from the HLOD build."), InSourceComponents.Num() - FilteredComponents.Num());
		}
		return FilteredComponents;
	}

#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
protected:
	UPROPERTY()
	TObjectPtr<const UHLODBuilderSettings> HLODBuilderSettings;
#endif
};


/**
 * Null HLOD builder that ignores it's input and generate no component.
 */
UCLASS(HideDropdown)
class ENGINE_API UNullHLODBuilder : public UHLODBuilder
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR
	virtual bool RequiresWarmup() const { return false; }
	virtual uint32 ComputeHLODHash(const UActorComponent* InSourceComponent) const { return 0; }
	virtual TArray<UActorComponent*> Build(const FHLODBuildContext& InHLODBuildContext, const TArray<UActorComponent*>& InSourceComponents) const { return {}; }
#endif
};
