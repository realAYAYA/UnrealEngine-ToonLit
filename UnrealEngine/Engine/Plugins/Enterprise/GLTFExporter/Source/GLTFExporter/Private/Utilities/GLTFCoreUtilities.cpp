// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utilities/GLTFCoreUtilities.h"
#include "Converters/GLTFTextureUtility.h"

float FGLTFCoreUtilities::ConvertLength(const float Length, const float ConversionScale)
{
	return Length * ConversionScale;
}

FGLTFVector3 FGLTFCoreUtilities::ConvertVector(const FVector3f& Vector)
{
	// Unreal uses a left-handed coordinate system, with Z up.
	// glTF uses a right-handed coordinate system, with Y up.
	return { Vector.X, Vector.Z, Vector.Y };
}

FGLTFVector3 FGLTFCoreUtilities::ConvertPosition(const FVector3f& Position, const float ConversionScale)
{
	return ConvertVector(Position * ConversionScale);
}

FGLTFVector3 FGLTFCoreUtilities::ConvertScale(const FVector3f& Scale)
{
	return ConvertVector(Scale);
}

FGLTFVector3 FGLTFCoreUtilities::ConvertNormal(const FVector3f& Normal)
{
	return ConvertVector(Normal);
}

FGLTFInt16Vector4 FGLTFCoreUtilities::ConvertNormal(const FPackedRGBA16N& Normal)
{
	return { Normal.X, Normal.Z, Normal.Y, 0 };
}

FGLTFInt8Vector4 FGLTFCoreUtilities::ConvertNormal(const FPackedNormal& Normal)
{
	return { Normal.Vector.X, Normal.Vector.Z, Normal.Vector.Y, 0 };
}

FGLTFVector4 FGLTFCoreUtilities::ConvertTangent(const FVector3f& Tangent)
{
	// glTF stores tangent as Vec4, with W component indicating handedness of tangent basis.
	return { Tangent.X, Tangent.Z, Tangent.Y, 1.0f };
}

FGLTFInt16Vector4 FGLTFCoreUtilities::ConvertTangent(const FPackedRGBA16N& Tangent)
{
	return { Tangent.X, Tangent.Z, Tangent.Y, MAX_int16 /* = 1.0 */ };
}

FGLTFInt8Vector4 FGLTFCoreUtilities::ConvertTangent(const FPackedNormal& Tangent)
{
	return { Tangent.Vector.X, Tangent.Vector.Z, Tangent.Vector.Y, MAX_int8 /* = 1.0 */ };
}

FGLTFVector2 FGLTFCoreUtilities::ConvertUV(const FVector2f& UV)
{
	// No conversion actually needed, this is primarily for type-safety.
	return { UV.X, UV.Y };
}

FGLTFVector2 FGLTFCoreUtilities::ConvertUV(const FVector2DHalf& UV)
{
	return ConvertUV(FVector2f(UV));
}

FGLTFColor4 FGLTFCoreUtilities::ConvertColor(const FLinearColor& Color, bool bForceLDR)
{
	if (bForceLDR)
	{
		return {
			FMath::Clamp(Color.R, 0.0f, 1.0f),
			FMath::Clamp(Color.G, 0.0f, 1.0f),
			FMath::Clamp(Color.B, 0.0f, 1.0f),
			FMath::Clamp(Color.A, 0.0f, 1.0f)
		};
	}

	// Just make sure its non-negative (which can happen when using MakeFromColorTemperature).
	return {
		FMath::Max(Color.R, 0.0f),
		FMath::Max(Color.G, 0.0f),
		FMath::Max(Color.B, 0.0f),
		FMath::Max(Color.A, 0.0f)
	};
}

FGLTFColor3 FGLTFCoreUtilities::ConvertColor3(const FLinearColor& Color, bool bForceLDR)
{
	if (bForceLDR)
	{
		return {
			FMath::Clamp(Color.R, 0.0f, 1.0f),
			FMath::Clamp(Color.G, 0.0f, 1.0f),
			FMath::Clamp(Color.B, 0.0f, 1.0f)
		};
	}

	// Just make sure its non-negative (which can happen when using MakeFromColorTemperature).
	return {
		FMath::Max(Color.R, 0.0f),
		FMath::Max(Color.G, 0.0f),
		FMath::Max(Color.B, 0.0f)
	};
}

FGLTFUInt8Color4 FGLTFCoreUtilities::ConvertColor(const FColor& Color)
{
	// Unreal uses ABGR or ARGB depending on endianness.
	// glTF always uses RGBA independent of endianness.
	return { Color.R, Color.G, Color.B, Color.A };
}

FGLTFQuaternion FGLTFCoreUtilities::ConvertRotation(const FRotator3f& Rotation)
{
	return ConvertRotation(Rotation.Quaternion());
}

