#include "SmoothNormalCommand.h"
#include "Engine/StaticMesh.h"
#include "RawMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Rendering/SkeletalMeshModel.h"

inline void FSmoothNormalCommand::SmoothNormal(TArray<FAssetData> SelectedAssets)
{
	for (int32 i = 0; i < SelectedAssets.Num(); i++)
	{
		if (SelectedAssets[i].AssetClassPath.ToString().Contains(TEXT("StaticMesh")))
		{
			SmoothNormalStaticMeshTriangle(SelectedAssets[i]);
		}
		if (SelectedAssets[i].AssetClassPath.ToString().Contains(TEXT("SkeletalMesh")))
		{
			SmoothNormalSkeletalMesh(SelectedAssets[i]);
		}
	}
}

void WieldVertex(const TMap<FVector3f, FVector3f>& VertexNormalMap, TMap<FVector3f, FVector3f>& VertexWieldRemap)
{
	TArray<FVector3f> AllPositions;
	TArray<FVector3f> WieldPositions;
	VertexNormalMap.GetKeys(AllPositions);

	for (int32 i = 0; i < AllPositions.Num(); i++)
	{
		int32 FoundIndex = INDEX_NONE;
		FVector3f Cur = AllPositions[i];
		for (int32 j = 0; j < WieldPositions.Num(); j++)
		{
			if (Cur.Equals(WieldPositions[j], 0.1f))
			{
				FoundIndex = j;
				break;
			}
		}
		if (FoundIndex == INDEX_NONE)
		{
			WieldPositions.Add(Cur);
			VertexWieldRemap.Add(Cur, Cur);
		}
		else
		{
			VertexWieldRemap.Add(Cur, WieldPositions[FoundIndex]);
		}
	}
	for (int32 i = 0; i < AllPositions.Num(); i++)
	{
		FVector3f Cur = AllPositions[i];
		FVector3f Wield = VertexWieldRemap[Cur];
		if (Wield != Cur)
		{
			//VertexNormalMap[Wield] += VertexNormalMap[Cur];
		}
	}
}

