// Copyright Epic Games, Inc. All Rights Reserved.

#include "SRCControllerPanelList.h"

#include "Behaviour/Builtin/Bind/RCBehaviourBind.h"
#include "Behaviour/Builtin/Bind/RCBehaviourBindNode.h"
#include "Commands/RemoteControlCommands.h"
#include "Controller/RCController.h"
#include "Controller/RCControllerContainer.h"
#include "IDetailTreeNode.h"
#include "Interfaces/IMainFrameModule.h"
#include "IPropertyRowGenerator.h"
#include "IRemoteControlModule.h"
#include "RCControllerModel.h"
#include "RCVirtualProperty.h"
#include "RCVirtualPropertyContainer.h"
#include "RemoteControlField.h"
#include "RemoteControlPreset.h"
#include "SDropTarget.h"
#include "SlateOptMacros.h"
#include "SRCControllerPanel.h"
#include "Styling/RemoteControlStyles.h"
#include "UI/Action/SRCActionPanelList.h"
#include "UI/BaseLogicUI/RCLogicModeBase.h"
#include "UI/RCUIHelpers.h"
#include "UI/RemoteControlPanelStyle.h"
#include "UI/SRCPanelExposedEntity.h"
#include "UI/SRCPanelDragHandle.h"
#include "UI/SRemoteControlPanel.h"
#include "Widgets/Views/SHeaderRow.h"

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

#define LOCTEXT_NAMESPACE "SRCControllerPanelList"

namespace UE::RCControllerPanelList
{
	enum class EDragDropSupportedModes : uint8
	{
		ReorderOnly,
		All,
		None
	};

	namespace Columns
	{
		const FName TypeColor = TEXT("TypeColor");
		const FName Name = TEXT("Controller Name");
		const FName Value = TEXT("Controller Value");
		const FName DragHandle = TEXT("Drag Handle");
	}

	class SControllerItemListRow : public SMultiColumnTableRow<TSharedRef<FRCControllerModel>>
	{
	public:
		void Construct(const FTableRowArgs& InArgs, const TSharedRef<STableViewBase>& OwnerTableView, TSharedRef<FRCControllerModel> InControllerItem, TSharedRef<SRCControllerPanelList> InControllerPanelList)
		{
			ControllerItem = InControllerItem;
			ControllerPanelList = InControllerPanelList;
			FSuperRowType::Construct(InArgs, OwnerTableView);
		}

		TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			if (!ensure(ControllerItem.IsValid()))
				return SNullWidget::NullWidget;


			if (ColumnName == UE::RCControllerPanelList::Columns::TypeColor)
			{
				if (URCController* Controller = Cast< URCController>(ControllerItem->GetVirtualProperty()))
				{
					return UE::RCUIHelpers::GetTypeColorWidget(Controller->GetProperty());
				}
			}
			else if (ColumnName == UE::RCControllerPanelList::Columns::Name)
			{
				return WrapWithDropTarget(ControllerItem->GetNameWidget());
			}
			else if (ColumnName == UE::RCControllerPanelList::Columns::Value)
			{
				return ControllerItem->GetWidget();
			}
			else if (ColumnName == UE::RCControllerPanelList::Columns::DragHandle)
			{
				SAssignNew(DragDropBorderWidget, SBorder)
					.BorderImage(FRemoteControlPanelStyle::Get()->GetBrush("RemoteControlPanel.ExposedFieldBorder"));

				TSharedRef<SWidget> DragHandleWidget = 
					SNew(SBox)
					.Padding(5.f)
					[
						SNew(SRCPanelDragHandle<FRCControllerDragDrop>, ControllerItem->GetId())
						.Widget(DragDropBorderWidget)
					];

				return WrapWithDropTarget(DragHandleWidget, EDragDropSupportedModes::ReorderOnly /*Allow reorder operation only*/);
			}

			return SNullWidget::NullWidget;
		}

		TSharedPtr<SBorder> DragDropBorderWidget;


		void OnDragEnter(FGeometry const& MyGeometry, FDragDropEvent const& DragDropEvent) override
		{
			bIsDragActive = true;
		}


		void OnDragLeave(FDragDropEvent const& DragDropEvent) override
		{
			bIsDragActive = false;
		}

	protected:

	private:

