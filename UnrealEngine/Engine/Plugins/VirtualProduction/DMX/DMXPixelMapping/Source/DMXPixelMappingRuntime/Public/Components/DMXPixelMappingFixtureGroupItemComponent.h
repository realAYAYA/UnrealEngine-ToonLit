// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMXPixelMappingOutputDMXComponent.h"

#include "Components/DMXPixelMappingFixtureGroupComponent.h"
#include "Components/DMXPixelMappingMatrixCellComponent.h"
#include "Library/DMXEntityReference.h"

#include "Misc/Attribute.h"

#include "DMXPixelMappingFixtureGroupItemComponent.generated.h"

struct FDMXAttributeName;
class UDMXModulator;
enum class EDMXColorMode : uint8;

class STextBlock;
class UTextureRenderTarget2D;


/**
 * Fixture Item pixel component
 */
UCLASS(AutoExpandCategories = ("Output Settings"))
class DMXPIXELMAPPINGRUNTIME_API UDMXPixelMappingFixtureGroupItemComponent
	: public UDMXPixelMappingOutputDMXComponent
{
	GENERATED_BODY()
public:
	/** Default Constructor */
	UDMXPixelMappingFixtureGroupItemComponent();

protected:
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedChainEvent) override;
#endif // WITH_EDITOR

public:
	//~ Begin UDMXPixelMappingBaseComponent implementation
	virtual const FName& GetNamePrefix() override;
	virtual void ResetDMX() override;
	virtual void SendDMX() override;
	virtual FString GetUserFriendlyName() const override;
	//~ End UDMXPixelMappingBaseComponent implementation

	//~ Begin UDMXPixelMappingOutputComponent implementation
#if WITH_EDITOR
	virtual bool IsVisible() const override;
#endif // WITH_EDITOR	
	virtual bool IsOverParent() const override;
	virtual void QueueDownsample() override;
	virtual int32 GetDownsamplePixelIndex() const override { return DownsamplePixelIndex; }
	virtual void SetPosition(const FVector2D& NewPosition) override;
	virtual void SetSize(const FVector2D& NewSize) override;
	//~ End UDMXPixelMappingOutputComponent implementation

public:
	//~ Begin UDMXPixelMappingOutputDMXComponent implementation
	virtual void RenderWithInputAndSendDMX() override;
	//~ End UDMXPixelMappingOutputDMXComponent implementation

	/** Check if a Component can be moved under another one (used for copy/move/duplicate) */
	virtual bool CanBeMovedTo(const UDMXPixelMappingBaseComponent* Component) const override;

private:
	/** Helper that returns the renderer component this component belongs to */
	UDMXPixelMappingRendererComponent* GetRendererComponent() const;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Selected Patch")
	FDMXEntityFixturePatchRef FixturePatchRef;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output Settings")
	EDMXColorMode ColorMode;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output Settings", meta = (DisplayName = "R"))
	bool AttributeRExpose;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output Settings", meta = (DisplayName = "G"))
	bool AttributeGExpose;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output Settings", meta = (DisplayName = "B"))
	bool AttributeBExpose;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output Settings", meta = (DisplayName = "Expose"))
	bool bMonochromeExpose;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output Settings", meta = (DisplayName = "Invert R"))
	bool AttributeRInvert;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output Settings", meta = (DisplayName = "Invert G"))
	bool AttributeGInvert;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output Settings", meta = (DisplayName = "Invert B"))
	bool AttributeBInvert;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output Settings", meta = (DisplayName = "Invert"))
	bool bMonochromeInvert;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output Settings", meta = (DisplayName = "R Attribute"))
	FDMXAttributeName AttributeR;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output Settings", meta = (DisplayName = "G Attribute"))
	FDMXAttributeName AttributeG;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output Settings", meta = (DisplayName = "B Attribute"))
	FDMXAttributeName AttributeB;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output Settings", meta = (DisplayName = "Intensity Attribute"))
	FDMXAttributeName MonochromeIntensity;

	/** Modulators applied to the output before sending DMX */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Transient, Category = "Output Settings", meta = (DisplayName = "Output Modulators"))
	TArray<TSubclassOf<UDMXModulator>> ModulatorClasses;

	UPROPERTY()
	TArray<TObjectPtr<UDMXModulator>> Modulators;

	/** Index of the cell pixel in downsample target buffer */
	int32 DownsamplePixelIndex;

	/** Creates attribute values from current data */
	TMap<FDMXAttributeName, float> CreateAttributeValues() const;
};
