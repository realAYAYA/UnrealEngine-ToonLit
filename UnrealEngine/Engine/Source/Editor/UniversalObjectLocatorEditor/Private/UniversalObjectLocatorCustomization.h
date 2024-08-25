// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"
#include "Internationalization/Text.h"
#include "Templates/SharedPointer.h"
#include "Misc/Optional.h"
#include "Types/SlateEnums.h"

#include "UniversalObjectLocator.h"
#include "IUniversalObjectLocatorCustomization.h"

struct FGeometry;
struct FAssetData;

class FReply;
class FDragDropEvent;
class FDetailWidgetRow;
class FDragDropOperation;

class IPropertyUtilities;
class IDetailChildrenBuilder;

class AActor;

class SBox;
class SWidget;

namespace UE::UniversalObjectLocator
{

class ILocatorEditor;

struct FUniversalObjectLocatorCustomization final : public IPropertyTypeCustomization, public IUniversalObjectLocatorCustomization
{
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

	UObject* GetContext() const override { return nullptr; }
	UObject* GetSingleObject() const override;
	FString GetPathToObject() const override;
	void SetValue(FUniversalObjectLocator&& InNewValue) const override;
	TSharedPtr<IPropertyHandle> GetProperty() const override;

private:

	struct FCachedData
	{
		TOptional<FUniversalObjectLocator> PropertyValue;
		TWeakObjectPtr<> WeakObject;
		FString ObjectPath;
		TOptional<FFragmentTypeHandle> FragmentType;
		FText FragmentTypeText;
		FName LocatorEditorType;
	};

private:

	void Rebuild() const;

	TSharedRef<SWidget> GetUserExposedFragmentTypeList();
	FText GetCurrentFragmentTypeText() const;

	bool HandleIsDragAllowed(TSharedPtr<FDragDropOperation> InDragOperation);
	FReply HandleDrop(const FGeometry& InGeometry, const FDragDropEvent& InDropEvent);

	void SetActor(AActor* InActor);

	const FCachedData& GetCachedData() const;

	FUniversalObjectLocator* GetSinglePropertyValue();
	const FUniversalObjectLocator* GetSinglePropertyValue() const;

	void ChangeEditorType(FName InNewEditor);
	bool CompareCurrentEditorType(FName InNewEditor) const;

private:

	TSharedPtr<IPropertyUtilities> PropertyUtilities;
	TSharedPtr<IPropertyHandle> PropertyHandle;
	TSharedPtr<SBox> Content;

	mutable FCachedData CachedData;

	TMap<FName, TSharedPtr<ILocatorEditor>> ApplicableLocators;
};


} // namespace UE::UniversalObjectLocator