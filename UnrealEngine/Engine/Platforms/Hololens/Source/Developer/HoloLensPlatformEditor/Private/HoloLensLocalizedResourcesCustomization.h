// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyEditorModule.h"
#include "IPropertyTypeCustomization.h"
#include "IDetailCustomNodeBuilder.h"

class FHoloLensLocalizedResourcesNodeBuilder : public IDetailCustomNodeBuilder, public TSharedFromThis<FHoloLensLocalizedResourcesNodeBuilder>
{
public:
	FHoloLensLocalizedResourcesNodeBuilder(TSharedRef<IPropertyHandle> InLocalizedResourceCollectionProperty, const FString& InPluginName);
	virtual ~FHoloLensLocalizedResourcesNodeBuilder();

	/** IDetailCustomNodeBuilder interface */
	virtual void SetOnRebuildChildren(FSimpleDelegate InOnRebuildChildren) override { OnRebuildChildren = InOnRebuildChildren; }
	virtual bool RequiresTick() const override { return false; }
	virtual void Tick(float DeltaTime) override {}
	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override {}
	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override;
	virtual bool InitiallyCollapsed() const override { return true; };
	virtual FName GetName() const override { return FName(TEXT("LocalizedResources")); }

private:
	void OnNumElementsChanged() { OnRebuildChildren.ExecuteIfBound(); }
	void AddLocalizedEntryForSelectedCulture();
	void AddLocalizedEntry(const FString& CultureId);
	void EmptyLocalizedEntries();

	FString OptionalPluginName;
	FSimpleDelegate OnRebuildChildren;
	TSharedRef<class IPropertyHandleArray> LocalizedResourceCollectionArray;
	TArray<TSharedPtr<FString>> AvailableUECultureNames;
	TSharedPtr<class STextComboBox> SelectedCulture;
};

class FHoloLensLocalizedResourcesCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	// IPropertyTypeCustomization interface
	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	// End of IPropertyTypeCustomization interface
};