		TSharedRef<SWidget> WrapWithDropTarget(const TSharedRef<SWidget> InWidget, const EDragDropSupportedModes SupportedMode = EDragDropSupportedModes::All)
		{
			return SNew(SDropTarget)
				.ValidColor(FStyleColors::AccentOrange)
				.VerticalImage(FRemoteControlPanelStyle::Get()->GetBrush("RemoteControlPanel.VerticalDash"))
				.HorizontalImage(FRemoteControlPanelStyle::Get()->GetBrush("RemoteControlPanel.HorizontalDash"))
				.OnDropped_Lambda([this](const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent) { return SControllerItemListRow::OnControllerItemDragDrop(InDragDropEvent.GetOperation()); })
				.OnAllowDrop(this, &SControllerItemListRow::OnAllowDrop, SupportedMode)
				.OnIsRecognized(this, &SControllerItemListRow::OnAllowDrop, SupportedMode)
				[
					InWidget
				];
		}

		FReply OnControllerItemDragDrop(TSharedPtr<FDragDropOperation> DragDropOperation)
		{
			bIsDragActive = false;
			ControllerPanelList->bIsAnyControllerItemEligibleForDragDrop = false;

			if (!DragDropOperation)
			{
				return FReply::Handled();
			}

			if (DragDropOperation->IsOfType<FRCControllerDragDrop>())
			{
				if (TSharedPtr<FRCControllerDragDrop> DragDropOp = StaticCastSharedPtr<FRCControllerDragDrop>(DragDropOperation))
				{
					const FGuid DragDropControllerId = DragDropOp->GetId();

					if (ControllerPanelList.IsValid())
					{
						TSharedPtr<FRCControllerModel> DragDropControllerItem = ControllerPanelList->FindControllerItemById(DragDropControllerId);

						if (ensure(ControllerItem && DragDropControllerItem))
						{
							ControllerPanelList->ReorderControllerItem(DragDropControllerItem.ToSharedRef(), ControllerItem.ToSharedRef());
						}
					}
				}
			}
			else if (DragDropOperation->IsOfType<FExposedEntityDragDrop>())
			{
				if (TSharedPtr<FExposedEntityDragDrop> DragDropOp = StaticCastSharedPtr<FExposedEntityDragDrop>(DragDropOperation))
				{
					// Fetch the Exposed Entity
					const FGuid ExposedEntityId = DragDropOp->GetId();

					if (URemoteControlPreset* Preset = ControllerPanelList->GetPreset())
					{
						if (TSharedPtr<const FRemoteControlProperty> RemoteControlProperty = Preset->GetExposedEntity<FRemoteControlProperty>(ExposedEntityId).Pin())
						{
							if (URCController* Controller = Cast<URCController>(ControllerItem->GetVirtualProperty()))
							{
								ControllerPanelList->CreateBindBehaviourAndAssignTo(Controller, RemoteControlProperty.ToSharedRef(), true);
							}
						}
					}
				}
			}

			return FReply::Handled();
		}

		bool OnAllowDrop(TSharedPtr<FDragDropOperation> DragDropOperation, const EDragDropSupportedModes SupportedMode)
		{
			if (!ensure(ControllerPanelList))
			{
				return false;
			}

			if (DragDropOperation && ControllerItem)
			{
				// Dragging Controllers onto Controllers (Reordering)
				if (DragDropOperation->IsOfType<FRCControllerDragDrop>())
				{
					if (TSharedPtr<FRCControllerDragDrop> DragDropOp = StaticCastSharedPtr<FRCControllerDragDrop>(DragDropOperation))
					{
						return true;
					}
				}
				else
				{
					if (SupportedMode == EDragDropSupportedModes::ReorderOnly)
					{
						return false; // This widget only supports Reordering operation as a drag-drop action
					}

					if (DragDropOperation->IsOfType<FExposedEntityDragDrop>())
					{
						if (TSharedPtr<FExposedEntityDragDrop> DragDropOp = StaticCastSharedPtr<FExposedEntityDragDrop>(DragDropOperation))
						{
							// Fetch the Exposed Entity
							const FGuid ExposedEntityId = DragDropOp->GetId();

							if (URemoteControlPreset* Preset = ControllerPanelList->GetPreset())
							{
								if (TSharedPtr<const FRemoteControlField> RemoteControlField = Preset->GetExposedEntity<FRemoteControlField>(ExposedEntityId).Pin())
								{
									if (URCController* Controller = Cast<URCController>(ControllerItem->GetVirtualProperty()))
									{
										const bool bAllowNumericInputAsStrings = true;

										const bool bAllowDrop = URCBehaviourBind::CanHaveActionForField(Controller, RemoteControlField.ToSharedRef(), bAllowNumericInputAsStrings);

										ControllerPanelList->bIsAnyControllerItemEligibleForDragDrop |= (bAllowDrop && bIsDragActive);

										return bAllowDrop;
									}
								}
							}
						}

					}
				}
			}

			return false;
		}

