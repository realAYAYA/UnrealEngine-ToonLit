// Copyright Epic Games, Inc. All Rights Reserved.

#include "SReplicationMultiToggleCheckbox.h"

#include "Replication/Client/ReplicationClientManager.h"
#include "Replication/Util/GlobalAuthorityCache.h"
#include "Widgets/ActiveSession/Replication/Client/ClientUtils.h"

#include "Algo/AnyOf.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Replication/Editor/Model/Object/IObjectHierarchyModel.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateTypes.h"
#include "Textures/SlateIcon.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SScaleBox.h"

#define LOCTEXT_NAMESPACE "SReplicationMultiToggleCheckbox"

namespace UE::MultiUserClient
{
	void SReplicationMultiToggleCheckbox::Construct(
		const FArguments& InArgs,
		FReplicationClientManager& InClientManager,
		TSharedRef<IConcertClient> InConcertClient
		)
	{
		Object = InArgs._Object;
		ObjectHierarchyModelAttribute = InArgs._ObjectHierarchyModel;
		check(ObjectHierarchyModelAttribute.IsSet() || ObjectHierarchyModelAttribute.IsBound());
		
		ClientManager = &InClientManager;
		ConcertClient = MoveTemp(InConcertClient);
		
		ChildSlot
		[
			SNew(SHorizontalBox)
			
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SCheckBox)
				.IsChecked(this, &SReplicationMultiToggleCheckbox::GetCheckboxStateForThisAndChildren)
				.IsEnabled(this, &SReplicationMultiToggleCheckbox::CanToggleThisOrChildren)
				.OnCheckStateChanged(this, &SReplicationMultiToggleCheckbox::OnCheckboxStateChanged)
			]
			
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SComboButton)
				.HasDownArrow(true)
				.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
				.ForegroundColor(FSlateColor::UseStyle())
				.ButtonContent()
				[
					SNew(SScaleBox)
					.Stretch(EStretch::ScaleToFit)
					.Visibility(this, &SReplicationMultiToggleCheckbox::GetWarningVisibility)
					.ToolTipText(this, &SReplicationMultiToggleCheckbox::GetWarningToolTipText)
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.WarningWithColor"))
					]
				]
				.OnGetMenuContent(this, &SReplicationMultiToggleCheckbox::GetDropDownMenuContent)
			]
		];
	}

	FText SReplicationMultiToggleCheckbox::GetRootToolTipText() const
	{
		const bool bHasProperties = IsCheckboxEnabledForObject(Object);
		if (!bHasProperties)
		{
			// TODO UE-200496 Update text if we predict there to be a conflict
			return LOCTEXT("Toggle.ToolTip.NoProperties", "Assign properties first.");
		}
		
		switch (GetCheckboxStateForObject(Object))
		{
		case ECheckBoxState::Unchecked: return LOCTEXT("Toggle.ToolTip.Unchecked", "Not replicating.");
		case ECheckBoxState::Checked: return LOCTEXT("Toggle.ToolTip.Checked", "Replicating all assigned objects.");
		case ECheckBoxState::Undetermined: return LOCTEXT("Toggle.ToolTip.Undetermined", "Replicating some assigned objects, but not all.");
		default: return FText::GetEmpty();
		}
	}

	ECheckBoxState SReplicationMultiToggleCheckbox::GetCheckboxStateForObjects(TConstArrayView<FSoftObjectPath> InObjects) const
	{
		// The checkbox shows
		// - checked if all editable clients with the objects registered have authority
		// - unchecked if all editable clients with the objects registered do not have authority
		// - undetermined otherwise

		bool bHadAnyEditableClient = false;
		TOptional<ECheckBoxState> ConsolidatedState;
		for (const FSoftObjectPath& InObject : InObjects)
		{
			const TArray<FGuid> ClientsWithAuthority = ClientManager->GetAuthorityCache().GetClientsWithAuthorityOverObject(InObject);

			// TODO UE-200496 Predict conflicts
			ClientManager->GetAuthorityCache().ForEachClientWithObjectInStream(InObject, [this, &ClientsWithAuthority, &bHadAnyEditableClient, &ConsolidatedState](const FGuid& ClientId)
			{
				// The state of the checkbox always skips non-editable clients
				const FReplicationClient* Client = ClientManager->FindClient(ClientId);
				if (!ensure(Client) || !Client->AllowsEditing())
				{
					return EBreakBehavior::Continue;
				}
			
				bHadAnyEditableClient = true;
				const bool bHasAuthority = ClientsWithAuthority.Contains(Client->GetEndpointId());
				const ECheckBoxState ClientState = bHasAuthority ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			
				if (!ConsolidatedState.IsSet())
				{
					ConsolidatedState = ClientState;
				}
				else if (*ConsolidatedState != ClientState)
				{
					ConsolidatedState = ECheckBoxState::Undetermined;
					return EBreakBehavior::Break;
				}
			
				return EBreakBehavior::Continue;
			});
		}
		
		return bHadAnyEditableClient ? ConsolidatedState.Get(ECheckBoxState::Unchecked) : ECheckBoxState::Unchecked;
	}

	bool SReplicationMultiToggleCheckbox::IsCheckboxEnabledForObject(FSoftObjectPath InObject) const
	{
		// TODO UE-200496 Predict conflicts
		
		bool bHasAtLeastOneEditableClient = false;
		ClientManager->GetAuthorityCache().ForEachClientWithObjectInStream(InObject, [this, &bHasAtLeastOneEditableClient](const FGuid& ClientId)
		{
			const FReplicationClient* Client = ClientManager->FindClient(ClientId);
			bHasAtLeastOneEditableClient = ensure(Client) && Client->AllowsEditing();
			return bHasAtLeastOneEditableClient ? EBreakBehavior::Break : EBreakBehavior::Continue;
		});
		return bHasAtLeastOneEditableClient;
	}

	void SReplicationMultiToggleCheckbox::OnCheckboxStateChanged(ECheckBoxState NewState) const
	{
		const bool bShouldHaveAuthority = NewState == ECheckBoxState::Checked;
		
		SetAuthorityForObject(bShouldHaveAuthority, Object);
		ObjectHierarchyModelAttribute.Get()->ForEachChildRecursive(
			Object, 
		[this, bShouldHaveAuthority](const FSoftObjectPath&, const FSoftObjectPath& ChildObject, ConcertSharedSlate::EChildRelationship)
			{
				SetAuthorityForObject(bShouldHaveAuthority, ChildObject);
				return EBreakBehavior::Continue;
			});
	}

	TSharedRef<SWidget> SReplicationMultiToggleCheckbox::GetDropDownMenuContent()
	{
		FMenuBuilder MenuBuilder(true, nullptr);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("OnlyThis", "Toggle this only"),
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SReplicationMultiToggleCheckbox::ToggleThis),
				FCanExecuteAction::CreateSP(this, &SReplicationMultiToggleCheckbox::CanToggleThis)
				)
			);
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ChildrenOnly", "Toggle children only"),
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SReplicationMultiToggleCheckbox::ToggleChildren),
				FCanExecuteAction::CreateSP(this, &SReplicationMultiToggleCheckbox::CanToggleChildren)
				)
			);
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ThisAndChildren", "Toggle this & children"),
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SReplicationMultiToggleCheckbox::ToggleThisAndChildren),
				FCanExecuteAction::CreateSP(this, &SReplicationMultiToggleCheckbox::CanToggleThisOrChildren)
				)
			);
		
		return MenuBuilder.MakeWidget();
	}

	TArray<FSoftObjectPath> SReplicationMultiToggleCheckbox::GetThisAndChildren() const
	{
		TArray<FSoftObjectPath> AllObjects = GetChildObjects();
		AllObjects.Add(Object);
		return AllObjects;
	}

	TArray<FSoftObjectPath> SReplicationMultiToggleCheckbox::GetChildObjects() const
	{
		return ObjectHierarchyModelAttribute.Get()->GetChildrenRecursive(Object);
	}

	void SReplicationMultiToggleCheckbox::ToggleObjects(TConstArrayView<FSoftObjectPath> Objects) const
	{
		if (Objects.IsEmpty())
		{
			return;
		}
		
		ECheckBoxState ConsolidatedCheckboxState = GetCheckboxStateForObject(Objects[0]);
		for (int32 i = 1; i < Objects.Num(); ++i)
		{
			const ECheckBoxState ObjectState = GetCheckboxStateForObject(Objects[i]);
			if (ConsolidatedCheckboxState != ObjectState)
			{
				// If the states are different, pretend they are all Checked so below we default to toggling off
				ConsolidatedCheckboxState = ECheckBoxState::Checked;
				break;
			}
		}

		const ECheckBoxState StateToSet = ConsolidatedCheckboxState == ECheckBoxState::Unchecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		const bool bShouldHaveAuthority = StateToSet == ECheckBoxState::Checked;
		for (const FSoftObjectPath& ObjectPath : Objects)
		{
			SetAuthorityForObject(bShouldHaveAuthority, ObjectPath);
		}
	}

	bool SReplicationMultiToggleCheckbox::CanToggleObjects(TConstArrayView<FSoftObjectPath> Objects) const
	{
		return Algo::AnyOf(Objects, [this](const FSoftObjectPath& ObjectPath)
		{
			return IsCheckboxEnabledForObject(ObjectPath);
		});
	}

	void SReplicationMultiToggleCheckbox::SetAuthorityForObject(bool bShouldHaveAuthority, const FSoftObjectPath& InObject) const
	{
		ClientManager->GetAuthorityCache().ForEachClientWithObjectInStream(InObject, [this, InObject, bShouldHaveAuthority](const FGuid& ClientId)
		{
			FReplicationClient* Client = ClientManager->FindClient(ClientId);
			if (ensure(Client) && Client->AllowsEditing())
			{
				Client->GetAuthorityDiffer().SetAuthorityIfAllowed({ InObject }, bShouldHaveAuthority);
			}
			
			return EBreakBehavior::Continue;
		});
	}

	EVisibility SReplicationMultiToggleCheckbox::GetWarningVisibility() const
	{
		const bool bContainsNonEditableClients = !GetNonEditableClients().IsEmpty();
		return IsCheckboxEnabledForObject(Object) && bContainsNonEditableClients
			? EVisibility::Visible
			: EVisibility::Collapsed;
	}

	FText SReplicationMultiToggleCheckbox::GetWarningToolTipText() const
	{
		const TArray<FGuid> NonEditableClients = GetNonEditableClients();
		if (NonEditableClients.IsEmpty())
		{
			// Avoid text substitution below
			return FText::GetEmpty();
		}
		
		FString Clients = FString::JoinBy(NonEditableClients, TEXT(", "), [this](const FGuid& ClientEndpointId)
		{
			return ClientUtils::GetClientDisplayName(*ConcertClient, ClientEndpointId);
		});
		return FText::Format(
			LOCTEXT("WarningIcon.ToolTip", "Has clients that cannot be edited remotely.\nClients: {0}"),
			FText::FromString(MoveTemp(Clients))
			);
	}

	TArray<FGuid> SReplicationMultiToggleCheckbox::GetNonEditableClients() const
	{
		TArray<FGuid> Result;
		ClientManager->GetAuthorityCache().ForEachClientWithObjectInStream(Object, [this, &Result](const FGuid& ClientId)
		{
			// The warning icon cares only about editable clients
			const FReplicationClient* Client = ClientManager->FindClient(ClientId);
			if (ensure(Client) && !Client->AllowsEditing())
			{
				Result.Add(ClientId);
			}
			return EBreakBehavior::Continue;
		});
		return Result;
	}
}

#undef LOCTEXT_NAMESPACE