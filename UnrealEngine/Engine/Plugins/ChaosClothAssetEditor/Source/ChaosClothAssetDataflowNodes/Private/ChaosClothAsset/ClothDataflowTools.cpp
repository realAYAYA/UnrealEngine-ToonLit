// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothDataflowTools.h"
#include "ChaosClothAsset/ClothGeometryTools.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "Dataflow/DataflowNode.h"
#include "DynamicMesh/NonManifoldMappingSupport.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Modules/ModuleManager.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "MeshUtilities.h"
#include "ReferenceSkeleton.h"
#include "SkeletalMeshAttributes.h"
#include "ToDynamicMesh.h"

DEFINE_LOG_CATEGORY(LogChaosClothAssetDataflowNodes);

namespace UE::Chaos::ClothAsset
{
	namespace Private
	{
		//
		// Wrapper for accessing a SkelMeshSection. Implements the interface expected by TToDynamicMesh<>.
		// This will weld all vertices which are the same.
		//
		template<bool bHasNormals = false, bool bHasTangents = false, bool bHasBiTangents = false, bool bHasColors = false>
		struct FSkelMeshSectionWrapper
		{
			typedef int32 TriIDType;
			typedef int32 VertIDType;
			typedef int32 WedgeIDType;
			typedef int32 UVIDType;
			typedef int32 NormalIDType;
			typedef int32 ColorIDType;

			FSkelMeshSectionWrapper(const FSkeletalMeshLODModel& SkeletalMeshModel, const int32 SectionIndex)
				: SourceSection(SkeletalMeshModel.Sections[SectionIndex])
				, IndexBuffer(SkeletalMeshModel.IndexBuffer.GetData() + SourceSection.BaseIndex, SourceSection.NumTriangles * 3)
			{
				const int32 NumVerts = SourceSection.SoftVertices.Num();
				const int32 NumTriangles = SourceSection.NumTriangles;

				// We need to weld the mesh verts to get rid of duplicates (happens for smoothing groups)
				TArray<FVector> UniqueVerts;
				OriginalToMerged.AddDefaulted(NumVerts);
				constexpr float ThreshSq = UE_THRESH_POINTS_ARE_SAME * UE_THRESH_POINTS_ARE_SAME;
				for (int32 VertIndex = 0; VertIndex < NumVerts; ++VertIndex)
				{
					const FSoftSkinVertex& SourceVert = SourceSection.SoftVertices[VertIndex];

					bool bUnique = true;
					int32 RemapIndex = INDEX_NONE;

					const int32 NumUniqueVerts = UniqueVerts.Num();
					for (int32 UniqueVertIndex = 0; UniqueVertIndex < NumUniqueVerts; ++UniqueVertIndex)
					{
						FVector& UniqueVert = UniqueVerts[UniqueVertIndex];

						if ((UniqueVert - (FVector)SourceVert.Position).SizeSquared() <= ThreshSq)
						{
							// Not unique
							bUnique = false;
							RemapIndex = UniqueVertIndex;

							break;
						}
					}

					if (bUnique)
					{
						// Unique
						UniqueVerts.Add((FVector)SourceVert.Position);
						OriginalIndexes.Add(VertIndex);
						OriginalToMerged[VertIndex] = VertIndex;
					}
					else
					{
						OriginalToMerged[VertIndex] = OriginalIndexes[RemapIndex];
					}
				}

				TriIDs.SetNum(NumTriangles);
				for (int32 Tri = 0; Tri < NumTriangles; ++Tri)
				{
					TriIDs[Tri] = Tri;
				}
			}

			int32 NumTris() const
			{
				return TriIDs.Num();
			}

			int32 NumVerts() const
			{
				return OriginalIndexes.Num();
			}

			int32 NumUVLayers() const
			{
				return MAX_TEXCOORDS;
			}

			// --"Vertex Buffer" info 
			const TArray<int32>& GetVertIDs() const
			{
				return OriginalIndexes;
			}

