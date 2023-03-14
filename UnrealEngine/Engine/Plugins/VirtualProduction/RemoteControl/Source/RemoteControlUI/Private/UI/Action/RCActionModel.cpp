// Copyright Epic Games, Inc. All Rights Reserved.

#include "RCActionModel.h"

#include "Commands/RemoteControlCommands.h"
#include "Controller/RCController.h"
#include "IDetailTreeNode.h"
#include "Interfaces/IMainFrameModule.h"
#include "IPropertyRowGenerator.h"
#include "RCVirtualProperty.h"
#include "RCVirtualPropertyContainer.h"
#include "RemoteControlField.h"
#include "RemoteControlPreset.h"
#include "Styling/RemoteControlStyles.h"
#include "UI/Action/SRCActionPanel.h"
#include "UI/Action/SRCVirtualPropertyWidget.h"
#include "UI/RCUIHelpers.h"
#include "UI/RemoteControlPanelStyle.h"
#include "UI/SRCPanelDragHandle.h"
#include "UI/SRCPanelExposedEntity.h"
#include "UI/SRemoteControlPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "RCActionModel"

namespace UE::RCActionPanelList
{
	namespace Columns
	{
		const FName VariableColor = TEXT("VariableColor");
		const FName DragDropHandle = TEXT("DragDropHandle");
		const FName Description = TEXT("Description");
		const FName Value = TEXT("Value");
	}

	class SActionItemListRow : public SMultiColumnTableRow<TSharedRef<FRCActionModel>>
	{
	public:
		void Construct(const FTableRowArgs& InArgs, const TSharedRef<STableViewBase>& OwnerTableView, TSharedRef<FRCActionModel> InActionItem)
		{
			ActionItem = InActionItem;
			FSuperRowType::Construct(InArgs, OwnerTableView);
		}

		TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			if (!ensure(ActionItem.IsValid()))
				return SNullWidget::NullWidget;

			if (ColumnName == UE::RCActionPanelList::Columns::VariableColor)
			{
				return ActionItem->GetTypeColorTagWidget();
			}
			else if (ColumnName == UE::RCActionPanelList::Columns::Description)
			{
				return ActionItem->GetNameWidget();
			}
			else if (ColumnName == UE::RCActionPanelList::Columns::Value)
			{
				return ActionItem->GetWidget();
			}

			// @todo: Implement Drag-Drop-handle column with Actions reordering support

			return SNullWidget::NullWidget;
		}

	private:
		TSharedPtr<FRCActionModel> ActionItem;
	};
}


FRCActionModel::FRCActionModel(URCAction* InAction, const TSharedPtr<class FRCBehaviourModel> InBehaviourItem, const TSharedPtr<SRemoteControlPanel> InRemoteControlPanel)
	: FRCLogicModeBase(InRemoteControlPanel),
	ActionWeakPtr(InAction)
{
	BehaviourItemWeakPtr = InBehaviourItem;

	RCPanelStyle = &FRemoteControlPanelStyle::Get()->GetWidgetStyle<FRCPanelStyle>("RemoteControlPanel.LogicControllersPanel");
}

