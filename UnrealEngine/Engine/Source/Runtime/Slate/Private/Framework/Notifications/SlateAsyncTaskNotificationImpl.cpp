// Copyright Epic Games, Inc. All Rights Reserved.

#include "Framework/Notifications/SlateAsyncTaskNotificationImpl.h"

#include "Widgets/Notifications/INotificationWidget.h"
#include "Framework/Notifications/NotificationManager.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"

#include "Misc/App.h"
#include "Misc/ScopeLock.h"
#include "Templates/Atomic.h"

#define LOCTEXT_NAMESPACE "SlateAsyncTaskNotification"

/*
 * FSlateAsyncTaskNotificationImpl
 */

FSlateAsyncTaskNotificationImpl::FSlateAsyncTaskNotificationImpl() : PromptAction(EAsyncTaskNotificationPromptAction::None)
{
	
}

FSlateAsyncTaskNotificationImpl::~FSlateAsyncTaskNotificationImpl()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
}

void FSlateAsyncTaskNotificationImpl::Initialize(const FAsyncTaskNotificationConfig& InConfig)
{
	NotificationConfig = InConfig;
	
	// Note: FCoreAsyncTaskNotificationImpl guarantees this is being called from the game thread

	// Initialize the UI if the Notification is not headless
	if (!NotificationConfig.bIsHeadless)
	{
		// Register the ticker to update the notification ever frame
		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &FSlateAsyncTaskNotificationImpl::TickNotification));

		// Register this as a Staged Notification (Allows notifications to remain open even after this is destroyed)
		FSlateNotificationManager::Get().RegisterStagedNotification(AsShared());

		PromptAction = FApp::IsUnattended() ? EAsyncTaskNotificationPromptAction::Unattended : EAsyncTaskNotificationPromptAction::None;
		bCanCancelAttr = InConfig.bCanCancel;
		bKeepOpenOnSuccessAttr = InConfig.bKeepOpenOnSuccess;
		bKeepOpenOnFailureAttr = InConfig.bKeepOpenOnFailure;
		SyncAttributes();

		// Mark the notification as pending so the UI can initialize
		PreviousCompletionState = EAsyncTaskNotificationState::None;
		SetPendingCompletionState(EAsyncTaskNotificationState::Pending); 
	}

	
	// This calls UpdateNotification to update the UI initialized above
	FCoreAsyncTaskNotificationImpl::Initialize(InConfig);
}

void FSlateAsyncTaskNotificationImpl::DestroyCurrentNotification()
{
	if(OwningNotification)
	{
		// Perform the normal automatic fadeout
		OwningNotification->ExpireAndFadeout();

		// Release our reference to our owner so that everything can be destroyed
		OwningNotification.Reset();
	}
}

void FSlateAsyncTaskNotificationImpl::CreateNewNotificationItem(EAsyncTaskNotificationState NewNotificationState)
{
	DestroyCurrentNotification();
	
	switch (NewNotificationState)
	{
	case EAsyncTaskNotificationState::Pending:
		CreatePendingNotification();
		break;
	case EAsyncTaskNotificationState::Failure:
		CreateFailureNotification();
		break;
	case EAsyncTaskNotificationState::Success:
		CreateSuccessNotification();
		break;
	case EAsyncTaskNotificationState::Prompt:
		CreatePromptNotification();
		break;
	}

}

void FSlateAsyncTaskNotificationImpl::SetOwner(TSharedPtr<SNotificationItem> InOwningNotification)
{
	OwningNotification = InOwningNotification;

	// Update the notification here to make sure it has the correct Text/Subtext/Hyperlink etc
	UpdateNotification();
}

TSharedPtr<SNotificationItem> FSlateAsyncTaskNotificationImpl::SetupNotificationItem(FNotificationInfo& NotificationInfo)
{
	NotificationInfo.FadeOutDuration = NotificationConfig.FadeOutDuration;
	NotificationInfo.ExpireDuration = NotificationConfig.ExpireDuration;
	NotificationInfo.FadeInDuration = NotificationConfig.FadeInDuration;
	NotificationInfo.bFireAndForget = false;

	TSharedPtr<SNotificationItem> NotificationItem = FSlateNotificationManager::Get().AddNotification(NotificationInfo);
	check(NotificationItem);
	
	SetOwner(NotificationItem);
	return NotificationItem;
}

