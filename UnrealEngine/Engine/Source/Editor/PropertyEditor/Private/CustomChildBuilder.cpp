// Copyright Epic Games, Inc. All Rights Reserved.

#include "CustomChildBuilder.h"
#include "Modules/ModuleManager.h"
#include "DetailGroup.h"
#include "PropertyHandleImpl.h"
#include "DetailPropertyRow.h"
#include "ObjectPropertyNode.h"
#include "SStandaloneCustomizedValueWidget.h"

IDetailChildrenBuilder& FCustomChildrenBuilder::AddCustomBuilder( TSharedRef<class IDetailCustomNodeBuilder> InCustomBuilder )
{
	FDetailLayoutCustomization NewCustomization;
	NewCustomization.CustomBuilderRow = MakeShareable( new FDetailCustomBuilderRow( InCustomBuilder ) );

	ChildCustomizations.Add( NewCustomization );
	return *this;
}

IDetailGroup& FCustomChildrenBuilder::AddGroup( FName GroupName, const FText& LocalizedDisplayName )
{
	FDetailLayoutCustomization NewCustomization;
	NewCustomization.DetailGroup = MakeShareable( new FDetailGroup( GroupName, ParentCategory.Pin().ToSharedRef(), LocalizedDisplayName ) );

	ChildCustomizations.Add( NewCustomization );

	return *NewCustomization.DetailGroup;
}

FDetailWidgetRow& FCustomChildrenBuilder::AddCustomRow( const FText& SearchString )
{
	const TSharedRef<FDetailWidgetRow> NewRow = MakeShared<FDetailWidgetRow>();
	FDetailLayoutCustomization NewCustomization;

	NewRow->FilterString( SearchString );

	// Bind to PasteFromText if specified
	if (const TSharedPtr<FOnPasteFromText> PasteFromTextDelegate = GetParentCategory().OnPasteFromText())
	{
		NewRow->OnPasteFromTextDelegate = PasteFromTextDelegate;
	}
	
	NewCustomization.WidgetDecl = NewRow;

	ChildCustomizations.Add( NewCustomization );
	return *NewRow;
}

IDetailPropertyRow& FCustomChildrenBuilder::AddProperty( TSharedRef<IPropertyHandle> PropertyHandle )
{
	check( PropertyHandle->IsValidHandle() )

	FDetailLayoutCustomization NewCustomization;
	NewCustomization.PropertyRow = MakeShareable( new FDetailPropertyRow( StaticCastSharedRef<FPropertyHandleBase>( PropertyHandle )->GetPropertyNode(), ParentCategory.Pin().ToSharedRef() ) );

	if (CustomResetChildToDefault.IsSet())
	{
		NewCustomization.PropertyRow->OverrideResetToDefault(CustomResetChildToDefault.GetValue());
	}

	ChildCustomizations.Add( NewCustomization );

	return *NewCustomization.PropertyRow;
}

IDetailPropertyRow* FCustomChildrenBuilder::AddExternalStructure(TSharedRef<FStructOnScope> ChildStructure, FName UniqueIdName)
{
	return AddExternalStructureProperty(ChildStructure, NAME_None, FAddPropertyParams().UniqueId(UniqueIdName));
}

IDetailPropertyRow* FCustomChildrenBuilder::AddExternalStructureProperty(TSharedRef<FStructOnScope> ChildStructure, FName PropertyName, const FAddPropertyParams& Params)
{
	FDetailLayoutCustomization NewCustomization;

	TSharedRef<FDetailCategoryImpl> ParentCategoryRef = ParentCategory.Pin().ToSharedRef();

	FDetailPropertyRow::MakeExternalPropertyRowCustomization(ChildStructure, PropertyName, ParentCategoryRef, NewCustomization, Params);

	TSharedPtr<FDetailPropertyRow> NewRow = NewCustomization.PropertyRow;

	if (NewRow.IsValid())
	{
		NewRow->SetCustomExpansionId(Params.GetUniqueId());

		TSharedPtr<FPropertyNode> PropertyNode = NewRow->GetPropertyNode();
		TSharedPtr<FComplexPropertyNode> RootNode = StaticCastSharedRef<FComplexPropertyNode>(PropertyNode->FindComplexParent()->AsShared());

		ChildCustomizations.Add(NewCustomization);
	}

	return NewRow.Get();
}

