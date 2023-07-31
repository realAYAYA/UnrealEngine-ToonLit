// Copyright Epic Games, Inc. All Rights Reserved.

#include "Json/GLTFJsonSkySphere.h"
#include "Json/GLTFJsonMesh.h"
#include "Json/GLTFJsonTexture.h"
#include "Json/GLTFJsonNode.h"

void FGLTFJsonSkySphereColorCurve::WriteArray(IGLTFJsonWriter& Writer) const
{
	for (int32 ComponentIndex = 0; ComponentIndex < ComponentCurves.Num(); ++ComponentIndex)
	{
		const FComponentCurve& ComponentCurve = ComponentCurves[ComponentIndex];

		Writer.StartArray();

		for (const FKey& Key: ComponentCurve.Keys)
		{
			Writer.Write(Key.Time);
			Writer.Write(Key.Value);
		}

		Writer.EndArray();
	}
}

void FGLTFJsonSkySphere::WriteObject(IGLTFJsonWriter& Writer) const
{
	if (!Name.IsEmpty())
	{
		Writer.Write(TEXT("name"), Name);
	}

	Writer.Write(TEXT("skySphereMesh"), SkySphereMesh);
	Writer.Write(TEXT("skyTexture"), SkyTexture);
	Writer.Write(TEXT("cloudsTexture"), CloudsTexture);
	Writer.Write(TEXT("starsTexture"), StarsTexture);

	if (DirectionalLight != nullptr)
	{
		Writer.Write(TEXT("directionalLight"), DirectionalLight);
	}

	Writer.Write(TEXT("sunHeight"), SunHeight);
	Writer.Write(TEXT("sunBrightness"), SunBrightness);
	Writer.Write(TEXT("starsBrightness"), StarsBrightness);
	Writer.Write(TEXT("cloudSpeed"), CloudSpeed);
	Writer.Write(TEXT("cloudOpacity"), CloudOpacity);
	Writer.Write(TEXT("horizonFalloff"), HorizonFalloff);

	Writer.Write(TEXT("sunRadius"), SunRadius);
	Writer.Write(TEXT("noisePower1"), NoisePower1);
	Writer.Write(TEXT("noisePower2"), NoisePower2);

	Writer.Write(TEXT("colorsDeterminedBySunPosition"), bColorsDeterminedBySunPosition);

	Writer.Write(TEXT("zenithColor"), ZenithColor);
	Writer.Write(TEXT("horizonColor"), HorizonColor);
	Writer.Write(TEXT("cloudColor"), CloudColor);

	if (!OverallColor.IsNearlyEqual(FGLTFJsonColor4::White, Writer.DefaultTolerance))
	{
		Writer.Write(TEXT("overallColor"), OverallColor);
	}

	if (ZenithColorCurve.ComponentCurves.Num() >= 3)
	{
		Writer.Write(TEXT("zenithColorCurve"), ZenithColorCurve);
	}

	if (HorizonColorCurve.ComponentCurves.Num() >= 3)
	{
		Writer.Write(TEXT("horizonColorCurve"), HorizonColorCurve);
	}

	if (CloudColorCurve.ComponentCurves.Num() >= 3)
	{
		Writer.Write(TEXT("cloudColorCurve"), CloudColorCurve);
	}

	if (!Scale.IsNearlyEqual(FGLTFJsonVector3::One, Writer.DefaultTolerance))
	{
		Writer.Write(TEXT("scale"), Scale);
	}
}
