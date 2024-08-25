// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetLocatorEditor.h"
#include "Modules/ModuleManager.h"
#include "UniversalObjectLocatorFragmentTypeHandle.h"
#include "UniversalObjectLocator.h"
#include "UniversalObjectLocatorEditor.h"
#include "IUniversalObjectLocatorEditorModule.h"
#include "IUniversalObjectLocatorCustomization.h"

#include "DragAndDrop/AssetDragDropOp.h"

#include "PropertyCustomizationHelpers.h"

#include "Widgets/Layout/SBox.h"


#define LOCTEXT_NAMESPACE "AssetLocatorEditor"

namespace UE::UniversalObjectLocator
{

bool FAssetLocatorEditor::IsDragSupported(TSharedPtr<FDragDropOperation> DragOperation, UObject* Context) const
{
	TSharedPtr<FAssetDragDropOp> ActorDrag;

	if (DragOperation->IsOfType<FAssetDragDropOp>())
	{
		TSharedPtr<FAssetDragDropOp> AssetDrag = StaticCastSharedPtr<FAssetDragDropOp>(DragOperation);
		if (AssetDrag->GetAssets().Num() == 1)
		{
			return true;
		}
	}

	return false;
}

UObject* FAssetLocatorEditor::ResolveDragOperation(TSharedPtr<FDragDropOperation> DragOperation, UObject* Context) const
{
	TSharedPtr<FAssetDragDropOp> ActorDrag;

	if (DragOperation->IsOfType<FAssetDragDropOp>())
	{
		TSharedPtr<FAssetDragDropOp> AssetDrag = StaticCastSharedPtr<FAssetDragDropOp>(DragOperation);
		if (AssetDrag->GetAssets().Num() == 1)
		{
			return AssetDrag->GetAssets()[0].FastGetAsset(true);
		}
	}

	return nullptr;
}

TSharedPtr<SWidget> FAssetLocatorEditor::MakeEditUI(TSharedPtr<IUniversalObjectLocatorCustomization> Customization)
{
	TSharedRef<SObjectPropertyEntryBox> EditWidget = SNew(SObjectPropertyEntryBox)
	.ObjectPath(Customization.ToSharedRef(), &IUniversalObjectLocatorCustomization::GetPathToObject)
	.PropertyHandle(Customization->GetProperty())
	.AllowedClass(UObject::StaticClass())
	.OnObjectChanged(this, &FAssetLocatorEditor::OnSetObject, TWeakPtr<IUniversalObjectLocatorCustomization>(Customization))
	.AllowClear(true)
	.DisplayUseSelected(true)
	.DisplayBrowse(true)
	.DisplayThumbnail(true);

	float MinWidth = 100.f;
	float MaxWidth = 500.f;
	EditWidget->GetDesiredWidth(MinWidth, MaxWidth);

	return SNew(SBox)
	.MinDesiredWidth(MinWidth)
	.MaxDesiredWidth(MaxWidth)
	[
		EditWidget
	];
}

FText FAssetLocatorEditor::GetDisplayText() const
{
	return LOCTEXT("AssetLocatorName", "Asset");
}

FText FAssetLocatorEditor::GetDisplayTooltip() const
{
	return LOCTEXT("AssetLocatorTooltip", "Change this to an asset reference");
}

FSlateIcon FAssetLocatorEditor::GetDisplayIcon() const
{
	return FSlateIcon();
}

void FAssetLocatorEditor::OnSetObject(const FAssetData& InNewObject, TWeakPtr<IUniversalObjectLocatorCustomization> WeakCustomization)
{
	TSharedPtr<IUniversalObjectLocatorCustomization> Customization = WeakCustomization.Pin();
	if (Customization)
	{
		// Assets are always absolute
		UObject* Object = InNewObject.FastGetAsset(true);

		FUniversalObjectLocator NewRef(Object, nullptr);
		Customization->SetValue(MoveTemp(NewRef));
	}
}

} // namespace UE::UniversalObjectLocator

#undef LOCTEXT_NAMESPACE