TSharedRef<SWidget> FRCActionModel::GetNameWidget() const
{
	if(URCAction* Action = GetAction())
	{
		if (URemoteControlPreset* Preset = GetPreset())
		{
			if (const TSharedPtr<FRemoteControlField> RemoteControlField = Preset->GetExposedEntity<FRemoteControlField>(Action->ExposedFieldId).Pin())
			{
				return SNew(SBox)
					.Padding(FMargin(8.f))
					.VAlign(EVerticalAlignment::VAlign_Center)
					[
						SNew(STextBlock).Text(FText::FromName(RemoteControlField->GetLabel()))
					];
			}
		}
	}

	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> FRCActionModel::GetWidget() const
{
	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> FRCActionModel::GetTypeColorTagWidget() const
{
	const FLinearColor TypeColor = GetActionTypeColor();

	// Type Color Bar
	return SNew(SBox)
		.HeightOverride(5.f)
		[
			SNew(SBorder)
			.Visibility(EVisibility::HitTestInvisible)
			.BorderImage(FAppStyle::Get().GetBrush("NumericEntrySpinBox.NarrowDecorator"))
			.BorderBackgroundColor(TypeColor)
			.Padding(FMargin(5.0f, 0.0f, 0.0f, 0.f))
		];
}

URCAction* FRCActionModel::GetAction() const
{
	return ActionWeakPtr.Get();
}

TSharedRef<ITableRow> FRCActionModel::OnGenerateWidgetForList(TSharedPtr<FRCActionModel> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	typedef UE::RCActionPanelList::SActionItemListRow ActionRowType;

	return SNew(ActionRowType, OwnerTable, InItem.ToSharedRef())
		.Style(&RCPanelStyle->TableRowStyle)
		.Padding(FMargin(3.f));
}

TSharedPtr<SHeaderRow> FRCActionModel::GetHeaderRow()
{
	const FRCPanelStyle* RCPanelStyle = &FRemoteControlPanelStyle::Get()->GetWidgetStyle<FRCPanelStyle>("RemoteControlPanel.MinorPanel");

	return SNew(SHeaderRow)
		.Style(&RCPanelStyle->HeaderRowStyle)

		+ SHeaderRow::Column(UE::RCActionPanelList::Columns::VariableColor)
		.DefaultLabel(LOCTEXT("RCActionVariableColorColumnHeader", ""))
		.FixedWidth(5.f)
		.HeaderContentPadding(RCPanelStyle->HeaderRowPadding)

		+ SHeaderRow::Column(UE::RCActionPanelList::Columns::DragDropHandle)
		.DefaultLabel(LOCTEXT("RCActionDragDropHandleColumnHeader", ""))
		.FixedWidth(25.f)
		.HeaderContentPadding(RCPanelStyle->HeaderRowPadding)

		+ SHeaderRow::Column(UE::RCActionPanelList::Columns::Description)
		.DefaultLabel(LOCTEXT("RCActionDescColumnHeader", "Description"))
		.FillWidth(0.5f)
		.HeaderContentPadding(RCPanelStyle->HeaderRowPadding)

		+ SHeaderRow::Column(UE::RCActionPanelList::Columns::Value)
		.DefaultLabel(LOCTEXT("RCActionValueColumnHeader", "Value"))
		.FillWidth(0.5f)
		.HeaderContentPadding(RCPanelStyle->HeaderRowPadding);
}

TSharedPtr<FRCActionModel> FRCActionModel::GetModelByActionType(URCAction* InAction, const TSharedPtr<class FRCBehaviourModel> InBehaviourItem, const TSharedPtr<SRemoteControlPanel> InRemoteControlPanel)
{
	if (URCPropertyAction* PropertyAction = Cast<URCPropertyAction>(InAction))
	{
		return MakeShared<FRCPropertyActionModel>(PropertyAction, InBehaviourItem, InRemoteControlPanel);
	}
	else if (URCFunctionAction* FunctionAction = Cast<URCFunctionAction>(InAction))
	{
		return MakeShared<FRCFunctionActionModel>(FunctionAction, InBehaviourItem, InRemoteControlPanel);
	}
	else
		return nullptr;
}

FRCPropertyActionType::FRCPropertyActionType(URCPropertyAction* InPropertyAction)
{
	PropertyActionWeakPtr = InPropertyAction;

	FPropertyRowGeneratorArgs Args;
	Args.bShouldShowHiddenProperties = true;
	PropertyRowGenerator = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreatePropertyRowGenerator(Args);

	if (InPropertyAction)
	{
		// Generate UI widget for Action input
		if (const TSharedPtr<FStructOnScope> StructOnScope = InPropertyAction->PropertySelfContainer->CreateStructOnScope())
		{
			PropertyRowGenerator->SetStructure(StructOnScope);
			
			for (const TSharedRef<IDetailTreeNode>& CategoryNode : PropertyRowGenerator->GetRootTreeNodes())
			{
				TArray<TSharedRef<IDetailTreeNode>> Children;
				CategoryNode->GetChildren(Children);

				for (const TSharedRef<IDetailTreeNode>& Child : Children)
				{
					// Special handling for any Remote Control Property that is actually a single element of an Array
					// For example: "Override Materials" of Static Mesh Actor when exposed as a property in the Remote Control preset
					//
					if (TSharedPtr<FRemoteControlProperty> RemoteControlProperty = InPropertyAction->GetRemoteControlProperty())
					{
						if (ensure(RemoteControlProperty->FieldPathInfo.GetSegmentCount() > 0))
						{
							const FRCFieldPathSegment& LastSegment = RemoteControlProperty->FieldPathInfo.Segments.Last();
							const int32 ArrayIndex = LastSegment.ArrayIndex;

							// Is this a Container element?
							if (ArrayIndex != INDEX_NONE)
							{
								TArray<TSharedRef<IDetailTreeNode>> InnerChildren;
								Child->GetChildren(InnerChildren);

								// If yes, extract the Detail node corresponding to that element (rather than the parent container)
								if (ensure(InnerChildren.IsValidIndex(ArrayIndex)))
								{
									DetailTreeNodeWeakPtr = InnerChildren[ArrayIndex];
								}
							}
						}
					}

					// For regular properties (non-container)
					if (!DetailTreeNodeWeakPtr.IsValid())
					{
						DetailTreeNodeWeakPtr = Child;
						break;
					}
				}
			}
		}
	}
}

const FName& FRCPropertyActionType::GetPropertyName() const
{
	return PropertyActionWeakPtr.Get()->PropertySelfContainer->PropertyName;
}

TSharedRef<SWidget> FRCPropertyActionType::GetPropertyWidget() const
{
	return UE::RCUIHelpers::GetGenericFieldWidget(DetailTreeNodeWeakPtr.Pin());
}

FLinearColor FRCPropertyActionType::GetPropertyTypeColor() const
{
	if (!ensure(PropertyActionWeakPtr.IsValid()))
	{
		return FLinearColor::White;
	}

	const URCPropertyAction* PropertyAction = PropertyActionWeakPtr.Get();

	const FLinearColor TypeColor = UE::RCUIHelpers::GetFieldClassTypeColor(PropertyAction->PropertySelfContainer->GetProperty());

	return TypeColor;
}

void FRCActionModel::AddSpecialContextMenuOptions(FMenuBuilder& MenuBuilder)
{
	if (TSharedPtr<SRemoteControlPanel> RemoteControlPanel = PanelWeakPtr.Pin())
	{
		// 1. Edit (if available)
		if (EditableVirtualPropertyWidget)
		{
			EditableVirtualPropertyWidget->AddEditContextMenuOption(MenuBuilder);
		}
	}
}

void FRCActionModel::OnSelectionExit()
{
	if (EditableVirtualPropertyWidget)
	{
		EditableVirtualPropertyWidget->ExitEditMode();
	}
}

#undef LOCTEXT_NAMESPACE