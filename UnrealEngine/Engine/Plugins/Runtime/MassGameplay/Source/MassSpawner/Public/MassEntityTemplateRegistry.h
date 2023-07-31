// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MassCommonTypes.h"
#include "MassEntityTemplate.h"
#include "MassTranslator.h"
#include "MassEntityTemplateRegistry.generated.h"


class UWorld;
class UMassEntityTraitBase;

enum class EFragmentInitialization : uint8
{
	DefaultInitializer,
	NoInitializer
};

struct FMassEntityTemplateBuildContext
{
	explicit FMassEntityTemplateBuildContext(FMassEntityTemplate& InTemplate)
		: Template(InTemplate)
	{}

	//----------------------------------------------------------------------//
	// Fragments 
	//----------------------------------------------------------------------//
	template<typename T>
	T& AddFragment_GetRef()
	{
		TypeAdded(*T::StaticStruct());
		return Template.AddFragment_GetRef<T>();
	}

	template<typename T>
	void AddFragment()
	{
		TypeAdded(*T::StaticStruct());
		Template.AddFragment<T>();
	}

	void AddFragment(FConstStructView InFragment)
	{ 
		checkf(InFragment.GetScriptStruct(), TEXT("Expecting a valid fragment type"));
		TypeAdded(*InFragment.GetScriptStruct());
		Template.AddFragment(InFragment);
	}

	template<typename T>
	void AddTag()
	{
		// Tags can be added by multiple traits, so they do not follow the same rules as fragments
		Template.AddTag<T>();
	}

	void AddTag(const UScriptStruct& TagType)
	{
		// Tags can be added by multiple traits, so they do not follow the same rules as fragments
		Template.AddTag(TagType);
	}

	template<typename T>
	void AddChunkFragment()
	{
		TypeAdded(*T::StaticStruct());
		Template.AddChunkFragment<T>();
	}

	void AddConstSharedFragment(const FConstSharedStruct& InSharedFragment)
	{
		checkf(InSharedFragment.GetScriptStruct(), TEXT("Expecting a valide shared fragment type"));
		TypeAdded(*InSharedFragment.GetScriptStruct());
		Template.AddConstSharedFragment(InSharedFragment);
	}

	void AddSharedFragment(const FSharedStruct& InSharedFragment)
	{
		checkf(InSharedFragment.GetScriptStruct(), TEXT("Expecting a valide shared fragment type"));
		TypeAdded(*InSharedFragment.GetScriptStruct());
		Template.AddSharedFragment(InSharedFragment);
	}

	template<typename T>
	bool HasFragment() const
	{
		ensureMsgf(!BuildingTrait, TEXT("This method is not expected to be called within the build from trait call."));
		return Template.HasFragment<T>();
	}
	
	bool HasFragment(const UScriptStruct& ScriptStruct) const
	{
		ensureMsgf(!BuildingTrait, TEXT("This method is not expected to be called within the build from trait call."));
		return Template.HasFragment(ScriptStruct);
	}

	template<typename T>
	bool HasTag() const
	{
		return Template.HasTag<T>();
	}

	template<typename T>
	bool HasChunkFragment() const
	{
		ensureMsgf(!BuildingTrait, TEXT("This method is not expected to be called within the build from trait call."));
		return Template.HasChunkFragment<T>();
	}

	template<typename T>
	bool HasSharedFragment() const
	{
		ensureMsgf(!BuildingTrait, TEXT("This method is not expected to be called within the build from trait call."));
		return Template.HasSharedFragment<T>();
	}

	bool HasSharedFragment(const UScriptStruct& ScriptStruct) const
	{
		ensureMsgf(!BuildingTrait, TEXT("This method is not expected to be called within the build from trait call."));
		return Template.HasSharedFragment(ScriptStruct);
	}

	//----------------------------------------------------------------------//
	// Translators
	//----------------------------------------------------------------------//
	template<typename T>
	void AddTranslator()
	{
		TypeAdded(*T::StaticClass());
		GetDefault<T>()->AppendRequiredTags(Template.GetMutableTags());
	}

	//----------------------------------------------------------------------//
	// Dependencies
	//----------------------------------------------------------------------//
	template<typename T>
	void RequireFragment()
	{
		AddDependency(T::StaticStruct());
	}

