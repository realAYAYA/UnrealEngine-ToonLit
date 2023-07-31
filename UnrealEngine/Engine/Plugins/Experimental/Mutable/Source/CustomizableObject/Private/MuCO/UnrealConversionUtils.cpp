// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/UnrealConversionUtils.h"

#include "Containers/IndirectArray.h"
#include "Containers/Map.h"
#include "CoreTypes.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/UnrealMemory.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Math/Transform.h"
#include "Misc/AssertionMacros.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableObjectSystemPrivate.h"
#include "MuCO/UnrealPortabilityHelpers.h"
#include "MuR/Mesh.h"
#include "MuR/MutableTrace.h"
#include "MuR/Parameters.h"
#include "MuR/Ptr.h"
#include "RawIndexBuffer.h"
#include "Rendering/ColorVertexBuffer.h"
#include "Rendering/MultiSizeIndexContainer.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Rendering/SkeletalMeshDuplicatedVerticesBuffer.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "Rendering/StaticMeshVertexBuffer.h"
#include "SkeletalMeshTypes.h"
#include "StaticMeshResources.h"
#include "Trace/Detail/Channel.h"
#include "UObject/NameTypes.h"

class USkeleton;

namespace UnrealConversionUtils
{
	// Hidden functions only used internally to aid other functions
	namespace
	{
		/**
		 * Initializes the static mesh vertex buffers object provided with the data found on the mutable buffers
		 * @param OutVertexBuffers - The Unreal's vertex buffers container to be updated with the mutable data.
		 * @param NumVertices - The amount of vertices on the buffer
		 * @param NumTexCoords - The amount of texture coordinates
		 * @param bUseFullPrecisionUVs - Determines if we want to use or not full precision UVs
		 * @param bNeedCPUAccess - Determines if CPU access is required
		 * @param InMutablePositionData - Mutable position data buffer
		 * @param InMutableTangentData - Mutable tangent data buffer
		 * @param InMutableTextureData - Mutable texture data buffer
		 */
		void FStaticMeshVertexBuffers_InitWithMutableData(
			FStaticMeshVertexBuffers& OutVertexBuffers,
			const int32 NumVertices,
			const int32 NumTexCoords,
			const bool bUseFullPrecisionUVs,
			const bool bNeedCPUAccess,
			const void* InMutablePositionData,
			const void* InMutableTangentData,
			const void* InMutableTextureData)
		{
			// positions
			OutVertexBuffers.PositionVertexBuffer.Init(NumVertices, bNeedCPUAccess);
			FMemory::Memcpy(OutVertexBuffers.PositionVertexBuffer.GetVertexData(), InMutablePositionData, NumVertices * OutVertexBuffers.PositionVertexBuffer.GetStride());

			// tangent and texture coords
			OutVertexBuffers.StaticMeshVertexBuffer.SetUseFullPrecisionUVs(true);
			OutVertexBuffers.StaticMeshVertexBuffer.SetUseHighPrecisionTangentBasis(false);
			OutVertexBuffers.StaticMeshVertexBuffer.Init(NumVertices, NumTexCoords, bNeedCPUAccess);
			FMemory::Memcpy(OutVertexBuffers.StaticMeshVertexBuffer.GetTangentData(), InMutableTangentData, OutVertexBuffers.StaticMeshVertexBuffer.GetTangentSize());
			FMemory::Memcpy(OutVertexBuffers.StaticMeshVertexBuffer.GetTexCoordData(), InMutableTextureData, OutVertexBuffers.StaticMeshVertexBuffer.GetTexCoordSize());
		}


		/**
		 * Initializes the color vertex buffers object provided with the data found on the mutable buffers
		 * @param OutVertexBuffers - The Unreal's vertex buffers container to be updated with the mutable data.
		 * @param NumVertices - The amount of vertices on the buffer
		 * @param InMutableColorData - Mutable color data buffer
		 */
		void FColorVertexBuffers_InitWithMutableData(
			FStaticMeshVertexBuffers& OutVertexBuffers,
			const int32 NumVertices,
			const void* InMutableColorData
		)
		{
			// positions
			OutVertexBuffers.ColorVertexBuffer.Init(NumVertices);
			FMemory::Memcpy(OutVertexBuffers.ColorVertexBuffer.GetVertexData(), InMutableColorData, NumVertices * OutVertexBuffers.ColorVertexBuffer.GetStride());
		}


