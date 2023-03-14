// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "GLTFAsset.h"
#include "InterchangeTranslatorBase.h"
#include "Animation/InterchangeAnimationPayloadInterface.h"
#include "Mesh/InterchangeStaticMeshPayload.h"
#include "Mesh/InterchangeStaticMeshPayloadInterface.h"
#include "Mesh/InterchangeSkeletalMeshPayload.h"
#include "Mesh/InterchangeSkeletalMeshPayloadInterface.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Scene/InterchangeVariantSetPayloadInterface.h"
#include "Texture/InterchangeTexturePayloadInterface.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "InterchangeGltfTranslator.generated.h"

class UInterchangeShaderGraphNode;
class UInterchangeShaderNode;
class UInterchangeVariantSetNode;
class UInterchangeMeshNode;

/* Gltf translator class support import of texture, material, static mesh, skeletal mesh, */

UCLASS(BlueprintType)
class UInterchangeGltfTranslator : public UInterchangeTranslatorBase
	, public IInterchangeStaticMeshPayloadInterface
	, public IInterchangeTexturePayloadInterface
	, public IInterchangeAnimationPayloadInterface
	, public IInterchangeVariantSetPayloadInterface
	, public IInterchangeSkeletalMeshPayloadInterface
{
	GENERATED_BODY()

public:
	/** Begin UInterchangeTranslatorBase API*/
	virtual EInterchangeTranslatorType GetTranslatorType() const override;
	virtual EInterchangeTranslatorAssetType GetSupportedAssetTypes() const override;
	virtual TArray<FString> GetSupportedFormats() const override;
	virtual bool Translate( UInterchangeBaseNodeContainer& BaseNodeContainer ) const override;
	/** End UInterchangeTranslatorBase API*/

	/* IInterchangeStaticMeshPayloadInterface Begin */

	virtual TFuture< TOptional< UE::Interchange::FStaticMeshPayloadData > > GetStaticMeshPayloadData( const FString& PayLoadKey ) const override;

	/* IInterchangeStaticMeshPayloadInterface End */

	/* IInterchangeTexturePayloadInterface Begin */

	virtual TOptional< UE::Interchange::FImportImage > GetTexturePayloadData( const UInterchangeSourceData* InSourceData, const FString& PayLoadKey ) const override;

	/* IInterchangeTexturePayloadInterface End */

	/* IInterchangeAnimationPayloadInterface Begin */
	virtual TFuture<TOptional<UE::Interchange::FAnimationCurvePayloadData>> GetAnimationCurvePayloadData(const FString& PayLoadKey) const override;
	virtual TFuture<TOptional<UE::Interchange::FAnimationStepCurvePayloadData>> GetAnimationStepCurvePayloadData(const FString& PayLoadKey) const override;
	virtual TFuture<TOptional<UE::Interchange::FAnimationBakeTransformPayloadData>> GetAnimationBakeTransformPayloadData(const FString& PayLoadKey, const double BakeFrequency, const double RangeStartSecond, const double RangeStopSecond) const override;
	/* IInterchangeAnimationPayloadInterface End */

	/* IInterchangeVariantSetPayloadInterface Begin */
	virtual TFuture<TOptional<UE::Interchange::FVariantSetPayloadData>> GetVariantSetPayloadData(const FString& PayloadKey) const override;
	/* IInterchangeVariantSetPayloadInterface End */

	/* IInterchangeSkeletalMeshPayloadInterface Begin */
	virtual TFuture<TOptional<UE::Interchange::FSkeletalMeshLodPayloadData>> GetSkeletalMeshLodPayloadData(const FString& PayLoadKey) const override;
	virtual TFuture<TOptional<UE::Interchange::FSkeletalMeshMorphTargetPayloadData>> GetSkeletalMeshMorphTargetPayloadData(const FString& PayLoadKey) const override;
	/* IInterchangeSkeletalMeshPayloadInterface End */

protected:
	using FNodeUidMap = TMap<const GLTF::FNode*, FString>;

	void HandleGltfSkeletons( UInterchangeBaseNodeContainer& NodeContainer, const FString& SceneNodeUid, const TArray<int32>& SkinnedMeshNodes, TSet<int>& UnusedMeshIndices ) const;
	void HandleGltfNode( UInterchangeBaseNodeContainer& NodeContainer, const GLTF::FNode& GltfNode, const FString& ParentNodeUid, const int32 NodeIndex, 
		bool &bHasVariants, TArray<int32>& SkinnedMeshNodes, TSet<int>& UnusedMeshIndices ) const;
	void HandleGltfMaterial( UInterchangeBaseNodeContainer& NodeContainer, const GLTF::FMaterial& GltfMaterial, UInterchangeShaderGraphNode& ShaderGraphNode ) const;
	void HandleGltfMaterialParameter( UInterchangeBaseNodeContainer& NodeContainer, const GLTF::FTextureMap& TextureMap, UInterchangeShaderNode& ShaderNode,
		const FString& MapName, const TVariant< FLinearColor, float >& MapFactor, const FString& OutputChannel, const bool bInverse = false, const bool bIsNormal = false ) const;
	void HandleGltfAnimation(UInterchangeBaseNodeContainer& NodeContainer, int32 AnimationIndex) const;
	void HandleGltfVariants(UInterchangeBaseNodeContainer& NodeContainer, const FString& FileName) const;
	UInterchangeMeshNode* HandleGltfMesh(UInterchangeBaseNodeContainer& NodeContainer, const GLTF::FMesh& GltfMesh, int MeshIndex,
		TSet<int>& UnusedMeshIndices, const FString& SkeletalName = "" /*If set it creates the mesh even if it was already created (for Skeletals)*/) const;

	/** Support for KHR_materials_clearcoat */
	void HandleGltfClearCoat( UInterchangeBaseNodeContainer& NodeContainer, const GLTF::FMaterial& GltfMaterial, UInterchangeShaderGraphNode& ShaderGraphNode, const bool bSwapNormalAndClearCoatNormal ) const;
	/** Support for KHR_materials_sheen */
	void HandleGltfSheen( UInterchangeBaseNodeContainer& NodeContainer, const GLTF::FMaterial& GltfMaterial, UInterchangeShaderGraphNode& ShaderGraphNode ) const;
	/** Support for KHR_materials_transmission */
	void HandleGltfTransmission( UInterchangeBaseNodeContainer& NodeContainer, const GLTF::FMaterial& GltfMaterial, UInterchangeShaderGraphNode& ShaderGraphNode ) const;

	/** Support for KHR_texture_transform */
	void HandleGltfTextureTransform( UInterchangeBaseNodeContainer& NodeContainer, const GLTF::FTextureTransform& TextureTransform, const int32 TexCoordIndex, UInterchangeShaderNode& ShaderNode ) const;

	bool GetVariantSetPayloadData(UE::Interchange::FVariantSetPayloadData& PayloadData) const;

private:
	void SetTextureSRGB(UInterchangeBaseNodeContainer& NodeContainer, const GLTF::FTextureMap& TextureMap) const;
	void SetTextureFlipGreenChannel(UInterchangeBaseNodeContainer& NodeContainer, const GLTF::FTextureMap& TextureMap) const;

	GLTF::FAsset GltfAsset;
	mutable FNodeUidMap NodeUidMap;
};