TArray<TSharedPtr<IPropertyHandle>> FCustomChildrenBuilder::AddAllExternalStructureProperties(TSharedRef<FStructOnScope> ChildStructure)
{
	return ParentCategory.Pin()->AddAllExternalStructureProperties(ChildStructure);
}

IDetailPropertyRow* FCustomChildrenBuilder::AddExternalObjects(const TArray<UObject*>& Objects, FName UniqueIdName)
{
	FAddPropertyParams Params = FAddPropertyParams()
		.UniqueId(UniqueIdName)
		.AllowChildren(true);

	return AddExternalObjects(Objects, Params);
}

IDetailPropertyRow* FCustomChildrenBuilder::AddExternalObjects(const TArray<UObject*>& Objects, const FAddPropertyParams& Params)
{
	return AddExternalObjectProperty(Objects, NAME_None, Params);
}

IDetailPropertyRow* FCustomChildrenBuilder::AddExternalObjectProperty(const TArray<UObject*>& Objects, FName PropertyName, const FAddPropertyParams& Params)
{
	TSharedRef<FDetailCategoryImpl> ParentCategoryRef = ParentCategory.Pin().ToSharedRef();

	FDetailLayoutCustomization NewCustomization;
	FDetailPropertyRow::MakeExternalPropertyRowCustomization(Objects, PropertyName, ParentCategoryRef, NewCustomization, Params);

	if (Params.ShouldHideRootObjectNode() && NewCustomization.HasPropertyNode() && NewCustomization.GetPropertyNode()->AsObjectNode())
	{
		NewCustomization.PropertyRow->SetForceShowOnlyChildren(true);
	}

	TSharedPtr<FDetailPropertyRow> NewRow = NewCustomization.PropertyRow;
	if (NewRow.IsValid())
	{
		NewRow->SetCustomExpansionId(Params.GetUniqueId());

		ChildCustomizations.Add(NewCustomization);
	}

	return NewRow.Get();
}

TSharedRef<SWidget> FCustomChildrenBuilder::GenerateStructValueWidget( TSharedRef<IPropertyHandle> StructPropertyHandle )
{
	FStructProperty* StructProperty = CastFieldChecked<FStructProperty>( StructPropertyHandle->GetProperty() );

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	
	IDetailsViewPrivate* DetailsView = ParentCategory.Pin()->GetDetailsView();

	FPropertyTypeLayoutCallback LayoutCallback = PropertyEditorModule.GetPropertyTypeCustomization(StructProperty, *StructPropertyHandle, DetailsView ? DetailsView->GetCustomPropertyTypeLayoutMap() : FCustomPropertyTypeLayoutMap() );
	if (LayoutCallback.IsValid())
	{
		TSharedRef<IPropertyTypeCustomization> CustomStructInterface = LayoutCallback.GetCustomizationInstance();

		return SNew( SStandaloneCustomizedValueWidget, CustomStructInterface, StructPropertyHandle).ParentCategory(ParentCategory.Pin().ToSharedRef());
	}
	else
	{
		// Uncustomized structs have nothing for their value content
		return SNullWidget::NullWidget;
	}
}

IDetailCategoryBuilder& FCustomChildrenBuilder::GetParentCategory() const
{
	return *ParentCategory.Pin();
}

FCustomChildrenBuilder& FCustomChildrenBuilder::OverrideResetChildrenToDefault(const FResetToDefaultOverride& ResetToDefault)
{
	CustomResetChildToDefault = ResetToDefault;
	return *this;
}

IDetailGroup* FCustomChildrenBuilder::GetParentGroup() const
{
	return ParentGroup.IsValid() ? ParentGroup.Pin().Get() : nullptr;
}