void FSmoothNormalCommand::SmoothNormalStaticMesh(FAssetData AssetData)
{
	UStaticMesh* StaticMesh = Cast<UStaticMesh>(AssetData.GetAsset());
	const FStaticMeshVertexBuffers& VertexBuffers = StaticMesh->GetRenderData()->LODResources[0].VertexBuffers;
	//const FPositionVertexBuffer& PositionBuffer = VertexBuffers.PositionVertexBuffer;
	const FStaticMeshVertexBuffer& VertexBuffer = VertexBuffers.StaticMeshVertexBuffer;

	const TArray<int32>& WedgeMap = StaticMesh->GetRenderData()->LODResources[0].WedgeMap;

	TMap<FVector3f, FVector3f> VertexNormalMap;
	TMap<FVector3f, FVector3f> VertexWieldRemap;
	for (int32 Index = 0; Index < StaticMesh->GetNumSourceModels(); Index++)
	{
		FStaticMeshSourceModel& SourceModel = StaticMesh->GetSourceModel(Index);
		FRawMesh RawMesh;

		SourceModel.LoadRawMesh(RawMesh);

		for (int32 WedgeIndex = 0; WedgeIndex < RawMesh.WedgeIndices.Num(); WedgeIndex++)
		{
			int32 RawVertexIndex = RawMesh.WedgeIndices[WedgeIndex];
			FVector3f RawVertexPosition = RawMesh.VertexPositions[RawVertexIndex];
			
			int32 VertexIndex = WedgeMap[WedgeIndex];
			//FVector3f RenderVertexPosition = PositionBuffer.VertexPosition(VertexIndex);
			FVector3f RenderVertexNormal = VertexBuffer.VertexTangentZ(VertexIndex);
			if (!VertexNormalMap.Contains(RawVertexPosition))
			{
				VertexNormalMap.Add(RawVertexPosition, FVector3f::ZeroVector);
			}
			VertexNormalMap[RawVertexPosition] += RenderVertexNormal;
		}

		WieldVertex(VertexNormalMap, VertexWieldRemap);
		
		if (RawMesh.WedgeTexCoords[1].Num() == 0)
		{
			RawMesh.WedgeTexCoords[1].AddDefaulted(RawMesh.WedgeIndices.Num());
		}
		if (RawMesh.WedgeTexCoords[2].Num() == 0)
		{
			RawMesh.WedgeTexCoords[2].AddDefaulted(RawMesh.WedgeIndices.Num());
		}
		RawMesh.WedgeTexCoords[3].Empty();
		for (int32 WedgeIndex = 0; WedgeIndex < RawMesh.WedgeIndices.Num(); WedgeIndex++)
		{
			FVector3f RawVertexPosition = RawMesh.VertexPositions[RawMesh.WedgeIndices[WedgeIndex]];

			int RenderVertexIndex = WedgeMap[WedgeIndex];
			FVector3f VertexTangentZ = VertexBuffer.VertexTangentZ(RenderVertexIndex);
			FVector3f VertexTangentX = VertexBuffer.VertexTangentX(RenderVertexIndex);
			FVector3f VertexTangentY = VertexBuffer.VertexTangentY(RenderVertexIndex);
			
			FVector3f WieldRemapVertex = VertexWieldRemap[RawVertexPosition];
			FVector3f SmoothNormal = VertexNormalMap[WieldRemapVertex].GetSafeNormal();
			FVector3f SmoothNormalAtTangent = FVector3f::ZeroVector;

			if (VertexTangentX != FVector3f::ZeroVector
				&&VertexTangentY != FVector3f::ZeroVector
				&&VertexTangentZ != FVector3f::ZeroVector)
			{
				FMatrix44f TangentToNormal(VertexTangentX, VertexTangentY, VertexTangentZ, FVector3f(0, 0, 0));
				//将平均法线转换到切线空间存储
				SmoothNormalAtTangent = TangentToNormal.InverseTransformVector(SmoothNormal).GetSafeNormal();
			}
			else
			{
				SmoothNormalAtTangent = FVector3f::ZeroVector;
			}
			RawMesh.WedgeTexCoords[3].Add(FVector2f(SmoothNormalAtTangent.X, SmoothNormalAtTangent.Y));
			RawMesh.WedgeTangentZ[WedgeIndex] = SmoothNormal;
		}
		
		SourceModel.SaveRawMesh(RawMesh);
	}
	
	StaticMesh->Build(false);
	StaticMesh->PostEditChange();
	StaticMesh->MarkPackageDirty();
}

void BuildSoftSkinVertexMap(TArray<FSoftSkinVertex>& Vertices, TMap<FVector3f, TArray<FSoftSkinVertex>>& VertexSkinMap)
{
	for (int32 i = 0; i < Vertices.Num(); i++)
	{
		if (VertexSkinMap.Contains(Vertices[i].Position))
		{
			VertexSkinMap[Vertices[i].Position].Add(Vertices[i]);
		}
		else
		{
			VertexSkinMap.Add(Vertices[i].Position, TArray<FSoftSkinVertex>());
			VertexSkinMap[Vertices[i].Position].Add(Vertices[i]);
		}
	}
}

FSoftSkinVertex* FindSoftSkinVertex(
	TMap<FVector3f, TArray<FSoftSkinVertex>>& VertexSkinMap,
	const FVector3f Center,
	const FVector3f Position,
	const FVector3f Normal,
	const FVector2f UV0)
{
	FSoftSkinVertex *Result = nullptr;
	if (VertexSkinMap.Contains(Center))
	{
		TArray<FSoftSkinVertex>& Array = VertexSkinMap[Center];
		for (int32 i = 0; i < Array.Num(); i++)
		{
			if (Array[i].Position == Position&&Array[i].UVs[0] == UV0)
			{
				if (Result == nullptr)
				{
					Result = &Array[i];
				}
				else
				{
					Result = &Array[i];
				}
			}

		}
	}
	return Result;
}

