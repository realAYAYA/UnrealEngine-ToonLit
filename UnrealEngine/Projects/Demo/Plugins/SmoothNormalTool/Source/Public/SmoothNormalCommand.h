#pragma once

#include "CoreMinimal.h"

struct FNormalInfo
{
	FVector Normal;
	float Angle;
};

class SMOOTHNORMALTOOL_API FSmoothNormalCommand
{
	
private:
	
	static void SmoothNormalSkeletalMesh(FAssetData AssetData);
	static void SmoothNormalStaticMeshTriangle(FAssetData AssetData);
	static void SmoothNormalStaticMesh(FAssetData AssetData);
	
public:
	
	static void SmoothNormal(TArray<FAssetData> SelectedAssets);
	
};