			const FVector3d GetPosition(const VertIDType VtxID) const
			{
				return (FVector3d)SourceSection.SoftVertices[VtxID].Position;
			}

			// --"Index Buffer" info
			const TArray<int32>& GetTriIDs() const
			{
				return TriIDs;
			}

			// return false if this TriID is not contained in mesh.
			bool GetTri(const TriIDType TriID, VertIDType& VID0, VertIDType& VID1, VertIDType& VID2) const
			{
				if (TriID >= 0 && TriID < (int32)SourceSection.NumTriangles)
				{
					VID0 = OriginalToMerged[IndexBuffer[3 * TriID + 0] - SourceSection.BaseVertexIndex];
					VID1 = OriginalToMerged[IndexBuffer[3 * TriID + 1] - SourceSection.BaseVertexIndex];
					VID2 = OriginalToMerged[IndexBuffer[3 * TriID + 2] - SourceSection.BaseVertexIndex];
					return true;
				}
				return false;
			}

			bool HasNormals() const
			{
				return bHasNormals;
			}

			bool HasTangents() const
			{
				return bHasTangents;
			}

			bool HasBiTangents() const
			{
				return bHasBiTangents;
			}

			bool HasColors() const
			{
				return bHasColors;
			}

			// Each triangle corner is a wedge. This will lookup into original unwelded soft verts
			void GetWedgeIDs(const TriIDType& TriID, WedgeIDType& WID0, WedgeIDType& WID1, WedgeIDType& WID2) const
			{
				WID0 = IndexBuffer[3 * TriID + 0] - SourceSection.BaseVertexIndex;
				WID1 = IndexBuffer[3 * TriID + 1] - SourceSection.BaseVertexIndex;
				WID2 = IndexBuffer[3 * TriID + 2] - SourceSection.BaseVertexIndex;
			}

			// attribute access per-wedge
			// NB:  ToDynamicMesh will attempt to weld identical attributes that are associated with the same vertex
			FVector2f GetWedgeUV(int32 UVLayerIndex, WedgeIDType WID) const
			{
				check(UVLayerIndex < MAX_TEXCOORDS);
				return SourceSection.SoftVertices[WID].UVs[UVLayerIndex];
			}

			FVector3f GetWedgeNormal(WedgeIDType WID) const
			{
				return SourceSection.SoftVertices[WID].TangentZ;
			}

			FVector3f GetWedgeTangent(WedgeIDType WID) const
			{
				return SourceSection.SoftVertices[WID].TangentX;
			}

			FVector3f GetWedgeBiTangent(WedgeIDType WID) const
			{
				return SourceSection.SoftVertices[WID].TangentY;
			}

			FVector4f GetWedgeColor(WedgeIDType WID) const
			{
				return FLinearColor(SourceSection.SoftVertices[WID].Color);
			}

			// attribute access that exploits shared attributes. 
			// each group of shared attributes presents itself as a mesh with its own attribute vertex buffer.
			// NB:  If the mesh has no shared Attr attributes, then Get{Attr}IDs() should return an empty array.
			// NB:  Get{Attr}Tri() functions should return false if the triangle is not set in the attribute mesh. 
			const TArray<UVIDType>& GetUVIDs(int32 LayerID) const
			{
				return EmptyArray;
			}

			FVector2f GetUV(int32 LayerID, UVIDType UVID) const
			{
				check(false);
				return FVector2f();
			}

			bool GetUVTri(int32 LayerID, const TriIDType& TID, UVIDType& ID0, UVIDType& ID1, UVIDType& ID2) const
			{
				return false;
			}

			const TArray<NormalIDType>& GetNormalIDs() const
			{
				return EmptyArray;
			}

			FVector3f GetNormal(NormalIDType ID) const
			{
				check(false);
				return FVector3f();
			}

			bool GetNormalTri(const TriIDType& TID, NormalIDType& NID0, NormalIDType& NID1, NormalIDType& NID2) const
			{
				return false;
			}

