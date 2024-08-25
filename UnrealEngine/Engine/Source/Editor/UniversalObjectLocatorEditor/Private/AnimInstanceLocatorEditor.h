// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UniversalObjectLocatorEditor.h"

namespace UE::UniversalObjectLocator
{

class FAnimInstanceLocatorEditor : public ILocatorEditor
{
public:
	bool IsDragSupported(TSharedPtr<FDragDropOperation> DragOperation, UObject* Context) const override;

	UObject* ResolveDragOperation(TSharedPtr<FDragDropOperation> DragOperation, UObject* Context) const override;

	TSharedPtr<SWidget> MakeEditUI(TSharedPtr<IUniversalObjectLocatorCustomization> Customization) override;

	FText GetDisplayText() const override;

	FText GetDisplayTooltip() const override;

	FSlateIcon GetDisplayIcon() const override;
};


} // namespace UE::UniversalObjectLocator
