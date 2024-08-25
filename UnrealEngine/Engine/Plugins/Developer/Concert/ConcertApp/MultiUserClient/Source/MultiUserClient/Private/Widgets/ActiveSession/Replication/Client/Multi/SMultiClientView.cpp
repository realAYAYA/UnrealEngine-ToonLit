// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMultiClientView.h"

#include "IClientSelectionModel.h"
#include "MultiStreamModel.h"
#include "Replication/ClientReplicationWidgetFactories.h"
#include "Replication/ReplicationWidgetFactories.h"
#include "Replication/Client/ReplicationClient.h"
#include "Replication/Client/ReplicationClientManager.h"
#include "Replication/Editor/Model/PropertySource/SelectPropertyFromUClassModel.h"
#include "Replication/Editor/View/IMultiReplicationStreamEditor.h"
#include "Replication/Editor/View/IReplicationStreamEditor.h"
#include "Replication/Editor/Model/ObjectSource/ActorSelectionSourceModel.h"
#include "Widgets/ActiveSession/Replication/Client/FrequencyContextMenuUtils.h"
#include "Widgets/ActiveSession/Replication/Client/Multi/Columns/MultiStreamColumns.h"
#include "Widgets/ActiveSession/Replication/Client/SClientToolbar.h"
#include "Widgets/ActiveSession/Replication/Client/SReplicationStatus.h"

#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "SMultiClientView"

