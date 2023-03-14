// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GLTFLogger.h"

#include "CoreMinimal.h"

class FJsonObject;

namespace GLTF
{
	struct FAsset;
	struct FMesh;
	struct FTextureMap;
	struct FMaterial;
	class FBinaryFileReader;
	class FExtensionsHandler;

	class GLTFCORE_API FFileReader : public FBaseLogger
	{
	public:
		FFileReader();
		~FFileReader();

		/**
		 * Loads the contents of a glTF file into the given asset.
		 *
		 * @param InFilePath - disk file path
		 * @param bInLoadImageData - if false then doesn't loads the data field of FImage
		 * @param bInLoadMetadata - load extra asset metadata
		 * images from a disk path, images from data sources will always be loaded.
		 * @param OutAsset - asset data destination
		 */
		void ReadFile(const FString& InFilePath, bool bInLoadImageData, bool bInLoadMetadata, GLTF::FAsset& OutAsset);

	private:
		void LoadMetadata(GLTF::FAsset& Asset);
		void ImportAsset(const FString& FilePath, bool bInLoadImageData, GLTF::FAsset& Asset);
		void AllocateExtraData(const FString& InResourcesPath, bool bInLoadImageData, TArray<uint8>& OutExtraData);

		void SetupBuffer(const FJsonObject& Object, const FString& Path);
		void SetupBufferView(const FJsonObject& Object) const;
		void SetupAccessor(const FJsonObject& Object) const;
		void SetupPrimitive(const FJsonObject& Object, GLTF::FMesh& Mesh) const;
		void SetupMesh(const FJsonObject& Object) const;

		void SetupScene(const FJsonObject& Object) const;
		void SetupNode(const FJsonObject& Object) const;
		void SetupCamera(const FJsonObject& Object) const;
		void SetupAnimation(const FJsonObject& Object) const;
		void SetupSkin(const FJsonObject& Object) const;

		void SetupImage(const FJsonObject& Object, const FString& Path, bool bInLoadImageData);
		void SetupSampler(const FJsonObject& Object) const;
		void SetupTexture(const FJsonObject& Object) const;
		void SetupMaterial(const FJsonObject& Object) const;

		template <typename SetupFunc>
		void SetupObjects(uint32 ObjectCount, const TCHAR* FieldName, SetupFunc Func) const;
		void SetupNodesType() const;

		void BuildParentIndices(int32 ParentNodeIndex, int32 CurrentNodeIndex) const;
		int32 FindRootJointIndex(int32 CurrentNodeIndex) const;
		void BuildRootJoints() const;

	private:
		uint32 BufferCount;
		uint32 BufferViewCount;
		uint32 ImageCount;

		TSharedPtr<FJsonObject>  JsonRoot;
		FString                        JsonBuffer;
		TUniquePtr<FBinaryFileReader>  BinaryReader;
		TUniquePtr<FExtensionsHandler> ExtensionsHandler;

		// Each mesh has its own primitives array.
		// Each primitive refers to the asset's global accessors array.
		GLTF::FAsset* Asset;

		// Current offset for extra data buffer.
		uint8* CurrentBufferOffset;
	};

}  // namespace GLTF
