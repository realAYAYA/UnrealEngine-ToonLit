// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "Converters/GLTFMaterialProxyFactory.h"
#include "Converters/GLTFMaterialUtility.h"
#include "Converters/GLTFImageUtility.h"
#include "Utilities/GLTFProxyMaterialUtilities.h"
#include "Materials/GLTFProxyMaterialInfo.h"
#include "Materials/MaterialInstanceConstant.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"

FGLTFMaterialProxyFactory::FGLTFMaterialProxyFactory(const UGLTFProxyOptions* Options)
	: Builder(TEXT(""), CreateExportOptions(Options))
{
	Builder.Texture2DConverter = CreateTextureConverter();
	Builder.ImageConverter = CreateImageConverter();
}

UMaterialInterface* FGLTFMaterialProxyFactory::Create(UMaterialInterface* OriginalMaterial)
{
	if (FGLTFProxyMaterialUtilities::IsProxyMaterial(OriginalMaterial))
	{
		return OriginalMaterial;
	}

	if (FGLTFMaterialUtility::NeedsMeshData(OriginalMaterial))
	{
		Builder.LogWarning(FString::Printf(
			TEXT("Material %s uses mesh data but prebaking will only use a simple quad as mesh data currently"),
			*OriginalMaterial->GetName()));
	}

	FGLTFJsonMaterial* JsonMaterial = Builder.AddUniqueMaterial(OriginalMaterial);
	if (JsonMaterial == nullptr)
	{
		// TODO: report error
		return nullptr;
	}

	MakeDirectory(RootPath);
	Builder.ProcessSlowTasks();

	UMaterialInstanceConstant* ProxyMaterial = CreateInstancedMaterial(OriginalMaterial, JsonMaterial->ShadingModel);
	if (ProxyMaterial != nullptr)
	{
		FGLTFProxyMaterialUtilities::SetProxyMaterial(OriginalMaterial, ProxyMaterial);
		SetBaseProperties(ProxyMaterial, OriginalMaterial);
		SetProxyParameters(ProxyMaterial, *JsonMaterial);
	}

	return ProxyMaterial;
}

void FGLTFMaterialProxyFactory::OpenLog()
{
	if (Builder.HasLoggedMessages())
	{
		Builder.OpenLog();
	}
}

void FGLTFMaterialProxyFactory::SetBaseProperties(UMaterialInstanceConstant* ProxyMaterial, UMaterialInterface* OriginalMaterial)
{
	const UMaterial* BaseMaterial = ProxyMaterial->GetMaterial();

	const bool bTwoSided = OriginalMaterial->IsTwoSided();
	if (bTwoSided != BaseMaterial->IsTwoSided())
	{
		ProxyMaterial->BasePropertyOverrides.bOverride_TwoSided = true;
		ProxyMaterial->BasePropertyOverrides.TwoSided = bTwoSided;
	}

	const EBlendMode BlendMode = OriginalMaterial->GetBlendMode();
	if (BlendMode != BaseMaterial->GetBlendMode())
	{
		ProxyMaterial->BasePropertyOverrides.bOverride_BlendMode = true;
		ProxyMaterial->BasePropertyOverrides.BlendMode = BlendMode;
	}

	const float OpacityMaskClipValue = OriginalMaterial->GetOpacityMaskClipValue();
	if (OpacityMaskClipValue != BaseMaterial->GetOpacityMaskClipValue())
	{
		ProxyMaterial->BasePropertyOverrides.bOverride_OpacityMaskClipValue = true;
		ProxyMaterial->BasePropertyOverrides.OpacityMaskClipValue = OpacityMaskClipValue;
	}
}

