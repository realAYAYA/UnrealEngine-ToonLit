// Copyright Epic Games, Inc. All Rights Reserved.
#include "OpenModelUtils.h"

#ifdef USE_OPENMODEL

#include "CADOptions.h"
#include "DatasmithUtils.h"
#include "DatasmithTranslator.h"
#include "IDatasmithSceneElements.h"

#include "MeshAttributes.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#endif

#include "AlDagNode.h"
#include "AlMesh.h"
#include "AlLayer.h"
#include "AlTesselate.h"

namespace UE_DATASMITHWIRETRANSLATOR_NAMESPACE
{

void OpenModelUtils::SetActorTransform(TSharedPtr<IDatasmithActorElement>& OutActorElement, const AlDagNode& InDagNode)
{
	if (OutActorElement)
	{
		AlLayer* LayerPtr = InDagNode.layer();
		if (AlIsValid(LayerPtr))
		{
			const TUniquePtr<AlLayer> Layer(LayerPtr);

			if (LayerPtr->isSymmetric())
			{
				return;
			}
		}

		AlMatrix4x4 AlMatrix;
		InDagNode.localTransformationMatrix(AlMatrix);

		FMatrix Matrix;
		double* MatrixFloats = (double*)Matrix.M;
		for (int32 IndexI = 0; IndexI < 4; ++IndexI)
		{
			for (int32 IndexJ = 0; IndexJ < 4; ++IndexJ)
			{
				MatrixFloats[IndexI * 4 + IndexJ] = AlMatrix[IndexI][IndexJ];
			}
		}
		FTransform LocalTransform(Matrix);
		FTransform LocalUETransform = FDatasmithUtils::ConvertTransform(FDatasmithUtils::EModelCoordSystem::ZUp_RightHanded, LocalTransform);

		OutActorElement->SetTranslation(LocalUETransform.GetTranslation());
		OutActorElement->SetScale(LocalUETransform.GetScale3D());
		OutActorElement->SetRotation(LocalUETransform.GetRotation());
	}
}

bool OpenModelUtils::IsValidActor(const TSharedPtr<IDatasmithActorElement>& ActorElement)
{
	if (ActorElement != nullptr)
	{
		if (ActorElement->GetChildrenCount() > 0)
		{
			return true;
		}
		else if (ActorElement->IsA(EDatasmithElementType::StaticMeshActor))
		{
			const TSharedPtr<IDatasmithMeshActorElement>& MeshActorElement = StaticCastSharedPtr<IDatasmithMeshActorElement>(ActorElement);
			return FCString::Strlen(MeshActorElement->GetStaticMeshPathName()) > 0;
		}
	}
	return false;
}

bool OpenModelUtils::TransferAlMeshToMeshDescription(const AlMesh& AliasMesh, const TCHAR* SlotMaterialId, FMeshDescription& MeshDescription, CADLibrary::FMeshParameters& MeshParameters, const bool bMerge)
{
	if (AliasMesh.numberOfVertices() == 0 || AliasMesh.numberOfTriangles() == 0)
	{
		return false;
	}

	if (!bMerge)
	{
		MeshDescription.Empty();
	}

	int32 NbStep = 1;
	FMatrix44f SymmetricMatrix;
	bool bIsSymmetricMesh = MeshParameters.bIsSymmetric;
	if (bIsSymmetricMesh)
	{
		NbStep = 2;
		SymmetricMatrix = FDatasmithUtils::GetSymmetricMatrix(MeshParameters.SymmetricOrigin, MeshParameters.SymmetricNormal);
	}

	// Gather all array data
	FStaticMeshAttributes Attributes(MeshDescription);
	TVertexInstanceAttributesRef<FVector3f> VertexInstanceNormals = Attributes.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector2f> VertexInstanceUVs = Attributes.GetVertexInstanceUVs();
	TPolygonGroupAttributesRef<FName> PolygonGroupImportedMaterialSlotNames = Attributes.GetPolygonGroupMaterialSlotNames();
	TVertexAttributesRef<FVector3f> VertexPositions = MeshDescription.GetVertexPositions();

	// Prepared for static mesh usage ?
	if (!VertexPositions.IsValid() || !VertexInstanceNormals.IsValid() || !VertexInstanceUVs.IsValid() || !PolygonGroupImportedMaterialSlotNames.IsValid())
	{
		return false;
	}

	bool bHasUVData = (AliasMesh.uvs() != nullptr);

	int VertexCount = AliasMesh.numberOfVertices();
	int TriangleCount = AliasMesh.numberOfTriangles();
	const int32 VertexInstanceCount = 3 * TriangleCount;

	TArray<FVertexID> VertexPositionIDs;
	VertexPositionIDs.SetNum(VertexCount * NbStep);

	// Reserve space for attributes
	// At this point, all the faces are triangles
	MeshDescription.ReserveNewVertices(VertexCount * NbStep);
	MeshDescription.ReserveNewVertexInstances(VertexInstanceCount * NbStep);
	MeshDescription.ReserveNewEdges(VertexInstanceCount * NbStep);
	MeshDescription.ReserveNewPolygons(TriangleCount * NbStep);

	MeshDescription.ReserveNewPolygonGroups(1);
	FPolygonGroupID PolyGroupId = MeshDescription.CreatePolygonGroup();
	FName ImportedSlotName = SlotMaterialId;
	PolygonGroupImportedMaterialSlotNames[PolyGroupId] = ImportedSlotName;

	// At least one UV set must exist.
	if (VertexInstanceUVs.GetNumChannels() == 0)
	{
		VertexInstanceUVs.SetNumChannels(1);
	}

	// Get Alias mesh info
	const float* AlVertices = AliasMesh.vertices();

	for (int32 Step = 0; Step < NbStep; Step++)
	{
		// Fill the vertex array
		if (Step == 0)
		{
			FVertexID* VertexPositionIDPtr = VertexPositionIDs.GetData();
			for (int Index = 0; Index < VertexCount; ++Index, ++VertexPositionIDPtr)
			{
				const float* CurVertex = AlVertices + 3 * Index;
				*VertexPositionIDPtr = MeshDescription.CreateVertex();
				// ConvertVector_ZUp_RightHanded
				VertexPositions[*VertexPositionIDPtr] = FVector3f(-CurVertex[0], CurVertex[1], CurVertex[2]);
			}
		}
		else
		{
			FVertexID* VertexPositionIDPtr = VertexPositionIDs.GetData() + VertexCount;
			for (int Index = 0, PositionIndex = VertexCount; Index < VertexCount; ++Index, ++VertexPositionIDPtr)
			{
				const float* CurVertex = AlVertices + 3 * Index;
				*VertexPositionIDPtr = MeshDescription.CreateVertex();
				// ConvertVector_ZUp_RightHanded
				VertexPositions[*VertexPositionIDPtr] = SymmetricMatrix.TransformPosition(FVector3f(-CurVertex[0], CurVertex[1], CurVertex[2]));
			}
		}

		FBox UVBBox(FVector(MAX_FLT), FVector(-MAX_FLT));

		const int32 CornerCount = 3; // only triangles
		FVertexID CornerVertexIDs[3];
		TArray<FVertexInstanceID> CornerVertexInstanceIDs;
		CornerVertexInstanceIDs.SetNum(3);

		// Get Alias mesh info
		const int* Triangles = AliasMesh.triangles();
		const float* AlNormals = AliasMesh.normals();
		const float* AlUVs = AliasMesh.uvs();

		// Get per-triangle data: indices, normals and uvs
		if (!MeshParameters.bNeedSwapOrientation == ((bool)Step))
		{
			for (int32 FaceIndex = 0; FaceIndex < TriangleCount; ++FaceIndex, Triangles += 3)
			{
				// Create Vertex instances and set their attributes
				for (int32 VertexIndex = 0, TIndex = 2; VertexIndex < CornerCount; ++VertexIndex, --TIndex)
				{
					CornerVertexIDs[VertexIndex] = VertexPositionIDs[Triangles[TIndex] + VertexCount * Step];
					CornerVertexInstanceIDs[VertexIndex] = MeshDescription.CreateVertexInstance(CornerVertexIDs[VertexIndex]);

					// Set the normal
					const float* CurNormal = &AlNormals[3 * Triangles[TIndex]];
					// ConvertVector_ZUp_RightHanded
					FVector3f UENormal(-CurNormal[0], CurNormal[1], CurNormal[2]);
					UENormal = UENormal.GetSafeNormal();
					if (Step > 0)
					{
						UENormal = SymmetricMatrix.TransformVector(UENormal);
					}
					else
					{
						UENormal *= -1.;
					}
					VertexInstanceNormals[CornerVertexInstanceIDs[VertexIndex]] = UENormal;
				}
				if (CornerVertexIDs[0] == CornerVertexIDs[1] || CornerVertexIDs[0] == CornerVertexIDs[2] || CornerVertexIDs[1] == CornerVertexIDs[2])
				{
					continue;
				}

				// Set the UV
				if (bHasUVData)
				{
					//for (int32 VertexIndex = 2; VertexIndex >= 0; --VertexIndex)
					for (int32 VertexIndex = 0, TIndex = 2; VertexIndex < CornerCount; ++VertexIndex, --TIndex)
					{
						FVector2D UVValues(AlUVs[2 * Triangles[TIndex] + 0], AlUVs[2 * Triangles[TIndex] + 1]);
						UVBBox += FVector(UVValues, 0.0f);
						VertexInstanceUVs.Set(CornerVertexInstanceIDs[VertexIndex], 0, FVector2f(UVValues));
					}
				}

				// Triangulate
				const FPolygonID NewPolygonID = MeshDescription.CreatePolygon(PolyGroupId, CornerVertexInstanceIDs);
			}
		}
		else
		{
			for (int32 FaceIndex = 0; FaceIndex < TriangleCount; ++FaceIndex, Triangles += 3)
			{
				// Create Vertex instances and set their attributes
				for (int32 VertexIndex = 0; VertexIndex < CornerCount; ++VertexIndex)
				{
					CornerVertexIDs[VertexIndex] = VertexPositionIDs[Triangles[VertexIndex] + VertexCount * Step];
					CornerVertexInstanceIDs[VertexIndex] = MeshDescription.CreateVertexInstance(CornerVertexIDs[VertexIndex]);

					// Set the normal
					const float* CurNormal = &AlNormals[3 * Triangles[VertexIndex]];

					// ConvertVector_ZUp_RightHanded
					FVector3f UENormal(-CurNormal[0], CurNormal[1], CurNormal[2]);
					UENormal = UENormal.GetSafeNormal();
					if (Step > 0)
					{
						UENormal = SymmetricMatrix.TransformVector(UENormal) * -1;
					}
					VertexInstanceNormals[CornerVertexInstanceIDs[VertexIndex]] = (FVector3f)UENormal;
				}
				if (CornerVertexIDs[0] == CornerVertexIDs[1] || CornerVertexIDs[0] == CornerVertexIDs[2] || CornerVertexIDs[1] == CornerVertexIDs[2])
				{
					continue;
				}

				// Set the UV
				if (bHasUVData)
				{
					for (int32 VertexIndex = 0; VertexIndex < CornerCount; ++VertexIndex)
					{
						FVector2D UVValues(AlUVs[2 * Triangles[VertexIndex] + 0], AlUVs[2 * Triangles[VertexIndex] + 1]);
						UVBBox += FVector(UVValues, 0.0f);
						VertexInstanceUVs.Set(CornerVertexInstanceIDs[VertexIndex], 0, FVector2f(UVValues));
					}
				}

				// Triangulate
				const FPolygonID NewPolygonID = MeshDescription.CreatePolygon(PolyGroupId, CornerVertexInstanceIDs);
			}
		}
	}

	return true;
}


TSharedPtr<AlDagNode> OpenModelUtils::TesselateDagLeaf(const AlDagNode& DagLeaf, ETesselatorType TessType, double Tolerance)
{
	AlDagNode* TesselatedNode = nullptr;
	statusCode TessStatus;

	switch (TessType)
	{
	case(ETesselatorType::Accurate):
		TessStatus = AlTesselate::chordHeightDeviationAccurate(TesselatedNode, &DagLeaf, Tolerance);
		break;
	case(ETesselatorType::Fast):
	default:
		TessStatus = AlTesselate::chordHeightDeviationFast(TesselatedNode, &DagLeaf, Tolerance);
		break;
	}

	if ((TessStatus == sSuccess) && (AlIsValid(TesselatedNode) == TRUE))
	{
		return TSharedPtr<AlDagNode>(TesselatedNode);
	}
	else
	{
		return TSharedPtr<AlDagNode>();
	}
}

}

#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#endif


