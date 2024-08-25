// Copyright Epic Games, Inc. All Rights Reserved.

#include "SReplicationStreamViewer.h"

#include "ConcertFrontendUtils.h"
#include "Replication/Editor/Model/IReplicationStreamModel.h"
#include "Replication/Editor/Model/Object/IObjectHierarchyModel.h"
#include "Replication/Editor/Model/ReplicatedObjectData.h"
#include "Replication/Editor/View/Column/ObjectColumnAdapter.h"
#include "Replication/Editor/View/Column/SelectionViewerColumns.h"
#include "Replication/Editor/View/ObjectViewer/Property/SPropertyTreeView.h"
#include "Replication/ObjectUtils.h"
#include "SReplicatedPropertyView.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SObjectToPropertyView"

namespace UE::ConcertSharedSlate
{
	void SReplicationStreamViewer::Construct(const FArguments& InArgs, TSharedRef<IReplicationStreamModel> InPropertiesModel)
	{
		PropertiesModel = MoveTemp(InPropertiesModel);
		ObjectHierarchy = InArgs._ObjectHierarchy;
		NameModel = InArgs._NameModel;
		
		ChildSlot
		[
			CreateContentWidget(InArgs)
		];

		Refresh();
		PropertyArea->SetExpanded(true);
	}

	void SReplicationStreamViewer::Refresh()
	{
		RequestObjectDataRefresh();
		RequestPropertyDataRefresh();
	}

	void SReplicationStreamViewer::RequestObjectColumnResort(const FName& ColumnId)
	{
		ReplicatedObjects->RequestResortForColumn(ColumnId);
	}

	void SReplicationStreamViewer::RequestPropertyColumnResort(const FName& ColumnId)
	{
		PropertySection->RequestResortForColumn(ColumnId);
	}

	TArray<FSoftObjectPath> SReplicationStreamViewer::GetObjectsBeingPropertyEdited() const
	{
		return PropertySection->GetObjectsSelectedForPropertyEditing();
	}

	void SReplicationStreamViewer::SelectObjects(TConstArrayView<FSoftObjectPath> Objects, bool bAtEndOfTick)
	{
		if (bHasRequestedObjectRefresh || bAtEndOfTick)
		{
			PendingToSelect = Objects;
			return;
		}
		
		TArray<TSharedPtr<FReplicatedObjectData>> NewSelectedItems; 
		Algo::TransformIf(AllObjectRowData, NewSelectedItems, [&Objects](const TSharedPtr<FReplicatedObjectData>& ObjectData)
			{
				return Objects.Contains(ObjectData->GetObjectPath());
			},
			[](const TSharedPtr<FReplicatedObjectData>& ObjectData){ return ObjectData; }
		);
		if (!NewSelectedItems.IsEmpty())
		{
			ReplicatedObjects->SetSelectedItems(NewSelectedItems, true);
		}
	}

	void SReplicationStreamViewer::ExpandObjects(TConstArrayView<FSoftObjectPath> Objects, bool bRecursive, bool bAtEndOfTick)
	{
		if (Objects.IsEmpty())
		{
			return;
		}

		if (bHasRequestedObjectRefresh || bAtEndOfTick)
		{
			PendingToExpand = Objects;
			bPendingExpandRecursively = bRecursive;
			return;
		}
		
		TArray<TSharedPtr<FReplicatedObjectData>> ItemsToExpand;
		ItemsToExpand.Reserve(Objects.Num());
		for (const FSoftObjectPath& Path : Objects)
		{
			if (const TSharedPtr<FReplicatedObjectData>* Item = PathToObjectDataCache.Find(Path))
			{
				ItemsToExpand.Add(*Item);
			}
			
			if (bRecursive && ObjectHierarchy)
			{
				ObjectHierarchy->ForEachChildRecursive(Path, [this, &ItemsToExpand](const FSoftObjectPath&, const FSoftObjectPath& ChildObject, EChildRelationship)
				{
					if (const TSharedPtr<FReplicatedObjectData>* Item = PathToObjectDataCache.Find(ChildObject))
					{
						ItemsToExpand.Add(*Item);
					}
					return EBreakBehavior::Continue;
				});
			}
		}

		if (!ItemsToExpand.IsEmpty())
		{
			ReplicatedObjects->SetExpandedItems(ItemsToExpand, true);
		}
	}