			const TArray<NormalIDType>& GetTangentIDs() const
			{
				return EmptyArray;
			}

			FVector3f GetTangent(NormalIDType ID) const
			{
				check(false);
				return FVector3f();
			}

			bool GetTangentTri(const TriIDType& TID, NormalIDType& NID0, NormalIDType& NID1, NormalIDType& NID2) const
			{
				return false;
			}

			const TArray<NormalIDType>& GetBiTangentIDs() const
			{
				return EmptyArray;
			}

			FVector3f GetBiTangent(NormalIDType ID) const
			{
				check(false);
				return FVector3f();
			}

			bool GetBiTangentTri(const TriIDType& TID, NormalIDType& NID0, NormalIDType& NID1, NormalIDType& NID2) const
			{
				return false;
			}

			const TArray<ColorIDType>& GetColorIDs() const
			{
				return EmptyArray;
			}

			FVector4f GetColor(ColorIDType ID) const
			{
				check(false);
				return FVector4f();
			}

			bool GetColorTri(const TriIDType& TID, ColorIDType& NID0, ColorIDType& NID1, ColorIDType& NID2) const
			{
				return false;
			}

			// weight maps information
			int32 NumWeightMapLayers() const
			{
				return 0;
			}

			float GetVertexWeight(int32 WeightMapIndex, int32 SrcVertID) const
			{
				check(false);
				return 0.f;
			}

			FName GetWeightMapName(int32 WeightMapIndex) const
			{
				check(false);
				return FName();
			}

			// skin weight attributes information
			int32 NumSkinWeightAttributes() const
			{
				return 1;
			}

			UE::AnimationCore::FBoneWeights GetVertexSkinWeight(int32 SkinWeightAttributeIndex, VertIDType VtxID) const
			{
				using namespace UE::AnimationCore;
				check(SkinWeightAttributeIndex == 0);
				const int32 NumInfluences = SourceSection.MaxBoneInfluences;
				const FSoftSkinVertex& SoftVertex = SourceSection.SoftVertices[VtxID];
				TArray<FBoneWeight> BoneWeightArray;
				BoneWeightArray.SetNumUninitialized(NumInfluences);
				for (int32 Idx = 0; Idx < NumInfluences; ++Idx)
				{
					BoneWeightArray[Idx] = FBoneWeight(SourceSection.BoneMap[SoftVertex.InfluenceBones[Idx]], (float)SoftVertex.InfluenceWeights[Idx] * UE::AnimationCore::InvMaxRawBoneWeightFloat);
				}
				return FBoneWeights::Create(BoneWeightArray, FBoneWeightsSettings());
			}

			FName GetSkinWeightAttributeName(int32 SkinWeightAttributeIndex) const
			{
				checkfSlow(SkinWeightAttributeIndex == 0, TEXT("Cloth assets should only have one skin weight profile"));
				return FSkeletalMeshAttributes::DefaultSkinWeightProfileName;
			}

			// bone attributes information
			int32 GetNumBones() const
			{
				return 0;
			}

			FName GetBoneName(int32 BoneIdx) const
			{
				check(false);
				return FName();
			}

			int32 GetBoneParentIndex(int32 BoneIdx) const
			{
				check(false);
				return INDEX_NONE;
			}

			FTransform GetBonePose(int32 BoneIdx) const
			{
				check(false);
				return FTransform();
			}

			FVector4f GetBoneColor(int32 BoneIdx) const
			{
				check(false);
				return FLinearColor::White;
			}


			const FSkelMeshSection& SourceSection;
			const TConstArrayView<uint32> IndexBuffer;
			TArray<int32> OriginalIndexes; // UniqueIndex -> OrigIndex
			TArray<int32> OriginalToMerged; // OriginalIndex -> OriginalIndexes[UniqueVertIndex] 
			TArray<int32> TriIDs;
			TArray<int32> EmptyArray;

		};
	} // Private