		//~ SWidget Interface
		virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override
		{
			if (ControllerItem.IsValid())
			{
				ControllerItem->EnterRenameMode();
			}

			return FSuperRowType::OnMouseButtonDoubleClick(InMyGeometry, InMouseEvent);
		}

private:
		TSharedPtr<FRCControllerModel> ControllerItem;
		TSharedPtr<SRCControllerPanelList> ControllerPanelList;
		bool bIsDragActive = false;
	};
} 

void SRCControllerPanelList::Construct(const FArguments& InArgs, const TSharedRef<SRCControllerPanel> InControllerPanel, const TSharedRef<SRemoteControlPanel> InRemoteControlPanel)
{
	SRCLogicPanelListBase::Construct(SRCLogicPanelListBase::FArguments(), InControllerPanel, InRemoteControlPanel);
	
	ControllerPanelWeakPtr = InControllerPanel;
	
	RCPanelStyle = &FRemoteControlPanelStyle::Get()->GetWidgetStyle<FRCPanelStyle>("RemoteControlPanel.LogicControllersPanel");

	ListView = SNew(SListView<TSharedPtr<FRCControllerModel>>)
		.ListItemsSource(&ControllerItems)
		.OnSelectionChanged(this, &SRCControllerPanelList::OnTreeSelectionChanged)
		.OnGenerateRow(this, &SRCControllerPanelList::OnGenerateWidgetForList)
		.SelectionMode(ESelectionMode::Single) // Current setup supports only single selection (and related display) of a Controller in the list
		.OnContextMenuOpening(this, &SRCLogicPanelListBase::GetContextMenuWidget)
		.HeaderRow(
			SNew(SHeaderRow)
			.Style(&RCPanelStyle->HeaderRowStyle)

			+ SHeaderRow::Column(UE::RCControllerPanelList::Columns::TypeColor)
			.DefaultLabel(LOCTEXT("ControllerNameColumnName", ""))
			.FixedWidth(30)
			.HeaderContentPadding(RCPanelStyle->HeaderRowPadding)

			+ SHeaderRow::Column(UE::RCControllerPanelList::Columns::DragHandle)
			.DefaultLabel(FText::GetEmpty())
			.FixedWidth(30)
			.HeaderContentPadding(RCPanelStyle->HeaderRowPadding)

			+ SHeaderRow::Column(UE::RCControllerPanelList::Columns::Name)
			.DefaultLabel(LOCTEXT("ControllerNameColumnName", "Name"))
			.FillWidth(0.15f)
			.HeaderContentPadding(RCPanelStyle->HeaderRowPadding)

			+ SHeaderRow::Column(UE::RCControllerPanelList::Columns::Value)
			.DefaultLabel(LOCTEXT("ControllerValueColumnName", "Input"))
			.FillWidth(0.75f)
			.HeaderContentPadding(RCPanelStyle->HeaderRowPadding)
		);

	ChildSlot
	[
		SNew(SDropTarget)
		.VerticalImage(FRemoteControlPanelStyle::Get()->GetBrush("RemoteControlPanel.VerticalDash"))
		.HorizontalImage(FRemoteControlPanelStyle::Get()->GetBrush("RemoteControlPanel.HorizontalDash"))
		.OnDropped_Lambda([this](const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent) { return SRCControllerPanelList::OnControllerListViewDragDrop(InDragDropEvent.GetOperation()); })
		.OnAllowDrop(this, &SRCControllerPanelList::OnAllowDrop)
		.OnIsRecognized(this, &SRCControllerPanelList::OnAllowDrop)
		[
			ListView.ToSharedRef()
		]
	];

	// Add delegates
	if (const URemoteControlPreset* Preset = ControllerPanelWeakPtr.Pin()->GetPreset())
	{
		// Refresh list
		const TSharedPtr<SRemoteControlPanel> RemoteControlPanel = ControllerPanelWeakPtr.Pin()->GetRemoteControlPanel();
		check(RemoteControlPanel)
		RemoteControlPanel->OnControllerAdded.AddSP(this, &SRCControllerPanelList::OnControllerAdded);
		RemoteControlPanel->OnEmptyControllers.AddSP(this, &SRCControllerPanelList::OnEmptyControllers);

		Preset->OnVirtualPropertyContainerModified().AddSP(this, &SRCControllerPanelList::OnControllerContainerModified);
	}

	FPropertyRowGeneratorArgs Args;
	Args.bShouldShowHiddenProperties = true;
	Args.NotifyHook = this;
	PropertyRowGenerator = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreatePropertyRowGenerator(Args);
	
	Reset();
}

