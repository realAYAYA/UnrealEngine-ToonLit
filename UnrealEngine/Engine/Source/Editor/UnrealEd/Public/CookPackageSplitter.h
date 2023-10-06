// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "AssetRegistry/IAssetRegistry.h"
#include "Containers/Array.h"
#include "Containers/List.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Hash/Blake3.h"
#include "HAL/PlatformCrt.h"
#include "HAL/PreprocessorHelpers.h"
#include "Misc/Optional.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

class UClass;
class UObject;
class UPackage;
struct FAssetDependency;
template <typename FuncType> class TFunctionRef;

/**
 * This class is used for packages that need to be split into multiple runtime packages.
 * It provides the instructions to the cooker for how to split the package.
 */
class ICookPackageSplitter
{
public:
	// Static API functions - these static functions are referenced by REGISTER_COOKPACKAGE_SPLITTER
	// before creating an instance of the class.
	/** Return whether the CookPackageSplitter subclass should handle the given SplitDataClass instance. */
	static bool ShouldSplit(UObject* SplitData) { return false; }
	/** Return DebugName for this SplitterClass in cook log messages. */
	static FString GetSplitterDebugName() { return TEXT("<NoNameSpecified>"); }


	// Virtual API functions - functions called from the cooker after creating the splitter.
	virtual ~ICookPackageSplitter() {}

	enum class ETeardown
	{
		Complete,
		Canceled,
	};
	/** Do teardown actions after all packages have saved, or when the cook is cancelled. Always called before destruction. */
	virtual void Teardown(ETeardown Status) {}

	/**
	 * If true, this splitter forces the Generator package objects it needs to remain referenced, and the cooker
	 * should expect them to still be in memory after a garbage collect so long as the splitter is alive.
	 */
	virtual bool UseInternalReferenceToAvoidGarbageCollect() { return false; }

	/** Data sent to the cooker to describe each desired generated package */
	struct FGeneratedPackage
	{
		/** Parent path for the generated package. If empty, uses the generator's package path. */
		FString GeneratedRootPath;
		/** Generated package relative to <GeneratedRootPath>/_Generated_. */
		FString RelativePath;
		UE_DEPRECATED(5.3, "Write to PackageDependencies instead")
		TArray<FName> Dependencies;
		/**
		 * AssetRegistry dependencies for the generated package. AR dependencies cause packages to be added to the
		 * current cook and cause invalidation of this package in iterative cooks if any dependencies change.
		 */
		TArray<FAssetDependency> PackageDependencies;
		/**
		 * Hash of the data used to construct the generated package that is not covered by the dependencies.
		 * Changes to this hash will cause invalidation of the package during iterative cooks.
		 */
		FBlake3Hash GenerationHash;
		/* GetGenerateList must specify true if the package will be a map (.umap, contains a UWorld or ULevel), else false */
		void SetCreateAsMap(bool bInCreateAsMap) { bCreateAsMap = bInCreateAsMap; }
		const TOptional<bool>& GetCreateAsMap() const { return bCreateAsMap; }
	private:
		TOptional<bool> bCreateAsMap;
	};

	/** Return the list of packages to generate. */
	virtual TArray<FGeneratedPackage> GetGenerateList(const UPackage* OwnerPackage, const UObject* OwnerObject) = 0;

