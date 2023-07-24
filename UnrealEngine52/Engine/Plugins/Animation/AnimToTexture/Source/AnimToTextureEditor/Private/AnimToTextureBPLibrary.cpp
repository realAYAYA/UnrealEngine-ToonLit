// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimToTextureBPLibrary.h"
#include "AnimToTexture.h"
#include "AnimToTextureUtils.h"
#include "AnimToTextureSkeletalMesh.h"
#include "EvaluateSequenceAnimInstance.h"

#include "LevelEditor.h"
#include "RawMesh.h"
#include "MeshUtilities.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimSequence.h"
#include "Math/Vector.h"
#include "Math/NumericLimits.h"
#include "MeshDescription.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MaterialEditingLibrary.h"

using namespace AnimToTexture_Private;

UAnimToTextureBPLibrary::UAnimToTextureBPLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}

void UAnimToTextureBPLibrary::AnimationToTexture(UAnimToTextureDataAsset* DataAsset, const FTransform RootTransform, bool& bSuccess)
{
	bSuccess = false; 

	if (!DataAsset)
	{
		return;
	}

	// Reset DataAsset Info Values
	DataAsset->ResetInfo();

	// Check StaticMesh
	if (!DataAsset->GetStaticMesh())
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid StaticMesh"));
		return;
	}

	// Check SkeletalMesh
	if (!DataAsset->GetSkeletalMesh())
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid SkeletalMesh"));
		return;
	}

	// Check Skeleton
	if (!DataAsset->GetSkeletalMesh()->GetSkeleton())
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid SkeletalMesh. No valid Skeleton found"));
		return;
	}

	// Check StaticMesh LOD
	if (!DataAsset->GetStaticMesh()->IsSourceModelValid(DataAsset->StaticLODIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid StaticMesh LOD Index"));
		return;
	}

	// Check SkeletalMesh LOD
	if (!DataAsset->GetSkeletalMesh()->IsValidLODIndex(DataAsset->SkeletalLODIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid SkeletalMesh LOD Index"));
		return;
	}

	// Check Socket.
	bool bValidSocket = false;
	if (DataAsset->AttachToSocket.IsValid() && !DataAsset->AttachToSocket.IsNone())
	{
		if (HasBone(DataAsset->GetSkeletalMesh(), DataAsset->AttachToSocket))
		{
			bValidSocket = true;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Invalid Socket: %s"), *DataAsset->AttachToSocket.ToString());
			return;
		}
	}
	if (bValidSocket && DataAsset->Mode == EAnimToTextureMode::Vertex)
	{
		UE_LOG(LogTemp, Warning, TEXT("Unable to use Socket in Vertex Mode. Use Bone Mode instead."));
		return;
	}

	// Check if UVChannel is being used by the Lightmap UV
	const FStaticMeshSourceModel& SourceModel = DataAsset->GetStaticMesh()->GetSourceModel(DataAsset->StaticLODIndex);
	if (SourceModel.BuildSettings.bGenerateLightmapUVs &&
		SourceModel.BuildSettings.DstLightmapIndex == DataAsset->UVChannel)
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid UVChannel: %i. Already used by LightMap"), DataAsset->UVChannel);
		return;
	}

	// ---------------------------------------------------------------------------		
	// Get Meshes Vertices and Mapping.
	// NOTE: We need to create a Mapping between the StaticMesh and the SkeletalMesh
	//       Since they dont have same number of points.
	//

	// Get Static Vertices 
	// NOTE: these are the RenderData Vertices.
	TArray<FVector3f> Vertices;
	TArray<FVector3f> Normals;
	const int32 NumVertices = GetVertices(DataAsset->GetStaticMesh(), DataAsset->StaticLODIndex, Vertices, Normals);

	// Get SkeletalMesh Vertices 
	TArray<FVector3f> SkelVertices;
	GetVertices(DataAsset->GetSkeletalMesh(), DataAsset->SkeletalLODIndex, SkelVertices);
	
	// Get Mapping
	TArray<FVertexToMeshMapping> Mapping;
	FVertexToMeshMapping::Create(
		DataAsset->GetStaticMesh(), DataAsset->StaticLODIndex,
		DataAsset->GetSkeletalMesh(), DataAsset->SkeletalLODIndex, Mapping);

	// --------------------------------------------------------------------------
		
	// Create Temp Actor
	check(GEditor);
	UWorld* World = GEditor->GetEditorWorldContext().World();
	check(World);

	AActor* Actor = World->SpawnActor<AActor>();
	check(Actor);

	// Create Temp SkeletalMesh Component
	USkeletalMeshComponent* MeshComponent = NewObject<USkeletalMeshComponent>(Actor);
	check(MeshComponent);
	MeshComponent->SetSkeletalMesh(DataAsset->GetSkeletalMesh());
	MeshComponent->SetForcedLOD(1); // Force to LOD0;
	MeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	MeshComponent->SetUpdateAnimationInEditor(true);
	MeshComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	MeshComponent->RegisterComponent();

	// ---------------------------------------------------------------------------
	// Get Reference Skeleton Transforms
	//
	int32 NumBones = INDEX_NONE;
	int32 SocketIndex = INDEX_NONE;
	TArray<FName>     BoneNames;
	TArray<FVector3f> BoneRefPositions;
	TArray<FVector4>  BoneRefRotations;
	TArray<FVector3f> BonePositions;
	TArray<FVector4>  BoneRotations;
	
	if (DataAsset->Mode == EAnimToTextureMode::Bone)
	{
		// Gets Ref Bone Position and Rotations.
		NumBones = GetRefBonePositionsAndRotations(MeshComponent,
			BoneRefPositions, BoneRefRotations);

		// NOTE: there is a limitation with the number of bones atm.
		if (NumBones > 256)
		{
			UE_LOG(LogTemp, Warning, TEXT("Invalid Number of Bones. There is a maximum of 256 bones"))
			return;
		}

		// Get Bone Names (no virtual)
		GetBoneNames(MeshComponent->GetSkeletalMeshAsset(), BoneNames);

		// Make sure array sizes are correct.
		check(BoneNames.Num() == NumBones);

		// Check if Socket is in BoneNames
		if (bValidSocket && !BoneNames.Find(DataAsset->AttachToSocket, SocketIndex))
		{
			UE_LOG(LogTemp, Warning, TEXT("Socket: %s not found in Raw Bone List"), *DataAsset->AttachToSocket.ToString());
			return;
		}

		// Add RefPose 
		// Note: this is added in the first frame of the Bone Position and Rotation Textures
		BonePositions.Append(BoneRefPositions);
		BoneRotations.Append(BoneRefRotations);
	}

	// ---------------------------------------------------------------------------
	// Get Vertex Data (for all frames)
	//		
	TArray<FVector3f> VertexDeltas;
	TArray<FVector3f> VertexNormals;
	
	// Get Animation Frames Data
	//
	const float SampleInterval = 1.f / DataAsset->SampleRate;

	for (const FAnimSequenceInfo& AnimSequenceInfo : DataAsset->AnimSequences)
	{
		UAnimSequence* AnimSequence = AnimSequenceInfo.AnimSequence;

		if (!AnimSequenceInfo.bEnabled || !AnimSequence)
		{
			continue;
		}
		
		// Make sure SkeletalMesh is compatible with AnimSequence
		if (!MeshComponent->GetSkeletalMeshAsset()->GetSkeleton()->IsCompatibleForEditor(AnimSequence->GetSkeleton()))
		{
			UE_LOG(LogTemp, Warning, TEXT("Invalid AnimSequence: %s for given SkeletalMesh: %s"), *AnimSequence->GetFName().ToString(), *MeshComponent->GetSkeletalMeshAsset()->GetFName().ToString());
			continue;
		}
		// Set AnimSequence
		else
		{			
			MeshComponent->SetAnimation(AnimSequence);
		}

		// -----------------------------------------------------------------------------------
		// Get Number of Frames
		//
		int32 StartFrame;
		int32 EndFrame;
		
		// Get Range from AnimSequence
		if (!AnimSequenceInfo.bUseCustomRange)
		{
			StartFrame = 0;
			EndFrame = AnimSequence->GetNumberOfSampledKeys() - 1; // AnimSequence->GetNumberOfFrames();
		}
		// Get Range from DataAsset
		else
		{
			StartFrame = AnimSequenceInfo.StartFrame;
			EndFrame = AnimSequenceInfo.EndFrame;
		}
		if (EndFrame - StartFrame <= 0)
		{
			continue;
		}
		
		// ---------------------------------------------------------------------------
		// 
		float Time = 0.f;
		const float EndTime = AnimSequence->GetTimeAtFrame(EndFrame);

		int32 SampleIndex = 0;

		while (Time < EndTime)
		{
			Time = FMath::Clamp(SampleIndex * SampleInterval, 0.f, EndTime);
			SampleIndex++;

			// Go To Time
			MeshComponent->SetPosition(Time);

			// Update SkelMesh Animation.
			MeshComponent->TickAnimation(0.f, false /*bNeedsValidRootMotion*/);
			// MeshComponent->TickComponent(0.f, ELevelTick::LEVELTICK_All, nullptr);
			MeshComponent->RefreshBoneTransforms(nullptr /*TickFunction*/);
			
			// ---------------------------------------------------------------------------
			// Store Vertex Deltas & Normals.
			//
			if (DataAsset->Mode == EAnimToTextureMode::Vertex)
			{
				TArray<FVector3f> VertexFrameDeltas;
				TArray<FVector3f> VertexFrameNormals;
				
				GetVertexDeltasAndNormals(MeshComponent, DataAsset->SkeletalLODIndex,
					Vertices, Normals, Mapping,
					RootTransform,
					VertexFrameDeltas, VertexFrameNormals);
					
				VertexDeltas.Append(VertexFrameDeltas);
				VertexNormals.Append(VertexFrameNormals);

			} // End Vertex Mode

			// ---------------------------------------------------------------------------
			// Store Bone Positions & Rotations
			//
			else if (DataAsset->Mode == EAnimToTextureMode::Bone)
			{
				TArray<FVector3f> BoneFramePositions;
				TArray<FVector4> BoneFrameRotations;

				GetBonePositionsAndRotations(MeshComponent, BoneRefPositions,
					BoneFramePositions, BoneFrameRotations);

				BonePositions.Append(BoneFramePositions);
				BoneRotations.Append(BoneFrameRotations);

			} // End Bone Mode

		} // End Frame

		// Store Anim Info Data
		FAnimInfo AnimInfo;
		AnimInfo.NumFrames = SampleIndex;
		AnimInfo.AnimStart = DataAsset->NumFrames;
		AnimInfo.bLooping = AnimSequenceInfo.bLooping;
		DataAsset->Animations.Add(AnimInfo);

		// Accumulate Frames
		DataAsset->NumFrames += AnimInfo.NumFrames;
	} // End Anim
		
	// Destroy Temp Component
	MeshComponent->UnregisterComponent();
	MeshComponent->DestroyComponent();

	Actor->Destroy();
	
	// ---------------------------------------------------------------------------
	// Nothing to do here ...
	//
	if (!DataAsset->NumFrames || !NumVertices)
	{
		return;
	}

	// ---------------------------------------------------------------------------
	if (DataAsset->Mode == EAnimToTextureMode::Vertex)
	{
		// Find Best Resolution for Vertex Data
		int32 Height, Width;
		if (!FindBestResolution(DataAsset->NumFrames, NumVertices, 
								Height, Width, DataAsset->VertexRowsPerFrame, 
								DataAsset->MaxHeight, DataAsset->MaxWidth, DataAsset->bEnforcePowerOfTwo))
		{
			UE_LOG(LogTemp, Warning, TEXT("Vertex Animation data cannot be fit in a %ix%i texture."), DataAsset->MaxHeight, DataAsset->MaxWidth);
			return;
		}

		// Normalize Vertex Data
		TArray<FVector3f> NormalizedVertexDeltas;
		TArray<FVector3f> NormalizedVertexNormals;
		NormalizeVertexData(
			VertexDeltas, VertexNormals,
			DataAsset->VertexMinBBox, DataAsset->VertexSizeBBox,
			NormalizedVertexDeltas, NormalizedVertexNormals);

		// Write Textures
		if (DataAsset->Precision == EAnimToTexturePrecision::SixteenBits)
		{
			WriteVectorsToTexture<FVector3f, FHighPrecision>(NormalizedVertexDeltas, DataAsset->NumFrames, DataAsset->VertexRowsPerFrame, Height, Width, DataAsset->GetVertexPositionTexture());
			WriteVectorsToTexture<FVector3f, FHighPrecision>(NormalizedVertexNormals, DataAsset->NumFrames, DataAsset->VertexRowsPerFrame, Height, Width, DataAsset->GetVertexNormalTexture());
		}
		else
		{
			WriteVectorsToTexture<FVector3f, FLowPrecision>(NormalizedVertexDeltas, DataAsset->NumFrames, DataAsset->VertexRowsPerFrame, Height, Width, DataAsset->GetVertexPositionTexture());
			WriteVectorsToTexture<FVector3f, FLowPrecision>(NormalizedVertexNormals, DataAsset->NumFrames, DataAsset->VertexRowsPerFrame, Height, Width, DataAsset->GetVertexNormalTexture());
		}		

		// Add Vertex UVChannel
		CreateUVChannel(DataAsset->GetStaticMesh(), DataAsset->StaticLODIndex, DataAsset->UVChannel, Height, Width);

		// Update Bounds
		SetBoundsExtensions(DataAsset->GetStaticMesh(), DataAsset->VertexMinBBox, DataAsset->VertexSizeBBox);

		// Done with StaticMesh
		DataAsset->GetStaticMesh()->PostEditChange();
	}

	// ---------------------------------------------------------------------------
	
	if (DataAsset->Mode == EAnimToTextureMode::Bone)
	{
		// Find Best Resolution for Bone Data
		int32 Height, Width;

		// Note we are adding +1 frame for the ref pose
		if (!FindBestResolution(DataAsset->NumFrames + 1, NumBones, 
								Height, Width, DataAsset->BoneRowsPerFrame, 
								DataAsset->MaxHeight, DataAsset->MaxWidth, DataAsset->bEnforcePowerOfTwo))
		{
			UE_LOG(LogTemp, Warning, TEXT("Bone Animation data cannot be fit in a %ix%i texture."), DataAsset->MaxHeight, DataAsset->MaxWidth);
			return;
		}

		// Write Bone Position and Rotation Textures
		{
			// Normalize Bone Data
			TArray<FVector3f> NormalizedBonePositions;
			TArray<FVector4> NormalizedBoneRotations;
			NormalizeBoneData(
				BonePositions, BoneRotations,
				DataAsset->BoneMinBBox, DataAsset->BoneSizeBBox,
				NormalizedBonePositions, NormalizedBoneRotations);

			// Write Textures
			if (DataAsset->Precision == EAnimToTexturePrecision::SixteenBits)
			{
				WriteVectorsToTexture<FVector3f, FHighPrecision>(NormalizedBonePositions, DataAsset->NumFrames + 1, DataAsset->BoneRowsPerFrame, Height, Width, DataAsset->GetBonePositionTexture());
				WriteVectorsToTexture<FVector4, FHighPrecision>(NormalizedBoneRotations, DataAsset->NumFrames + 1, DataAsset->BoneRowsPerFrame, Height, Width, DataAsset->GetBoneRotationTexture());
			}
			else
			{
				WriteVectorsToTexture<FVector3f, FLowPrecision>(NormalizedBonePositions, DataAsset->NumFrames + 1, DataAsset->BoneRowsPerFrame, Height, Width, DataAsset->GetBonePositionTexture());
				WriteVectorsToTexture<FVector4, FLowPrecision>(NormalizedBoneRotations, DataAsset->NumFrames + 1, DataAsset->BoneRowsPerFrame, Height, Width, DataAsset->GetBoneRotationTexture());
			}
		}

		// ---------------------------------------------------------------------------

		// Find Best Resolution for Weights Data
		if (!FindBestResolution(2, NumVertices, 
								Height, Width, DataAsset->BoneWeightRowsPerFrame, 
								DataAsset->MaxHeight, DataAsset->MaxWidth, DataAsset->bEnforcePowerOfTwo))
		{
			UE_LOG(LogTemp, Warning, TEXT("Weights Data cannot be fit in a %ix%i texture."), DataAsset->MaxHeight, DataAsset->MaxWidth);
			return;
		}

		// Write Weights Texture
		{
			TArray<TVertexSkinWeight<4>> SkinWeights;

			// Store Influence Weights
			if (!bValidSocket)
			{
				// Get SkeletalMesh Weights
				TArray<VertexSkinWeightMax> SkeletalMeshSkinWeights;
				GetSkinWeights(DataAsset->GetSkeletalMesh(), DataAsset->SkeletalLODIndex, SkeletalMeshSkinWeights);

				// Interpolate Weights with Mapping
				TArray<VertexSkinWeightMax> StaticMeshSkinWeights;
				FVertexToMeshMapping::InterpolateSkinWeights(Mapping, SkeletalMeshSkinWeights, StaticMeshSkinWeights);
				
				// Reduce SkeletalMesh Weights to 4 highest influences.
				ReduceSkinWeights(StaticMeshSkinWeights, SkinWeights);
			}

			// If Valid Socket, set all influences to same index.
			else
			{
				// Set all indices and weights to same SocketIndex
				SkinWeights.SetNumUninitialized(NumVertices);
				for (TVertexSkinWeight<4>& SkinWeight : SkinWeights)
				{
					SkinWeight.BoneWeights = TStaticArray<uint8, 4>(InPlace, 255);
					SkinWeight.MeshBoneIndices = TStaticArray<uint16, 4>(InPlace, SocketIndex);
				}
			}

			// Write Bone Weights Texture
			WriteSkinWeightsToTexture(SkinWeights,
				DataAsset->BoneWeightRowsPerFrame, Height, Width, DataAsset->GetBoneWeightTexture());
		}

		// Add Vertex UVChannel
		CreateUVChannel(DataAsset->GetStaticMesh(), DataAsset->StaticLODIndex, DataAsset->UVChannel, Height, Width);

		// Update Bounds
		SetBoundsExtensions(DataAsset->GetStaticMesh(), DataAsset->BoneMinBBox, DataAsset->BoneSizeBBox);

		// Done with StaticMesh
		DataAsset->GetStaticMesh()->PostEditChange();
	}

	// ---------------------------------------------------------------------------
	// Mark Packages dirty
	//
	DataAsset->MarkPackageDirty();
	
	// All good here !
	bSuccess = true;
}


