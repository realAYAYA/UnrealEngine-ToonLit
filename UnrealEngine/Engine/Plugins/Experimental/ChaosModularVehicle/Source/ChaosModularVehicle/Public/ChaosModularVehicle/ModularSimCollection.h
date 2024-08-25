// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeometryCollection/GeometryCollection.h"

namespace Chaos
{
	class FChaosArchive;
}

/**
* FModularSimCollection (FTransformCollection)
*/
//class CHAOSMODULARVEHICLE_API FModularSimCollection : public FTransformCollection
class CHAOSMODULARVEHICLE_API FModularSimCollection : public FGeometryCollection
{
public:
	//typedef FTransformCollection Super;
	typedef FGeometryCollection Super;

	FModularSimCollection();
	FModularSimCollection(FModularSimCollection&) = delete;
	FModularSimCollection& operator=(const FModularSimCollection&) = delete;
	FModularSimCollection(FModularSimCollection&&) = default;
	FModularSimCollection& operator=(FModularSimCollection&&) = default;

	/**
	 * Create a GeometryCollection from Vertex and Indices arrays
	 */
	static FModularSimCollection* NewModularSimulationCollection(const FTransformCollection& Base);
	static FModularSimCollection* NewModularSimulationCollection();
	static void Init(FModularSimCollection* Collection);

	/*
	* Index of simulation module associated with each transform node
	*   FManagedArray<int32> SimModuleIndex = this->FindAttribute<int32>("SimModuleIndex",FGeometryCollection::TransformGroup);
	*/
	static const FName SimModuleIndexAttribute;
	TManagedArray<int32> SimModuleIndex;

	// TODO: need to do this conversion somewhere - putting here for now
	void GenerateSimTree();

protected:
	void Construct();

};

FORCEINLINE Chaos::FChaosArchive& operator<<(Chaos::FChaosArchive& Ar, FModularSimCollection& Value)
{
	Value.Serialize(Ar);
	return Ar;
}
