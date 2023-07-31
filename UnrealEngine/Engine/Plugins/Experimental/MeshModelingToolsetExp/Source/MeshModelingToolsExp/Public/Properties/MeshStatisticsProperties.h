// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolBuilder.h"
#include "GeometryBase.h"
#include "MeshStatisticsProperties.generated.h"


// predeclarations
PREDECLARE_USE_GEOMETRY_CLASS(FDynamicMesh3);


UCLASS()
class MESHMODELINGTOOLSEXP_API UMeshStatisticsProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:

	UPROPERTY(VisibleAnywhere, Category = MeshStatistics, meta = (NoResetToDefault))
	FString Mesh;

	UPROPERTY(VisibleAnywhere, Category = MeshStatistics, AdvancedDisplay, meta = (NoResetToDefault))
	FString UV;

	UPROPERTY(VisibleAnywhere, Category = MeshStatistics, AdvancedDisplay, meta = (NoResetToDefault))
	FString Attributes;

	void Update(const FDynamicMesh3& Mesh);
};

