// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryCollection/ManagedArrayAccessor.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/Facades/CollectionUVFacade.h"

namespace GeometryCollection::Facades
{

	class CHAOS_API FCollectionMeshFacade : public FCollectionUVFacade
	{
	public:
		FCollectionMeshFacade(FManagedArrayCollection& InCollection);
		FCollectionMeshFacade(const FManagedArrayCollection& InCollection);

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
		 * Returns the vertex indicies for the bone
		 */
		const TArray<int32> GetVertexIndices(int32 BoneIdx) const;

		/**
		 * Returns the vertex positions for the bone in bone space
		 */
		const TArrayView<const FVector3f> GetVertexPositions(int32 BoneIdx) const;

		/**
		 * Returns the face indices for the bone
		 */
		const TArray<int32> GetFaceIndices(int32 BoneIdx) const;

		/**
		 * Adds and sets "Internal" attribute in FGeometryCollection::FacesGroup to designate internal faces
		 */
		static void AddInternalAttribute(FGeometryCollection& InGeometryCollection, const TArray<int32>& InMaterialID);

		/**
		 * Returns the vertex indices of the face for the bone
		 */
//		const TArrayView<FIntVector> GetFaceVertexIndices(int32 BoneIdx) const;

		/**
		 * Bakes the transforms into the vertex positions and sets the bone transforms to identity
		 */
		 void BakeTransform(int32 TransformIdx, const FTransform& InTransform);

		TManagedArrayAccessor<int32> TransformToGeometryIndexAttribute;
		TManagedArrayAccessor<FVector3f> VertexAttribute;
		TManagedArrayAccessor<FVector3f> TangentUAttribute;
		TManagedArrayAccessor<FVector3f> TangentVAttribute;
		TManagedArrayAccessor<FVector3f> NormalAttribute;
		TManagedArrayAccessor<FLinearColor> ColorAttribute;
		TManagedArrayAccessor<int32> BoneMapAttribute;
		TManagedArrayAccessor<int32> VertexStartAttribute;
		TManagedArrayAccessor<int32> VertexCountAttribute;
		TManagedArrayAccessor<FIntVector> IndicesAttribute;
		TManagedArrayAccessor<bool> VisibleAttribute;
		TManagedArrayAccessor<int32> MaterialIndexAttribute;
		TManagedArrayAccessor<int32> MaterialIDAttribute;
		TManagedArrayAccessor<int32> FaceStartAttribute;
		TManagedArrayAccessor<int32> FaceCountAttribute;
	};

}