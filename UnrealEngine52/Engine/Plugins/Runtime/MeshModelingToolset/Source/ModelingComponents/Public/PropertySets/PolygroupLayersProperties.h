// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InteractiveTool.h"
#include "GeometryBase.h"
#include "Polygroups/PolygroupSet.h"
#include "PolygroupLayersProperties.generated.h"

PREDECLARE_USE_GEOMETRY_CLASS(FDynamicMesh3);

/**
 * Basic Tool Property Set that allows for selecting from a list of FNames (that we assume are Polygroup Layers)
 */
UCLASS()
class MODELINGCOMPONENTS_API UPolygroupLayersProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:

	/** Select PolyGroup layer to use. */
	UPROPERTY(EditAnywhere, Category = "PolyGroup Layer", meta = (DisplayName = "Active PolyGroup", GetOptions = GetGroupLayersFunc))
	FName ActiveGroupLayer = "Default";

	// Provides set of available group layers
	UFUNCTION()
	TArray<FString> GetGroupLayersFunc() { return GroupLayersList; }

	// internal list used to implement above
	UPROPERTY(meta = (TransientToolProperty))
	TArray<FString> GroupLayersList;

	void InitializeGroupLayers(const FDynamicMesh3* Mesh);

	void InitializeGroupLayers(const TSet<FName>& LayerNames);

	// return true if any option other than "Default" is selected
	bool HasSelectedPolygroup() const;

	void SetSelectedFromPolygroupIndex(int32 Index);

	UE::Geometry::FPolygroupLayer GetSelectedLayer(const FDynamicMesh3& FromMesh);
};

