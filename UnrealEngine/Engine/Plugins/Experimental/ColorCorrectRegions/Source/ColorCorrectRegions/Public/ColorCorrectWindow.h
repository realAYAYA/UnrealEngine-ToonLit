// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ColorCorrectRegion.h"
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "Engine/Classes/Components/MeshComponent.h"
#include "Engine/Scene.h"

#include "StageActor/IDisplayClusterStageActor.h"

#include "ColorCorrectWindow.generated.h"


class UColorCorrectRegionsSubsystem;
class UBillboardComponent;

/**
 * A Color Correction Window that functions the same way as Color Correction Regions except it modifies anything that is behind it.
 * Color Correction Windows do not have Priority property and instead are sorted based on the distance from the camera.
 */
UCLASS(Blueprintable, notplaceable)
class COLORCORRECTREGIONS_API AColorCorrectionWindow : public AColorCorrectRegion, public IDisplayClusterStageActor
{
	GENERATED_UCLASS_BODY()
public:

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> MeshComponents;

	/** Region type. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Color Correction", Meta = (DisplayName = "Type"))
	EColorCorrectWindowType WindowType;

#if WITH_EDITOR
	/** Called when any of the properties are changed. */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;
	virtual FName GetCustomIconName() const override;
#endif

private:
#if WITH_METADATA
	void CreateIcon();
#endif // WITH_METADATA

	/** Swaps meshes for different CCW. */
	void SetMeshVisibilityForWindowType();

public:

	// ~Begin IDisplayClusterStageActor interface
	UFUNCTION(BlueprintCallable, Category = Orientation)
	virtual void SetLongitude(double InValue) override;
	UFUNCTION(BlueprintCallable, Category = Orientation)
	virtual double GetLongitude() const override;

	UFUNCTION(BlueprintCallable, Category = Orientation)
	virtual void SetLatitude(double InValue) override;
	UFUNCTION(BlueprintCallable, Category = Orientation)
	virtual double GetLatitude() const override;

	UFUNCTION(BlueprintCallable, Category = Orientation)
	virtual void SetDistanceFromCenter(double InValue) override;
	UFUNCTION(BlueprintCallable, Category = Orientation)
	virtual double GetDistanceFromCenter() const override;

	UFUNCTION(BlueprintCallable, Category = Orientation)
	virtual void SetSpin(double InValue) override;
	UFUNCTION(BlueprintCallable, Category = Orientation)
	virtual double GetSpin() const override;

	UFUNCTION(BlueprintCallable, Category = Orientation)
	virtual void SetPitch(double InValue) override;
	UFUNCTION(BlueprintCallable, Category = Orientation)
	virtual double GetPitch() const override;

	UFUNCTION(BlueprintCallable, Category = Orientation)
	virtual void SetYaw(double InValue) override;
	UFUNCTION(BlueprintCallable, Category = Orientation)
	virtual double GetYaw() const override;

	UFUNCTION(BlueprintCallable, Category = Orientation)
	virtual void SetRadialOffset(double InValue) override;
	UFUNCTION(BlueprintCallable, Category = Orientation)
	virtual double GetRadialOffset() const override;

	UFUNCTION(BlueprintCallable, Category = Orientation)
	virtual void SetScale(const FVector2D& InScale) override;
	UFUNCTION(BlueprintCallable, Category = Orientation)
	virtual FVector2D GetScale() const override;

	UFUNCTION(BlueprintSetter)
	virtual void SetOrigin(const FTransform& InOrigin) override;
	UFUNCTION(BlueprintGetter)
	virtual FTransform GetOrigin() const override;

	UFUNCTION(BlueprintCallable, Category = Orientation)
	virtual void SetPositionalParams(const FDisplayClusterPositionalParams& InParams) override;
	UFUNCTION(BlueprintCallable, Category = Orientation)
	virtual FDisplayClusterPositionalParams GetPositionalParams() const override;

	virtual void GetPositionalProperties(FPositionalPropertyArray& OutPropertyPairs) const override;
	// ~End IDisplayClusterStageActor interface
	
protected:
	UPROPERTY(EditAnywhere, Category = Orientation, meta = (ShowOnlyInnerProperties))
	FDisplayClusterPositionalParams PositionalParams;
	
	UPROPERTY(VisibleAnywhere, BlueprintSetter=SetOrigin, BlueprintGetter=GetOrigin, Category = Orientation)
	FTransform Origin;

	/** Update the transform when a positional setter is called */
	bool bNotifyOnParamSetter = true;
};

UCLASS(Deprecated, Blueprintable, notplaceable, meta = (DeprecationMessage = "This is a deprecated version of Color Correct Window. Please re-create Color Correct Window to remove this warning."))
class COLORCORRECTREGIONS_API ADEPRECATED_ColorCorrectWindow : public AColorCorrectionWindow
{
	GENERATED_UCLASS_BODY()
};