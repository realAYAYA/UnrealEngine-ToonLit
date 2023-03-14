// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GLTFAnimation.h"
#include "GLTFMaterial.h"
#include "GLTFMesh.h"
#include "GLTFNode.h"
#include "GLTFTexture.h"

#include "Containers/Set.h"
#include "CoreMinimal.h"

namespace GLTF
{
	enum class EExtension
	{
		KHR_MaterialsPbrSpecularGlossiness,
		KHR_MaterialsUnlit,
		KHR_MaterialsClearCoat,
		KHR_MaterialsTransmission,
		KHR_MaterialsSheen,
		KHR_MaterialsVariants,
		KHR_MaterialsIOR,
		KHR_MaterialsSpecular,
		KHR_TextureTransform,
		KHR_DracoMeshCompression,
		KHR_LightsPunctual,
		KHR_Blend,
		MSFT_TextureDDS,
		MSFT_PackingNormalRoughnessMetallic,
		MSFT_PackingOcclusionRoughnessMetallic,
		Count
	};

	struct GLTFCORE_API FScene
	{
		FString       Name;
		TArray<int32> Nodes;
	};

	struct GLTFCORE_API FMetadata
	{
		struct FExtraData
		{
			FString Name;
			FString Value;
		};
		FString            GeneratorName;
		float              Version;
		TArray<FExtraData> Extras;

		const FExtraData* GetExtraData(const FString& Name) const;
	};

	struct GLTFCORE_API FAsset : FNoncopyable
	{
		static const TSet<EExtension> SupportedExtensions;

		enum EValidationCheck
		{
			Valid,
			InvalidMeshPresent   = 0x1,
			InvalidNodeTransform = 0x2,
		};

		FString Name;

		TArray<FBuffer>        Buffers;
		TArray<FBufferView>    BufferViews;
		TArray<FValidAccessor> Accessors;
		TArray<FMesh>          Meshes;

		TArray<FScene>     Scenes;
		TArray<FNode>      Nodes;
		TArray<FCamera>    Cameras;
		TArray<FLight>     Lights;
		TArray<FSkinInfo>  Skins;
		TArray<FAnimation> Animations;

		TArray<FImage>    Images;
		TArray<FSampler>  Samplers;
		TArray<FTexture>  Textures;
		TArray<FMaterial> Materials;

		TArray<FString>   Variants;

		TSet<EExtension> ExtensionsUsed;
		TArray<FString>	 RequiredExtensions;
		FMetadata        Metadata;

		/**
		 * Will clear the asset's buffers.
		 *
		 * @param BinBufferKBytes - bytes to reserve for the bin chunk buffer.
		 * @param ExtraBinBufferKBytes -  bytes to reserve for the extra binary buffer(eg. image, mime data, etc.)
		 * @note Only reserves buffers if they had any existing data.
		 */
		void Clear(uint32 BinBufferKBytes, uint32 ExtraBinBufferKBytes);

		/**
		 * Will generate names for any entities(nodes, meshes, etc.) that have the name field missing.
		 *
		 * @param Prefix - prefix to add to the entities name.
		 */
		void GenerateNames(const FString& Prefix);

		/**
		 * Finds the indices for the nodes which are root nodes.
		 *
		 * @param NodeIndices - array with the results
		 */
		void GetRootNodes(TArray<int32>& NodeIndices);

		/**
		 * Returns EValidationCheck::Valid if the asset passes the post-import validation checks.
		 */
		EValidationCheck ValidationCheck() const;

	private:
		// Binary glTF files can have embedded data after JSON.
		// This will be empty when reading from a text glTF (common) or a binary glTF with no BIN chunk (rare).
		TArray<uint8> BinData;
		// Extra binary data used for images from disk, mime data and so on.
		TArray<uint8> ExtraBinData;

		friend class FFileReader;
	};

	GLTFCORE_API const TCHAR* ToString(EExtension Extension);

}  // namespace GLTF