	void FClothDataflowTools::AddRenderPatternFromSkeletalMeshSection(const TSharedRef<FManagedArrayCollection>& ClothCollection, const FSkeletalMeshLODModel& SkeletalMeshModel, const int32 SectionIndex, const FString& RenderMaterialPathName)
	{
		check(SectionIndex < SkeletalMeshModel.Sections.Num());

		FCollectionClothFacade Cloth(ClothCollection);
		check(Cloth.IsValid());

		FCollectionClothRenderPatternFacade ClothPatternFacade = Cloth.AddGetRenderPattern();

		const FSkelMeshSection& Section = SkeletalMeshModel.Sections[SectionIndex];
		ClothPatternFacade.SetNumRenderVertices(Section.NumVertices);
		ClothPatternFacade.SetNumRenderFaces(Section.NumTriangles);

		TArrayView<FVector3f> RenderPosition = ClothPatternFacade.GetRenderPosition();
		TArrayView<FVector3f> RenderNormal = ClothPatternFacade.GetRenderNormal();
		TArrayView<FVector3f> RenderTangentU = ClothPatternFacade.GetRenderTangentU();
		TArrayView<FVector3f> RenderTangentV = ClothPatternFacade.GetRenderTangentV();
		TArrayView<TArray<FVector2f>> RenderUVs = ClothPatternFacade.GetRenderUVs();
		TArrayView<FLinearColor> RenderColor = ClothPatternFacade.GetRenderColor();
		TArrayView<TArray<int32>> RenderBoneIndices = ClothPatternFacade.GetRenderBoneIndices();
		TArrayView<TArray<float>> RenderBoneWeights = ClothPatternFacade.GetRenderBoneWeights();
		for (int32 VertexIndex = 0; VertexIndex < Section.NumVertices; ++VertexIndex)
		{
			const FSoftSkinVertex& SoftVertex = Section.SoftVertices[VertexIndex];

			RenderPosition[VertexIndex] = SoftVertex.Position;
			RenderNormal[VertexIndex] = SoftVertex.TangentZ;
			RenderTangentU[VertexIndex] = SoftVertex.TangentX;
			RenderTangentV[VertexIndex] = SoftVertex.TangentY;
			RenderUVs[VertexIndex].SetNum(MAX_TEXCOORDS);
			for (int32 TexCoordIndex = 0; TexCoordIndex < MAX_TEXCOORDS; ++TexCoordIndex)
			{
				RenderUVs[VertexIndex][TexCoordIndex] = SoftVertex.UVs[TexCoordIndex];
			}

			RenderColor[VertexIndex] = FLinearColor(SoftVertex.Color);

			const int32 NumBones = Section.MaxBoneInfluences;
			RenderBoneIndices[VertexIndex].SetNum(NumBones);
			RenderBoneWeights[VertexIndex].SetNum(NumBones);
			for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
			{
				RenderBoneIndices[VertexIndex][BoneIndex] = (int32)Section.BoneMap[(int32)SoftVertex.InfluenceBones[BoneIndex]];
				RenderBoneWeights[VertexIndex][BoneIndex] = (float)SoftVertex.InfluenceWeights[BoneIndex] * UE::AnimationCore::InvMaxRawBoneWeightFloat;
			}
		}

		const int32 VertexOffset = ClothPatternFacade.GetRenderVerticesOffset();
		TArrayView<FIntVector3> RenderIndices = ClothPatternFacade.GetRenderIndices();
		for (uint32 FaceIndex = 0; FaceIndex < Section.NumTriangles; ++FaceIndex)
		{
			const uint32 IndexOffset = Section.BaseIndex + FaceIndex * 3;
			RenderIndices[FaceIndex] = FIntVector3(
				SkeletalMeshModel.IndexBuffer[IndexOffset + 0] - Section.BaseVertexIndex + VertexOffset,
				SkeletalMeshModel.IndexBuffer[IndexOffset + 1] - Section.BaseVertexIndex + VertexOffset,
				SkeletalMeshModel.IndexBuffer[IndexOffset + 2] - Section.BaseVertexIndex + VertexOffset
			);
		}
		ClothPatternFacade.SetRenderMaterialPathName(RenderMaterialPathName);
	}
	
