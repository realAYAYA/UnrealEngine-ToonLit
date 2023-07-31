// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonCore.h"

struct GLTFEXPORTER_API FGLTFJsonSkySphereColorCurve : IGLTFJsonArray
{
	struct FKey
	{
		float Time;
		float Value;
	};

	struct FComponentCurve
	{
		TArray<FKey> Keys;
	};

	TArray<FComponentCurve> ComponentCurves;

	virtual void WriteArray(IGLTFJsonWriter& Writer) const override;
};

struct GLTFEXPORTER_API FGLTFJsonSkySphere : IGLTFJsonIndexedObject
{
	FString Name;

	FGLTFJsonMesh*    SkySphereMesh;
	FGLTFJsonTexture* SkyTexture;
	FGLTFJsonTexture* CloudsTexture;
	FGLTFJsonTexture* StarsTexture;
	FGLTFJsonNode*    DirectionalLight;

	float SunHeight;
	float SunBrightness;
	float StarsBrightness;
	float CloudSpeed;
	float CloudOpacity;
	float HorizonFalloff;

	float SunRadius;
	float NoisePower1;
	float NoisePower2;

	bool bColorsDeterminedBySunPosition;

	FGLTFJsonColor4 ZenithColor;
	FGLTFJsonColor4 HorizonColor;
	FGLTFJsonColor4 CloudColor;
	FGLTFJsonColor4 OverallColor;

	FGLTFJsonSkySphereColorCurve ZenithColorCurve;
	FGLTFJsonSkySphereColorCurve HorizonColorCurve;
	FGLTFJsonSkySphereColorCurve CloudColorCurve;

	FGLTFJsonVector3 Scale;

	virtual void WriteObject(IGLTFJsonWriter& Writer) const override;

protected:

	friend TGLTFJsonIndexedObjectArray<FGLTFJsonSkySphere, void>;

	FGLTFJsonSkySphere(int32 Index)
		: IGLTFJsonIndexedObject(Index)
		, SkySphereMesh(nullptr)
		, SkyTexture(nullptr)
		, CloudsTexture(nullptr)
		, StarsTexture(nullptr)
		, DirectionalLight(nullptr)
		, SunHeight(0)
		, SunBrightness(0)
		, StarsBrightness(0)
		, CloudSpeed(0)
		, CloudOpacity(0)
		, HorizonFalloff(0)
		, SunRadius(0)
		, NoisePower1(0)
		, NoisePower2(0)
		, bColorsDeterminedBySunPosition(false)
		, ZenithColor(FGLTFJsonColor4::White)
		, HorizonColor(FGLTFJsonColor4::White)
		, CloudColor(FGLTFJsonColor4::White)
		, OverallColor(FGLTFJsonColor4::White)
		, Scale(FGLTFJsonVector3::One)
	{
	}
};
