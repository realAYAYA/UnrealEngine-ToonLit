// Copyright Epic Games, Inc. All Rights Reserved.

#include "GLTFTextureFactory.h"

#include "GLTFMaterial.h"
#include "GLTFMaterialElement.h"
#include "GLTFTexture.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/Texture2D.h"
#include "Factories/TextureFactory.h"
#include "ObjectTools.h"
#include "PackageTools.h"


namespace GLTFImporterImpl
{
	TextureFilter ConvertFilter(GLTF::FSampler::EFilter Filter)
	{
		switch (Filter)
		{
			case GLTF::FSampler::EFilter::Nearest:
				return TextureFilter::TF_Nearest;
			case GLTF::FSampler::EFilter::LinearMipmapNearest:
				return TextureFilter::TF_Bilinear;
			case GLTF::FSampler::EFilter::LinearMipmapLinear:
				return TextureFilter::TF_Trilinear;
				// Other glTF filter values have no direct correlation to Unreal
			default:
				return TextureFilter::TF_Default;
		}
	}

	TextureAddress ConvertWrap(GLTF::FSampler::EWrap Wrap)
	{
		switch (Wrap)
		{
			case GLTF::FSampler::EWrap::Repeat:
				return TextureAddress::TA_Wrap;
			case GLTF::FSampler::EWrap::MirroredRepeat:
				return TextureAddress::TA_Mirror;
			case GLTF::FSampler::EWrap::ClampToEdge:
				return TextureAddress::TA_Clamp;

			default:
				return TextureAddress::TA_Wrap;
		}
	}

	TextureGroup ConvertGroup(GLTF::ETextureMode TextureMode)
	{
		switch (TextureMode)
		{
			case GLTF::ETextureMode::Color:
				return TextureGroup::TEXTUREGROUP_World;
			case GLTF::ETextureMode::Grayscale:
				return TextureGroup::TEXTUREGROUP_WorldSpecular;
			case GLTF::ETextureMode::Normal:
				return TextureGroup::TEXTUREGROUP_WorldNormalMap;
			default:
				return TextureGroup::TEXTUREGROUP_World;
		}
	}

	const TCHAR* ConvertExtension(GLTF::FImage::EFormat TextureFormat)
	{
		switch (TextureFormat)
		{
			case GLTF::FImage::EFormat::PNG:
				return TEXT("png");
				break;
			case GLTF::FImage::EFormat::JPEG:
				return TEXT("jpeg");
				break;
			default:
				check(false);
				return TEXT("unknown");
		}
	}
}
FGLTFTextureFactory::FGLTFTextureFactory(TArray<GLTF::FLogMessage>& LogMessages)
    : LogMessages(LogMessages)
    , Factory(NewObject<UTextureFactory>())
{
}

FGLTFTextureFactory::~FGLTFTextureFactory()
{
	CleanUp();
}

GLTF::ITextureElement* FGLTFTextureFactory::CreateTexture(const GLTF::FTexture& GltfTexture, UObject* ParentPackage, EObjectFlags Flags,
                                                          GLTF::ETextureMode TextureMode)
{
	using namespace GLTFImporterImpl;

	const FString TextureName = ObjectTools::SanitizeObjectName(FPaths::GetBaseFilename(GltfTexture.Name));
	if (TextureName.IsEmpty())
	{
		return nullptr;
	}

	// save texture settings if texture exists
	Factory->SuppressImportOverwriteDialog();

	const FString PackageName  = UPackageTools::SanitizePackageName(FPaths::Combine(ParentPackage->GetName(), TEXT("Textures"), TextureName));
	UPackage*     AssetPackage = CreatePackage(*PackageName);

	UTexture2D* Texture = nullptr;
	if (!FPaths::GetExtension(GltfTexture.Source.FilePath).IsEmpty())
	{
		check(GltfTexture.Source.Data == nullptr);
		bool bOperationCanceled = false;
		Texture                 = static_cast<UTexture2D*>(Factory->FactoryCreateFile(UTexture2D::StaticClass(), AssetPackage, *TextureName, Flags,
                                                                      GltfTexture.Source.FilePath, nullptr, nullptr, bOperationCanceled));
		if (Texture)
		{
			Texture->AssetImportData->Update(GltfTexture.Source.FilePath);
		}
	}
	else if (GltfTexture.Source.Format != GLTF::FImage::EFormat::Unknown)
	{
		check(GltfTexture.Source.Data != nullptr);

		const uint8* PtrTextureData    = GltfTexture.Source.Data;
		const uint8* PtrTextureDataEnd = PtrTextureData + GltfTexture.Source.DataByteLength;
		Texture = (UTexture2D*)Factory->FactoryCreateBinary(UTexture2D::StaticClass(), AssetPackage, *TextureName, Flags, nullptr,
		                                                    ConvertExtension(GltfTexture.Source.Format), PtrTextureData, PtrTextureDataEnd, nullptr);
	}

	if (Texture)
	{
		TextureMipGenSettings MipGenSettings = TMGS_FromTextureGroup;

		if (!Texture->Source.IsPowerOfTwo())
		{
			MipGenSettings = TMGS_NoMipmaps;

			LogMessages.Emplace(GLTF::EMessageSeverity::Warning,
			                    FString::Printf(TEXT("Texture %s does not have power of two dimensions and therefore no mipmaps will be generated"),
			                                    *Texture->GetName()));
		}

		const GLTF::FSampler& Sampler = GltfTexture.Sampler;
		Texture->MipGenSettings       = MipGenSettings;
		Texture->CompressionNoAlpha   = false;
		Texture->CompressionSettings  = TextureCompressionSettings::TC_Default;
		Texture->Filter               = ConvertFilter(Sampler.MinFilter);
		Texture->AddressY             = ConvertWrap(Sampler.WrapS);
		Texture->AddressX             = ConvertWrap(Sampler.WrapT);
		Texture->LODGroup             = ConvertGroup(TextureMode);
		Texture->SRGB                 = TextureMode == GLTF::ETextureMode::Color;
		Texture->bFlipGreenChannel    = false;
		Texture->UpdateResource();
		Texture->PostEditChange();
		Texture->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(Texture);
	}

	TSharedPtr<GLTF::ITextureElement> TextureElement(new FGLTFTextureElement(*Texture));
	CreatedTextures.Add(TextureElement);
	return TextureElement.Get();
}

void FGLTFTextureFactory::CleanUp()
{
	CreatedTextures.Empty();
}
