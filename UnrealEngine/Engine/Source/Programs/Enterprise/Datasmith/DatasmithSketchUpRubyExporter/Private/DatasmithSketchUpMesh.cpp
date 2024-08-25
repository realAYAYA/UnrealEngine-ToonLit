// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithSketchUpMesh.h"
#include "DatasmithSketchUpUtils.h"

#include "DatasmithSketchUpExportContext.h"

// SketchUp to Datasmith exporter classes.
#include "DatasmithSketchUpMaterial.h"
#include "DatasmithSketchUpMetadata.h"
#include "DatasmithSketchUpString.h"
#include "DatasmithSketchUpSummary.h"
#include "DatasmithSketchUpComponent.h"

// SketchUp SDK.
#include "DatasmithSketchUpSDKBegins.h"
#include <SketchUpAPI/model/component_definition.h>
#include "SketchUpAPI/model/drawing_element.h"
#include "SketchUpAPI/model/edge.h"
#include "SketchUpAPI/model/face.h"
#include "SketchUpAPI/model/entities.h"
#include "SketchUpAPI/model/entity.h"
#include "SketchUpAPI/model/image.h"
#include "SketchUpAPI/model/layer.h"
#include "SketchUpAPI/model/mesh_helper.h"
#include "SketchUpAPI/model/uv_helper.h"
#include "SketchUpAPI/geometry/point3d.h"
#include "SketchUpAPI/geometry/vector3d.h"
#include <SketchUpAPI/geometry/transformation.h>
#include "DatasmithSketchUpSDKCeases.h"

// Datasmith SDK.

#include "DatasmithMesh.h"
#include "DatasmithMeshExporter.h"
#include "DatasmithSceneExporter.h"
#include "DatasmithSceneFactory.h"
#include "DatasmithUtils.h"
#include "Misc/SecureHash.h"

#include "Async/Async.h"
#include "UObject/GarbageCollection.h"

class FDatasmithMesh;

using namespace DatasmithSketchUp;


namespace DatasmithSketchUp
{
	// The vertex indices of a mesh triangle in a tessellated SketchUp face.
	struct SMeshTriangleIndices
	{
		size_t IndexA; // index of the first triangle vertex
		size_t IndexB; // index of the second triangle vertex
		size_t IndexC; // index of the third triangle vertex
	};

	// The vertex normals of a mesh triangle in a tessellated SketchUp face.
	struct SMeshTriangleNormals
	{
		SUVector3D NormalA; // normal of the first triangle vertex
		SUVector3D NormalB; // normal of the second triangle vertex
		SUVector3D NormalC; // normal of the third triangle vertex
	};

	// Used to extract geometry before filling DatasmithMesh
	class FDatasmithSketchUpMesh : FNoncopyable
	{
	public:

		// Convert the combined mesh into a Datasmith mesh.
		void ConvertMeshToDatasmith(FExportContext& Context, SUTransformation Transform, FDatasmithMesh& OutDMesh) const;

		// Tessellate a SketchUp face into a triangle mesh merged into the combined mesh.
		void AddFace(FExportContext& Context, SUFaceRef InSFaceRef, FLayerIDType LayerId);

		int32 GetOrCreateSlotForMaterial(FMaterialIDType MaterialID)
		{
			if (int32* SlotIdPtr = SlotIdForMaterialId.Find(MaterialID))
			{
				return *SlotIdPtr;
			}
			else
			{
				int32 SlotId = MaterialIDForSlotId.Num();
				MaterialIDForSlotId.Add(MaterialID);// Assign material to slot
				SlotIdForMaterialId.Add(MaterialID, SlotId); // store back reference from material to slot
				return SlotId;
			}
		}

		int32 GetOrCreateSlotForLayer(FLayerIDType LayerID)
		{
			if (int32* SlotIdPtr = SlotIdForLayerId.Find(LayerID))
			{
				return *SlotIdPtr;
			}
			else
			{
				int32 SlotId = LayerIDForSlotId.Num();
				LayerIDForSlotId.Add(LayerID);// Assign material to slot
				SlotIdForLayerId.Add(LayerID, SlotId); // store back reference from material to slot
				return SlotId;
			}
		}

		// Return whether or not the combined mesh contains geometry.
		bool ContainsGeometry() const;

		// Combined mesh vertex points.
		TArray<SUPoint3D> MeshVertexPoints;

		// Combined mesh vertex normals.
		TArray<SUVector3D> MeshVertexNormals;

		// Combined mesh vertex UVQ texture coordinates.
		TArray<SUUVQ> MeshVertexUVQs;

		// Combined mesh triangle vertex indices.
		TArray<SMeshTriangleIndices> MeshTriangleIndices;

		// Combined mesh triangle material IDs.
		TArray<int32> MeshTriangleSlotIds;

		TArray<FEntityIDType> MaterialIDForSlotId;
		TMap<FEntityIDType, int32> SlotIdForMaterialId;

		TArray<FLayerIDType> LayerIDForSlotId;
		TMap<FLayerIDType, int32> SlotIdForLayerId;

		bool bHasFacesWithDefaultMaterial = false;;

	};


	inline bool FDatasmithSketchUpMesh::ContainsGeometry() const
	{
		return (MeshVertexPoints.Num() > 0 && MeshTriangleIndices.Num() > 0);
	}

