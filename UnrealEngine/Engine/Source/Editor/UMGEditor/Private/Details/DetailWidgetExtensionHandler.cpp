// Copyright Epic Games, Inc. All Rights Reserved.

#include "Details/DetailWidgetExtensionHandler.h"
#include "UMGEditorProjectSettings.h"
#include "WidgetBlueprintEditor.h"
#include "Engine/Blueprint.h"
#include "Binding/WidgetBinding.h"
#include "WidgetBlueprint.h"
#include "Customizations/UMGDetailCustomizations.h"

FDetailWidgetExtensionHandler::FDetailWidgetExtensionHandler(TSharedPtr<FWidgetBlueprintEditor> InBlueprintEditor)
	: BlueprintEditor( InBlueprintEditor )
{}

bool FDetailWidgetExtensionHandler::IsPropertyExtendable(const UClass* InObjectClass, const IPropertyHandle& InPropertyHandle) const
{
	// TODO UMG make this work for multiple widgets.
	if ( InPropertyHandle.GetNumOuterObjects() == 1 )
	{
		TArray<UObject*> Objects;
		InPropertyHandle.GetOuterObjects(Objects);

		// Make the extension handler run for child struct/array properties if the parent is bindable.
		// This is so we can later disable any child rows of a bound property in ExtendWidgetRow().
		// Check to see if there's a delegate for the parent property.
		TSharedPtr<const IPropertyHandle> ParentPropertyHandle;
		for (TSharedPtr<const IPropertyHandle> CurrentPropertyHandle = InPropertyHandle.AsShared();
			CurrentPropertyHandle && CurrentPropertyHandle->GetProperty();
			CurrentPropertyHandle = CurrentPropertyHandle->GetParentHandle())
		{
			ParentPropertyHandle = CurrentPropertyHandle;
		}

		// We don't allow bindings on the CDO.
		if (Objects[0] != nullptr && Objects[0]->HasAnyFlags(RF_ClassDefaultObject) )
		{
			return false;
		}

		TSharedPtr<FWidgetBlueprintEditor> BPEd = BlueprintEditor.Pin();
		if (BPEd == nullptr || Objects[0] == BPEd->GetPreview())
		{
			return false;
		}

		FProperty* ParentProperty = ParentPropertyHandle->GetProperty();
		FString DelegateName = ParentProperty->GetName() + "Delegate";

		if ( UClass* ContainerClass = ParentProperty->GetOwner<UClass>() )
		{
			FDelegateProperty* DelegateProperty = FindFProperty<FDelegateProperty>(ContainerClass, FName(*DelegateName));
			if ( DelegateProperty )
			{
				return true;
			}
		}
	}

	return false;
}

void FDetailWidgetExtensionHandler::ExtendWidgetRow(
	FDetailWidgetRow& InWidgetRow,
	const IDetailLayoutBuilder& InDetailBuilder,
	const UClass* InObjectClass,
	TSharedPtr<IPropertyHandle> InPropertyHandle)
{
	// Always make the value widget disabled when the property is bindable.
	const TWeakPtr<FWidgetBlueprintEditor> BlueprintEditorPtr = BlueprintEditor;
	const TSharedRef<IPropertyHandle> PropertyHandleRef = InPropertyHandle.ToSharedRef();
	InWidgetRow.IsValueEnabled(TAttribute<bool>::Create(
		[BlueprintEditorPtr, PropertyHandleRef]() {
			// NOTE: HasPropertyBindings is potentially expensive (see its implementation) and this will
			// get called for every bindable property's row, on every frame.
			// However, this is not currently a big concern because we only extend the widget row
			// in cases where there is one OuterObject (see IsPropertyExtendable above).
			// But the performance might degrade if we extend the widget row with the property binding
			// widget for a multiple-selection.
			return !FBlueprintWidgetCustomization::HasPropertyBindings(BlueprintEditorPtr, PropertyHandleRef);
		}));

	FProperty* Property = InPropertyHandle->GetProperty();
	FString DelegateName = Property->GetName() + "Delegate";

	// Don't show the property binding widget for child properties of a bindable property,
	// i.e. if the property's owner isn't the class itself.
	// This check is necessary because IsPropertyExtendable() accepts all bindable properties
	// and their descendants, primarily so we can disable the descendants' value widgets above.
	const UClass* OwnerClass = Property->GetOwner<UClass>();
	if (!OwnerClass) {
		return;
	}

	FDelegateProperty* DelegateProperty = FindFieldChecked<FDelegateProperty>(OwnerClass, FName(*DelegateName));

	const bool bIsEditable = Property->HasAnyPropertyFlags(CPF_Edit | CPF_EditConst);
	const bool bDoSignaturesMatch = DelegateProperty->SignatureFunction->GetReturnProperty()->SameType(Property);

	if ( !ensure(bIsEditable && bDoSignaturesMatch) )
	{
		return;
	}

	UWidgetBlueprint* WidgetBlueprint = BlueprintEditor.Pin()->GetWidgetBlueprintObj();

	if (!WidgetBlueprint->ArePropertyBindingsAllowed())
	{
		// Even if they don't want them on, we need to show them so they can remove them if they had any.
		if (BlueprintEditor.Pin()->GetWidgetBlueprintObj()->Bindings.Num() == 0)
		{
			return;
		}
	}

	InWidgetRow.ExtensionContent()
	[
		FBlueprintWidgetCustomization::MakePropertyBindingWidget(BlueprintEditor, DelegateProperty, InPropertyHandle.ToSharedRef(), true)
	];
}
