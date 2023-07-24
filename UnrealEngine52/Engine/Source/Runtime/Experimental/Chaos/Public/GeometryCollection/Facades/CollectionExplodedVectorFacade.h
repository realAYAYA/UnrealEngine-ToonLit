// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryCollection/ManagedArrayAccessor.h"
#include "GeometryCollection/ManagedArrayCollection.h"


namespace GeometryCollection::Facades
{

	class CHAOS_API FCollectionExplodedVectorFacade
	{
	public:
		FCollectionExplodedVectorFacade(FManagedArrayCollection& InCollection);
		FCollectionExplodedVectorFacade(const FManagedArrayCollection& InCollection);

		/**
		 * returns true if all the necessary attributes are present
		 * if not then the API can be used to create
		 */
		bool IsValid() const;

		/**
		 * Add the necessary attributes if they are missing
		 */
		void DefineSchema();

		/**
		 * Adds exploded vector to global matrices translation 
		 */
		void UpdateGlobalMatricesWithExplodedVectors(TArray<FMatrix>& InOutGlobalMatrices) const;
		void UpdateGlobalMatricesWithExplodedVectors(TArray<FTransform>& InOutGlobalTransforms) const;

	private:

		TManagedArrayAccessor<FVector3f> ExplodedVectorAttribute;
	};

}