bool SRCControllerPanelList::IsEmpty() const
{
	return ControllerItems.IsEmpty();
}

int32 SRCControllerPanelList::Num() const
{
	return NumControllerItems();
}

int32 SRCControllerPanelList::NumSelectedLogicItems() const
{
	return ListView->GetNumItemsSelected();
}

void SRCControllerPanelList::Reset()
{
	ControllerItems.Empty();

	check(ControllerPanelWeakPtr.IsValid());

	TSharedPtr<SRCControllerPanel> ControllerPanel = ControllerPanelWeakPtr.Pin();
	URemoteControlPreset* Preset = ControllerPanel->GetPreset();
	TSharedPtr<SRemoteControlPanel> RemoteControlPanel = ControllerPanel->GetRemoteControlPanel();
	
	check(Preset);

	PropertyRowGenerator->SetStructure(Preset->GetControllerContainerStructOnScope());
	PropertyRowGenerator->OnFinishedChangingProperties().AddSP(this, &SRCControllerPanelList::OnFinishedChangingProperties);

	// Generator should be moved to separate class
	TArray<TSharedRef<IDetailTreeNode>> RootTreeNodes = PropertyRowGenerator->GetRootTreeNodes();

	for (const TSharedRef<IDetailTreeNode>& CategoryNode : RootTreeNodes)
	{
		TArray<TSharedRef<IDetailTreeNode>> Children;
		CategoryNode->GetChildren(Children);

		ControllerItems.SetNumZeroed(Children.Num());

		for (TSharedRef<IDetailTreeNode>& Child : Children)
		{
			FProperty* Property = Child->CreatePropertyHandle()->GetProperty();
 			check(Property);

			if (URCVirtualPropertyBase* Controller = Preset->GetController(Property->GetFName()))
			{
				if(ensureAlways(ControllerItems.IsValidIndex(Controller->DisplayIndex)))
					ControllerItems[Controller->DisplayIndex] = MakeShared<FRCControllerModel>(Controller, Child, RemoteControlPanel);
			}
		}
	}

	ListView->RebuildList();
}

TSharedRef<ITableRow> SRCControllerPanelList::OnGenerateWidgetForList(TSharedPtr<FRCControllerModel> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	typedef UE::RCControllerPanelList::SControllerItemListRow SControllerRowType;

	return SNew(SControllerRowType, OwnerTable, InItem.ToSharedRef(), SharedThis(this))
		.Style(&RCPanelStyle->TableRowStyle)
		.Padding(FMargin(4.5f));
}

void SRCControllerPanelList::OnTreeSelectionChanged(TSharedPtr<FRCControllerModel> InItem, ESelectInfo::Type)
{
	if (TSharedPtr<SRCControllerPanel> ControllerPanel = ControllerPanelWeakPtr.Pin())
	{
		if (const TSharedPtr<SRemoteControlPanel> RemoteControlPanel = ControllerPanel->GetRemoteControlPanel())
		{
			if (InItem != SelectedControllerItemWeakPtr.Pin())
			{
				SelectedControllerItemWeakPtr = InItem;
				RemoteControlPanel->OnControllerSelectionChanged.Broadcast(InItem);
				RemoteControlPanel->OnBehaviourSelectionChanged.Broadcast(InItem.IsValid() ? InItem->GetSelectedBehaviourModel() : nullptr);
			}
		}
	}
}

void SRCControllerPanelList::SelectController(URCController* InController)
{
	for (TSharedPtr<FRCControllerModel> ControllerItem : ControllerItems)
	{
		if (!ensure(ControllerItem))
		{
			continue;
		}

		if (ControllerItem->GetVirtualProperty() == InController)
		{
			ListView->SetSelection(ControllerItem);
		}
	}
}

void SRCControllerPanelList::OnControllerAdded(const FName& InNewPropertyName)
{
	Reset();
}

