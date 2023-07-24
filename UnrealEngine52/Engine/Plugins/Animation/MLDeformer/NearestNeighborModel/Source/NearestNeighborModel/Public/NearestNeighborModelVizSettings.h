// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MLDeformerMorphModelVizSettings.h"
#include "NearestNeighborModelVizSettings.generated.h"

class UGeometryCache;
namespace UE::NearestNeighborModel
{
    class FNearestNeighborEditorModel;
};

/**
 * The vizualization settings specific to the the vertex delta model.
 */
UCLASS()
class NEARESTNEIGHBORMODEL_API UNearestNeighborModelVizSettings 
	: public UMLDeformerMorphModelVizSettings
{
	GENERATED_BODY()
#if	WITH_EDITORONLY_DATA
public:
	static FName GetNearestNeighborActorsOffsetPropertyName() { return GET_MEMBER_NAME_CHECKED(UNearestNeighborModelVizSettings, NearestNeighborActorsOffset); }
	static FName GetNearestNeighborIdsPropertyName() { return GET_MEMBER_NAME_CHECKED(UNearestNeighborModelVizSettings, NearestNeighborIds); }
	
	friend class UE::NearestNeighborModel::FNearestNeighborEditorModel;

protected:
	UPROPERTY(EditAnywhere, Category = "Live Settings")
	float NearestNeighborActorsOffset = 2.0f;

	UPROPERTY(VisibleAnywhere, Category = "Live Settings")
	TArray<uint32> NearestNeighborIds;

private:
	void SetNearestNeighborActorsOffset(float InOffset) { NearestNeighborActorsOffset = InOffset; }
	float GetNearestNeighborActorsOffset() const { return NearestNeighborActorsOffset; }
	void SetNearestNeighborIds(const TArray<uint32>& InNearestNeighborIds) { NearestNeighborIds = InNearestNeighborIds; }
#endif
};