	void FDatasmithSketchUpMesh::AddFace(FExportContext& Context, SUFaceRef InSFaceRef, FLayerIDType LayerId)
	{
		// Retrieve the SketchUp face front material.
		SUMaterialRef FrontMaterialRef = SU_INVALID;
		SUFaceGetFrontMaterial(InSFaceRef, &FrontMaterialRef); // we can ignore the returned SU_RESULT

		// Retrieve the SketchUp face back material.
		SUMaterialRef BackMaterialRef = SU_INVALID;
		SUFaceGetBackMaterial(InSFaceRef, &BackMaterialRef); // we can ignore the returned SU_RESULT

		// Fall back on the back material when there is no face front material, but there is a face back material.
		bool bUseBackMaterial = SUIsInvalid(FrontMaterialRef) && SUIsValid(BackMaterialRef);
		bool bUseFrontMaterial = !bUseBackMaterial;

		// Create a UV helper for the SketchUp face.
		SUTextureWriterRef TextureWriterRef = SU_INVALID;
		SUUVHelperRef UVHelperRef = SU_INVALID;
		SUFaceGetUVHelper(InSFaceRef, bUseFrontMaterial, bUseBackMaterial, TextureWriterRef, &UVHelperRef); // we can ignore the returned SU_RESULT

		// Tessellate the SketchUp face into a SketchUp triangle mesh.
		SUMeshHelperRef MeshRef = SU_INVALID;
		SUMeshHelperCreateWithUVHelper(&MeshRef, InSFaceRef, UVHelperRef); // we can ignore the returned SU_RESULT

		// Get the number of mesh vertices.
		size_t VertexCount = 0;
		SUMeshHelperGetNumVertices(MeshRef, &VertexCount); // we can ignore the returned SU_RESULT

		// Get the number of mesh triangles.
		size_t TriangleCount = 0;
		SUMeshHelperGetNumTriangles(MeshRef, &TriangleCount); // we can ignore the returned SU_RESULT

		if (VertexCount == 0 || TriangleCount == 0)
		{
			// Release the SketchUp triangle mesh.
			SUMeshHelperRelease(&MeshRef); // we can ignore the returned SU_RESULT

			// Release SketchUp face UV helper.
			SUUVHelperRelease(&UVHelperRef); // we can ignore the returned SU_RESULT

			return;
		}

		// Retrieve the mesh vertex points.
		size_t VertexPointCount = VertexCount;
		TArray<SUPoint3D> VertexPoints;
		VertexPoints.SetNumUninitialized(VertexPointCount);
		SUMeshHelperGetVertices(MeshRef, VertexPointCount, VertexPoints.GetData(), &VertexPointCount); // we can ignore the returned SU_RESULT
		VertexPoints.SetNum(VertexPointCount);

		// Retrieve the mesh vertex normals.
		size_t VertexNormalCount = VertexCount;
		TArray<SUVector3D> VertexNormals;
		VertexNormals.SetNumUninitialized(VertexNormalCount);
		SUMeshHelperGetNormals(MeshRef, VertexNormalCount, VertexNormals.GetData(), &VertexNormalCount); // we can ignore the returned SU_RESULT
		VertexNormals.SetNum(VertexNormalCount);

		// Retrieve the mesh triangle vertex indices, by strides of three indices per triangle.
		size_t TriangleVertexIndexCount = TriangleCount * 3;
		TArray<size_t> TriangleVertexIndices;
		TriangleVertexIndices.SetNumUninitialized(TriangleVertexIndexCount);
		SUMeshHelperGetVertexIndices(MeshRef, TriangleVertexIndexCount, TriangleVertexIndices.GetData(), &TriangleVertexIndexCount); // we can ignore the returned SU_RESULT
		TriangleVertexIndices.SetNum(TriangleVertexIndexCount);

		// Get the SketchUp triangle mesh vertex offset into the combined mesh vertex vector.
		int32 MeshVertexIndexOffset = MeshVertexPoints.Num();

		// Combine the mesh vertex points.
		MeshVertexPoints.Append(VertexPoints);

		MeshVertexNormals.Reserve(MeshVertexNormals.Num() + VertexNormalCount);
		MeshVertexUVQs.Reserve(MeshVertexUVQs.Num() + VertexPointCount);
		MeshTriangleIndices.Reserve(MeshTriangleIndices.Num() + TriangleVertexIndexCount);

		if (bUseFrontMaterial)
		{
			// Combine the mesh vertex normals.
			MeshVertexNormals.Append(VertexNormals);

			// Combine the mesh vertex front UVQ texture coordinates.
			for (size_t VertexPointIndex = 0; VertexPointIndex < VertexPointCount; VertexPointIndex++)
			{
				// Retrieve the front UVQ texture coordinate of the mesh vertex.
				SUUVQ SUVQ;
				SUUVHelperGetFrontUVQ(UVHelperRef, &VertexPoints[VertexPointIndex], &SUVQ); // we can ignore the returned SU_RESULT

				MeshVertexUVQs.Add(SUVQ);
			}

			// Combine the mesh front-facing triangle vertex indices.
			for (size_t Index = 0; Index < TriangleVertexIndexCount;)
			{
				size_t IndexA = MeshVertexIndexOffset + TriangleVertexIndices[Index++];
				size_t IndexB = MeshVertexIndexOffset + TriangleVertexIndices[Index++];
				size_t IndexC = MeshVertexIndexOffset + TriangleVertexIndices[Index++];

				SMeshTriangleIndices TriangleIndices = { IndexA, IndexB, IndexC };

				MeshTriangleIndices.Add(TriangleIndices);
			}
		}
		else
		{
			// Combine the mesh vertex normals.
			for (size_t VertexNormalIndex = 0; VertexNormalIndex < VertexNormalCount; VertexNormalIndex++)
			{
				// Reverse the mesh vertex normal.
				SUVector3D VertexNormal = VertexNormals[VertexNormalIndex];
				VertexNormal.x = -VertexNormal.x;
				VertexNormal.y = -VertexNormal.y;
				VertexNormal.z = -VertexNormal.z;

				MeshVertexNormals.Add(VertexNormal);
			}

			// Combine the mesh vertex back UVQ texture coordinates.
			for (size_t VertexPointIndex = 0; VertexPointIndex < VertexPointCount; VertexPointIndex++)
			{
				// Retrieve the back UVQ texture coordinate of the mesh vertex.
				SUUVQ UVQ;
				SUUVHelperGetBackUVQ(UVHelperRef, &VertexPoints[VertexPointIndex], &UVQ); // we can ignore the returned SU_RESULT

				MeshVertexUVQs.Add(UVQ);
			}

			// Combine the mesh back-facing triangle vertex indices.
			for (size_t Index = 0; Index < TriangleVertexIndexCount;)
			{
				size_t IndexA = MeshVertexIndexOffset + TriangleVertexIndices[Index++];
				size_t IndexB = MeshVertexIndexOffset + TriangleVertexIndices[Index++];
				size_t IndexC = MeshVertexIndexOffset + TriangleVertexIndices[Index++];

				SMeshTriangleIndices TriangleIndices = { IndexC, IndexB, IndexA };

				MeshTriangleIndices.Add(TriangleIndices);
			}
		}

		// Release the SketchUp triangle mesh.
		SUMeshHelperRelease(&MeshRef); // we can ignore the returned SU_RESULT

		// Release SketchUp face UV helper.
		SUUVHelperRelease(&UVHelperRef); // we can ignore the returned SU_RESULT


		int32 SlotId = 0; // Default material slot

		if (Context.bColorByLayer)
		{
			// Get the front material ID of the SketckUp front material.
			SlotId = GetOrCreateSlotForLayer(LayerId);
		}
		else
		{
			// Get the SketckUp material ID.
			if (bUseFrontMaterial)
			{
				if (SUIsValid(FrontMaterialRef))
				{
					// Get the front material ID of the SketckUp front material.
					SlotId = GetOrCreateSlotForMaterial(DatasmithSketchUpUtils::GetMaterialID(FrontMaterialRef));
				}
			}
			else // bUseBackMaterial
			{
				if (SUIsValid(BackMaterialRef))
				{
					// Get the back material ID of the SketckUp back material.
					SlotId = GetOrCreateSlotForMaterial(DatasmithSketchUpUtils::GetMaterialID(BackMaterialRef));
				}
			}
		}

		if (SlotId == 0)
		{
			// todo: it's possible to skip adding slot=0 when there's no faces with 'default' material
			// for this need to compute MeshTriangleSlotIds afterwards(when all materials are known)
			bHasFacesWithDefaultMaterial = true;
		}

		MeshTriangleSlotIds.Reserve(MeshTriangleSlotIds.Num() + TriangleCount);
		
		for(int32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
		{
			MeshTriangleSlotIds.Add(SlotId);
		}
	}

	void FDatasmithSketchUpMesh::ConvertMeshToDatasmith(FExportContext& Context, SUTransformation Transform, FDatasmithMesh& OutDMesh) const
	{
		// Get the number of mesh vertices (must be > 0).
		int32 VertexCount = MeshVertexPoints.Num();

		// Set the number of vertices of the exported Datasmith mesh.
		OutDMesh.SetVerticesCount(VertexCount);

		for (int32 VertexNo = 0; VertexNo < VertexCount; VertexNo++)
		{
			SUPoint3D VertexPoint = MeshVertexPoints[VertexNo];
			SUPoint3DTransform(&Transform, &VertexPoint);
			
			FVector P = FVector(DatasmithSketchUpUtils::FromSketchUp::ConvertPosition(VertexPoint));
			OutDMesh.SetVertex(VertexNo, P.X, P.Y, P.Z);
		}

		// Set the number of Datasmith mesh UV channels.
		OutDMesh.SetUVChannelsCount(1);

		OutDMesh.SetUVCount(0, VertexCount);

		// Convert SketchUp UVQ texture coordinates to Datasmith UV texture coordinates.
		for (int32 VertexNo = 0; VertexNo < VertexCount; VertexNo++)
		{
			// Set and flip vertically the UV texture coordinates in Datasmith mesh UV channel 0.
			SUUVQ const& SUVQ = MeshVertexUVQs[VertexNo];
			OutDMesh.SetUV(0, VertexNo, SUVQ.u / SUVQ.q, -SUVQ.v / SUVQ.q);
		}

		// Get the number of mesh triangles (must be > 0).
		int32 TriangleCount = MeshTriangleIndices.Num();

		// Set the number of triangles of the exported Datasmith mesh.
		OutDMesh.SetFacesCount(TriangleCount);

		SUTransformation TransformNoTranslation = Transform;
		TransformNoTranslation.values[12] = 0;
		TransformNoTranslation.values[13] = 0;
		TransformNoTranslation.values[14] = 0;

		// Make inverse-transposed transform for normals from orientation component of source transform
		SUTransformation TransformInverse;
		SUTransformationGetInverse(&TransformNoTranslation, &TransformInverse);
		SUTransformation TransformForNormals;

		// Transpose
		for (int32 X = 0; X < 4; ++X)
		{
			for (int32 Y = 0; Y < 4; ++Y)
			{
				TransformForNormals.values[X + Y*4] = TransformNoTranslation.values[Y + X*4];
			}
		}

		TArray<SUVector3D> MeshVertexNormalsBaked;
		MeshVertexNormalsBaked.Reserve(MeshVertexNormals.Num());
		for (SUVector3D Normal : MeshVertexNormals)
		{
			SUVector3DTransform(&TransformForNormals, &Normal);
			MeshVertexNormalsBaked.Add(Normal);
		}

		// Convert triangle vertex indices and normals from SketchUp right-handed Z-up coordinates to Unreal left-handed Z-up coordinates.
		// To avoid perturbating X, which is forward in Unreal, the handedness conversion is done by flipping the side vector Y.
		for (int32 TriangleNo = 0, NormalNo = 0; TriangleNo < TriangleCount; TriangleNo++)
		{
			// Set the triangle smoothing mask in the exported Datasmith mesh.
			uint32 SmoothingMask = 0; // no smoothing
			OutDMesh.SetFaceSmoothingMask(TriangleNo, SmoothingMask);

			// Set the triangle vertex indices in the exported Datasmith mesh.
			SMeshTriangleIndices const& TriangleIndices = MeshTriangleIndices[TriangleNo];
			OutDMesh.SetFace(TriangleNo, int32(TriangleIndices.IndexA), int32(TriangleIndices.IndexB), int32(TriangleIndices.IndexC), MeshTriangleSlotIds[TriangleNo]);

			// Set the triangle vertex normals in the exported Datasmith mesh.
			SMeshTriangleNormals TriangleNormals = { MeshVertexNormalsBaked[TriangleIndices.IndexA],
													 MeshVertexNormalsBaked[TriangleIndices.IndexB],
													 MeshVertexNormalsBaked[TriangleIndices.IndexC] };
			OutDMesh.SetNormal(NormalNo++, float(TriangleNormals.NormalA.x), float(-TriangleNormals.NormalA.y), float(TriangleNormals.NormalA.z));
			OutDMesh.SetNormal(NormalNo++, float(TriangleNormals.NormalB.x), float(-TriangleNormals.NormalB.y), float(TriangleNormals.NormalB.z));
			OutDMesh.SetNormal(NormalNo++, float(TriangleNormals.NormalC.x), float(-TriangleNormals.NormalC.y), float(TriangleNormals.NormalC.z));

			// Set the triangle UV coordinate indices in the exported Datasmith mesh.
			OutDMesh.SetFaceUV(TriangleNo, 0, int32(TriangleIndices.IndexA), int32(TriangleIndices.IndexB), int32(TriangleIndices.IndexC));
		}
	}

}

namespace DatasmithSketchUp
{
	// Parses SU Image entity geometry
	class FImageParser
	{
	public:
		FImageParser(SUEntityRef InEntityRef): EntityRef(InEntityRef)
		{
		}