// 
void UAnimToTextureBPLibrary::GetVertexDeltasAndNormals(const USkeletalMeshComponent* SkeletalMeshComponent, const int32 LODIndex, 
	const TArray<FVector3f>& Vertices, const TArray<FVector3f>& Normals,  const TArray<FVertexToMeshMapping>& Mapping,
	const FTransform RootTransform,
	TArray<FVector3f>& OutVertexDeltas, TArray<FVector3f>& OutVertexNormals)
{
	OutVertexDeltas.Reset();
	OutVertexNormals.Reset();

	check(Vertices.Num() == Mapping.Num());

	TArray<FVector3f> SkinnedVertices;
	GetSkinnedVertices(SkeletalMeshComponent, LODIndex, SkinnedVertices);

	// Loop thru static vertices and find the mapped SkeletalMesh Vertex
	for (int32 VertexIndex = 0; VertexIndex < Vertices.Num(); VertexIndex++)
	{
		// Get Static/SkeletalMesh Mapping
		const FVertexToMeshMapping& VertexMapping = Mapping[VertexIndex];

		// Transform Vertex
		// (assuming indices are valid)
		const FIntVector3& Triangle = VertexMapping.Triangle;
		const FVector3f& A = SkinnedVertices[Triangle.X];
		const FVector3f& B = SkinnedVertices[Triangle.Y];
		const FVector3f& C = SkinnedVertices[Triangle.Z];

		const FVector3f VertexPosition = VertexMapping.TransformPosition(Vertices[VertexIndex], A, B, C);
		const FVector3f VertexNormal = VertexMapping.TransformVector(Normals[VertexIndex], A, B, C);

		// Transform Position and Delta with RootTransform
		const FVector3f TransformedVertexDelta = ((FVector3f)RootTransform.TransformPosition((FVector)VertexPosition)) - Vertices[VertexIndex];
		const FVector3f TransformedVertexNormal = (FVector3f)RootTransform.TransformVector((FVector)VertexNormal);
		
		OutVertexDeltas.Add(TransformedVertexDelta);
		OutVertexNormals.Add(TransformedVertexNormal);
	}
}


