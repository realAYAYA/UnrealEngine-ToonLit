// Copyright Epic Games, Inc. All Rights Reserved.

#include "SBaseReplicationStreamEditor.h"

#include "FakeObjectToPropertiesEditorModel.h"
#include "Model/Item/SourceModelBuilders.h"
#include "Replication/Editor/Model/IEditableReplicationStreamModel.h"
#include "Replication/Editor/Model/Property/IPropertySelectionSourceModel.h"
#include "Replication/Editor/Model/ReplicatedObjectData.h"
#include "Replication/Editor/View/DisplayUtils.h"
#include "Replication/Editor/View/ObjectViewer/SReplicationStreamViewer.h"
#include "Replication/Editor/Model/ObjectSource/IObjectSelectionSourceModel.h"

#include "Algo/AnyOf.h"
#include "Replication/Editor/Model/Object/IObjectHierarchyModel.h"
#include "UObject/Class.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "SBaseReplicationStreamEditor"

namespace UE::ConcertSharedSlate
{
	void SBaseReplicationStreamEditor::Construct(
		const FArguments& InArgs,
		TSharedRef<IEditableReplicationStreamModel> InPropertiesModel,
		TSharedRef<IObjectSelectionSourceModel> InObjectSelectionSource,
		TSharedRef<IPropertySelectionSourceModel> InPropertySelectionSource)
	{
		ObjectSelectionSource = MoveTemp(InObjectSelectionSource);
		PropertySelectionSource = MoveTemp(InPropertySelectionSource);
		ObjectHierarchy = InArgs._ObjectHierarchy;
		
		EditablePropertiesModel = MoveTemp(InPropertiesModel);
		EditablePropertiesModel->OnObjectsChanged().AddSP(this, &SBaseReplicationStreamEditor::OnObjectsChanged);
		EditablePropertiesModel->OnPropertiesChanged().AddSP(this, &SBaseReplicationStreamEditor::OnPropertiesChanged);
		
		PropertiesModelAdapter = MakeShared<FFakeObjectToPropertiesEditorModel>(EditablePropertiesModel.ToSharedRef(), PropertySelectionSource.ToSharedRef());

		IsEditingEnabledAttribute = InArgs._IsEditingEnabled;
		EditingDisabledToolTipTextAttribute = InArgs._EditingDisabledToolTipText;
		OnExtendObjectsContextMenuDelegate = InArgs._OnExtendObjectsContextMenu;
		
		ChildSlot
		[
			SAssignNew(ReplicationViewer, SReplicationStreamViewer, PropertiesModelAdapter.ToSharedRef())
				.PropertyTreeView(InArgs._PropertyTreeView)
				.ObjectColumns(InArgs._ObjectColumns)
				.PrimaryObjectSort(InArgs._PrimaryObjectSort)
				.SecondaryObjectSort(InArgs._SecondaryObjectSort)
				.ObjectHierarchy(InArgs._ObjectHierarchy)
				.NameModel(InArgs._NameModel)
				.OnDeleteObjects(this, &SBaseReplicationStreamEditor::OnDeleteObjects)
				.OnObjectsContextMenuOpening(this, &SBaseReplicationStreamEditor::OnObjectsContextMenuOpening)
				.LeftOfObjectSearchBar()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						BuildRootAddObjectWidgets()
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						InArgs._LeftOfObjectSearchBar.Widget
					]
				]
				.NoOutlinerObjects(LOCTEXT("NoObjects", "Add objects to replicate"))
		];
	}

	SBaseReplicationStreamEditor::~SBaseReplicationStreamEditor()
	{
		EditablePropertiesModel->OnObjectsChanged().RemoveAll(this);
		EditablePropertiesModel->OnPropertiesChanged().RemoveAll(this);
	}

	void SBaseReplicationStreamEditor::Refresh()
	{
		ReplicationViewer->Refresh();
	}

	void SBaseReplicationStreamEditor::RequestObjectColumnResort(const FName& ColumnId)
	{
		ReplicationViewer->RequestObjectColumnResort(ColumnId);
	}

	void SBaseReplicationStreamEditor::RequestPropertyColumnResort(const FName& ColumnId)
	{
		ReplicationViewer->RequestPropertyColumnResort(ColumnId);
	}

	TArray<FSoftObjectPath> SBaseReplicationStreamEditor::GetObjectsBeingPropertyEdited() const
	{
		return ReplicationViewer->GetObjectsBeingPropertyEdited();
	}

	bool SBaseReplicationStreamEditor::IsEditingDisabled() const
	{
		return (IsEditingEnabledAttribute.IsBound() || IsEditingEnabledAttribute.IsSet())
			&& !IsEditingEnabledAttribute.Get();
	}

	FText SBaseReplicationStreamEditor::GetEditingDisabledText() const
	{
		return (EditingDisabledToolTipTextAttribute.IsBound() || EditingDisabledToolTipTextAttribute.IsSet())
			? EditingDisabledToolTipTextAttribute.Get()
			: FText::GetEmpty();
	}

	void SBaseReplicationStreamEditor::OnObjectsChanged(TConstArrayView<UObject*> AddedObjects, TConstArrayView<FSoftObjectPath> RemovedObjects, EReplicatedObjectChangeReason ChangeReason)
	{
		ReplicationViewer->RequestObjectDataRefresh();
			
		// Newly added objects should be automatically selected
		if (!AddedObjects.IsEmpty())
		{
			TArray<FSoftObjectPath> TopLevelObjects;
			
			// Goal: select an object so the property view immediately shows properties for some object.
			// Problem: if objects have different classes, property view will be empty (incompatible class)
			// Solution: Select the highest object in the hierarchy, which is usually an actor. Users usually select an actor to add in the "Add" combo button so this also makes intuitive sense.
			// Caveat: There may be multiple hierarchies (e.g. if multiple actors were added). This is a very seldom case though: too bad.
			Algo::TransformIf(AddedObjects, TopLevelObjects,
				[this, &AddedObjects](const UObject* Object)
				{
					const TOptional<IObjectHierarchyModel::FParentInfo> ParentInfo = ObjectHierarchy->GetParentInfo(Object);
					const bool bIsTopOfHierarchy = !ParentInfo || !AddedObjects.ContainsByPredicate([&ParentInfo](UObject* AddedObject){ return FSoftObjectPath(AddedObject) == ParentInfo->Parent; });
					return bIsTopOfHierarchy;
				},
				[](const UObject* Object){ return Object; });
			ReplicationViewer->SelectObjects(TopLevelObjects);

			// Expand the hierarchy for all added objects for easier editing
			constexpr bool bRecursive = true;
			ReplicationViewer->ExpandObjects(TopLevelObjects, bRecursive);
		}
	}

	void SBaseReplicationStreamEditor::OnPropertiesChanged()
	{
		ReplicationViewer->RequestPropertyDataRefresh();
	}

	TSharedRef<SWidget> SBaseReplicationStreamEditor::BuildRootAddObjectWidgets()
	{
		using namespace ConcertSharedSlate;
		
		const TSharedRef<SHorizontalBox> Root = SNew(SHorizontalBox);
		const FSourceModelBuilders<FSelectableObjectInfo>::FItemPickerArgs Args = MakeObjectSourceBuilderArgs();
		for (const TSourceSelectionCategory<FSelectableObjectInfo>& Category : ObjectSelectionSource->GetRootSources())
		{
			Root->AddSlot()
				.AutoWidth()
				[
					FSourceModelBuilders<FSelectableObjectInfo>::BuildCategory(Category, Args)	
				];
		}
		return Root;
	}

	void SBaseReplicationStreamEditor::OnObjectsSelectedForAdding(TArray<FSelectableObjectInfo> ObjectsToAdd)
	{
		TArray<UObject*> Objects;
		Algo::TransformIf(
			ObjectsToAdd,
			Objects,
			[](const FSelectableObjectInfo& SelectableObject) { return SelectableObject.Object.IsValid(); },
			[](const FSelectableObjectInfo& SelectableObject){ return SelectableObject.Object.Get(); }
			);

		TGuardValue<bool> GuardObjectSelection(bIsAddingFromSelection, true);
		EditablePropertiesModel->AddObjects(Objects);
	}

	void SBaseReplicationStreamEditor::OnDeleteObjects(const TArray<TSharedPtr<FReplicatedObjectData>>& ObjectsToDelete) const
	{
		if (IsEditingDisabled())
		{
			return;
		}
		
		TArray<FSoftObjectPath> DeleteObjectPaths;
		Algo::Transform(ObjectsToDelete, DeleteObjectPaths, [](const TSharedPtr<FReplicatedObjectData>& Data) { return Data->GetObjectPath(); });

		// We want to delete children not listed in the outliner, such as components and other subobjects
		TArray<FSoftObjectPath> ObjectAndChildren;
		EditablePropertiesModel->ForEachReplicatedObject([&ObjectsToDelete, &ObjectAndChildren](const FSoftObjectPath& ReplicatedObject)
		{
			const bool bIsChildOfDeletedObject = Algo::AnyOf(ObjectsToDelete, [&ReplicatedObject](const TSharedPtr<FReplicatedObjectData>& ObjectData)
			{
				// ReplicatedObject is a child of a deleted object if the path before it contains one of the deleted objects
				return ReplicatedObject.ToString().Contains(ObjectData->GetObjectPath().ToString());
			});
			if (bIsChildOfDeletedObject)
			{
				ObjectAndChildren.Add(ReplicatedObject);
			}
			return EBreakBehavior::Continue;
		});
		
		EditablePropertiesModel->RemoveObjects(ObjectAndChildren);
	}

	TSharedPtr<SWidget> SBaseReplicationStreamEditor::OnObjectsContextMenuOpening()
	{
		FMenuBuilder MenuBuilder(true, nullptr);
		
		AddObjectSourceContextMenuOptions(MenuBuilder);
		MenuBuilder.AddMenuEntry(
			LOCTEXT("DeleteItems", "Delete"),
			TAttribute<FText>::CreateLambda([this](){ return GetEditingDisabledText(); }),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SBaseReplicationStreamEditor::OnDeleteObjects_PassByValue, ReplicationViewer->GetSelectedOutlinerObjects()),
				FCanExecuteAction::CreateLambda([this]() { return !IsEditingDisabled(); })
				),
			NAME_None,
			EUserInterfaceActionType::Button
		);

		TArray<FSoftObjectPath> SelectedObjects;
		Algo::Transform(ReplicationViewer->GetSelectedOutlinerObjects(), SelectedObjects, [](const TSharedPtr<FReplicatedObjectData>& Data){ return Data->GetObjectPath(); });
		OnExtendObjectsContextMenuDelegate.ExecuteIfBound(MenuBuilder, SelectedObjects);
		
		return MenuBuilder.MakeWidget();
	}

	void SBaseReplicationStreamEditor::AddObjectSourceContextMenuOptions(FMenuBuilder& MenuBuilder)
	{
		using namespace ConcertSharedSlate;
		
		// Context menu generation is only supported for single items
		const TArray<TSharedPtr<FReplicatedObjectData>> SelectedObjects = ReplicationViewer->GetSelectedOutlinerObjects();
		if (SelectedObjects.Num() != 1)
		{
			return;
		}
		
		const TArray<TSharedRef<IObjectSourceModel>> ContextMenuOptions = ObjectSelectionSource->GetContextMenuOptions(SelectedObjects[0]->GetObjectPath());
		if (ContextMenuOptions.IsEmpty())
		{
			return;
		}
		
		const FSourceModelBuilders<FSelectableObjectInfo>::FItemPickerArgs Args = MakeObjectSourceBuilderArgs();
		for (const TSharedRef<IObjectSourceModel>& SourceModel : ContextMenuOptions)
		{
			bool bCanAddAnyObject = false; 
			SourceModel->EnumerateSelectableItems([this, &bCanAddAnyObject](const FSelectableObjectInfo& SelectableOption) mutable
			{
				bCanAddAnyObject |= !EditablePropertiesModel->ContainsObjects({ SelectableOption.Object.Get() });
				return bCanAddAnyObject ? EBreakBehavior::Break : EBreakBehavior::Continue;
			});

			// Skip showing context menu options which will not add anything new.
			if (bCanAddAnyObject)
			{
				FSourceModelBuilders<FSelectableObjectInfo>::AddOptionToMenu(SourceModel, Args, MenuBuilder);
			}
		}
		
		MenuBuilder.AddSeparator();
	}

	ConcertSharedSlate::FSourceModelBuilders<FSelectableObjectInfo>::FItemPickerArgs SBaseReplicationStreamEditor::MakeObjectSourceBuilderArgs()
	{
		using namespace ConcertSharedSlate;
		using FBuilderDelegates = FSourceModelBuilders<FSelectableObjectInfo>;
		
		return FSourceModelBuilders<FSelectableObjectInfo>::FItemPickerArgs
		{
			FBuilderDelegates::FOnItemsSelected::CreateSP(this, &SBaseReplicationStreamEditor::OnObjectsSelectedForAdding),
			FBuilderDelegates::FGetItemDisplayString::CreateLambda([](const FSelectableObjectInfo& Item)
			{
				return Item.Object.IsValid() ? DisplayUtils::GetObjectDisplayString(*Item.Object.Get()) : TEXT("");
			}),
			FBuilderDelegates::FGetItemIcon::CreateLambda([](const FSelectableObjectInfo& Item)
			{
				return Item.Object.IsValid() ? DisplayUtils::GetObjectIcon(*Item.Object.Get()) : FSlateIcon();
			}),
			FBuilderDelegates::FIsItemSelected::CreateLambda([this](const FSelectableObjectInfo& Item)
			{
				return EditablePropertiesModel->ContainsObjects({ Item.Object.Get() } );
			}),
			TAttribute<bool>::CreateLambda([this]() { return !IsEditingDisabled(); }),
			TAttribute<FText>::CreateLambda([this]() { return GetEditingDisabledText(); }),
			EItemPickerFlags::DisplayOptionListInline
		}; 
	}
}

#undef LOCTEXT_NAMESPACE