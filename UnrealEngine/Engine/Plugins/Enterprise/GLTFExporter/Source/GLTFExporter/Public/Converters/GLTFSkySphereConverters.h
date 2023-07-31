// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core/GLTFColor.h"
#include "Json/GLTFJsonCore.h"
#include "Converters/GLTFConverter.h"
#include "Converters/GLTFBuilderContext.h"

class AActor;
class UMaterialInstance;
struct FGLTFJsonSkySphereColorCurve;

typedef TGLTFConverter<FGLTFJsonSkySphere*, const AActor*> IGLTFSkySphereConverter;

class GLTFEXPORTER_API FGLTFSkySphereConverter : public FGLTFBuilderContext, public IGLTFSkySphereConverter
{
public:

	using FGLTFBuilderContext::FGLTFBuilderContext;

protected:

	virtual FGLTFJsonSkySphere* Convert(const AActor* SkySphereActor) override;

private:

	enum class ESkySphereTextureParameter
	{
		SkyTexture,
		CloudsTexture,
		StarsTexture
	};

	template <class ValueType>
	void ConvertProperty(const AActor* Actor, const TCHAR* PropertyName, ValueType& OutValue) const;

	void ConvertColorProperty(const AActor* Actor, const TCHAR* PropertyName, FGLTFColor4& OutValue) const;
	void ConvertColorCurveProperty(const AActor* Actor, const TCHAR* PropertyName, FGLTFJsonSkySphereColorCurve& OutValue) const;
	void ConvertScalarParameter(const AActor* Actor, const UMaterialInstance* Material, const TCHAR* ParameterName, float& OutValue) const;

	void ConvertTextureParameter(const AActor* Actor, const UMaterialInstance* Material, const ESkySphereTextureParameter Parameter, FGLTFJsonTexture*& OutValue) const;
};
