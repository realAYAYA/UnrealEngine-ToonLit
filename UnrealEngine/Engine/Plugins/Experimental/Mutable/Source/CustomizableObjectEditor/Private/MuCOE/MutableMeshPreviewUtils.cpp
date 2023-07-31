// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/MutableMeshPreviewUtils.h"

#include "Animation/Skeleton.h"
#include "Containers/Array.h"
#include "Containers/IndirectArray.h"
#include "Containers/Map.h"
#include "Containers/SparseArray.h"
#include "CoreGlobals.h"
#include "Engine/EngineTypes.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/PlatformCrt.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Math/BoxSphereBounds.h"
#include "Math/Matrix.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/AssertionMacros.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/MutableMeshBufferUtils.h"
#include "MuCO/UnrealConversionUtils.h"
#include "MuCO/UnrealPortabilityHelpers.h"
#include "MuR/Mesh.h"
#include "MuR/MeshBufferSet.h"
#include "MuR/MutableTrace.h"
#include "MuR/OpMeshFormat.h"
#include "MuR/Parameters.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Skeleton.h"
#include "PerPlatformProperties.h"
#include "ReferenceSkeleton.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Trace/Detail/Channel.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectPtr.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealNames.h"

class UMaterialInterface;


namespace MutableMeshPreviewUtils
{
	namespace
	{
		/** It generates a copy of the provided constant mesh but making sure the buffer structure is the one
		 * unreal and our systems want. This is because some of the meshes we provide to mutable do not all have the
		 * same structure.
		 * @param InMutableMesh Mutable mesh whose buffers should be formatted following the desired buffer structure.
		 * @return An identical mutable mesh to the provided one in terms of data but with the buffer structure that UE
		 * requires
		 */
		mu::MeshPtrConst GenerateCompatibleMutableMesh(mu::MeshPtrConst InMutableMesh)
		{
			// 1) Generate a mutable mesh to be the one hosting the buffer format we desire (it will be required for 
			mu::MeshPtr FormattedMutableMesh = new::mu::Mesh();
			mu::FMeshBufferSet& FormattedVertexBuffers = FormattedMutableMesh->GetVertexBuffers();
			mu::FMeshBufferSet& FormattedIndexBuffers = FormattedMutableMesh->GetIndexBuffers();
			
			// 2) Generate the buffers for that mesh with the format unreal expects
			{
				// 2.1)		Locate what buffers are present on the mutable constant mesh
				const mu::FMeshBufferSet& OriginVertexBuffers = InMutableMesh->GetVertexBuffers();

				// Data extracted from the original buffers to be used later when setting up the formatted mesh
				bool bOriginDoesHaveVertexColorData = false;
				int32 NumbOfTextCoordChannels = 0;
				int32 MaxNumBonesPerVertex = 0;
				int32 BoneIndicesSizeBytes = 0;
				int32 BoneWeightsSizeBytes = 0;

				// Contingency to avoid mismatches between the ideal semantic indices on the provided mutable mesh against the
				// ones that should be .
				int TextureSemanticIndices[MutableMeshBufferUtils::MaxTexCordChannelCount] = {-1,-1,-1,-1};
				
				// Vertex buffers
				const int32 MutableMeshBuffersCount = OriginVertexBuffers.GetBufferCount();
				for (int32 BufferIndex = 0; BufferIndex < MutableMeshBuffersCount; BufferIndex++)
				{
					const int32 BufferChannelCount = OriginVertexBuffers.GetBufferChannelCount(BufferIndex);
					for (int32 ChannelIndex = 0; ChannelIndex < BufferChannelCount; ChannelIndex ++)
					{
						mu::MESH_BUFFER_SEMANTIC BufferSemantic = mu::MESH_BUFFER_SEMANTIC::MBS_NONE;
						mu::MESH_BUFFER_FORMAT BufferFormat = mu::MESH_BUFFER_FORMAT::MBF_NONE;
						int32 BufferComponentCount = 0;
						int32 SemanticIndex = 0;

						// Get the data from mutable that we require from the selected buffer set buffer
						OriginVertexBuffers.GetChannel
						(
							BufferIndex,
							ChannelIndex,
							&(BufferSemantic),
							&(SemanticIndex),
							&(BufferFormat),
							&(BufferComponentCount),
							nullptr
						);

						switch (BufferSemantic)
						{
							case mu::MESH_BUFFER_SEMANTIC::MBS_COLOUR:
							{
								// Does the original mesh have vert color data?
								bOriginDoesHaveVertexColorData = true;
								break;
							}

							case mu::MESH_BUFFER_SEMANTIC::MBS_TEXCOORDS:
							{
								// Store the texture semantic index to be later used during the buffer formatting process on  mu::MeshFormat
								TextureSemanticIndices[NumbOfTextCoordChannels] = SemanticIndex;
									
								// Store the amount of located texture data channels
								NumbOfTextCoordChannels++;
								break;
							}

							case mu::MESH_BUFFER_SEMANTIC::MBS_BONEINDICES:
							{
								// Store the amount of bones a vertex can be skinned to.
								MaxNumBonesPerVertex = BufferComponentCount;
								BoneIndicesSizeBytes = BufferFormat == mu::MESH_BUFFER_FORMAT::MBF_INT16 ? 2 : 1;
								break;
							}

							case mu::MESH_BUFFER_SEMANTIC::MBS_BONEWEIGHTS:
							{
								// Store the amount of bones a vertex can be skinned to.
								BoneWeightsSizeBytes = BufferFormat == mu::MESH_BUFFER_FORMAT::MBF_NUINT16 ? 2 : 1;
								break;
							}

							default:
							{
								break;
							}
						}
					}
				}
				
				// 2.2)		Generate the mesh buffer Sets so they follow the expected structure.

				// Setup the buffers to later setup the channels each of them have
				int32 MutableBufferCount = MUTABLE_VERTEXBUFFER_TEXCOORDS + 1;
				{
					// Bone Indices
					{
						// They are mandatory when generating a new skeletal mesh
						MutableBufferCount++;
					}
					
					// (Optional) Vertex colors
					if (bOriginDoesHaveVertexColorData)
					{
						MutableBufferCount++;
					}
				}
				FormattedVertexBuffers.SetBufferCount(MutableBufferCount);

				// Setup the channels of each buffer

				int32 CurrentVertexBuffer = 0;
				{
					// Vertex buffer (positions)
					MutableMeshBufferUtils::SetupVertexPositionsBuffer(CurrentVertexBuffer,FormattedVertexBuffers);
					CurrentVertexBuffer++;

					// Tangent buffer
					MutableMeshBufferUtils::SetupTangentBuffer(CurrentVertexBuffer,FormattedVertexBuffers);
					CurrentVertexBuffer++;
				
					// Texture coords buffer
					MutableMeshBufferUtils::SetupTexCoordinatesBuffer(CurrentVertexBuffer,NumbOfTextCoordChannels,FormattedVertexBuffers, TextureSemanticIndices);
					CurrentVertexBuffer++;
				
					// Skin buffer
					MutableMeshBufferUtils::SetupSkinBuffer(CurrentVertexBuffer, BoneIndicesSizeBytes, BoneWeightsSizeBytes, MaxNumBonesPerVertex, FormattedVertexBuffers);
					CurrentVertexBuffer++;

					// Colour buffer
					if (bOriginDoesHaveVertexColorData)
					{
						MutableMeshBufferUtils::SetupVertexColorBuffer(CurrentVertexBuffer,FormattedVertexBuffers);
						CurrentVertexBuffer++;
					}

					// Index buffer
					MutableMeshBufferUtils::SetupIndexBuffer(FormattedIndexBuffers);
				}
			}
			
			// At this point a new mutable mesh with no data but with the appropriate structure for UE to be able to convert it to an
			// Skeletal mesh is ready to be used as an argument for the process of reformatting the Mutable Mesh we want to preview

			// 3) Call mu::MeshFormat to get the mesh with the wanted buffer structure to have the data of the original mesh
			mu::MeshPtrConst ResultMutableMesh = mu::MeshFormat(
				InMutableMesh.get(),
				FormattedMutableMesh.get(),
				false,
				true,
				true,
				false,
				false );
			
			return ResultMutableMesh;
		}