		/**
		 * Initializes the skin vertex buffers object provided with the data found on the mutable buffers
		 * @param OutVertexWeightBuffer - The Unreal's vertex buffers container to be updated with the mutable data.
		   * @param NumVertices - The amount of vertices on the buffer
		 * @param NumBones - The amount of bones to use to init the skin weights buffer
		   * @param NumBoneInfluences - The amount of bone influences on the buffer
		 * @param bNeedCPUAccess - Determines if CPU access is required
		 * @param InMutableData - Mutable data buffer
		 */
		void FSkinWeightVertexBuffer_InitWithMutableData(
			FSkinWeightVertexBuffer& OutVertexWeightBuffer,
			const int32 NumVertices,
			const int32 NumBones,
			const int32 NumBoneInfluences,
			const bool bNeedCPUAccess,
			const void* InMutableData)
		{
			// \todo Ugly cast.
			const_cast<FSkinWeightDataVertexBuffer*>(OutVertexWeightBuffer.GetDataVertexBuffer())->SetMaxBoneInfluences(NumBoneInfluences);
			const_cast<FSkinWeightDataVertexBuffer*>(OutVertexWeightBuffer.GetDataVertexBuffer())->Init(NumBones, NumVertices);

			if (NumVertices)
			{
				OutVertexWeightBuffer.SetNeedsCPUAccess(bNeedCPUAccess);

				FSkinWeightInfo* Data = OutVertexWeightBuffer.GetDataVertexBuffer()->GetWeightData();
				FMemory::Memcpy(Data, InMutableData, OutVertexWeightBuffer.GetVertexDataSize());
			}
		}
	}



	void BuildRefSkeleton(FInstanceUpdateData::FSkeletonData* OutMutSkeletonData
		, const FReferenceSkeleton& InSourceReferenceSkeleton, const TArray<bool>& InUsedBones,
		FReferenceSkeleton& InRefSkeleton, const USkeleton* InSkeleton)
	{
		const int32 SourceBoneCount = InSourceReferenceSkeleton.GetNum();
		const TArray<FTransform>& SourceRawMeshBonePose = InSourceReferenceSkeleton.GetRawRefBonePose();

		MUTABLE_CPUPROFILER_SCOPE(BuildSkeletonData_BuildRefSkeleton);

		// Build new RefSkeleton	
		FReferenceSkeletonModifier RefSkeletonModifier(InRefSkeleton, InSkeleton);

		const TArray<FMeshBoneInfo>& BoneInfo = InSourceReferenceSkeleton.GetRawRefBoneInfo();

		TMap<FName, uint16> BoneToFinalBoneIndexMap;
		BoneToFinalBoneIndexMap.Reserve(SourceBoneCount);

		uint32 FinalBoneCount = 0;
		for (int32 BoneIndex = 0; BoneIndex < SourceBoneCount; ++BoneIndex)
		{
			if (!InUsedBones[BoneIndex])
			{
				continue;
			}

			FName BoneName = BoneInfo[BoneIndex].Name;

			// Build a bone to index map so we can remap BoneMaps and ActiveBoneIndices later on
			BoneToFinalBoneIndexMap.Add(BoneName, FinalBoneCount);
			FinalBoneCount++;

			// Find parent index
			const int32 SourceParentIndex = BoneInfo[BoneIndex].ParentIndex;
			const int32 ParentIndex = SourceParentIndex != INDEX_NONE ? BoneToFinalBoneIndexMap[BoneInfo[SourceParentIndex].Name] : INDEX_NONE;

			RefSkeletonModifier.Add(FMeshBoneInfo(BoneName, BoneName.ToString(), ParentIndex), SourceRawMeshBonePose[BoneIndex]);
		}
	}


	void BuildSkeletalMeshElementDataAtLOD(const int32 MeshLODIndex,
		mu::MeshPtrConst InMutableMesh, USkeletalMesh* OutSkeletalMesh)
	{
		int32 SurfaceCount = 0;
		if (InMutableMesh)
		{
			SurfaceCount = InMutableMesh->GetSurfaceCount();
		}

		Helper_GetLODInfoArray(OutSkeletalMesh)[MeshLODIndex].LODMaterialMap.SetNum(1);
		Helper_GetLODInfoArray(OutSkeletalMesh)[MeshLODIndex].LODMaterialMap[0] = 0;

		for (int32 SurfaceIndex = 0; SurfaceIndex < SurfaceCount; ++SurfaceIndex)
		{
			new(Helper_GetLODRenderSections(OutSkeletalMesh, MeshLODIndex)) Helper_SkelMeshRenderSection();
		}
	}