void SRCControllerPanelList::OnNotifyPreChangeProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	if (URemoteControlPreset* Preset = GetPreset())
	{
		Preset->OnNotifyPreChangeVirtualProperty(PropertyChangedEvent);
	}
}

void SRCControllerPanelList::OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	if (URemoteControlPreset* Preset = GetPreset())
	{
		Preset->OnModifyController(PropertyChangedEvent);
	}
}

void SRCControllerPanelList::OnEmptyControllers()
{
	if (TSharedPtr< SRCControllerPanel> ControllerPanel = ControllerPanelWeakPtr.Pin())
	{
		if (TSharedPtr<SRemoteControlPanel> RemoteControlPanel = ControllerPanel->GetRemoteControlPanel())
		{
			RemoteControlPanel->OnControllerSelectionChanged.Broadcast(nullptr);
			RemoteControlPanel->OnBehaviourSelectionChanged.Broadcast(nullptr);
		}

		Reset();
	}
}

void SRCControllerPanelList::OnControllerContainerModified()
{
	Reset();
}

void SRCControllerPanelList::BroadcastOnItemRemoved()
{
	if (const TSharedPtr<SRemoteControlPanel> RemoteControlPanel = ControllerPanelWeakPtr.Pin()->GetRemoteControlPanel())
	{
		RemoteControlPanel->OnControllerSelectionChanged.Broadcast(nullptr);
		RemoteControlPanel->OnBehaviourSelectionChanged.Broadcast(nullptr);
	}
}

URemoteControlPreset* SRCControllerPanelList::GetPreset()
{
	if (ControllerPanelWeakPtr.IsValid())
	{
		return ControllerPanelWeakPtr.Pin()->GetPreset();
	}

	return nullptr;	
}

int32 SRCControllerPanelList::RemoveModel(const TSharedPtr<FRCLogicModeBase> InModel)
{
	if(ControllerPanelWeakPtr.IsValid())
	{
		if (URemoteControlPreset* Preset = ControllerPanelWeakPtr.Pin()->GetPreset())
		{
			if(const TSharedPtr<FRCControllerModel> SelectedController = StaticCastSharedPtr<FRCControllerModel>(InModel))
			{
				if(URCVirtualPropertyBase* ControllerToRemove = SelectedController->GetVirtualProperty())
				{
					const int32 DisplayIndexToRemove = ControllerToRemove->DisplayIndex;

					FScopedTransaction Transaction(LOCTEXT("RemoveController", "Remove Controller"));
					Preset->Modify();

					// Remove Model from Data Container
					const bool bRemoved = Preset->RemoveController(SelectedController->GetPropertyName());
					if (bRemoved)
					{
						// Shift all display indices higher than the deleted index down by 1
						for (int32 ControllerIndex = 0; ControllerIndex < ControllerItems.Num(); ControllerIndex++)
						{
							if (ensure(ControllerItems[ControllerIndex]))
							{
								if (URCVirtualPropertyBase* Controller = ControllerItems[ControllerIndex]->GetVirtualProperty())
								{
									if (Controller->DisplayIndex > DisplayIndexToRemove)
									{
										Controller->Modify();

										Controller->SetDisplayIndex(Controller->DisplayIndex - 1);
									}
								}
							}
						}

						return 1; // Remove Count
					}
				}
			}
		}
	}

	return 0;
}

bool SRCControllerPanelList::IsListFocused() const
{
	return ListView->HasAnyUserFocus().IsSet() || ContextMenuWidgetCached.IsValid();
}

void SRCControllerPanelList::DeleteSelectedPanelItem()
{
	DeleteItemFromLogicPanel<FRCControllerModel>(ControllerItems, ListView->GetSelectedItems());
}

void SRCControllerPanelList::NotifyPreChange(FEditPropertyChain* PropertyAboutToChange)
{
	// If a Vector is modified and the Z value changes, the sub property (corresponding to Z) gets notified to us.
	// However for Controllers we are actually interested in the parent Struct property (corresponding to the Vector) as the Virtual Property is associated with Struct's FProperty (rather than its X/Y/Z components)
	// For this reason the "Active Member Node" is extracted below from the child property
	if (FEditPropertyChain::TDoubleLinkedListNode* ActiveMemberNode = PropertyAboutToChange->GetActiveMemberNode())
	{
		FPropertyChangedEvent PropertyChangedEvent(ActiveMemberNode->GetValue());

		OnNotifyPreChangeProperties(PropertyChangedEvent);
	}
}

