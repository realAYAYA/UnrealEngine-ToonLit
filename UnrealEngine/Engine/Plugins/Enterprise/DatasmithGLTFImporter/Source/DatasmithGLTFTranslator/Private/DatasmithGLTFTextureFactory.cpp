// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithGLTFTextureFactory.h"

#include "DatasmithGLTFMaterialElement.h"

#include "DatasmithSceneFactory.h"
#include "IDatasmithSceneElements.h"

#include "GLTFMaterial.h"
#include "GLTFTexture.h"

#include "EditorFramework/AssetImportData.h"
#include "Engine/Texture2D.h"

namespace DatasmithGLTFImporterImpl
{
	EDatasmithTextureFilter ConvertFilter(GLTF::FSampler::EFilter Filter)
	{
		switch (Filter)
		{
			case GLTF::FSampler::EFilter::Nearest:
				return EDatasmithTextureFilter::Nearest;
			case GLTF::FSampler::EFilter::LinearMipmapNearest:
				return EDatasmithTextureFilter::Bilinear;
			case GLTF::FSampler::EFilter::LinearMipmapLinear:
				return EDatasmithTextureFilter::Trilinear;
				// Other glTF filter values have no direct correlation to Unreal
			default:
				return EDatasmithTextureFilter::Default;
		}
	}

	EDatasmithTextureAddress ConvertWrap(GLTF::FSampler::EWrap Wrap)
	{
		switch (Wrap)
		{
			case GLTF::FSampler::EWrap::Repeat:
				return EDatasmithTextureAddress::Wrap;
			case GLTF::FSampler::EWrap::MirroredRepeat:
				return EDatasmithTextureAddress::Mirror;
			case GLTF::FSampler::EWrap::ClampToEdge:
				return EDatasmithTextureAddress::Clamp;

			default:
				return EDatasmithTextureAddress::Wrap;
		}
	}

	EDatasmithTextureFormat ConvertFormat(GLTF::FImage::EFormat Format)
	{
		switch (Format)
		{
			case GLTF::FImage::EFormat::JPEG:
				return EDatasmithTextureFormat::JPEG;
			case GLTF::FImage::EFormat::PNG:
				return EDatasmithTextureFormat::PNG;
			default:
				check(false);
				return EDatasmithTextureFormat::JPEG;
		}
	}

	EDatasmithTextureMode ConvertTextureMode(GLTF::ETextureMode Mode)
	{
		switch (Mode)
		{
			case GLTF::ETextureMode::Color:
				return EDatasmithTextureMode::Diffuse;
			case GLTF::ETextureMode::Grayscale:
				return EDatasmithTextureMode::Specular;
			case GLTF::ETextureMode::Normal:
				return EDatasmithTextureMode::Normal;
			default:
				check(false);
				return EDatasmithTextureMode::Diffuse;
		}
	}
}
FDatasmithGLTFTextureFactory::FDatasmithGLTFTextureFactory()
	: CurrentScene(nullptr)
{
}

FDatasmithGLTFTextureFactory::~FDatasmithGLTFTextureFactory()
{
	FDatasmithGLTFTextureFactory::CleanUp();
}

GLTF::ITextureElement* FDatasmithGLTFTextureFactory::CreateTexture(const GLTF::FTexture& GltfTexture, UObject* ParentPackage, EObjectFlags Flags,
                                                                   GLTF::ETextureMode TextureMode)
{
	using namespace DatasmithGLTFImporterImpl;

	if (GltfTexture.Name.IsEmpty())
	{
		return nullptr;
	}

	if (GltfTexture.Source.FilePath.IsEmpty() && GltfTexture.Source.DataByteLength == 0)
	{
		return nullptr;
	}

	FString TextureName = GltfTexture.Name;
	if (CreatedTextures.Contains(TextureName))
	{
		return CreatedTextures[TextureName].Get();
	}

	TSharedRef<IDatasmithTextureElement> Texture = FDatasmithSceneFactory::CreateTexture(*TextureName);
	Texture->SetTextureMode(ConvertTextureMode(TextureMode));
	Texture->SetTextureFilter(ConvertFilter(GltfTexture.Sampler.MinFilter));
	Texture->SetTextureAddressX(ConvertWrap(GltfTexture.Sampler.WrapS));
	Texture->SetTextureAddressY(ConvertWrap(GltfTexture.Sampler.WrapT));
	Texture->SetSRGB(TextureMode == GLTF::ETextureMode::Color ? EDatasmithColorSpace::sRGB : EDatasmithColorSpace::Linear);

	if (GltfTexture.Source.DataByteLength > 0)
	{
		Texture->SetData(GltfTexture.Source.Data, GltfTexture.Source.DataByteLength, ConvertFormat(GltfTexture.Source.Format));
	}
	else
	{
		Texture->SetFile(*GltfTexture.Source.FilePath);
		Texture->SetFileHash(FMD5Hash::HashFile(*GltfTexture.Source.FilePath));
	}

	CurrentScene->AddTexture(Texture);

	TSharedPtr<GLTF::ITextureElement> TextureElement(new FDatasmithGLTFTextureElement(Texture));
	CreatedTextures.Add(TextureName, TextureElement);
	return TextureElement.Get();
}

void FDatasmithGLTFTextureFactory::CleanUp()
{
	CreatedTextures.Empty();
}