void FSlateAsyncTaskNotificationImpl::CreatePendingNotification()
{
	FNotificationInfo NotificationInfo(FText::GetEmpty());

	// Pending Notifications have a throbber to show progress
	NotificationInfo.bUseThrobber = true;
	
	AddCancelButton(NotificationInfo, SNotificationItem::CS_Pending);
	
	TSharedPtr<SNotificationItem> NotificationItem = SetupNotificationItem(NotificationInfo);
}

void FSlateAsyncTaskNotificationImpl::CreateSuccessNotification()
{
	FNotificationInfo NotificationInfo(FText::GetEmpty());

	NotificationInfo.Image = FAppStyle::Get().GetBrush("NotificationList.SuccessImage");
	AddCloseButton(NotificationInfo);
	
	TSharedPtr<SNotificationItem> NotificationItem = SetupNotificationItem(NotificationInfo);
}

void FSlateAsyncTaskNotificationImpl::CreateFailureNotification()
{
	FNotificationInfo NotificationInfo(FText::GetEmpty());
	
	NotificationInfo.Image = FAppStyle::Get().GetBrush("NotificationList.FailImage");
	AddCloseButton(NotificationInfo);

	TSharedPtr<SNotificationItem> NotificationItem = SetupNotificationItem(NotificationInfo);
}

void FSlateAsyncTaskNotificationImpl::CreatePromptNotification()
{
	FNotificationInfo NotificationInfo(FText::GetEmpty());

	AddPromptButton(NotificationInfo);
	
	AddCancelButton(NotificationInfo, SNotificationItem::CS_None);
	
	TSharedPtr<SNotificationItem> NotificationItem = SetupNotificationItem(NotificationInfo);
}

void FSlateAsyncTaskNotificationImpl::AddPromptButton(FNotificationInfo &NotificationInfo)
{
	if(GetPromptButtonVisibility() == EVisibility::Visible)
	{
		FNotificationButtonInfo PromptButtonInfo(
		PromptText,
		FText::GetEmpty(),
		FSimpleDelegate::CreateSP(this, &FSlateAsyncTaskNotificationImpl::OnPromptButtonClicked),
		SNotificationItem::CS_None
		);
	
		NotificationInfo.ButtonDetails.Add(PromptButtonInfo);
	}
}


void FSlateAsyncTaskNotificationImpl::AddCancelButton(FNotificationInfo &NotificationInfo, SNotificationItem::ECompletionState VisibleInState)
{
	if(GetCancelButtonVisibility() == EVisibility::Visible)
	{
		FNotificationButtonInfo CancelButtonInfo(
		LOCTEXT("CancelButton", "Cancel"),
		FText::GetEmpty(),
		FSimpleDelegate::CreateSP(this, &FSlateAsyncTaskNotificationImpl::OnCancelButtonClicked),
		VisibleInState
		);
	
		NotificationInfo.ButtonDetails.Add(CancelButtonInfo);
	}
}

void FSlateAsyncTaskNotificationImpl::AddCloseButton(FNotificationInfo &NotificationInfo)
{
	if(GetCloseButtonVisibility() == EVisibility::Visible)
	{
		FNotificationButtonInfo CloseButtonInfo(
		LOCTEXT("CloseButton", "Close"),
		FText::GetEmpty(),
		FSimpleDelegate::CreateSP(this, &FSlateAsyncTaskNotificationImpl::OnCloseButtonClicked)
		);

		CloseButtonInfo.VisibilityOnSuccess = EVisibility::Visible;
		CloseButtonInfo.VisibilityOnFail = EVisibility::Visible;
		
		NotificationInfo.ButtonDetails.Add(CloseButtonInfo);
	}
}

void FSlateAsyncTaskNotificationImpl::SyncAttributes()
{
	FScopeLock Lock(&AttributesCS);

	bCanCancel = bCanCancelAttr.Get(false);
	bKeepOpenOnSuccess = bKeepOpenOnSuccessAttr.Get(false);
	bKeepOpenOnFailure = bKeepOpenOnFailureAttr.Get(false);
}