void SRCControllerPanelList::EnterRenameMode()
{
	if (TSharedPtr<FRCControllerModel> SelectedItem = SelectedControllerItemWeakPtr.Pin())
	{
		SelectedItem->EnterRenameMode();
	}
}

TSharedPtr<FRCControllerModel> SRCControllerPanelList::FindControllerItemById(const FGuid& InId) const
{
	for (TSharedPtr<FRCControllerModel> ControllerItem : ControllerItems)
	{
		if (ControllerItem && ControllerItem->GetId() == InId)
		{
			return ControllerItem;
		}
	}

	return nullptr;
}

void SRCControllerPanelList::ReorderControllerItem(TSharedRef<FRCControllerModel> ItemToMove, TSharedRef<FRCControllerModel> AnchorItem)
{
	int32 Index = ControllerItems.Find(AnchorItem);

	// Update UI list
	ControllerItems.RemoveSingle(ItemToMove);
	ControllerItems.Insert(ItemToMove, Index);

	// Update display indices
	for (int32 i = Index; i < ControllerItems.Num(); i++)
	{
		if (ensure(ControllerItems[i]))
		{
			if (URCVirtualPropertyBase* Controller = ControllerItems[i]->GetVirtualProperty())
			{
				Controller->SetDisplayIndex(i);
			}
		}
	}

	ListView->RequestListRefresh();
}

static TSharedPtr<FExposedEntityDragDrop> GetExposedEntityDragDrop(TSharedPtr<FDragDropOperation> DragDropOperation)
{
	if (DragDropOperation)
	{
		if (DragDropOperation->IsOfType<FExposedEntityDragDrop>())
		{
			return StaticCastSharedPtr<FExposedEntityDragDrop>(DragDropOperation);
		}
	}

	return nullptr;
}

bool SRCControllerPanelList::OnAllowDrop(TSharedPtr<FDragDropOperation> DragDropOperation)
{
	if (IsListViewHovered())
	{
		bIsAnyControllerItemEligibleForDragDrop = false;
	}
	// Ensures that this drop target is visually disabled whenever the user is attempting to drop onto an existing Controller (rather than the ListView's empty space)
	else if (bIsAnyControllerItemEligibleForDragDrop)
	{
		return false;
	}

	if (TSharedPtr<FExposedEntityDragDrop> DragDropOp = GetExposedEntityDragDrop(DragDropOperation))
	{
		// Fetch the Exposed Entity
		const FGuid ExposedEntityId = DragDropOp->GetId();

		if (URemoteControlPreset* Preset = GetPreset())
		{
			if (TSharedPtr<const FRemoteControlField> RemoteControlField = Preset->GetExposedEntity<FRemoteControlField>(ExposedEntityId).Pin())
			{
				return RemoteControlField->FieldType == EExposedFieldType::Property;
			}
		}
	}

	return false;
}

FReply SRCControllerPanelList::OnControllerListViewDragDrop(TSharedPtr<FDragDropOperation> DragDropOperation)
{
	if (TSharedPtr<FExposedEntityDragDrop> DragDropOp = GetExposedEntityDragDrop(DragDropOperation))
	{
		// Fetch the Exposed Entity
		const FGuid ExposedEntityId = DragDropOp->GetId();

		if (URemoteControlPreset* Preset = GetPreset())
		{
			if (TSharedPtr<const FRemoteControlProperty> RemoteControlProperty = Preset->GetExposedEntity<FRemoteControlProperty>(ExposedEntityId).Pin())
			{
				CreateAutoBindForProperty(RemoteControlProperty);
			}
		}
	}
	
	return FReply::Handled();
}

