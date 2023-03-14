// Copyright Epic Games, Inc. All Rights Reserved.

#include "Json/GLTFJsonMaterial.h"
#include "Json/GLTFJsonTexture.h"

void FGLTFJsonTextureInfo::WriteObject(IGLTFJsonWriter& Writer) const
{
	Writer.Write(TEXT("index"), Index);

	if (TexCoord != 0)
	{
		Writer.Write(TEXT("texCoord"), TexCoord);
	}

	if (!Transform.IsNearlyDefault(Writer.DefaultTolerance))
	{
		Writer.StartExtensions();
		Writer.Write(EGLTFJsonExtension::KHR_TextureTransform, Transform);
		Writer.EndExtensions();
	}
}

void FGLTFJsonNormalTextureInfo::WriteObject(IGLTFJsonWriter& Writer) const
{
	Writer.Write(TEXT("index"), Index);

	if (TexCoord != 0)
	{
		Writer.Write(TEXT("texCoord"), TexCoord);
	}

	if (!FMath::IsNearlyEqual(Scale, 1, Writer.DefaultTolerance))
	{
		Writer.Write(TEXT("scale"), Scale);
	}
}

void FGLTFJsonOcclusionTextureInfo::WriteObject(IGLTFJsonWriter& Writer) const
{
	Writer.Write(TEXT("index"), Index);

	if (TexCoord != 0)
	{
		Writer.Write(TEXT("texCoord"), TexCoord);
	}

	if (!FMath::IsNearlyEqual(Strength, 1, Writer.DefaultTolerance))
	{
		Writer.Write(TEXT("strength"), Strength);
	}
}

void FGLTFJsonPBRMetallicRoughness::WriteObject(IGLTFJsonWriter& Writer) const
{
	if (!BaseColorFactor.IsNearlyEqual(FGLTFJsonColor4::White, Writer.DefaultTolerance))
	{
		Writer.Write(TEXT("baseColorFactor"), BaseColorFactor);
	}

	if (BaseColorTexture.Index != nullptr)
	{
		Writer.Write(TEXT("baseColorTexture"), BaseColorTexture);
	}

	if (!FMath::IsNearlyEqual(MetallicFactor, 1, Writer.DefaultTolerance))
	{
		Writer.Write(TEXT("metallicFactor"), MetallicFactor);
	}

	if (!FMath::IsNearlyEqual(RoughnessFactor, 1, Writer.DefaultTolerance))
	{
		Writer.Write(TEXT("roughnessFactor"), RoughnessFactor);
	}

	if (MetallicRoughnessTexture.Index != nullptr)
	{
		Writer.Write(TEXT("metallicRoughnessTexture"), MetallicRoughnessTexture);
	}
}

void FGLTFJsonClearCoatExtension::WriteObject(IGLTFJsonWriter& Writer) const
{
	if (!FMath::IsNearlyEqual(ClearCoatFactor, 0, Writer.DefaultTolerance))
	{
		Writer.Write(TEXT("clearcoatFactor"), ClearCoatFactor);
	}

	if (ClearCoatTexture.Index != nullptr)
	{
		Writer.Write(TEXT("clearcoatTexture"), ClearCoatTexture);
	}

	if (!FMath::IsNearlyEqual(ClearCoatRoughnessFactor, 0, Writer.DefaultTolerance))
	{
		Writer.Write(TEXT("clearcoatRoughnessFactor"), ClearCoatRoughnessFactor);
	}

	if (ClearCoatRoughnessTexture.Index != nullptr)
	{
		Writer.Write(TEXT("clearcoatRoughnessTexture"), ClearCoatRoughnessTexture);
	}

	if (ClearCoatNormalTexture.Index != nullptr)
	{
		Writer.Write(TEXT("clearcoatNormalTexture"), ClearCoatNormalTexture);
	}
}

void FGLTFJsonMaterial::WriteObject(IGLTFJsonWriter& Writer) const
{
	if (!Name.IsEmpty())
	{
		Writer.Write(TEXT("name"), Name);
	}

	if (ShadingModel != EGLTFJsonShadingModel::None)
	{
		Writer.Write(TEXT("pbrMetallicRoughness"), PBRMetallicRoughness);
	}

	if (NormalTexture.Index != nullptr)
	{
		Writer.Write(TEXT("normalTexture"), NormalTexture);
	}

	if (OcclusionTexture.Index != nullptr)
	{
		Writer.Write(TEXT("occlusionTexture"), OcclusionTexture);
	}

	if (EmissiveTexture.Index != nullptr)
	{
		Writer.Write(TEXT("emissiveTexture"), EmissiveTexture);
	}

	if (!EmissiveFactor.IsNearlyEqual(FGLTFJsonColor3::Black, Writer.DefaultTolerance))
	{
		Writer.Write(TEXT("emissiveFactor"), EmissiveFactor);
	}

	if (AlphaMode != EGLTFJsonAlphaMode::Opaque)
	{
		Writer.Write(TEXT("alphaMode"), AlphaMode);

		if (AlphaMode == EGLTFJsonAlphaMode::Mask && !FMath::IsNearlyEqual(AlphaCutoff, 0.5f, Writer.DefaultTolerance))
		{
			Writer.Write(TEXT("alphaCutoff"), AlphaCutoff);
		}
	}

	if (DoubleSided)
	{
		Writer.Write(TEXT("doubleSided"), DoubleSided);
	}

	if (BlendMode != EGLTFJsonBlendMode::None || ShadingModel == EGLTFJsonShadingModel::Unlit || ShadingModel == EGLTFJsonShadingModel::ClearCoat)
	{
		Writer.StartExtensions();

		if (BlendMode != EGLTFJsonBlendMode::None)
		{
			Writer.StartExtension(EGLTFJsonExtension::EPIC_BlendModes);
			Writer.Write(TEXT("blendMode"), BlendMode);
			Writer.EndExtension();
		}

		if (ShadingModel == EGLTFJsonShadingModel::Unlit)
		{
			Writer.StartExtension(EGLTFJsonExtension::KHR_MaterialsUnlit);
			// Write empty object
			Writer.EndExtension();
		}
		else if (ShadingModel == EGLTFJsonShadingModel::ClearCoat)
		{
			Writer.Write(EGLTFJsonExtension::KHR_MaterialsClearCoat, ClearCoat);
		}

		Writer.EndExtensions();
	}
}
