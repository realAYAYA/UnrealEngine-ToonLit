// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonRoot.h"
#include "Builders/GLTFFileBuilder.h"

class GLTFEXPORTER_API FGLTFJsonBuilder : public FGLTFFileBuilder
{
public:

	FGLTFJsonScene*& DefaultScene;

	FGLTFJsonBuilder(const FString& FileName, const UGLTFExportOptions* ExportOptions = nullptr);

	void AddExtension(EGLTFJsonExtension Extension, bool bIsRequired = false);

	FGLTFJsonAccessor* AddAccessor();
	FGLTFJsonAnimation* AddAnimation();
	FGLTFJsonBuffer* AddBuffer();
	FGLTFJsonBufferView* AddBufferView();
	FGLTFJsonCamera* AddCamera();
	FGLTFJsonImage* AddImage();
	FGLTFJsonMaterial* AddMaterial();
	FGLTFJsonMesh* AddMesh();
	FGLTFJsonNode* AddNode();
	FGLTFJsonSampler* AddSampler();
	FGLTFJsonScene* AddScene();
	FGLTFJsonSkin* AddSkin();
	FGLTFJsonTexture* AddTexture();
	FGLTFJsonBackdrop* AddBackdrop();
	FGLTFJsonLight* AddLight();
	FGLTFJsonLightMap* AddLightMap();
	FGLTFJsonSkySphere* AddSkySphere();
	FGLTFJsonEpicLevelVariantSets* AddEpicLevelVariantSets();
	FGLTFJsonKhrMaterialVariant* AddKhrMaterialVariant();

	const FGLTFJsonRoot& GetRoot() const;

protected:

	void WriteJsonArchive(FArchive& Archive);

	TSet<EGLTFJsonExtension> GetCustomExtensionsUsed() const;

private:

	FString GetGeneratorString() const;

	static bool IsCustomExtension(EGLTFJsonExtension Extension);

	FGLTFJsonRoot JsonRoot;
};