		/** Method designed to scan the vertex buffers of the provided mutable mesh and tell the caller if the buffers are
		 * set up following what UE requires. 
		 * @param InMutableMesh The Mutable Mesh that is wanted to be checked.
		 * @param bLogFindingsToConsole If true the console will display not only if the buffers were found but also if they are
		 * badly located inside the buffer set.
		 * @return True if the buffers are where they should be, false if not correctly placed or missing.
		 */
		bool AreVertexBuffersCorrectlyLocated(const mu::MeshPtrConst InMutableMesh, bool bLogFindingsToConsole = false )
		{
			bool bCanBeConverted = true;

			// At the moment requires the presence of all "mandatory" buffers. Only meshes designed to be displayed by unreal
			// will therefore be processable at the moment and not always
			const mu::FMeshBufferSet& VertexBuffers = InMutableMesh->GetVertexBuffers();

			int32 Buffer = -1;
			int32 Channel = -1;

			// POSITION BUFFER INDEX CHECK
			VertexBuffers.FindChannel(mu::MESH_BUFFER_SEMANTIC::MBS_POSITION, 0, &Buffer, &Channel);
			if (Buffer == -1)
			{
				if (bLogFindingsToConsole)
				{
					UE_LOG(LogMutable, Warning, TEXT("\tVertex Positions Buffer not found."));
				}

				bCanBeConverted = false;
			}
			else if (Buffer != MUTABLE_VERTEXBUFFER_POSITION)
			{
				if (bLogFindingsToConsole)
				{
					UE_LOG(LogMutable, Warning,
					       TEXT(
						       "\tBad Vertex Positions buffer position ( at %d instead of %d )."
					       ), Buffer, MUTABLE_VERTEXBUFFER_POSITION)
				}

				bCanBeConverted = false;
			}

			// TANGENT BUFFER INDEX CHECK
			VertexBuffers.FindChannel(mu::MESH_BUFFER_SEMANTIC::MBS_TANGENT, 0, &Buffer, &Channel);
			if (Buffer == -1)
			{
				if (bLogFindingsToConsole)
				{
					UE_LOG(LogMutable, Warning, TEXT("\tTangents Buffer not found."));
				}

				bCanBeConverted = false;
			}
			else if (Buffer != MUTABLE_VERTEXBUFFER_TANGENT)
			{
				if (bLogFindingsToConsole)
				{
					UE_LOG(LogMutable, Warning,
					       TEXT(
						       "\tBad Tangent buffer position (  at %d instead of %d ). "
					       ), Buffer, MUTABLE_VERTEXBUFFER_TANGENT);
				}

				bCanBeConverted = false;
			}

			// TEX COORDS BUFFER INDEX CHECK
			VertexBuffers.FindChannel(mu::MESH_BUFFER_SEMANTIC::MBS_TEXCOORDS, 0, &Buffer, &Channel);
			if (Buffer == -1)
			{
				if (bLogFindingsToConsole)
				{
					UE_LOG(LogMutable, Warning, TEXT("\tTexCords Buffer not found."));
				}

				bCanBeConverted = false;
			}
			else if (Buffer != MUTABLE_VERTEXBUFFER_TEXCOORDS)
			{
				if (bLogFindingsToConsole)
				{
					UE_LOG(LogMutable, Warning,
					       TEXT(
						       "\tBad TexCoords buffer position ( at %d instead of %d ). "
					       ), Buffer, MUTABLE_VERTEXBUFFER_TEXCOORDS);
				}

				bCanBeConverted = false;
			}
			
			return bCanBeConverted;
		}

