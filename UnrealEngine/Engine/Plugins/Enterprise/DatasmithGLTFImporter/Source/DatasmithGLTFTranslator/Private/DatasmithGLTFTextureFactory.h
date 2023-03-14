// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include "GLTFMaterialFactory.h"

class IDatasmithScene;
namespace GLTF
{
	struct FTexture;
	struct FTextureMap;
	class ITextureElement;
}

class FDatasmithGLTFTextureFactory : public GLTF::ITextureFactory
{
public:
	IDatasmithScene* CurrentScene;

public:
	FDatasmithGLTFTextureFactory();
	virtual ~FDatasmithGLTFTextureFactory();

	virtual GLTF::ITextureElement* CreateTexture(const GLTF::FTexture& GltfTexture, UObject* ParentPackage, EObjectFlags Flags,
	                                             GLTF::ETextureMode TextureMode) override;

	virtual void CleanUp() override;

private:
	TMap<FString, TSharedPtr<GLTF::ITextureElement> > CreatedTextures;
};
