// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/MutableMeshPreviewUtils.h"

#include "Animation/Skeleton.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/MutableMeshBufferUtils.h"
#include "MuCO/UnrealConversionUtils.h"
#include "MuCO/UnrealPortabilityHelpers.h"
#include "MuR/OpMeshFormat.h"
#include "Engine/SkeletalMesh.h"
#include "MuCO/CustomizableObject.h"
#include "UObject/Package.h"

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
			mu::Ptr<mu::Mesh> FormatResult = new mu::Mesh();

			bool bOutSuccess = false;
			mu::MeshFormat(FormatResult.get(), InMutableMesh.get(), FormattedMutableMesh.get(),
				false, true, true, false, false,
				bOutSuccess);
		
			return FormatResult;
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


		/** Provides the OutSkeletalMesh with a new skeleton
		 * @param OutSkeletalMesh - The skeletal mesh whose skeleton we want to generate.
		 * @return True if the operation could be performed successfully, false if not.
		 */
		bool BuildSkeletalMeshSkeletonData(mu::MeshPtrConst InMutableMesh, USkeletalMesh* OutSkeletalMesh)
		{
			if (!InMutableMesh || !OutSkeletalMesh)
			{
				return false;
			}
			
			// The skeleton will only have one bone, and all bon indices will be mapped to it.
			TObjectPtr<USkeleton> Skeleton = NewObject<USkeleton>();

			// Build new RefSkeleton	
			{
				// Scope is important, FReferenceSkeletonModifier will rebuild the reference skeleon on destroy
				FReferenceSkeletonModifier RefSkeletonModifier(Skeleton);
				RefSkeletonModifier.Add(FMeshBoneInfo(FName("Root"), FString("Root"), INDEX_NONE), FTransform::Identity);
			}

			OutSkeletalMesh->SetSkeleton(Skeleton);
			OutSkeletalMesh->SetRefSkeleton(Skeleton->GetReferenceSkeleton());
			OutSkeletalMesh->GetRefBasesInvMatrix().Empty(1);
			OutSkeletalMesh->CalculateInvRefMatrices();

			// Compute imported bounds
			FBoxSphereBounds Bounds;

			const int32 BonePoseCount = InMutableMesh->GetBonePoseCount();
			if (BonePoseCount > 0)
			{
				// Extract bounds from the pose of the mesh
				TArray<FVector> Points;
				Points.Reserve(BonePoseCount);
				for (int32 BoneIndex = 0; BoneIndex < BonePoseCount; ++BoneIndex)
				{
					FTransform3f Transform;
					InMutableMesh->GetBoneTransform(BoneIndex, Transform);

					Points.Add(FVector(Transform.GetTranslation()));
				}

				Bounds = FBoxSphereBounds(Points.GetData(), Points.Num());
				Bounds.ExpandBy(1.2f);
			}
			else
			{
				// Set bounds even if they're not correct
				Bounds = FBoxSphereBounds(FSphere(FVector(0,0,0), 1000));
			}

			OutSkeletalMesh->SetImportedBounds(Bounds);

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

			OutSkeletalMesh->GetLODInfoArray()[0].LODMaterialMap.SetNumZeroed(1);

			// Add RenderSections for each surface in the mesh
			if (InMutableMesh)
			{
				for (int32 SurfaceIndex = 0; SurfaceIndex < InMutableMesh->GetSurfaceCount(); ++SurfaceIndex)
				{
					new(OutSkeletalMesh->GetResourceForRendering()->LODRenderData[0].RenderSections) FSkelMeshRenderSection();
				}
			}
		}


		/** Prepares the skeletal mesh to be able to be rendered. It should provide you with a mesh ready to be rendered
		 * @param InMutableMesh - The mutable mesh to be used as reference
		 * @param InRefSkeletalMesh - A reference skeletal mesh to aid on the preparation of the OutSkeletalMesh
		 * @param InBoneMap - An array with all the bones used by the mesh.
		 * @param OutSkeletalMesh - The skeletal mesh to set up.
		 * @return True if the operation could be performed successfully, false if not.
		 */
		bool BuildSkeletalMeshRenderData(const mu::MeshPtrConst InMutableMesh, USkeletalMesh* OutSkeletalMesh)
		{
			OutSkeletalMesh->SetHasBeenSimplified(false);
			OutSkeletalMesh->SetHasVertexColors(false);

			// Find how many bones the bonemap could have
			const int32 NumBonesInBoneMap = !InMutableMesh->GetBoneMap().IsEmpty() ? InMutableMesh->GetBoneMap().Num() : InMutableMesh->GetBonePoseCount();
			
			// Fill the bonemap with zeros
			TArray<uint16> BoneMap;
			BoneMap.SetNumZeroed(FMath::Max(NumBonesInBoneMap, 1));

			FSkeletalMeshLODRenderData& LODResource = OutSkeletalMesh->GetResourceForRendering()->LODRenderData[0];

			// Load buffer data found on the mutable model onto the out skeletal mesh
			// It includes vertex and index buffers
			UnrealConversionUtils::SetupRenderSections(
				LODResource,
				InMutableMesh,
				BoneMap,
				0);

			UnrealConversionUtils::CopyMutableVertexBuffers(
				LODResource,
				InMutableMesh,
				false); 
			

			// Ensure there's at least one bone in the bonemap of each render section, the root.
			for (FSkelMeshRenderSection& RenderSection : LODResource.RenderSections)
			{
				if (RenderSection.BoneMap.IsEmpty())
				{
					RenderSection.BoneMap.Add(0);
				}
			}

			// Add root as the only required bone
			LODResource.ActiveBoneIndices.Add(0);
			LODResource.RequiredBones.Add(0);
			
			// Copy index buffers or fail to generate the mesh
			if (!UnrealConversionUtils::CopyMutableIndexBuffers(LODResource, InMutableMesh))
			{
				return false;
			}
			
			// Update LOD and streaming data
			LODResource.bIsLODOptional = false;
			LODResource.bStreamedDataInlined = false;
			
			return true;
		}
	}

	/*
	 * Publicly accessible functions of MutableMeshPreviewUtils
	 */

	USkeletalMesh* GenerateSkeletalMeshFromMutableMesh(mu::MeshPtrConst InMutableMesh)
	{
		if (!InMutableMesh)
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
				
				UE_LOG(LogMutable, Verbose, TEXT("Buffers Restructuring was succesfull."));
			}

			// We should now have a mutable mesh following a buffer structure where the required buffers are there to be
			// later found and used to generate a proper Skeletal Mesh
		}
		
		// Generate a new skeletal mesh to be filled up with the data found on the mutable mesh and the reference skeletal mesh
		USkeletalMesh* GeneratedSkeletalMesh = NewObject<USkeletalMesh>();

		// Build the reference skeleton for the Generated Skeletal mesh
		bool bSuccess = BuildSkeletalMeshSkeletonData(InMutableMesh, GeneratedSkeletalMesh);
		if (!bSuccess)
		{
			return nullptr;
		}
		
		// Prepare the mesh to be filled with rendering data (buffers)
		{
			GeneratedSkeletalMesh->AllocateResourceForRendering();
			GeneratedSkeletalMesh->GetResourceForRendering()->LODRenderData.Add(new FSkeletalMeshLODRenderData());

			FSkeletalMeshLODInfo& LastLODInfo = GeneratedSkeletalMesh->GetLODInfoArray().AddDefaulted_GetRef();
			LastLODInfo.BuildSettings.bUseFullPrecisionUVs = true;
			LastLODInfo.bAllowCPUAccess = false;
		}

		// Set the material data and initialize the sections that will be used later
		BuildSkeletalMeshElementData(InMutableMesh,GeneratedSkeletalMesh);
		
		// Load all data from the mutable mesh onto the skeletal mesh object
		bSuccess = BuildSkeletalMeshRenderData(InMutableMesh, GeneratedSkeletalMesh);
		if (!bSuccess)
		{
			return nullptr;
		}

		// Prepares the mesh to be rendered
		GeneratedSkeletalMesh->InitResources();
		
		return GeneratedSkeletalMesh;
	}
}