int32 UAnimToTextureBPLibrary::GetRefBonePositionsAndRotations(const USkeletalMeshComponent* SkeletalMeshComponent,
	TArray<FVector3f>& OutBoneRefPositions, TArray<FVector4>& OutBoneRefRotations)
{
	OutBoneRefPositions.Reset();
	OutBoneRefRotations.Reset();

	// Get Number of RawBones (no virtual)
	const int32 NumBones = GetNumBones(SkeletalMeshComponent->GetSkeletalMeshAsset());
	
	// Get Raw Ref Bone (no virtual)
	TArray<FTransform> RefBoneTransforms;
	GetRefBoneTransforms(SkeletalMeshComponent->GetSkeletalMeshAsset(), RefBoneTransforms);
	DecomposeTransformations(RefBoneTransforms, OutBoneRefPositions, OutBoneRefRotations);

	return NumBones;
}


int32 UAnimToTextureBPLibrary::GetBonePositionsAndRotations(const USkeletalMeshComponent* SkeletalMeshComponent, const TArray<FVector3f>& BoneRefPositions,
	TArray<FVector3f>& BonePositions, TArray<FVector4>& BoneRotations)
{
	BonePositions.Reset();
	BoneRotations.Reset();

	// Get Relative Transforms
	// Note: Size is of Raw bones in SkeletalMesh. These are the original/raw bones of the asset, without Virtual Bones.
	TArray<FMatrix44f> RefToLocals;
	SkeletalMeshComponent->CacheRefToLocalMatrices(RefToLocals);
	const int32 NumBones = RefToLocals.Num();

	// check size
	check(NumBones == BoneRefPositions.Num());

	// Get Component Space Transforms
	// Note returns all transforms, including VirtualBones
	const TArray<FTransform>& CompSpaceTransforms = SkeletalMeshComponent->GetComponentSpaceTransforms();
	check(CompSpaceTransforms.Num() >= RefToLocals.Num());

	for (int32 BoneIndex = 0; BoneIndex < NumBones; BoneIndex++)
	{
		// Decompose Transformation (ComponentSpace)
		const FTransform& CompSpaceTransform = CompSpaceTransforms[BoneIndex];
		FVector3f BonePosition;
		FVector4 BoneRotation;
		DecomposeTransformation(CompSpaceTransform, BonePosition, BoneRotation);

		// Position Delta (from RefPose)
		const FVector3f Delta = BonePosition - BoneRefPositions[BoneIndex];

		// Decompose Transformation (Relative to RefPose)
		FVector3f BoneRelativePosition;
		FVector4 BoneRelativeRotation;
		const FMatrix RefToLocalMatrix(RefToLocals[BoneIndex]);
		const FTransform RelativeTransform(RefToLocalMatrix);
		DecomposeTransformation(RelativeTransform, BoneRelativePosition, BoneRelativeRotation);

		BonePositions.Add(Delta);
		BoneRotations.Add(BoneRelativeRotation);
	}

	return NumBones;
}