	/** Representation of a generated package that is provided when populating the generator package. */
	struct FGeneratedPackageForPreSave
	{
		/** RelativePath returned from GetGenerateList */
		FString RelativePath;
		/** Root returned from GetGenerateList */
		FString GeneratedRootPath;
		/**
		 * Non-null UPackage. Possibly an empty placeholder package. Provided so that the generator package
		 * can create import references to objects that will be stored in the generated package.
		 */
		UPackage* Package = nullptr;
		/** *GetCreateAsMap returned from GetGenerateList. The package filename extension has already been set based on this. */
		bool bCreatedAsMap = false;
	};
	/**
	 * Called before presaving the parent generator package, to give the generator a chance to inform the cooker which objects will
	 * be moved into the generator package that are not already present in it.
	 * 
	 * @param OwnerPackage				The generator package being split
	 * @param OwnerObject				The SplitDataClass instance that this CookPackageSplitter instance was created for
	 * @param GeneratedPackages			Placeholder UPackage and relative path information for all packages that will be generated
	 * @param OutObjectsToMove			List of all the objects that will be moved into the Generator package during its save
	 * @param OutKeepReferencedPackages A list of packages which should be kept referenced until all generated packages for
	 *                                  the generator have finished saving.
	 * 
	 * @return							True if successfully populated, false on error (this will cause a cook error).
	 */
	virtual bool PopulateGeneratorPackage(UPackage* OwnerPackage, UObject* OwnerObject,
		const TArray<ICookPackageSplitter::FGeneratedPackageForPreSave>& GeneratedPackages, TArray<UObject*>& OutObjectsToMove,
		TArray<UPackage*>& OutKeepReferencedPackages)
	{
		return true;
	}

	/**
	 * Called before saving the parent generator package, after PopulateGeneratorPackage but before PopulateGeneratedPackage for any
	 * generated packages. Make any required adjustments to the parent package before it is saved into the target domain.
	 *
	 * @param OwnerPackage				The generator package being split
	 * @param OwnerObject				The SplitDataClass instance that this CookPackageSplitter instance was created for
	 * @param GeneratedPackages			Placeholder UPackage and relative path information for all packages that will be generated
	 * @param OutKeepReferencedPackages A list of packages which should be kept referenced until all generated packages for
	 *                                  the generator have finished saving.
	 *
	 * @return							True if successfully presaved, false on error (this will cause a cook error).
	 */
	virtual bool PreSaveGeneratorPackage(UPackage* OwnerPackage, UObject* OwnerObject,
		const TArray<FGeneratedPackageForPreSave>& PlaceholderPackages, TArray<UPackage*>& OutKeepReferencedPackages)
	{
		return true;
	}

	/**
	 * Called after saving the parent generator package. Undo any required adjustments to the parent package that
	 * were made in PreSaveGeneratorPackage, so that the package is once again ready for use in the editor or in
	 * future GetGenerateList or PreSaveGeneratedPackage calls
	 */
	virtual void PostSaveGeneratorPackage(UPackage* OwnerPackage, UObject* OwnerObject)
	{
	}

	/** Representation of a generated package when it itself is being populated. */
	struct FGeneratedPackageForPopulate
	{
		/** RelativePath returned from GetGenerateList */
		FString RelativePath;
		/** Root returned from GetGenerateList */
		FString GeneratedRootPath;
		/**
		 * The UPackage that has been created for the package. Possibly empty, but may also contain still have modifications
		 * that were made during PopulateGeneratorPackage.
		 */
		UPackage* Package = nullptr;
		/** *GetCreateAsMap returned from GetGenerateList. The package filename extension has already been set based on this. */
		bool bCreatedAsMap = false;
	};

	/**
	 * Try to populate a generated package.
	 *
	 * Receive an empty UPackage generated from an element in GetGenerateList and populate it.
	 * Return a list of all the objects that will be moved into the Generated package during its save, so the cooker
	 * can call BeginCacheForCookedPlatformData on them before the move
	 * After returning, the given package will be queued for saving into the TargetDomain
	 *
	 * @param OwnerPackage				The parent package being split
	 * @param OwnerObject				The SplitDataClass instance that this CookPackageSplitter instance was created for
	 * @param GeneratedPackage			Pointer and information about the package to populate
	 * @param OutObjectsToMove			List of all the objects that will be moved into the generated package during its save
	 * @param OutKeepReferencedPackages A list of packages which should be kept referenced until all generated packages for
	 *                                  for the generator have finished saving.
	 * 
	 * @return							True if successfully populated, false on error (this will cause a cook error).
	 */
	virtual bool PopulateGeneratedPackage(UPackage* OwnerPackage, UObject* OwnerObject,
		const FGeneratedPackageForPopulate& GeneratedPackage, TArray<UObject*>& OutObjectsToMove,
		TArray<UPackage*>& OutKeepReferencedPackages)
	{
		return true;
	}
	