void FSmoothNormalCommand::SmoothNormalSkeletalMesh(FAssetData AssetData)
{
	UObject* Obj = AssetData.GetAsset();
	USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Obj);
	SkeletalMesh->Build();

	//IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>(TEXT("MeshUtilities"));
	FMeshBuildSettings MeshBuildSettings;
	MeshBuildSettings.bRemoveDegenerates = true;
	MeshBuildSettings.bUseMikkTSpace = false;

	TMap<FVector3f, FVector3f> VertexNormalMap;

	FSkeletalMeshImportData RawMesh;

	if (const FMeshDescription* MeshDescription = SkeletalMesh->GetMeshDescription(0))
	{
		RawMesh = FSkeletalMeshImportData::CreateFromMeshDescription(*MeshDescription);
	}

	TMap<FVector3f, TArray<FSoftSkinVertex>> VertexSkinMap;
	{
		FSkeletalMeshModel* SkeletalMeshModel = SkeletalMesh->GetImportedModel();
		FSkeletalMeshLODModel* ImportedModel = &SkeletalMeshModel->LODModels[0];
		TArray<FSoftSkinVertex> Vertices;
		ImportedModel->GetVertices(Vertices);

		int NumFaces = ImportedModel->IndexBuffer.Num() / 3;

		for (int i = 0; i < NumFaces; i++)
		{
			FVector3f Center = FVector3f::ZeroVector;

			bool flag=false;

			for (int j = 0; j < 3; j++)
			{
				int VertIndex = ImportedModel->IndexBuffer[i * 3 + j];

				Center += Vertices[VertIndex].Position;

				const int32 ZeroCount = Vertices[VertIndex].TangentX.IsZero()+ Vertices[VertIndex].TangentY.IsZero()+ Vertices[VertIndex].TangentZ.IsNearlyZero3();
				if (ZeroCount >= 2)
				{
					flag = true;
				}
			}
			if (flag)
			{
				continue;
			}
			Center /= 3;
			if (!VertexSkinMap.Contains(Center))
			{
				VertexSkinMap.Add(Center, TArray<FSoftSkinVertex>());
			}
			else
			{
				flag = true;
			}
			for (int32 j = 0; j < 3; j++)
			{
				int32 VertIndex = ImportedModel->IndexBuffer[i * 3 + j];

				VertexSkinMap[Center].Add(Vertices[VertIndex]);

			}
		}
	}

	for (int32 FaceIndex = 0; FaceIndex < RawMesh.Faces.Num(); FaceIndex++)
	{
		SkeletalMeshImportData::FTriangle Face = RawMesh.Faces[FaceIndex];

		for (int32 i = 0; i < 3; i++)
		{
			SkeletalMeshImportData::FVertex Wedge = RawMesh.Wedges[Face.WedgeIndex[i]];
			FVector3f VertexPosition = RawMesh.Points[Wedge.VertexIndex];
			FVector3f VertexNormal = Face.TangentZ[i];
			if (!VertexNormalMap.Contains(VertexPosition))
			{
				VertexNormalMap.Add(VertexPosition, FVector3f::ZeroVector);
			}
			VertexNormalMap[VertexPosition] += VertexNormal;
		}

	}
	
	for (int32 FaceIndex = 0; FaceIndex < RawMesh.Faces.Num(); FaceIndex++)
	{
		SkeletalMeshImportData::FTriangle Face = RawMesh.Faces[FaceIndex];

		FVector3f Center = FVector3f::ZeroVector;
		for (int i = 0; i < 3; i++)
		{
			SkeletalMeshImportData::FVertex Wedge = RawMesh.Wedges[Face.WedgeIndex[i]];
			FVector3f VertexPosition = RawMesh.Points[Wedge.VertexIndex];
			Center += VertexPosition;
		}
		Center /= 3;

		for (int i = 0; i < 3; i++)
		{
			SkeletalMeshImportData::FVertex Wedge = RawMesh.Wedges[Face.WedgeIndex[i]];
			FVector3f VertexPosition = RawMesh.Points[Wedge.VertexIndex];

			FVector3f SmoothNormal = VertexNormalMap[VertexPosition].GetSafeNormal();

			FSoftSkinVertex* SkinVertex = FindSoftSkinVertex(VertexSkinMap, Center, VertexPosition, Face.TangentZ[i],Wedge.UVs[0]);

			FVector3f SmoothNormalAtTangent;
			if (SkinVertex != nullptr)
			{
				FVector3f TangentX = SkinVertex->TangentX;
				FVector3f TangentY = SkinVertex->TangentY;
				FVector3f TangentZ = SkinVertex->TangentZ;

				FMatrix44f TangentToNormal(TangentX, TangentY, TangentZ, FVector3f(0, 0, 0));

				SmoothNormalAtTangent = TangentToNormal.InverseTransformVector(SmoothNormal);
			}
			else
			{
				SmoothNormalAtTangent = FVector3f(0, 0, 1);
				SkinVertex = FindSoftSkinVertex(VertexSkinMap, Center, VertexPosition, Face.TangentZ[i], Wedge.UVs[0]);
			}
			
			RawMesh.Wedges[Face.WedgeIndex[i]].UVs[1] = FVector2f::ZeroVector;
			RawMesh.Wedges[Face.WedgeIndex[i]].UVs[2] = FVector2f::ZeroVector;
			RawMesh.Wedges[Face.WedgeIndex[i]].UVs[3] = FVector2f(SmoothNormalAtTangent.X, SmoothNormalAtTangent.Y);
			
		}

	}
	RawMesh.NumTexCoords = 4;
	
	FMeshDescription MeshDescription;
	if (RawMesh.GetMeshDescription(nullptr, &SkeletalMesh->GetLODInfo(0)->BuildSettings, MeshDescription))
	{
		SkeletalMesh->CreateMeshDescription(0, MoveTemp(MeshDescription));
		SkeletalMesh->CommitMeshDescription(0);
	}

	SkeletalMesh->Build();
	SkeletalMesh->PostEditChange();
	SkeletalMesh->MarkPackageDirty();
}