	void SetupRenderSections(
		const USkeletalMesh* OutSkeletalMesh,
		const mu::MeshPtrConst InMutableMesh,
		const int32 MeshLODIndex,
		const TArray<uint16>& InBoneMap)
	{
		check(InMutableMesh);

		const mu::FMeshBufferSet& MutableMeshVertexBuffers = InMutableMesh->GetVertexBuffers();

		// Find the number of influences from this mesh
		int32 NumBoneInfluences = 0;
		int32 boneIndexBuffer = -1;
		int32 boneIndexChannel = -1;
		MutableMeshVertexBuffers.FindChannel(mu::MBS_BONEINDICES, 0, &boneIndexBuffer, &boneIndexChannel);
		if (boneIndexBuffer >= 0 || boneIndexChannel >= 0)
		{
			MutableMeshVertexBuffers.GetChannel(boneIndexBuffer, boneIndexChannel,
				nullptr, nullptr, nullptr, &NumBoneInfluences, nullptr);
		}

		const int32 SurfaceCount = InMutableMesh->GetSurfaceCount();
		for (int32 SurfaceIndex = 0; SurfaceIndex < SurfaceCount; ++SurfaceIndex)
		{
			MUTABLE_CPUPROFILER_SCOPE(UpdateSkeletalMesh_SurfaceLoop);

			int32 FirstIndex;
			int32 IndexCount;
			int32 FirstVertex;
			int32 VertexCount;
			InMutableMesh->GetSurface(SurfaceIndex, &FirstVertex, &VertexCount, &FirstIndex, &IndexCount);
			FSkelMeshRenderSection& Section = Helper_GetLODRenderSections(OutSkeletalMesh, MeshLODIndex)[SurfaceIndex];

			Section.DuplicatedVerticesBuffer.Init(1, TMap<int, TArray<int32>>());

			if (VertexCount == 0 || IndexCount == 0)
			{
				Section.bDisabled = true;
				continue; // Unreal doesn't like empty meshes
			}

			Section.BaseIndex = FirstIndex;
			Section.NumTriangles = IndexCount / 3;
			Section.BaseVertexIndex = FirstVertex;
			Section.MaxBoneInfluences = NumBoneInfluences;
			Section.NumVertices = VertexCount;
			Section.BoneMap.Append(InBoneMap);
		}
	}