	void FClothDataflowTools::AddSimPatternsFromSkeletalMeshSection(const TSharedRef<FManagedArrayCollection>& ClothCollection, const FSkeletalMeshLODModel& SkeletalMeshModel, const int32 SectionIndex, const int32 UVChannelIndex, const FVector2f& UVScale)
	{
		check(SectionIndex < SkeletalMeshModel.Sections.Num());

		// Convert to DynamicMesh and then use that to create patterns.
		UE::Geometry::TToDynamicMesh<Private::FSkelMeshSectionWrapper<>> SkelMeshSectionToDynamicMesh;
		Private::FSkelMeshSectionWrapper<> SectionWrapper(SkeletalMeshModel, SectionIndex);

		UE::Geometry::FDynamicMesh3 DynamicMesh;
		DynamicMesh.EnableAttributes();
		constexpr bool bCopyTangents = false;
		SkelMeshSectionToDynamicMesh.Convert(DynamicMesh, SectionWrapper, [](int32) { return 0; }, [](int32) { return INDEX_NONE; }, bCopyTangents);

		// Set ToSrcVertIDMap as an overlay that the build sim mesh code expects.
		UE::Geometry::FNonManifoldMappingSupport::AttachNonManifoldVertexMappingData(SkelMeshSectionToDynamicMesh.ToSrcVertIDMap, DynamicMesh);

		constexpr bool bAppend = true;
		FClothGeometryTools::BuildSimMeshFromDynamicMesh(ClothCollection, DynamicMesh, UVChannelIndex, UVScale, bAppend);
	}

	void FClothDataflowTools::LogAndToastWarning(const FDataflowNode& DataflowNode, const FText& Headline, const FText& Details)
	{
		static const FTextFormat TextFormat = FTextFormat::FromString(TEXT("{0}: {1}\n{2}"));
		const FText NodeName = FText::FromName(DataflowNode.GetName());
		const FText Text = FText::Format(TextFormat, NodeName, Headline, Details);

		FNotificationInfo NotificationInfo(Text);
		NotificationInfo.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(NotificationInfo);

		UE_LOG(LogChaosClothAssetDataflowNodes, Warning, TEXT("%s"), *Text.ToString());
	}

	void FClothDataflowTools::MakeCollectionName(FString& InOutString)
	{
		InOutString = SlugStringForValidName(InOutString, TEXT("_")).Replace(TEXT("\\"), TEXT("_"));
		bool bCharsWereRemoved;
		do { InOutString.TrimCharInline(TEXT('_'), &bCharsWereRemoved); } while (bCharsWereRemoved);
	}

	static void CopyBuildSettings(const FMeshBuildSettings& InStaticMeshBuildSettings, FSkeletalMeshBuildSettings& OutSkeletalMeshBuildSettings)
	{
		OutSkeletalMeshBuildSettings.bRecomputeNormals = InStaticMeshBuildSettings.bRecomputeNormals;
		OutSkeletalMeshBuildSettings.bRecomputeTangents = InStaticMeshBuildSettings.bRecomputeTangents;
		OutSkeletalMeshBuildSettings.bUseMikkTSpace = InStaticMeshBuildSettings.bUseMikkTSpace;
		OutSkeletalMeshBuildSettings.bComputeWeightedNormals = InStaticMeshBuildSettings.bComputeWeightedNormals;
		OutSkeletalMeshBuildSettings.bRemoveDegenerates = InStaticMeshBuildSettings.bRemoveDegenerates;
		OutSkeletalMeshBuildSettings.bUseHighPrecisionTangentBasis = InStaticMeshBuildSettings.bUseHighPrecisionTangentBasis;
		OutSkeletalMeshBuildSettings.bUseFullPrecisionUVs = InStaticMeshBuildSettings.bUseFullPrecisionUVs;
		OutSkeletalMeshBuildSettings.bUseBackwardsCompatibleF16TruncUVs = InStaticMeshBuildSettings.bUseBackwardsCompatibleF16TruncUVs;
		// The rest we leave at defaults.
	}