		/** Loads up the OutSkeletalMesh with all the bone data found on the InMutableMesh if found to be a skeleton there. If not it will fail
		 * @param InMutableMesh - The mutable mesh to read bone data from
		 * @param OutFinalBoneNames - An array of names containing the names of the bones to be used on the skeletal mesh
		 * @param OutBoneMap - Array with all the bones used on the final mesh
		 */ 
		void PrepareSkeletonData(mu::MeshPtrConst InMutableMesh, TArray<FName>& OutFinalBoneNames, TArray<uint16>& OutBoneMap)
		{
			if (!InMutableMesh)
			{
				return;
			}
			
			// Helpers
			TMap<FName, int16> BoneNameToIndex;
			TArray<int16> ParentBoneIndices;
			TArray<int16> ParentChain;
			TArray<bool> UsedBones;
			

			// Use first valid LOD bone count as a potential total number of bones, used for pre-allocating data arrays
			if ( InMutableMesh && InMutableMesh->GetSkeleton())
			{
				const int32 TotalPossibleBones = InMutableMesh->GetSkeleton()->GetBoneCount();
				
				// Helpers
				BoneNameToIndex.Empty(TotalPossibleBones);
				ParentBoneIndices.Empty(TotalPossibleBones);

				ParentChain.SetNumZeroed(TotalPossibleBones);

				// Out Data
				OutFinalBoneNames.Reserve(TotalPossibleBones);
			}

			// Locate what bones are being used by asking each vertex with what bone index it is related with.
			// We end up having an array of Bool variables where each index corresponds to a bone on the mutable skeleton
			// and the value determines if the bone is found to be used by any of the vertices located on the
			// mutable mesh surfaces
			{
				if (!InMutableMesh || !InMutableMesh->GetSkeleton())
				{
					return;
				}

				const mu::Skeleton* Skeleton = InMutableMesh->GetSkeleton().get();
				const int32 BoneCount = Skeleton->GetBoneCount();

				const mu::FMeshBufferSet& MutableMeshVertexBuffers = InMutableMesh->GetVertexBuffers();

				const int32 NumVerticesLODModel = InMutableMesh->GetVertexCount();
				const int32 SurfaceCount = InMutableMesh->GetSurfaceCount();

				mu::MESH_BUFFER_FORMAT BoneIndexFormat = mu::MBF_NONE;
				int32 BoneIndexComponents = 0;
				int32 BoneIndexOffset = 0;
				int32 BoneIndexBuffer = -1;
				int32 BoneIndexChannel = -1;
				MutableMeshVertexBuffers.FindChannel(mu::MBS_BONEINDICES, 0, &BoneIndexBuffer, &BoneIndexChannel);
				if (BoneIndexBuffer >= 0 || BoneIndexChannel >= 0)
				{
					MutableMeshVertexBuffers.GetChannel(BoneIndexBuffer, BoneIndexChannel,
						nullptr, nullptr, &BoneIndexFormat, &BoneIndexComponents, &BoneIndexOffset);
				}

				const int32 ElementSize = MutableMeshVertexBuffers.GetElementSize(BoneIndexBuffer);

				const uint8_t* BufferStart = MutableMeshVertexBuffers.GetBufferData(BoneIndexBuffer) + BoneIndexOffset;

				const int NumBoneInfluences = BoneIndexComponents;
				{
					UsedBones.Empty(BoneCount);
					UsedBones.AddDefaulted(BoneCount);

					for (int32 Surface = 0; Surface < SurfaceCount; Surface++)
					{
						int32 FirstIndex;
						int32 IndexCount;
						int32 FirstVertex;
						int32 VertexCount;
						InMutableMesh->GetSurface(Surface, &FirstVertex, &VertexCount, &FirstIndex, &IndexCount);

						if (VertexCount == 0 || IndexCount == 0)
						{
							continue;
						}

						const uint8_t* VertexBoneIndexPtr = BufferStart + FirstVertex * ElementSize;

						if (BoneIndexFormat == mu::MBF_UINT8)
						{
							for (int32 v = 0; v < VertexCount; ++v)
							{
								for (int32 i = 0; i < BoneIndexComponents; ++i)
								{
									const size_t SectionBoneIndex = VertexBoneIndexPtr[i];
									UsedBones[SectionBoneIndex] = true;
								}
								VertexBoneIndexPtr += ElementSize;
							}
						}
						else if (BoneIndexFormat == mu::MBF_UINT16)
						{
							for (int32 v = 0; v < VertexCount; ++v)
							{
								for (int32 i = 0; i < BoneIndexComponents; ++i)
								{
									const size_t SectionBoneIndex = ((uint16*)VertexBoneIndexPtr)[i];
									UsedBones[SectionBoneIndex] = true;
								}
								VertexBoneIndexPtr += ElementSize;
							}
						}
						else if (BoneIndexFormat == mu::MBF_UINT32)
						{
							for (int32 v = 0; v < VertexCount; ++v)
							{
								for (int32 i = 0; i < BoneIndexComponents; ++i)
								{
									const size_t SectionBoneIndex = ((uint32_t*)VertexBoneIndexPtr)[i];
									UsedBones[SectionBoneIndex] = true;
								}
								VertexBoneIndexPtr += ElementSize;
							}
						}
						else
						{
							// Unsupported bone index format in generated mutable mesh
							check(false);
						}

					}
				}

				// Fill the BoneMap with the data found on the mutable skeleton and with the aid of the
				// bool array with all the bones we are using in this mesh from all the ones found on the mu::Skeleton
				{
					OutBoneMap.Reserve(BoneCount);

					for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
					{
						if (!UsedBones[BoneIndex])
						{
							// Add root as a placeholder
							OutBoneMap.Add(0);
							continue;
						}

						FName BoneName = Skeleton->GetBoneName(BoneIndex);

						int16* FinalBoneIndexPtr = BoneNameToIndex.Find(BoneName);
						if (!FinalBoneIndexPtr) // New bone!
						{
							// Ensure parent chain is valid
							int16 FinalParentIndex = INDEX_NONE;

							const int16 MutParentIndex = Skeleton->GetBoneParent(BoneIndex);
							if (MutParentIndex != INDEX_NONE) // Bone has a parent, ensure the parent exists
							{
								// Ensure parent chain until root exists
								uint16 ParentChainCount = 0;

								// Find parents to add
								int16 NextMutParentIndex = MutParentIndex;
								while (NextMutParentIndex != INDEX_NONE)
								{
									if (int16* Found = BoneNameToIndex.Find(Skeleton->GetBoneName(NextMutParentIndex)))
									{
										FinalParentIndex = *Found;
										break;
									}

									ParentChain[ParentChainCount] = NextMutParentIndex;
									++ParentChainCount;

									NextMutParentIndex = Skeleton->GetBoneParent(NextMutParentIndex);
								}

								// Add parent bones to the list and to the active bones array
								for (int16 ParentChainIndex = ParentChainCount - 1; ParentChainIndex >= 0; --ParentChainIndex)
								{
									FName ChainParentName = Skeleton->GetBoneName(ParentChain[ParentChainIndex]);

									// Set the parent for the bone we're about to add (previous ParentIndex)
									ParentBoneIndices.Add(FinalParentIndex);

									// Add the parent
									FinalParentIndex = OutFinalBoneNames.Num();
									BoneNameToIndex.Add(ChainParentName, FinalParentIndex);
									OutFinalBoneNames.Add(ChainParentName);
								}
							}

							const int16 NewBoneIndex = OutFinalBoneNames.Num();
							OutBoneMap.Add(NewBoneIndex);

							// Add new bone to the ReferenceSkeleton
							BoneNameToIndex.Add(BoneName, NewBoneIndex);
							OutFinalBoneNames.Add(BoneName);
							ParentBoneIndices.Add(FinalParentIndex);
						}
						else
						{
							const int16 FinalBoneIndex = *FinalBoneIndexPtr;
							OutBoneMap.Add(FinalBoneIndex);
						}
					}
				}

				OutBoneMap.Shrink();
			}

			OutFinalBoneNames.Shrink();
		}