void FSlateAsyncTaskNotificationImpl::OnSetCompletionState(SNotificationItem::ECompletionState InState)
{
	check(InState == GetNotificationCompletionState());

	// If we completed and we aren't keeping the notification open (which will show the Close button), then expire the notification immediately
	if ((InState == SNotificationItem::CS_Success || InState == SNotificationItem::CS_Fail) && GetCloseButtonVisibility() == EVisibility::Collapsed)
	{
		DestroyCurrentNotification();

		FSlateNotificationManager::Get().UnregisterStagedNotification(AsShared());
	}

	// Reset the `PromptAction` state when changing completion state
	PromptAction = FApp::IsUnattended() ? EAsyncTaskNotificationPromptAction::Unattended : EAsyncTaskNotificationPromptAction::None;
}

void FSlateAsyncTaskNotificationImpl::SetPendingCompletionState(const EAsyncTaskNotificationState InPendingCompletionState)
{
	FScopeLock Lock(&CompletionCS);

	// Set the completion state
	PendingCompletionState = InPendingCompletionState;
}

void FSlateAsyncTaskNotificationImpl::SetCanCancel(const TAttribute<bool>& InCanCancel)
{
	if (!NotificationConfig.bIsHeadless)
	{
		FScopeLock Lock(&AttributesCS);

		bCanCancelAttr = InCanCancel;
	}
}

void FSlateAsyncTaskNotificationImpl::SetKeepOpenOnSuccess(const TAttribute<bool>& InKeepOpenOnSuccess)
{
	if (!NotificationConfig.bIsHeadless)
	{
		FScopeLock Lock(&AttributesCS);

		bKeepOpenOnSuccessAttr = InKeepOpenOnSuccess;
	}
}

void FSlateAsyncTaskNotificationImpl::SetKeepOpenOnFailure(const TAttribute<bool>& InKeepOpenOnFailure)
{
	if (!NotificationConfig.bIsHeadless)
	{
		FScopeLock Lock(&AttributesCS);

		bKeepOpenOnFailureAttr = InKeepOpenOnFailure;
	}
}

bool FSlateAsyncTaskNotificationImpl::IsCancelButtonEnabled() const
{
	return bCanCancel && PromptAction == EAsyncTaskNotificationPromptAction::None;
}

