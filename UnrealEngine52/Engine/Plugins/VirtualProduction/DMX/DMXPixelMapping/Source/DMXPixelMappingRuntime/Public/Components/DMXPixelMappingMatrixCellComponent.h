// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMXPixelMappingOutputDMXComponent.h"
#include "DMXAttribute.h"
#include "Library/DMXEntityReference.h"

#include "DMXPixelMappingMatrixCellComponent.generated.h"


class SUniformGridPanel;
class UTextureRenderTarget2D;
enum class EDMXColorMode : uint8;

/**
 * Matrix pixel component
 */
UCLASS()
class DMXPIXELMAPPINGRUNTIME_API UDMXPixelMappingMatrixCellComponent
	: public UDMXPixelMappingOutputDMXComponent
{
	GENERATED_BODY()

	/** Let the matrix component edit size and position */
	friend class UDMXPixelMappingMatrixComponent;

public:
	/** Default Constructor */
	UDMXPixelMappingMatrixCellComponent();

	//~ Begin UObject implementation
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ End UObject implementation

	//~ Begin UDMXPixelMappingBaseComponent implementation
	virtual const FName& GetNamePrefix() override;
	virtual void ResetDMX() override;
	virtual FString GetUserFriendlyName() const override;
	//~ End UDMXPixelMappingBaseComponent implementation

	//~ Begin UDMXPixelMappingOutputComponent implementation
#if WITH_EDITOR
	PRAGMA_DISABLE_DEPRECATION_WARNINGS // FDMXPixelMappingComponentWidget is deprecated
	virtual TSharedRef<FDMXPixelMappingComponentWidget> BuildSlot(TSharedRef<SConstraintCanvas> InCanvas) override;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	virtual bool IsVisible() const override;
	virtual FLinearColor GetEditorColor() const override;
#endif // WITH_EDITOR	
	virtual bool IsOverParent() const override;
	virtual int32 GetDownsamplePixelIndex() const override { return DownsamplePixelIndex; }
	virtual void QueueDownsample() override;
	virtual void SetPosition(const FVector2D& NewPosition) override;
	virtual void SetSize(const FVector2D& NewSize) override;
	//~ End UDMXPixelMappingOutputComponent implementation

public:
	//~ Begin UDMXPixelMappingOutputDMXComponent implementation
	virtual void RenderWithInputAndSendDMX() override;
	//~ End UDMXPixelMappingOutputDMXComponent implementation

	void SetCellCoordinate(FIntPoint InCellCoordinate);
	const FIntPoint& GetCellCoordinate() { return CellCoordinate; }

	/** Check if a Component can be moved under another one (used for copy/move/duplicate) */
	virtual bool CanBeMovedTo(const UDMXPixelMappingBaseComponent* Component) const override;

private:
	/** Helper that returns the renderer component this component belongs to */
	UDMXPixelMappingRendererComponent* GetRendererComponent() const;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cell Settings")
	int32 CellID;

	UPROPERTY()
	FDMXEntityFixturePatchRef FixturePatchMatrixRef_DEPRECATED;

	/** Creates attribute values from current data */
	TMap<FDMXAttributeName, float> CreateAttributeValues() const;

private:
	UPROPERTY()
	FIntPoint CellCoordinate;

	/** Index of the cell pixel in downsample target buffer */
	int32 DownsamplePixelIndex;
};
