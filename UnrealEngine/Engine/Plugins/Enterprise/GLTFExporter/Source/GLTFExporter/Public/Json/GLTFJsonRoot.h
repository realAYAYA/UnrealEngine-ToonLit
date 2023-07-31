// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonCore.h"
#include "Json/GLTFJsonAsset.h"
#include "Json/GLTFJsonAccessor.h"
#include "Json/GLTFJsonAnimation.h"
#include "Json/GLTFJsonBuffer.h"
#include "Json/GLTFJsonBufferView.h"
#include "Json/GLTFJsonCamera.h"
#include "Json/GLTFJsonImage.h"
#include "Json/GLTFJsonMaterial.h"
#include "Json/GLTFJsonMesh.h"
#include "Json/GLTFJsonNode.h"
#include "Json/GLTFJsonSampler.h"
#include "Json/GLTFJsonScene.h"
#include "Json/GLTFJsonSkin.h"
#include "Json/GLTFJsonTexture.h"
#include "Json/GLTFJsonBackdrop.h"
#include "Json/GLTFJsonLight.h"
#include "Json/GLTFJsonLightMap.h"
#include "Json/GLTFJsonSkySphere.h"
#include "Json/GLTFJsonEpicLevelVariantSets.h"
#include "Json/GLTFJsonKhrMaterialVariant.h"

struct GLTFEXPORTER_API FGLTFJsonRoot : IGLTFJsonObject
{
	FGLTFJsonAsset Asset;

	FGLTFJsonExtensions Extensions;

	FGLTFJsonScene* DefaultScene;

	TGLTFJsonIndexedObjectArray<FGLTFJsonAccessor>   Accessors;
	TGLTFJsonIndexedObjectArray<FGLTFJsonAnimation>  Animations;
	TGLTFJsonIndexedObjectArray<FGLTFJsonBuffer>     Buffers;
	TGLTFJsonIndexedObjectArray<FGLTFJsonBufferView> BufferViews;
	TGLTFJsonIndexedObjectArray<FGLTFJsonCamera>     Cameras;
	TGLTFJsonIndexedObjectArray<FGLTFJsonMaterial>   Materials;
	TGLTFJsonIndexedObjectArray<FGLTFJsonMesh>       Meshes;
	TGLTFJsonIndexedObjectArray<FGLTFJsonNode>       Nodes;
	TGLTFJsonIndexedObjectArray<FGLTFJsonImage>      Images;
	TGLTFJsonIndexedObjectArray<FGLTFJsonSampler>    Samplers;
	TGLTFJsonIndexedObjectArray<FGLTFJsonScene>      Scenes;
	TGLTFJsonIndexedObjectArray<FGLTFJsonSkin>       Skins;
	TGLTFJsonIndexedObjectArray<FGLTFJsonTexture>    Textures;
	TGLTFJsonIndexedObjectArray<FGLTFJsonBackdrop>   Backdrops;
	TGLTFJsonIndexedObjectArray<FGLTFJsonLight>      Lights;
	TGLTFJsonIndexedObjectArray<FGLTFJsonLightMap>   LightMaps;
	TGLTFJsonIndexedObjectArray<FGLTFJsonSkySphere>  SkySpheres;
	TGLTFJsonIndexedObjectArray<FGLTFJsonEpicLevelVariantSets> EpicLevelVariantSets;
	TGLTFJsonIndexedObjectArray<FGLTFJsonKhrMaterialVariant>   KhrMaterialVariants;

	FGLTFJsonRoot()
		: DefaultScene(nullptr)
	{
	}

	FGLTFJsonRoot(FGLTFJsonRoot&&) = default;
	FGLTFJsonRoot& operator=(FGLTFJsonRoot&&) = default;

	FGLTFJsonRoot(const FGLTFJsonRoot&) = delete;
	FGLTFJsonRoot& operator=(const FGLTFJsonRoot&) = delete;

	virtual void WriteObject(IGLTFJsonWriter& Writer) const override;

	void WriteJson(FArchive& Archive, bool bPrettyJson, float DefaultTolerance);
};