	TArray<TSharedPtr<FReplicatedObjectData>> SReplicationStreamViewer::GetSelectedOutlinerObjects() const
	{
		TArray<TSharedPtr<FReplicatedObjectData>> SelectedItems = ReplicatedObjects->GetSelectedItems();
		// Items may have been removed this tick. However, selected items may not have been updated yet because STreeView processes item changes at the end of tick. 
		SelectedItems.SetNum(Algo::RemoveIf(SelectedItems, [this](const TSharedPtr<FReplicatedObjectData>& ObjectData)
		{
			const bool bIsInModel = PropertiesModel->ContainsObjects({ ObjectData->GetObjectPath() });
			const TOptional<FSoftObjectPath> OwningActor = ObjectUtils::GetActorOf(ObjectData->GetObjectPath());
			const bool bHierarchyIsInModel = OwningActor.IsSet() && PropertiesModel->ContainsObjects({ *OwningActor });
			return !bIsInModel && !bHierarchyIsInModel;
		}));
		return SelectedItems;
	}

	void SReplicationStreamViewer::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
	{
		if (bHasRequestedObjectRefresh)
		{
			bHasRequestedObjectRefresh = false;
			bHasRequestedPropertyRefresh = true;
			RefreshObjectData();
		}

		if (bHasRequestedPropertyRefresh)
		{
			bHasRequestedPropertyRefresh = false;
			RefreshPropertyData();
		}

		if (!PendingToSelect.IsEmpty())
		{
			SelectObjects(PendingToSelect);
			PendingToSelect.Reset();
		}

		if (!PendingToExpand.IsEmpty())
		{
			ExpandObjects(PendingToExpand, bPendingExpandRecursively);
			PendingToExpand.Reset();
		}
		
		IReplicationStreamViewer::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	}

	TSharedRef<FReplicatedObjectData> SReplicationStreamViewer::AllocateObjectData(FSoftObjectPath ObjectPath)
	{
		return MakeShared<FReplicatedObjectData>(MoveTemp(ObjectPath));
	}

	TSharedRef<SWidget> SReplicationStreamViewer::CreateContentWidget(const FArguments& InArgs)
	{
		return SNew(SSplitter)
			.Orientation(Orient_Vertical)

			+SSplitter::Slot()
			 .Value(1.f)
			[
				CreateOutlinerSection(InArgs)
			]

			+SSplitter::Slot()
			.SizeRule(TAttribute<SSplitter::ESizeRule>(this, &SReplicationStreamViewer::GetPropertyAreaSizeRule))
			.Value(2.f)
			[
				CreatePropertiesSection(InArgs)
			];
	}