namespace UE::MultiUserClient
{
	void SMultiClientView::Construct(const FArguments& InArgs, TSharedRef<IConcertClient> InConcertClient, FReplicationClientManager& InClientManager, IClientSelectionModel& InDisplayClientsModel)
	{
		StreamModel = MakeShared<FMultiStreamModel>(InDisplayClientsModel, InClientManager);

		ClientManager = &InClientManager;
		ClientManager->OnRemoteClientsChanged().AddSP(this, &SMultiClientView::RebuildClientSubscriptions);
		SelectionModel = &InDisplayClientsModel;
		SelectionModel->OnSelectionChanged().AddSP(this, &SMultiClientView::RebuildClientSubscriptions);

		TSharedPtr<SVerticalBox> Content;
		ChildSlot
		[
			SAssignNew(Content, SVerticalBox)

			// Toolbar
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.f)
			[
				SNew(SClientToolbar)
				.ViewSelectionArea() [ InArgs._ViewSelectionArea.Widget ]
			]

			// Editor
			+SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				CreateEditorContent(InConcertClient, InClientManager)
			]
		];
		
		SReplicationStatus::AppendReplicationStatus(*Content, InClientManager.GetAuthorityCache(),
			SReplicationStatus::FArguments()
			.DisplayedClients(this, &SMultiClientView::GetDisplayClientIds)
			.ForEachReplicatedObject(this, &SMultiClientView::EnumerateObjectsInStreams)
			);

		RebuildClientSubscriptions();
	}

	SMultiClientView::~SMultiClientView()
	{
		ClientManager->OnRemoteClientsChanged().RemoveAll(this);
		CleanClientSubscriptions();
	}

	TSharedRef<SWidget> SMultiClientView::CreateEditorContent(const TSharedRef<IConcertClient>& InConcertClient, FReplicationClientManager& InClientManager)
	{
		using namespace UE::ConcertSharedSlate;

		TAttribute<TSharedPtr<IMultiReplicationStreamEditor>> MultiStreamEditorAttribute =
		   TAttribute<TSharedPtr<IMultiReplicationStreamEditor>>::CreateLambda([this]()
		   {
			   return StreamEditor;
		   });
		const TAttribute<IObjectHierarchyModel*> ObjectHierarchyAttribute =
		   TAttribute<IObjectHierarchyModel*>::CreateLambda([this]()
		   {
			   return ObjectHierarchy.Get();
		   });
		FGetAutoAssignTarget GetAutoAssignTargetDelegate = FGetAutoAssignTarget::CreateLambda([this, &InClientManager](TConstArrayView<UObject*>)
		{
			const TSharedRef<IEditableReplicationStreamModel>& LocalStream = InClientManager.GetLocalClient().GetClientEditModel();
			return StreamModel->GetEditableStreams().Contains(LocalStream) ? LocalStream.ToSharedPtr() : nullptr;
		});
		
		ConcertClientSharedSlate::FFilterablePropertyTreeViewParams TreeViewParams
		{
			.AdditionalPropertyColumns =
			{
				ReplicationColumns::Property::LabelColumn(),
				ReplicationColumns::Property::TypeColumn(),
				MultiStreamColumns::AssignPropertyColumn(MultiStreamEditorAttribute, InConcertClient, InClientManager)
			}
		};
		TSharedRef<IPropertyTreeView> PropertyTreeView = CreateFilterablePropertyTreeView(MoveTemp(TreeViewParams));
		
		FCreateMultiStreamEditorParams Params
		{
			.MultiStreamModel = StreamModel.ToSharedRef(),
			.ConsolidatedObjectModel = ConcertClientSharedSlate::CreateTransactionalStreamModel(),
			.ObjectSource = MakeShared<ConcertClientSharedSlate::FActorSelectionSourceModel>(),
			.PropertySource = MakeShared<ConcertClientSharedSlate::FSelectPropertyFromUClassModel>(),
			.GetAutoAssignToStreamDelegate = MoveTemp(GetAutoAssignTargetDelegate)
		};
		
		ObjectHierarchy = ConcertClientSharedSlate::CreateObjectHierarchyForComponentHierarchy();
		FCreateViewerParams ViewerParams
		{
			.PropertyTreeView = MoveTemp(PropertyTreeView),
			.ObjectHierarchy = ObjectHierarchy, // This makes actors have children in the top view
			.NameModel = ConcertClientSharedSlate::CreateEditorObjectNameModel(), // This makes actors use their labels, and components use the names given in the BP editor
			.OnExtendObjectsContextMenu = FExtendObjectMenu::CreateSP(this, &SMultiClientView::ExtendObjectContextMenu),
			.ObjectColumns =
			{
				MultiStreamColumns::ReplicationToggle(InConcertClient, ObjectHierarchyAttribute, InClientManager),
				MultiStreamColumns::ReassignOwnership(InConcertClient, MultiStreamEditorAttribute, ObjectHierarchyAttribute, InClientManager.GetReassignmentLogic(), InClientManager)
			}
		};
		
		StreamEditor = CreateBaseMultiStreamEditor(MoveTemp(Params), MoveTemp(ViewerParams));
		check(StreamEditor);
		return StreamEditor.ToSharedRef();
	}

	TSet<FGuid> SMultiClientView::GetDisplayClientIds() const
	{
		TSet<FGuid> ClientIds;
		StreamModel->ForEachClient([&ClientIds](const FReplicationClient* Client)
		{
			ClientIds.Add(Client->GetEndpointId());
			return EBreakBehavior::Continue;
		});
		return ClientIds;
	}

	void SMultiClientView::EnumerateObjectsInStreams(TFunctionRef<void(const FSoftObjectPath&)> Consumer) const
	{
		StreamModel->ForEachClient([&Consumer](const FReplicationClient* Client)
		{
			Client->GetClientEditModel()->ForEachReplicatedObject([&Consumer](const FSoftObjectPath& Object)
			{
				Consumer(Object);
				return EBreakBehavior::Continue;
			});
			return EBreakBehavior::Continue;
		});
	}

	void SMultiClientView::RebuildClientSubscriptions()
	{
		CleanClientSubscriptions();

		ClientManager->ForEachClient([this](FReplicationClient& Client)
		{
			if (SelectionModel->ContainsClient(Client.GetEndpointId()))
			{
				Client.OnModelChanged().AddSP(this, &SMultiClientView::OnClientChanged);
				Client.OnHierarchyNeedsRefresh().AddRaw(this, &SMultiClientView::OnHierarchyNeedsRefresh);
			}
			
			return EBreakBehavior::Continue;
		});
	}

	void SMultiClientView::CleanClientSubscriptions() const
	{
		ClientManager->ForEachClient([this](FReplicationClient& Client)
		{
			Client.OnModelChanged().RemoveAll(this);
			Client.OnHierarchyNeedsRefresh().RemoveAll(this);
			return EBreakBehavior::Continue;
		});
	}

	void SMultiClientView::OnClientChanged() const
	{
		// When reassignment operations complete, the content of the columns changes so a resort is required.
		StreamEditor->GetEditorBase().RequestObjectColumnResort(MultiStreamColumns::ReassignOwnershipColumnId);
		StreamEditor->GetEditorBase().RequestPropertyColumnResort(MultiStreamColumns::AssignPropertyColumnId);
	}

	void SMultiClientView::OnHierarchyNeedsRefresh() const
	{
		// It's a bit excessive to refresh all objects when the hierarchy might have changed but it's simple (and only happens once at end of tick)
		StreamEditor->GetEditorBase().Refresh();
	}

	void SMultiClientView::ExtendObjectContextMenu(FMenuBuilder& MenuBuilder, TConstArrayView<FSoftObjectPath> ContextObjects) const
	{
		FrequencyContextMenuUtils::AddFrequencyOptionsIfOneContextObject_MultiClient(MenuBuilder, ContextObjects, *ClientManager);
	}
}

#undef LOCTEXT_NAMESPACE