// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/ClientReplicationWidgetFactories.h"

#include "Editor/View/ClientEditorColumns.h"
#include "Editor/View/PropertyTree/SFilteredPropertyTreeView.h"
#include "Replication/ReplicationWidgetFactories.h"
#include "Replication/Editor/Model/PropertyUtils.h"
#include "Replication/Editor/Model/Object/EditorObjectHierarchyModel.h"
#include "Replication/Editor/Model/Object/EditorObjectNameModel.h"
#include "Replication/Editor/Model/ReplicationStreamObject.h"
#include "Replication/Editor/View/Column/SelectionViewerColumns.h"
#include "Replication/Editor/View/ObjectEditor/SDefaultReplicationStreamEditor.h"
#include "Replication/Editor/Model/TransactionalReplicationStreamModel.h"

#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"

namespace UE::ConcertClientSharedSlate
{
	TSharedRef<ConcertSharedSlate::IObjectHierarchyModel> CreateObjectHierarchyForComponentHierarchy()
	{
		return MakeShared<FEditorObjectHierarchyModel>();
	}

	TSharedRef<ConcertSharedSlate::IObjectNameModel> CreateEditorObjectNameModel()
	{
		return MakeShared<FEditorObjectNameModel>();
	}

	TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel> CreateTransactionalStreamModel(
		TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel> BaseModel,
		UObject& OwnerObject
		)
	{
		return MakeShared<ConcertSharedSlate::FTransactionalReplicationStreamModel>(
			MoveTemp(BaseModel),
			OwnerObject
			);
	}

	TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel> CreateTransactionalStreamModel()
	{
		constexpr EObjectFlags Flags = RF_Transient | RF_Transactional;
		UReplicationStreamObject* Object = NewObject<UReplicationStreamObject>(GetTransientPackage(), NAME_None, Flags);

		TAttribute<FConcertObjectReplicationMap*> Attribute = TAttribute<FConcertObjectReplicationMap*>::CreateLambda([WeakPtr = TWeakObjectPtr<UReplicationStreamObject>(Object)]() -> FConcertObjectReplicationMap* 
		{
			if (WeakPtr.IsValid())
			{
				return &WeakPtr->ReplicationMap;
			}
			return nullptr;
		});

		return CreateTransactionalStreamModel(ConcertSharedSlate::CreateBaseStreamModel(MoveTemp(Attribute)), *Object);
	}
	
	TSharedRef<ConcertSharedSlate::IPropertyTreeView> CreateFilterablePropertyTreeView(FFilterablePropertyTreeViewParams Params)
	{
		return SNew(SFilteredPropertyTreeView, MoveTemp(Params));
	}
	
	TSharedRef<ConcertSharedSlate::IReplicationStreamEditor> CreateDefaultStreamEditor(FDefaultStreamEditorParams Params)
	{
		using namespace ConcertSharedSlate;
		using namespace ConcertSharedSlate::ReplicationColumns;

		// When a user adds e.g. a struct property all of its child properties should be auto added as well.
		FExtendProperties ExtendPropertiesDelegate = FExtendProperties::CreateLambda(
		[DataModel = Params.BaseEditorParams.DataModel, AppendPropertyDelegate = MoveTemp(Params.OnExtendAddedProperties)]
			(const FSoftObjectPath& Object, TArray<FConcertPropertyChain>& InOutPropertiesToAdd)
			{
				const FSoftClassPath ClassPath = DataModel->GetObjectClass(Object);
				PropertyUtils::AppendAdditionalPropertiesToAdd(ClassPath, InOutPropertiesToAdd);
				AppendPropertyDelegate.ExecuteIfBound(Object, InOutPropertiesToAdd);
			});
		
		// This is a hack.
		// The architecturally correct way to fix is pass FReplicationPropertyColumn the FSoftObjectPath to the object for which the column is being constructed.
		struct FEditorIndirection
		{
			TSharedPtr<IReplicationStreamEditor> Editor;
		};
		TSharedRef<FEditorIndirection> Indirection = MakeShared<FEditorIndirection>();
		
		const FPropertyColumnEntry ReplicatesColumn = ReplicationColumns::Property::ReplicatesColumns(
			TAttribute<IReplicationStreamViewer*>::CreateLambda([Indirection](){ return Indirection->Editor.Get(); }),
			Params.BaseEditorParams.DataModel,
			MoveTemp(ExtendPropertiesDelegate),
			TCheckboxColumnDelegates<FPropertyTreeRowContext>::FIsEnabled::CreateLambda([IsEnabled = Params.BaseEditorParams.IsEditingEnabled](const FPropertyTreeRowContext&)
			{
				return !IsEnabled.IsBound() || IsEnabled.Get();
			}),
			Params.BaseEditorParams.EditingDisabledToolTipText
			);
		
		TArray<FPropertyColumnEntry>& PropertyColumns = Params.PropertyColumns;
		const bool bHasType = PropertyColumns.ContainsByPredicate([](const FPropertyColumnEntry& Entry)
		{
			return Entry.ColumnId == Property::TypeColumnId;
		});
		if (!bHasType)
		{
			PropertyColumns.Add(Property::TypeColumn());
		}
		PropertyColumns.Add(ReplicatesColumn);

		FCreateViewerParams ViewerParams
		{
			.PropertyTreeView = CreateFilterablePropertyTreeView({ .AdditionalPropertyColumns = MoveTemp(PropertyColumns) }),
			.ObjectHierarchy = MoveTemp(Params.ObjectHierarchy),
			.NameModel = MoveTemp(Params.NameModel),
			.OnExtendObjectsContextMenu = MoveTemp(Params.OnExtendObjectsContextMenu),
			.ObjectColumns = MoveTemp(Params.ObjectColumns),
			.PrimaryObjectSort = FColumnSortInfo{ TopLevel::LabelColumnId, EColumnSortMode::Ascending },
			.SecondaryObjectSort = FColumnSortInfo{ TopLevel::LabelColumnId, EColumnSortMode::Ascending },
		};
		
		TSharedRef<SDefaultReplicationStreamEditor> Editor = SNew(SDefaultReplicationStreamEditor, MoveTemp(Params.BaseEditorParams), MoveTemp(ViewerParams));
		Indirection->Editor = Editor;
		return Editor;
	}
}