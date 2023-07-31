// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeEditorDataDetails.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "StateTreeEditorData.h"
#include "StateTreeViewModel.h"
#include "StateTree.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "IPropertyUtilities.h"
#include "PropertyBagDetails.h"
#include "StateTreeEditorStyle.h"
#include "Styling/StyleColors.h"

#define LOCTEXT_NAMESPACE "StateTreeEditor"

TSharedRef<IDetailCustomization> FStateTreeEditorDataDetails::MakeInstance()
{
	return MakeShareable(new FStateTreeEditorDataDetails);
}

void FStateTreeEditorDataDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Find StateTreeEditorData associated with this panel.
	const UStateTreeEditorData* EditorData = nullptr;
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	for (TWeakObjectPtr<UObject>& WeakObject : Objects)
	{
		if (UStateTreeEditorData* Object = Cast<UStateTreeEditorData>(WeakObject.Get()))
		{
			EditorData = Object;
			break;
		}
	}
	const UStateTreeSchema* Schema = EditorData ? EditorData->Schema : nullptr;

	// Common category
	IDetailCategoryBuilder& CommonCategory = DetailBuilder.EditCategory(TEXT("Common"), LOCTEXT("EditorDataDetailsCommon", "Common"));
	CommonCategory.SetSortOrder(0);

	// Context category
	IDetailCategoryBuilder& ContextDataCategory = DetailBuilder.EditCategory(TEXT("Context"), LOCTEXT("EditorDataDetailsContext", "Context"));
	ContextDataCategory.SetSortOrder(1);

	if (Schema != nullptr)
	{
		for (const FStateTreeExternalDataDesc& ContextData : Schema->GetContextDataDescs())
		{
			if (ContextData.Struct == nullptr)
			{
				continue;
			}
			
			FEdGraphPinType PinType;
			PinType.PinSubCategory = NAME_None;
			if (ContextData.Struct->IsA<UScriptStruct>())
			{
				PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
			}
			else if (ContextData.Struct->IsA<UClass>())
			{
				PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
			}
			else
			{
				continue;
			}
			PinType.PinSubCategoryObject = const_cast<UStruct*>(ContextData.Struct.Get());

			const UEdGraphSchema_K2* EdGraphSchema = GetDefault<UEdGraphSchema_K2>();
			const FSlateBrush* Icon = FBlueprintEditorUtils::GetIconFromPin(PinType, true);
			const FLinearColor Color = EdGraphSchema->GetPinTypeColor(PinType);

			const FText DataName = FText::FromName(ContextData.Name);
			const FText DataType = ContextData.Struct != nullptr ? ContextData.Struct->GetDisplayNameText() : FText::GetEmpty();
			
			ContextDataCategory.AddCustomRow(DataName)
				.NameContent()
				[
					SNew(SHorizontalBox)
					
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(IDetailLayoutBuilder::GetDetailFont())
						.Text(DataName)
					]
					
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(4.0f, 0.0f)
					[
						SNew(SBorder)
						.Padding(FMargin(6, 1))
						.BorderImage(new FSlateRoundedBoxBrush(FStyleColors::Hover, 6))
						[
							SNew(STextBlock)
							.TextStyle(FStateTreeEditorStyle::Get(), "StateTree.Node.Operand")
							.ColorAndOpacity(FStyleColors::Foreground)
							.Text(LOCTEXT("LabelContext", "CONTEXT"))
							.ToolTipText(LOCTEXT("ContextSourceTooltip", "This is Context Object, it passed in from where the StateTree is being used."))
						]
					]
				]
				.ValueContent()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(4.0f, 0.0f)
					[
						SNew(SImage)
						.Image(Icon)
						.ColorAndOpacity(Color)
					]
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(IDetailLayoutBuilder::GetDetailFont())
						.Text(DataType)
					]
				];
		}
	}
	
	// Parameters category
	IDetailCategoryBuilder& ParametersCategory = DetailBuilder.EditCategory(TEXT("Parameters"), LOCTEXT("EditorDataDetailsParameters", "Parameters"));
	ParametersCategory.SetSortOrder(2);
	{
		// Show parameters as a category.
		IPropertyUtilities* PropUtils = &DetailBuilder.GetPropertyUtilities().Get();
		TSharedPtr<IPropertyHandle> RootParametersProperty = DetailBuilder.GetProperty(TEXT("RootParameters")); // FStateTreeStateParameters
		check(RootParametersProperty);
		RootParametersProperty->MarkHiddenByCustomization();

		TSharedPtr<IPropertyHandle> ParametersProperty = RootParametersProperty->GetChildHandle(TEXT("Parameters")); // FInstancedPropertyBag
		check(ParametersProperty);

		TSharedRef<SHorizontalBox> HeaderContentWidget = SNew(SHorizontalBox);
		HeaderContentWidget->AddSlot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		[
			FPropertyBagDetails::MakeAddPropertyWidget(ParametersProperty, PropUtils).ToSharedRef()
		];
		ParametersCategory.HeaderContent(HeaderContentWidget);

		TSharedRef<FPropertyBagInstanceDataDetails> InstanceDetails = MakeShareable(new FPropertyBagInstanceDataDetails(ParametersProperty, PropUtils, false));
		ParametersCategory.AddCustomBuilder(InstanceDetails);
	}

	// Evaluators category
	TSharedPtr<IPropertyHandle> EvaluatorsProperty = DetailBuilder.GetProperty(TEXT("Evaluators"));
	check(EvaluatorsProperty.IsValid());
	const FName EvalCategoryName(TEXT("Evaluators"));
	if (Schema && Schema->AllowEvaluators())
	{
		MakeArrayCategory(DetailBuilder, EvalCategoryName, LOCTEXT("EditorDataDetailsEvaluators", "Evaluators"), /*SortOrder*/3, EvaluatorsProperty);
	}
	else
	{
		DetailBuilder.EditCategory(EvalCategoryName).SetCategoryVisibility(false);
	}

	// Refresh the UI when the Schema changes.	
	TSharedPtr<IPropertyHandle> SchemaProperty = DetailBuilder.GetProperty(TEXT("Schema"));
	check(SchemaProperty.IsValid());
	TSharedPtr<IPropertyUtilities> PropUtils = DetailBuilder.GetPropertyUtilities();
	SchemaProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([PropUtils] ()
	{
		if (PropUtils.IsValid())
		{
			PropUtils->ForceRefresh();
		}
	}));
	SchemaProperty->SetOnChildPropertyValueChanged(FSimpleDelegate::CreateLambda([PropUtils] ()
	{
		if (PropUtils.IsValid())
		{
			PropUtils->ForceRefresh();
		}
	}));

}

