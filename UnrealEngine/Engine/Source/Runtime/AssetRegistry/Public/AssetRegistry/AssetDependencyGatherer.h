// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryState.h"
#include "Containers/Array.h"
#include "Containers/List.h"
#include "HAL/PreprocessorHelpers.h"
#include "Misc/AssetRegistryInterface.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

class FAssetRegistryState;
class FString;
class UClass;
struct FARCompiledFilter;
struct FARFilter;
struct FAssetData;
template <typename FuncType> class TFunctionRef;

/**
 * Interface class for functions that return extra dependencies to add to Assets of a given Class, 
 * to e.g. add dependencies on external Assets in a subdirectory. 
 */
class IAssetDependencyGatherer
{
public:
	struct FGathereredDependency
	{
		FName PackageName;
		UE::AssetRegistry::EDependencyProperty Property;
	};

	/**
	 * Return the extra dependencies to add to the given AssetData, which is guaranteed to 
	 * have ClassType equal to the class for which the Gatherer was registered.
	 * 
	 * WARNING: For high performance these callbacks are called inside the critical section of the AssetRegistry. 
	 * Attempting to call public functions on the AssetRegistry will deadlock. 
	 * To send queries about what assets exist, used the passed-in interface functions instead.
	 */
	virtual void GatherDependencies(const FAssetData& AssetData, const FAssetRegistryState& AssetRegistryState, 
		TFunctionRef<FARCompiledFilter(const FARFilter&)> CompileFilterFunc, TArray<IAssetDependencyGatherer::FGathereredDependency>& OutDependencies, 
		TArray<FString>& OutDependencyDirectories) const = 0;
};


namespace UE::AssetDependencyGatherer::Private
{
	class FRegisteredAssetDependencyGatherer
	{
	public:
		DECLARE_MULTICAST_DELEGATE(FOnAssetDependencyGathererRegistered);
		static ASSETREGISTRY_API FOnAssetDependencyGathererRegistered OnAssetDependencyGathererRegistered;

		ASSETREGISTRY_API FRegisteredAssetDependencyGatherer();
		ASSETREGISTRY_API virtual ~FRegisteredAssetDependencyGatherer();

		virtual UClass* GetAssetClass() const = 0;
		virtual void GatherDependencies(const FAssetData& AssetData, const FAssetRegistryState& AssetRegistryState, 
			TFunctionRef<FARCompiledFilter(const FARFilter&)> CompileFilterFunc, TArray<IAssetDependencyGatherer::FGathereredDependency>& OutDependencies, 
			TArray<FString>& OutDependencyDirectories) const = 0;

		static ASSETREGISTRY_API void ForEach(TFunctionRef<void(FRegisteredAssetDependencyGatherer*)> Func);
		static bool IsEmpty() { return !GetRegisteredList(); }
	private:

		static ASSETREGISTRY_API TLinkedList<FRegisteredAssetDependencyGatherer*>*& GetRegisteredList();
		TLinkedList<FRegisteredAssetDependencyGatherer*> GlobalListLink;
	};
}


/**
 * Used to Register an IAssetDependencyGatherer for a class
 *
 * Example usage:
 *
 * class FMyAssetDependencyGatherer : public IAssetDependencyGatherer { ... }
 * REGISTER_ASSETDEPENDENCY_GATHERER(FMyAssetDependencyGatherer, UMyAssetClass);
 */
#define REGISTER_ASSETDEPENDENCY_GATHERER(GathererClass, AssetClass) \
class PREPROCESSOR_JOIN(PREPROCESSOR_JOIN(GathererClass, AssetClass), _Register) : public UE::AssetDependencyGatherer::Private::FRegisteredAssetDependencyGatherer \
{ \
	virtual UClass* GetAssetClass() const override { return AssetClass::StaticClass(); } \
	virtual void GatherDependencies(const FAssetData& AssetData, const FAssetRegistryState& AssetRegistryState, TFunctionRef<FARCompiledFilter(const FARFilter&)> CompileFilterFunc, TArray<IAssetDependencyGatherer::FGathereredDependency>& OutDependencies, TArray<FString>& OutDependencyDirectories) const override \
	{ \
		GathererClass Gatherer; \
		Gatherer.GatherDependencies(AssetData, AssetRegistryState, CompileFilterFunc, OutDependencies, OutDependencyDirectories); \
	} \
}; \
namespace PREPROCESSOR_JOIN(GathererClass, AssetClass) \
{ \
	static PREPROCESSOR_JOIN(PREPROCESSOR_JOIN(GathererClass, AssetClass), _Register) DefaultObject; \
}

#endif // WITH_EDITOR
