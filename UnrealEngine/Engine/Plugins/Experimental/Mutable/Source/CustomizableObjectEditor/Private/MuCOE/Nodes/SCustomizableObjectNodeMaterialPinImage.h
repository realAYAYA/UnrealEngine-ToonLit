// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Internationalization/Text.h"
#include "Layout/Visibility.h"
#include "SGraphPin.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SWidget;
class UEdGraphPin;
struct FSlateBrush;


/** Material node custom Image pin. Allows to define a custom style to Image pins.
 *  
 * Currently these different styles allow the user to differentiate the different states an Image pin can have:
 * - passthrough: The texture does NOT go trough the mutable pipeline.
 * - mutable: The texture goes trough the mutable pipeline.
 */
class SCustomizableObjectNodeMaterialPinImage : public SGraphPin
{
public:

	SLATE_BEGIN_ARGS(SCustomizableObjectNodeMaterialPinImage) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

	TSharedRef<SWidget>	GetDefaultValueWidget() override;
	const FSlateBrush* GetPinIcon() const override;

private:
	/** Return pin state text. */
	FText GetDefaultValueText() const;

	/** Return true if the pin state is visible. */
	EVisibility GetDefaultValueVisibility() const;

	/** Return pin tool tip. */
	FText GetTooltipText() const;

	const FSlateBrush* CachedPinMutableConnected;
	const FSlateBrush* CachedPinMutableDisconnected;
	const FSlateBrush* CachedPinPassthroughDisconnected;
};
