// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utilities/GLTFJsonUtilities.h"

const TCHAR* FGLTFJsonUtilities::GetValue(EGLTFJsonExtension Enum)
{
	switch (Enum)
	{
		case EGLTFJsonExtension::KHR_LightsPunctual:      return TEXT("KHR_lights_punctual");
		case EGLTFJsonExtension::KHR_MaterialsClearCoat:  return TEXT("KHR_materials_clearcoat");
		case EGLTFJsonExtension::KHR_MaterialsUnlit:      return TEXT("KHR_materials_unlit");
		case EGLTFJsonExtension::KHR_MaterialsVariants:   return TEXT("KHR_materials_variants");
		case EGLTFJsonExtension::KHR_MeshQuantization:    return TEXT("KHR_mesh_quantization");
		case EGLTFJsonExtension::KHR_TextureTransform:    return TEXT("KHR_texture_transform");
		case EGLTFJsonExtension::EPIC_AnimationPlayback:  return TEXT("EPIC_animation_playback");
		case EGLTFJsonExtension::EPIC_BlendModes:         return TEXT("EPIC_blend_modes");
		case EGLTFJsonExtension::EPIC_HDRIBackdrops:      return TEXT("EPIC_hdri_backdrops");
		case EGLTFJsonExtension::EPIC_LevelVariantSets:   return TEXT("EPIC_level_variant_sets");
		case EGLTFJsonExtension::EPIC_LightmapTextures:   return TEXT("EPIC_lightmap_textures");
		case EGLTFJsonExtension::EPIC_SkySpheres:         return TEXT("EPIC_sky_spheres");
		case EGLTFJsonExtension::EPIC_TextureHDREncoding: return TEXT("EPIC_texture_hdr_encoding");
		default:
			checkNoEntry();
			return TEXT("");
	}
}

const TCHAR* FGLTFJsonUtilities::GetValue(EGLTFJsonAlphaMode Enum)
{
	switch (Enum)
	{
		case EGLTFJsonAlphaMode::Opaque: return TEXT("OPAQUE");
		case EGLTFJsonAlphaMode::Blend:  return TEXT("BLEND");
		case EGLTFJsonAlphaMode::Mask:   return TEXT("MASK");
		default:
			checkNoEntry();
			return TEXT("");
	}
}

const TCHAR* FGLTFJsonUtilities::GetValue(EGLTFJsonBlendMode Enum)
{
	switch (Enum)
	{
		case EGLTFJsonBlendMode::Additive:       return TEXT("ADDITIVE");
		case EGLTFJsonBlendMode::Modulate:       return TEXT("MODULATE");
		case EGLTFJsonBlendMode::AlphaComposite: return TEXT("ALPHACOMPOSITE");
		default:
			checkNoEntry();
			return TEXT("");
	}
}

const TCHAR* FGLTFJsonUtilities::GetValue(EGLTFJsonMimeType Enum)
{
	switch (Enum)
	{
		case EGLTFJsonMimeType::PNG:  return TEXT("image/png");
		case EGLTFJsonMimeType::JPEG: return TEXT("image/jpeg");
		default:
			checkNoEntry();
			return TEXT("");
	}
}

const TCHAR* FGLTFJsonUtilities::GetValue(EGLTFJsonAccessorType Enum)
{
	switch (Enum)
	{
		case EGLTFJsonAccessorType::Scalar: return TEXT("SCALAR");
		case EGLTFJsonAccessorType::Vec2:   return TEXT("VEC2");
		case EGLTFJsonAccessorType::Vec3:   return TEXT("VEC3");
		case EGLTFJsonAccessorType::Vec4:   return TEXT("VEC4");
		case EGLTFJsonAccessorType::Mat2:   return TEXT("MAT2");
		case EGLTFJsonAccessorType::Mat3:   return TEXT("MAT3");
		case EGLTFJsonAccessorType::Mat4:   return TEXT("MAT4");
		default:
			checkNoEntry();
			return TEXT("");
	}
}

const TCHAR* FGLTFJsonUtilities::GetValue(EGLTFJsonHDREncoding Enum)
{
	switch (Enum)
	{
		case EGLTFJsonHDREncoding::RGBM: return TEXT("RGBM");
		case EGLTFJsonHDREncoding::RGBE: return TEXT("RGBE");
		default:
			checkNoEntry();
			return TEXT("");
	}
}

const TCHAR* FGLTFJsonUtilities::GetValue(EGLTFJsonCubeFace Enum)
{
	switch (Enum)
	{
		case EGLTFJsonCubeFace::PosX: return TEXT("PosX");
		case EGLTFJsonCubeFace::NegX: return TEXT("NegX");
		case EGLTFJsonCubeFace::PosY: return TEXT("PosY");
		case EGLTFJsonCubeFace::NegY: return TEXT("NegY");
		case EGLTFJsonCubeFace::PosZ: return TEXT("PosZ");
		case EGLTFJsonCubeFace::NegZ: return TEXT("NegZ");
		default:
			checkNoEntry();
			return TEXT("");
	}
}

const TCHAR* FGLTFJsonUtilities::GetValue(EGLTFJsonCameraType Enum)
{
	switch (Enum)
	{
		case EGLTFJsonCameraType::Perspective:  return TEXT("perspective");
		case EGLTFJsonCameraType::Orthographic: return TEXT("orthographic");
		default:
			checkNoEntry();
			return TEXT("");
	}
}

const TCHAR* FGLTFJsonUtilities::GetValue(EGLTFJsonLightType Enum)
{
	switch (Enum)
	{
		case EGLTFJsonLightType::Directional: return TEXT("directional");
		case EGLTFJsonLightType::Point:       return TEXT("point");
		case EGLTFJsonLightType::Spot:        return TEXT("spot");
		default:
			checkNoEntry();
			return TEXT("");
	}
}

const TCHAR* FGLTFJsonUtilities::GetValue(EGLTFJsonInterpolation Enum)
{
	switch (Enum)
	{
		case EGLTFJsonInterpolation::Linear:      return TEXT("LINEAR");
		case EGLTFJsonInterpolation::Step:        return TEXT("STEP");
		case EGLTFJsonInterpolation::CubicSpline: return TEXT("CUBICSPLINE");
		default:
			checkNoEntry();
			return TEXT("");
	}
}

const TCHAR* FGLTFJsonUtilities::GetValue(EGLTFJsonTargetPath Enum)
{
	switch (Enum)
	{
		case EGLTFJsonTargetPath::Translation: return TEXT("translation");
		case EGLTFJsonTargetPath::Rotation:    return TEXT("rotation");
		case EGLTFJsonTargetPath::Scale:       return TEXT("scale");
		case EGLTFJsonTargetPath::Weights:     return TEXT("weights");
		default:
			checkNoEntry();
			return TEXT("");
	}
}

const TCHAR* FGLTFJsonUtilities::GetValue(EGLTFJsonShadingModel Enum)
{
	switch (Enum)
	{
		case EGLTFJsonShadingModel::Default:   return TEXT("Default");
		case EGLTFJsonShadingModel::Unlit:     return TEXT("Unlit");
		case EGLTFJsonShadingModel::ClearCoat: return TEXT("ClearCoat");
		default:
			checkNoEntry();
			return TEXT("");
	}
}