FGLTFQuaternion FGLTFCoreUtilities::ConvertRotation(const FQuat4f& Rotation)
{
	// Unreal uses a left-handed coordinate system, with Z up.
	// glTF uses a right-handed coordinate system, with Y up.
	// Rotation = (qX, qY, qZ, qW) = (sin(angle/2) * aX, sin(angle/2) * aY, sin(angle/2) * aZ, cons(angle/2))
	// where (aX, aY, aZ) - rotation axis, angle - rotation angle
	// Y swapped with Z between these coordinate systems
	// also, as handedness is changed rotation is inversed - hence negation
	// therefore glTFRotation = (-qX, -qZ, -qY, qw)

	const FQuat4f Normalized = Rotation.GetNormalized();
	return { -Normalized.X, -Normalized.Z, -Normalized.Y, Normalized.W };
}

FGLTFMatrix4 FGLTFCoreUtilities::ConvertMatrix(const FMatrix44f& Matrix)
{
	// Unreal stores matrix elements in row major order.
	// glTF stores matrix elements in column major order.

	FGLTFMatrix4 Result;
	for (int32 Row = 0; Row < 4; ++Row)
	{
		for (int32 Col = 0; Col < 4; ++Col)
		{
			Result(Row, Col) = Matrix.M[Row][Col];
		}
	}
	return Result;
}

FGLTFMatrix4 FGLTFCoreUtilities::ConvertTransform(const FTransform3f& Transform, const float ConversionScale)
{
	const FQuat4f Rotation = Transform.GetRotation();
	const FVector3f Translation = Transform.GetTranslation();
	const FVector3f Scale = Transform.GetScale3D();

	const FQuat4f ConvertedRotation(-Rotation.X, -Rotation.Z, -Rotation.Y, Rotation.W);
	const FVector3f ConvertedTranslation = FVector3f(Translation.X, Translation.Z, Translation.Y) * ConversionScale;
	const FVector3f ConvertedScale(Scale.X, Scale.Z, Scale.Y);

	const FTransform3f ConvertedTransform(ConvertedRotation, ConvertedTranslation, ConvertedScale);
	const FMatrix44f Matrix = ConvertedTransform.ToMatrixWithScale(); // TODO: should it be ToMatrixNoScale()?
	return ConvertMatrix(Matrix);
}

float FGLTFCoreUtilities::ConvertFieldOfView(float FOVInDegrees, float AspectRatio)
{
	const float HorizontalFOV = FMath::DegreesToRadians(FOVInDegrees);
	const float VerticalFOV = 2 * FMath::Atan(FMath::Tan(HorizontalFOV / 2) / AspectRatio);
	return VerticalFOV;
}

float FGLTFCoreUtilities::ConvertLightAngle(const float Angle)
{
	// Unreal uses degrees.
	// glTF uses radians.
	return FMath::DegreesToRadians(Angle);
}

FGLTFQuaternion FGLTFCoreUtilities::GetLocalCameraRotation()
{
	// Unreal uses +X axis as camera direction in Unreal coordinates.
	// glTF uses +Y as camera direction in Unreal coordinates.

	static FGLTFQuaternion Rotation = ConvertRotation(FRotator3f(0, 90, 0));
	return Rotation;
}

FGLTFQuaternion FGLTFCoreUtilities::GetLocalLightRotation()
{
	// Unreal uses +X axis as light direction in Unreal coordinates.
	// glTF uses +Y as light direction in Unreal coordinates.

	static FGLTFQuaternion Rotation = ConvertRotation(FRotator3f(0, 90, 0));
	return Rotation;
}

EGLTFJsonCameraType FGLTFCoreUtilities::ConvertCameraType(ECameraProjectionMode::Type ProjectionMode)
{
	switch (ProjectionMode)
	{
		case ECameraProjectionMode::Perspective:  return EGLTFJsonCameraType::Perspective;
		case ECameraProjectionMode::Orthographic: return EGLTFJsonCameraType::Orthographic;
		default:                                  return EGLTFJsonCameraType::None;
	}
}

EGLTFJsonLightType FGLTFCoreUtilities::ConvertLightType(ELightComponentType ComponentType)
{
	switch (ComponentType)
	{
		case LightType_Directional: return EGLTFJsonLightType::Directional;
		case LightType_Point:       return EGLTFJsonLightType::Point;
		case LightType_Spot:        return EGLTFJsonLightType::Spot;
		default:                    return EGLTFJsonLightType::None;
	}
}

EGLTFJsonInterpolation FGLTFCoreUtilities::ConvertInterpolation(const EAnimInterpolationType Type)
{
	switch (Type)
	{
		case EAnimInterpolationType::Linear: return EGLTFJsonInterpolation::Linear;
		case EAnimInterpolationType::Step:   return EGLTFJsonInterpolation::Step;
		default:                             return EGLTFJsonInterpolation::None;
	}
}

