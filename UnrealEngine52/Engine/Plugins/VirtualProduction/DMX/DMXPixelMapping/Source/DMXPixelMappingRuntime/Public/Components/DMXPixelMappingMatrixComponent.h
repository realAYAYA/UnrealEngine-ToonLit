// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMXPixelMappingOutputComponent.h"
#include "Library/DMXEntityReference.h"
#include "Library/DMXEntityFixtureType.h"
#include "DMXPixelMappingMatrixComponent.generated.h"

enum class EDMXColorMode : uint8;
class UDMXLibrary;
class UDMXPixelMappingColorSpace;
class UDMXPixelMappingLayoutScript;


/**
 * DMX Matrix group component
 */
UCLASS()
class DMXPIXELMAPPINGRUNTIME_API UDMXPixelMappingMatrixComponent
	: public UDMXPixelMappingOutputComponent
{
	GENERATED_BODY()

	DECLARE_EVENT_TwoParams(UDMXPixelMappingMatrixComponent, FDMXPixelMappingOnMatrixChanged, UDMXPixelMapping* /** PixelMapping */, UDMXPixelMappingMatrixComponent* /** AddedComponent */);

	/** Helper callback for loop through all component child */
	using ChildCallback = TFunctionRef<void(UDMXPixelMappingMatrixCellComponent*)>;

public:
	/** Default Constructor */
	UDMXPixelMappingMatrixComponent();

	/** Gets an Event broadcast when a the matrix (and by that its num cells) changed */
	static FDMXPixelMappingOnMatrixChanged& GetOnMatrixChanged()
	{
		static FDMXPixelMappingOnMatrixChanged OnMatrixChanged;
		return OnMatrixChanged;
	}

	// ~Begin UObject interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedChainEvent) override;
#endif // WITH_EDITOR
	// ~End UObject interface
	
	/**  Logs properties that were changed in underlying fixture patch or fixture type  */
	void LogInvalidProperties();

public:
	// ~Begin UDMXPixelMappingBaseComponent interface
	virtual const FName& GetNamePrefix() override;
	virtual void ResetDMX() override;
	virtual void SendDMX() override;
	virtual bool CanBeMovedTo(const UDMXPixelMappingBaseComponent* Component) const override;
	virtual FString GetUserFriendlyName() const override;
	// ~End UDMXPixelMappingBaseComponent interface

	// ~Begin UDMXPixelMappingOutputComponent interface
#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif // WITH_EDITOR
	virtual bool IsOverParent() const override;
	virtual void QueueDownsample() override;
	virtual void SetPosition(const FVector2D& NewPosition) override;
	virtual void SetSize(const FVector2D& NewSize) override;
	// ~End UDMXPixelMappingOutputComponent interface

	/** Handles changes in position */
	void HandlePositionChanged();

	/** Handles changes in size or in matrix */
	void HandleSizeChanged();

	/** Handles changes in size or in matrix */
	void HandleMatrixChanged();

protected:
	/** Called when the fixture type in use changed */
	void OnFixtureTypeChanged(const UDMXEntityFixtureType* FixtureType);

	/** Called when the fixture patch in use changed */
	void OnFixturePatchChanged(const UDMXEntityFixturePatch* FixturePatch);

#if WITH_EDITORONLY_DATA
	/** Children available PreEditUndo, useful to hide all removed ones in post edit undo */
	TArray<UDMXPixelMappingBaseComponent*> PreEditUndoMatrixCellChildren;
#endif // WITH_EDITORONLY_DATA

private:
	/** True while the component is updating its children */
	bool bIsUpdatingChildren = false;

	/** Position before it was changed */
	FVector2D PreEditChangePosition;

public:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FDMXEntityFixturePatchRef FixturePatchMatrixRef_DEPRECATED;
#endif // WITH_EDITORONLY_DATA

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Matrix Settings")
	FDMXEntityFixturePatchRef FixturePatchRef;

	/** Sets which color space Pixel Mapping sends */
	UPROPERTY(Transient, EditAnywhere, NoClear, Category = "Color Space", Meta = (DisplayPriority = 2, DisplayName = "Output Mode", ShowDisplayNames))
	TSubclassOf<UDMXPixelMappingColorSpace> ColorSpaceClass;

	/** The Color Space currently in use */
	UPROPERTY(VisibleAnywhere, Instanced, Category = "Color Space")
	TObjectPtr<UDMXPixelMappingColorSpace> ColorSpace;

	/** Modulators applied to the output before sending DMX */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Transient, Category = "Output Settings", meta = (DisplayName = "Output Modulators"))
	TArray<TSubclassOf<UDMXModulator>> ModulatorClasses;

	/** The actual modulator instances */
	UPROPERTY()
	TArray<TObjectPtr<UDMXModulator>> Modulators;

	UPROPERTY()
	FIntPoint CoordinateGrid;

	UPROPERTY()
	FVector2D CellSize;

	UPROPERTY()
	EDMXPixelMappingDistribution Distribution;

	/** Layout script for the children of this component (hidden in customizations and displayed in its own panel). */
	UPROPERTY(EditAnywhere, Instanced, Category = "Layout")
	TObjectPtr<UDMXPixelMappingLayoutScript> LayoutScript;

	//////////////////////
	// Deprecated Members
	