		/** Provides the OutSkeletalMesh with a new reference skeleton based on the data found on the other method inputs
		 * @param InRefSkeletalMesh - The skeletal mesh to be used as reference for the copying of data not held by the mutable
		 * @param OutSkeletalMesh - The skeletal mesh whose reference skeleton we want to generate.
		 * @return True if the operation could be performed successfully, false if not.
		 */
		bool BuildSkeletalMeshSkeletonData(const USkeletalMesh* InRefSkeletalMesh, USkeletalMesh* OutSkeletalMesh)
		{
			if (!OutSkeletalMesh || !InRefSkeletalMesh)
			{
				return false;
			}

			// get a copy of the skeleton used by the reference skeletal mesh
			const USkeleton* OriginalSkeleton = InRefSkeletalMesh->GetSkeleton();
			if (!OriginalSkeleton)
			{
				return false;
			}
			
			TObjectPtr<USkeleton> Skeleton = DuplicateObject(OriginalSkeleton,OriginalSkeleton->GetExternalPackage()) ;
			OutSkeletalMesh->SetSkeleton(Skeleton);
			
			const FReferenceSkeleton& SourceReferenceSkeleton = Skeleton->GetReferenceSkeleton();
			const int32 SourceBoneCount = SourceReferenceSkeleton.GetRawBoneNum();
			
			// Used to add them in strictly increasing order
			TArray<bool> UsedBones;
			UsedBones.Init(true, SourceBoneCount);	

			// New reference skeleton
			FReferenceSkeleton RefSkeleton;
			
			// Build RefSkeleton
			UnrealConversionUtils::BuildRefSkeleton(
				nullptr,
				SourceReferenceSkeleton,
				UsedBones,
				RefSkeleton,
				Skeleton);

			OutSkeletalMesh->SetRefSkeleton(RefSkeleton);

			OutSkeletalMesh->GetRefBasesInvMatrix().Empty(RefSkeleton.GetRawBoneNum());
			OutSkeletalMesh->CalculateInvRefMatrices();

			return true;
		}

