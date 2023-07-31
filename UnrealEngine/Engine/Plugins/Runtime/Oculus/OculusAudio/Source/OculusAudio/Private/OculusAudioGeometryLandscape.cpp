// Copyright Epic Games, Inc. All Rights Reserved.
#include "OculusAudioGeometryLandscape.h"
#include "OculusAudioContextManager.h"
#include "OculusAudioMixer.h"

#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeComponent.h"
#include "LandscapeInfo.h"
#include "LandscapeDataAccess.h"

#define OVRA_AUDIO_GEOMETRY_LANDSCAPE_LATEST_VERSION 1

static const FGuid UOculusAudioGeometryLandscapeGUID(0xa7387e03, 0x7a95402c, 0x9292fe5f, 0x170d05a9); // random GUID, guaranteed to be random.
static const FCustomVersionRegistration UOculusAudioGeometryLandscapeGUIDRegistration(UOculusAudioGeometryLandscapeGUID, OVRA_AUDIO_GEOMETRY_LANDSCAPE_LATEST_VERSION, TEXT("OculusAudioGeometryLandscapeVersion"));


void UOculusAudioGeometryLandscape::Serialize(FArchive & Ar)
{
	// Tell the archive we are using a custom version.
	Ar.UsingCustomVersion(UOculusAudioGeometryLandscapeGUID);

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

#if WITH_EDITOR
	UWorld* World = GetWorld();
	if (Ar.IsSaving() && World != nullptr && !World->IsGameWorld())
	{
		CreateGeometryFromLandscape(Context, World, &ovrGeometryLandscape);

		if (ovrGeometryLandscape != nullptr)
		{
			ovrResult Result = OVRA_CALL(ovrAudio_AudioGeometryWriteMeshData)(ovrGeometryLandscape, &Serializer);
			check(Result == ovrSuccess);

			Result = OVRA_CALL(ovrAudio_DestroyAudioGeometry)(ovrGeometryLandscape);
			check(Result == ovrSuccess);

			ovrGeometryLandscape = nullptr;
		}
	}
#endif
	
	if (Ar.IsLoading())
	{
		ovrResult Result = OVRA_CALL(ovrAudio_CreateAudioGeometry)(Context, &ovrGeometryLandscape);
		check(Result == ovrSuccess);

		int64 PreviousPosition = Ar.Tell();
		Result = OVRA_CALL(ovrAudio_AudioGeometryReadMeshData)(ovrGeometryLandscape, &Serializer);
		if (Result != ovrSuccess)
		{
			Result = OVRA_CALL(ovrAudio_DestroyAudioGeometry)(ovrGeometryLandscape);
			check(Result == ovrSuccess);

			ovrGeometryLandscape = nullptr;
			Ar.Seek(PreviousPosition);
		}
	}
}

#if WITH_EDITOR
void UOculusAudioGeometryLandscape::CreateGeometryFromLandscape(ovrAudioContext Context, UWorld* World, ovrAudioGeometry* Geometry)
{
	for (TActorIterator<ALandscape> Itr(World); Itr; ++Itr)
	{
		TArray<FVector> Vertices;
		TArray<uint32> Indices;
		int32 FaceCount = 0;

		auto LandscapeCollisionComponents = Itr->CollisionComponents;

		ULandscapeInfo* LandscapeInfo = Itr->CreateLandscapeInfo();
		if (LandscapeInfo == nullptr)
		{
			UE_LOG(LogAudio, Warning, TEXT("Unabled to create audio propagation geometry for landscape, landscape info not found!"));
			return;
		}

		for (auto It = LandscapeInfo->XYtoComponentMap.CreateIterator(); It; ++It)
		{
			ULandscapeComponent* Component = It.Value();
			FLandscapeComponentDataInterface DataInterface(Component);

			const int Offset = Vertices.Num();
			const int Length = Component->ComponentSizeQuads + 1;
			for (auto y = 0; y < Length; ++y)
			{
				for (auto x = 0; x < Length; ++x)
				{
					Vertices.Add(OculusAudioSpatializationAudioMixer::ToOVRVector(DataInterface.GetWorldVertex(x, y)));
					
					// there are only (N-1)^2 quads for N^2 verts
					if (x == (Length - 1) || y == (Length - 1))
						continue;

					auto calcIndex = [](int x, int y, int length) { return (y * length) + x; };
					Indices.Add(Offset + calcIndex(x, y, Length));
					Indices.Add(Offset + calcIndex(x, y + 1, Length));
					Indices.Add(Offset + calcIndex(x + 1, y + 1, Length));
					Indices.Add(Offset + calcIndex(x + 1, y, Length));

					++FaceCount;
				}
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
		MeshGroup.faceType = ovrAudioFaceType_Quads;
		MeshGroup.material = ovrMaterial;

		ovrMesh.groups = &MeshGroup;
		ovrMesh.groupCount = 1;

		Result = OVRA_CALL(ovrAudio_CreateAudioGeometry)(Context, Geometry);
		if (Result != ovrSuccess) {
			UE_LOG(LogAudio, Warning, TEXT("Failed to create audio propagation geometry for landscape!"));
			return;
		}

		Result = OVRA_CALL(ovrAudio_AudioGeometryUploadMesh)(*Geometry, &ovrMesh);
		if (Result != ovrSuccess) {
			UE_LOG(LogAudio, Warning, TEXT("Failed adding landscape geometry to the audio propagation sub-system!"));
			return;
		}
	}

	UE_LOG(LogAudio, Warning, TEXT("Landscape complete!"));
}
#endif