void FGLTFMaterialProxyFactory::SetProxyParameters(UMaterialInstanceConstant* ProxyMaterial, const FGLTFJsonMaterial& JsonMaterial)
{
	SetProxyParameter(ProxyMaterial, FGLTFProxyMaterialInfo::BaseColorFactor, JsonMaterial.PBRMetallicRoughness.BaseColorFactor);
	SetProxyParameter(ProxyMaterial, FGLTFProxyMaterialInfo::BaseColor, JsonMaterial.PBRMetallicRoughness.BaseColorTexture);

	if (JsonMaterial.ShadingModel == EGLTFJsonShadingModel::Default || JsonMaterial.ShadingModel == EGLTFJsonShadingModel::ClearCoat)
	{
		SetProxyParameter(ProxyMaterial, FGLTFProxyMaterialInfo::EmissiveFactor, JsonMaterial.EmissiveFactor);
		SetProxyParameter(ProxyMaterial, FGLTFProxyMaterialInfo::Emissive, JsonMaterial.EmissiveTexture);

		SetProxyParameter(ProxyMaterial, FGLTFProxyMaterialInfo::MetallicFactor, JsonMaterial.PBRMetallicRoughness.MetallicFactor);
		SetProxyParameter(ProxyMaterial, FGLTFProxyMaterialInfo::RoughnessFactor, JsonMaterial.PBRMetallicRoughness.RoughnessFactor);
		SetProxyParameter(ProxyMaterial, FGLTFProxyMaterialInfo::MetallicRoughness, JsonMaterial.PBRMetallicRoughness.MetallicRoughnessTexture);

		SetProxyParameter(ProxyMaterial, FGLTFProxyMaterialInfo::NormalScale, JsonMaterial.NormalTexture.Scale);
		SetProxyParameter(ProxyMaterial, FGLTFProxyMaterialInfo::Normal, JsonMaterial.NormalTexture);

		SetProxyParameter(ProxyMaterial, FGLTFProxyMaterialInfo::OcclusionStrength, JsonMaterial.OcclusionTexture.Strength);
		SetProxyParameter(ProxyMaterial, FGLTFProxyMaterialInfo::Occlusion, JsonMaterial.OcclusionTexture);

		if (JsonMaterial.ShadingModel == EGLTFJsonShadingModel::ClearCoat)
		{
			SetProxyParameter(ProxyMaterial, FGLTFProxyMaterialInfo::ClearCoatFactor, JsonMaterial.ClearCoat.ClearCoatFactor);
			SetProxyParameter(ProxyMaterial, FGLTFProxyMaterialInfo::ClearCoat, JsonMaterial.ClearCoat.ClearCoatTexture);

			SetProxyParameter(ProxyMaterial, FGLTFProxyMaterialInfo::ClearCoatRoughnessFactor, JsonMaterial.ClearCoat.ClearCoatRoughnessFactor);
			SetProxyParameter(ProxyMaterial, FGLTFProxyMaterialInfo::ClearCoatRoughness, JsonMaterial.ClearCoat.ClearCoatRoughnessTexture);

			SetProxyParameter(ProxyMaterial, FGLTFProxyMaterialInfo::ClearCoatNormalScale, JsonMaterial.ClearCoat.ClearCoatNormalTexture.Scale);
			SetProxyParameter(ProxyMaterial, FGLTFProxyMaterialInfo::ClearCoatNormal, JsonMaterial.ClearCoat.ClearCoatNormalTexture);
		}
	}
}

void FGLTFMaterialProxyFactory::SetProxyParameter(UMaterialInstanceConstant* ProxyMaterial, const TGLTFProxyMaterialParameterInfo<float>& ParameterInfo, float Scalar)
{
	ParameterInfo.Set(ProxyMaterial, Scalar, true);
}

void FGLTFMaterialProxyFactory::SetProxyParameter(UMaterialInstanceConstant* ProxyMaterial, const TGLTFProxyMaterialParameterInfo<FLinearColor>& ParameterInfo, const FGLTFJsonColor3& Color)
{
	ParameterInfo.Set(ProxyMaterial, FLinearColor(Color.R, Color.G, Color.B), true);
}

void FGLTFMaterialProxyFactory::SetProxyParameter(UMaterialInstanceConstant* ProxyMaterial, const TGLTFProxyMaterialParameterInfo<FLinearColor>& ParameterInfo, const FGLTFJsonColor4& Color)
{
	ParameterInfo.Set(ProxyMaterial, FLinearColor(Color.R, Color.G, Color.B, Color.A), true);
}

void FGLTFMaterialProxyFactory::SetProxyParameter(UMaterialInstanceConstant* ProxyMaterial, const FGLTFProxyMaterialTextureParameterInfo& ParameterInfo, const FGLTFJsonTextureInfo& TextureInfo)
{
	UTexture* Texture = FindOrCreateTexture(TextureInfo.Index, ParameterInfo);
	if (Texture == nullptr)
	{
		return;
	}

	ParameterInfo.Texture.Set(ProxyMaterial, Texture, true);
	ParameterInfo.UVIndex.Set(ProxyMaterial, TextureInfo.TexCoord, true);
	ParameterInfo.UVOffset.Set(ProxyMaterial, FLinearColor(TextureInfo.Transform.Offset.X, TextureInfo.Transform.Offset.Y, 0, 0), true);
	ParameterInfo.UVScale.Set(ProxyMaterial, FLinearColor(TextureInfo.Transform.Scale.X, TextureInfo.Transform.Scale.Y, 0, 0), true);
	ParameterInfo.UVRotation.Set(ProxyMaterial, TextureInfo.Transform.Rotation, true);
}

