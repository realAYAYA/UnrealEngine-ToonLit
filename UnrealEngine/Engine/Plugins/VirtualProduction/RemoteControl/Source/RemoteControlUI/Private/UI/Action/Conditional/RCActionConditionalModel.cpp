// Copyright Epic Games, Inc. All Rights Reserved.

#include "RCActionConditionalModel.h"

#include "Behaviour/Builtin/Conditional/RCBehaviourConditional.h"
#include "Controller/RCController.h"
#include "IDetailTreeNode.h"
#include "IPropertyRowGenerator.h"
#include "RCVirtualProperty.h"
#include "RCVirtualPropertyContainer.h"
#include "RemoteControlField.h"
#include "RemoteControlPreset.h"
#include "Styling/RemoteControlStyles.h"
#include "UI/Action/SRCVirtualPropertyWidget.h"
#include "UI/Behaviour/Builtin/Conditional/RCBehaviourConditionalModel.h"
#include "UI/RCUIHelpers.h"
#include "UI/RemoteControlPanelStyle.h"
#include "UI/SRCPanelDragHandle.h"
#include "UI/SRCPanelExposedEntity.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "RCActionModel"

namespace UE::RCActionPanelConditionalList
{
	namespace Columns
	{
		const FName TypeColorTag = TEXT("TypeColorTag");
		const FName DragDropHandle = TEXT("DragDropHandle");
		const FName Condition = TEXT("Condition");
		const FName Description = TEXT("Description");
		const FName Value = TEXT("Value");
	}

	class SActionItemListRow : public SMultiColumnTableRow<TSharedRef<FRCActionModel>>
	{
	public:
		void Construct(const FTableRowArgs& InArgs, const TSharedRef<STableViewBase>& OwnerTableView, TSharedRef<FRCActionConditionalModel> InActionItem)
		{
			ActionItem = InActionItem;
			FSuperRowType::Construct(InArgs, OwnerTableView);
		}

		TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			if (!ensure(ActionItem.IsValid()))
				return SNullWidget::NullWidget;

			if (ColumnName == UE::RCActionPanelConditionalList::Columns::TypeColorTag)
			{
				return ActionItem->GetTypeColorTagWidget();
			}
			else if (ColumnName == UE::RCActionPanelConditionalList::Columns::Condition)
			{
				return ActionItem->GetConditionWidget();
			}
			else if (ColumnName == UE::RCActionPanelConditionalList::Columns::Description)
			{
				return ActionItem->GetNameWidget();
			}
			else if (ColumnName == UE::RCActionPanelConditionalList::Columns::Value)
			{
				return ActionItem->GetWidget();
			}

			// @todo: Implement Drag-Drop-handle column with Actions reordering support

			return SNullWidget::NullWidget;
		}

	private:
		TSharedPtr<FRCActionConditionalModel> ActionItem;
	};
}

TSharedPtr<SHeaderRow> FRCActionConditionalModel::GetHeaderRow()
{
	const FRCPanelStyle* RCPanelStyle = &FRemoteControlPanelStyle::Get()->GetWidgetStyle<FRCPanelStyle>("RemoteControlPanel.MinorPanel");

	return SNew(SHeaderRow)
		.Style(&RCPanelStyle->HeaderRowStyle)

		+ SHeaderRow::Column(UE::RCActionPanelConditionalList::Columns::TypeColorTag)
		.DefaultLabel(LOCTEXT("RCActionTypeColorColumnHeader", ""))
		.FixedWidth(5.f)
		.HeaderContentPadding(RCPanelStyle->HeaderRowPadding)

		+ SHeaderRow::Column(UE::RCActionPanelConditionalList::Columns::DragDropHandle)
		.DefaultLabel(LOCTEXT("RCActionDragDropHandleColumnHeader", ""))
		.FixedWidth(25.f)
		.HeaderContentPadding(RCPanelStyle->HeaderRowPadding)

		+ SHeaderRow::Column(UE::RCActionPanelConditionalList::Columns::Condition)
		.DefaultLabel(LOCTEXT("RCActionConditionColumnHeader", "Condition"))
		.FillWidth(0.2f)
		.HeaderContentPadding(RCPanelStyle->HeaderRowPadding)

		+ SHeaderRow::Column(UE::RCActionPanelConditionalList::Columns::Description)
		.DefaultLabel(LOCTEXT("RCActionDescColumnHeader", "Description"))
		.FillWidth(0.4f)
		.HeaderContentPadding(RCPanelStyle->HeaderRowPadding)

		+ SHeaderRow::Column(UE::RCActionPanelConditionalList::Columns::Value)
		.DefaultLabel(LOCTEXT("RCActionValueColumnHeader", "Value"))
		.FillWidth(0.4f)
		.HeaderContentPadding(RCPanelStyle->HeaderRowPadding);
}