	void AddDependency(const UStruct* Dependency)
	{
		TraitsDependencies.Add( {Dependency, BuildingTrait} );
	}

	//----------------------------------------------------------------------//
	// Template access
	//----------------------------------------------------------------------//
	FMassEntityTemplateID GetTemplateID() const { return Template.GetTemplateID(); }
	TArray<FMassEntityTemplate::FObjectFragmentInitializerFunction>& GetMutableObjectFragmentInitializers() { return Template.GetMutableObjectFragmentInitializers(); }

	//----------------------------------------------------------------------//
	// Build methods
	//----------------------------------------------------------------------//

	/**
	 * Builds context from a list of traits
	 * @param Traits is the list of all the traits to build an entity
	 * @param World owning the MassEntitySubsystem for which the entity template is built
	 * @return true if there were no validation errors
	 */
	bool BuildFromTraits(TConstArrayView<UMassEntityTraitBase*> Traits, const UWorld& World);

protected:

	/**
	 * Validate the build context for fragment trait ownership and trait fragment missing dependency
	 * @param World owning the MassEntitySubsystem for which the entity template is validated against
	 * @return true if there were no validation errors
	 */
	bool ValidateBuildContext(const UWorld& World);

	void TypeAdded(const UStruct& Type)
	{
		if (ensureMsgf(BuildingTrait, TEXT("Expected to be called within the BuildTemplateFromTrait method")))
		{
			TraitAddedTypes.Add(&Type, BuildingTrait);
		}
	}

	const UMassEntityTraitBase* BuildingTrait = nullptr;
	TMultiMap<const UStruct*, const UMassEntityTraitBase*> TraitAddedTypes;
	TArray< TTuple<const UStruct*, const UMassEntityTraitBase*> > TraitsDependencies;

	FMassEntityTemplate& Template;
};

/** @todo document 
 */
UCLASS()
class MASSSPAWNER_API UMassEntityTemplateRegistry : public UObject
{
	GENERATED_BODY()
public:
	// @todo consider TFunction instead
	DECLARE_DELEGATE_ThreeParams(FStructToTemplateBuilderDelegate, const UWorld* /*World*/, const FInstancedStruct& /*InStructInstance*/, FMassEntityTemplateBuildContext& /*BuildContext*/);

	UMassEntityTemplateRegistry() = default;
	virtual void BeginDestroy() override;
	virtual UWorld* GetWorld() const override;

	static FStructToTemplateBuilderDelegate& FindOrAdd(const UScriptStruct& DataType);

	const FMassEntityTemplate* FindOrBuildStructTemplate(const FInstancedStruct& StructInstance);

	/** Removes all the cached template instances */
	void DebugReset();

	const FMassEntityTemplate* FindTemplateFromTemplateID(FMassEntityTemplateID TemplateID) const;
	FMassEntityTemplate* FindMutableTemplateFromTemplateID(FMassEntityTemplateID TemplateID);

	FMassEntityTemplate& CreateTemplate(const uint32 HashLookup, FMassEntityTemplateID TemplateID);
	void DestroyTemplate(const uint32 HashLookup, FMassEntityTemplateID TemplateID);
	void InitializeEntityTemplate(FMassEntityTemplate& OutTemplate) const;

protected:
	/** @return true if a template has been built, false otherwise */
	bool BuildTemplateImpl(const FStructToTemplateBuilderDelegate& Builder, const FInstancedStruct& StructInstance, FMassEntityTemplate& OutTemplate);

protected:
	static TMap<const UScriptStruct*, FStructToTemplateBuilderDelegate> StructBasedBuilders;

	/** 
	 *  Map from a hash to a FMassEntityTemplateID
	 *  For build from FInstancedStruct it will be the Combined hash of the UScriptStruct FName and FTemplateRegistryHelpers::CalcHash()
	 *  This hash will not be deterministic between server and clients
	 */
	TMap<uint32, FMassEntityTemplateID> LookupTemplateIDMap;

	UPROPERTY(Transient)
	TMap<FMassEntityTemplateID, FMassEntityTemplate> TemplateIDToTemplateMap;
};