void SRCControllerPanelList::CreateAutoBindForProperty(TSharedPtr<const FRemoteControlProperty> RemoteControlProperty)
{
	if (URemoteControlPreset* Preset = GetPreset())
	{
		// Derive the input data needed for creating a new Controller
		FProperty* Property = RemoteControlProperty->GetProperty();
		EPropertyBagPropertyType PropertyBagType = EPropertyBagPropertyType::None;
		UObject* StructObject = nullptr;

		// In the Logic realm we use a single type like (eg: String / Int) to represent various related types (String/Name/Text, Int32, Int64, etc)
		// For this reason explicit mapping conversion is required between a given FProperty type and the desired Controller type
		bool bSuccess = URCBehaviourBind::GetPropertyBagTypeFromFieldProperty(Property, PropertyBagType, StructObject);
		if(bSuccess)
		{
			// Step 1. Create a Controller of matching type
			URCController* NewController = Cast<URCController>(Preset->AddController(URCController::StaticClass(), PropertyBagType, StructObject));
			NewController->DisplayIndex = Preset->GetNumControllers() - 1;

			// Transfer property value from Exposed Property to the New Controller.
			// The goal is to keep values synced for a Controller newly created via "Auto Bind"
			URCBehaviourBind::CopyPropertyValueToController(NewController, RemoteControlProperty.ToSharedRef());

			// Step 2. Refresh UI
			Reset();

			// Step 3. Create Bind Behaviour and Bind to the property
			constexpr bool bExecuteBind = true;
			CreateBindBehaviourAndAssignTo(NewController, RemoteControlProperty.ToSharedRef(), bExecuteBind);
		}
	}
}

void SRCControllerPanelList::CreateBindBehaviourAndAssignTo(URCController* Controller, TSharedRef<const FRemoteControlProperty> InRemoteControlProperty, const bool bExecuteBind)
{
	const URCBehaviourBind* BindBehaviour = nullptr;

	bool bRequiresNumericConversion = false;
	if (!URCBehaviourBind::CanHaveActionForField(Controller, InRemoteControlProperty, false))
	{
		if (URCBehaviourBind::CanHaveActionForField(Controller, InRemoteControlProperty, true))
		{
			bRequiresNumericConversion = true;
		}
		else
		{
			ensureAlwaysMsgf(false, TEXT("Incompatible property provided for Auto Bind!"));
			return;
		}
	}

	for (const URCBehaviour* Behaviour : Controller->Behaviours)
	{
		if (Behaviour && Behaviour->IsA(URCBehaviourBind::StaticClass()))
		{
			BindBehaviour = Cast<URCBehaviourBind>(Behaviour);

			// In case numeric conversion is required we might have multiple Bind behaviours with different settings,
			// so we do not break in case there is at least one Bind behaviour with a matching clause. If not, we will create a new Bind behaviour with the requried setting (see below)
			if (!bRequiresNumericConversion || BindBehaviour->AreNumericInputsAllowedAsStrings())
			{
				break;
			}
		}
	}

	if (BindBehaviour)
	{
		if (bRequiresNumericConversion && !BindBehaviour->AreNumericInputsAllowedAsStrings())
		{
			// If the requested Bind operation requires numeric conversion but the existing Bind behaviour doesn't support this, then we prefer creating a new Bind behaviour to facilitate this operation.
			// This allows the the user to successfully perform the Auto Bind as desired without disrupting the setting on the existing Bind behaviour
			BindBehaviour = nullptr;
		}
		
	}

	if (!BindBehaviour)
	{
		URCBehaviourBind* NewBindBehaviour = Cast<URCBehaviourBind>(Controller->AddBehaviour(URCBehaviourBindNode::StaticClass()));

		// If this is a new Bind Behaviour and we are attempting to link unrelated (but compatible types), via numeric conversion, then we apply the bAllowNumericInputAsStrings flag
		NewBindBehaviour->SetAllowNumericInputAsStrings(bRequiresNumericConversion);

		BindBehaviour = NewBindBehaviour;

		// Broadcast new behaviour
		if (const TSharedPtr<SRCControllerPanel> ControllerPanel = ControllerPanelWeakPtr.Pin())
		{
			if (const TSharedPtr<SRemoteControlPanel> RemoteControlPanel = ControllerPanel->GetRemoteControlPanel())
			{
				RemoteControlPanel->OnBehaviourAdded.Broadcast(BindBehaviour);
			}
		}
	}

	if (ensure(BindBehaviour))
	{
		URCBehaviourBind* BindBehaviourMutable = const_cast<URCBehaviourBind*>(BindBehaviour);
		URCAction* BindAction = BindBehaviourMutable->AddPropertyBindAction(InRemoteControlProperty);

		if (bExecuteBind)
		{
			BindAction->Execute();
		}
	}

	// Update the UI selection to provide the user visual feedback indicating that a Bind behaviour has been created/updated
	SelectController(Controller);
}

bool SRCControllerPanelList::IsListViewHovered()
{
	return ListView->IsDirectlyHovered();
}

#undef LOCTEXT_NAMESPACE