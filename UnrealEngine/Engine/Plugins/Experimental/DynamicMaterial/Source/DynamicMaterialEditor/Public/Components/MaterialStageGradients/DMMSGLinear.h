// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialStageGradient.h"
#include "DMMSGLinear.generated.h"

struct FDMMaterialBuildState;
class UMaterialExpression;

UENUM(BlueprintType)
enum class ELinearGradientTileType : uint8
{
	NoTile,
	Tile,
	TileAndMirror
};

UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIALEDITOR_API UDMMaterialStageGradientLinear : public UDMMaterialStageGradient
{
	GENERATED_BODY()

public:
	UDMMaterialStageGradientLinear();

	virtual void GenerateExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState) const override;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	virtual ELinearGradientTileType GetTilingType() const { return Tiling; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	virtual void SetTilingType(ELinearGradientTileType InType);

protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Getter=GetTilingType, Setter=SetTilingType, Category = "Material Designer")
	ELinearGradientTileType Tiling;
};
