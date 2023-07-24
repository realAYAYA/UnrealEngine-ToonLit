// Copyright Epic Games, Inc. All Rights Reserved.


#include "GeometryCollection/Facades/CollectionConnectionGraphFacade.h"
#include "GeometryCollection/TransformCollection.h"

namespace GeometryCollection::Facades
{

	FCollectionConnectionGraphFacade::FCollectionConnectionGraphFacade(FManagedArrayCollection& InCollection)
		: ConnectionsAttribute(InCollection, "Connections", FTransformCollection::TransformGroup)
#if UE_BUILD_DEBUG
		, ParentAttribute(InCollection, "Parent", FTransformCollection::TransformGroup)
#endif
	{
	}

	FCollectionConnectionGraphFacade::FCollectionConnectionGraphFacade(const FManagedArrayCollection& InCollection)
		: ConnectionsAttribute(InCollection, "Connections", FTransformCollection::TransformGroup)
#if UE_BUILD_DEBUG
		, ParentAttribute(InCollection, "Parent", FTransformCollection::TransformGroup)
#endif
	{
	}

	bool FCollectionConnectionGraphFacade::IsValid() const
	{
		return ConnectionsAttribute.IsValid();
	}

	void FCollectionConnectionGraphFacade::DefineSchema()
	{
		check(!IsConst());

		ConnectionsAttribute.Add();
	}
	
	void FCollectionConnectionGraphFacade::ClearAttributes()
	{
		check(!IsConst());

		ConnectionsAttribute.Remove();
	}

	void FCollectionConnectionGraphFacade::Connect(int32 BoneA, int32 BoneB)
	{
		check(!IsConst());

#if UE_BUILD_DEBUG
		// Expect to always have a parent array and connected bones should always have the same parent
		checkSlow(ParentAttribute.IsValid() && ParentAttribute.Get()[BoneA] == ParentAttribute.Get()[BoneB]);
#endif
		TManagedArray<TSet<int32>>& Connections = ConnectionsAttribute.Modify();
		Connections[BoneA].Add(BoneB);
		Connections[BoneB].Add(BoneA);
	}

};


