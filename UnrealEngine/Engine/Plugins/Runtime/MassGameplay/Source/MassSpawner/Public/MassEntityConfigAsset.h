// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MassEntityTemplate.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityConfigAsset.generated.h"

class UMassEntityConfigAsset;
class UMassEntityTraitBase;

/**
 * Describes a Mass agent to spawn. The struct can be embedded to allow last minute changes to the agent (i.e. for debugging).
 * The agent config describes a unique list of features which are used to create an entity template.
 * Derived configs can override parent features.
 */
USTRUCT()
struct MASSSPAWNER_API FMassEntityConfig
{
	friend class UMassTemplateRegistry;

	GENERATED_BODY()

	FMassEntityConfig() = default;
	FMassEntityConfig(UMassEntityConfigAsset& InParent);

	/** Create entity template based on the features included in this config.
	 *  @param World in which we are creating the template.
	 *  @param ConfigOwner Owner of the FMassEntityConfig used for error reporting.
	 */
	const FMassEntityTemplate& GetOrCreateEntityTemplate(const UWorld& World, const UObject& ConfigOwner) const;

	void DestroyEntityTemplate(const UWorld& World, const UObject& ConfigOwner) const;

	/** 
	 * Fetches the EntityTemplate for given World, fails a check if one cannot be found.
	 */
	const FMassEntityTemplate& GetEntityTemplateChecked(const UWorld& World, const UObject& ConfigOwner) const;

	/** @return Parent config */
	const UMassEntityConfigAsset* GetParent() const { return Parent; }

	/** @return View to the array of features defined on this config */
	TConstArrayView<UMassEntityTraitBase*> GetTraits() const { return Traits; }

	/** Adds Trait to the collection of traits hosted by this FMassEntityConfig instance */
	void AddTrait(UMassEntityTraitBase& Trait);

	/** Validates if the entity template is well built */
	bool ValidateEntityTemplate(const UWorld& World, const UObject& ConfigOwner);

protected:
	/** Combines traits based on the config hierarchy and returns list of unique traits */
	void GetCombinedTraits(TArray<UMassEntityTraitBase*>& OutTraits, TArray<const UObject*>& Visited, const UObject& ConfigOwner) const;

	/** Reference to parent config asset */
	UPROPERTY(Category = "Derived Traits", EditAnywhere)
	TObjectPtr<UMassEntityConfigAsset> Parent = nullptr;

	/** Array of unique traits of this config */
	UPROPERTY(Category = "Traits", EditAnywhere, Instanced)
	TArray<TObjectPtr<UMassEntityTraitBase>> Traits;

private:
	const FMassEntityTemplate* GetEntityTemplateInternal(const UWorld& World, const UObject& ConfigOwner, uint32& HashOut, FMassEntityTemplateID& TemplateIDOut, TArray<UMassEntityTraitBase*>& CombinedTraitsOut) const;
};

/**
 * Agent Config asset allows to create shared configs that can be used as base for derived configs.
 * The asset can be used as is i.e. on a spawner, or you can use FMassEntityConfig to allow last minute changes at use site.
 */
UCLASS(BlueprintType)
class MASSSPAWNER_API UMassEntityConfigAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** @return Agent config stored in this asset */
	const FMassEntityConfig& GetConfig() const { return Config; }

	/** @return Mutable agent config stored in this asset */
	FMassEntityConfig& GetMutableConfig() { return Config; }

	const FMassEntityTemplate& GetOrCreateEntityTemplate(const UWorld& World) const
	{
		return Config.GetOrCreateEntityTemplate(World, *this);
	}

	void DestroyEntityTemplate(const UWorld& World) const
	{
		Config.DestroyEntityTemplate(World, *this);
	}

#if WITH_EDITOR

	UFUNCTION(CallInEditor, Category = "Entity Config")
	void ValidateEntityConfig();

#endif // WITH_EDITOR

protected:
	/** The config described in this asset. */
	UPROPERTY(Category = "Entity Config", EditAnywhere)
	FMassEntityConfig Config;
};