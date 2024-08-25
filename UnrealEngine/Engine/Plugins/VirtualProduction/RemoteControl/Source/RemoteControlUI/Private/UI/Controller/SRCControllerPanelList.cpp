// Copyright Epic Games, Inc. All Rights Reserved.

#include "SRCControllerPanelList.h"

#include "Behaviour/Builtin/Bind/RCBehaviourBind.h"
#include "Behaviour/Builtin/Bind/RCBehaviourBindNode.h"
#include "Commands/RemoteControlCommands.h"
#include "Controller/RCController.h"
#include "Controller/RCControllerContainer.h"
#include "Controller/RCCustomControllerUtilities.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "IDetailTreeNode.h"
#include "IPropertyRowGenerator.h"
#include "IRemoteControlModule.h"
#include "Interfaces/IMainFrameModule.h"
#include "Materials/MaterialInterface.h"
#include "RCControllerModel.h"
#include "RCMultiController.h"
#include "RCVirtualProperty.h"
#include "RCVirtualPropertyContainer.h"
#include "RemoteControlField.h"
#include "RemoteControlPreset.h"
#include "SDropTarget.h"
#include "SRCControllerPanel.h"
#include "SlateOptMacros.h"
#include "Styling/RemoteControlStyles.h"
#include "UI/Action/SRCActionPanelList.h"
#include "UI/BaseLogicUI/RCLogicModeBase.h"
#include "UI/RCUIHelpers.h"
#include "UI/RemoteControlPanelStyle.h"
#include "UI/SRCPanelDragHandle.h"
#include "UI/SRCPanelExposedEntity.h"
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
		const FName ControllerId = TEXT("Controller Id");
		const FName Description = TEXT("Controller Description");
		const FName Value = TEXT("Controller Value");
		const FName DragHandle = TEXT("Drag Handle");
		const FName FieldId = TEXT("Controller Field Id");
		const FName ValueTypeSelection = TEXT("Value Type Selection");
	}

	const TSet<UScriptStruct*>& GetSupportedStructs()
	{
		static const TSet<UScriptStruct*> SupportedStructs = {
			TBaseStructure<FVector>::Get(),
			TBaseStructure<FVector2D>::Get(),
			TBaseStructure<FRotator>::Get(),
			TBaseStructure<FColor>::Get()
		};
		
		return SupportedStructs;
	}

	const TSet<UClass*>& GetSupportedObjects()
	{
		static const TSet<UClass*> SupportedObjects = {
			UTexture::StaticClass(),
			UStaticMesh::StaticClass(),
			UMaterialInterface::StaticClass(),
		};
		
		return SupportedObjects;
	}
	
	bool IsStructPropertyTypeSupported(const FStructProperty* InStructProperty)
	{
		if (InStructProperty)
		{
			return GetSupportedStructs().Contains(InStructProperty->Struct);
		}

		return false;
	}

	bool IsObjectPropertyTypeSupported(const FObjectProperty* InObjectProperty)
	{
		if (InObjectProperty)
		{
			return GetSupportedObjects().Contains(InObjectProperty->PropertyClass);
		}

		return false;
	}

	class SControllerItemListRow : public SMultiColumnTableRow<TSharedRef<FRCControllerModel>>
	{
	public:
		void Construct(const FTableRowArgs& InArgs, const TSharedRef<STableViewBase>& OwnerTableView, TSharedRef<FRCControllerModel> InControllerItem, TSharedRef<SRCControllerPanelList> InControllerPanelList)
		{
			ControllerItem = InControllerItem;
			ControllerPanelListWeakPtr = InControllerPanelList.ToWeakPtr();
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
					if (const FProperty* Property = Controller->GetProperty())
					{
						return UE::RCUIHelpers::GetTypeColorWidget(Property);
					}
				}
			}
			else if (ColumnName == UE::RCControllerPanelList::Columns::FieldId)
			{
				return WrapWithDropTarget(ControllerItem->GetFieldIdWidget());
			}
			else if (ColumnName == UE::RCControllerPanelList::Columns::ValueTypeSelection)
			{
				return WrapWithDropTarget(ControllerItem->GetTypeSelectionWidget());
			}
			else if (ColumnName == UE::RCControllerPanelList::Columns::ControllerId)
			{
				return WrapWithDropTarget(ControllerItem->GetNameWidget());
			}
			else if (ColumnName == UE::RCControllerPanelList::Columns::Description)
			{
				return WrapWithDropTarget(ControllerItem->GetDescriptionWidget());
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
			else if (const TSharedPtr<SRCControllerPanelList> ControlPanelList = ControllerPanelListWeakPtr.Pin())
			{
				if (ControlPanelList->GetCustomColumns().Contains(ColumnName))
				{
					return ControllerItem->GetControllerExtensionWidget(ColumnName);
				}
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
			check(ControllerPanelListWeakPtr.IsValid());
			const TSharedPtr<SRCControllerPanelList> ControllerPanelList = ControllerPanelListWeakPtr.Pin();
			
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
					const TArray<FGuid>& ExposedEntitiesIds = DragDropOp->GetSelectedIds();

					if (ExposedEntitiesIds.Num() == 1)
					{
						if (URemoteControlPreset* Preset = ControllerPanelList->GetPreset())
						{
							if (TSharedPtr<const FRemoteControlProperty> RemoteControlProperty = Preset->GetExposedEntity<FRemoteControlProperty>(ExposedEntitiesIds[0]).Pin())
							{
								if (URCController* Controller = Cast<URCController>(ControllerItem->GetVirtualProperty()))
								{
									ControllerPanelList->CreateBindBehaviourAndAssignTo(Controller, RemoteControlProperty.ToSharedRef(), true);
								}
							}
						}
					}
				}
			}

			return FReply::Handled();
		}

		bool OnAllowDrop(TSharedPtr<FDragDropOperation> DragDropOperation, const EDragDropSupportedModes SupportedMode)
		{
			if (!ensure(ControllerPanelListWeakPtr.IsValid()))
			{
				return false;
			}
			
			const TSharedPtr<SRCControllerPanelList> ControllerPanelList = ControllerPanelListWeakPtr.Pin();

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
							const TArray<FGuid>& ExposedEntitiesIds = DragDropOp->GetSelectedIds();

							if (ExposedEntitiesIds.Num() == 1)
							{
								if (URemoteControlPreset* Preset = ControllerPanelList->GetPreset())
								{
									if (TSharedPtr<const FRemoteControlField> RemoteControlField = Preset->GetExposedEntity<FRemoteControlField>(ExposedEntitiesIds[0]).Pin())
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
			}

			return false;
		}

		//~ SWidget Interface
		virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override
		{
			if (ControllerItem.IsValid())
			{
				ControllerItem->EnterDescriptionEditingMode();
			}

			return FSuperRowType::OnMouseButtonDoubleClick(InMyGeometry, InMouseEvent);
		}

private:
		TSharedPtr<FRCControllerModel> ControllerItem;
		TWeakPtr<SRCControllerPanelList> ControllerPanelListWeakPtr;
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
			SAssignNew(ControllersHeaderRow, SHeaderRow)
			.Style(&RCPanelStyle->HeaderRowStyle)

			+ SHeaderRow::Column(UE::RCControllerPanelList::Columns::TypeColor)
			.DefaultLabel(LOCTEXT("ControllerColorColumnName", ""))
			.FixedWidth(15)
			.HeaderContentPadding(RCPanelStyle->HeaderRowPadding)

			+ SHeaderRow::Column(UE::RCControllerPanelList::Columns::DragHandle)
			.DefaultLabel(FText::GetEmpty())
			.FixedWidth(15)
			.HeaderContentPadding(RCPanelStyle->HeaderRowPadding)

			+ SHeaderRow::Column(UE::RCControllerPanelList::Columns::ControllerId)
			.DefaultLabel(LOCTEXT("ControllerIdColumnName", "Controller Id"))
			.FillWidth(0.2f)
			.HeaderContentPadding(RCPanelStyle->HeaderRowPadding)

			+ SHeaderRow::Column(UE::RCControllerPanelList::Columns::Description)
			.DefaultLabel(LOCTEXT("ControllerNameColumnDescription", "Description"))
			.FillWidth(0.35f)

			+ SHeaderRow::Column(UE::RCControllerPanelList::Columns::Value)
			.DefaultLabel(LOCTEXT("ControllerValueColumnName", "Input"))
			.FillWidth(0.45f)
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
	for (const TSharedPtr<FRCControllerModel>& ControllerModel : ControllerItems)
	{
		if (ControllerModel)
		{
			ControllerModel->OnValueTypeChanged.RemoveAll(this);
		}
	}
	
	ControllerItems.Empty();

	check(ControllerPanelWeakPtr.IsValid());

	TSharedPtr<SRCControllerPanel> ControllerPanel = ControllerPanelWeakPtr.Pin();
	URemoteControlPreset* Preset = ControllerPanel->GetPreset();
	TSharedPtr<SRemoteControlPanel> RemoteControlPanel = ControllerPanel->GetRemoteControlPanel();
	
	check(Preset);

	PropertyRowGenerator->SetStructure(Preset->GetControllerContainerStructOnScope());
	if (!PropertyRowGenerator->OnFinishedChangingProperties().IsBoundToObject(this))
	{
		PropertyRowGenerator->OnFinishedChangingProperties().AddSP(this, &SRCControllerPanelList::OnFinishedChangingProperties);
	}

	// Generator should be moved to separate class
	TArray<TSharedRef<IDetailTreeNode>> RootTreeNodes = PropertyRowGenerator->GetRootTreeNodes();

	MultiControllers.ResetMultiControllers();

	bool bShowFieldIdsColumn = false;
	
	for (const TSharedRef<IDetailTreeNode>& CategoryNode : RootTreeNodes)
	{
		TArray<TSharedRef<IDetailTreeNode>> Children;
		CategoryNode->GetChildren(Children);

		ControllerItems.SetNumZeroed(Children.Num());

		for (TSharedRef<IDetailTreeNode>& Child : Children)
		{
			FProperty* Property = Child->CreatePropertyHandle()->GetProperty();
			check(Property);

			if (Property->IsA<FStrProperty>() || Property->IsA<FTextProperty>())
			{
				Property->AppendMetaData({{TEXT("multiline"), TEXT("true")}});
			}

			if (URCVirtualPropertyBase* Controller = Preset->GetController(Property->GetFName()))
			{
				bool bIsVisible = true;
				bool bIsMultiController = false;

				const FName& FieldId = Controller->FieldId;

				if (FieldId != NAME_None)
				{
					// there's at least one Field Id set, let's show their column
					bShowFieldIdsColumn = true;
				}
				
				// MultiController Mode: only showing one Controller per Field Id
				if (bIsInMultiControllerMode && Preset->GetControllersByFieldId(FieldId).Num() > 1)
				{					
					bIsMultiController = MultiControllers.TryToAddAsMultiController(Controller);
					bIsVisible = bIsMultiController;
				}

				if (bIsVisible)
				{
					if (ensureAlways(ControllerItems.IsValidIndex(Controller->DisplayIndex)))
					{
						const TSharedRef<FRCControllerModel> ControllerModel = MakeShared<FRCControllerModel>(Controller, Child, RemoteControlPanel);
						ControllerItems[Controller->DisplayIndex] = ControllerModel;

						if (bIsMultiController)
						{
							ControllerModel->SetMultiController(bIsMultiController);
							ControllerModel->OnValueTypeChanged.AddSP(this, &SRCControllerPanelList::OnControllerValueTypeChanged);
							ControllerModel->OnValueChanged.AddSP(this, &SRCControllerPanelList::OnControllerValueChanged);
						}
					}
				}
			}
		}
	}

	// sort by Field Id
	if (bIsInMultiControllerMode)
	{
		Algo::Sort(ControllerItems, [](const TSharedPtr<FRCControllerModel>& A, const TSharedPtr<FRCControllerModel>& B)
		{
			if (A.IsValid() && B.IsValid())
			{
				const URCVirtualPropertyBase* ControllerA = A->GetVirtualProperty();
				const URCVirtualPropertyBase* ControllerB = B->GetVirtualProperty();

				if (ControllerA && ControllerB)
				{
					return ControllerA->FieldId.FastLess(ControllerB->FieldId);
				}
			}

			return false;
		});
	}

	ShowFieldIdHeaderColumn(bShowFieldIdsColumn);
	ShowValueTypeHeaderColumn(bIsInMultiControllerMode);

	// Handle custom additional columns
	CustomColumns.Empty();
	IRemoteControlUIModule::Get().OnAddControllerExtensionColumn().Broadcast(CustomColumns);
	for (const FName& ColumnName : CustomColumns)
	{
		if (ControllersHeaderRow.IsValid())
		{
			const bool bColumnIsGenerated = ControllersHeaderRow->IsColumnGenerated(ColumnName);
			if (!bColumnIsGenerated)
			{
				ControllersHeaderRow->AddColumn(
					SHeaderRow::FColumn::FArguments()
					.ColumnId(ColumnName)
					.DefaultLabel(FText::FromName(ColumnName))
					.FillWidth(0.2f)
					.HeaderContentPadding(RCPanelStyle->HeaderRowPadding)
				);
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

void SRCControllerPanelList::OnControllerValueTypeChanged(URCVirtualPropertyBase* InController, EPropertyBagPropertyType InValueType)
{	
	if (InController)
	{		
		MultiControllers.UpdateFieldIdValueType(InController->FieldId, InValueType);
		Reset();

		// todo: do we also want to refresh controllers values after type change?
	}
}

void SRCControllerPanelList::OnControllerValueChanged(URCVirtualPropertyBase* InController)
{	
	const FName& FieldId = InController->FieldId;

	FRCMultiController MultiController = MultiControllers.GetMultiController(FieldId);

	if (MultiController.IsValid())
	{
		MultiController.UpdateHandledControllersValue();
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

void SRCControllerPanelList::DeleteSelectedPanelItems()
{
	DeleteItemsFromLogicPanel<FRCControllerModel>(ControllerItems, ListView->GetSelectedItems());
}

TArray<TSharedPtr<FRCLogicModeBase>> SRCControllerPanelList::GetSelectedLogicItems()
{
	// Controllers don't support multi selection
	if (const TSharedPtr<FRCLogicModeBase> SelectedControllerItemPtr = SelectedControllerItemWeakPtr.Pin())
	{
		return { SelectedControllerItemPtr };
	}
	return TArray<TSharedPtr<FRCLogicModeBase>>();
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
		SelectedItem->EnterDescriptionEditingMode();
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
	for (int32 i = 0; i < ControllerItems.Num(); i++)
	{
		if (ControllerItems[i])
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

bool SRCControllerPanelList::IsEntitySupported(const FGuid ExposedEntityId)
{
	if (URemoteControlPreset* Preset = GetPreset())
	{
		if (const TSharedPtr<const FRemoteControlProperty>& RemoteControlProperty = Preset->GetExposedEntity<FRemoteControlProperty>(ExposedEntityId).Pin())
		{
			if (!RemoteControlProperty->IsEditable())
			{
				// Property with error(s)
				return false;
			}

			if (RemoteControlProperty->FieldType == EExposedFieldType::Property)
			{
				const FProperty* Property = RemoteControlProperty->GetProperty();
				
				if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
				{
					if (const UEnum* Enum = EnumProperty->GetEnum())
					{
						const int64 MaxEnumValue = Enum->GetMaxEnumValue();
						const uint32 NeededBits = FMath::RoundUpToPowerOfTwo(MaxEnumValue);

						// 8 bits enums only
						return NeededBits <= 256;
					}
				}
				else if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
				{
					return UE::RCControllerPanelList::IsStructPropertyTypeSupported(StructProperty);
				}
				else if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
				{
					return UE::RCControllerPanelList::IsObjectPropertyTypeSupported(ObjectProperty);
				}

				return true;
			}
		}
	}

	return false;
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
		const TArray<FGuid>& ExposedEntitiesIds = DragDropOp->GetSelectedIds();

		// Check if Entity is supported by controllers and currently only 1 dragged entity dragged is supported
		return ExposedEntitiesIds.Num() == 1 && IsEntitySupported(ExposedEntitiesIds[0]);
	}

	return false;
}

FReply SRCControllerPanelList::OnControllerListViewDragDrop(TSharedPtr<FDragDropOperation> DragDropOperation)
{
	if (TSharedPtr<FExposedEntityDragDrop> DragDropOp = GetExposedEntityDragDrop(DragDropOperation))
	{
		// Fetch the Exposed Entity
		const FGuid ExposedEntityId = DragDropOp->GetNodeId();

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
			// Preparation step, in case we are dealing with a custom controller
			FString CustomControllerName = TEXT("");
			if (StructObject == UTexture::StaticClass() || StructObject == UTexture2D::StaticClass())
			{
				if (PropertyBagType == EPropertyBagPropertyType::String)
				{
					StructObject = nullptr;
					CustomControllerName = UE::RCCustomControllers::CustomTextureControllerName;
				}
			}
			
			// Step 1. Create a Controller of matching type
			URCController* NewController = Cast<URCController>(Preset->AddController(URCController::StaticClass(), PropertyBagType, StructObject, RemoteControlProperty->FieldName));
			NewController->DisplayIndex = Preset->GetNumControllers() - 1;

			// Add metadata to this controller, if this is a custom controller
			if (!CustomControllerName.IsEmpty())
			{
				const TMap<FName, FString>& CustomControllerMetaData = UE::RCCustomControllers::GetCustomControllerMetaData(CustomControllerName);
				for (const TPair<FName, FString>& Pair : CustomControllerMetaData)
				{
					NewController->SetMetadataValue(Pair.Key, Pair.Value);
				}
			}

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

void SRCControllerPanelList::ShowValueTypeHeaderColumn(bool bInShowColumn)
{
	if (ControllersHeaderRow.IsValid())
	{
		const bool bColumnIsGenerated = ControllersHeaderRow->IsColumnGenerated(UE::RCControllerPanelList::Columns::ValueTypeSelection);
		if (bInShowColumn)
		{
			if (!bColumnIsGenerated)
			{
				ControllersHeaderRow->AddColumn(
					SHeaderRow::FColumn::FArguments()
					.ColumnId(UE::RCControllerPanelList::Columns::ValueTypeSelection)
					.DefaultLabel(LOCTEXT("ControllerValueTypeColumnName", "Value Type"))
					.FillWidth(0.2f)
					.HeaderContentPadding(RCPanelStyle->HeaderRowPadding)
				);
			}
		}
		else if (bColumnIsGenerated)
		{
			ControllersHeaderRow->RemoveColumn(UE::RCControllerPanelList::Columns::ValueTypeSelection);
		}
	}
}

void SRCControllerPanelList::ShowFieldIdHeaderColumn(bool bInShowColumn)
{	
	if (ControllersHeaderRow.IsValid())
	{
		const bool bColumnIsGenerated = ControllersHeaderRow->IsColumnGenerated(UE::RCControllerPanelList::Columns::FieldId);
		if (bInShowColumn)
		{
			if (!bColumnIsGenerated)
			{
				ControllersHeaderRow->InsertColumn(
					SHeaderRow::FColumn::FArguments()
					.ColumnId(UE::RCControllerPanelList::Columns::FieldId)
					.DefaultLabel(LOCTEXT("ControllerNameColumnFieldId", "Field Id"))
					.FillWidth(0.2f)
					.HeaderContentPadding(RCPanelStyle->HeaderRowPadding),
					2);
			}
		}
		else if (bColumnIsGenerated)
		{
			ControllersHeaderRow->RemoveColumn(UE::RCControllerPanelList::Columns::FieldId);
		}
	}
}

void SRCControllerPanelList::AddColumn(const FName& InColumnName)
{
	CustomColumns.AddUnique(InColumnName);
}

void SRCControllerPanelList::SetMultiControllerMode(bool bIsUniqueModeOn)
{
	bIsInMultiControllerMode = bIsUniqueModeOn;
	Reset();
}

#undef LOCTEXT_NAMESPACE
