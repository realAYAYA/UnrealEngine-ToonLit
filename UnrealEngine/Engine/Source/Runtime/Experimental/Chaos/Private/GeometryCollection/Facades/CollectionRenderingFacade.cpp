// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GeometryCollection.cpp: FGeometryCollection methods.
=============================================================================*/

#include "GeometryCollection/Facades/CollectionRenderingFacade.h"
#include "GeometryCollection/GeometryCollection.h"

namespace GeometryCollection::Facades
{

	FRenderingFacade::FRenderingFacade(FManagedArrayCollection& InCollection)
		: ConstCollection(InCollection)
		, Collection(&InCollection)
		, VertexAttribute(InCollection, "Vertex", FGeometryCollection::VerticesGroup)
		, VertexToGeometryIndexAttribute(InCollection, "GeometryIndex", FGeometryCollection::VerticesGroup, FGeometryCollection::GeometryGroup)
		, VertexSelectionAttribute(InCollection, "SelectionState", FGeometryCollection::VerticesGroup)
		, VertexHitProxyIndexAttribute(InCollection, "HitIndex", FGeometryCollection::VerticesGroup)
		, VertexNormalAttribute(InCollection, "Normal", FGeometryCollection::VerticesGroup)
		, VertexColorAttribute(InCollection, "Color", FGeometryCollection::VerticesGroup)
		, IndicesAttribute(InCollection, "Indices", FGeometryCollection::FacesGroup, FGeometryCollection::VerticesGroup)
		, MaterialIDAttribute(InCollection, "MaterialID", FGeometryCollection::FacesGroup)
		, TriangleSectionAttribute(InCollection, "Sections", FGeometryCollection::MaterialGroup)
		, GeometryNameAttribute(InCollection, "Name", FGeometryCollection::GeometryGroup)
		, GeometryHitProxyIndexAttribute(InCollection, "HitIndex", FGeometryCollection::GeometryGroup)
		, VertexStartAttribute(InCollection, "VertexStart", FGeometryCollection::GeometryGroup)
		, VertexCountAttribute(InCollection, "VertexCount", FGeometryCollection::GeometryGroup)
		, IndicesStartAttribute(InCollection, "IndicesStart", FGeometryCollection::GeometryGroup)
		, IndicesCountAttribute(InCollection, "IndicesCount", FGeometryCollection::GeometryGroup)
		, GeometrySelectionAttribute(InCollection, "SelectionState", FGeometryCollection::GeometryGroup)

	{}

	FRenderingFacade::FRenderingFacade(const FManagedArrayCollection& InCollection)
		: ConstCollection(InCollection)
		, Collection(nullptr)
		, VertexAttribute(InCollection, "Vertex", FGeometryCollection::VerticesGroup)
		, VertexToGeometryIndexAttribute(InCollection, "GeometryIndex", FGeometryCollection::VerticesGroup, FGeometryCollection::GeometryGroup)
		, VertexSelectionAttribute(InCollection, "SelectionState", FGeometryCollection::VerticesGroup)
		, VertexHitProxyIndexAttribute(InCollection, "HitIndex", FGeometryCollection::VerticesGroup)
		, VertexNormalAttribute(InCollection, "Normal", FGeometryCollection::VerticesGroup)
		, VertexColorAttribute(InCollection, "Color", FGeometryCollection::VerticesGroup)
		, IndicesAttribute(InCollection, "Indices", FGeometryCollection::FacesGroup, FGeometryCollection::VerticesGroup)
		, MaterialIDAttribute(InCollection, "MaterialID", FGeometryCollection::FacesGroup)
		, TriangleSectionAttribute(InCollection, "Sections", FGeometryCollection::MaterialGroup)
		, GeometryNameAttribute(InCollection, "Name", FGeometryCollection::GeometryGroup)
		, GeometryHitProxyIndexAttribute(InCollection, "HitIndex", FGeometryCollection::GeometryGroup)
		, VertexStartAttribute(InCollection, "VertexStart", FGeometryCollection::GeometryGroup, FGeometryCollection::VerticesGroup)
		, VertexCountAttribute(InCollection, "VertexCount", FGeometryCollection::GeometryGroup)
		, IndicesStartAttribute(InCollection, "IndicesStart", FGeometryCollection::GeometryGroup, FGeometryCollection::FacesGroup)
		, IndicesCountAttribute(InCollection, "IndicesCount", FGeometryCollection::GeometryGroup)
		, GeometrySelectionAttribute(InCollection, "SelectionState", FGeometryCollection::GeometryGroup)
	{}

