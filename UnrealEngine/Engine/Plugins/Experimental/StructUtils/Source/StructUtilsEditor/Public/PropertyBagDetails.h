// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InstancedStructDetails.h"
#include "EdGraphSchema_K2.h"
#include "PropertyBag.h"
#include "PropertyBagDetails.generated.h"

class FReply;
enum class EPinContainerType : uint8;
struct FEdGraphSchemaAction;

class IPropertyHandle;
class IPropertyUtilities;
class IDetailPropertyRow;
class SInlineEditableTextBlock;
class SWidget;

/**
 * Type customization for FInstancedPropertyBag.
 */
class STRUCTUTILSEDITOR_API FPropertyBagDetails : public IPropertyTypeCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** Creates add property widget. */
	static TSharedPtr<SWidget> MakeAddPropertyWidget(TSharedPtr<IPropertyHandle> InStructProperty, TSharedPtr<IPropertyUtilities> InPropUtils, EPropertyBagPropertyType DefaultType = EPropertyBagPropertyType::Bool);
	
protected:
	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

private:

	FReply OnAddProperty() const;
	
	/** Handle to the struct property being edited */
	TSharedPtr<IPropertyHandle> StructProperty;
	TSharedPtr<IPropertyUtilities> PropUtils;
	EPropertyBagPropertyType DefaultType = EPropertyBagPropertyType::Bool;
	bool bFixedLayout = false;
	bool bAllowArrays = true;
};

/** 
 * Node builder for FInstancedPropertyBag children.
 *  - ValueProperty is FInstancedStruct of the FInstancedPropertyBag
 *  - StructProperty is FInstancedPropertyBag
 * Can be used in a implementation of a IPropertyTypeCustomization CustomizeChildren() to display editable FInstancedPropertyBag contents.
 * Use FPropertyBagDetails::MakeAddPropertyWidget() to create the add property widget.
 * OnChildRowAdded() is called right after each property is added, which allows the property row to be customizable.
 */
class STRUCTUTILSEDITOR_API FPropertyBagInstanceDataDetails : public FInstancedStructDataDetails
{
public:

	FPropertyBagInstanceDataDetails(TSharedPtr<IPropertyHandle> InStructProperty, const TSharedPtr<IPropertyUtilities>& InPropUtils, const bool bInFixedLayout, const bool bInAllowArrays = true);

	virtual void OnChildRowAdded(IDetailPropertyRow& ChildRow) override;
	
	/** Enum describing if a property is overridden, or undetermined (e.g. multiselection) */
	enum class EPropertyOverrideState
	{
		Yes,
		No,
		Undetermined,
	};

	/** Interface to allow to modify override status of a specific parameter. */
	struct IPropertyBagOverrideProvider
	{
		virtual ~IPropertyBagOverrideProvider()
		{
		}

		virtual bool IsPropertyOverridden(const FGuid PropertyID) const = 0;
		virtual void SetPropertyOverride(const FGuid PropertyID, const bool bIsOverridden) const = 0;
	};

	/**
	 * Callback function for EnumeratePropertyBags.
	 * @return true to continue enumeration
	 */
	using EnumeratePropertyBagFuncRef = TFunctionRef<bool(const FInstancedPropertyBag& /*DefaultPropertyBag*/, FInstancedPropertyBag& /*PropertyBag*/, IPropertyBagOverrideProvider& /*OverrideProvider*/)>;

	/**
	 * Method that is called to determine if a derived class has property override logic implemented.
	 * If true is returned, the overridden class is expected to implement PreChangeOverrides(), PostChangeOverrides(), EnumeratePropertyBags().
	 * @return true of derived class has override logic implemented.
	 */
	virtual bool HasPropertyOverrides() const
	{
		return false;
	}

	/** Called before property override is changed. */
	virtual void PreChangeOverrides()
	{
		// ParentPropertyHandle would be the property that holds the property overrides. 
		//		ParentPropertyHandle->NotifyPreChange();
		checkf(false, TEXT("PreChangeOverrides() is expecgted to be implemented when HasPropertyOverrides() returns true."));
	}

	/** Called after property override is changed. */
	virtual void PostChangeOverrides()
	{
		// ParentPropertyHandle is be the property that holds the property overrides. 
		//		ParentPropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
		//		ParentPropertyHandle->NotifyFinishedChangingProperties();
		checkf(false, TEXT("PostChangeOverrides() is expecgted to be implemented when HasPropertyOverrides() returns true."));
	}

	/**
	 * Called to enumerate each property bag on the property handle.
	 * The Func expects DefaultPropertyBag (the values that are override), and PropertyBag (the one that PropertyBagHandle points to),
	 * and instance of IPropertyBagOverrideProvider which is used to query if specific property is overridden, or to set the property override state.
	 */
	virtual void EnumeratePropertyBags(TSharedPtr<IPropertyHandle> PropertyBagHandle, const EnumeratePropertyBagFuncRef& Func) const
	{
		checkf(false, TEXT("EnumeratePropertyBags() is expecgted to be implemented when HasPropertyOverrides() returns true."));
	}

	/** @return true if property of specified child property is overridden. */
	virtual EPropertyOverrideState IsPropertyOverridden(TSharedPtr<IPropertyHandle> ChildPropertyHandle) const;
	
	/** Called to set the override state of specified child property. */
	virtual void SetPropertyOverride(TSharedPtr<IPropertyHandle> ChildPropertyHandle, const bool bIsOverridden);

	/** @return true if the child property has default value. */
	virtual bool IsDefaultValue(TSharedPtr<IPropertyHandle> ChildPropertyHandle) const;

	/** Called to reset the child property to default value. */
	virtual void ResetToDefault(TSharedPtr<IPropertyHandle> ChildPropertyHandle);
	
protected:
	TSharedRef<SWidget> OnPropertyNameContent(TSharedPtr<IPropertyHandle> ChildPropertyHandle, TSharedPtr<SInlineEditableTextBlock> InlineWidget) const;

	TSharedPtr<IPropertyHandle> BagStructProperty;
	TSharedPtr<IPropertyUtilities> PropUtils;
	bool bFixedLayout = false;
	bool bAllowArrays = true;
};

/**
 * Specific property bag schema to allow customizing the requirements (e.g. supported containers).
 */
UCLASS()
class STRUCTUTILSEDITOR_API UPropertyBagSchema : public UEdGraphSchema_K2
{
	GENERATED_BODY()
public:
	virtual bool SupportsPinTypeContainer(TWeakPtr<const FEdGraphSchemaAction> SchemaAction, const FEdGraphPinType& PinType, const EPinContainerType& ContainerType) const override;
};
