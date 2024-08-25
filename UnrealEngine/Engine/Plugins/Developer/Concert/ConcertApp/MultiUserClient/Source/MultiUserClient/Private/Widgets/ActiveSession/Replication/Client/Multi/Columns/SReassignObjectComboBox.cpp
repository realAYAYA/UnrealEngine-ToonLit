// Copyright Epic Games, Inc. All Rights Reserved.

#include "SReassignObjectComboBox.h"

#include "MultiUserReplicationStyle.h"
#include "Misc/EBreakBehavior.h"
#include "Replication/Client/ReplicationClient.h"
#include "Replication/Submission/MultiEdit/ReassignObjectPropertiesLogic.h"
#include "Widgets/ActiveSession/Replication/Client/ClientUtils.h"
#include "Widgets/ActiveSession/Replication/Misc/SNoClients.h"
#include "Widgets/ClientName/SHorizontalClientList.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Replication/Editor/Model/Object/IObjectHierarchyModel.h"
#include "Textures/SlateIcon.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SScaleBox.h"

#define LOCTEXT_NAMESPACE "SReassignObjectComboBox"

namespace UE::MultiUserClient
{
	namespace ReassignObjctComboBox
	{
		TArray<FGuid> GetDisplayedClients(
			const FReassignObjectPropertiesLogic& ReassignmentLogic,
			const FSoftObjectPath& ManagedObject
			)
		{
			TArray<FGuid> ClientsWithOwnership;
			ReassignmentLogic.EnumerateClientOwnershipState(ManagedObject, [&ClientsWithOwnership](const FGuid& ClientId, FReassignObjectPropertiesLogic::EOwnershipState Ownership)
			{
				if (Ownership == FReassignObjectPropertiesLogic::EOwnershipState::HasObjectRegistered)
				{
					ClientsWithOwnership.Add(ClientId);
				}
				return EBreakBehavior::Continue;
			});
			return ClientsWithOwnership;
		}
	}
	
	void SReassignObjectComboBox::PopulateSearchTerms(
		const IConcertClientSession& Session,
		const FReassignObjectPropertiesLogic& ReassignmentLogic,
		const FSoftObjectPath& ManagedObject,
		TArray<FString>& InOutSearchTerms
		)
	{
		for (const FGuid& ClientId : ReassignObjctComboBox::GetDisplayedClients(ReassignmentLogic, ManagedObject))
		{
			InOutSearchTerms.Add(ClientUtils::GetClientDisplayName(Session, ClientId));
		}
	}

	TOptional<FString> SReassignObjectComboBox::GetDisplayString(
		const TSharedRef<IConcertClient>& LocalConcertClient,
		const FReassignObjectPropertiesLogic& ReassignmentLogic,
		const FSoftObjectPath& ManagedObject
		)
	{
		using SWidgetType = ConcertClientSharedSlate::SHorizontalClientList;
		const TArray<FGuid> Clients = ReassignObjctComboBox::GetDisplayedClients(ReassignmentLogic, ManagedObject);
		return SWidgetType::GetDisplayString(
			LocalConcertClient.Get(),
			Clients,
			SWidgetType::FSortPredicate::CreateStatic(&SWidgetType::SortLocalClientFirstThenAlphabetical, LocalConcertClient)
			);
	}

	void SReassignObjectComboBox::Construct(
		const FArguments& InArgs,
		TSharedRef<IConcertClient> InConcertClient,
		FReassignObjectPropertiesLogic& InReassignmentLogic,
		const FReplicationClientManager& InReplicationManager
		)
	{
		ConcertClient = MoveTemp(InConcertClient);
		ReassignmentLogic = &InReassignmentLogic;
		ReplicationManager = &InReplicationManager;
		HighlightText = InArgs._HighlightText;

		ManagedObject = InArgs._ManagedObject;
		ObjectHierarchyModelAttribute = InArgs._ObjectHierarchyModel;
		check(ObjectHierarchyModelAttribute.IsSet() || ObjectHierarchyModelAttribute.IsBound());

		OnReassignAllOptionClickedDelegate = InArgs._OnReassignAllOptionClicked;
		
		ChildSlot
		[
			SNew(SHorizontalBox)

			// For indicating that a change is pending
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SThrobber)
				.Visibility(this, &SReassignObjectComboBox::GetThrobberVisibility)
			]
			
