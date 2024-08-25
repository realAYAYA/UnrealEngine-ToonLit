// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimTypes.h"
#include "Camera/CameraTypes.h"
#include "Core/GLTFVector.h"
#include "Core/GLTFColor.h"
#include "Core/GLTFMatrix.h"
#include "Core/GLTFQuaternion.h"
#include "Json/GLTFJsonEnums.h"
#include "Engine/EngineTypes.h"
#include "Engine/Texture.h"
#include "PackedNormal.h"
#include "RHIDefinitions.h"
#include "SceneTypes.h"

struct GLTFEXPORTER_API FGLTFCoreUtilities
{
	static float ConvertLength(const float Length, const float ConversionScale = 0.01);

	static FGLTFVector3 ConvertVector(const FVector3f& Vector);

	static FGLTFVector3 ConvertPosition(const FVector3f& Position, const float ConversionScale = 0.01);

	static FGLTFVector3 ConvertScale(const FVector3f& Scale);

	static FGLTFVector3 ConvertNormal(const FVector3f& Normal);
	static FGLTFInt8Vector4 ConvertNormal(const FPackedNormal& Normal);
	static FGLTFInt16Vector4 ConvertNormal(const FPackedRGBA16N& Normal);

	static FGLTFVector4 ConvertTangent(const FVector3f& Tangent, const FVector4f& Normal = FVector4f(ForceInitToZero));
	static FGLTFInt8Vector4 ConvertTangent(const FPackedNormal& Tangent, const FPackedNormal& Normal = FPackedNormal());
	static FGLTFInt16Vector4 ConvertTangent(const FPackedRGBA16N& Tangent, const FPackedRGBA16N& Normal = FPackedRGBA16N());

	static FGLTFVector2 ConvertUV(const FVector2f& UV);
	static FGLTFVector2 ConvertUV(const FVector2DHalf& UV);

	static FGLTFColor4 ConvertColor(const FLinearColor& Color);
	static FGLTFColor3 ConvertColor3(const FLinearColor& Color);
	static FGLTFUInt8Color4 ConvertColor(const FColor& Color);

	static FGLTFQuaternion ConvertRotation(const FRotator3f& Rotation);
	static FGLTFQuaternion ConvertRotation(const FQuat4f& Rotation);

	static FGLTFMatrix4 ConvertMatrix(const FMatrix44f& Matrix);

	static FGLTFMatrix4 ConvertTransform(const FTransform3f& Transform, const float ConversionScale = 0.01);

	static float ConvertFieldOfView(float FOVInDegrees, float AspectRatio);

	static float ConvertLightAngle(const float Angle);

	static FGLTFQuaternion GetLocalCameraRotation();
	static FGLTFQuaternion GetLocalLightRotation();

	static EGLTFJsonCameraType ConvertCameraType(ECameraProjectionMode::Type ProjectionMode);
	static EGLTFJsonLightType ConvertLightType(ELightComponentType ComponentType);

	static EGLTFJsonInterpolation ConvertInterpolation(const EAnimInterpolationType Type);

	static EGLTFJsonShadingModel ConvertShadingModel(EMaterialShadingModel ShadingModel);
	static FString GetShadingModelString(EGLTFJsonShadingModel ShadingModel);

	static EGLTFJsonAlphaMode ConvertAlphaMode(EBlendMode Mode);

	static EGLTFJsonTextureWrap ConvertWrap(TextureAddress Address);

	static EGLTFJsonTextureFilter ConvertMinFilter(TextureFilter Filter);
	static EGLTFJsonTextureFilter ConvertMagFilter(TextureFilter Filter);

	template <typename ComponentType>
	static EGLTFJsonComponentType GetComponentType()
	{
		if constexpr (std::is_same_v<ComponentType, int8  >) return EGLTFJsonComponentType::Int8;
		else if constexpr (std::is_same_v<ComponentType, uint8 >) return EGLTFJsonComponentType::UInt8;
		else if constexpr (std::is_same_v<ComponentType, int16 >) return EGLTFJsonComponentType::Int16;
		else if constexpr (std::is_same_v<ComponentType, uint16>) return EGLTFJsonComponentType::UInt16;
		else if constexpr (std::is_same_v<ComponentType, int32 >) return EGLTFJsonComponentType::Int32;
		else if constexpr (std::is_same_v<ComponentType, uint32>) return EGLTFJsonComponentType::UInt32;
		else if constexpr (std::is_same_v<ComponentType, float >) return EGLTFJsonComponentType::Float;
		else return EGLTFJsonComponentType::None;
	}
};