UTexture2D* FGLTFMaterialProxyFactory::FindOrCreateTexture(FGLTFJsonTexture* JsonTexture, const FGLTFProxyMaterialTextureParameterInfo& ParameterInfo)
{
	if (JsonTexture == nullptr)
	{
		return nullptr;
	}

	// TODO: fix potential conflict when same texture used for different material properties that have different encoding (sRGB vs Linear, Normalmap compression etc)

	UTexture2D** FoundPtr = Textures.Find(JsonTexture);
	if (FoundPtr != nullptr)
	{
		return *FoundPtr;
	}

	const FGLTFImageData* ImageData = Images.Find(JsonTexture->Source);
	if (ImageData == nullptr)
	{
		// TODO: report error
		return nullptr;
	}

	const FGLTFJsonSampler& JsonSampler = *JsonTexture->Sampler;
	UTexture2D* Texture = CreateTexture(ImageData, JsonSampler, ParameterInfo);
	Textures.Add(JsonTexture, Texture);
	return Texture;
}

UTexture2D* FGLTFMaterialProxyFactory::CreateTexture(const FGLTFImageData* ImageData, const FGLTFJsonSampler& JsonSampler, const FGLTFProxyMaterialTextureParameterInfo& ParameterInfo)
{
	const bool bSRGB = ParameterInfo == FGLTFProxyMaterialInfo::BaseColor || ParameterInfo == FGLTFProxyMaterialInfo::Emissive;
	const bool bNormalMap = ParameterInfo == FGLTFProxyMaterialInfo::Normal || ParameterInfo == FGLTFProxyMaterialInfo::ClearCoatNormal;

	FCreateTexture2DParameters TexParams;
	TexParams.bUseAlpha = !ImageData->bIgnoreAlpha;
	TexParams.CompressionSettings = bNormalMap ? TC_Normalmap : TC_Default;
	TexParams.bDeferCompression = true;
	TexParams.bSRGB = bSRGB;
	TexParams.TextureGroup = bNormalMap ? TEXTUREGROUP_WorldNormalMap : TEXTUREGROUP_World;
	TexParams.SourceGuidHash = FGuid();

	const FString BaseName = TEXT("T_GLTF_") + ImageData->Filename;
	UPackage* Package = FindOrCreatePackage(BaseName);

	UTexture2D* Texture = FImageUtils::CreateTexture2D(ImageData->Size.X, ImageData->Size.Y, *ImageData->Pixels, Package, BaseName, RF_Public | RF_Standalone, TexParams);
	Texture->Filter = ConvertFilter(JsonSampler.MagFilter);
	Texture->AddressX = ConvertWrap(JsonSampler.WrapS);
	Texture->AddressY = ConvertWrap(JsonSampler.WrapT);
	return Texture;
}

UMaterialInstanceConstant* FGLTFMaterialProxyFactory::CreateInstancedMaterial(UMaterialInterface* OriginalMaterial, EGLTFJsonShadingModel ShadingModel)
{
	const FString BaseName = TEXT("MI_GLTF_") + OriginalMaterial->GetName();
	UPackage* Package = FindOrCreatePackage(BaseName);
	return FGLTFProxyMaterialUtilities::CreateProxyMaterial(ShadingModel, Package, *BaseName, RF_Public | RF_Standalone);
}

UPackage* FGLTFMaterialProxyFactory::FindOrCreatePackage(const FString& BaseName)
{
	const FString PackageName = RootPath / BaseName;
	UPackage* Package = CreatePackage(*PackageName);
	Package->FullyLoad();
	Package->Modify();
	// TODO: do we need to delete any old assets in the package?
	return Package;
}

TUniquePtr<IGLTFTexture2DConverter> FGLTFMaterialProxyFactory::CreateTextureConverter()
{
	class FGLTFCustomTexture2DConverter final: public IGLTFTexture2DConverter
	{
	public:

		FGLTFMaterialProxyFactory& Factory;

		FGLTFCustomTexture2DConverter(FGLTFMaterialProxyFactory& Factory)
			: Factory(Factory)
		{
		}

	protected:

		virtual void Sanitize(const UTexture2D*& Texture2D, bool& bToSRGB) override
		{
			bToSRGB = false; // ignore
		}

		virtual FGLTFJsonTexture* Convert(const UTexture2D* Texture2D, bool bToSRGB) override
		{
			FGLTFJsonTexture* Texture = Factory.Builder.AddTexture();
			Factory.Textures.Add(Texture, const_cast<UTexture2D*>(Texture2D));
			return Texture;
		}
	};

	return MakeUnique<FGLTFCustomTexture2DConverter>(*this);
}