FRCActionConditionalModel::FRCActionConditionalModel(URCAction* InAction, const TSharedPtr<class FRCBehaviourModel> InBehaviourItem, const TSharedPtr<SRemoteControlPanel> InRemoteControlPanel)
	: FRCActionModel(InAction, InBehaviourItem, InRemoteControlPanel)
{
}

TSharedPtr<FRCActionConditionalModel> FRCActionConditionalModel::GetModelByActionType(URCAction* InAction, const TSharedPtr<class FRCBehaviourModel> InBehaviourItem, const TSharedPtr<SRemoteControlPanel> InRemoteControlPanel)
{
	if (URCPropertyAction* PropertyAction = Cast<URCPropertyAction>(InAction))
	{
		return MakeShared<FRCPropertyActionConditionalModel>(PropertyAction, InBehaviourItem, InRemoteControlPanel);
	}
	else if (URCFunctionAction* FunctionAction = Cast<URCFunctionAction>(InAction))
	{
		return MakeShared<FRCFunctionActionConditionalModel>(FunctionAction, InBehaviourItem, InRemoteControlPanel);
	}

	return nullptr;
}

TSharedRef<SWidget> FRCActionConditionalModel::GetConditionWidget()
{
	if (TSharedPtr<FRCBehaviourConditionalModel> BehaviourItem = StaticCastSharedPtr<FRCBehaviourConditionalModel>(BehaviourItemWeakPtr.Pin()))
	{
		if (URCBehaviourConditional* Behaviour = Cast<URCBehaviourConditional>(BehaviourItem->GetBehaviour()))
		{
			if (FRCBehaviourCondition* Condition = Behaviour->Conditions.Find(GetAction()))
			{
				if(ensure(Condition->Comparand))
				{
					return SAssignNew(EditableVirtualPropertyWidget, SRCVirtualPropertyWidget, Condition->Comparand)
						.OnGenerateWidget(this, &FRCActionConditionalModel::OnGenerateConditionWidget);
				}
			}
		}
	}

	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> FRCActionConditionalModel::OnGenerateConditionWidget(URCVirtualPropertySelfContainer* InComparand)
{
	if (!ensure(InComparand))
	{
		return SNullWidget::NullWidget;
	}

	if (TSharedPtr<FRCBehaviourConditionalModel> BehaviourItem = StaticCastSharedPtr<FRCBehaviourConditionalModel>(BehaviourItemWeakPtr.Pin()))
	{
		if (URCBehaviourConditional* Behaviour = Cast<URCBehaviourConditional>(BehaviourItem->GetBehaviour()))
		{
			if (FRCBehaviourCondition* Condition = Behaviour->Conditions.Find(GetAction()))
			{
				const FText ConditionText = Behaviour->GetConditionTypeAsText(Condition->ConditionType);

				FText ConditionComparandText;

				if (Condition->ConditionType == ERCBehaviourConditionType::Else)
				{
					ConditionComparandText = ConditionText; // Else doesn't need to display the comparand
				}
				else
				{
					const FText ComparandValueText = FText::FromName(*Condition->Comparand->GetDisplayValueAsString());

					ConditionComparandText = FText::Format(FText::FromName("{0} {1}"), ConditionText, ComparandValueText);
				}

				return SNew(STextBlock).Text(ConditionComparandText);
			}
		}
	}

	return SNullWidget::NullWidget;
}

TSharedRef<ITableRow> FRCActionConditionalModel::OnGenerateWidgetForList(TSharedPtr<FRCActionConditionalModel> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	typedef UE::RCActionPanelConditionalList::SActionItemListRow ActionRowType;

	return SNew(ActionRowType, OwnerTable, InItem.ToSharedRef())
		.Style(&RCPanelStyle->TableRowStyle)
		.Padding(FMargin(3.f));
}

#undef LOCTEXT_NAMESPACE