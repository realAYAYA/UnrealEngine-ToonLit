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
	class CHAOSCLOTH_API FClothingSimulationConfig final
	{
	public:
		FClothingSimulationConfig();
		FClothingSimulationConfig(const TSharedPtr<const FManagedArrayCollection>& InPropertyCollection);

		~FClothingSimulationConfig() = default;

		FClothingSimulationConfig(const FClothingSimulationConfig&) = delete;
		FClothingSimulationConfig(FClothingSimulationConfig&&) = delete;
		FClothingSimulationConfig& operator=(const FClothingSimulationConfig&) = delete;
		FClothingSimulationConfig& operator=(FClothingSimulationConfig&&) = delete;

		/** Initialize configuration from cloth config UObjects. */
		void Initialize(const UChaosClothConfig* ClothConfig, const UChaosClothSharedSimConfig* ClothSharedConfig);

		/** Initialize config from a property collection. */
		void Initialize(const TSharedPtr<const FManagedArrayCollection>& InPropertyCollection);

		/** Return a property collection facade for reading properties from this configuration. */
		const Softs::FCollectionPropertyConstFacade& GetProperties() const;

		/** Return a property collection facade for setting properties for this configuration. */
		Softs::FCollectionPropertyFacade& GetProperties();

		/** Return this configuration's internal property collection. */
		TSharedPtr<const FManagedArrayCollection> GetPropertyCollection() const { return TSharedPtr<const FManagedArrayCollection>(PropertyCollection); }

	private:
		TSharedPtr<FManagedArrayCollection> PropertyCollection;
		TUniquePtr<Softs::FCollectionPropertyMutableFacade> Properties;
	};
} // namespace Chaos