		int32 ImageId = 0;
		FLayerIDType LayerId;

		void Parse(FExportContext& Context, FDatasmithSketchUpMesh& ExtractedMesh)
		{
			SUEntityGetID(EntityRef, &ImageId);

			// Record every face's layer(even for invisible faces!). When face layer visibility changes 
			// this geometry needs to be rebuilt
			SULayerRef LayerRef = SU_INVALID;
			SUImageRef ImageRef = SUImageFromEntity(EntityRef);
			SUDrawingElementGetLayer(SUImageToDrawingElement(ImageRef), &LayerRef);
			LayerId = DatasmithSketchUpUtils::GetEntityID(SULayerToEntity(LayerRef));

			bool bHidden = false;
			SUDrawingElementGetHidden(SUImageToDrawingElement(ImageRef), &bHidden);

			if (!bHidden && Context.Layers.IsLayerVisible(LayerRef))
			{
				TArray<SUPoint3D>& MeshVertexPoints = ExtractedMesh.MeshVertexPoints;
				TArray<SUVector3D>& MeshVertexNormals = ExtractedMesh.MeshVertexNormals;
				TArray<SUUVQ>& MeshVertexUVQs = ExtractedMesh.MeshVertexUVQs;
				TArray<SMeshTriangleIndices>& MeshTriangleIndices = ExtractedMesh.MeshTriangleIndices;
				TArray<int32>& MeshTriangleSlotIds = ExtractedMesh.MeshTriangleSlotIds;

				// Get the SketchUp triangle mesh vertex offset into the combined mesh vertex vector.
				int32 MeshVertexIndexOffset = MeshVertexPoints.Num();

				const int32 VertexPointCount = 4;
				const int32 VertexNormalCount = 4;
				const int32 TriangleCount = 2;
				const int32 TriangleVertexIndexCount = 6;

				MeshVertexNormals.Reserve(MeshVertexNormals.Num() + VertexNormalCount);
				MeshVertexUVQs.Reserve(MeshVertexUVQs.Num() + VertexPointCount);
				MeshTriangleIndices.Reserve(MeshTriangleIndices.Num() + TriangleVertexIndexCount);

				SUVector3D Normal{0, 0, 1}; // todo: fix

				double WidthInch = 0;
				double HeightInch = 0;
				if (SUImageGetDimensions(ImageRef, &WidthInch, &HeightInch) != SU_ERROR_NONE)
				{
					return;
				}

				for (size_t VertexPointIndex = 0; VertexPointIndex < VertexPointCount; VertexPointIndex++)
				{

					MeshVertexNormals.Add(Normal);

					double X = (VertexPointIndex % 2);
					double Y = (VertexPointIndex / 2);

					// Put 1 in Q as we divide later by it
					MeshVertexUVQs.Add(SUUVQ{X, Y, 1});
					MeshVertexPoints.Add(SUPoint3D{X * WidthInch, Y * HeightInch, 0});
				}

				const size_t TriangleVertexIndices[TriangleVertexIndexCount] = 
				{
					0, 1, 2,
					3, 2, 1
				};

				// Combine the mesh front-facing triangle vertex indices.
				for (size_t Index = 0; Index < TriangleVertexIndexCount;)
				{
					size_t IndexA = MeshVertexIndexOffset + TriangleVertexIndices[Index++];
					size_t IndexB = MeshVertexIndexOffset + TriangleVertexIndices[Index++];
					size_t IndexC = MeshVertexIndexOffset + TriangleVertexIndices[Index++];

					SMeshTriangleIndices TriangleIndices = { IndexA, IndexB, IndexC };

					MeshTriangleIndices.Add(TriangleIndices);
				}

				MeshTriangleSlotIds.Reserve(MeshTriangleSlotIds.Num() + TriangleCount);
					
				for(int32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
				{
					MeshTriangleSlotIds.Add(0);
				}
			}
		}

		SUEntityRef EntityRef;
	};
}


bool FEntitiesGeometry::IsMeshUsingInheritedMaterial(int32 MeshIndex)
{
	return Meshes[MeshIndex]->bIsUsingInheritedMaterial;
}

int32 FEntitiesGeometry::GetInheritedMaterialOverrideSlotId()
{
	return 0;
}


const TCHAR* FEntitiesGeometry::GetMeshElementName(int32 MeshIndex)
{
	return Meshes[MeshIndex]->DatasmithMesh->GetName();
}

void ScanSketchUpEntitiesFaces(FExportContext& Context, SUEntitiesRef EntitiesRef, FEntitiesGeometry& Geometry, TFunctionRef<void(TSharedPtr<FDatasmithSketchUpMesh> ExtractedMesh)> OnNewExtractedMesh);
void CombineSketchUpEntitiesFaces(FExportContext& Context, SUEntitiesRef EntitiesRef, FEntitiesGeometry& Geometry, TFunctionRef<void(TSharedPtr<FDatasmithSketchUpMesh> ExtractedMesh)> OnNewExtractedMesh);

void FEntities::UpdateGeometry(FExportContext& Context)
{
	if (EntitiesGeometry.IsValid())
	{
		Context.Materials.UnregisterGeometry(EntitiesGeometry.Get());
		// Remove mesh elements from scene
		RemoveMeshesFromDatasmithScene(Context);

		Context.EntitiesObjects.UnregisterEntities(*this);
		EntitiesGeometry->FaceIds.Reset();
		EntitiesGeometry->Layers.Reset();
	}
	else
	{
		EntitiesGeometry = MakeShared<FEntitiesGeometry>();
	}

	int32 MeshCount = 0;

	TFunction<void(TSharedPtr<FDatasmithSketchUpMesh> ExtractedMesh)> ProcessExtractedMesh = [&Context, this, &MeshCount](TSharedPtr<FDatasmithSketchUpMesh> ExtractedMeshPtr)
	{
		if (ExtractedMeshPtr->ContainsGeometry())
		{
			FString MeshElementName = FString::Printf(TEXT("M%ls_%d"), *Definition.GetSketchupSourceId(), MeshCount + 1); // Count meshes from 1
			FString MeshLabel = FDatasmithUtils::SanitizeObjectName(Definition.GetSketchupSourceName());

			TSharedPtr<FDatasmithInstantiatedMesh> Mesh;
			// Create MeshElement or reuse existing
			if (MeshCount < EntitiesGeometry->Meshes.Num())
			{
				Mesh = EntitiesGeometry->Meshes[MeshCount];
				Mesh->SlotIdForMaterialId.Reset();
				Mesh->SlotIdForLayerId.Reset();
			}
			else
			{
				Mesh = MakeShared<FDatasmithInstantiatedMesh>();
				EntitiesGeometry->Meshes.Add(Mesh);
			}
			// todo: reuse DatasmithMesh when it allows to reset material slots
			Mesh->DatasmithMesh = FDatasmithSceneFactory::CreateMesh(TEXT(""));

			MeshCount++;

			Mesh->DatasmithMesh->SetName(*MeshElementName);
			Mesh->DatasmithMesh->SetLabel(*MeshLabel);
			Mesh->bIsUsingInheritedMaterial = ExtractedMeshPtr->bHasFacesWithDefaultMaterial;

			// Add the non-inherited materials used by the combined mesh triangles.
			if (Context.bColorByLayer)
			{
				for (int32 SlotId = 0;SlotId < ExtractedMeshPtr->LayerIDForSlotId.Num(); ++SlotId)
				{
					FLayerIDType LayerID = ExtractedMeshPtr->LayerIDForSlotId[SlotId];
					Mesh->SlotIdForLayerId.FindOrAdd(LayerID, SlotId);

					if (FMaterialOccurrence* Material = Context.Materials.LayerMaterials.RegisterGeometryForLayer(LayerID, EntitiesGeometry.Get()))
					{
						Mesh->DatasmithMesh->SetMaterial(Material->GetName(), SlotId);
					}
				}
			}
			else
			{
				for (int32 SlotId = 0;SlotId < ExtractedMeshPtr->MaterialIDForSlotId.Num(); ++SlotId)
				{
					if (SlotId == 0 && !ExtractedMeshPtr->bHasFacesWithDefaultMaterial)
					{
						continue; // Skip adding Default material slot if it's not used
					}

					FMaterialIDType MeshMaterialID = ExtractedMeshPtr->MaterialIDForSlotId[SlotId];
					Mesh->SlotIdForMaterialId.FindOrAdd(MeshMaterialID, SlotId);
					// Default or (somehow)missing materials are also assigned to mesh(as a default material)
					if (FMaterialOccurrence* Material = Context.Materials.RegularMaterials.RegisterGeometry(MeshMaterialID, EntitiesGeometry.Get()))
					{
						Mesh->DatasmithMesh->SetMaterial(Material->GetName(), SlotId);
					}
				}
			}
			
			SUTransformation Transform = Definition.GetMeshBakedTransform();

			Context.MeshExportTasks.Emplace(Async(
				EAsyncExecution::ThreadPool,
				[&Context, Mesh, ExtractedMeshPtr, Transform]()
				{
					FDatasmithMeshExporter DatasmithMeshExporter;
					FDatasmithMesh DatasmithMesh;
					ExtractedMeshPtr->ConvertMeshToDatasmith(Context, Transform, DatasmithMesh);

					FGCScopeGuard GCGuard; // Prevent GC from running while UDatasmithMesh is created in ExportToUObject.

					return DatasmithMeshExporter.ExportToUObject(Mesh->DatasmithMesh, Context.GetAssetsOutputPath(), DatasmithMesh, nullptr, FDatasmithExportOptions::LightmapUV);
				}
			));
		}
	};

	if (Context.Options.bSeparateDisconnectedMeshes)
	{
		ScanSketchUpEntitiesFaces(Context, EntitiesRef, *EntitiesGeometry, ProcessExtractedMesh);
	}
	else
	{
		CombineSketchUpEntitiesFaces(Context, EntitiesRef, *EntitiesGeometry, ProcessExtractedMesh);
	}

	EntitiesGeometry->Meshes.SetNum(MeshCount);

	Context.EntitiesObjects.RegisterEntities(*this);
}

void FEntities::AddMeshesToDatasmithScene(FExportContext& Context)
{
	for (TSharedPtr<FDatasmithInstantiatedMesh> Mesh : EntitiesGeometry->Meshes)
	{
		Context.DatasmithScene->AddMesh(Mesh->DatasmithMesh);
	}
}

void FEntities::RemoveMeshesFromDatasmithScene(FExportContext& Context)
{
	for (TSharedPtr<FDatasmithInstantiatedMesh> Mesh : EntitiesGeometry->Meshes)
	{
		Context.DatasmithScene->RemoveMesh(Mesh->DatasmithMesh);
	}
}

TArray<SUGroupRef> FEntities::GetGroups()
{
	// Get the number of groups in the SketchUp model entities.
	size_t SourceGroupCount;
	SUEntitiesGetNumGroups(EntitiesRef, &SourceGroupCount);
	// Retrieve the groups in the source SketchUp entities.
	TArray<SUGroupRef> SGroups;
	SGroups.Init(SU_INVALID, SourceGroupCount);
	SUEntitiesGetGroups(EntitiesRef, SourceGroupCount, SGroups.GetData(), &SourceGroupCount); // we can ignore the returned SU_RESULT
	SGroups.SetNum(SourceGroupCount);
	return MoveTemp(SGroups);
}

TArray<SUComponentInstanceRef> FEntities::GetComponentInstances()
{
	// Get the number of component instances in the SketchUp model entities.
	size_t SourceComponentInstanceCount;
	SUEntitiesGetNumInstances(EntitiesRef, &SourceComponentInstanceCount);

	// Retrieve the component instances in the source SketchUp entities.
	TArray<SUComponentInstanceRef> SComponentInstances;
	SComponentInstances.Init(SU_INVALID, SourceComponentInstanceCount);
	SUEntitiesGetInstances(EntitiesRef, SourceComponentInstanceCount, SComponentInstances.GetData(), &SourceComponentInstanceCount); // we can ignore the returned SU_RESULT
	SComponentInstances.SetNum(SourceComponentInstanceCount);
	return MoveTemp(SComponentInstances);
}

TArray<SUImageRef> FEntities::GetImages()
{
	size_t ImageCount = 0;
	SUEntitiesGetNumImages(EntitiesRef, &ImageCount);

	TArray<SUImageRef> ImageRefs;
	ImageRefs.Init(SU_INVALID, ImageCount);
	SUEntitiesGetImages(EntitiesRef, ImageCount, ImageRefs.GetData(), &ImageCount);
	ImageRefs.SetNum(ImageCount);

	return MoveTemp(ImageRefs);
}

void ScanSketchUpEntitiesFaces(FExportContext& Context, SUEntitiesRef EntitiesRef, FEntitiesGeometry& Geometry, TFunctionRef<void(TSharedPtr<FDatasmithSketchUpMesh> ExtractedMesh)> OnNewExtractedMesh)
{
	// Get the number of faces in the source SketchUp entities.
	size_t SFaceCount = 0;
	SUEntitiesGetNumFaces(EntitiesRef, &SFaceCount); // we can ignore the returned SU_RESULT

	if (SFaceCount == 0)
	{
		return;
	}

	// Retrieve the faces in the source SketchUp entities.
	TArray<SUFaceRef> SFaces;
	SFaces.Init(SU_INVALID, SFaceCount);
	SUEntitiesGetFaces(EntitiesRef, SFaceCount, SFaces.GetData(), &SFaceCount);
	SFaces.SetNum(SFaceCount);

	TSet<int32> ScannedEdgeIDSet;

	// Mesh index inside the SketchUp component definition.
	int32 MeshIndex = 0;

	for (SUFaceRef SSourceFaceRef : SFaces)
	{
		// Get the Source SketckUp face ID.
		int32 SSourceFaceID = DatasmithSketchUpUtils::GetFaceID(SSourceFaceRef);

		// Do not scan more than once a valid SketckUp face.
		if (SUIsInvalid(SSourceFaceRef) || Geometry.FaceIds.Contains(SSourceFaceID))
		{
			continue;
		}

		// Create a mesh combining the geometry of the SketchUp connected faces.
		TSharedPtr<FDatasmithSketchUpMesh> ExtractedMeshPtr = MakeShared<FDatasmithSketchUpMesh>();
		FDatasmithSketchUpMesh& ExtractedMesh = *ExtractedMeshPtr;
		ExtractedMesh.GetOrCreateSlotForMaterial(FMaterial::INHERITED_MATERIAL_ID); // Add default material to Slot=0

		// The source SketchUp face needs to be scanned once.
		TArray<SUFaceRef> FacesToScan;
		FacesToScan.Add(SSourceFaceRef);
		Geometry.FaceIds.Add(SSourceFaceID);

		// Collect all connected faces
		while (FacesToScan.Num() > 0)
		{
			SUFaceRef SScannedFaceRef = FacesToScan.Pop(EAllowShrinking::No);

			// int64 SFacePID = 0;
			// SUEntityGetPersistentID(SUFaceToEntity(SScannedFaceRef), &SFacePID);
			// ADD_TRACE_LINE(TEXT("   Face %lld"), SFacePID);

			// Record every face's layer(even for invisible faces!). When face layer visibility changes 
			// this geometry needs to be rebuilt
			SULayerRef LayerRef = SU_INVALID;
			SUDrawingElementGetLayer(SUFaceToDrawingElement(SScannedFaceRef), &LayerRef);
			FLayerIDType LayerId = DatasmithSketchUpUtils::GetEntityID(SULayerToEntity(LayerRef));
			Geometry.Layers.Add(LayerId);

			bool bFaceHidden = false;
			SUDrawingElementGetHidden(SUFaceToDrawingElement(SScannedFaceRef), &bFaceHidden);

			if (!bFaceHidden && Context.Layers.IsLayerVisible(LayerRef))
			{
				ExtractedMesh.AddFace(Context, SScannedFaceRef, LayerId);
			}

			// Get the number of SketchUp face edges.
			size_t SEdgeCount = 0;
			SUFaceGetNumEdges(SScannedFaceRef, &SEdgeCount); // we can ignore the returned SU_RESULT

			// Retrieve the SketchUp face edges.
			TArray<SUEdgeRef> SEdges;
			SEdges.Init(SU_INVALID, SEdgeCount);
			SUFaceGetEdges(SScannedFaceRef, SEdgeCount, SEdges.GetData(), &SEdgeCount); // we can ignore the returned SU_RESULT
			SEdges.SetNum(SEdgeCount);

			for (SUEdgeRef SEdgeRef : SEdges)
			{
				// Get the SketckUp edge ID.
				int32 SEdgeID = DatasmithSketchUpUtils::GetEdgeID(SEdgeRef);

				// Avoid scanning more than once this SketckUp edge.
				if (!ScannedEdgeIDSet.Contains(SEdgeID))
				{
					ScannedEdgeIDSet.Add(SEdgeID);

					// Get the number of SketchUp faces associated with the edge.
					size_t SEdgeFaceCount = 0;
					SUEdgeGetNumFaces(SEdgeRef, &SEdgeFaceCount); // we can ignore the returned SU_RESULT

					// Retrieve the SketchUp faces associated with the edge.
					TArray<SUFaceRef> SEdgeFaces;
					SEdgeFaces.Init(SU_INVALID, SEdgeFaceCount);
					SUEdgeGetFaces(SEdgeRef, SEdgeFaceCount, SEdgeFaces.GetData(), &SEdgeFaceCount); // we can ignore the returned SU_RESULT
					SEdgeFaces.SetNum(SEdgeFaceCount);

					for (SUFaceRef SFaceRef : SEdgeFaces)
					{
						// Get the SketckUp face ID.
						int32 SFaceID = DatasmithSketchUpUtils::GetFaceID(SFaceRef);

						// Avoid scanning more than once this SketckUp face.
						if (!Geometry.FaceIds.Contains(SFaceID))
						{
							Geometry.FaceIds.Add(SFaceID);

							// This SketchUp face is connected and needs to be scanned further.
							FacesToScan.Add(SFaceRef);
						}
					}
				}
			}
		}

		OnNewExtractedMesh(ExtractedMeshPtr);
	}
}

void CombineSketchUpEntitiesFaces(FExportContext& Context, SUEntitiesRef EntitiesRef, FEntitiesGeometry& Geometry, TFunctionRef<void(TSharedPtr<FDatasmithSketchUpMesh> ExtractedMesh)> OnNewExtractedMesh)
{
	// Get the number of faces in the source SketchUp entities.
	size_t SFaceCount = 0;
	SUEntitiesGetNumFaces(EntitiesRef, &SFaceCount); // we can ignore the returned SU_RESULT

	if (SFaceCount == 0)
	{
		return;
	}

	// Retrieve the faces in the source SketchUp entities.
	TArray<SUFaceRef> SFaces;
	SFaces.Init(SU_INVALID, SFaceCount);
	SUEntitiesGetFaces(EntitiesRef, SFaceCount, SFaces.GetData(), &SFaceCount);
	SFaces.SetNum(SFaceCount);

	// Create a mesh combining the geometry of the SketchUp connected faces.
	TSharedPtr<FDatasmithSketchUpMesh> ExtractedMeshPtr = MakeShared<FDatasmithSketchUpMesh>();
	FDatasmithSketchUpMesh& ExtractedMesh = *ExtractedMeshPtr;
	ExtractedMesh.GetOrCreateSlotForMaterial(FMaterial::INHERITED_MATERIAL_ID); // Add default material to Slot=0

	for (SUFaceRef FaceRef : SFaces)
	{
		// Get the Source SketckUp face ID.
		int32 SSourceFaceID = DatasmithSketchUpUtils::GetFaceID(FaceRef);

		// Do not scan more than once a valid SketckUp face.
		if (SUIsInvalid(FaceRef) || Geometry.FaceIds.Contains(SSourceFaceID))
		{
			continue;
		}

		Geometry.FaceIds.Add(SSourceFaceID);

		// Record every face's layer(even for invisible faces!). When face layer visibility changes 
		// this geometry needs to be rebuilt
		SULayerRef LayerRef = SU_INVALID;
		SUDrawingElementGetLayer(SUFaceToDrawingElement(FaceRef), &LayerRef);
		FLayerIDType LayerId = DatasmithSketchUpUtils::GetEntityID(SULayerToEntity(LayerRef));
		Geometry.Layers.Add(LayerId);

		bool bFaceHidden = false;
		SUDrawingElementGetHidden(SUFaceToDrawingElement(FaceRef), &bFaceHidden);

		if (!bFaceHidden && Context.Layers.IsLayerVisible(LayerRef))
		{
			ExtractedMesh.AddFace(Context, FaceRef, LayerId);
		}
	}

	OnNewExtractedMesh(ExtractedMeshPtr);
}


void FImage::UpdateGeometry(FExportContext& Context)
{
	FImageParser ImageParser(EntityRef);
	FDatasmithSketchUpMesh ExtractedMesh;
	ImageParser.Parse(Context, ExtractedMesh);

	FDatasmithMeshExporter DatasmithMeshExporter;
	FDatasmithMesh DatasmithMesh;

	SUTransformation Transform;
	SUTransformationScale(&Transform, 1.0);

	ExtractedMesh.ConvertMeshToDatasmith(Context, Transform, DatasmithMesh);

	DatasmithMeshElement = FDatasmithSceneFactory::CreateMesh(TEXT(""));

	DatasmithMeshElement->SetName(GetMeshElementName());
	DatasmithMeshElement->SetLabel(*GetName());

	FGCScopeGuard GCGuard; // Prevent GC from running while UDatasmithMesh is created in ExportToUObject. 
	bool bResult = DatasmithMeshExporter.ExportToUObject(DatasmithMeshElement, Context.GetAssetsOutputPath(), DatasmithMesh, nullptr, FDatasmithExportOptions::LightmapUV);

	// todo: something like AddMeshesToDatasmithScene
	Context.DatasmithScene->AddMesh(DatasmithMeshElement);
}

