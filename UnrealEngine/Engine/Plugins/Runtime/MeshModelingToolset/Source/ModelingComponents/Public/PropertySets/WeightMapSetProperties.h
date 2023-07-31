// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InteractiveTool.h"
#include "WeightMapSetProperties.generated.h"

struct FMeshDescription;


/**
 * Basic Tool Property Set that allows for selecting from a list of FNames (that we assume are Weight Maps)
 */
UCLASS()
class MODELINGCOMPONENTS_API UWeightMapSetProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:

	/** Select vertex weight map. If configured, the weight map value will be sampled to modulate displacement intensity. */
	UPROPERTY(EditAnywhere, Category = WeightMap, meta = (GetOptions = GetWeightMapsFunc))
	FName WeightMap;

	// this function is called provide set of available weight maps
	UFUNCTION()
	TArray<FString> GetWeightMapsFunc();

	// internal list used to implement above
	UPROPERTY(meta = (TransientToolProperty))
	TArray<FString> WeightMapsList;

	UPROPERTY(EditAnywhere, Category = WeightMap)
	bool bInvertWeightMap = false;

	// set list of weightmap FNames explicitly. Adds "None" as first option.
	void InitializeWeightMaps(const TArray<FName>& WeightMapNames);

	// set list of weightmap FNames based on per-vertex float attributes in MeshDescription. Adds "None" as first option.
	void InitializeFromMesh(const FMeshDescription* Mesh);

	// return true if any option other than "None" is selected
	bool HasSelectedWeightMap() const;

	// set selected weightmap from its position in the WeightMapsList
	void SetSelectedFromWeightMapIndex(int32 Index);
};