	//
	//  Initialization
	//

	void FRenderingFacade::DefineSchema()
	{
		check(!IsConst());
		VertexAttribute.Add();
		VertexSelectionAttribute.Add();
		VertexToGeometryIndexAttribute.Add();
		VertexHitProxyIndexAttribute.Add();
		VertexNormalAttribute.Add();
		VertexColorAttribute.Add();
		IndicesAttribute.Add();
		MaterialIDAttribute.Add();
		TriangleSectionAttribute.Add();
		GeometryNameAttribute.Add();
		GeometryHitProxyIndexAttribute.Add();
		VertexStartAttribute.Add();
		VertexCountAttribute.Add();
		IndicesStartAttribute.Add();
		IndicesCountAttribute.Add();
		GeometrySelectionAttribute.Add();
	}

	bool FRenderingFacade::CanRenderSurface( ) const
	{
		return  IsValid() && GetIndices().Num() && GetVertices().Num();
	}

	bool FRenderingFacade::IsValid( ) const
	{
		return VertexAttribute.IsValid() && VertexToGeometryIndexAttribute.IsValid() &&
			VertexSelectionAttribute.IsValid() && VertexHitProxyIndexAttribute.IsValid() &&
			IndicesAttribute.IsValid() &&
			MaterialIDAttribute.IsValid() && TriangleSectionAttribute.IsValid() &&
			GeometryNameAttribute.IsValid() && GeometryHitProxyIndexAttribute.IsValid() &&
			VertexStartAttribute.IsValid() && VertexCountAttribute.IsValid() &&
			IndicesStartAttribute.IsValid() && IndicesCountAttribute.IsValid() &&
			GeometrySelectionAttribute.IsValid() && VertexColorAttribute.IsValid() &&
			VertexNormalAttribute.IsValid();
	}

	int32 FRenderingFacade::NumTriangles() const
	{
		if (IsValid())
		{
			return GetIndices().Num();
		}
			 
		return 0;
	}


	void FRenderingFacade::AddTriangle(const Chaos::FTriangle& InTriangle)
	{
		check(!IsConst());
		if (IsValid())
		{
			auto CollectionVert = [](const Chaos::FVec3& V) { return FVector3f(float(V.X), float(V.Y), float(V.Z)); };

			TManagedArray<FVector3f>& Vertices = VertexAttribute.Modify();
			TManagedArray<FIntVector>& Indices = IndicesAttribute.Modify();
			
			int32 IndicesStart = IndicesAttribute.AddElements(1);
			int32 VertexStart = VertexAttribute.AddElements(3);

			Indices[IndicesStart] = FIntVector(VertexStart, VertexStart + 1, VertexStart + 2);
			Vertices[VertexStart] = CollectionVert(InTriangle[0]);
			Vertices[VertexStart + 1] = CollectionVert(InTriangle[1]);
			Vertices[VertexStart + 2] = CollectionVert(InTriangle[2]);
		}
	}


	void FRenderingFacade::AddSurface(TArray<FVector3f>&& InVertices, TArray<FIntVector>&& InIndices, TArray<FVector3f>&& InNormals, TArray<FLinearColor>&& InColors)
	{
		check(!IsConst());
		if (IsValid())
		{
			auto CollectionVert = [](const Chaos::FVec3& V) { return FVector3f(float(V.X), float(V.Y), float(V.Z)); };

			TManagedArray<FVector3f>& Vertices = VertexAttribute.Modify();
			TManagedArray<FIntVector>& Indices = IndicesAttribute.Modify();

			int32 IndicesStart = IndicesAttribute.AddElements(InIndices.Num());
			int32 VertexStart = VertexAttribute.AddElements(InVertices.Num());
			
			FIntVector * IndiciesDest = Indices.GetData() + IndicesStart;
			FMemory::Memmove((void*)IndiciesDest, InIndices.GetData(), sizeof(FIntVector) * InIndices.Num());

			for (int i = IndicesStart; i < IndicesStart + InIndices.Num(); i++)
			{
				Indices[i][0] += VertexStart;
				Indices[i][1] += VertexStart;
				Indices[i][2] += VertexStart;
			}

			const FVector3f * VerticesDest = Vertices.GetData() + VertexStart;
			FMemory::Memmove((void*)VerticesDest, InVertices.GetData(), sizeof(FVector3f) * InVertices.Num());

			// Add VertexNormals
			TManagedArray<FVector3f>& Normals = VertexNormalAttribute.Modify();

			const FVector3f* VertexNormalsDest = Normals.GetData() + VertexStart;
			FMemory::Memmove((void*)VertexNormalsDest, InNormals.GetData(), sizeof(FVector3f) * InNormals.Num());

			// Add VertexColors
			TManagedArray<FLinearColor>& VertexColors = VertexColorAttribute.Modify();

			const FLinearColor* VertexColorsDest = VertexColors.GetData() + VertexStart;
			FMemory::Memmove((void*)VertexColorsDest, InColors.GetData(), sizeof(FLinearColor) * InColors.Num());
		}
	}