TUniquePtr<IGLTFImageConverter> FGLTFMaterialProxyFactory::CreateImageConverter()
{
	class FGLTFCustomImageConverter final: public IGLTFImageConverter
	{
	public:

		FGLTFMaterialProxyFactory& Factory;
		TSet<FString> UniqueFilenames;

		FGLTFCustomImageConverter(FGLTFMaterialProxyFactory& Factory)
			: Factory(Factory)
		{
		}

	protected:

		virtual FGLTFJsonImage* Convert(TGLTFSuperfluous<FString> Name, EGLTFTextureType Type, bool bIgnoreAlpha, FIntPoint Size, TGLTFSharedArray<FColor> Pixels) override
		{
			const FString Filename = FGLTFImageUtility::GetUniqueFilename(Name, TEXT(""), UniqueFilenames);
			UniqueFilenames.Add(Filename);

			FGLTFJsonImage* Image = Factory.Builder.AddImage();
			Factory.Images.Add(Image, { Filename, Type, bIgnoreAlpha, Size, Pixels });
			return Image;
		}
	};

	return MakeUnique<FGLTFCustomImageConverter>(*this);
}

bool FGLTFMaterialProxyFactory::MakeDirectory(const FString& PackagePath)
{
	const FString DirPath = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(PackagePath + TEXT("/")));
	if (DirPath.IsEmpty())
	{
		return false;
	}

	bool bResult = true;
	if (!IFileManager::Get().DirectoryExists(*DirPath))
	{
		bResult = IFileManager::Get().MakeDirectory(*DirPath, true);
	}

	if (bResult)
	{
		const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		AssetRegistryModule.Get().AddPath(PackagePath);
	}

	return bResult;
}

UGLTFExportOptions* FGLTFMaterialProxyFactory::CreateExportOptions(const UGLTFProxyOptions* ProxyOptions)
{
	UGLTFExportOptions* ExportOptions = NewObject<UGLTFExportOptions>();
	ExportOptions->ResetToDefault();
	ExportOptions->bExportProxyMaterials = false;
	ExportOptions->bExportExtraBlendModes = true;
	ExportOptions->BakeMaterialInputs = ProxyOptions->bBakeMaterialInputs ? EGLTFMaterialBakeMode::Simple : EGLTFMaterialBakeMode::Disabled;
	ExportOptions->DefaultMaterialBakeSize = ProxyOptions->DefaultMaterialBakeSize;
	ExportOptions->DefaultMaterialBakeFilter = ProxyOptions->DefaultMaterialBakeFilter;
	ExportOptions->DefaultMaterialBakeTiling = ProxyOptions->DefaultMaterialBakeTiling;
	ExportOptions->DefaultInputBakeSettings = ProxyOptions->DefaultInputBakeSettings;
	ExportOptions->bAdjustNormalmaps = false;
	return ExportOptions;
}

TextureAddress FGLTFMaterialProxyFactory::ConvertWrap(EGLTFJsonTextureWrap Wrap)
{
	switch (Wrap)
	{
		case EGLTFJsonTextureWrap::Repeat:         return TA_Wrap;
		case EGLTFJsonTextureWrap::MirroredRepeat: return TA_Mirror;
		case EGLTFJsonTextureWrap::ClampToEdge:    return TA_Clamp;
		default:
			checkNoEntry();
			return TA_MAX;
	}
}

TextureFilter FGLTFMaterialProxyFactory::ConvertFilter(EGLTFJsonTextureFilter Filter)
{
	switch (Filter)
	{
		case EGLTFJsonTextureFilter::Nearest:              return TF_Nearest;
		case EGLTFJsonTextureFilter::NearestMipmapNearest: return TF_Nearest;
		case EGLTFJsonTextureFilter::LinearMipmapNearest:  return TF_Bilinear;
		case EGLTFJsonTextureFilter::NearestMipmapLinear:  return TF_Bilinear;
		case EGLTFJsonTextureFilter::Linear:               return TF_Trilinear;
		case EGLTFJsonTextureFilter::LinearMipmapLinear:   return TF_Trilinear;
		default:
			checkNoEntry();
			return TF_MAX;
	}
}

#endif
