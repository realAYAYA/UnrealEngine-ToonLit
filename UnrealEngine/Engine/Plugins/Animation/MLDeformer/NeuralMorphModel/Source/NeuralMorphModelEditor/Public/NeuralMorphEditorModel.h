// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "MLDeformerMorphModelEditorModel.h"
#include "NeuralMorphModel.h"

class UNeuralMorphModel;

namespace UE::MLDeformer
{
	class SMLDeformerInputWidget;
}

namespace UE::NeuralMorphModel
{
	using namespace UE::MLDeformer;

	/**
	 * The editor model related to the neural morph model's runtime class (UNeuralMorphModel).
	 */
	class NEURALMORPHMODELEDITOR_API FNeuralMorphEditorModel
		: public UE::MLDeformer::FMLDeformerMorphModelEditorModel
	{
	public:
		// We need to implement this static MakeInstance method.
		static FMLDeformerEditorModel* MakeInstance();

		// FGCObject overrides.
		virtual FString GetReferencerName() const override		{ return TEXT("FNeuralMorphEditorModel"); }
		virtual bool LoadTrainedNetwork() const override;
		virtual void UpdateIsReadyForTrainingState() override;
		// ~END FGCObject overrides.
	
		// FMLDeformerEditorModel overrides.
		virtual void Init(const InitSettings& Settings) override;
		virtual ETrainingResult Train() override;
		virtual void InitInputInfo(UMLDeformerInputInfo* InputInfo) override;
		virtual FText GetOverlayText() const override;
		virtual void OnPropertyChanged(FPropertyChangedEvent& PropertyChangedEvent) override;
		virtual TSharedPtr<SMLDeformerInputWidget> CreateInputWidget() override;
		virtual void OnPostInputAssetChanged() override;
		virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;
		// ~END FMLDeformerEditorModel overrides.

		// FMLDeformerMorphModelEditorModel overrides.
		virtual const TArrayView<const float> GetMaskForMorphTarget(int32 MorphTargetIndex) const override;
		virtual bool IsInputMaskingSupported() const override	{ return true; }
		// ~FMLDeformerMorphModelEditorModel overrides.

		/** Get a pointer to the neural morph runtime model. */
		UNeuralMorphModel* GetNeuralMorphModel() const			{ return Cast<UNeuralMorphModel>(Model); }

		/**
		 * Build a set of masks, all concatenated into one flat array.
		 * The number of masks is (NumBones + NumCurves + NumBoneGroups + NumCurveGroups).
		 * Each mask contains Model->GetNumBaseMeshVerts() number of floats.
		 * The masks for curves and curve groups are all set to 1.0.
		 * The masks for bones and bone groups contain all the vertices they influence during skinning, as well as the ones from the parent bone.
		 * This will modify the item mask buffer as returned by UMLDeformerMorphModel::GetInputItemMaskBuffer().
		 * @param OutMaskBuffer The mask buffer to write to. This will automatically be resized.
		 */
		void BuildMaskBuffer(TArray<float>& OutMaskBuffer);

		/** Rebuild the mask buffer. */
		void RebuildEditorMaskInfo();

		/**
		 * Generate a unique name for a bone group.
		 * @return The unique name.
		 */
		FName GenerateUniqueBoneGroupName() const;

		/**
		 * Generate a unique name for a curve group.
		 * @return The unique name.
		 */
		FName GenerateUniqueCurveGroupName() const;

		/**
		 * Remove mask infos for bones that do not exist in the bone include list.
		 */
		void RemoveNonExistingMaskInfos();

		/** Clear all bone mask infos. */
		void ResetBoneMaskInfos();

		/** Clear all bone group mask infos. */
		void ResetBoneGroupMaskInfos();

		/**
		 * Generate bone mask infos for all bones in the bone input list.
		 * Use a specific hierarchy depth for this, which defines how deep up and down the hierarchy we should traverse for each specific bone.
		 * Imagine we generate the mask for a bone in a chain of bones. We look at all vertices skinned to this bone, and include those vertices in the mask.
		 * With a hierarchy depth of 1, include all vertices skinned to its parent bone as well as to its child bone.
		 * With a hierarchy depth of 2 we also include the parent of the parent and the child of the child if it exists.
		 * We always want the mask to be large enough, otherwise if say skin stretches a lot, we will not capture the vertex deltas for those vertices far away from the bone, which can cause issues.
		 * However we don't want the mask to be too large either, as that costs performance and memory usage.
		 * @param HierarchyDepth The hierarchy depth to use, which should be a value of 1 or above.
		 */
		void GenerateBoneMaskInfos(int32 HierarchyDepth);

		/**
		 * Generate the mask infos for all bone groups. 
		 * Look at the GenerateBoneMaskInfos method for a detailed description of the HierarchyDepth.
		 * @param HierarchyDepth The hierarchy depth to use, which should be a value of 1 or above.
		 * @see GenerateBoneMaskInfos
		 */
		void GenerateBoneGroupMaskInfos(int32 HierarchyDepth);

		/**
		 * Generate the mask info for a given bone.
		 * Look at the GenerateBoneMaskInfos method for a detailed description of the HierarchyDepth.
		 * @param InputInfoBoneIndex The bone index inside the input info object.
		 * @param HierarchyDepth The hierarchy depth to use, which should be a value of 1 or above.
		 * @see GenerateBoneMaskInfos
		 */
		void GenerateBoneMaskInfo(int32 InputInfoBoneIndex, int32 HierarchyDepth);

		/**
		 * Generate the mask info for a given bone group.
		 * Look at the GenerateBoneMaskInfos method for a detailed description of the HierarchyDepth.
		 * @param InputInfoBoneIndex The bone index inside the input info object.
		 * @param HierarchyDepth The hierarchy depth to use, which should be a value of 1 or above.
		 * @see GenerateBoneMaskInfos
		 */
		void GenerateBoneGroupMaskInfo(int32 InputInfoBoneGroupIndex, int32 HierarchyDepth);

		/**
		 * Apply the mask info to the mask buffer of floats.
		 * @param SkeletalMesh The skeletal mesh that the mask relates to.
		 * @param MaskInfo The mask info object to apply to the float mask buffer.
		 * @param ItemMaskBuffer The mask buffer for this item. This should be an array view of size NumBaseMeshVerts.
		 */
		void ApplyMaskInfoToMaskBuffer(const USkeletalMesh* SkeletalMesh, const FNeuralMorphMaskInfo& MaskInfo, TArrayView<float> ItemMaskBuffer);

		/**
		 * Set the mask visualization item index. This specifies which item (bone, curve, bone group, curve group) to visualize the mask for.
		 * The first index values are for all input bones. Indices that come after that are for curves, then for bone groups and finally followed by curve groups.
		 * So if there are 10 input bones and 4 bone groups, and the visualization index is 11, it means it's for the second bone group.
		 * @param InMaskVizItemIndex The mask visualization index.
		 */
		void SetMaskVisualizationItemIndex(int32 InMaskVizItemIndex)	{ MaskVizItemIndex = InMaskVizItemIndex; }

		/**
		 * Get the mask visualization item index. This specifies which item (bone, curve, bone group, curve group) to visualize the mask for.
		 * The first index values are for all input bones. Indices that come after that are for curves, then for bone groups and finally followed by curve groups.
		 * So if there are 10 input bones and 4 bone groups, and the visualization index is 11, it means it's for the second bone group.
		 * @return The item to visualize the mask for.
		 */
		int32 GetMaskVisualizationItemIndex() const						{ return MaskVizItemIndex; }

		/**
		 * Draw the mask for a given item. An item can be a bone, curve or anything else that has a mask.
		 * @param PDI The debug draw interface.
		 * @param MaskItemIndex The item index. The valid range on default is [0..NumEditorInfoBones + NumEditorInfoCurves - 1]. The curves are attached after the bones.
		 *        So if you have 10 bones, and 5 curves, and the MaskItemIndex is value 12, it will draw the mask for the third curve, because items [0..9] would be for the bones, and [10..14] for the curves.
		 * @param DrawOffset The offset in world space units to draw the mask at.
		 */
		void DebugDrawItemMask(FPrimitiveDrawInterface* PDI, int32 MaskItemIndex, const FVector& DrawOffset);

	private:
		/**
		 * Add child twist bones to the list of skeletal bone indices.
		 * This will iterate over all bones inside the SkelBoneIndices array and check if there are direct child nodes inside the ref skeleton that contain
		 * the substring "twist" inside their name. If so, that bone will be added to the SkelBoneIndices array as well.
		 * The "twist" string is case-insensitive. The "twist" string can be configured on a per project basis inside the UNeuralMorphModel::TwistBoneSubString property, which 
		 * is exposed in the per project .ini file. On default this value is "twist".
		 * @param RefSkel The reference skeleton we use to find child nodes.
		 * @param SkelBoneIndices The input and output array of indices inside the reference skeleton. This method potentially adds new entries to this array.
		 */
		void AddTwistBones(const FReferenceSkeleton& RefSkel, TArray<int32>& SkelBoneIndices);

	protected:
		int32 MaskVizItemIndex = INDEX_NONE;
	};
}	// namespace UE::NeuralMorphModel
