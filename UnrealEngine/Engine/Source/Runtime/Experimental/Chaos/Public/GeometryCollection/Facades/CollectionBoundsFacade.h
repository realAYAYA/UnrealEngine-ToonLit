// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Containers/UnrealString.h"
#include "Containers/Array.h"
#include "Math/MathFwd.h"
#include "GeometryCollection/ManagedArrayAccessor.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "GeometryCollection/Facades/CollectionSelectionFacade.h"
#include "Chaos/Triangle.h"

namespace GeometryCollection::Facades
{

	/**
	* FBoundsFacade
	* 
	* Defines common API for calculating the bounding box on a collection
	* 
	*/
	class FBoundsFacade
	{
	public:

		/**
		* FBoundsFacade Constuctor
		* @param FManagedArrayCollection : Collection input
		*/
		CHAOS_API FBoundsFacade(FManagedArrayCollection& InSelf);
		CHAOS_API FBoundsFacade(const FManagedArrayCollection& InSelf);

		/** Create the facade attributes. */
		CHAOS_API void DefineSchema();

		/** Is the facade defined constant. */
		bool IsConst() const { return BoundingBoxAttribute.IsConst(); }

		/** Is the Facade defined on the collection? */
		CHAOS_API bool IsValid() const;		

		/** UpdateBoundingBox */
		CHAOS_API void UpdateBoundingBox();

		/** BoundingBox access */
		const TManagedArray< FBox >& GetBoundingBoxes() const { return BoundingBoxAttribute.Get(); }

		/** Centroids (Centers of BoundingBoxes) access */
		CHAOS_API TArray<FVector> GetCentroids() const;

		/** BoundingBox for the whole collection in Collection space */
		CHAOS_API FBox GetBoundingBoxInCollectionSpace() const;

		/** Returns the positions of the vertices of an FBox */
		static CHAOS_API TArray<FVector> GetBoundingBoxVertexPositions(const FBox& InBox);

		/** TransformToGeometryIndex Access */
		const TManagedArray< int32 >& GetTransformToGeometryIndex() const { return TransformToGeometryIndexAttribute.Get(); }
	protected:

		/** Transform based bounds evaluation, where the vertices are nested within a transform */
		void UpdateTransformBasedBoundingBox();

		/** Vertex based bounds evaluation, where the vertices are NOT nested within a transform */
		void UpdateVertexBasedBoundingBox();

	private:
		const FManagedArrayCollection& ConstCollection;
		FManagedArrayCollection* Collection = nullptr;

		TManagedArrayAccessor<FBox>			BoundingBoxAttribute;
		TManagedArrayAccessor<FVector3f>	VertexAttribute;
		TManagedArrayAccessor<int32>		BoneMapAttribute;
		TManagedArrayAccessor<int32>		TransformToGeometryIndexAttribute;
		TManagedArrayAccessor<int32>		VertexStartAttribute;
		TManagedArrayAccessor<int32>		VertexCountAttribute;
	};

}
