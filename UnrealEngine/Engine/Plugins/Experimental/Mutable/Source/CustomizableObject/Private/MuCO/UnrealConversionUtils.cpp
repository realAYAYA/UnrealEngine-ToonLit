// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/UnrealConversionUtils.h"

#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/UnrealPortabilityHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "GPUSkinVertexFactory.h"

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
			const void* InMutableData,
			const uint32 MutableDataSize)
		{
			FSkinWeightDataVertexBuffer* VertexBuffer = OutVertexWeightBuffer.GetDataVertexBuffer();
			VertexBuffer->SetMaxBoneInfluences(NumBoneInfluences);
			VertexBuffer->Init(NumBones, NumVertices);

			bool bIsVariableBonesPerVertex = OutVertexWeightBuffer.GetDataVertexBuffer()->GetVariableBonesPerVertex();
			check(!FGPUBaseSkinVertexFactory::UseUnlimitedBoneInfluences(NumBoneInfluences) || bIsVariableBonesPerVertex);

			OutVertexWeightBuffer.SetNeedsCPUAccess(bNeedCPUAccess);

			if (NumVertices)
			{
				void* Data = VertexBuffer->GetWeightData();

				uint32 OutVertexWeightBufferSize = OutVertexWeightBuffer.GetDataVertexBuffer()->GetVertexDataSize();
				ensure(MutableDataSize == OutVertexWeightBufferSize);

				FMemory::Memcpy(Data, InMutableData, OutVertexWeightBufferSize);

				if (bIsVariableBonesPerVertex)
				{
					OutVertexWeightBuffer.RebuildLookupVertexBuffer();

					{
						MUTABLE_CPUPROFILER_SCOPE(OptimizeVertexAndLookupBuffers);

						// Everything in this scope is optional and makes extra copies, but will optimize the variable bone
						// influences buffers. Without it, the vertices are assumed to have a constant NumBoneInfluences per vertex.
						TArray<FSkinWeightInfo> TempVertices;
						OutVertexWeightBuffer.GetSkinWeights(TempVertices);

						// The assignment operator actually optimizes the DataVertexBuffer
						OutVertexWeightBuffer = TempVertices;
					}
				}
			}
		}
	}


	void SetupRenderSections(
		const USkeletalMesh* OutSkeletalMesh,
		const mu::MeshPtrConst InMutableMesh,
		const int32 MeshLODIndex,
		const TArray<uint16>& InBoneMap,
		const int32 InFirstBoneMapIndex)
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
			MUTABLE_CPUPROFILER_SCOPE(SetupRenderSections);

			int32 FirstIndex;
			int32 IndexCount;
			int32 FirstVertex;
			int32 VertexCount;
			int32 FirstBone;
			int32 BoneCount;
			InMutableMesh->GetSurface(SurfaceIndex, &FirstVertex, &VertexCount, &FirstIndex, &IndexCount, &FirstBone, &BoneCount);
			FSkelMeshRenderSection& Section = OutSkeletalMesh->GetResourceForRendering()->LODRenderData[MeshLODIndex].RenderSections[SurfaceIndex];

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

			// InBoneMaps may contain bonemaps from other sections. Copy the bones belonging to this mesh.
			FirstBone += InFirstBoneMapIndex;
			
			Section.BoneMap.Reserve(BoneCount);
			for (int32 BoneMapIndex = 0; BoneMapIndex < BoneCount; ++BoneMapIndex, ++FirstBone)
			{
				Section.BoneMap.Add(InBoneMap[FirstBone]);
			}
		}
	}


	void CopyMutableVertexBuffers(
		USkeletalMesh* SkeletalMesh,
		const mu::MeshPtrConst MutableMesh,
		const int32 MeshLODIndex)

	{
		MUTABLE_CPUPROFILER_SCOPE(CopyMutableVertexBuffers);

		FSkeletalMeshLODRenderData& LODModel = SkeletalMesh->GetResourceForRendering()->LODRenderData[MeshLODIndex];
		const mu::FMeshBufferSet& MutableMeshVertexBuffers = MutableMesh->GetVertexBuffers();
		const int32 NumVertices = MutableMeshVertexBuffers.GetElementCount();

		MUTABLE_CPUPROFILER_SCOPE(UpdateSkeletalMesh_SurfaceLoop_MemCpy);

		const ESkeletalMeshVertexFlags BuildFlags = SkeletalMesh->GetVertexBufferFlags();
		const bool bUseFullPrecisionUVs = EnumHasAllFlags(BuildFlags, ESkeletalMeshVertexFlags::UseFullPrecisionUVs);
		const bool bHasVertexColors = EnumHasAllFlags(BuildFlags, ESkeletalMeshVertexFlags::HasVertexColors);
		const int NumTexCoords = MutableMeshVertexBuffers.GetBufferChannelCount(MUTABLE_VERTEXBUFFER_TEXCOORDS);

		const bool bNeedsCPUAccess = SkeletalMesh->GetLODInfoArray()[MeshLODIndex].bAllowCPUAccess;

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

		if (BoneIndexFormat == mu::MBF_UINT16)
		{
			LODModel.SkinWeightVertexBuffer.SetUse16BitBoneIndex(true);
		}

		if (BoneWeightFormat == mu::MBF_NUINT16)
		{
			LODModel.SkinWeightVertexBuffer.SetUse16BitBoneWeight(true);
		}

		// Init skin weight buffer
		FSkinWeightVertexBuffer_InitWithMutableData(
			LODModel.SkinWeightVertexBuffer,
			NumVertices,
			NumBoneInfluences * NumVertices,
			NumBoneInfluences,
			bNeedsCPUAccess,
			MutableMeshVertexBuffers.GetBufferData(BoneIndexBuffer),
			MutableMeshVertexBuffers.GetBufferDataSize(BoneIndexBuffer)
		);

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

					check(FGPUBaseSkinVertexFactory::UseUnlimitedBoneInfluences(BonesPerVertex) ||
						!LODModel.SkinWeightVertexBuffer.GetVariableBonesPerVertex());
					FSkinWeightVertexBuffer_InitWithMutableData(
						LODModel.SkinWeightVertexBuffer,
						NumVertices,
						NumBones,
						NumBoneInfluences,
						bNeedsCPUAccess,
						MutableMeshVertexBuffers.GetBufferData(Buffer),
						MutableMeshVertexBuffers.GetBufferDataSize(Buffer)
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
		MUTABLE_CPUPROFILER_SCOPE(CopyMutableIndexBuffers);

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


	struct FAuxMutableSkinWeightVertexKey
	{
		FAuxMutableSkinWeightVertexKey(const uint8* InKey, uint8 InKeySize, uint32 InHash)
			: Key(InKey), KeySize(InKeySize), Hash(InHash)
		{
		};

		const uint8* Key;
		uint8 KeySize;
		uint32 Hash;

		friend uint32 GetTypeHash(const FAuxMutableSkinWeightVertexKey& InKey)
		{
			return InKey.Hash;
		}

		bool operator==(const FAuxMutableSkinWeightVertexKey& o) const
		{
			return FMemory::Memcmp(o.Key, Key, KeySize) == 0;
		}
	};

	
	void CopyMutableSkinWeightProfilesBuffers(
		FSkeletalMeshLODRenderData& LODModel,
		const FName ProfileName,
		const mu::FMeshBufferSet& MutableMeshVertexBuffers,
		const int32 BoneIndexBuffer)
	{
		MUTABLE_CPUPROFILER_SCOPE(CopyMutableSkinWeightProfilesBuffers);

		// Basic Buffer override settings
		FRuntimeSkinWeightProfileData& Override = LODModel.SkinWeightProfilesData.AddOverrideData(ProfileName);

		const uint8 NumInfluences = LODModel.GetVertexBufferMaxBoneInfluences();
		Override.NumWeightsPerVertex = NumInfluences;

		const bool b16BitBoneIndices = LODModel.DoesVertexBufferUse16BitBoneIndex();
		Override.b16BitBoneIndices = b16BitBoneIndices;
		const int32 BoneIndexSize = b16BitBoneIndices ? 2 : 1;

		// BoneWeights channel info
		mu::MESH_BUFFER_FORMAT Format;
		int32 MutableNumInfluences;
		int32 Offset;
		MutableMeshVertexBuffers.GetChannel(BoneIndexBuffer, 2, nullptr, nullptr, &Format, &MutableNumInfluences, &Offset);

		const uint8 MutBoneIndexSize = Format == mu::MBF_UINT16 ? 2 : 1;
		const uint8 MutBoneIndicesSize = Offset - sizeof(int32);
		const uint8 MutBoneWeightsSize = MutableNumInfluences;
		const uint8 MutBoneWeightVertexSize = MutBoneIndicesSize + MutBoneWeightsSize;

		check(BoneIndexSize == MutBoneIndexSize);

		const int32 ElementCount = MutableMeshVertexBuffers.GetElementCount();
		Override.BoneIDs.Reserve(ElementCount * MutBoneIndicesSize);
		Override.BoneWeights.Reserve(ElementCount * NumInfluences);
		Override.VertexIndexToInfluenceOffset.Reserve(ElementCount);

		TMap<FAuxMutableSkinWeightVertexKey, int32> HashToUniqueWeightIndexMap;
		HashToUniqueWeightIndexMap.Reserve(ElementCount);
		int32 UniqueWeightsCount = 0;

		const uint8* SkinWeightsBuffer = reinterpret_cast<const uint8*>(MutableMeshVertexBuffers.GetBufferData(BoneIndexBuffer));
		for (int32 ElementIndex = 0; ElementIndex < ElementCount; ++ElementIndex)
		{
			uint32 ElementHash;
			FMemory::Memcpy(&ElementHash, SkinWeightsBuffer, sizeof(int32));
			SkinWeightsBuffer += sizeof(int32);

			if (ElementHash > 0)
			{
				if (int32* OverrideIndex = HashToUniqueWeightIndexMap.Find({ SkinWeightsBuffer, MutBoneWeightVertexSize, ElementHash }))
				{
					Override.VertexIndexToInfluenceOffset.Add(ElementIndex, *OverrideIndex);
					SkinWeightsBuffer += MutBoneWeightVertexSize;
				}
				else
				{
					Override.VertexIndexToInfluenceOffset.Add(ElementIndex, UniqueWeightsCount);
					HashToUniqueWeightIndexMap.Add({ SkinWeightsBuffer, MutBoneWeightVertexSize, ElementHash }, UniqueWeightsCount);

					Override.BoneIDs.SetNumUninitialized((UniqueWeightsCount + 1) * MutBoneIndicesSize, false);
					FMemory::Memcpy(&Override.BoneIDs[UniqueWeightsCount * MutBoneIndicesSize], SkinWeightsBuffer, MutBoneIndicesSize);
					SkinWeightsBuffer += MutBoneIndicesSize;

					Override.BoneWeights.SetNumUninitialized((UniqueWeightsCount + 1) * MutBoneWeightsSize, false);
					FMemory::Memcpy(&Override.BoneWeights[UniqueWeightsCount * MutBoneWeightsSize], SkinWeightsBuffer, MutBoneWeightsSize);
					SkinWeightsBuffer += MutBoneWeightsSize;
					++UniqueWeightsCount;
				}
			}
			else
			{
				SkinWeightsBuffer += MutBoneWeightVertexSize;
			}
		}

		Override.BoneIDs.Shrink();
		Override.BoneWeights.Shrink();
		Override.VertexIndexToInfluenceOffset.Shrink();
	}

	
	 void CopySkeletalMeshLODRenderData(const USkeletalMesh* SrcSkeletalMesh,	USkeletalMesh* DestSkeletalMesh, int32 SrcLODIndex, int32 DestLODIndex)
	 {
		 MUTABLE_CPUPROFILER_SCOPE(CopySkeletalMeshLODRenderData);

		 if (!SrcSkeletalMesh || !DestSkeletalMesh)
		 {
			 return;
		 }

		 if (!SrcSkeletalMesh->IsValidLODIndex(SrcLODIndex) || !DestSkeletalMesh->IsValidLODIndex(DestLODIndex))
		 {
			 // Trying to copy from/to an invalid LOD index
			 check(false);
			 return;
		 }

		 const FSkeletalMeshLODRenderData& SrcLODModel = SrcSkeletalMesh->GetResourceForRendering()->LODRenderData[SrcLODIndex];
		 FSkeletalMeshLODRenderData& DestLODModel = DestSkeletalMesh->GetResourceForRendering()->LODRenderData[DestLODIndex];

		 // Copying render sections
		 {
			 const FSkeletalMeshLODInfo& SrcLODInfo = SrcSkeletalMesh->GetLODInfoArray()[SrcLODIndex];
			 FSkeletalMeshLODInfo& DestLODInfo = DestSkeletalMesh->GetLODInfoArray()[DestLODIndex];

			 DestLODInfo.LODMaterialMap = SrcLODInfo.LODMaterialMap;

			 const int32 SurfaceCount = SrcLODModel.RenderSections.Num();
			 for (int32 SurfaceIndex = 0; SurfaceIndex < SurfaceCount; ++SurfaceIndex)
			 {
				 const FSkelMeshRenderSection& SrcSection = SrcSkeletalMesh->GetResourceForRendering()->LODRenderData[SrcLODIndex].RenderSections[SurfaceIndex];
				 FSkelMeshRenderSection* DestSection = new(DestSkeletalMesh->GetResourceForRendering()->LODRenderData[DestLODIndex].RenderSections) FSkelMeshRenderSection();

				 DestSection->DuplicatedVerticesBuffer.Init(1, TMap<int, TArray<int32>>());
				 DestSection->bDisabled = SrcSection.bDisabled;

				 if (!DestSection->bDisabled)
				 {
					 DestSection->BaseIndex = SrcSection.BaseIndex;
					 DestSection->NumTriangles = SrcSection.NumTriangles;
					 DestSection->BaseVertexIndex = SrcSection.BaseVertexIndex;
					 DestSection->MaxBoneInfluences = SrcSection.MaxBoneInfluences;
					 DestSection->NumVertices = SrcSection.NumVertices;
					 DestSection->BoneMap = SrcSection.BoneMap;
				 }
			 }
		 }

		 const FStaticMeshVertexBuffers& SrcStaticVertexBuffer = SrcLODModel.StaticVertexBuffers;
		 FStaticMeshVertexBuffers& DestStaticVertexBuffer = DestLODModel.StaticVertexBuffers;

		 const int32 NumVertices = SrcStaticVertexBuffer.PositionVertexBuffer.GetNumVertices();
		 const int32 NumTexCoords = SrcStaticVertexBuffer.StaticMeshVertexBuffer.GetNumTexCoords();

		 bool bAllowCPUAccess = SrcSkeletalMesh->GetLODInfoArray()[SrcLODIndex].bAllowCPUAccess;

		 // Copying Static Vertex Buffers
		 {
			 // Position buffer
			 DestStaticVertexBuffer.PositionVertexBuffer.Init(NumVertices, bAllowCPUAccess);
			 FMemory::Memcpy(DestStaticVertexBuffer.PositionVertexBuffer.GetVertexData(), SrcStaticVertexBuffer.PositionVertexBuffer.GetVertexData(), NumVertices * DestStaticVertexBuffer.PositionVertexBuffer.GetStride());

			 // Tangent and Texture coords buffers
			 DestStaticVertexBuffer.StaticMeshVertexBuffer.SetUseFullPrecisionUVs(true);
			 DestStaticVertexBuffer.StaticMeshVertexBuffer.SetUseHighPrecisionTangentBasis(false);
			 DestStaticVertexBuffer.StaticMeshVertexBuffer.Init(NumVertices, NumTexCoords, bAllowCPUAccess);
			 FMemory::Memcpy(DestStaticVertexBuffer.StaticMeshVertexBuffer.GetTangentData(), SrcStaticVertexBuffer.StaticMeshVertexBuffer.GetTangentData(), DestStaticVertexBuffer.StaticMeshVertexBuffer.GetTangentSize());
			 FMemory::Memcpy(DestStaticVertexBuffer.StaticMeshVertexBuffer.GetTexCoordData(), SrcStaticVertexBuffer.StaticMeshVertexBuffer.GetTexCoordData(), DestStaticVertexBuffer.StaticMeshVertexBuffer.GetTexCoordSize());

			 // Color buffer
			 if (SrcSkeletalMesh->GetHasVertexColors())
			 {
				 DestSkeletalMesh->SetHasVertexColors(true);

				 DestStaticVertexBuffer.ColorVertexBuffer.Init(NumVertices);
				 FMemory::Memcpy(DestStaticVertexBuffer.ColorVertexBuffer.GetVertexData(), SrcStaticVertexBuffer.ColorVertexBuffer.GetVertexData(), NumVertices * DestStaticVertexBuffer.ColorVertexBuffer.GetStride());
			 }
		 }

		 // Copying Skin Buffers
		 {
			 const FSkinWeightVertexBuffer& SrcSkinWeightBuffer = SrcLODModel.SkinWeightVertexBuffer;
			 FSkinWeightVertexBuffer& DestSkinWeightBuffer = DestLODModel.SkinWeightVertexBuffer;

			 int32 NumBoneInfluences = SrcSkinWeightBuffer.GetDataVertexBuffer()->GetMaxBoneInfluences();
			 int32 NumBones = SrcSkinWeightBuffer.GetDataVertexBuffer()->GetNumBoneWeights();

			 DestSkinWeightBuffer.SetUse16BitBoneIndex(SrcSkinWeightBuffer.Use16BitBoneIndex());
			 FSkinWeightDataVertexBuffer* SkinWeightDataVertexBuffer = DestSkinWeightBuffer.GetDataVertexBuffer();
			 SkinWeightDataVertexBuffer->SetMaxBoneInfluences(NumBoneInfluences);
			 SkinWeightDataVertexBuffer->Init(NumBones, NumVertices);

			 if (NumVertices)
			 {
				 DestSkinWeightBuffer.SetNeedsCPUAccess(bAllowCPUAccess);

				 const void* SrcData = SrcSkinWeightBuffer.GetDataVertexBuffer()->GetWeightData();
				 void* Data = SkinWeightDataVertexBuffer->GetWeightData();
				 check(SrcData);
				 check(Data);

				 FMemory::Memcpy(Data, SrcData, DestSkinWeightBuffer.GetVertexDataSize());
			 }
		 }

		 // Copying Skin Weight Profiles Buffers
		 {
			 int32 NumSkinWeightProfiles = SrcSkeletalMesh->GetSkinWeightProfiles().Num();
			 for (int32 ProfileIndex = 0; ProfileIndex < NumSkinWeightProfiles; ++ProfileIndex)
			 {
				 DestSkeletalMesh->AddSkinWeightProfile(SrcSkeletalMesh->GetSkinWeightProfiles()[ProfileIndex]);

				 const FRuntimeSkinWeightProfileData* SrcProfile = SrcLODModel.SkinWeightProfilesData.GetOverrideData(SrcSkeletalMesh->GetSkinWeightProfiles()[ProfileIndex].Name);
				 FRuntimeSkinWeightProfileData& DestProfile = DestLODModel.SkinWeightProfilesData.AddOverrideData(SrcSkeletalMesh->GetSkinWeightProfiles()[ProfileIndex].Name);

				 DestProfile.NumWeightsPerVertex = SrcProfile->NumWeightsPerVertex;
				 DestProfile.b16BitBoneIndices = SrcProfile->b16BitBoneIndices;

				 DestProfile.BoneIDs = SrcProfile->BoneIDs;
				 DestProfile.BoneWeights = SrcProfile->BoneWeights;
				 DestProfile.VertexIndexToInfluenceOffset = SrcProfile->VertexIndexToInfluenceOffset;
			 }
		 }

		 // Copying Indices
		 {
			 if (SrcLODModel.MultiSizeIndexContainer.GetIndexBuffer())
			 {
				 int32 IndexCount = SrcLODModel.MultiSizeIndexContainer.GetIndexBuffer()->Num();
				 int32 ElementSize = SrcLODModel.MultiSizeIndexContainer.GetDataTypeSize();

				 const void* Data = SrcSkeletalMesh->GetResourceForRendering()->LODRenderData[SrcLODIndex].MultiSizeIndexContainer.GetIndexBuffer()->GetPointerTo(0);

				 DestLODModel.MultiSizeIndexContainer.CreateIndexBuffer(ElementSize);
				 DestLODModel.MultiSizeIndexContainer.GetIndexBuffer()->Insert(0, IndexCount);
				 FMemory::Memcpy(DestLODModel.MultiSizeIndexContainer.GetIndexBuffer()->GetPointerTo(0), Data, IndexCount * ElementSize);
			 }
		 }

		 DestLODModel.ActiveBoneIndices.Append(SrcLODModel.ActiveBoneIndices);
		 DestLODModel.RequiredBones.Append(SrcLODModel.RequiredBones);
		 DestLODModel.bIsLODOptional = SrcLODModel.bIsLODOptional;
		 DestLODModel.bStreamedDataInlined = SrcLODModel.bStreamedDataInlined;
		 DestLODModel.BuffersSize = SrcLODModel.BuffersSize;
	}
}
