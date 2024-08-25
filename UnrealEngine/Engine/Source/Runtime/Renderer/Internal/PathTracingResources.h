// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class FRDGTexture;

struct FPathTracingResources
{
	FRDGTexture* DenoisedRadiance = nullptr;
	FRDGTexture* Radiance = nullptr;
	FRDGTexture* Albedo = nullptr;
	FRDGTexture* Normal = nullptr;
	FRDGTexture* Variance = nullptr;
	
	bool bPostProcessEnabled = false;
};