void FStateTreeEditorDataDetails::MakeArrayCategory(IDetailLayoutBuilder& DetailBuilder, FName CategoryName, const FText& DisplayName, int32 SortOrder, TSharedPtr<IPropertyHandle> PropertyHandle)
{
	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(CategoryName, DisplayName);
	Category.SetSortOrder(SortOrder);

	TSharedRef<SHorizontalBox> HeaderContentWidget = SNew(SHorizontalBox);
	HeaderContentWidget->AddSlot()
	.HAlign(HAlign_Right)
	.VAlign(VAlign_Center)
	[
		PropertyHandle->CreateDefaultPropertyButtonWidgets()
	];
	Category.HeaderContent(HeaderContentWidget);

	// Add items inline
	TSharedRef<FDetailArrayBuilder> Builder = MakeShareable(new FDetailArrayBuilder(PropertyHandle.ToSharedRef(), /*InGenerateHeader*/ false, /*InDisplayResetToDefault*/ true, /*InDisplayElementNum*/ false));
	Builder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateLambda([](TSharedRef<IPropertyHandle> PropertyHandle, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder)
		{
			ChildrenBuilder.AddProperty(PropertyHandle);
		}));
	Category.AddCustomBuilder(Builder, /*bForAdvanced*/ false);
}

#undef LOCTEXT_NAMESPACE