void FSmoothNormalCommand::SmoothNormalStaticMeshTriangle(FAssetData AssetData)
{
	UStaticMesh* StaticMesh = Cast<UStaticMesh>(AssetData.GetAsset());
	const FStaticMeshVertexBuffers& VertexBuffers = StaticMesh->GetRenderData()->LODResources[0].VertexBuffers;
	//const FPositionVertexBuffer& PositionBuffer = VertexBuffers.PositionVertexBuffer;
	const FStaticMeshVertexBuffer& VertexBuffer = VertexBuffers.StaticMeshVertexBuffer;

	const TArray<int32>& WedgeMap = StaticMesh->GetRenderData()->LODResources[0].WedgeMap;

	TMap<FVector3f, FVector3f> VertexNormalMap;
	TMap<FVector3f, FVector3f> VertexWieldRemap;
	TMap<FVector3f, TArray<FVector3f>> WeightingNormalMap;
	for(int Index = 0; Index < StaticMesh->GetNumSourceModels(); Index++)
	{
		FStaticMeshSourceModel& SourceModel = StaticMesh->GetSourceModel(Index);
		FRawMesh RawMesh;

		SourceModel.LoadRawMesh(RawMesh);
		{
			check(RawMesh.WedgeIndices.Num() % 3 == 0);
		}
		for (int WedgeIndex = 0; WedgeIndex <= RawMesh.WedgeIndices.Num() -3; WedgeIndex = WedgeIndex+3)
		{
			int RawVertexIndex = RawMesh.WedgeIndices[WedgeIndex];
			int RawVertexIndex1 = RawMesh.WedgeIndices[WedgeIndex + 1];
			int RawVertexIndex2 = RawMesh.WedgeIndices[WedgeIndex + 2];
			FVector3f RawVertexPosition = RawMesh.VertexPositions[RawVertexIndex];
			FVector3f RawVertexPosition1 = RawMesh.VertexPositions[RawVertexIndex1];
			FVector3f RawVertexPosition2 = RawMesh.VertexPositions[RawVertexIndex2];
			int VertexIndex = WedgeMap[WedgeIndex];
			int VertexIndex1 = WedgeMap[WedgeIndex + 1];
			int VertexIndex2 = WedgeMap[WedgeIndex + 2 ];
			FVector3f VertexNormal = VertexBuffer.VertexTangentZ(VertexIndex);

			FVector3f Side = RawVertexPosition1 - RawVertexPosition;
			FVector3f Side1 = RawVertexPosition2 - RawVertexPosition;
			float Angle = acos(FVector3f::DotProduct(Side.GetSafeNormal(), Side1.GetSafeNormal()));
			check(Angle>=0);
			VertexNormal *= Angle;
			if(!WeightingNormalMap.Contains(RawVertexPosition))
			{
				TArray<FVector3f> Normals;
				
				Normals.Add(VertexNormal);
				WeightingNormalMap.Add(RawVertexPosition, Normals);
			}
			else
			{

				auto& Array = WeightingNormalMap[RawVertexPosition];
				Array.Add(VertexNormal);
			}
			VertexNormal = VertexBuffer.VertexTangentZ(VertexIndex1);
			Side = RawVertexPosition - RawVertexPosition1;
			Side1 = RawVertexPosition2 - RawVertexPosition1;
			Angle = acos(FVector3f::DotProduct(Side.GetSafeNormal(), Side1.GetSafeNormal()));
			check(Angle>=0);
			VertexNormal *= Angle;
			if (!WeightingNormalMap.Contains(RawVertexPosition1))
			{
				TArray<FVector3f> Normals;
				
				Normals.Add(VertexNormal);
				WeightingNormalMap.Add(RawVertexPosition1, Normals);
			}
			else
			{

				auto& Array = WeightingNormalMap[RawVertexPosition1];
				Array.Add(VertexNormal);
			}
			VertexNormal = VertexBuffer.VertexTangentZ(VertexIndex2);
			Side = RawVertexPosition - RawVertexPosition2;
			Side1 = RawVertexPosition1 - RawVertexPosition2;
			Angle = acos(FVector3f::DotProduct(Side.GetSafeNormal(), Side1.GetSafeNormal()));
			check(Angle>=0);
			VertexNormal *= Angle;
			if (!WeightingNormalMap.Contains(RawVertexPosition2))
			{
				TArray<FVector3f> Normals;
				
				Normals.Add(VertexNormal);
				WeightingNormalMap.Add(RawVertexPosition2, Normals);
			}
			else
			{

				auto& Array = WeightingNormalMap[RawVertexPosition2];
				Array.Add(VertexNormal);
			}
		}
		if (RawMesh.WedgeTexCoords[1].Num() == 0)
		{
			RawMesh.WedgeTexCoords[1].AddDefaulted(RawMesh.WedgeIndices.Num());
		}
		if (RawMesh.WedgeTexCoords[2].Num() == 0)
		{
			RawMesh.WedgeTexCoords[2].AddDefaulted(RawMesh.WedgeIndices.Num());
		}
		RawMesh.WedgeTexCoords[3].Empty();
		
		for (int32 WedgeIndex = 0; WedgeIndex < RawMesh.WedgeIndices.Num(); WedgeIndex++)
		{
			FVector3f RawVertexPosition = RawMesh.VertexPositions[RawMesh.WedgeIndices[WedgeIndex]];

			int32 RenderVertexIndex = WedgeMap[WedgeIndex];
			FVector3f VertexTangentZ = VertexBuffer.VertexTangentZ(RenderVertexIndex);
			FVector3f VertexTangentX = VertexBuffer.VertexTangentX(RenderVertexIndex);
			FVector3f VertexTangentY = VertexBuffer.VertexTangentY(RenderVertexIndex);
			FVector3f SmoothNormal = FVector3f::ZeroVector;
			if (!WeightingNormalMap.Contains(RawVertexPosition))
			{
				check(false);
			}
			else
			{
				const TArray<FVector3f>& WeightingNormals  = WeightingNormalMap[RawVertexPosition];
				for(const auto& Normal : WeightingNormals)
				{
					SmoothNormal += Normal;
				}
			}
			SmoothNormal = SmoothNormal.GetSafeNormal();
			FVector3f SmoothNormalAtTangent = FVector3f::ZeroVector;

			if (VertexTangentX != FVector3f::ZeroVector
				&&VertexTangentY != FVector3f::ZeroVector
				&&VertexTangentZ != FVector3f::ZeroVector)
			{
				FMatrix44f TangentToNormal(VertexTangentX, VertexTangentY, VertexTangentZ, FVector3f(0, 0, 0));
				SmoothNormalAtTangent = TangentToNormal.InverseTransformVector(SmoothNormal).GetSafeNormal();
			}
			else
			{
				SmoothNormalAtTangent = FVector3f::ZeroVector;
			}
			
			RawMesh.WedgeTexCoords[3].Add(FVector2f(SmoothNormalAtTangent.X, SmoothNormalAtTangent.Y));
		}
		SourceModel.SaveRawMesh(RawMesh);
	}
	
	StaticMesh->Build(false);
	StaticMesh->PostEditChange();
	StaticMesh->MarkPackageDirty();
}
