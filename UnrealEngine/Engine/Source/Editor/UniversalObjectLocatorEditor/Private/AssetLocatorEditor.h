// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UniversalObjectLocatorEditor.h"

struct FAssetData;

namespace UE::UniversalObjectLocator
{

class IUniversalObjectLocatorCustomization;

class FAssetLocatorEditor : public ILocatorEditor
{
	bool IsDragSupported(TSharedPtr<FDragDropOperation> DragOperation, UObject* Context) const override;

	UObject* ResolveDragOperation(TSharedPtr<FDragDropOperation> DragOperation, UObject* Context) const override;

	TSharedPtr<SWidget> MakeEditUI(TSharedPtr<IUniversalObjectLocatorCustomization> Customization) override;

	FText GetDisplayText() const override;

	FText GetDisplayTooltip() const override;

	FSlateIcon GetDisplayIcon() const override;

private:

	void OnSetObject(const FAssetData& InNewObject, TWeakPtr<IUniversalObjectLocatorCustomization> WeakCustomization);
};

} // namespace UE::UniversalObjectLocator