EVisibility FSlateAsyncTaskNotificationImpl::GetCancelButtonVisibility() const
{
	return (bCanCancel && (State == EAsyncTaskNotificationState::Pending || State == EAsyncTaskNotificationState::Prompt))
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

void FSlateAsyncTaskNotificationImpl::OnCancelButtonClicked()
{
	PromptAction = EAsyncTaskNotificationPromptAction::Cancel;
}

bool FSlateAsyncTaskNotificationImpl::IsPromptButtonEnabled() const
{
	return PromptAction == EAsyncTaskNotificationPromptAction::None;
}

EVisibility FSlateAsyncTaskNotificationImpl::GetPromptButtonVisibility() const
{
	return (!FApp::IsUnattended() && State == EAsyncTaskNotificationState::Prompt)
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

void FSlateAsyncTaskNotificationImpl::OnPromptButtonClicked()
{
	PromptAction = EAsyncTaskNotificationPromptAction::Continue;
}

FText FSlateAsyncTaskNotificationImpl::GetPromptButtonText() const
{
	return PromptText;
}

EVisibility FSlateAsyncTaskNotificationImpl::GetCloseButtonVisibility() const
{
	return (!FApp::IsUnattended() && ((bKeepOpenOnSuccess && State == EAsyncTaskNotificationState::Success) || (bKeepOpenOnFailure && State == EAsyncTaskNotificationState::Failure)))
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

void FSlateAsyncTaskNotificationImpl::OnCloseButtonClicked()
{
	if (OwningNotification)
	{
		// Expire the notification immediately and ensure it fades quickly so that clicking the buttons feels responsive
		OwningNotification->SetExpireDuration(0.0f);
		OwningNotification->SetFadeOutDuration(0.5f);
		OwningNotification->ExpireAndFadeout();

		// Release our reference to our owner so that everything can be destroyed
		OwningNotification.Reset();

		// Unregister the Staged Notification to complete the cleanup
		FSlateNotificationManager::Get().UnregisterStagedNotification(AsShared());
	}
}

void FSlateAsyncTaskNotificationImpl::OnHyperlinkClicked() const
{
	Hyperlink.ExecuteIfBound();
}

FText FSlateAsyncTaskNotificationImpl::GetHyperlinkText() const
{
	return HyperlinkText;
}

EVisibility FSlateAsyncTaskNotificationImpl::GetHyperlinkVisibility() const
{
	return Hyperlink.IsBound() ? EVisibility::Visible : EVisibility::Collapsed;
}

SNotificationItem::ECompletionState FSlateAsyncTaskNotificationImpl::GetNotificationCompletionState() const
{
	if (OwningNotification)
	{
		return OwningNotification->GetCompletionState();
	}
	return SNotificationItem::CS_None;
}

void FSlateAsyncTaskNotificationImpl::UpdateNotification()
{
	FCoreAsyncTaskNotificationImpl::UpdateNotification();
	
	if (!NotificationConfig.bIsHeadless)
	{
		// Update the notification UI only if the state hasn't changed (i.e this notification will not be deleted)
		if(OwningNotification && State == PreviousCompletionState)
		{
			/* Slate requries the notification to be updated from the main thread, so we add a one frame ticker for it */
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&FSlateAsyncTaskNotificationImpl::UpdateNotificationDeferred, OwningNotification, TitleText, ProgressText, Hyperlink, HyperlinkText));
		}

		// Set the Pending Completion State in case the notification has to change
		SetPendingCompletionState(State);
	}
}

bool FSlateAsyncTaskNotificationImpl::UpdateNotificationDeferred(float InDeltaTime, TSharedPtr<SNotificationItem> OwningNotification, FText TitleText, FText ProgressText, FSimpleDelegate Hyperlink, FText HyperlinkText)
{
	OwningNotification->SetText(TitleText);
	OwningNotification->SetSubText(ProgressText);
	OwningNotification->SetHyperlink(Hyperlink, HyperlinkText);

	// We only want this function to tick once
	return false;
}

EAsyncTaskNotificationPromptAction FSlateAsyncTaskNotificationImpl::GetPromptAction() const
{
	if(NotificationConfig.bIsHeadless)
	{
		return EAsyncTaskNotificationPromptAction::Unattended;
	}
	return PromptAction;
}

bool FSlateAsyncTaskNotificationImpl::TickNotification(float InDeltaTime)
{
	SyncAttributes();
	
	EAsyncTaskNotificationState CompletionStateToApply = EAsyncTaskNotificationState::None;
	{
		FScopeLock Lock(&CompletionCS);

		if (PendingCompletionState.IsSet())
		{
			CompletionStateToApply = PendingCompletionState.GetValue();
			PendingCompletionState.Reset();
		}
	}

	// Create a new notification if the state changed to a valid state
	if (PreviousCompletionState != CompletionStateToApply && CompletionStateToApply != EAsyncTaskNotificationState::None)
	{
		// Reset the State of the previous notification if it was 'Pending', to make any misleading buttons disappear
		if(OwningNotification && PreviousCompletionState == EAsyncTaskNotificationState::Pending)
		{
			OwningNotification->SetCompletionState(SNotificationItem::CS_None);
		}
		
		PreviousCompletionState = CompletionStateToApply;

		// Create a new notification based on the new state
		CreateNewNotificationItem(CompletionStateToApply);

		if(OwningNotification)
		{
			SNotificationItem::ECompletionState OwningCompletionState = SNotificationItem::CS_None;
			switch (CompletionStateToApply)
			{
			case EAsyncTaskNotificationState::Pending:
				OwningCompletionState = SNotificationItem::CS_Pending;
				break;
			case EAsyncTaskNotificationState::Failure:
				OwningCompletionState = SNotificationItem::CS_Fail;
				break;
			case EAsyncTaskNotificationState::Success:
				OwningCompletionState = SNotificationItem::CS_Success;
				break;
			case EAsyncTaskNotificationState::Prompt:
				OwningNotification->Pulse(FLinearColor(0.f, 0.f, 1.f));

				break;
			}
			if (OwningCompletionState != SNotificationItem::CS_None && OwningCompletionState != OwningNotification->GetCompletionState())
			{
				OwningNotification->SetCompletionState(OwningCompletionState);
				OnSetCompletionState(OwningCompletionState);

				// We don't need the ticker anymore if the notification is complete
				if(OwningCompletionState == SNotificationItem::CS_Success || OwningCompletionState == SNotificationItem::CS_Fail)
				{
					return false;
				}
			}
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