		/** Prepares the OutSkeletal mesh with the required materials taking in consideration the amount of surfaces found on
		 * the InMutableMesh. As it is working with a mutable mesh, and a mutable mesh only represents one lod, the target lod is 0
		 * @param InMutableMesh - The reference mutable mesh to be reading data from
		 * @param OutSkeletalMesh - The skeletal mesh to be getting it's materials set up.
		 */
		void BuildSkeletalMeshElementData(const mu::MeshPtrConst InMutableMesh,USkeletalMesh* OutSkeletalMesh)
		{
			// Set up Unreal's default material
			UMaterialInterface* UnrealMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
			OutSkeletalMesh->GetMaterials().SetNum(1);
			OutSkeletalMesh->GetMaterials()[0] = UnrealMaterial;

			constexpr int32 MeshLODIndex = 0;
			UnrealConversionUtils::BuildSkeletalMeshElementDataAtLOD(MeshLODIndex,InMutableMesh,OutSkeletalMesh);
		}


		/** Prepares the skeletal mesh to be able to be rendered. It should provide you with a mesh ready to be rendered
		 * @param InMutableMesh - The mutable mesh to be used as reference
		 * @param InRefSkeletalMesh - A reference skeletal mesh to aid on the preparation of the OutSkeletalMesh
		 * @param InBoneMap - An array with all the bones used by the mesh.
		 * @param OutSkeletalMesh - The skeletal mesh to set up.
		 * @return True if the operation could be performed successfully, false if not.
		 */
		bool BuildSkeletalMeshRenderData(
			const mu::MeshPtrConst InMutableMesh, const USkeletalMesh* InRefSkeletalMesh,
			const TArray<uint16>& InBoneMap, USkeletalMesh* OutSkeletalMesh)
		{
			OutSkeletalMesh->SetHasBeenSimplified(false);
			OutSkeletalMesh->SetHasVertexColors(false);

			// Load buffer data found on the mutable model onto the out skeletal mesh
			// It includes vertex and index buffers
			UnrealConversionUtils::SetupRenderSections(
				OutSkeletalMesh,
				InMutableMesh,
				0,
				InBoneMap);

			UnrealConversionUtils::CopyMutableVertexBuffers(
				OutSkeletalMesh,
				InMutableMesh,
				0); 
			
			FSkeletalMeshLODRenderData& LODModel = Helper_GetLODData(OutSkeletalMesh)[0];

			// Generate the active bones array based on the bones found on BoneMap
			TArray<uint16> ActiveBones;
			{
				ActiveBones.Reserve(InBoneMap.Num());
				for (const uint16& BoneMapIndex : InBoneMap)
				{
					ActiveBones.AddUnique(BoneMapIndex);
				}
				ActiveBones.Sort();
			}

			LODModel.ActiveBoneIndices.Append(ActiveBones);
			LODModel.RequiredBones.Append(ActiveBones);
			
			// Copy index buffers or fail to generate the mesh
			if (!UnrealConversionUtils::CopyMutableIndexBuffers(InMutableMesh, LODModel))
			{
				return false;
			}
			
			// Update LOD and streaming data
			LODModel.bIsLODOptional = false;
			LODModel.bStreamedDataInlined = false;

			if (InRefSkeletalMesh)
			{
				const FBoxSphereBounds Bounds = ((USkeletalMesh*)InRefSkeletalMesh)->GetBounds();
				OutSkeletalMesh->SetImportedBounds(Bounds);
			}
			
			return true;
		}
	}