void UAnimToTextureBPLibrary::UpdateMaterialInstanceFromDataAsset(UAnimToTextureDataAsset* DataAsset, UMaterialInstanceConstant* MaterialInstance, 
	const bool bAnimate, const EAnimToTextureNumBoneInfluences NumBoneInfluences, const EMaterialParameterAssociation MaterialParameterAssociation)
{
	if (!MaterialInstance || !DataAsset)
	{
		return;
	}

	// Set UVChannel
	switch (DataAsset->UVChannel)
	{
		case 0:
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV0, true, MaterialParameterAssociation);
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV1, false, MaterialParameterAssociation);
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV2, false, MaterialParameterAssociation);
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV3, false, MaterialParameterAssociation);
			break;
		case 1:
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV0, false, MaterialParameterAssociation);
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV1, true, MaterialParameterAssociation);
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV2, false, MaterialParameterAssociation);
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV3, false, MaterialParameterAssociation);
			break;
		case 2:
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV0, false, MaterialParameterAssociation);
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV1, false, MaterialParameterAssociation);
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV2, true, MaterialParameterAssociation);
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV3, false, MaterialParameterAssociation);
			break;
		case 3:
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV0, false, MaterialParameterAssociation);
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV1, false, MaterialParameterAssociation);
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV2, false, MaterialParameterAssociation);
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV3, true, MaterialParameterAssociation);
			break;
		default:
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV0, false, MaterialParameterAssociation);
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV1, true, MaterialParameterAssociation);
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV2, false, MaterialParameterAssociation);
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseUV3, false, MaterialParameterAssociation);
			break;
	}

	// Update Vertex Params
	if (DataAsset->Mode == EAnimToTextureMode::Vertex)
	{
		FLinearColor VectorParameter;
		VectorParameter = FLinearColor(DataAsset->VertexMinBBox);
		UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(MaterialInstance, AnimToTextureParamNames::BoundingBoxMin, VectorParameter, MaterialParameterAssociation);
		
		VectorParameter = FLinearColor(DataAsset->VertexSizeBBox);
		UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(MaterialInstance, AnimToTextureParamNames::BoundingBoxScale, VectorParameter, MaterialParameterAssociation);
		UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(MaterialInstance, AnimToTextureParamNames::NumFrames, DataAsset->NumFrames, MaterialParameterAssociation);
		UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(MaterialInstance, AnimToTextureParamNames::RowsPerFrame, DataAsset->VertexRowsPerFrame, MaterialParameterAssociation);

		UMaterialEditingLibrary::SetMaterialInstanceTextureParameterValue(MaterialInstance, AnimToTextureParamNames::VertexPositionTexture, DataAsset->GetVertexPositionTexture(), MaterialParameterAssociation);
		UMaterialEditingLibrary::SetMaterialInstanceTextureParameterValue(MaterialInstance, AnimToTextureParamNames::VertexNormalTexture, DataAsset->GetVertexNormalTexture(), MaterialParameterAssociation);

	}

	// Update Bone Params
	else if (DataAsset->Mode == EAnimToTextureMode::Bone)
	{
		FLinearColor VectorParameter;
		VectorParameter = FLinearColor(DataAsset->BoneMinBBox);
		UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(MaterialInstance, AnimToTextureParamNames::BoundingBoxMin, VectorParameter, MaterialParameterAssociation);

		VectorParameter = FLinearColor(DataAsset->BoneSizeBBox);
		UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(MaterialInstance, AnimToTextureParamNames::BoundingBoxScale, VectorParameter, MaterialParameterAssociation);
		UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(MaterialInstance, AnimToTextureParamNames::NumFrames, DataAsset->NumFrames, MaterialParameterAssociation);
		UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(MaterialInstance, AnimToTextureParamNames::RowsPerFrame, DataAsset->BoneRowsPerFrame, MaterialParameterAssociation);
		UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(MaterialInstance, AnimToTextureParamNames::BoneWeightRowsPerFrame, DataAsset->BoneWeightRowsPerFrame, MaterialParameterAssociation);

		UMaterialEditingLibrary::SetMaterialInstanceTextureParameterValue(MaterialInstance, AnimToTextureParamNames::BonePositionTexture, DataAsset->GetBonePositionTexture(), MaterialParameterAssociation);
		UMaterialEditingLibrary::SetMaterialInstanceTextureParameterValue(MaterialInstance, AnimToTextureParamNames::BoneRotationTexture, DataAsset->GetBoneRotationTexture(), MaterialParameterAssociation);
		UMaterialEditingLibrary::SetMaterialInstanceTextureParameterValue(MaterialInstance, AnimToTextureParamNames::BoneWeightsTexture, DataAsset->GetBoneWeightTexture(), MaterialParameterAssociation);

		// Num Influences
		switch (NumBoneInfluences)
		{
			case EAnimToTextureNumBoneInfluences::One:
				UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseTwoInfluences, false, MaterialParameterAssociation);
				UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseFourInfluences, false, MaterialParameterAssociation);
				break;
			case EAnimToTextureNumBoneInfluences::Two:
				UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseTwoInfluences, true, MaterialParameterAssociation);
				UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseFourInfluences, false, MaterialParameterAssociation);
				break;
			case EAnimToTextureNumBoneInfluences::Four:
				UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseTwoInfluences, false, MaterialParameterAssociation);
				UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::UseFourInfluences, true, MaterialParameterAssociation);
				break;
		}

	}

	// Animate
	UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(MaterialInstance, AnimToTextureParamNames::Animate, bAnimate, MaterialParameterAssociation);

	// Update Material
	UMaterialEditingLibrary::UpdateMaterialInstance(MaterialInstance);

	// Rebuild Material
	UMaterialEditingLibrary::RebuildMaterialInstanceEditors(MaterialInstance->GetMaterial());

	// Set Preview Mesh
	if (DataAsset->GetStaticMesh())
	{
		MaterialInstance->PreviewMesh = DataAsset->GetStaticMesh();
	}

	MaterialInstance->MarkPackageDirty();
}


