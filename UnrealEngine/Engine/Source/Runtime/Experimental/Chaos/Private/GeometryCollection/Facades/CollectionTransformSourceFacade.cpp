// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GeometryCollection.cpp: FGeometryCollection methods.
=============================================================================*/

#include "GeometryCollection/Facades/CollectionTransformSourceFacade.h"
#include "GeometryCollection/TransformCollection.h"
#include "GeometryCollection/ManagedArrayAccessor.h"

namespace GeometryCollection::Facades
{
	// Groups 
	const FName FTransformSource::TransformSourceGroupName = "TransformSource";

	// Attributes
	const FName FTransformSource::SourceNameAttributeName = "Name";
	const FName FTransformSource::SourceGuidAttributeName = "GuidID";
	const FName FTransformSource::SourceRootsAttributeName = "Roots";

	FTransformSource::FTransformSource(FManagedArrayCollection& InCollection)
		: SourceNameAttribute(InCollection, SourceNameAttributeName, TransformSourceGroupName)
		, SourceGuidAttribute(InCollection, SourceGuidAttributeName, TransformSourceGroupName)
		, SourceRootsAttribute(InCollection, SourceRootsAttributeName, TransformSourceGroupName, FTransformCollection::TransformGroup)
	{}

	FTransformSource::FTransformSource(const FManagedArrayCollection& InCollection)
		: SourceNameAttribute(InCollection, SourceNameAttributeName, TransformSourceGroupName)
		, SourceGuidAttribute(InCollection, SourceGuidAttributeName, TransformSourceGroupName)
		, SourceRootsAttribute(InCollection, SourceRootsAttributeName, TransformSourceGroupName, FTransformCollection::TransformGroup)
	{}
	//
	//  Initialization
	//

	void FTransformSource::DefineSchema()
	{
		check(!IsConst());
		SourceNameAttribute.Add();
		SourceGuidAttribute.Add();
		SourceRootsAttribute.Add();
	}

	bool FTransformSource::IsValid() const
	{
		return SourceNameAttribute.IsValid() && SourceGuidAttribute.IsValid() && SourceRootsAttribute.IsValid(); 
	}

	//
	//  Add Data
	//
	void FTransformSource::AddTransformSource(const FString& InName, const FString& InGuid, const TSet<int32>& InRoots)
	{
		check(!IsConst());
		DefineSchema();

		int Idx = SourceNameAttribute.AddElements(1);
		SourceNameAttribute.Modify()[Idx] = InName;
		SourceGuidAttribute.Modify()[Idx] = InGuid;
		SourceRootsAttribute.Modify()[Idx] = InRoots;
	}

	//
	//  Get Data
	//
	TSet<int32> FTransformSource::GetTransformSource(const FString& InName, const FString& InGuid) const
	{
		if (IsValid())
		{
			const TManagedArray<TSet<int32>>& Roots = SourceRootsAttribute.Get();
			const TManagedArray<FString>& Guids = SourceGuidAttribute.Get();
			const TManagedArray<FString>&Names = SourceNameAttribute.Get();

			int32 GroupNum = SourceNameAttribute.Num();
			for (int i = 0; i < GroupNum; i++)
			{
				if (Guids[i] == InGuid && Names[i].Equals(InName))
				{
					return Roots[i];
				}
			}
		}
		return TSet<int32>();
	}
};