	bool FClothDataflowTools::BuildSkeletalMeshModelFromMeshDescription(const FMeshDescription* const InMeshDescription, const FMeshBuildSettings& InBuildSettings, FSkeletalMeshLODModel& SkeletalMeshModel)
	{
		// This is following StaticToSkeletalMeshConverter.cpp::AddLODFromStaticMeshSourceModel
		FSkeletalMeshBuildSettings BuildSettings;
		CopyBuildSettings(InBuildSettings, BuildSettings);
		FMeshDescription SkeletalMeshGeometry = *InMeshDescription;
		FSkeletalMeshAttributes SkeletalMeshAttributes(SkeletalMeshGeometry);
		SkeletalMeshAttributes.Register();

		// Full binding to the root bone.
		constexpr int32 RootBoneIndex = 0;
		FSkinWeightsVertexAttributesRef SkinWeights = SkeletalMeshAttributes.GetVertexSkinWeights();
		UE::AnimationCore::FBoneWeight RootInfluence(RootBoneIndex, 1.0f);
		UE::AnimationCore::FBoneWeights RootBinding = UE::AnimationCore::FBoneWeights::Create({ RootInfluence });

		for (const FVertexID VertexID : SkeletalMeshGeometry.Vertices().GetElementIDs())
		{
			SkinWeights.Set(VertexID, RootBinding);
		}

		FSkeletalMeshImportData SkeletalMeshImportGeometry = FSkeletalMeshImportData::CreateFromMeshDescription(SkeletalMeshGeometry);
		// Data needed by BuildSkeletalMesh
		TArray<FVector3f> LODPoints;
		TArray<SkeletalMeshImportData::FMeshWedge> LODWedges;
		TArray<SkeletalMeshImportData::FMeshFace> LODFaces;
		TArray<SkeletalMeshImportData::FVertInfluence> LODInfluences;
		TArray<int32> LODPointToRawMap;
		SkeletalMeshImportGeometry.CopyLODImportData(LODPoints, LODWedges, LODFaces, LODInfluences, LODPointToRawMap);
		IMeshUtilities::MeshBuildOptions BuildOptions;
		BuildOptions.TargetPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
		BuildOptions.FillOptions(BuildSettings);

		static const FString SkeletalMeshName("ClothAssetStaticMeshImportConvert"); // This is only used by warning messages in the mesh builder.
		// Build a RefSkeleton with just a root bone. The BuildSkeletalMesh code expects you have a reference skeleton with at least one bone to work.
		FReferenceSkeleton RootBoneRefSkeleton;
		FReferenceSkeletonModifier SkeletonModifier(RootBoneRefSkeleton, nullptr);
		FMeshBoneInfo RootBoneInfo;
		RootBoneInfo.Name = FName("Root");
		SkeletonModifier.Add(RootBoneInfo, FTransform());
		RootBoneRefSkeleton.RebuildRefSkeleton(nullptr, true);

		IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
		TArray<FText> WarningMessages;
		if (!MeshUtilities.BuildSkeletalMesh(SkeletalMeshModel, SkeletalMeshName, RootBoneRefSkeleton, LODInfluences, LODWedges, LODFaces, LODPoints, LODPointToRawMap, BuildOptions, &WarningMessages))
		{
			for (const FText& Message : WarningMessages)
			{
				UE_LOG(LogChaosClothAssetDataflowNodes, Warning, TEXT("%s"), *Message.ToString());
			}
			return false;
		}
		return true;
	}
}  // End namespace UE::Chaos::ClothAsset