	TSharedRef<SWidget> SReplicationStreamViewer::CreateOutlinerSection(const FArguments& InArgs)
	{
		TArray<FObjectColumnEntry> Columns = InArgs._ObjectColumns;
		Columns.Add(ReplicationColumns::TopLevel::LabelColumn(PropertiesModel.ToSharedRef(), NameModel.Get()));
		Columns.Add(ReplicationColumns::TopLevel::TypeColumn(PropertiesModel.ToSharedRef()));
		
		const bool bHasNoOutlinerObjectsAttribute = InArgs._NoOutlinerObjects.IsBound() || InArgs._NoOutlinerObjects.IsSet(); 
		const TAttribute<FText> NoObjectsAttribute = bHasNoOutlinerObjectsAttribute ? InArgs._NoOutlinerObjects : LOCTEXT("NoObjects", "No objects to display");

		// Set both primary and secondary in case one is overriden but always use the override.
		const FColumnSortInfo PrimaryObjectSort = InArgs._PrimaryObjectSort.IsValid()
			? InArgs._PrimaryObjectSort
			: FColumnSortInfo{ ReplicationColumns::TopLevel::LabelColumnId, EColumnSortMode::Ascending };
		const FColumnSortInfo SecondaryObjectSort = InArgs._SecondaryObjectSort.IsValid()
			? InArgs._SecondaryObjectSort
			: FColumnSortInfo{ ReplicationColumns::TopLevel::LabelColumnId, EColumnSortMode::Ascending };

		ObjectViewOptions.OnDisplaySubobjectsToggled().AddSP(this, &SReplicationStreamViewer::OnSubobjectViewOptionToggled);
		
		return SAssignNew(ReplicatedObjects, SReplicationTreeView<FReplicatedObjectData>)
			.RootItemsSource(&RootObjectRowData)
			.OnGetChildren(this, &SReplicationStreamViewer::GetObjectRowChildren)
			.OnContextMenuOpening(InArgs._OnObjectsContextMenuOpening)
			.OnDeleteItems(InArgs._OnDeleteObjects)
			.OnSelectionChanged_Lambda([this]()
			{
				RequestPropertyDataRefresh();
			})
			.Columns(FObjectColumnAdapter::Transform(MoveTemp(Columns)))
			.ExpandableColumnLabel(ReplicationColumns::TopLevel::LabelColumnId)
			.PrimarySort(PrimaryObjectSort)
			.SecondarySort(SecondaryObjectSort)
			.SelectionMode(ESelectionMode::Multi)
			.LeftOfSearchBar() [ InArgs._LeftOfObjectSearchBar.Widget ]
			.RightOfSearchBar()
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					InArgs._RightOfObjectSearchBar.Widget
				]
				
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					ObjectViewOptions.MakeViewOptionsComboButton()
				]
			]
			.NoItemsContent() [ SNew(STextBlock).Text(NoObjectsAttribute) ]
			.RowStyle(FAppStyle::Get(), "TableView.AlternatingRow");
	}

	TSharedRef<SWidget> SReplicationStreamViewer::CreatePropertiesSection(const FArguments& InArgs)
	{
		return SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			.Padding(0.0f)
			[
				SAssignNew(PropertyArea, SExpandableArea)
				.InitiallyCollapsed(true) 
				.BorderBackgroundColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f))
				.BorderImage_Lambda([this]() { return ConcertFrontendUtils::GetExpandableAreaBorderImage(*PropertyArea); })
				.BodyBorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
				.BodyBorderBackgroundColor(FLinearColor::White)
				.OnAreaExpansionChanged(this, &SReplicationStreamViewer::OnPropertyAreaExpansionChanged)
				.Padding(0.0f)
				.HeaderContent()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ReplicatedProperties", "Properties"))
					.Font(FAppStyle::Get().GetFontStyle("DetailsView.CategoryFontStyle"))
					.ShadowOffset(FVector2D(1.0f, 1.0f))
				]
				.BodyContent()
				[
					SAssignNew(PropertySection, SReplicatedPropertyView, InArgs._PropertyTreeView.ToSharedRef(), PropertiesModel.ToSharedRef())
					.GetSelectedRootObjects_Lambda([this](){ return GetSelectedOutlinerObjects(); })
					.NameModel(InArgs._NameModel)
				]
			];
	}
	
	void SReplicationStreamViewer::RefreshObjectData()
	{
		// Re-using existing instances is tricky: we cannot update the object path in an item because the list view will no detect this change;
		// list view only looks at the shared ptr address. So the UI will not be refreshed. Since the number of items will be small, just reallocate... 
		AllObjectRowData.Empty();

		// Try to re-use old instances by using the old PathToObjectDataCache. This is also done so the expansion states restore correctly in the tree view.
		TMap<FSoftObjectPath, TSharedPtr<FReplicatedObjectData>> NewPathToObjectDataCache;
		
		// Do a complete refresh.
		// Complete refresh is acceptable because the list is updated infrequently and typically small < 500 items.
		// An alternative would be to change RefreshObjectData to be called with two variables ObjectsAdded and ObjectsRemoved.
		PropertiesModel->ForEachReplicatedObject([this, &NewPathToObjectDataCache](const FSoftObjectPath& ObjectPath) mutable
		{
			TOptional<IObjectHierarchyModel::FParentInfo> ParentInfo = ObjectHierarchy->GetParentInfo(ObjectPath);
			const bool bIsActor = !ParentInfo; 
			if (bIsActor || ShouldDisplayObject(ObjectPath, ParentInfo->Relationship))
			{
				const TSharedPtr<FReplicatedObjectData>* ExistingItem = PathToObjectDataCache.Find(ObjectPath);
				ExistingItem = ExistingItem ? ExistingItem : NewPathToObjectDataCache.Find(ObjectPath);
				const TSharedRef<FReplicatedObjectData> Item = ExistingItem ? ExistingItem->ToSharedRef() : AllocateObjectData(ObjectPath);
				AllObjectRowData.AddUnique(Item);
				NewPathToObjectDataCache.Emplace(ObjectPath, Item);
				
				BuildObjectHierarchyIfNeeded(Item, NewPathToObjectDataCache);
			}
			
			return EBreakBehavior::Continue;
		});

		// Only refresh the tree if it is necessary as it causes us to select stuff in the subobject view
		if (!PathToObjectDataCache.OrderIndependentCompareEqual(NewPathToObjectDataCache))
		{
			// If an item was removed, then NewPathToObjectDataCache does not contain it. 
			PathToObjectDataCache = MoveTemp(NewPathToObjectDataCache);

			// The tree view requires the item source to only contain the root items. Children are discovered via GetObjectRowChildren. We re-use GetObjectRowChildren to remove any non-root nodes.
			BuildRootObjectRowData();
			ReplicatedObjects->RequestRefilter();
		}
	}

	void SReplicationStreamViewer::RefreshPropertyData()
	{
		PropertySection->RefreshPropertyData();
	}

	void SReplicationStreamViewer::BuildRootObjectRowData()
	{
		TSet<TSharedPtr<FReplicatedObjectData>> NonRootNodes;
		for (const TSharedPtr<FReplicatedObjectData>& Node : AllObjectRowData)
		{
			GetObjectRowChildren(Node, [&NonRootNodes](TSharedPtr<FReplicatedObjectData> Child)
			{
				NonRootNodes.Add(MoveTemp(Child));
			});
		}

		// Make RootObjectRowData only contain those nodes which were not listed as children 
		RootObjectRowData.Empty(NonRootNodes.Num());
		for (const TSharedPtr<FReplicatedObjectData>& Node : AllObjectRowData)
		{
			if (!NonRootNodes.Contains(Node))
			{
				RootObjectRowData.Add(Node);
			}
		}
		
		RootObjectRowData.Sort([](const TSharedPtr<FReplicatedObjectData>& Left, const TSharedPtr<FReplicatedObjectData>& Right)
		{
			return Left->GetObjectPath().GetSubPathString() < Right->GetObjectPath().GetSubPathString();
		});
	}

	void SReplicationStreamViewer::BuildObjectHierarchyIfNeeded(TSharedPtr<FReplicatedObjectData> ReplicatedObjectData, TMap<FSoftObjectPath, TSharedPtr<FReplicatedObjectData>>& NewPathToObjectDataCache)
	{
		// We're are not supposed to display any hierarchy in the outliner if ObjectHierarchy is not set.
		const FSoftObjectPath& ObjectPath = ReplicatedObjectData->GetObjectPath();
		if (!ObjectHierarchy)
		{
			return;
		}

		// Find top level object of ReplicatedObjectData
		const FSoftObjectPath OwningActor = ObjectUtils::GetActorOf(ObjectPath).Get(ObjectPath);
		if (!ObjectUtils::IsActor(OwningActor))
		{
			return;
		}
		
		// Add all objects that appear in the hierarchy of ReplicatedObjectData
		const auto AddItem = [this, &NewPathToObjectDataCache](const FSoftObjectPath& ObjectPath)
		{
			const TSharedPtr<FReplicatedObjectData>* ExistingItem = PathToObjectDataCache.Find(ObjectPath);
			ExistingItem = ExistingItem ? ExistingItem : NewPathToObjectDataCache.Find(ObjectPath);
			const TSharedRef<FReplicatedObjectData> Item = ExistingItem ? ExistingItem->ToSharedRef() : AllocateObjectData(ObjectPath);
			AllObjectRowData.AddUnique(Item);
			NewPathToObjectDataCache.Emplace(ObjectPath, Item);
		};
		
		AddItem(OwningActor);
		ObjectHierarchy->ForEachChildRecursive(OwningActor, [this, &AddItem](const FSoftObjectPath&, const FSoftObjectPath& ChildObject, EChildRelationship Relationship)
		{
			if (ShouldDisplayObject(ChildObject, Relationship))
			{
				AddItem(ChildObject);
			}
			return EBreakBehavior::Continue;
		});
	}

	void SReplicationStreamViewer::GetObjectRowChildren(TSharedPtr<FReplicatedObjectData> ReplicatedObjectData, TFunctionRef<void(TSharedPtr<FReplicatedObjectData>)> ProcessChild)
	{
		// Important: this view should be possible to be built in programs, so it should not reference things like AActor, UActorComponent, ResolveObject, etc. directly.
		
		const FSoftObjectPath& SearchedObject = ReplicatedObjectData->GetObjectPath();
		if (!ObjectHierarchy)
		{
			return;
		}

		ObjectHierarchy->ForEachDirectChild(SearchedObject, [this, &ProcessChild](const FSoftObjectPath& ChildObject, EChildRelationship Relationship)
		{
			if (const TSharedPtr<FReplicatedObjectData>* ObjectData = PathToObjectDataCache.Find(ChildObject))
			{
				ProcessChild(*ObjectData);
			}
			return EBreakBehavior::Continue;
		});
	}

	bool SReplicationStreamViewer::ShouldDisplayObject(const FSoftObjectPath& Object, EChildRelationship Relationship) const
	{
		const bool bSkipSubobject = Relationship == EChildRelationship::Subobject && !ObjectViewOptions.ShouldDisplaySubobjects();
		return !bSkipSubobject;
	}
}

#undef LOCTEXT_NAMESPACE