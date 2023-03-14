// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeStateParametersDetails.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "IDetailPropertyRow.h"
#include "IDetailChildrenBuilder.h"
#include "Widgets/Text/STextBlock.h"
#include "PropertyBagDetails.h"
#include "StateTree.h"
#include "StateTreeEditorData.h"
#include "StateTreePropertyHelpers.h"
#include "StateTreeBindingExtension.h"

#define LOCTEXT_NAMESPACE "StateTreeEditor"


//----------------------------------------------------------------//
// FStateTreeStateParametersInstanceDataDetails
//----------------------------------------------------------------//

class FStateTreeStateParametersInstanceDataDetails : public FPropertyBagInstanceDataDetails
{
public:
	FStateTreeStateParametersInstanceDataDetails(TSharedPtr<IPropertyHandle> InStructProperty, IPropertyUtilities* InPropUtils, const bool bInFixedLayout, FGuid InID, UStateTreeEditorData* InEditorData)
		: FPropertyBagInstanceDataDetails(InStructProperty, InPropUtils, bInFixedLayout)
		, EditorData(InEditorData)
	{
		EditorPropBindings = EditorData ? EditorData->GetPropertyEditorBindings() : nullptr;
		ID = InID;
	}

	virtual void OnChildRowAdded(IDetailPropertyRow& ChildRow) override
	{
		if (!EditorPropBindings
			|| !bFixedLayout		// No binding for parameter definitions.
			|| !ID.IsValid())
		{
			FPropertyBagInstanceDataDetails::OnChildRowAdded(ChildRow);
			return;
		}

		TSharedPtr<IPropertyHandle> ChildPropHandle = ChildRow.GetPropertyHandle();
		const FProperty* Property = ChildPropHandle->GetProperty();

		// Set the category to Parameter so that the binding extension will pick it up.
		static const FName CategoryName(TEXT("Category"));
		ChildPropHandle->SetInstanceMetaData(CategoryName, TEXT("Parameter"));
	
		// Conditionally control visibility of the value field of bound properties.

		// Pass the node ID to binding extension. Since the properties are added using AddChildStructure(), we break the hierarchy and cannot access parent.
		ChildPropHandle->SetInstanceMetaData(UE::StateTree::PropertyBinding::StateTreeNodeIDName, LexToString(ID));

		FStateTreeEditorPropertyPath Path(ID, *Property->GetFName().ToString());
		TSharedPtr<SWidget> NameWidget;
		TSharedPtr<SWidget> ValueWidget;
		FDetailWidgetRow Row;
		ChildRow.GetDefaultWidgets(NameWidget, ValueWidget, Row);

		auto IsValueVisible = TAttribute<EVisibility>::Create([Path, this]() -> EVisibility
			{
				return EditorPropBindings->HasPropertyBinding(Path) ? EVisibility::Collapsed : EVisibility::Visible;
			});

		ChildRow
			.CustomWidget(/*bShowChildren*/true)
			.NameContent()
			[
				NameWidget.ToSharedRef()
			]
			.ValueContent()
			[
				SNew(SBox)
				.Visibility(IsValueVisible)
				[
					ValueWidget.ToSharedRef()
				]
			];
	}

	UStateTreeEditorData* EditorData;
	FStateTreeEditorPropertyBindings* EditorPropBindings;
	FGuid ID;
};


//----------------------------------------------------------------//
// FStateTreeStateParametersDetails
//----------------------------------------------------------------//

TSharedRef<IPropertyTypeCustomization> FStateTreeStateParametersDetails::MakeInstance()
{
	return MakeShareable(new FStateTreeStateParametersDetails);
}

void FStateTreeStateParametersDetails::CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructProperty = StructPropertyHandle;
	PropUtils = StructCustomizationUtils.GetPropertyUtilities().Get();

	ParametersProperty = StructProperty->GetChildHandle(TEXT("Parameters"));
	FixedLayoutProperty = StructProperty->GetChildHandle(TEXT("bFixedLayout"));
	IDProperty = StructProperty->GetChildHandle(TEXT("ID"));
	check(ParametersProperty.IsValid());
	check(FixedLayoutProperty.IsValid());
	check(IDProperty.IsValid());

	FindOuterObjects();
	
	bFixedLayout = false;
	FixedLayoutProperty->GetValue(bFixedLayout);

	TSharedPtr<SWidget> ValueWidget = SNullWidget::NullWidget;
	if (!bFixedLayout)
	{
		ValueWidget = FPropertyBagDetails::MakeAddPropertyWidget(ParametersProperty, PropUtils);
	}
	
	HeaderRow
		.NameContent()
		[
			ParametersProperty->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			ValueWidget.ToSharedRef()
		]
		.ShouldAutoExpand(true);
}

void FStateTreeStateParametersDetails::CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	FGuid ID;
	UE::StateTree::PropertyHelpers::GetStructValue<FGuid>(IDProperty, ID);

	// Show the Value (FInstancedStruct) as child rows.
	TSharedRef<FStateTreeStateParametersInstanceDataDetails> InstanceDetails = MakeShareable(new FStateTreeStateParametersInstanceDataDetails(ParametersProperty, PropUtils, bFixedLayout, ID, EditorData));
	StructBuilder.AddCustomBuilder(InstanceDetails);
}

void FStateTreeStateParametersDetails::FindOuterObjects()
{
	check(StructProperty);
	
	EditorData = nullptr;
	StateTree = nullptr;

	TArray<UObject*> OuterObjects;
	StructProperty->GetOuterObjects(OuterObjects);
	for (UObject* Outer : OuterObjects)
	{
		UStateTreeEditorData* OuterEditorData = Outer->GetTypedOuter<UStateTreeEditorData>();
		UStateTree* OuterStateTree = OuterEditorData ? OuterEditorData->GetTypedOuter<UStateTree>() : nullptr;
		if (OuterEditorData && OuterStateTree)
		{
			StateTree = OuterStateTree;
			EditorData = OuterEditorData;
			break;
		}
	}
}


#undef LOCTEXT_NAMESPACE
