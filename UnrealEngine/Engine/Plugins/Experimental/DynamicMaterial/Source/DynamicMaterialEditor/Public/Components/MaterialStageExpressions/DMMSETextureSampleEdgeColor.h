// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialStageExpression.h"
#include "DMMSETextureSampleEdgeColor.generated.h"

UENUM(BlueprintType)
enum class EDMEdgeLocation : uint8
{
	TopLeft,
	Top,
	TopRight,
	Left,
	Center,
	Right,
	BottomLeft,
	Bottom,
	BottomRight,
	Custom
};

UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIALEDITOR_API UDMMaterialStageExpressionTextureSampleEdgeColor : public UDMMaterialStageExpression
{
	GENERATED_BODY()

public:
	UDMMaterialStageExpressionTextureSampleEdgeColor();

	//~ Begin UDMMaterialStageThroughput
	virtual bool CanChangeInputType(int32 InInputIndex) const override;
	virtual bool IsInputVisible(int32 InInputIndex) const override;
	virtual void AddDefaultInput(int32 InInputIndex) const override;
	//~ End UDMMaterialStageThroughput

	//~ Begin UObject
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject

protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Material Designer", meta=(AllowPrivateAccess = "true"))
	EDMEdgeLocation EdgeLocation;

	void OnEdgeLocationChanged();
};
