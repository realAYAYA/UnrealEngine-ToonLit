// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "IDetailCustomNodeBuilder.h"
#include "IDetailCustomization.h"
#include "Templates/SharedPointer.h"

class IPropertyHandleMap;
class SWidget;

class FAvaTagCollectionCustomization : public IDetailCustomization
{
public:
	//~ Begin IDetailCustomization
	virtual void CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder) override;
	//~ End IDetailCustomization
};

class FAvaTagMapBuilder : public IDetailCustomNodeBuilder, public TSharedFromThis<FAvaTagMapBuilder>
{
public:
	FAvaTagMapBuilder(const TSharedRef<IPropertyHandle>& InTagMapProperty);

	virtual ~FAvaTagMapBuilder() override;

	//~ Begin IDetailCustomNodeBuilder
	virtual FName GetName() const override;
	virtual bool InitiallyCollapsed() const override { return false; }
	virtual void GenerateHeaderRowContent(FDetailWidgetRow& InNodeRow) override {}
	virtual void GenerateChildContent(IDetailChildrenBuilder& InChildrenBuilder) override;
	virtual TSharedPtr<IPropertyHandle> GetPropertyHandle() const override;
	virtual void SetOnRebuildChildren(FSimpleDelegate InOnRebuildChildren) override;
	//~ End IDetailCustomNodeBuilder

private:
	TSharedRef<SWidget> CreatePropertyButtonsWidget(TSharedPtr<IPropertyHandle> InTagHandle);

	void DeleteItem(TSharedPtr<IPropertyHandle> InTagHandle);

	void SearchForReferences(TSharedPtr<IPropertyHandle> InTagIdHandle);

	void OnNumChildrenChanged();

	TSharedPtr<IPropertyHandleMap> MapProperty;

	TSharedPtr<IPropertyHandle> BaseProperty;

	FDelegateHandle OnNumElementsChangedHandle;

	FSimpleDelegate OnRebuildChildren;
};

