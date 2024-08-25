// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animators/PropertyAnimatorCoreBase.h"
#include "Customizations/OperatorStackEditorStackCustomization.h"
#include "PropertyAnimatorCoreEditorStackCustomization.generated.h"

class UPropertyAnimatorCoreComponent;
class UPropertyAnimatorCoreBase;
class UToolMenu;

/** Property Controller customization for operator stack tab */
UCLASS()
class UPropertyAnimatorCoreEditorStackCustomization : public UOperatorStackEditorStackCustomization
{
	GENERATED_BODY()

public:
	UPropertyAnimatorCoreEditorStackCustomization();

	//~ Begin UOperatorStackEditorStackCustomization
	virtual bool TransformContextItem(const FOperatorStackEditorItemPtr& InItem, TArray<FOperatorStackEditorItemPtr>& OutTransformedItems) const override;
	virtual void CustomizeStackHeader(const FOperatorStackEditorTree& InItemTree, FOperatorStackEditorHeaderBuilder& InHeaderBuilder) override;
	virtual void CustomizeItemHeader(const FOperatorStackEditorItemPtr& InItem, const FOperatorStackEditorTree& InItemTree, FOperatorStackEditorHeaderBuilder& InHeaderBuilder) override;
	virtual void CustomizeItemBody(const FOperatorStackEditorItemPtr& InItem, const FOperatorStackEditorTree& InItemTree, FOperatorStackEditorBodyBuilder& InBodyBuilder) override;
	virtual void CustomizeItemFooter(const FOperatorStackEditorItemPtr& InItem, const FOperatorStackEditorTree& InItemTree, FOperatorStackEditorFooterBuilder& InFooterBuilder) override;
	virtual bool OnIsItemDraggable(const FOperatorStackEditorItemPtr& InItem) override;
	virtual const FSlateBrush* GetIcon() const override;
	//~ End UOperatorStackEditorStackCustomization

protected:
	/** Remove animator menu action */
	void RemoveControllerAction(UPropertyAnimatorCoreBase* InAnimator) const;
	void RemoveControllersAction(UPropertyAnimatorCoreComponent* InComponent) const;

	/** Fill item action menus */
	void FillAddAnimatorMenuSection(UToolMenu* InToolMenu) const;
	void FillComponentHeaderActionMenu(UToolMenu* InToolMenu);
	void FillAnimatorHeaderActionMenu(UToolMenu* InToolMenu);
	void FillAnimatorContextActionMenu(UToolMenu* InToolMenu) const;

	/** Create commands for animator */
	TSharedRef<FUICommandList> CreateAnimatorCommands(UPropertyAnimatorCoreBase* InAnimator);
};