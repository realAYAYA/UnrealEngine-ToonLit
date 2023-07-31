// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"
#include "Internationalization/Text.h"
#include "PropertyEditorModule.h"
#include "Templates/SharedPointer.h"

class FDataLayerDragDropOp;
class FDetailWidgetRow;
class FDragDropEvent;
class FDragDropOperation;
class FReply;
class IDetailChildrenBuilder;
class IPropertyHandle;
class SWidget;
class UDataLayerInstance;
struct EVisibility;
struct FGeometry;
struct FSlateBrush;
struct FSlateColor;

struct FDataLayerPropertyTypeCustomization : public IPropertyTypeCustomization
{
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override {}

private:

	void AssignDataLayer(const UDataLayerInstance* InDataLayer);

	UDataLayerInstance* GetDataLayerFromPropertyHandle(FPropertyAccess::Result* OutPropertyAccessResult = nullptr) const;
	FText GetDataLayerText() const;
	FSlateColor GetForegroundColor() const;
	const FSlateBrush* GetDataLayerIcon() const;
	EVisibility GetSelectDataLayerVisibility() const;

	TSharedRef<SWidget> OnGetDataLayerMenu();
	void OnBrowse();
	FReply OnSelectDataLayer();
	FReply OnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent);
	bool OnVerifyDrag(TSharedPtr<FDragDropOperation> InDragDrop);
	TSharedPtr<const FDataLayerDragDropOp> GetDataLayerDragDropOp(TSharedPtr<FDragDropOperation> InDragDrop);

	TSharedPtr<IPropertyHandle> PropertyHandle;
};
