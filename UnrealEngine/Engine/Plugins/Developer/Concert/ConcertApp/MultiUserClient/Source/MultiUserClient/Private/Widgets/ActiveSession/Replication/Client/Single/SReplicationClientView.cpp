// Copyright Epic Games, Inc. All Rights Reserved.

#include "SReplicationClientView.h"

#include "Replication/ClientReplicationWidgetFactories.h"
#include "Replication/Client/ReplicationClient.h"
#include "Replication/Editor/Model/PropertySource/SelectPropertyFromUClassModel.h"
#include "Replication/Editor/View/IReplicationStreamEditor.h"
#include "Replication/Editor/Model/ObjectSource/ActorSelectionSourceModel.h"
#include "Replication/ReplicationWidgetFactories.h"
#include "Replication/Client/ReplicationClientManager.h"
#include "Replication/Submission/ISubmissionWorkflow.h"
#include "Widgets/ActiveSession/Replication/Client/FrequencyContextMenuUtils.h"
#include "Widgets/ActiveSession/Replication/Client/Single/Columns/SingleClientColumns.h"
#include "Widgets/ActiveSession/Replication/Client/SReplicationStatus.h"
#include "Widgets/ActiveSession/Replication/Client/SClientToolbar.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "SReplicationClientView"

namespace UE::MultiUserClient
{
	void SReplicationClientView::Construct(const FArguments& InArgs, const TSharedRef<IConcertClient>& InClient, FReplicationClientManager& InClientManager)
	{
		ConcertClient = InClient;
		ClientManager = &InClientManager;
		
		GetReplicationClientAttribute = InArgs._GetReplicationClient;
		FReplicationClient* ReplicationClient = GetReplicationClientAttribute.Get();
		check(ReplicationClient);
		
		FGlobalAuthorityCache& AuthorityCache = ClientManager->GetAuthorityCache();
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
				CreateContent(*ReplicationClient)
			]
		];

		SReplicationStatus::AppendReplicationStatus(*Content, InClientManager.GetAuthorityCache(),
			SReplicationStatus::FArguments()
			.DisplayedClients(TSet{ ReplicationClient->GetEndpointId() })
			.ForEachReplicatedObject(this, &SReplicationClientView::EnumerateReplicatedObjects)
			);
		
		// Refresh UI if streams change externally, e.g. a remote client changed what they sent
		ReplicationClient->OnModelChanged().AddSP(this, &SReplicationClientView::OnModelChanged);
		// It's a bit excessive to refresh all objects when the hierarchy might have changed but it's simple (and only happens once at end of tick)
		ReplicationClient->OnHierarchyNeedsRefresh().AddLambda([this](){ EditorView->Refresh(); });
	}

	TSharedRef<SWidget> SReplicationClientView::CreateContent(FReplicationClient& InReplicationClient)
	{
		using namespace ConcertSharedSlate;
		FAuthorityChangeTracker& AuthorityTracker = InReplicationClient.GetAuthorityDiffer();
		ISubmissionWorkflow& SubmissionWorkflow = InReplicationClient.GetSubmissionWorkflow();
		FGlobalAuthorityCache& AuthorityCache = ClientManager->GetAuthorityCache();
		
		const TAttribute<const IReplicationStreamViewer*> GetReplicationViewerAttribute =
			TAttribute<const IReplicationStreamViewer*>::CreateLambda([this](){ return EditorView.Get(); });

		// Add checkboxes in front of top level and subobject rows for changing authority
		ConcertClientSharedSlate::FDefaultStreamEditorParams DefaultEditorParams
		{
			.BaseEditorParams =
			{
				.DataModel = InReplicationClient.GetClientEditModel(),
				.ObjectSource = MakeShared<ConcertClientSharedSlate::FActorSelectionSourceModel>(),
				.PropertySource = MakeShared<ConcertClientSharedSlate::FSelectPropertyFromUClassModel>(),
				.IsEditingEnabled = TAttribute<bool>::CreateLambda([&SubmissionWorkflow](){ return CanEverSubmit(SubmissionWorkflow.GetUploadability()); }),
				.EditingDisabledToolTipText = LOCTEXT("Editing.NotImplemented", "Editing remote clients is not implemented. You can only edit the local client."),
			},
			.PropertyColumns =
			{
				SingleClientColumns::ConflictWarningForProperty(ConcertClient.ToSharedRef(), GetReplicationViewerAttribute, AuthorityCache, InReplicationClient.GetEndpointId()),
				SingleClientColumns::OwnerOfProperty(ConcertClient.ToSharedRef(), AuthorityCache, GetReplicationViewerAttribute) 
			},
			.ObjectHierarchy = ConcertClientSharedSlate::CreateObjectHierarchyForComponentHierarchy(), // This makes actors have children in the top view
            .NameModel = ConcertClientSharedSlate::CreateEditorObjectNameModel(), // This makes actors use their labels, and components use the names given in the BP editor
            .OnExtendObjectsContextMenu = FExtendObjectMenu::CreateSP(this, &SReplicationClientView::ExtendObjectContextMenu),
			.ObjectColumns =
			{
				SingleClientColumns::ToggleObjectAuthority(AuthorityTracker, SubmissionWorkflow),
				SingleClientColumns::OwnerOfObject(ConcertClient.ToSharedRef(), AuthorityCache),
				SingleClientColumns::ConflictWarningForObject(ConcertClient.ToSharedRef(), AuthorityCache, InReplicationClient.GetEndpointId())
			}
		};

		EditorView = ConcertClientSharedSlate::CreateDefaultStreamEditor(MoveTemp(DefaultEditorParams));
		return EditorView.ToSharedRef();
	}

	void SReplicationClientView::ExtendObjectContextMenu(FMenuBuilder& MenuBuilder, TConstArrayView<FSoftObjectPath> ContextObjects) const
	{
		const FReplicationClient* ReplicationClient = GetReplicationClientAttribute.Get();
		check(ReplicationClient);
		
		FrequencyContextMenuUtils::AddFrequencyOptionsIfOneContextObject_SingleClient(MenuBuilder, ContextObjects, ReplicationClient->GetEndpointId(), *ClientManager);
	}

	void SReplicationClientView::OnModelChanged() const
	{
		EditorView->Refresh();
	}

	void SReplicationClientView::EnumerateReplicatedObjects(TFunctionRef<void(const FSoftObjectPath&)> Consumer) const
	{
		const FReplicationClient* ReplicationClient = GetReplicationClientAttribute.Get();
		check(ReplicationClient);
		
		ReplicationClient->GetClientEditModel()->ForEachReplicatedObject([&Consumer](const FSoftObjectPath& Object)
		{
			Consumer(Object);
			return EBreakBehavior::Continue;
		});
	}
}

#undef LOCTEXT_NAMESPACE