	TArray<FRenderingFacade::FTriangleSection> 
	FRenderingFacade::BuildMeshSections(const TArray<FIntVector>& InputIndices, TArray<int32> BaseMeshOriginalIndicesIndex, TArray<FIntVector>& RetIndices) const
	{
		check(!IsConst());
		return FGeometryCollectionSection::BuildMeshSections(ConstCollection, InputIndices, BaseMeshOriginalIndicesIndex, RetIndices);
	}


	int32 FRenderingFacade::StartGeometryGroup(FString InName)
	{
		check(!IsConst());

		int32 GeomIndex = INDEX_NONE;
		if (IsValid())
		{
			GeomIndex = GeometryNameAttribute.AddElements(1);
			GeometryNameAttribute.Modify()[GeomIndex] = InName;

			VertexStartAttribute.Modify()[GeomIndex] = VertexAttribute.Num();
			VertexCountAttribute.Modify()[GeomIndex] = 0;
			IndicesStartAttribute.Modify()[GeomIndex] = IndicesAttribute.Num();
			IndicesCountAttribute.Modify()[GeomIndex] = 0;
			GeometrySelectionAttribute.Modify()[GeomIndex] = 0;
		}
		return GeomIndex;
	}

	void FRenderingFacade::EndGeometryGroup(int32 InGeomIndex)
	{
		check(!IsConst());
		if (IsValid())
		{
			check( GeometryNameAttribute.Num()-1 == InGeomIndex );

			if (VertexStartAttribute.Get()[InGeomIndex] < VertexAttribute.Num())
			{
				VertexCountAttribute.Modify()[InGeomIndex] = VertexAttribute.Num() - VertexStartAttribute.Get()[InGeomIndex];

				TManagedArray<int32>& GeomIndexAttr = VertexToGeometryIndexAttribute.Modify();
				for (int i = VertexStartAttribute.Get()[InGeomIndex]; i < VertexAttribute.Num(); i++)
				{
					GeomIndexAttr[i] = InGeomIndex;
				}
			}
			else
			{
				VertexStartAttribute.Modify()[InGeomIndex] = VertexAttribute.Num();
			}

			if (IndicesStartAttribute.Get()[InGeomIndex] < IndicesAttribute.Num())
			{
				IndicesCountAttribute.Modify()[InGeomIndex] = IndicesAttribute.Num() - IndicesStartAttribute.Get()[InGeomIndex];
			}
			else
			{
				IndicesStartAttribute.Modify()[InGeomIndex] = IndicesAttribute.Num();
			}
		}
	}

	FRenderingFacade::FStringIntMap FRenderingFacade::GetGeometryNameToIndexMap() const
	{
		FStringIntMap Map;
		for (int32 i = 0; i < GeometryNameAttribute.Num(); i++)
		{
			Map.Add(GetGeometryName()[i], i);
		}
		return Map;
	}


	int32 FRenderingFacade::NumVerticesOnSelectedGeometry() const
	{
		const TManagedArray<int32>& SelectedGeometry = GeometrySelectionAttribute.Get();
		const TManagedArray<int32>& VertexCount = VertexCountAttribute.Get();
		int32 RetCount = 0;
		for (int i = 0; i < SelectedGeometry.Num(); i++)
			if (SelectedGeometry[i])
				RetCount += VertexCount[i];
		return RetCount;
	}


}; // GeometryCollection::Facades


