// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"

struct FManagedArrayCollection;
class UChaosClothConfig;
class UChaosClothSharedSimConfig;

namespace Chaos
{
	namespace Softs
	{
		class FCollectionPropertyConstFacade;
		class FCollectionPropertyFacade;
		class FCollectionPropertyMutableFacade;
	}

	// Cloth simulation properties
	class FClothingSimulationConfig final
	{
	public:
		CHAOSCLOTH_API FClothingSimulationConfig();
		UE_DEPRECATED(5.3, "Use TArray version of this constructor.")
		CHAOSCLOTH_API FClothingSimulationConfig(const TSharedPtr<const FManagedArrayCollection>& InPropertyCollection);
		CHAOSCLOTH_API FClothingSimulationConfig(const TArray<TSharedPtr<const FManagedArrayCollection>>& InPropertyCollections);

		CHAOSCLOTH_API ~FClothingSimulationConfig();

		FClothingSimulationConfig(const FClothingSimulationConfig&) = delete;
		FClothingSimulationConfig(FClothingSimulationConfig&&) = delete;
		FClothingSimulationConfig& operator=(const FClothingSimulationConfig&) = delete;
		FClothingSimulationConfig& operator=(FClothingSimulationConfig&&) = delete;

		/**
		 * Initialize configuration from cloth config UObjects.
		 * @param ClothConfig The cloth config UObject.
		 * @param ClothSharedConfig The cloth solver shared config UObject.
		 * @param bUseLegacyConfig Whether to make the config a legacy cloth config, so that the constraints disable themselves with missing masks, ...etc.
		 */
		CHAOSCLOTH_API void Initialize(const UChaosClothConfig* ClothConfig, const UChaosClothSharedSimConfig* ClothSharedConfig, bool bUseLegacyConfig = false);

		/** Initialize config from a property collection. */
		UE_DEPRECATED(5.3, "Use TArray version of Initialize.")
		CHAOSCLOTH_API void Initialize(const TSharedPtr<const FManagedArrayCollection>& InPropertyCollection)
		{
			Initialize(TArray<TSharedPtr<const FManagedArrayCollection>>({ InPropertyCollection }));
			bIsLegacySingleLOD = true;
		}

		/** Initialize config from an array of property collections (one per LOD). */
		CHAOSCLOTH_API void Initialize(const TArray<TSharedPtr<const FManagedArrayCollection>>& InPropertyCollections);

		/** Return a property collection facade for reading properties from this configuration. */
		CHAOSCLOTH_API const Softs::FCollectionPropertyConstFacade& GetProperties(int32 LODIndex) const;
		CHAOSCLOTH_API const Softs::FCollectionPropertyConstFacade& GetProperties() const 
		{
			ensureMsgf(bIsLegacySingleLOD, TEXT("LODIndex-less version should only be used with legacy single LOD configs."));
			return GetProperties(0); 
		}

		/** Return a property collection facade for setting properties for this configuration. */
		CHAOSCLOTH_API Softs::FCollectionPropertyFacade& GetProperties(int32 LODIndex);
		CHAOSCLOTH_API Softs::FCollectionPropertyFacade& GetProperties() 
		{
			ensureMsgf(bIsLegacySingleLOD, TEXT("LODIndex-less version should only be used with legacy single LOD configs."));
			return GetProperties(0); 
		}

		/** Return this configuration's internal property collection. */
		TSharedPtr<const FManagedArrayCollection> GetPropertyCollection(int32 LODIndex) const 
		{
			return bIsLegacySingleLOD ? TSharedPtr<const FManagedArrayCollection>(PropertyCollections[0]) : 
				TSharedPtr<const FManagedArrayCollection>(PropertyCollections[LODIndex]);
		}
		TSharedPtr<const FManagedArrayCollection> GetPropertyCollection() const 
		{
			ensureMsgf(bIsLegacySingleLOD, TEXT("LODIndex-less version should only be used with legacy single LOD configs."));
			return GetPropertyCollection(0); 
		}

		bool IsLegacySingleLOD() const { return bIsLegacySingleLOD; }
		int32 GetNumLODs() const { return PropertyCollections.Num(); }
		bool IsValidLOD(int32 LODIndex) const
		{
			return bIsLegacySingleLOD ? true : Properties.IsValidIndex(LODIndex);
		}

		template<typename FunctionType>
		void ForAllProperties(FunctionType Func)
		{
			for (TUniquePtr<Softs::FCollectionPropertyMutableFacade>& Property : Properties)
			{
				Func((Softs::FCollectionPropertyFacade&)(*Property));
			}
		}

		template<typename FunctionType>
		void ForAllProperties(FunctionType Func) const
		{
			for (const TUniquePtr<Softs::FCollectionPropertyMutableFacade>& Property : Properties)
			{
				Func((Softs::FCollectionPropertyConstFacade&)(*Property));
			}
		}

	private:
		bool bIsLegacySingleLOD = false;
		TArray<TSharedPtr<FManagedArrayCollection>> PropertyCollections;
		TArray<TUniquePtr<Softs::FCollectionPropertyMutableFacade>> Properties;
	};
} // namespace Chaos
