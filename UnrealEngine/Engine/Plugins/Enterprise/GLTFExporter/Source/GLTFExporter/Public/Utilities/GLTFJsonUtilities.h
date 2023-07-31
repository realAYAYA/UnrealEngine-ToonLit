// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Json/GLTFJsonEnums.h"

struct GLTFEXPORTER_API FGLTFJsonUtilities
{
	template <typename EnumType, typename = typename TEnableIf<TIsEnum<EnumType>::Value>::Type>
	static int32 GetValue(EnumType Enum)
	{
		return static_cast<int32>(Enum);
	}

	static const TCHAR* GetValue(EGLTFJsonExtension Enum);
	static const TCHAR* GetValue(EGLTFJsonAlphaMode Enum);
	static const TCHAR* GetValue(EGLTFJsonBlendMode Enum);
	static const TCHAR* GetValue(EGLTFJsonMimeType Enum);
	static const TCHAR* GetValue(EGLTFJsonAccessorType Enum);
	static const TCHAR* GetValue(EGLTFJsonHDREncoding Enum);
	static const TCHAR* GetValue(EGLTFJsonCubeFace Enum);
	static const TCHAR* GetValue(EGLTFJsonCameraType Enum);
	static const TCHAR* GetValue(EGLTFJsonLightType Enum);
	static const TCHAR* GetValue(EGLTFJsonInterpolation Enum);
	static const TCHAR* GetValue(EGLTFJsonTargetPath Enum);
	static const TCHAR* GetValue(EGLTFJsonShadingModel Enum);
};