bool UAnimToTextureBPLibrary::SetLightMapIndex(UStaticMesh* StaticMesh, const int32 LODIndex, const int32 LightmapIndex, bool bGenerateLightmapUVs)
{
	if (!StaticMesh)
	{
		return false;
	}

	if (LODIndex >= 0 && !StaticMesh->IsSourceModelValid(LODIndex))
	{
		return false;
	}

	// 
	FStaticMeshSourceModel& SourceModel = StaticMesh->GetSourceModel(LODIndex);
	SourceModel.BuildSettings.bGenerateLightmapUVs = bGenerateLightmapUVs;
	SourceModel.BuildSettings.DstLightmapIndex = LightmapIndex;
	StaticMesh->SetLightMapCoordinateIndex(LightmapIndex);

	// Build mesh from source
	StaticMesh->Build(false);
	StaticMesh->PostEditChange();
	StaticMesh->MarkPackageDirty();

	return true;
}


UStaticMesh* UAnimToTextureBPLibrary::ConvertSkeletalMeshToStaticMesh(USkeletalMesh* SkeletalMesh, const FString PackageName, const int32 LODIndex)
{
	if (!SkeletalMesh || PackageName.IsEmpty())
	{
		return nullptr;
	}

	if (!FPackageName::IsValidObjectPath(PackageName))
	{
		return nullptr;
	}

	if (LODIndex >= 0 && !SkeletalMesh->IsValidLODIndex(LODIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid LODIndex: %i"), LODIndex);
		return nullptr;
	}

	// Create Temp Actor
	check(GEditor);
	UWorld* World = GEditor->GetEditorWorldContext().World();
	check(World);
	AActor* Actor = World->SpawnActor<AActor>();
	check(Actor);

	// Create Temp SkeletalMesh Component
	USkeletalMeshComponent* MeshComponent = NewObject<USkeletalMeshComponent>(Actor);
	MeshComponent->RegisterComponent();
	MeshComponent->SetSkeletalMesh(SkeletalMesh);
	TArray<UMeshComponent*> MeshComponents = { MeshComponent };

	UStaticMesh* OutStaticMesh = nullptr;
	bool bGeneratedCorrectly = true;

	// Create New StaticMesh
	if (!FPackageName::DoesPackageExist(PackageName))
	{
		IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
		OutStaticMesh = MeshUtilities.ConvertMeshesToStaticMesh(MeshComponents, FTransform::Identity, PackageName);
	}
	// Update Existing StaticMesh
	else
	{
		// Load Existing Mesh
		OutStaticMesh = LoadObject<UStaticMesh>(nullptr, *PackageName);
	}

	if (OutStaticMesh)
	{
		// Create Temp Package.
		// because 
		UPackage* TransientPackage = GetTransientPackage();

		// Create Temp Mesh.
		IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
		UStaticMesh* TempMesh = MeshUtilities.ConvertMeshesToStaticMesh(MeshComponents, FTransform::Identity, TransientPackage->GetPathName());

		// make sure transactional flag is on
		TempMesh->SetFlags(RF_Transactional);

		// Copy All LODs
		if (LODIndex < 0)
		{
			const int32 NumSourceModels = TempMesh->GetNumSourceModels();
			OutStaticMesh->SetNumSourceModels(NumSourceModels);

			for (int32 Index = 0; Index < NumSourceModels; ++Index)
			{
				// Get RawMesh
				FRawMesh RawMesh;
				TempMesh->GetSourceModel(Index).LoadRawMesh(RawMesh);

				// Set RawMesh
				OutStaticMesh->GetSourceModel(Index).SaveRawMesh(RawMesh);
			};
		}

		// Copy Single LOD
		else
		{
			if (LODIndex >= TempMesh->GetNumSourceModels())
			{
				UE_LOG(LogTemp, Warning, TEXT("Invalid Source Model Index: %i"), LODIndex);
				bGeneratedCorrectly = false;
			}
			else
			{
				OutStaticMesh->SetNumSourceModels(1);

				// Get RawMesh
				FRawMesh RawMesh;
				TempMesh->GetSourceModel(LODIndex).LoadRawMesh(RawMesh);

				// Set RawMesh
				OutStaticMesh->GetSourceModel(0).SaveRawMesh(RawMesh);
			}
		}
			
		// Copy Materials
		const TArray<FStaticMaterial>& Materials = TempMesh->GetStaticMaterials();
		OutStaticMesh->SetStaticMaterials(Materials);

		// Done
		TArray<FText> OutErrors;
		OutStaticMesh->Build(true, &OutErrors);
		OutStaticMesh->MarkPackageDirty();
	}

	// Destroy Temp Component and Actor
	MeshComponent->UnregisterComponent();
	MeshComponent->DestroyComponent();
	Actor->Destroy();

	return bGeneratedCorrectly ? OutStaticMesh : nullptr;
}


void UAnimToTextureBPLibrary::NormalizeVertexData(
	const TArray<FVector3f>& Deltas, const TArray<FVector3f>& Normals,
	FVector& OutMinBBox, FVector& OutSizeBBox,
	TArray<FVector3f>& OutNormalizedDeltas, TArray<FVector3f>& OutNormalizedNormals)
{
	check(Deltas.Num() == Normals.Num());

	// ---------------------------------------------------------------------------
	// Compute Bounding Box
	//
	OutMinBBox = { TNumericLimits<float>::Max(), TNumericLimits<float>::Max(), TNumericLimits<float>::Max() };
	FVector3f MaxBBox = { TNumericLimits<float>::Min(), TNumericLimits<float>::Min(), TNumericLimits<float>::Min() };
	
	for (const FVector3f& Delta: Deltas)
	{
		// Find Min/Max BoundingBox
		OutMinBBox.X = FMath::Min(Delta.X, OutMinBBox.X);
		OutMinBBox.Y = FMath::Min(Delta.Y, OutMinBBox.Y);
		OutMinBBox.Z = FMath::Min(Delta.Z, OutMinBBox.Z);

		MaxBBox.X = FMath::Max(Delta.X, MaxBBox.X);
		MaxBBox.Y = FMath::Max(Delta.Y, MaxBBox.Y);
		MaxBBox.Z = FMath::Max(Delta.Z, MaxBBox.Z);
	}

	OutSizeBBox = (FVector)MaxBBox - OutMinBBox;

	// ---------------------------------------------------------------------------
	// Normalize Vertex Position Deltas
	// Basically we want all deltas to be between [0, 1]
	
	// Compute Normalization Factor per-axis.
	const FVector NormFactor = {
		1.f / static_cast<float>(OutSizeBBox.X),
		1.f / static_cast<float>(OutSizeBBox.Y),
		1.f / static_cast<float>(OutSizeBBox.Z) };

	OutNormalizedDeltas.SetNumUninitialized(Deltas.Num());
	for (int32 Index = 0; Index < Deltas.Num(); ++Index)
	{
		OutNormalizedDeltas[Index] = (FVector3f)(((FVector)Deltas[Index] - OutMinBBox) * NormFactor);
	}

	// ---------------------------------------------------------------------------
	// Normalize Vertex Normals
	// And move them to [0, 1]
	
	OutNormalizedNormals.SetNumUninitialized(Normals.Num());
	for (int32 Index = 0; Index < Normals.Num(); ++Index)
	{
		OutNormalizedNormals[Index] = (Normals[Index].GetSafeNormal() + FVector3f::OneVector) * 0.5f;
	}

}

void UAnimToTextureBPLibrary::NormalizeBoneData(
	const TArray<FVector3f>& Positions, const TArray<FVector4>& Rotations,
	FVector& OutMinBBox, FVector& OutSizeBBox, 
	TArray<FVector3f>& OutNormalizedPositions, TArray<FVector4>& OutNormalizedRotations)
{
	check(Positions.Num() == Rotations.Num());

	// ---------------------------------------------------------------------------
	// Compute Position Bounding Box
	//
	OutMinBBox = { TNumericLimits<float>::Max(), TNumericLimits<float>::Max(), TNumericLimits<float>::Max() };
	FVector3f MaxBBox = { TNumericLimits<float>::Min(), TNumericLimits<float>::Min(), TNumericLimits<float>::Min() };

	for (const FVector3f& Position : Positions)
	{
		// Find Min/Max BoundingBox
		OutMinBBox.X = FMath::Min(Position.X, OutMinBBox.X);
		OutMinBBox.Y = FMath::Min(Position.Y, OutMinBBox.Y);
		OutMinBBox.Z = FMath::Min(Position.Z, OutMinBBox.Z);

		MaxBBox.X = FMath::Max(Position.X, MaxBBox.X);
		MaxBBox.Y = FMath::Max(Position.Y, MaxBBox.Y);
		MaxBBox.Z = FMath::Max(Position.Z, MaxBBox.Z);
	}

	OutSizeBBox = (FVector)MaxBBox - OutMinBBox;

	// ---------------------------------------------------------------------------
	// Normalize Bone Position.
	// Basically we want all positions to be between [0, 1]

	// Compute Normalization Factor per-axis.
	const FVector NormFactor = {
		1.f / static_cast<float>(OutSizeBBox.X),
		1.f / static_cast<float>(OutSizeBBox.Y),
		1.f / static_cast<float>(OutSizeBBox.Z) };

	OutNormalizedPositions.SetNumUninitialized(Positions.Num());
	for (int32 Index = 0; Index < Positions.Num(); ++Index)
	{
		OutNormalizedPositions[Index] = FVector3f(((FVector)Positions[Index] - OutMinBBox) * NormFactor);
	}

	// ---------------------------------------------------------------------------
	// Normalize Rotations
	// And move them to [0, 1]
	OutNormalizedRotations.SetNumUninitialized(Rotations.Num());
	for (int32 Index = 0; Index < Rotations.Num(); ++Index)
	{
		const FVector4 Axis = Rotations[Index];
		const float Angle = Rotations[Index].W; // Angle are returned in radians and they go from [0-pi*2]

		OutNormalizedRotations[Index] = (Axis.GetSafeNormal() + FVector::OneVector) * 0.5f;
		OutNormalizedRotations[Index].W = Angle / (PI * 2.f);
	}
}


bool UAnimToTextureBPLibrary::CreateUVChannel(
	UStaticMesh* StaticMesh, const int32 LODIndex, const int32 UVChannelIndex,
	const int32 Height, const int32 Width)
{
	check(StaticMesh);

	if (!StaticMesh->IsSourceModelValid(LODIndex))
	{
		return false;
	}

	// ----------------------------------------------------------------------------
	// Get Mesh Description.
	// This is needed for Inserting UVChannel
	FMeshDescription* MeshDescription = StaticMesh->GetMeshDescription(LODIndex);
	check(MeshDescription);

	// Add New UVChannel.
	if (UVChannelIndex == StaticMesh->GetNumUVChannels(LODIndex))
	{
		if (!StaticMesh->InsertUVChannel(LODIndex, UVChannelIndex))
		{
			UE_LOG(LogTemp, Warning, TEXT("Unable to Add UVChannel"));
			return false;
		}
	}
	else if (UVChannelIndex > StaticMesh->GetNumUVChannels(LODIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("UVChannel: %i Out of Range. Number of existing UVChannels: %i"), UVChannelIndex, StaticMesh->GetNumUVChannels(LODIndex));
		return false;
	}

	// -----------------------------------------------------------------------------

	TMap<FVertexInstanceID, FVector2D> TexCoords;

	for (const FVertexInstanceID VertexInstanceID : MeshDescription->VertexInstances().GetElementIDs())
	{
		const FVertexID VertexID = MeshDescription->GetVertexInstanceVertex(VertexInstanceID);
		const int32 VertexIndex = VertexID.GetValue();

		float U = (0.5f / (float)Width) + (VertexIndex % Width) / (float)Width;
		float V = (0.5f / (float)Height) + (VertexIndex / Width) / (float)Height;
		
		TexCoords.Add(VertexInstanceID, FVector2D(U, V));
	}

	// Set Full Precision UVs
	SetFullPrecisionUVs(StaticMesh, LODIndex, true);

	// Set UVs
	if (StaticMesh->SetUVChannel(LODIndex, UVChannelIndex, TexCoords))
	{
		return true;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Unable to Set UVChannel: %i. TexCoords: %i"), UVChannelIndex, TexCoords.Num());
		return false;
	};

	return false;
}

bool UAnimToTextureBPLibrary::FindBestResolution(
	const int32 NumFrames, const int32 NumElements,
	int32& OutHeight, int32& OutWidth, int32& OutRowsPerFrame,
	const int32 MaxHeight, const int32 MaxWidth, bool bEnforcePowerOfTwo)
{
	if (bEnforcePowerOfTwo)
	{
		OutWidth = 2;
		while (OutWidth < NumElements && OutWidth < MaxWidth)
		{
			OutWidth *= 2;
		}
		OutRowsPerFrame = FMath::CeilToInt(NumElements / (float)OutWidth);

		const int32 TargetHeight = NumFrames * OutRowsPerFrame;
		OutHeight = 2;
		while (OutHeight < TargetHeight)
		{
			OutHeight *= 2;
		}
	}
	else
	{
		OutRowsPerFrame = FMath::CeilToInt(NumElements / (float)MaxWidth);
		OutWidth = FMath::CeilToInt(NumElements / (float)OutRowsPerFrame);
		OutHeight = NumFrames * OutRowsPerFrame;
	}

	const bool bValidResolution = OutWidth <= MaxWidth && OutHeight <= MaxHeight;
	return bValidResolution;
};

void UAnimToTextureBPLibrary::SetFullPrecisionUVs(UStaticMesh* StaticMesh, int32 LODIndex, bool bFullPrecision)
{
	check(StaticMesh);

	if (StaticMesh->IsSourceModelValid(LODIndex))
	{
		FStaticMeshSourceModel& SourceModel = StaticMesh->GetSourceModel(LODIndex);
		SourceModel.BuildSettings.bUseFullPrecisionUVs = bFullPrecision;
	}
}

void UAnimToTextureBPLibrary::SetBoundsExtensions(UStaticMesh* StaticMesh, const FVector& MinBBox, const FVector& SizeBBox)
{
	check(StaticMesh);

	// Calculate MaxBBox
	const FVector MaxBBox = SizeBBox + MinBBox;

	// Reset current extension bounds
	const FVector PositiveBoundsExtension = StaticMesh->GetPositiveBoundsExtension();
	const FVector NegativeBoundsExtension = StaticMesh->GetNegativeBoundsExtension();
		
	// Get current BoundingBox including extensions
	FBox BoundingBox = StaticMesh->GetBoundingBox();
		
	// Remove extensions from BoundingBox
	BoundingBox.Max -= PositiveBoundsExtension;
	BoundingBox.Min += NegativeBoundsExtension;
		
	// Calculate New BoundingBox
	FVector NewMaxBBox(
		FMath::Max(BoundingBox.Max.X, MaxBBox.X),
		FMath::Max(BoundingBox.Max.Y, MaxBBox.Y),
		FMath::Max(BoundingBox.Max.Z, MaxBBox.Z)
	);
		
	FVector NewMinBBox(
		FMath::Min(BoundingBox.Min.X, MinBBox.X),
		FMath::Min(BoundingBox.Min.Y, MinBBox.Y),
		FMath::Min(BoundingBox.Min.Z, MinBBox.Z)
	);

	// Calculate New Extensions
	FVector NewPositiveBoundsExtension = NewMaxBBox - BoundingBox.Max;
	FVector NewNegativeBoundsExtension = BoundingBox.Min - NewMinBBox;
				
	// Update StaticMesh
	StaticMesh->SetPositiveBoundsExtension(NewPositiveBoundsExtension);
	StaticMesh->SetNegativeBoundsExtension(NewNegativeBoundsExtension);
	StaticMesh->CalculateExtendedBounds();
}
