// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"
#include "MuCOE/PinViewer/SPinViewerPinDetails.h"
#include "Templates/SharedPointer.h"
#include "Types/SlateEnums.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SSpinBox.h"

class FString;
class SWidget;

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
