// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "GeometryMaskTypes.h"
#include "IGeometryMaskReadInterface.h"

#include "GeometryMaskReadComponent.generated.h"

class UCanvasRenderTarget2D;
class UGeometryMaskCanvas;

UCLASS(Blueprintable, BlueprintType, HideCategories=(Activation, Cooking, AssetUserData, Navigation), meta=(BlueprintSpawnableComponent))
class GEOMETRYMASK_API UGeometryMaskReadComponent
	: public UGeometryMaskCanvasReferenceComponentBase
	, public IGeometryMaskReadInterface
{
	GENERATED_BODY()

public:
	//~ Begin IGeometryMaskReadInterface
	virtual void SetParameters(FGeometryMaskReadParameters& InParameters) override;
	virtual const FGeometryMaskReadParameters& GetParameters() const override
	{
		return Parameters;
	}
	virtual FOnGeometryMaskSetCanvasNativeDelegate& OnSetCanvas() override
	{
		return OnSetCanvasDelegate;
	}
	//~ End IGeometryMaskReadInterface

protected:
	//~ Begin UGeometryMaskCanvasReferenceComponentBase
	virtual bool TryResolveCanvas() override;
	//~ End UGeometryMaskCanvasReferenceComponentBase

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetParameters", Getter="GetParameters", Category="Mask", meta=(ShowOnlyInnerProperties))
	FGeometryMaskReadParameters Parameters;
};
