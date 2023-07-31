// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusHandTracking.h"
#include "OculusHMD.h"
#include "Misc/CoreDelegates.h"
#include "IOculusInputModule.h"

#include "Components/SkeletalMeshComponent.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Model.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"

#define OCULUS_TO_UE4_SCALE 100.0f

namespace OculusInput
{

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FQuat FOculusHandTracking::GetBoneRotation(const int32 ControllerIndex, const EOculusHandType DeviceHand, const EBone BoneId)
{
	IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
	FPlatformUserId InPlatformUser = FGenericPlatformMisc::GetPlatformUserForUserIndex(ControllerIndex);
	FInputDeviceId InDeviceId = INPUTDEVICEID_NONE;
	DeviceMapper.RemapControllerIdToPlatformUserAndDevice(ControllerIndex, InPlatformUser, InDeviceId);
	
	FQuat Rotation = FQuat::Identity;
	if (BoneId <= EBone::Invalid && BoneId >= EBone::Bone_Max)
	{
		return Rotation;
	}

#if OCULUS_INPUT_SUPPORTED_PLATFORMS
	TSharedPtr<FOculusInput> OculusInputModule = StaticCastSharedPtr<FOculusInput>(IOculusInputModule::Get().GetInputDevice());
	if (OculusInputModule.IsValid())
	{
		TArray<FOculusControllerPair> ControllerPairs = OculusInputModule.Get()->ControllerPairs;
		for (const FOculusControllerPair& HandPair : ControllerPairs)
		{
			if (HandPair.DeviceId == InDeviceId)
			{
				if (DeviceHand != EOculusHandType::None)
				{
					ovrpHand Hand = DeviceHand == EOculusHandType::HandLeft ? ovrpHand_Left : ovrpHand_Right;
					const FOculusHandControllerState& HandState = HandPair.HandControllerStates[Hand];
					int32 OvrBoneId = ToOvrBone(BoneId);
					Rotation = HandState.BoneRotations[OvrBoneId];
					break;
				}
			}
		}
	}
#endif

	return Rotation;
}

float FOculusHandTracking::GetHandScale(const int32 ControllerIndex, const EOculusHandType DeviceHand)
{
#if OCULUS_INPUT_SUPPORTED_PLATFORMS
	IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
	FPlatformUserId InPlatformUser = FGenericPlatformMisc::GetPlatformUserForUserIndex(ControllerIndex);
	FInputDeviceId InDeviceId = INPUTDEVICEID_NONE;
	DeviceMapper.RemapControllerIdToPlatformUserAndDevice(ControllerIndex, InPlatformUser, InDeviceId);
	
	TSharedPtr<FOculusInput> OculusInputModule = StaticCastSharedPtr<FOculusInput>(IOculusInputModule::Get().GetInputDevice());
	if (OculusInputModule.IsValid())
	{
		TArray<FOculusControllerPair> ControllerPairs = OculusInputModule.Get()->ControllerPairs;
		for (const FOculusControllerPair& HandPair : ControllerPairs)
		{
			if (HandPair.DeviceId == InDeviceId)
			{
				if (DeviceHand != EOculusHandType::None)
				{
					ovrpHand Hand = DeviceHand == EOculusHandType::HandLeft ? ovrpHand_Left : ovrpHand_Right;
					return HandPair.HandControllerStates[Hand].HandScale;
				}
			}
		}
	}
#endif
	return 1.0f;
}

ETrackingConfidence FOculusHandTracking::GetTrackingConfidence(const int32 ControllerIndex, const EOculusHandType DeviceHand)
{
#if OCULUS_INPUT_SUPPORTED_PLATFORMS
	IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
	FPlatformUserId InPlatformUser = FGenericPlatformMisc::GetPlatformUserForUserIndex(ControllerIndex);
	FInputDeviceId InDeviceId = INPUTDEVICEID_NONE;
	DeviceMapper.RemapControllerIdToPlatformUserAndDevice(ControllerIndex, InPlatformUser, InDeviceId);
	
	TSharedPtr<FOculusInput> OculusInputModule = StaticCastSharedPtr<FOculusInput>(IOculusInputModule::Get().GetInputDevice());
	if (OculusInputModule.IsValid())
	{
		TArray<FOculusControllerPair> ControllerPairs = OculusInputModule.Get()->ControllerPairs;
		for (const FOculusControllerPair& HandPair : ControllerPairs)
		{
			if (HandPair.DeviceId == InDeviceId)
			{
				if (DeviceHand != EOculusHandType::None)
				{
					ovrpHand Hand = DeviceHand == EOculusHandType::HandLeft ? ovrpHand_Left : ovrpHand_Right;
					return HandPair.HandControllerStates[Hand].TrackingConfidence;
				}
			}
		}
	}
#endif
	return ETrackingConfidence::Low;
}

ETrackingConfidence FOculusHandTracking::GetFingerTrackingConfidence(const int32 ControllerIndex, const EOculusHandType DeviceHand, const EOculusHandAxes Finger)
{
	IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
	FPlatformUserId InPlatformUser = FGenericPlatformMisc::GetPlatformUserForUserIndex(ControllerIndex);
	FInputDeviceId InDeviceId = INPUTDEVICEID_NONE;
	DeviceMapper.RemapControllerIdToPlatformUserAndDevice(ControllerIndex, InPlatformUser, InDeviceId);
	
	TSharedPtr<FOculusInput> OculusInputModule = StaticCastSharedPtr<FOculusInput>(IOculusInputModule::Get().GetInputDevice());
	if (OculusInputModule.IsValid())
	{
		TArray<FOculusControllerPair> ControllerPairs = OculusInputModule.Get()->ControllerPairs;
		for (const FOculusControllerPair& HandPair : ControllerPairs)
		{
			if (HandPair.DeviceId == InDeviceId)
			{
				if (DeviceHand != EOculusHandType::None)
				{
					ovrpHand Hand = DeviceHand == EOculusHandType::HandLeft ? ovrpHand_Left : ovrpHand_Right;
					return HandPair.HandControllerStates[Hand].FingerConfidences[(int)Finger];
				}
			}
		}
	}
	return ETrackingConfidence::Low;
}

FTransform FOculusHandTracking::GetPointerPose(const int32 ControllerIndex, const EOculusHandType DeviceHand, const float WorldToMeters)
{
#if OCULUS_INPUT_SUPPORTED_PLATFORMS
	IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
	FPlatformUserId InPlatformUser = FGenericPlatformMisc::GetPlatformUserForUserIndex(ControllerIndex);
	FInputDeviceId InDeviceId = INPUTDEVICEID_NONE;
	DeviceMapper.RemapControllerIdToPlatformUserAndDevice(ControllerIndex, InPlatformUser, InDeviceId);
	
	TSharedPtr<FOculusInput> OculusInputModule = StaticCastSharedPtr<FOculusInput>(IOculusInputModule::Get().GetInputDevice());
	if (OculusInputModule.IsValid())
	{
		TArray<FOculusControllerPair> ControllerPairs = OculusInputModule.Get()->ControllerPairs;
		for (const FOculusControllerPair& HandPair : ControllerPairs)
		{
			if (HandPair.DeviceId == InDeviceId)
			{
				if (DeviceHand != EOculusHandType::None)
				{
					ovrpHand Hand = DeviceHand == EOculusHandType::HandLeft ? ovrpHand_Left : ovrpHand_Right;
					FTransform PoseTransform = HandPair.HandControllerStates[Hand].PointerPose;
					PoseTransform.SetLocation(PoseTransform.GetLocation() * WorldToMeters);
					return PoseTransform;
				}
			}
		}
	}
#endif
	return FTransform();
}

bool FOculusHandTracking::IsPointerPoseValid(const int32 ControllerIndex, const EOculusHandType DeviceHand)
{
#if OCULUS_INPUT_SUPPORTED_PLATFORMS
	IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
	FPlatformUserId InPlatformUser = FGenericPlatformMisc::GetPlatformUserForUserIndex(ControllerIndex);
	FInputDeviceId InDeviceId = INPUTDEVICEID_NONE;
	DeviceMapper.RemapControllerIdToPlatformUserAndDevice(ControllerIndex, InPlatformUser, InDeviceId);
	
	TSharedPtr<FOculusInput> OculusInputModule = StaticCastSharedPtr<FOculusInput>(IOculusInputModule::Get().GetInputDevice());
	if (OculusInputModule.IsValid())
	{
		TArray<FOculusControllerPair> ControllerPairs = OculusInputModule.Get()->ControllerPairs;
		for (const FOculusControllerPair& HandPair : ControllerPairs)
		{
			if (HandPair.DeviceId == InDeviceId)
			{
				if (DeviceHand != EOculusHandType::None)
				{
					ovrpHand Hand = DeviceHand == EOculusHandType::HandLeft ? ovrpHand_Left : ovrpHand_Right;
					return HandPair.HandControllerStates[Hand].bIsPointerPoseValid;
				}
			}
		}
	}
#endif
	return false;
}

bool FOculusHandTracking::IsHandTrackingEnabled()
{
#if OCULUS_INPUT_SUPPORTED_PLATFORMS
	ovrpBool result;
	FOculusHMDModule::GetPluginWrapper().GetHandTrackingEnabled(&result);
	return result == ovrpBool_True;
#else
	return false;
#endif
}

bool FOculusHandTracking::IsHandDominant(const int32 ControllerIndex, const EOculusHandType DeviceHand)
{
#if OCULUS_INPUT_SUPPORTED_PLATFORMS
	IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
	FPlatformUserId InPlatformUser = FGenericPlatformMisc::GetPlatformUserForUserIndex(ControllerIndex);
	FInputDeviceId InDeviceId = INPUTDEVICEID_NONE;
	DeviceMapper.RemapControllerIdToPlatformUserAndDevice(ControllerIndex, InPlatformUser, InDeviceId);
	
	TSharedPtr<FOculusInput> OculusInputModule = StaticCastSharedPtr<FOculusInput>(IOculusInputModule::Get().GetInputDevice());
	if (OculusInputModule.IsValid())
	{
		TArray<FOculusControllerPair> ControllerPairs = OculusInputModule.Get()->ControllerPairs;
		for (const FOculusControllerPair& HandPair : ControllerPairs)
		{
			if (HandPair.DeviceId == InDeviceId)
			{
				if (DeviceHand != EOculusHandType::None)
				{
					ovrpHand Hand = DeviceHand == EOculusHandType::HandLeft ? ovrpHand_Left : ovrpHand_Right;
					return HandPair.HandControllerStates[Hand].bIsDominantHand;
				}
			}
		}
	}
#endif
	return false;
}

bool FOculusHandTracking::IsHandPositionValid(int32 ControllerIndex, EOculusHandType DeviceHand)
{
	IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
	FPlatformUserId InPlatformUser = FGenericPlatformMisc::GetPlatformUserForUserIndex(ControllerIndex);
	FInputDeviceId InDeviceId = INPUTDEVICEID_NONE;
	DeviceMapper.RemapControllerIdToPlatformUserAndDevice(ControllerIndex, InPlatformUser, InDeviceId);
	
	TSharedPtr<FOculusInput> OculusInputModule = StaticCastSharedPtr<FOculusInput>(IOculusInputModule::Get().GetInputDevice());
	if (OculusInputModule.IsValid())
	{
		TArray<FOculusControllerPair> ControllerPairs = OculusInputModule.Get()->ControllerPairs;
		for (const FOculusControllerPair& HandPair : ControllerPairs)
		{
			if (HandPair.DeviceId == InDeviceId)
			{
				if (DeviceHand != EOculusHandType::None)
				{
					ovrpHand Hand = DeviceHand == EOculusHandType::HandLeft ? ovrpHand_Left : ovrpHand_Right;
					return HandPair.HandControllerStates[Hand].bIsPositionValid;
				}
			}
		}
	}
	return false;
}

bool FOculusHandTracking::GetHandSkeletalMesh(USkeletalMesh* HandSkeletalMesh, const EOculusHandType SkeletonType, const EOculusHandType MeshType, const float WorldToMeters)
{
#if OCULUS_INPUT_SUPPORTED_PLATFORMS
	if (HandSkeletalMesh)
	{
		ovrpMesh* OvrMesh = new ovrpMesh();
		ovrpSkeleton2* OvrSkeleton = new ovrpSkeleton2();

		ovrpSkeletonType OvrSkeletonType = (ovrpSkeletonType)((int32)SkeletonType - 1);
		ovrpMeshType OvrMeshType = (ovrpMeshType)((int32)MeshType - 1);
		ovrpResult SkelResult = FOculusHMDModule::GetPluginWrapper().GetSkeleton2(OvrSkeletonType, OvrSkeleton);
		ovrpResult MeshResult = FOculusHMDModule::GetPluginWrapper().GetMesh(OvrMeshType, OvrMesh);
		if (SkelResult != ovrpSuccess || MeshResult != ovrpSuccess)
		{
#if !WITH_EDITOR
			UE_LOG(LogOcHandTracking, Error, TEXT("Failed to get mesh or skeleton data from Oculus runtime."));
#endif
			delete OvrMesh;
			delete OvrSkeleton;

			return false;
		}

		// Create Skeletal Mesh LOD Render Data
#if WITH_EDITOR
		FSkeletalMeshLODModel* LodRenderData = new FSkeletalMeshLODModel();
		HandSkeletalMesh->GetImportedModel()->LODModels.Add(LodRenderData);
#else
		FSkeletalMeshLODRenderData* LodRenderData = new FSkeletalMeshLODRenderData();
		HandSkeletalMesh->AllocateResourceForRendering();
		HandSkeletalMesh->GetResourceForRendering()->LODRenderData.Add(LodRenderData);
#endif

		// Set default LOD Info
		FSkeletalMeshLODInfo& LodInfo = HandSkeletalMesh->AddLODInfo();
		LodInfo.ScreenSize = 0.3f;
		LodInfo.LODHysteresis = 0.2f;
		LodInfo.BuildSettings.bUseFullPrecisionUVs = true;

		InitializeHandSkeleton(HandSkeletalMesh, OvrSkeleton, WorldToMeters);

		// Add default material as backup
		LodInfo.LODMaterialMap.Add(0);
		UMaterialInterface* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
		HandSkeletalMesh->GetMaterials().Add(DefaultMaterial);
		HandSkeletalMesh->GetMaterials()[0].UVChannelData.bInitialized = true;

		// Set skeletal mesh properties
		HandSkeletalMesh->SetHasVertexColors(true);
		HandSkeletalMesh->SetHasBeenSimplified(false);
		HandSkeletalMesh->SetEnablePerPolyCollision(false);

		InitializeHandMesh(HandSkeletalMesh, OvrMesh, WorldToMeters);

#if WITH_EDITOR
		HandSkeletalMesh->InvalidateDeriveDataCacheGUID();
		HandSkeletalMesh->PostEditChange();
#endif

		// Create Skeleton object and merge all bones
		HandSkeletalMesh->SetSkeleton(NewObject<USkeleton>());
		HandSkeletalMesh->GetSkeleton()->MergeAllBonesToBoneTree(HandSkeletalMesh);
		HandSkeletalMesh->PostLoad();

		delete OvrMesh;
		delete OvrSkeleton;

		return true;
	}
#endif
	return false;
}

void FOculusHandTracking::InitializeHandMesh(USkeletalMesh* SkeletalMesh, const ovrpMesh* OvrMesh, const float WorldToMeters)
{
#if WITH_EDITOR
	FSkeletalMeshLODModel* LodRenderData = &SkeletalMesh->GetImportedModel()->LODModels[0];

	// Initialize mesh section
	LodRenderData->Sections.SetNumUninitialized(1);
	new(&LodRenderData->Sections[0]) FSkelMeshSection();
	auto& MeshSection = LodRenderData->Sections[0];

	// Set default mesh section properties
	MeshSection.MaterialIndex = 0;
	MeshSection.BaseIndex = 0;
	MeshSection.NumTriangles = OvrMesh->NumIndices / 3;
	MeshSection.BaseVertexIndex = 0;
	MeshSection.MaxBoneInfluences = 4;
	MeshSection.NumVertices = OvrMesh->NumVertices;

	float MaxDistSq = MIN_flt;
	for (uint32_t VertexIndex = 0; VertexIndex < OvrMesh->NumVertices; VertexIndex++)
	{
		FSoftSkinVertex SoftVertex;
		
		// Update vertex data
		SoftVertex.Color = FColor::White;
		ovrpVector3f VertexPosition = OvrMesh->VertexPositions[VertexIndex];
		ovrpVector3f Normal = OvrMesh->VertexNormals[VertexIndex];
		SoftVertex.Position = FVector3f(VertexPosition.x, VertexPosition.z, VertexPosition.y) * WorldToMeters;
		SoftVertex.TangentZ = FVector3f(Normal.x, Normal.z, Normal.y);
		SoftVertex.TangentX = FVector3f(1.0f, 0.0f, 0.0f);
		SoftVertex.TangentY = FVector3f(0.0f, 1.0f, 0.0f);// SoftVertex.TangentZ^ SoftVertex.TangentX* SoftVertex.TangentZ.W;
		SoftVertex.UVs[0] = FVector2f(OvrMesh->VertexUV0[VertexIndex].x, OvrMesh->VertexUV0[VertexIndex].y);

		// Update the Bounds
		float VertexDistSq = SoftVertex.Position.SizeSquared();
		if (VertexDistSq > MaxDistSq)
			MaxDistSq = VertexDistSq;

		// Update blend weights and indices
		ovrpVector4f BlendWeights = OvrMesh->BlendWeights[VertexIndex];
		ovrpVector4s BlendIndices = OvrMesh->BlendIndices[VertexIndex];

		uint8 TotalWeight = 0;
		SoftVertex.InfluenceWeights[0] = 255.f * BlendWeights.x;
		SoftVertex.InfluenceBones[0] = BlendIndices.x;
		TotalWeight += 255.f * BlendWeights.x;
		SoftVertex.InfluenceWeights[1] = 255.f * BlendWeights.y;
		SoftVertex.InfluenceBones[1] = BlendIndices.y;
		TotalWeight += 255.f * BlendWeights.y;
		SoftVertex.InfluenceWeights[2] = 255.f * BlendWeights.z;
		SoftVertex.InfluenceBones[2] = BlendIndices.z;
		TotalWeight += 255.f * BlendWeights.z;
		SoftVertex.InfluenceWeights[3] = 255.f * BlendWeights.w;
		SoftVertex.InfluenceBones[3] = BlendIndices.w;
		TotalWeight += 255.f * BlendWeights.w;

		MeshSection.SoftVertices.Add(SoftVertex);
	}

	// Update bone map
	for (uint32 BoneIndex = 0; BoneIndex < (uint32)SkeletalMesh->GetRefSkeleton().GetNum(); BoneIndex++)
	{
		MeshSection.BoneMap.Add(BoneIndex);
	}

	// Update LOD render data
	LodRenderData->NumVertices = OvrMesh->NumVertices;
	LodRenderData->NumTexCoords = 1;

	// Create index buffer
	for (uint32_t Index = 0; Index < OvrMesh->NumIndices; Index++)
	{
		LodRenderData->IndexBuffer.Add(OvrMesh->Indices[Index]);
	}

	// Finalize Bounds
	float MaxDist = FMath::Sqrt(MaxDistSq);
	FBoxSphereBounds Bounds;
	Bounds.Origin = FVector::ZeroVector;
	Bounds.BoxExtent = FVector(MaxDist);
	Bounds.SphereRadius = MaxDist;
	SkeletalMesh->SetImportedBounds(Bounds);

#else
	FSkeletalMeshLODRenderData* LodRenderData = &SkeletalMesh->GetResourceForRendering()->LODRenderData[0];

	// Initialize Mesh Section
	LodRenderData->RenderSections.SetNumUninitialized(1);
	new(&LodRenderData->RenderSections[0]) FSkelMeshRenderSection();
	auto& MeshSection = LodRenderData->RenderSections[0];

	// Initialize render section properties
	MeshSection.MaterialIndex = 0;
	MeshSection.BaseIndex = 0;
	MeshSection.NumTriangles = OvrMesh->NumIndices / 3;
	MeshSection.BaseVertexIndex = 0;
	MeshSection.MaxBoneInfluences = 4;
	MeshSection.NumVertices = OvrMesh->NumVertices;
	MeshSection.bCastShadow = true;
	MeshSection.bDisabled = false;
	MeshSection.bRecomputeTangent = false;

	// Initialize Vertex Buffers
	LodRenderData->StaticVertexBuffers.PositionVertexBuffer.Init(OvrMesh->NumVertices);
	LodRenderData->StaticVertexBuffers.StaticMeshVertexBuffer.Init(OvrMesh->NumVertices, 1);
	LodRenderData->StaticVertexBuffers.ColorVertexBuffer.Init(OvrMesh->NumVertices);

	// Initialize Skin Weights
	TArray<FSkinWeightInfo> InWeights;
	InWeights.AddUninitialized(OvrMesh->NumVertices);

	float MaxDistSq = MIN_flt;
	TMap<int32, TArray<int32>> OverlappingVertices;
	for (uint32_t VertexIndex = 0; VertexIndex < OvrMesh->NumVertices; VertexIndex++)
	{
		// Initialize vertex data
		FModelVertex ModelVertex;

		// Update Model Vertex
		ovrpVector3f VertexPosition = OvrMesh->VertexPositions[VertexIndex];
		ovrpVector3f Normal = OvrMesh->VertexNormals[VertexIndex];
		ModelVertex.Position = FVector3f(VertexPosition.x, VertexPosition.z, VertexPosition.y) * WorldToMeters;	// LWC_TODO: Precision loss?
		ModelVertex.TangentZ = FVector3f(Normal.x, Normal.z, Normal.y);
		ModelVertex.TangentX = FVector3f(1.0f, 0.0f, 0.0f);
		ModelVertex.TexCoord = FVector2f(OvrMesh->VertexUV0[VertexIndex].x, OvrMesh->VertexUV0[VertexIndex].y);

		// Add Model Vertex data to vertex buffer
		LodRenderData->StaticVertexBuffers.PositionVertexBuffer.VertexPosition(VertexIndex) = ModelVertex.Position;
		LodRenderData->StaticVertexBuffers.StaticMeshVertexBuffer.SetVertexTangents(VertexIndex, ModelVertex.TangentX, ModelVertex.GetTangentY(), ModelVertex.TangentZ);
		LodRenderData->StaticVertexBuffers.StaticMeshVertexBuffer.SetVertexUV(VertexIndex, 0, ModelVertex.TexCoord);

		// Update the Bounds
		float VertexDistSq = ModelVertex.Position.SizeSquared();
		if (VertexDistSq > MaxDistSq)
			MaxDistSq = VertexDistSq;

		// Set vertex blend weights and indices
		TArray<int32> Vertices;
		ovrpVector4f BlendWeights = OvrMesh->BlendWeights[VertexIndex];
		ovrpVector4s BlendIndices = OvrMesh->BlendIndices[VertexIndex];

		InWeights[VertexIndex].InfluenceWeights[0] = 255.f * BlendWeights.x;
		InWeights[VertexIndex].InfluenceBones[0] = BlendIndices.x;
		Vertices.Add(BlendIndices.x);
		InWeights[VertexIndex].InfluenceWeights[1] = 255.f * BlendWeights.y;
		InWeights[VertexIndex].InfluenceBones[1] = BlendIndices.y;
		Vertices.Add(BlendIndices.y);
		InWeights[VertexIndex].InfluenceWeights[2] = 255.f * BlendWeights.z;
		InWeights[VertexIndex].InfluenceBones[2] = BlendIndices.z;
		Vertices.Add(BlendIndices.z);
		InWeights[VertexIndex].InfluenceWeights[3] = 255.f * BlendWeights.w;
		InWeights[VertexIndex].InfluenceBones[3] = BlendIndices.w;
		Vertices.Add(BlendIndices.w);

		OverlappingVertices.Add(VertexIndex, Vertices);
	}

	// Update bone map for mesh section
	for (uint32 BoneIndex = 0; BoneIndex < (uint32)SkeletalMesh->GetRefSkeleton().GetNum(); BoneIndex++)
	{
		MeshSection.BoneMap.Add(BoneIndex);
	}

	// Finalize Bounds
	float MaxDist = FMath::Sqrt(MaxDistSq);
	FBoxSphereBounds Bounds;
	Bounds.Origin = FVector::ZeroVector;
	Bounds.BoxExtent = FVector(MaxDist);
	Bounds.SphereRadius = MaxDist;
	SkeletalMesh->SetImportedBounds(Bounds);

	// Assign skin weights to vertex buffer
	LodRenderData->SkinWeightVertexBuffer = InWeights;
	MeshSection.DuplicatedVerticesBuffer.Init(OvrMesh->NumVertices, OverlappingVertices);

	// Set index buffer
	LodRenderData->MultiSizeIndexContainer.CreateIndexBuffer(sizeof(uint16_t));
	for (uint32_t Index = 0; Index < OvrMesh->NumIndices; Index++)
	{
		LodRenderData->MultiSizeIndexContainer.GetIndexBuffer()->AddItem(OvrMesh->Indices[Index]);
	}
#endif
}

void FOculusHandTracking::InitializeHandSkeleton(USkeletalMesh* SkeletalMesh, const ovrpSkeleton2* OvrSkeleton, const float WorldToMeters)
{
	SkeletalMesh->GetRefSkeleton().Empty(OvrSkeleton->NumBones);

#if WITH_EDITOR
	FSkeletalMeshLODModel* LodRenderData = &SkeletalMesh->GetImportedModel()->LODModels[0];
#else
	FSkeletalMeshLODRenderData* LodRenderData = &SkeletalMesh->GetResourceForRendering()->LODRenderData[0];
#endif
	SkeletalMesh->SetHasBeenSimplified(false);
	SkeletalMesh->SetHasVertexColors(true);

	for (uint32 BoneIndex = 0; BoneIndex < OvrSkeleton->NumBones; BoneIndex++)
	{
		LodRenderData->ActiveBoneIndices.Add(BoneIndex);
		LodRenderData->RequiredBones.Add(BoneIndex);

		FString BoneString = GetBoneName(BoneIndex);
		FName BoneName = FName(*BoneString);

		FTransform Transform = FTransform::Identity;
		FVector BonePosition = OvrBoneVectorToFVector(OvrSkeleton->Bones[BoneIndex].Pose.Position, WorldToMeters);
		FQuat BoneRotation = BoneIndex == 0 ? FQuat(-1.0f, 0.0f, 0.0f, 1.0f) : OvrBoneQuatToFQuat(OvrSkeleton->Bones[BoneIndex].Pose.Orientation);
		Transform.SetLocation(BonePosition);
		Transform.SetRotation(BoneRotation);

		FReferenceSkeletonModifier Modifier = FReferenceSkeletonModifier(SkeletalMesh->GetRefSkeleton(), nullptr);
		int32 ParentIndex = -1;
		if (BoneIndex > 0)
		{
			if (OvrSkeleton->Bones[BoneIndex].ParentBoneIndex == ovrpBoneId::ovrpBoneId_Invalid)
			{
				ParentIndex = 0;
			}
			else
			{
				ParentIndex = OvrSkeleton->Bones[BoneIndex].ParentBoneIndex;
			}
		}
		Modifier.Add(FMeshBoneInfo(BoneName, BoneString, ParentIndex), Transform);
	}
	SkeletalMesh->CalculateInvRefMatrices();
}

TArray<FOculusCapsuleCollider> FOculusHandTracking::InitializeHandPhysics(const EOculusHandType SkeletonType, USkinnedMeshComponent* HandComponent, const float WorldToMeters)
{
	TArray<FOculusCapsuleCollider> CollisionCapsules;
	ovrpSkeleton2* OvrSkeleton = new ovrpSkeleton2();

#if OCULUS_INPUT_SUPPORTED_PLATFORMS
	ovrpSkeletonType OvrSkeletonType = (ovrpSkeletonType)((int32)SkeletonType - 1);
	if (FOculusHMDModule::GetPluginWrapper().GetSkeleton2(OvrSkeletonType, OvrSkeleton) != ovrpSuccess)
	{
#if !WITH_EDITOR
		UE_LOG(LogOcHandTracking, Error, TEXT("Failed to get skeleton data from Oculus runtime."));
#endif
		delete OvrSkeleton;
		return CollisionCapsules;
	}
#endif

	TArray<UPrimitiveComponent*> IgnoreCapsules;
	CollisionCapsules.AddDefaulted(OvrSkeleton->NumBoneCapsules);
	for (uint32 CapsuleIndex = 0; CapsuleIndex < OvrSkeleton->NumBoneCapsules; CapsuleIndex++)
	{
		ovrpBoneCapsule OvrBoneCapsule = OvrSkeleton->BoneCapsules[CapsuleIndex];

		UCapsuleComponent* Capsule = NewObject<UCapsuleComponent>(HandComponent);

		FVector CapsulePointZero = OvrBoneVectorToFVector(OvrBoneCapsule.Points[0], WorldToMeters);
		FVector CapsulePointOne = OvrBoneVectorToFVector(OvrBoneCapsule.Points[1], WorldToMeters);
		FVector Delta = (CapsulePointOne - CapsulePointZero);

		FName BoneName = HandComponent->GetSkinnedAsset()->GetRefSkeleton().GetBoneName(OvrBoneCapsule.BoneIndex);

		float CapsuleHeight = Delta.Size();
		float CapsuleRadius = OvrBoneCapsule.Radius * WorldToMeters;

		Capsule->SetCapsuleRadius(CapsuleRadius);
		Capsule->SetCapsuleHalfHeight(Delta.Size() / 2 + CapsuleRadius);
		Capsule->SetupAttachment(HandComponent, BoneName);
		Capsule->SetCollisionProfileName(HandComponent->GetCollisionProfileName());
		Capsule->RegisterComponentWithWorld(HandComponent->GetWorld());
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		FRotator CapsuleRotation = FQuat::FindBetweenVectors(FVector::RightVector, Delta).Rotator() + FRotator(0, 0, 90);;

		Capsule->SetRelativeRotation(CapsuleRotation);
		Capsule->SetRelativeLocation(CapsulePointZero + (Delta / 2));

		CollisionCapsules[CapsuleIndex].Capsule = Capsule;
		CollisionCapsules[CapsuleIndex].BoneId = (EBone)OvrBoneCapsule.BoneIndex;

		IgnoreCapsules.Add(Capsule);
	}

	for (int32 CapsuleIndex = 0; CapsuleIndex < CollisionCapsules.Num(); CapsuleIndex++)
	{
		CollisionCapsules[CapsuleIndex].Capsule->MoveIgnoreComponents = IgnoreCapsules;
	}

	return CollisionCapsules;
}

ovrpBoneId FOculusHandTracking::ToOvrBone(EBone Bone)
{
	if (Bone > EBone::Bone_Max)
		return ovrpBoneId_Invalid;

	return (ovrpBoneId)Bone;
}

FString FOculusHandTracking::GetBoneName(uint8 Bone)
{
	uint8 HandBone = Bone == ovrpBoneId_Invalid ? (uint8)EBone::Invalid : Bone;

	UEnum* BoneEnum = FindObject<UEnum>(nullptr, TEXT("/Script/OculusInput.EBone"), true);
	if (BoneEnum)
	{
		return BoneEnum->GetDisplayNameTextByValue(HandBone).ToString();
	}
	return FString("Invalid");
}

ETrackingConfidence FOculusHandTracking::ToETrackingConfidence(ovrpTrackingConfidence Confidence)
{
	ETrackingConfidence TrackingConfidence = ETrackingConfidence::Low;
	switch (Confidence)
	{
	case ovrpTrackingConfidence_Low:
		TrackingConfidence = ETrackingConfidence::Low;
		break;
	case ovrpTrackingConfidence_High:
		TrackingConfidence = ETrackingConfidence::High;
		break;
	}
	return TrackingConfidence;
}

FVector FOculusHandTracking::OvrBoneVectorToFVector(ovrpVector3f ovrpVector, float WorldToMeters)
{
	return FVector(ovrpVector.x, -ovrpVector.y, ovrpVector.z) * WorldToMeters;
}

FQuat FOculusHandTracking::OvrBoneQuatToFQuat(ovrpQuatf ovrpQuat)
{
	return FQuat(ovrpQuat.x, -ovrpQuat.y, ovrpQuat.z, -ovrpQuat.w);
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}