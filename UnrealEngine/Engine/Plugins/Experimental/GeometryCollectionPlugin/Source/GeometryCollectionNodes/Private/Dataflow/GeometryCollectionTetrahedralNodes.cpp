// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/GeometryCollectionTetrahedralNodes.h"

#include "GeometryCollection/ManagedArrayCollection.h"


namespace Dataflow
{
	void GeometryCollectionTetrahedralNodes()
	{
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGenerateTetrahedralCollectionDataflowNodes);
	}
}


void FGenerateTetrahedralCollectionDataflowNodes::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<DataType>(&Collection))
	{
		DataType InCollection = GetValue<DataType>(Context, &Collection);

		//
		// @todo(dataflow) : Implemention tetrahedral generation for a closed surface.
		//

		SetValue<DataType>(Context, InCollection, &Collection);
	}
}

