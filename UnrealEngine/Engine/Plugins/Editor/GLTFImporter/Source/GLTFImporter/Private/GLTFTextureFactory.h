// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/StrongObjectPtr.h"

#include "GLTFLogger.h"
#include "GLTFMaterialFactory.h"

class UTexture;
class UTextureFactory;

namespace GLTF
{
	struct FTexture;
	struct FTextureMap;
	class ITextureElement;
}

class FGLTFTextureFactory : public GLTF::ITextureFactory
{
public:
	FGLTFTextureFactory(TArray<GLTF::FLogMessage>& LogMessages);
	virtual ~FGLTFTextureFactory();

	virtual GLTF::ITextureElement* CreateTexture(const GLTF::FTexture& GltfTexture, UObject* ParentPackage, EObjectFlags Flags,
	                                             GLTF::ETextureMode TextureMode) override;

	virtual void CleanUp() override;

private:
	TArray<GLTF::FLogMessage>&                 LogMessages;
	TArray<TSharedPtr<GLTF::ITextureElement> > CreatedTextures;
	TStrongObjectPtr<UTextureFactory>          Factory;
};
