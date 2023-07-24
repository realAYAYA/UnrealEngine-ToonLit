// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Util/ProgressCancel.h"
#include "ModelingOperators.h"

#include "UVLayoutOp.generated.h"

class UUVLayoutProperties;

namespace UE
{
namespace Geometry
{
	class FDynamicMesh3;
	class FDynamicMeshUVPacker;

enum class EUVLayoutOpLayoutModes
{
	TransformOnly = 0,
	RepackToUnitRect = 1,
	StackInUnitRect = 2
};

class MODELINGOPERATORS_API FUVLayoutOp : public FDynamicMeshOperator
{
public:
	virtual ~FUVLayoutOp() {}

	// inputs
	TSharedPtr<FDynamicMesh3, ESPMode::ThreadSafe> OriginalMesh;

	EUVLayoutOpLayoutModes UVLayoutMode = EUVLayoutOpLayoutModes::RepackToUnitRect;

	int UVLayerIndex = 0;
	int TextureResolution = 128;
	bool bAllowFlips = false;
	bool bAlwaysSplitBowties = true;
	float UVScaleFactor = 1.0;
	float GutterSize = 1.0;
	bool bMaintainOriginatingUDIM = false;
	TOptional<TSet<int32>> Selection;
	FVector2f UVTranslation = FVector2f::Zero();

	void SetTransform(const FTransformSRT3d& Transform);

	//
	// FDynamicMeshOperator implementation
	// 

	virtual void CalculateResult(FProgressCancel* Progress) override;

protected:
	void ExecutePacker(FDynamicMeshUVPacker& Packer);
};

} // end namespace UE::Geometry
} // end namespace UE

/**
 * Can be hooked up to a UMeshOpPreviewWithBackgroundCompute to perform UV Layout operations.
 *
 * Inherits from UObject so that it can hold a strong pointer to the settings UObject, which
 * needs to be a UObject to be displayed in the details panel.
 */
UCLASS()
class MODELINGOPERATORS_API UUVLayoutOperatorFactory : public UObject, public UE::Geometry::IDynamicMeshOperatorFactory
{
	GENERATED_BODY()

public:
	// IDynamicMeshOperatorFactory API
	virtual TUniquePtr<UE::Geometry::FDynamicMeshOperator> MakeNewOperator() override;

	UPROPERTY()
	TObjectPtr<UUVLayoutProperties> Settings;
	TOptional<TSet<int32>> Selection;
	TSharedPtr<UE::Geometry::FDynamicMesh3, ESPMode::ThreadSafe> OriginalMesh;
	TUniqueFunction<int32()> GetSelectedUVChannel = []() { return 0; };
	FTransform TargetTransform;
};