			+SHorizontalBox::Slot()
			[
				SNew(SComboButton)
				.HasDownArrow(true)
				.ButtonContent()
				[
					SAssignNew(ComboClientList, ConcertClientSharedSlate::SHorizontalClientList, InConcertClient)
					.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
					.HighlightText_Lambda([this](){ return HighlightText ? *HighlightText : FText::GetEmpty(); })
					.EmptyListSlot() [ SNew(SNoClients) ]
				]
				.OnGetMenuContent(this, &SReassignObjectComboBox::MakeMenuContent)
			]
		];
		
		UpdateComboButtonContent();
		ReassignmentLogic->OnOwnershipChanged().AddSP(this, &SReassignObjectComboBox::UpdateComboButtonContent);
	}

	SReassignObjectComboBox::~SReassignObjectComboBox()
	{
		ReassignmentLogic->OnOwnershipChanged().RemoveAll(this);
	}

	void SReassignObjectComboBox::UpdateComboButtonContent() const
	{
		ComboClientList->RefreshList(
			ReassignObjctComboBox::GetDisplayedClients(*ReassignmentLogic, ManagedObject)
			);
	}

	EVisibility SReassignObjectComboBox::GetThrobberVisibility() const
	{
		if (!ReassignmentLogic->IsReassigning(ManagedObject))
		{
			return EVisibility::Collapsed;
		}
		
		const TOptional<FDateTime> TimeStarted = ReassignmentLogic->GetTimeReassignmentWasStarted();
		const float WaitTime = FMultiUserReplicationStyle::Get()->GetFloat(TEXT("AllClients.Reassignment.DisplayThrobberAfterSeconds"));
		const bool bIsTakingLongTime = TimeStarted
			&& (FDateTime::Now() - *TimeStarted).GetTotalSeconds() >= WaitTime;
		return bIsTakingLongTime ? EVisibility::Visible : EVisibility::Collapsed;
	}

	TSharedRef<SWidget> SReassignObjectComboBox::MakeMenuContent() const
	{
		FMenuBuilder MenuBuilder(false, nullptr);
		const TArray<const FReplicationClient*> SortedClients = ClientUtils::GetSortedClientList(*ConcertClient, *ReplicationManager);
		const FText CanAssignText;
		
		MenuBuilder.BeginSection(TEXT("Reassign.This"), LOCTEXT("Reassign.This", "Reassign this to"));
		AddReassignSection(MenuBuilder, SortedClients, FInlineObjectPathArray{ ManagedObject });
		MenuBuilder.EndSection();

		// Do not distract the user with more options if children have no assigned properties
		const bool bHasChildrenWithProperties = ReassignmentLogic->IsAnyObjectOwned(GetChildrenOfManagedObject());
		if (bHasChildrenWithProperties)
		{
			MenuBuilder.BeginSection(TEXT("Reassign.Children"), LOCTEXT("Reassign.Children", "Reassign children to"));
			AddReassignSection(MenuBuilder, SortedClients, TAttribute<FInlineObjectPathArray>::CreateLambda([this](){ return GetChildrenOfManagedObject(); }));
			MenuBuilder.EndSection();
		}
		
		return MenuBuilder.MakeWidget();
	}

	void SReassignObjectComboBox::AddReassignSection(
		FMenuBuilder& MenuBuilder,
		const TArray<const FReplicationClient*>& SortedClients,
		TAttribute<FInlineObjectPathArray> ObjectsToAssign
		) const
	{
		for (const FReplicationClient* Client : SortedClients)
		{
			const FGuid& ClientId = Client->GetEndpointId();
			MenuBuilder.AddMenuEntry(
				FText::FromString(ClientUtils::GetClientDisplayName(*ConcertClient, Client->GetEndpointId())),
				TAttribute<FText>::CreateLambda([this, ClientId, ObjectsToAssign]()
				{
					FText CannotEditReason;
					const bool bCanReassign = ReassignmentLogic->CanReassignAnyTo(ObjectsToAssign.Get(), ClientId, &CannotEditReason);
					return bCanReassign ? LOCTEXT("DoReassign", "Reassign to this client") : CannotEditReason;
				}),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([this, ObjectsToAssign, ClientId]()
					{
						TArray<FSoftObjectPath> ObjectsNotInline;
						Algo::Transform(ObjectsToAssign.Get(), ObjectsNotInline, [](const FSoftObjectPath& ObjectPath){ return ObjectPath; });
						ReassignmentLogic->ReassignAllTo(ObjectsNotInline, ClientId);
						
						OnReassignAllOptionClickedDelegate.ExecuteIfBound(ClientId);
					}),
					FCanExecuteAction::CreateLambda([this, ObjectsToAssign, ClientId](){ return ReassignmentLogic->CanReassignAnyTo(ObjectsToAssign.Get(), ClientId); }),
					FIsActionChecked::CreateLambda([this, ObjectsToAssign, ClientId](){ return ReassignmentLogic->OwnsAnyOf(ObjectsToAssign.Get(), ClientId); })
					),
					NAME_None,
					EUserInterfaceActionType::Check
				);
		}
	}

	SReassignObjectComboBox::FInlineObjectPathArray SReassignObjectComboBox::GetChildrenOfManagedObject() const
	{
		return ObjectHierarchyModelAttribute.Get()->GetChildrenRecursive<FInlineAllocator>(ManagedObject);
	}
}

#undef LOCTEXT_NAMESPACE