public:
#if WITH_EDITORONLY_DATA
	UPROPERTY(Meta = (DeprecatedProperty, DeprecationMessage = "Deprecated in favor of DMXPixelMappingColorSpace. See ColorSpace member."))
	EDMXColorMode ColorMode_DEPRECATED;

	UPROPERTY(Meta = (DeprecatedProperty, DeprecationMessage = "Deprecated in favor of DMXPixelMappingColorSpace. See ColorSpace member."))
	bool AttributeRExpose_DEPRECATED;

	UPROPERTY(Meta = (DeprecatedProperty, DeprecationMessage = "Deprecated in favor of DMXPixelMappingColorSpace. See ColorSpace member."))
	bool AttributeGExpose_DEPRECATED;

	UPROPERTY(Meta = (DeprecatedProperty, DeprecationMessage = "Deprecated in favor of DMXPixelMappingColorSpace. See ColorSpace member."))
	bool AttributeBExpose_DEPRECATED;

	UPROPERTY(Meta = (DeprecatedProperty, DeprecationMessage = "Deprecated in favor of DMXPixelMappingColorSpace. See ColorSpace member."))
	bool bMonochromeExpose_DEPRECATED;

	UPROPERTY(Meta = (DeprecatedProperty, DeprecationMessage = "Deprecated in favor of DMXPixelMappingColorSpace. See ColorSpace member."))
	bool AttributeRInvert_DEPRECATED;

	UPROPERTY(Meta = (DeprecatedProperty, DeprecationMessage = "Deprecated in favor of DMXPixelMappingColorSpace. See ColorSpace member."))
	bool AttributeGInvert_DEPRECATED;

	UPROPERTY(Meta = (DeprecatedProperty, DeprecationMessage = "Deprecated in favor of DMXPixelMappingColorSpace. See ColorSpace member."))
	bool AttributeBInvert_DEPRECATED;

	UPROPERTY(Meta = (DeprecatedProperty, DeprecationMessage = "Deprecated in favor of DMXPixelMappingColorSpace. See ColorSpace member."))
	bool bMonochromeInvert_DEPRECATED;

	UPROPERTY(Meta = (DeprecatedProperty, DeprecationMessage = "Deprecated in favor of DMXPixelMappingColorSpace. See ColorSpace member."))
	FDMXAttributeName AttributeR_DEPRECATED;

	UPROPERTY(Meta = (DeprecatedProperty, DeprecationMessage = "Deprecated in favor of DMXPixelMappingColorSpace. See ColorSpace member."))
	FDMXAttributeName AttributeG_DEPRECATED;

	UPROPERTY(Meta = (DeprecatedProperty, DeprecationMessage = "Deprecated in favor of DMXPixelMappingColorSpace. See ColorSpace member."))
	FDMXAttributeName AttributeB_DEPRECATED;

	UPROPERTY(Meta = (DeprecatedProperty, DeprecationMessage = "Deprecated in favor of DMXPixelMappingColorSpace. See ColorSpace member."))
	FDMXAttributeName MonochromeIntensity_DEPRECATED;
#endif // WITH_EDITORONLY_DATA
};
