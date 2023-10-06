// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Builders/GLTFFileBuilder.h"
#include "Json/GLTFJsonRoot.h"

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
	FGLTFJsonLight* AddLight();
	FGLTFJsonMaterialVariant* AddMaterialVariant();

	const FGLTFJsonRoot& GetRoot() const;

protected:

	bool WriteJsonArchive(FArchive& Archive);

private:

	static FString GetGeneratorString();
	static FString GetCopyrightString();

	FGLTFJsonRoot JsonRoot;
};
