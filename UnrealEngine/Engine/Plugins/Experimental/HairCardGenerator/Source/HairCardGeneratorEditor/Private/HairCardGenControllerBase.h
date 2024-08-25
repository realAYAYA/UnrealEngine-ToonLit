// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GroomCacheData.h"
#include "Containers/Map.h"
#include "Templates/Tuple.h"


#include "HairCardGenControllerBase.generated.h"

class FHairDescription;
//struct FHairCardGenerationData;

USTRUCT(BlueprintType)
struct FHairCardGen_StrandData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Strand")
	int GroupID = -1;
};

// With "Guide" data stipped out (since it isn't used as part of the mesh generation algorithm)
USTRUCT(BlueprintType)
struct FHairCardGen_GroomData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Groom")
	FString BasisType;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Groom")
	FString CurveType;

	// Array of index buffers. Each buffer defines a distinct hair strand in the groom.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Groom")
	TArray<FHairCardGen_StrandData> Strands;

	// Flattened Array of 3D vetex positions 
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Groom")
	TArray<float> VertexPositions;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Groom")
	TArray<float> VertexWidths;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Groom")
	int32 HairlineGroupID = INDEX_NONE;
};


class UGroomAsset;
class UHairCardGeneratorPluginSettings;

/**
 * 
 */
UCLASS(Blueprintable)
class UHairCardGenControllerBase : public UObject
{
	GENERATED_BODY()

public:
	UHairCardGenControllerBase();

	UFUNCTION(BlueprintImplementableEvent, Category = Python)
	int GetPointsPerCurve();

	UFUNCTION(BlueprintImplementableEvent, Category = Python)
	bool LoadGroomData(const FHairCardGen_GroomData& GroomData, const FString& Name, const FString& CachedGroomsPath, const bool SaveCached = false);

	UFUNCTION(BlueprintImplementableEvent, Category = Python)
	bool LoadSettings(const UHairCardGeneratorPluginSettings* GeneratorSettings);

	UFUNCTION(BlueprintImplementableEvent, Category = Python)
	bool GenerateCardsGeometry();

	UFUNCTION(BlueprintImplementableEvent, Category = Python)
	bool GenerateClumps(const int GroupId = -1);

	UFUNCTION(BlueprintImplementableEvent, Category = Python)
	TArray<int> SetOptimizations(const int GroupId = -1);

	UFUNCTION(BlueprintImplementableEvent, Category = Python)
	TArray<float> GetAverageCurve(const int Id, const int Cid);

	UFUNCTION(BlueprintImplementableEvent, Category = Python)
	void SetInterpolatedAvgCurve(const int Id, const int Cid, const TArray<float>& Points);

	UFUNCTION(BlueprintImplementableEvent, Category = Python)
	bool ClusterTextures(int GroupIndex);

	UFUNCTION(BlueprintImplementableEvent, Category = Python)
	bool GenerateTextureLayout();

	UFUNCTION(BlueprintImplementableEvent, Category = Python)
	bool GenerateTextureAtlases(const float WidthScale = -1);

	UFUNCTION(BlueprintImplementableEvent, Category = Python)
	bool GenerateMesh(UStaticMesh* StaticMesh);

	UFUNCTION(BlueprintImplementableEvent, Category = Python)
	bool TestWriteFbxMesh();

	UFUNCTION(BlueprintImplementableEvent, Category = Python)
	bool CheckClusterTextures(int GroupIndex);

	// Helper C++ function for building a static mesh from verts/faces, etc.
	UFUNCTION(BlueprintCallable, Category = Python)
	void CreateCardsStaticMesh(UStaticMesh* StaticMesh, const TArray<float>& verts, const TArray<int32>& faces, const TArray<float>& normals, const TArray<float>& uvs, const TArray<int32>& groups);


	TObjectPtr<UHairCardGeneratorPluginSettings>& GetGroomSettings(TObjectPtr<UGroomAsset> Groom, int LODIndex);
	void UpdateGroomSettings(TObjectPtr<UGroomAsset> Groom, int LODIndex, int GroupID, TObjectPtr<UHairCardGeneratorPluginSettings> NewSettings);

private:


	UPROPERTY(Transient)
	TMap<FString,TObjectPtr<UHairCardGeneratorPluginSettings>> GroomSettingsMap;
};
