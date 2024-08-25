// Copyright Epic Games, Inc. All Rights Reserved. 

#include "Dataflow/CollectionRenderingPatternUtility.h"

#include "Dataflow/DataflowEdNode.h"
#include "Dataflow/DataflowObject.h"
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMesh/NonManifoldMappingSupport.h"
#include "Engine/SkeletalMesh.h"
#include "GeometryCollection/Facades/CollectionRenderingFacade.h"
#include "SkeletalMeshAttributes.h"
#include "ToDynamicMesh.h"

using namespace UE::Geometry;

namespace Dataflow
{
	namespace Conversion
	{
		// Convert a rendering facade to a dynamic mesh
		void RenderingFacadeToDynamicMesh(const GeometryCollection::Facades::FRenderingFacade& Facade, FDynamicMesh3& DynamicMesh)
		{
			if (Facade.CanRenderSurface())
			{
				const int32 NumTriangles = Facade.NumTriangles();
				const int32 NumVertices = Facade.NumVertices();

				const TManagedArray<FIntVector>& Indices = Facade.GetIndices();
				const TManagedArray<FVector3f>& Positions = Facade.GetVertices();
				const TManagedArray<FVector3f>& Normals = Facade.GetNormals();
				const TManagedArray<FLinearColor>& Colors = Facade.GetVertexColor();

				for (int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
				{
					DynamicMesh.AppendVertex(FVertexInfo(FVector3d(Positions[VertexIndex]), Normals[VertexIndex],
						FVector3f(Colors[VertexIndex].R, Colors[VertexIndex].G, Colors[VertexIndex].B)));
				}
				for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles; ++TriangleIndex)
				{
					DynamicMesh.AppendTriangle(FIndex3i(Indices[TriangleIndex].X, Indices[TriangleIndex].Y, Indices[TriangleIndex].Z));
				}
				FMeshNormals::QuickComputeVertexNormals(DynamicMesh);

				DynamicMesh.EnableAttributes();
				DynamicMesh.Attributes()->EnablePrimaryColors();
				DynamicMesh.Attributes()->PrimaryColors()->CreateFromPredicate([](int ParentVID, int TriIDA, int TriIDB) {return true; }, 0.f);
				DynamicMesh.EnableVertexColors(FVector3f::Zero());

				FDynamicMeshColorOverlay* const ColorOverlay = DynamicMesh.Attributes()->PrimaryColors();

				auto SetColorsFromWeights = [&](int TriangleID)
				{
					const FIndex3i Tri = DynamicMesh.GetTriangle(TriangleID);
					const FIndex3i ColorElementTri = ColorOverlay->GetTriangle(TriangleID);
					for (int TriVertIndex = 0; TriVertIndex < 3; ++TriVertIndex)
					{
						FVector4f Color(Colors[Tri[TriVertIndex]]); Color.W = 1.0f;
						ColorOverlay->SetElement(ColorElementTri[TriVertIndex], Color);
					}
				};
				for (const int TriangleID : DynamicMesh.TriangleIndicesItr())
				{
					SetColorsFromWeights(TriangleID);
				}
			}
		}

		// Convert a dataflow component to a dynamic mesh
		void DataflowToDynamicMesh(TSharedPtr<::Dataflow::FEngineContext> DataflowContext, UObject* Asset, UDataflow* Dataflow, FDynamicMesh3& DynamicMesh)
		{
			// just call update on the preview scene.

			FManagedArrayCollection RenderCollection;
			GeometryCollection::Facades::FRenderingFacade Facade(RenderCollection);
			Facade.DefineSchema();


			for (const UDataflowEdNode* Target : Dataflow->GetRenderTargets())
			{
				if (Target)
				{
					// @todo(brice) Fix this cast
					// Target->Render(Facade, DataflowContext);
				}
			}
			RenderingFacadeToDynamicMesh(Facade, DynamicMesh);
		}

		// Convert a dynamic mesh to a rendering facade
		void DynamicMeshToRenderingFacade(const FDynamicMesh3& DynamicMesh, GeometryCollection::Facades::FRenderingFacade& Facade)
		{
			if (Facade.CanRenderSurface())
			{
				const int32 NumTriangles = Facade.NumTriangles();
				const int32 NumVertices = Facade.NumVertices();

				// We can only override vertices attributes (position, normals, colors)
				if ((NumTriangles == DynamicMesh.TriangleCount()) && (NumVertices == DynamicMesh.VertexCount()))
				{
					TManagedArray<FVector3f>& Positions = Facade.ModifyVertices();
					TManagedArray<FVector3f>& Normals = Facade.ModifyNormals();
					TManagedArray<FLinearColor>& Colors = Facade.ModifyVertexColor();

					for (int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
					{
						Positions[VertexIndex] = FVector3f(DynamicMesh.GetVertex(VertexIndex));
						Normals[VertexIndex] = DynamicMesh.GetVertexNormal(VertexIndex);
						Colors[VertexIndex] = DynamicMesh.GetVertexColor(VertexIndex);
					}
				}
			}
		}

		// Convert a dynamic mesh to a dataflow component
		void DynamicMeshToDataflow(const FDynamicMesh3& DynamicMesh, UDataflow* Dataflow)
		{
			//todo
		}
	}

}	// namespace Dataflow