EGLTFJsonShadingModel FGLTFCoreUtilities::ConvertShadingModel(EMaterialShadingModel ShadingModel)
{
	switch (ShadingModel)
	{
		case MSM_Unlit:      return EGLTFJsonShadingModel::Unlit;
		case MSM_DefaultLit: return EGLTFJsonShadingModel::Default;
		case MSM_ClearCoat:  return EGLTFJsonShadingModel::ClearCoat;
		default:             return EGLTFJsonShadingModel::None;
	}
}

EGLTFJsonAlphaMode FGLTFCoreUtilities::ConvertAlphaMode(EBlendMode Mode)
{
	switch (Mode)
	{
		case BLEND_Opaque:         return EGLTFJsonAlphaMode::Opaque;
		case BLEND_Masked:         return EGLTFJsonAlphaMode::Mask;
		case BLEND_Translucent:    return EGLTFJsonAlphaMode::Blend;
		case BLEND_Additive:       return EGLTFJsonAlphaMode::Blend;
		case BLEND_Modulate:       return EGLTFJsonAlphaMode::Blend;
		case BLEND_AlphaComposite: return EGLTFJsonAlphaMode::Blend;
		default:                   return EGLTFJsonAlphaMode::None;
	}
}

EGLTFJsonBlendMode FGLTFCoreUtilities::ConvertBlendMode(EBlendMode Mode)
{
	switch (Mode)
	{
		case BLEND_Additive:       return EGLTFJsonBlendMode::Additive;
		case BLEND_Modulate:       return EGLTFJsonBlendMode::Modulate;
		case BLEND_AlphaComposite: return EGLTFJsonBlendMode::AlphaComposite;
		default:                   return EGLTFJsonBlendMode::None;
	}
}

EGLTFJsonTextureWrap FGLTFCoreUtilities::ConvertWrap(TextureAddress Address)
{
	switch (Address)
	{
		case TA_Wrap:   return EGLTFJsonTextureWrap::Repeat;
		case TA_Mirror: return EGLTFJsonTextureWrap::MirroredRepeat;
		case TA_Clamp:  return EGLTFJsonTextureWrap::ClampToEdge;
		default:        return EGLTFJsonTextureWrap::None; // TODO: add error handling in callers
	}
}

EGLTFJsonTextureFilter FGLTFCoreUtilities::ConvertMinFilter(TextureFilter Filter)
{
	switch (Filter)
	{
		case TF_Nearest:   return EGLTFJsonTextureFilter::NearestMipmapNearest;
		case TF_Bilinear:  return EGLTFJsonTextureFilter::LinearMipmapNearest;
		case TF_Trilinear: return EGLTFJsonTextureFilter::LinearMipmapLinear;
		default:           return EGLTFJsonTextureFilter::None;
	}
}

EGLTFJsonTextureFilter FGLTFCoreUtilities::ConvertMagFilter(TextureFilter Filter)
{
	switch (Filter)
	{
		case TF_Nearest:   return EGLTFJsonTextureFilter::Nearest;
		case TF_Bilinear:  return EGLTFJsonTextureFilter::Linear;
		case TF_Trilinear: return EGLTFJsonTextureFilter::Linear;
		default:           return EGLTFJsonTextureFilter::None;
	}
}

EGLTFJsonTextureFilter FGLTFCoreUtilities::ConvertMinFilter(TextureFilter Filter, TextureGroup LODGroup)
{
	return ConvertMinFilter(Filter == TF_Default ? FGLTFTextureUtility::GetDefaultFilter(LODGroup) : Filter);
}

EGLTFJsonTextureFilter FGLTFCoreUtilities::ConvertMagFilter(TextureFilter Filter, TextureGroup LODGroup)
{
	return ConvertMagFilter(Filter == TF_Default ? FGLTFTextureUtility::GetDefaultFilter(LODGroup) : Filter);
}

EGLTFJsonCubeFace FGLTFCoreUtilities::ConvertCubeFace(ECubeFace CubeFace)
{
	switch (CubeFace)
	{
		case CubeFace_PosX:	return EGLTFJsonCubeFace::NegX;
		case CubeFace_NegX:	return EGLTFJsonCubeFace::PosX;
		case CubeFace_PosY:	return EGLTFJsonCubeFace::PosZ;
		case CubeFace_NegY:	return EGLTFJsonCubeFace::NegZ;
		case CubeFace_PosZ:	return EGLTFJsonCubeFace::PosY;
		case CubeFace_NegZ:	return EGLTFJsonCubeFace::NegY;
		default:            return EGLTFJsonCubeFace::None; // TODO: add error handling in callers
	}
}
