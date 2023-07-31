// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "UObject/WeakObjectPtr.h"

enum class EDMXPixelMappingDistribution : uint8;
enum class EDMXCellFormat : uint8;
class FDMXPixelMappingToolkit;
class UDMXEntityFixturePatch;
class UDMXMVRFixtureNode;
class UDMXPixelMappingBaseComponent;
class UDMXPixelMappingOutputComponent;
class UDMXPixelMappingScreenComponent;


/** 
 * Model for the Pixel Mapping Output Components.
 * For Screen Component, see DMXPixelMappingScreenComponentModel.
 */
class FDMXPixelMappingOutputComponentModel
	: public TSharedFromThis<FDMXPixelMappingOutputComponentModel>
{
public:
	/** Constructor */
	FDMXPixelMappingOutputComponentModel(const TSharedRef<FDMXPixelMappingToolkit>& InToolkit, TWeakObjectPtr<UDMXPixelMappingOutputComponent> InOutputComponent);

	/** Destructor */
	virtual ~FDMXPixelMappingOutputComponentModel();

	/** Returns the position of the component */
	FVector2D GetPosition() const;

	/** Returns the size of the component */
	FVector2D GetSize() const;

	/** Returns the Name of the component */
	FText GetName() const;

	/** Returns true if the name should drawn its name */
	bool ShouldDrawName() const;

	/** Returns true if the name should drawn its name above the widget */
	bool ShouldDrawNameAbove() const;

	/** Returns true if this component has a fixture patch and related info */
	bool HasPatchInfo() const;

	/** Returns the addresses of the patch as text */
	FText GetAddressesText() const;

	/** Returns the Fixture ID as text. Empty unless a Matrix and Fixture Group Item component */
	FText GetFixtureIDText() const;

	/** Returns true if this component has a Cell ID */
	bool HasCellID() const;

	/** Returns the Cell ID as text. Empty unless a Matrix Cell component. */
	FText GetCellIDText() const;

	/** Returns the color of the widget */
	FLinearColor GetColor() const;

	/** Returns true if this Model handles the Other component */
	bool Equals(UDMXPixelMappingBaseComponent* Other) const; 

private:
	/** Called when selected Components changed */
	void OnSelectedComponentsChanged();

	/** Returns the Fixture Patch of the component, or nullptr if the component does not use a Fixture Patch. */
	const UDMXEntityFixturePatch* GetFixturePatch() const;

	/** Updates the Fixture Node that correlates to this component */
	void UpdateFixtureNode();

	/** The Fixture Node that corresponds to this component, only valid for Group Item and Matrix */
	TWeakObjectPtr<UDMXMVRFixtureNode> WeakFixtureNode;

	/** True if the component is selected */
	bool bSelected = false;

	/** The output component */
	TWeakObjectPtr<UDMXPixelMappingOutputComponent> WeakOutputComponent;

	/** The toolkit from which the Model currently sources */
	TWeakPtr<FDMXPixelMappingToolkit> WeakToolkit;
};


/** Model for Pixel Mapping Screen Components */
class FDMXPixelMappingScreenComponentModel
	: public TSharedFromThis<FDMXPixelMappingScreenComponentModel>
{
public:
	/** Constructor */
	FDMXPixelMappingScreenComponentModel(const TSharedRef<FDMXPixelMappingToolkit>& InToolkit, TWeakObjectPtr<UDMXPixelMappingScreenComponent> InScreenComponent);

	/** Destructor */
	virtual ~FDMXPixelMappingScreenComponentModel();

	/** Returns the position of the component */
	FVector2D GetPosition() const;

	/** Returns the size of the component */
	FVector2D GetSize() const;

	/** Returns the number of Columns of the Screen Component */
	int32 GetNumColumns() const;

	/** Returns the number of Rows of the Screen Component */
	int32 GetNumRows() const;

	/** Returns the pixel mapping distribution of the cells */
	EDMXPixelMappingDistribution GetDistribution() const;

	/** Returns the color format of the cells */
	EDMXCellFormat GetCellFormat() const;

	/** Returns true if the components wants to show the universe (it has a related property) */
	bool ComponentWantsToShowUniverse() const;

	/** Returns the universe of the screen component */
	int32 GetUniverse() const;

	/** Returns true if the components wants to show the channel (it has a related property) */
	bool ComponentWantsToShowChannel() const;

	/** Returns the starting address of the screen component */
	int32 GetStartingChannel() const;

	/** Returns the color of the widget */
	FLinearColor GetColor() const;

	/** Returns true if this Model handles the Other component */
	bool Equals(UDMXPixelMappingBaseComponent* Other) const;

private:
	/** Called when selected Components changed */
	void OnSelectedComponentsChanged();

	/** True if the component is selected */
	bool bSelected = false;

	/** The output component */
	TWeakObjectPtr<UDMXPixelMappingScreenComponent> WeakScreenComponent;

	/** The toolkit from which the Model currently sources */
	TWeakPtr<FDMXPixelMappingToolkit> WeakToolkit;
};
