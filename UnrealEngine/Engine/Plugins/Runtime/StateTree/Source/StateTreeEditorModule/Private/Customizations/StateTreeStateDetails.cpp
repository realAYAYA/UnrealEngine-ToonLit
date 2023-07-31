// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeStateDetails.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "IPropertyUtilities.h"
#include "StateTreeEditorData.h"
#include "StateTreeViewModel.h"
#include "StateTree.h"


#define LOCTEXT_NAMESPACE "StateTreeEditor"


TSharedRef<IDetailCustomization> FStateTreeStateDetails::MakeInstance()
{
	return MakeShareable(new FStateTreeStateDetails);
}

void FStateTreeStateDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Find StateTreeEditorData associated with this panel.
	const UStateTreeEditorData* EditorData = nullptr;
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	for (TWeakObjectPtr<UObject>& WeakObject : Objects)
	{
		if (const UObject* Object = WeakObject.Get())
		{
			const UStateTreeEditorData* OuterEditorData = Object->GetTypedOuter<UStateTreeEditorData>();
			{
				EditorData = OuterEditorData;
				break;
			}
		}
	}
	const UStateTreeSchema* Schema = EditorData ? EditorData->Schema : nullptr;

	TSharedPtr<IPropertyHandle> TasksProperty = DetailBuilder.GetProperty(TEXT("Tasks"));
	TSharedPtr<IPropertyHandle> SingleTaskProperty = DetailBuilder.GetProperty(TEXT("SingleTask"));
	TSharedPtr<IPropertyHandle> EnterConditionsProperty = DetailBuilder.GetProperty(TEXT("EnterConditions"));
	TSharedPtr<IPropertyHandle> TransitionsProperty = DetailBuilder.GetProperty(TEXT("Transitions"));
	TSharedPtr<IPropertyHandle> TypeProperty = DetailBuilder.GetProperty(TEXT("Type"));
	TSharedPtr<IPropertyHandle> LinkedStateProperty = DetailBuilder.GetProperty(TEXT("LinkedState"));
	TSharedPtr<IPropertyHandle> ParametersProperty = DetailBuilder.GetProperty(TEXT("Parameters"));

	uint8 StateTypeValue = 0;
	TypeProperty->GetValue(StateTypeValue);
	const EStateTreeStateType StateType = (EStateTreeStateType)StateTypeValue;
	
	IDetailCategoryBuilder& StateCategory = DetailBuilder.EditCategory(TEXT("State"), LOCTEXT("StateDetailsState", "State"));
	StateCategory.SetSortOrder(0);

	if (StateType != EStateTreeStateType::Linked)
	{
		LinkedStateProperty->MarkHiddenByCustomization();
	}
	
	if (!(StateType == EStateTreeStateType::Subtree || StateType == EStateTreeStateType::Linked))
	{
		ParametersProperty->MarkHiddenByCustomization();
	}
	
	const FName EnterConditionsCategoryName(TEXT("Enter Conditions"));
	if (Schema && Schema->AllowEnterConditions())
	{
		MakeArrayCategory(DetailBuilder, EnterConditionsCategoryName, LOCTEXT("StateDetailsEnterConditions", "Enter Conditions"), 2, EnterConditionsProperty);
	}
	else
	{
		DetailBuilder.EditCategory(EnterConditionsCategoryName).SetCategoryVisibility(false);
	}

	if ((StateType == EStateTreeStateType::State || StateType == EStateTreeStateType::Subtree))
	{
		if (Schema && Schema->AllowMultipleTasks())
		{
			const FName TasksCategoryName(TEXT("Tasks"));
			MakeArrayCategory(DetailBuilder, TasksCategoryName, LOCTEXT("StateDetailsTasks", "Tasks"), 3, TasksProperty);
			SingleTaskProperty->MarkHiddenByCustomization();
		}
		else
		{
			const FName TaskCategoryName(TEXT("Task"));
			IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(TaskCategoryName);
			Category.SetSortOrder(3);
			
			IDetailPropertyRow& Row = Category.AddProperty(SingleTaskProperty);
			Row.ShouldAutoExpand(true);
			
			TasksProperty->MarkHiddenByCustomization();
		}
	}
	else
	{
		SingleTaskProperty->MarkHiddenByCustomization();
		TasksProperty->MarkHiddenByCustomization();
	}

	MakeArrayCategory(DetailBuilder, "Transitions", LOCTEXT("StateDetailsTransitions", "Transitions"), 4, TransitionsProperty);

	// Refresh the UI when the type changes.	
	TSharedPtr<IPropertyUtilities> PropUtils = DetailBuilder.GetPropertyUtilities();
	TypeProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([PropUtils] ()
	{
		if (PropUtils.IsValid())
		{
			PropUtils->ForceRefresh();
		}
	}));
}

void FStateTreeStateDetails::MakeArrayCategory(IDetailLayoutBuilder& DetailBuilder, FName CategoryName, const FText& DisplayName, int32 SortOrder, TSharedPtr<IPropertyHandle> PropertyHandle)
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
