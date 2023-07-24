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
	* FRenderingFacade
	*
	* Defines common API for storing rendering data.
	*
	*/
	class CHAOS_API FRenderingFacade
	{
	public:
		typedef FGeometryCollectionSection FTriangleSection;
		typedef TMap<FString, int32> FStringIntMap;
		/**
		* FRenderingFacade Constuctor
		* @param VertexDependencyGroup : GroupName the index attribute is dependent on.
		*/
		FRenderingFacade(FManagedArrayCollection& InSelf);
		FRenderingFacade(const FManagedArrayCollection& InSelf);

		/**Create the facade.*/
		void DefineSchema();

		/** Is the facade defined constant. */
		bool IsConst() const { return Collection==nullptr; }

		/**Is the Facade defined on the collection?*/
		bool IsValid() const;

		/**Does it support rendering surfaces.*/
		bool CanRenderSurface() const;

		//
		// Facade API
		//

		/**Number of triangles to render.*/
		int32 NumTriangles() const;

		/**Add a triangle to the rendering view.*/
		void AddTriangle(const Chaos::FTriangle& InTriangle);

		/** Add a surface to the rendering view.*/
		void AddSurface(TArray<FVector3f>&& InVertices, TArray<FIntVector>&& InIndices, TArray<FVector3f>&& InNormals, TArray<FLinearColor>&& InColors);
		
		/** GetIndices */
		const TManagedArray< FIntVector >& GetIndices() const { return IndicesAttribute.Get(); }

		/** GetMaterialID */
		const TManagedArray< int32 >& GetMaterialID() const { return MaterialIDAttribute.Get(); }

		/** GetTriangleSections */
		const TManagedArray< FTriangleSection >& GetTriangleSections() const { return TriangleSectionAttribute.Get(); }

		/** BuildMeshSections */
		TArray<FTriangleSection> BuildMeshSections(const TArray<FIntVector>& Indices, TArray<int32> BaseMeshOriginalIndicesIndex, TArray<FIntVector>& RetIndices) const;


		//
		//  Vertices
		//

		/** GetVertices */
		const TManagedArray< FVector3f >& GetVertices() const { return VertexAttribute.Get(); }

		/** GetVertexSelectionAttribute */
		const TManagedArray< int32 >& GetVertexSelection() const { return VertexSelectionAttribute.Get(); }
		      TManagedArray< int32 >& ModifyVertexSelection() { check(!IsConst()); return VertexSelectionAttribute.Modify(); }

		/** GetVertexToGeometryIndexAttribute */
		const TManagedArray< int32 >& GetVertexToGeometryIndex() const { return VertexToGeometryIndexAttribute.Get(); }

		/** HitProxyIDAttribute */
		const TManagedArray< int32 >& GetVertexHitProxyIndex() const { return VertexHitProxyIndexAttribute.Get(); }
		TManagedArray< int32 >& ModifyVertexHitProxyIndex() { check(!IsConst()); return VertexHitProxyIndexAttribute.Modify(); }

		/** NumVertices */
		int32 NumVertices() const { return VertexAttribute.Num(); }

		/** GetVertexColorAttribute */
		const TManagedArray<FLinearColor>& GetVertexColor() const { return VertexColorAttribute.Get(); }
		TManagedArray<FLinearColor>& ModifyVertexColor() { check(!IsConst()); return VertexColorAttribute.Modify(); }

		//
		// Geometry Group Attributes
		//

		/** Geometry Group Start : */
		int32 StartGeometryGroup(FString InName);

		/** Geometry Group End : */
		void EndGeometryGroup(int32 InGeometryGroupIndex);

		int32 NumGeometry() const { return GeometryNameAttribute.Num(); }

		/** GeometryNameAttribute */
		const TManagedArray< FString >& GetGeometryName() const { return GeometryNameAttribute.Get(); }

		/** HitProxyIDAttribute */
		const TManagedArray< int32 >& GetGeometryHitProxyIndex() const { return GeometryHitProxyIndexAttribute.Get(); }
		      TManagedArray< int32 >& ModifyGeometryHitProxyIndex() {check(!IsConst());return GeometryHitProxyIndexAttribute.Modify(); }

		/** VertexStartAttribute */
		const TManagedArray< int32 >& GetVertexStart() const { return VertexStartAttribute.Get(); }

		/** VertexCountAttribute */
		const TManagedArray< int32 >& GetVertexCount() const { return VertexCountAttribute.Get(); }

		/** IndicesStartAttribute */
		const TManagedArray< int32 >& GetIndicesStart() const { return IndicesStartAttribute.Get(); }

		/** IndicesCountAttribute */
		const TManagedArray< int32 >& GetIndicesCount() const { return IndicesCountAttribute.Get(); }

		/** SelectionState */
		const TManagedArray< int32 >& GetSelectionState() const { return GeometrySelectionAttribute.Get(); }
		      TManagedArray< int32 >& ModifySelectionState() { check(!IsConst()); return GeometrySelectionAttribute.Modify(); }

		/** NumVerticesOnSelectedGeometry */
		int32 NumVerticesOnSelectedGeometry() const;

		/** GetGeometryNameToIndexMap */
		FStringIntMap GetGeometryNameToIndexMap() const;

	private : 
		const FManagedArrayCollection& ConstCollection;
		FManagedArrayCollection* Collection = nullptr;

		TManagedArrayAccessor<FVector3f> VertexAttribute;
		TManagedArrayAccessor<int32> VertexToGeometryIndexAttribute;
		TManagedArrayAccessor<int32> VertexSelectionAttribute;
		TManagedArrayAccessor<int32> VertexHitProxyIndexAttribute;
		TManagedArrayAccessor<FVector3f> VertexNormalAttribute;
		TManagedArrayAccessor<FLinearColor> VertexColorAttribute;

		TManagedArrayAccessor<FIntVector> IndicesAttribute;
		TManagedArrayAccessor<int32> MaterialIDAttribute;
		TManagedArrayAccessor<FTriangleSection> TriangleSectionAttribute;

		TManagedArrayAccessor<FString> GeometryNameAttribute;
		TManagedArrayAccessor<int32> GeometryHitProxyIndexAttribute;
		TManagedArrayAccessor<int32> VertexStartAttribute;
		TManagedArrayAccessor<int32> VertexCountAttribute;
		TManagedArrayAccessor<int32> IndicesStartAttribute;
		TManagedArrayAccessor<int32> IndicesCountAttribute;
		TManagedArrayAccessor<int32> GeometrySelectionAttribute;
	};

}