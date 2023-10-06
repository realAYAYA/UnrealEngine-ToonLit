// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/PinViewer/SPinViewerPinDetails.h"

#include "SPinViewerNodeMaterialPinImageDetails.generated.h"

class UCustomizableObjectNodeMaterialPinDataImage;
enum class EPinMode;
namespace ESelectInfo { enum Type : int; }
template <typename NumericType> class SSpinBox;

class FString;
class SWidget;
class UEdGraphPin;

/** UV Layout Mode UI Enum. */
UENUM()
enum class EUVLayoutMode
{
	/** Equal to UV_LAYOUT_DEFAULT */
	Default,
	/** Equal to UV_LAYOUT_IGNORE */
	Ignore,
	/** Any valid index. */
	Index,
};

/** Codify the UVLayout into a single integer. */
int32 UVLayoutModeToUVLayout(const EUVLayoutMode Mode, const int32 UVLayout);

/** Given the codified UV Layout, returns the UV Layout mode. */
EUVLayoutMode UVLayoutToUVLayoutMode(int32 UVLayout);

/** Node Material Pin Image custom details. Allows Image Pin Data to be modified by the user. */
class SPinViewerNodeMaterialPinImageDetails : public SPinViewerPinDetails
{
public:
	SLATE_BEGIN_ARGS(SPinViewerNodeMaterialPinImageDetails) {}
		SLATE_ARGUMENT(UEdGraphPin*, Pin)
		SLATE_ARGUMENT(UCustomizableObjectNodeMaterialPinDataImage*, PinData)
	SLATE_END_ARGS()

	/** Slate constructor. */
	void Construct(const FArguments& InArgs);

private:
	TArray<TSharedPtr<EPinMode>> PinModeOptions;
	TArray<TSharedPtr<EUVLayoutMode>> UVLayoutModeOptions;
	TSharedPtr<SSpinBox<int32>> UVLayoutSSpinBox;
	TSharedPtr<SWidget> UVLayout;
	TMap<TSharedPtr<FString>, EPinMode> PinModeComboBoxOptionValues;
	
	/** Pointer to the Pin to show and modigy. */
	UEdGraphPin* Pin = nullptr;

	/** Pin Data to show and modify. */
	UCustomizableObjectNodeMaterialPinDataImage* PinData = nullptr;
	
	/** Callback for Pin Mode changed. */
	void PinModeOnSelectionChanged(TSharedPtr<EPinMode> PinMode, ESelectInfo::Type Arg);

	/** Callback for UV Layout Mode changed. */
	void UVLayoutModeOnSelectionChanged(TSharedPtr<EUVLayoutMode> LayoutMode, ESelectInfo::Type Arg);

	static void UVLayoutVisibility(EUVLayoutMode LayoutMode, TSharedPtr<SWidget> Widget);
	
	/** Callback for UV Layout Index changed. */
	void UVLayoutOnValueChanged(int32 Value);
};
