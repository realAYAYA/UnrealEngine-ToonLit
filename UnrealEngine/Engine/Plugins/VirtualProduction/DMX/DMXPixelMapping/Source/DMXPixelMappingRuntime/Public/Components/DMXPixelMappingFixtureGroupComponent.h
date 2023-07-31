// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMXPixelMappingOutputComponent.h"
#include "Library/DMXEntityReference.h"
#include "DMXPixelMappingFixtureGroupComponent.generated.h"

class UDMXLibrary;
class UDMXPixelMappingLayoutScript;

class STextBlock;
class SUniformGridPanel;


/**
 * Container component for Fixture Items
 */
UCLASS()
class DMXPIXELMAPPINGRUNTIME_API UDMXPixelMappingFixtureGroupComponent
	: public UDMXPixelMappingOutputComponent
{
	GENERATED_BODY()
public:
	/** Default Constructor */
	UDMXPixelMappingFixtureGroupComponent();

	//~ Begin UObject implementation
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
#endif // WITH_EDITOR
	//~ End UObject implementation

	//~ Begin UDMXPixelMappingBaseComponent implementation
	virtual const FName& GetNamePrefix() override;
	virtual void AddChild(UDMXPixelMappingBaseComponent* InComponent) override;
	virtual bool CanBeMovedTo(const UDMXPixelMappingBaseComponent* Component) const override;
	virtual void ResetDMX() override;
	virtual void SendDMX() override;
	virtual FString GetUserFriendlyName() const override;
	//~ End UDMXPixelMappingBaseComponent implementation

	//~ Begin UDMXPixelMappingOutputComponent implementation
#if WITH_EDITOR
	virtual bool IsExposedToTemplate() { return true; }
	virtual const FText GetPaletteCategory() override;
#endif // WITH_EDITOR
	virtual void QueueDownsample() override;
	virtual void SetPosition(const FVector2D& NewPosition) override;
	virtual void SetSize(const FVector2D& NewSize) override;
	//~ End UDMXPixelMappingOutputComponent implementation

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fixture List")
	TObjectPtr<UDMXLibrary> DMXLibrary;

	/** Layout script for the children of this component (hidden in customizations and displayed in its own panel). */
	UPROPERTY(EditAnywhere, Instanced, Category = "Layout")
	TObjectPtr<UDMXPixelMappingLayoutScript> LayoutScript;

private:
	/** Handles changes in position */
	void HandlePositionChanged();

	/** Holds the last set size */
	FVector2D LastPosition;

	static const FVector2D MinGroupSize;
};