	void CopyMutableVertexBuffers(
		USkeletalMesh* SkeletalMesh,
		const mu::MeshPtrConst MutableMesh,
		const int32 MeshLODIndex)
	{
		MUTABLE_CPUPROFILER_SCOPE(UpdateSkeletalMesh_SurfaceLoop_MemCpy);

		FSkeletalMeshLODRenderData& LODModel = Helper_GetLODData(SkeletalMesh)[MeshLODIndex];
		const mu::FMeshBufferSet& MutableMeshVertexBuffers = MutableMesh->GetVertexBuffers();
		const int32 NumVertices = MutableMeshVertexBuffers.GetElementCount();

		uint32 BuildFlags = SkeletalMesh->GetVertexBufferFlags();
		const bool bUseFullPrecisionUVs = (BuildFlags & ESkeletalMeshVertexFlags::UseFullPrecisionUVs) != 0;
		const bool bHasVertexColors = (BuildFlags & ESkeletalMeshVertexFlags::HasVertexColors) != 0;
		const int NumTexCoords = MutableMeshVertexBuffers.GetBufferChannelCount(MUTABLE_VERTEXBUFFER_TEXCOORDS);

		const bool bNeedsCPUAccess = Helper_GetLODInfoArray(SkeletalMesh)[MeshLODIndex].bAllowCPUAccess;

		FStaticMeshVertexBuffers_InitWithMutableData(
			LODModel.StaticVertexBuffers,
			NumVertices,
			NumTexCoords,
			bUseFullPrecisionUVs,
			bNeedsCPUAccess,
			MutableMeshVertexBuffers.GetBufferData(MUTABLE_VERTEXBUFFER_POSITION),
			MutableMeshVertexBuffers.GetBufferData(MUTABLE_VERTEXBUFFER_TANGENT),
			MutableMeshVertexBuffers.GetBufferData(MUTABLE_VERTEXBUFFER_TEXCOORDS)
		);

		mu::MESH_BUFFER_FORMAT BoneIndexFormat = mu::MBF_NONE;
		int32 NumBoneInfluences = 0;
		int32 BoneIndexBuffer = -1;
		int32 BoneIndexChannel = -1;
		MutableMeshVertexBuffers.FindChannel(mu::MBS_BONEINDICES, 0, &BoneIndexBuffer, &BoneIndexChannel);
		if (BoneIndexBuffer >= 0 || BoneIndexChannel >= 0)
		{
			MutableMeshVertexBuffers.GetChannel(BoneIndexBuffer, BoneIndexChannel,
				nullptr, nullptr, &BoneIndexFormat, &NumBoneInfluences, nullptr);
		}

		mu::MESH_BUFFER_FORMAT BoneWeightFormat = mu::MBF_NONE;
		int32 BoneWeightBuffer = -1;
		int32 BoneWeightChannel = -1;
		MutableMeshVertexBuffers.FindChannel(mu::MBS_BONEWEIGHTS, 0, &BoneWeightBuffer, &BoneWeightChannel);
		if (BoneWeightBuffer >= 0 || BoneWeightChannel >= 0)
		{
			MutableMeshVertexBuffers.GetChannel(BoneWeightBuffer, BoneWeightChannel,
				nullptr, nullptr, &BoneWeightFormat, nullptr, nullptr);
		}

		// Init skin weight buffer
		FSkinWeightVertexBuffer_InitWithMutableData(
			LODModel.SkinWeightVertexBuffer,
			NumVertices,
			NumBoneInfluences * NumVertices,
			NumBoneInfluences,
			bNeedsCPUAccess,
			MutableMeshVertexBuffers.GetBufferData(BoneIndexBuffer)
		);

		if (BoneIndexFormat == mu::MBF_UINT16)
		{
			LODModel.SkinWeightVertexBuffer.SetUse16BitBoneIndex(true);
		}

		if (BoneWeightFormat == mu::MBF_NUINT16)
		{
			// TODO: 
			unimplemented()
		}

		// Optional buffers
		for (int Buffer = MUTABLE_VERTEXBUFFER_TEXCOORDS + 1; Buffer < MutableMeshVertexBuffers.GetBufferCount(); ++Buffer)
		{
			if (MutableMeshVertexBuffers.GetBufferChannelCount(Buffer) > 0)
			{
				mu::MESH_BUFFER_SEMANTIC Semantic;
				mu::MESH_BUFFER_FORMAT Format;
				int32 SemanticIndex;
				int32 ComponentCount;
				int32 Offset;
				MutableMeshVertexBuffers.GetChannel(Buffer, 0, &Semantic, &SemanticIndex, &Format, &ComponentCount, &Offset);

				if (Semantic == mu::MBS_BONEINDICES)
				{
					const int32 BonesPerVertex = ComponentCount;
					const int32 NumBones = BonesPerVertex * NumVertices;

					check(LODModel.SkinWeightVertexBuffer.GetVariableBonesPerVertex() == false);
					FSkinWeightVertexBuffer_InitWithMutableData(
						LODModel.SkinWeightVertexBuffer,
						NumVertices,
						NumBones,
						NumBoneInfluences,
						bNeedsCPUAccess,
						MutableMeshVertexBuffers.GetBufferData(Buffer)
					);
				}

				// colour buffer?
				else if (Semantic == mu::MBS_COLOUR)
				{
					SkeletalMesh->SetHasVertexColors(true);
					const void* DataPtr = MutableMeshVertexBuffers.GetBufferData(Buffer);
					FColorVertexBuffers_InitWithMutableData(LODModel.StaticVertexBuffers, NumVertices, DataPtr);
					check(LODModel.StaticVertexBuffers.ColorVertexBuffer.GetStride() == MutableMeshVertexBuffers.GetElementSize(Buffer));
				}
			}
		}
	}


	bool CopyMutableIndexBuffers(mu::MeshPtrConst InMutableMesh, FSkeletalMeshLODRenderData& LODModel)
	{
		MUTABLE_CPUPROFILER_SCOPE(UpdateSkeletalMesh_BuildSkeletalMeshRenderData_IndexLoop);

		const int32 IndexCount = InMutableMesh->GetIndexBuffers().GetElementCount();

		if (IndexCount == 0)
		{
			// Copy indices from an empty buffer
			UE_LOG(LogMutable, Error, TEXT("UCustomizableInstancePrivateData::BuildSkeletalMeshRenderData is converting an empty mesh."));
			return false;
		}

		const uint8* DataPtr = InMutableMesh->GetIndexBuffers().GetBufferData(0);
		const int32 ElementSize = InMutableMesh->GetIndexBuffers().GetElementSize(0);

		LODModel.MultiSizeIndexContainer.CreateIndexBuffer(ElementSize);
		LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Insert(0, IndexCount);
		FMemory::Memcpy(LODModel.MultiSizeIndexContainer.GetIndexBuffer()->GetPointerTo(0), DataPtr, IndexCount * ElementSize);

		return true;
	}

}

	
		
