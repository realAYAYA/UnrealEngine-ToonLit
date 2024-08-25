// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeStateDetails.h"
#include "IDetailChildrenBuilder.h"
#include "DetailLayoutBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "IPropertyUtilities.h"
#include "StateTreeEditor.h"
#include "StateTreeEditorData.h"
#include "StateTreeSchema.h"
#include "Debugger/StateTreeDebuggerUIExtensions.h"

#define LOCTEXT_NAMESPACE "StateTreeEditor"


TSharedRef<IDetailCustomization> FStateTreeStateDetails::MakeInstance()
{
	return MakeShareable(new FStateTreeStateDetails);
}

void FStateTreeStateDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Find StateTreeEditorData associated with this panel.
	UStateTreeEditorData* EditorData = nullptr;
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	for (TWeakObjectPtr<UObject>& WeakObject : Objects)
	{
		if (const UObject* Object = WeakObject.Get())
		{
			if (UStateTreeEditorData* OuterEditorData = Object->GetTypedOuter<UStateTreeEditorData>())
			{
				EditorData = OuterEditorData;
				break;
			}
		}
	}
	const UStateTreeSchema* Schema = EditorData ? EditorData->Schema : nullptr;

	const TSharedPtr<IPropertyHandle> IDProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStateTreeState, ID));
	const TSharedPtr<IPropertyHandle> EnabledProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStateTreeState, bEnabled));
	const TSharedPtr<IPropertyHandle> TasksProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStateTreeState, Tasks));
	const TSharedPtr<IPropertyHandle> SingleTaskProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStateTreeState, SingleTask));
	const TSharedPtr<IPropertyHandle> EnterConditionsProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStateTreeState, EnterConditions));
	const TSharedPtr<IPropertyHandle> TransitionsProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStateTreeState, Transitions));
	const TSharedPtr<IPropertyHandle> TypeProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStateTreeState, Type));
	const TSharedPtr<IPropertyHandle> LinkedSubtreeProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStateTreeState, LinkedSubtree));
	const TSharedPtr<IPropertyHandle> LinkedAssetProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStateTreeState, LinkedAsset));
	const TSharedPtr<IPropertyHandle> ParametersProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStateTreeState, Parameters));
	const TSharedPtr<IPropertyHandle> SelectionBehaviorProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStateTreeState, SelectionBehavior));

	EnabledProperty->MarkHiddenByCustomization();
	if (UE::StateTree::Editor::GbDisplayItemIds == false)
	{
		IDProperty->MarkHiddenByCustomization();
	}

	uint8 StateTypeValue = 0;
	TypeProperty->GetValue(StateTypeValue);
	const EStateTreeStateType StateType = (EStateTreeStateType)StateTypeValue;
	
	IDetailCategoryBuilder& StateCategory = DetailBuilder.EditCategory(TEXT("State"), LOCTEXT("StateDetailsState", "State"));
	StateCategory.SetSortOrder(0);

	StateCategory.HeaderContent(UE::StateTreeEditor::DebuggerExtensions::CreateStateWidget(DetailBuilder, EditorData));

	if (StateType != EStateTreeStateType::Linked)
	{
		LinkedSubtreeProperty->MarkHiddenByCustomization();
	}
	if (StateType != EStateTreeStateType::LinkedAsset)
	{
		LinkedAssetProperty->MarkHiddenByCustomization();
	}
	
	if (StateType == EStateTreeStateType::Linked || StateType == EStateTreeStateType::LinkedAsset)
	{
		SelectionBehaviorProperty->MarkHiddenByCustomization();
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

	const TSharedRef<SHorizontalBox> HeaderContentWidget = SNew(SHorizontalBox)
		.IsEnabled(DetailBuilder.GetPropertyUtilities(), &IPropertyUtilities::IsPropertyEditingEnabled);

	HeaderContentWidget->AddSlot()
	.HAlign(HAlign_Right)
	.VAlign(VAlign_Center)
	[
		PropertyHandle->CreateDefaultPropertyButtonWidgets()
	];
	Category.HeaderContent(HeaderContentWidget);

	// Add items inline
	const TSharedRef<FDetailArrayBuilder> Builder = MakeShareable(new FDetailArrayBuilder(PropertyHandle.ToSharedRef(), /*InGenerateHeader*/ false, /*InDisplayResetToDefault*/ true, /*InDisplayElementNum*/ false));
	Builder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateLambda([](TSharedRef<IPropertyHandle> PropertyHandle, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder)
	{
		ChildrenBuilder.AddProperty(PropertyHandle);
	}));
	Category.AddCustomBuilder(Builder, /*bForAdvanced*/ false);
}

#undef LOCTEXT_NAMESPACE