	/*
	 * Publicly accessible functions of MutableMeshPreviewUtils
	 */

	USkeletalMesh* GenerateSkeletalMeshFromMutableMesh(mu::MeshPtrConst InMutableMesh,
		const USkeletalMesh* InReferenceSkeletalMesh)
	{
		if (!InMutableMesh ||
			!InReferenceSkeletalMesh)
		{
			return nullptr;
		}

		// Buffer structures checks and corrective operations
		{
			// If the semantic channels for the required buffer data are on not the buffer they are expected to be then
			// and only then format the mesh to follow what our systems and Unreal expect in terms of channel on buffer structure
			if (!AreVertexBuffersCorrectlyLocated(InMutableMesh))
			{
				UE_LOG(LogMutable, Warning,
				       TEXT(
					       "Restructuring Mutable Mesh Buffers:  "
				       ));
				
				// Since we know the mesh does not have the required buffer structure do format it to match our requirements
				InMutableMesh = GenerateCompatibleMutableMesh(InMutableMesh);

				// If the conversion was required then make sure all went well
				if (!AreVertexBuffersCorrectlyLocated(InMutableMesh,true))
				{
					// The formatting operation failed, do not proceed
					UE_LOG(LogMutable,Error,TEXT("Restructuring of mutable mesh buffers failed: Aborting Skeletal Mesh generation. "));
					return nullptr;
				}
				
				UE_LOG(LogMutable,Log,TEXT("Buffers Restructuring was succesfull."));
			}

			// We should now have a mutable mesh following a buffer structure where the required buffers are there to be
			// later found and used to generate a proper Skeletal Mesh
		}
		
		
		// Bone processed data containers
		TArray<uint16> BoneMap;
		TArray<FName> BoneNames;

		// Provided a mutable skeleton do load the BoneNames and BoneMap from there
		if (InMutableMesh->GetSkeleton())
		{
			// Prepare the skeleton data and get the bone names to be used on the generated mesh alongside with the indices of
			// such bones on the final mesh.
			PrepareSkeletonData(InMutableMesh,BoneNames,BoneMap);
		}
		else
		{
			// No mutable skeleton has been loaded.
			// Use the data already set on the reference skeletal mesh to do the work of PrepareSkeletonData
			// but without having access to the mutable skeleton

			// Add all reference bones set on the reference skeleton onto our new mesh.
			const FReferenceSkeleton& ReferenceMeshReferenceSkeleton = InReferenceSkeletalMesh->GetRefSkeleton();
			for	(int32 RefBone = 0; RefBone < ReferenceMeshReferenceSkeleton.GetRawBoneNum(); RefBone++)
			{
				BoneMap.Add(RefBone);
			}
		}
		
		// Generate a new skeletal mesh to be filled up with the data found on the mutable mesh and the reference skeletal mesh
		USkeletalMesh* GeneratedSkeletalMesh =
			NewObject<USkeletalMesh>(GetTransientPackage(), NAME_None, RF_Transient);

		// build the reference skeleton for the Generated Skeletal mesh
		bool bSuccess = BuildSkeletalMeshSkeletonData(InReferenceSkeletalMesh,GeneratedSkeletalMesh);
		if (!bSuccess)
		{
			return nullptr;
		}
		
		// Load the data present on the reference skeletal mesh that could not be grabbed from the mu::Mesh
		GeneratedSkeletalMesh->SetImportedBounds(InReferenceSkeletalMesh->GetBounds()); // TODO Generate Bounds
		GeneratedSkeletalMesh->SetPhysicsAsset(InReferenceSkeletalMesh->GetPhysicsAsset());
		GeneratedSkeletalMesh->SetEnablePerPolyCollision(InReferenceSkeletalMesh->GetEnablePerPolyCollision());

		GeneratedSkeletalMesh->AllocateResourceForRendering();

		// Prepare the mesh to be filled with rendering data (buffers)
		{
			Helper_GetLODData(GeneratedSkeletalMesh).Add(( Helper_GetLODData(GeneratedSkeletalMesh).Num()) ? &Helper_GetLODData(GeneratedSkeletalMesh)[0] : new Helper_LODDataType());
			Helper_GetLODInfoArray(GeneratedSkeletalMesh).Add(FSkeletalMeshLODInfo());

	#if WITH_EDITORONLY_DATA
			Helper_GetLODInfoArray(GeneratedSkeletalMesh).Last().BuildSettings.bUseFullPrecisionUVs = true;
	#endif

			constexpr int32 LODIndex = 0;
			if (Helper_GetLODInfoArray(InReferenceSkeletalMesh).IsValidIndex(LODIndex))
			{
				Helper_GetLODInfoArray(GeneratedSkeletalMesh).Last().ScreenSize = Helper_GetLODInfoArray(InReferenceSkeletalMesh)[LODIndex].ScreenSize;
				Helper_GetLODInfoArray(GeneratedSkeletalMesh).Last().LODHysteresis = Helper_GetLODInfoArray(InReferenceSkeletalMesh)[LODIndex].LODHysteresis;
				Helper_GetLODInfoArray(GeneratedSkeletalMesh).Last().bSupportUniformlyDistributedSampling = Helper_GetLODInfoArray(InReferenceSkeletalMesh)[LODIndex].bSupportUniformlyDistributedSampling;
				Helper_GetLODInfoArray(GeneratedSkeletalMesh).Last().bAllowCPUAccess = Helper_GetLODInfoArray(InReferenceSkeletalMesh)[LODIndex].bAllowCPUAccess;
			}
			else
			{
				Helper_GetLODInfoArray(GeneratedSkeletalMesh).Last().ScreenSize = 0.3f / (LODIndex + 1);
				Helper_GetLODInfoArray(GeneratedSkeletalMesh).Last().LODHysteresis = 0.02f;
				UE_LOG(LogMutable, Warning, TEXT("Setting default values for LOD ScreenSize and LODHysteresis because the values cannot be found in the reference mesh"));
			}
		}

		// Set the material data and initialize the sections that will be used later
		BuildSkeletalMeshElementData(InMutableMesh,GeneratedSkeletalMesh);
		
		// Load all data from the mutable mesh onto the skeletal mesh object
		bSuccess = BuildSkeletalMeshRenderData(InMutableMesh,InReferenceSkeletalMesh,BoneMap,GeneratedSkeletalMesh);
		if (!bSuccess)
		{
			return nullptr;
		}

		// Prepares the mesh to be rendered
		GeneratedSkeletalMesh->InitResources();
		
		return GeneratedSkeletalMesh;
	}

}









