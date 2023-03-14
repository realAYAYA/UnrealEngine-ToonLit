// Copyright Epic Games, Inc. All Rights Reserved.
#include "OculusAudioGeometryBSP.h"
#include "OculusAudioContextManager.h"
#include "OculusAudioMixer.h"

#include "EngineUtils.h"
#include "Model.h"

#define OVRA_AUDIO_GEOMETRY_BSP_LATEST_VERSION 1


static const FGuid UOculusAudioGeometryBSPGUID(0xa03e414e, 0x4ba74409, 0xbd6b2568, 0x2ed8f1f2); // random GUID, guaranteed to be random.
static const FCustomVersionRegistration UOculusAudioGeometryBSPGUIDRegistration(UOculusAudioGeometryBSPGUID, OVRA_AUDIO_GEOMETRY_BSP_LATEST_VERSION, TEXT("OculusAudioGeometryBSPVersion"));


void UOculusAudioGeometryBSP::Serialize(FArchive & Ar)
{
	// Tell the archive we are using a custom version.
	Ar.UsingCustomVersion(UOculusAudioGeometryBSPGUID);

	Super::Serialize(Ar);

	struct Delta {
		static size_t Read(void* userData, void* bytes, size_t byteCount) {
			FArchive* Archive = static_cast<FArchive*>(userData);
			check(Archive->IsLoading());

			// Check that we are not reading past end of archive..
			int64 CurrentPosition = Archive->Tell();
			int64 TotalSize = Archive->TotalSize();
			if ((CurrentPosition + static_cast<int64>(byteCount)) > TotalSize)
			{
				return 0;
			}

			Archive->Serialize(bytes, byteCount);
			return Archive->GetError() ? 0 : byteCount;
		}
		static size_t Write(void* userData, const void* bytes, size_t byteCount) {
			FArchive* Archive = static_cast<FArchive*>(userData);
			check(Archive->IsSaving());
			Archive->Serialize(const_cast<void*>(bytes), byteCount);
			return Archive->GetError() ? 0 : byteCount;
		}
		static int64_t Seek(void* userData, int64_t seekOffset) {
			FArchive* Archive = static_cast<FArchive*>(userData);
			int64 Start = Archive->Tell();
			Archive->Seek(seekOffset);
			return 0;
		}
	};

	ovrAudioSerializer Serializer;
	Serializer.read = Delta::Read;
	Serializer.write = Delta::Write;
	Serializer.seek = Delta::Seek;
	Serializer.userData = &Ar;

	ovrAudioContext Context = FOculusAudioContextManager::GetOrCreateSerializationContext(this);

	UWorld* World = GetWorld();
	if (Ar.IsSaving() && World != nullptr && !World->IsGameWorld())
	{
		CreateGeometryFromBSP(Context, World, &ovrGeometryBSP);

		if (ovrGeometryBSP == nullptr)
		{
			// nothing to serialize
			return;
		}

		ovrResult Result = OVRA_CALL(ovrAudio_AudioGeometryWriteMeshData)(ovrGeometryBSP, &Serializer);
		check(Result == ovrSuccess);

		Result = OVRA_CALL(ovrAudio_DestroyAudioGeometry)(ovrGeometryBSP);
		check(Result == ovrSuccess);

		ovrGeometryBSP = nullptr;
	}
	else if (Ar.IsLoading())
	{
		ovrResult Result = OVRA_CALL(ovrAudio_CreateAudioGeometry)(Context, &ovrGeometryBSP);
		check(Result == ovrSuccess);

		int64 PreviousPosition = Ar.Tell();
		Result = OVRA_CALL(ovrAudio_AudioGeometryReadMeshData)(ovrGeometryBSP, &Serializer);
		if (Result != ovrSuccess)
		{
			Result = OVRA_CALL(ovrAudio_DestroyAudioGeometry)(ovrGeometryBSP);
			check(Result == ovrSuccess);

			ovrGeometryBSP = nullptr;
			Ar.Seek(PreviousPosition);
		}
	}
}

void UOculusAudioGeometryBSP::CreateGeometryFromBSP(ovrAudioContext Context, UWorld* World, ovrAudioGeometry* Geometry)
{
	TArray<FVector> Vertices;
	TArray<int32> Indices;
	for (auto& WorldVertex : World->GetModel()->Points)
	{
		Vertices.Add(OculusAudioSpatializationAudioMixer::ToOVRVector((FVector)WorldVertex));
	}

	if (Vertices.Num() == 0)
	{
		UE_LOG(LogAudio, Warning, TEXT("No BSP geometry found in level!"));
		return;
	}

	int FaceCount = 0;
	for (auto& WorldNode : World->GetModel()->Nodes)
	{
		if (WorldNode.NumVertices <= 2)
		{
			continue;
		}

		int32 Index0 = World->GetModel()->Verts[WorldNode.iVertPool + 0].pVertex;
		int32 Index1 = World->GetModel()->Verts[WorldNode.iVertPool + 1].pVertex;
		int32 Index2;

		for (auto v = 2; v < WorldNode.NumVertices; ++v)
		{
			Index2 = World->GetModel()->Verts[WorldNode.iVertPool + v].pVertex;
			Indices.Add(Index0);
			Indices.Add(Index2);
			Indices.Add(Index1);
			Index1 = Index2;

			++FaceCount;
		}
	}

	ovrAudioMesh ovrMesh = { };

	ovrAudioMeshVertices ovrVertices = { 0 };
	ovrVertices.vertices = Vertices.GetData();
	ovrVertices.vertexCount = Vertices.Num();
	check(ovrVertices.vertexCount != 0);
	ovrVertices.vertexType = ovrAudioScalarType_Float32;
	ovrMesh.vertices = ovrVertices;

	ovrAudioMeshIndices ovrIndices = { 0 };
	ovrIndices.indices = Indices.GetData();
	ovrIndices.indexCount = Indices.Num();
	check(ovrIndices.indexCount != 0);
	ovrIndices.indexType = ovrAudioScalarType_UInt32;
	ovrMesh.indices = ovrIndices;

	ovrAudioMaterial ovrMaterial = nullptr;
	ovrResult Result = OVRA_CALL(ovrAudio_CreateAudioMaterial)(Context, &ovrMaterial);
	check(Result == ovrSuccess);

	ConstructMaterial(ovrMaterial);

	ovrAudioMeshGroup MeshGroup = { 0 };
	MeshGroup.faceCount = FaceCount;
	MeshGroup.faceType = ovrAudioFaceType_Triangles;
	MeshGroup.material = ovrMaterial;
	ovrMesh.groups = &MeshGroup;
	ovrMesh.groupCount = 1;

	Result = OVRA_CALL(ovrAudio_CreateAudioGeometry)(Context, Geometry);
	if (Result != ovrSuccess) {
		UE_LOG(LogAudio, Warning, TEXT("Failed to create audio propagation geometry for BSP!"));
		return;
	}

	Result = OVRA_CALL(ovrAudio_AudioGeometryUploadMesh)(*Geometry, &ovrMesh);
	if (Result != ovrSuccess) {
		UE_LOG(LogAudio, Warning, TEXT("Failed adding BSP geometry to the audio propagation sub-system!"));
		return;
	}

	UE_LOG(LogAudio, Warning, TEXT("BSP complete!"));
}