	/**
	 * Called before saving a generated package, after PopulateGeneratedPackage. Make any required adjustments to the
	 * generated package before it is saved into the target domain.
	 * 
	 * @param OwnerPackage				The parent package being split
	 * @param OwnerObject				The SplitDataClass instance that this CookPackageSplitter instance was created for
	 * @param GeneratedPackage			Pointer and information about the package to populate
	 * @param OutKeepReferencedPackages A list of packages which should be kept referenced until all generated packages for
	 *                                  for the generator have finished saving.
	 * 
	 * @return							True if successfully presaved, false on error (this will cause a cook error).
	 */
	virtual bool PreSaveGeneratedPackage(UPackage* OwnerPackage, UObject* OwnerObject,
		const FGeneratedPackageForPopulate& GeneratedPackage, TArray<UPackage*>& OutKeepReferencedPackages)
	{
		return true;
	};

	/**
	 * Called after saving a generated package. Undo any required adjustments to the parent package that
	 * were made in PreSaveGeneratedPackage, so that the parent package is once again ready for use in the editor or in
	 * future PreSaveGeneratedPackage calls.
	 */
	virtual void PostSaveGeneratedPackage(UPackage* OwnerPackage, UObject* OwnerObject,
		const FGeneratedPackageForPopulate& GeneratedPackage)
	{
	}

	/** Called when the Owner package needs to be reloaded after a garbage collect in order to populate a generated package. */
	virtual void OnOwnerReloaded(UPackage* OwnerPackage, UObject* OwnerObject) {}
};

namespace UE
{
namespace Cook
{
namespace Private
{

/** Interface for internal use only (used by REGISTER_COOKPACKAGE_SPLITTER to register an ICookPackageSplitter for a class) */
class FRegisteredCookPackageSplitter
{
public:
	UNREALED_API FRegisteredCookPackageSplitter();
	UNREALED_API virtual ~FRegisteredCookPackageSplitter();

	virtual UClass* GetSplitDataClass() const = 0;
	virtual bool ShouldSplitPackage(UObject* Object) const = 0;
	virtual ICookPackageSplitter* CreateInstance(UObject* Object) const = 0;
	virtual FString GetSplitterDebugName() const = 0;

	static UNREALED_API void ForEach(TFunctionRef<void(FRegisteredCookPackageSplitter*)> Func);

private:
	
	static UNREALED_API TLinkedList<FRegisteredCookPackageSplitter*>*& GetRegisteredList();
	TLinkedList<FRegisteredCookPackageSplitter*> GlobalListLink;
};

}
}
}

/**
 * Used to Register an ICookPackageSplitter for a class
 *
 * Example usage:
 *
 * class FMyCookPackageSplitter : public ICookPackageSplitter { ... }
 * REGISTER_COOKPACKAGE_SPLITTER(FMyCookPackageSplitter, UMySplitDataClass);
 */
#define REGISTER_COOKPACKAGE_SPLITTER(SplitterClass, SplitDataClass) \
class PREPROCESSOR_JOIN(PREPROCESSOR_JOIN(SplitterClass, SplitDataClass), _Register) : public UE::Cook::Private::FRegisteredCookPackageSplitter \
{ \
	virtual UClass* GetSplitDataClass() const override { return SplitDataClass::StaticClass(); } \
	virtual bool ShouldSplitPackage(UObject* Object) const override { return SplitterClass::ShouldSplit(Object); } \
	virtual ICookPackageSplitter* CreateInstance(UObject* SplitData) const override { return new SplitterClass(); } \
	virtual FString GetSplitterDebugName() const override { return SplitterClass::GetSplitterDebugName(); } \
}; \
namespace PREPROCESSOR_JOIN(SplitterClass, SplitDataClass) \
{ \
	static PREPROCESSOR_JOIN(PREPROCESSOR_JOIN(SplitterClass, SplitDataClass), _Register) DefaultObject; \
}

#endif
