// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"
#include "IDetailCustomNodeBuilder.h"
#include "EditorUndoClient.h"
#include "StructViewerFilter.h"

class IPropertyHandle;
class IDetailPropertyRow;
class IPropertyHandle;
class FStructOnScope;
class SWidget;
class SComboButton;
struct FInstancedStruct;
class FInstancedStructProvider;

/**
 * Type customization for FInstancedStruct.
 */
class STRUCTUTILSEDITOR_API FInstancedStructDetails : public IPropertyTypeCustomization
{
public:
	virtual ~FInstancedStructDetails() override;
	
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

private:

	using FReplacementObjectMap = TMap<UObject*, UObject*>;
	void OnObjectsReinstanced(const FReplacementObjectMap& ObjectMap);
	
	FText GetDisplayValueString() const;
	FText GetTooltipText() const;
	const FSlateBrush* GetDisplayValueIcon() const;
	TSharedRef<SWidget> GenerateStructPicker();
	void OnStructPicked(const UScriptStruct* InStruct);

	/** Handle to the struct property being edited */
	TSharedPtr<IPropertyHandle> StructProperty;

	/** The base struct that we're allowing to be picked (controlled by the "BaseStruct" meta-data) */
	UScriptStruct* BaseScriptStruct = nullptr;

	TSharedPtr<SComboButton> ComboButton;
	TSharedPtr<IPropertyUtilities> PropUtils;
	
	FDelegateHandle OnObjectsReinstancedHandle;
};

/** 
 * Node builder for FInstancedStruct children.
 * Expects property handle holding FInstancedStruct as input.
 * Can be used in a implementation of a IPropertyTypeCustomization CustomizeChildren() to display editable FInstancedStruct contents.
 * OnChildRowAdded() is called right after each property is added, which allows the property row to be customizable.
 */
class STRUCTUTILSEDITOR_API FInstancedStructDataDetails : public IDetailCustomNodeBuilder, public TSharedFromThis<FInstancedStructDataDetails>
{
public:
	FInstancedStructDataDetails(TSharedPtr<IPropertyHandle> InStructProperty);
	virtual ~FInstancedStructDataDetails() override;

	//~ Begin IDetailCustomNodeBuilder interface
	virtual void SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren) override;
	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override;
	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override;
	virtual void Tick(float DeltaTime) override;
	virtual bool RequiresTick() const override { return true; }
	virtual bool InitiallyCollapsed() const override { return false; }
	virtual FName GetName() const override;
	//~ End IDetailCustomNodeBuilder interface

	// Called when a child is added, override to customize a child row.
	virtual void OnChildRowAdded(IDetailPropertyRow& ChildRow) {}

private:
	void OnUserDefinedStructReinstancedHandle(const class UUserDefinedStruct& Struct);

	/** Pre/Post change notifications for struct value changes */
	void OnStructValuePreChange();
	void OnStructValuePostChange();
	void OnStructHandlePostChange();

	/** Returns type of the instanced struct for each instance/object being edited. */
	TArray<TWeakObjectPtr<const UStruct>> GetInstanceTypes() const;

	/** Cached instance types, used to invalidate the layout when types change. */
	TArray<TWeakObjectPtr<const UStruct>> CachedInstanceTypes;
	
	/** Handle to the struct property being edited */
	TSharedPtr<IPropertyHandle> StructProperty;

	/** Struct provider for the structs. */
	TSharedPtr<FInstancedStructProvider> StructProvider;

	/** Delegate that can be used to refresh the child rows of the current struct (eg, when changing struct type) */
	FSimpleDelegate OnRegenerateChildren;

	/** True if we're currently handling a StructValuePostChange */
	bool bIsHandlingStructValuePostChange = false;
	
	FDelegateHandle UserDefinedStructReinstancedHandle;

protected:
	void OnStructLayoutChanges();
};

/**
 * Filter used by the instanced struct struct picker.
 */
 class STRUCTUTILSEDITOR_API FInstancedStructFilter : public IStructViewerFilter
{
public:
	/** The base struct for the property that classes must be a child-of. */
	const UScriptStruct* BaseStruct = nullptr;

	// A flag controlling whether we allow UserDefinedStructs
	bool bAllowUserDefinedStructs = false;

	// A flag controlling whether we allow to select the BaseStruct
	bool bAllowBaseStruct = true;

	virtual bool IsStructAllowed(const FStructViewerInitializationOptions& InInitOptions, const UScriptStruct* InStruct, TSharedRef<FStructViewerFilterFuncs> InFilterFuncs) override;
	virtual bool IsUnloadedStructAllowed(const FStructViewerInitializationOptions& InInitOptions, const FSoftObjectPath& InStructPath, TSharedRef<FStructViewerFilterFuncs> InFilterFuncs) override;
};


#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "UnrealClient.h"
#endif
