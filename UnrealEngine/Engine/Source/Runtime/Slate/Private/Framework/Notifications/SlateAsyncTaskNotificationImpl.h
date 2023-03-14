// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/CoreAsyncTaskNotificationImpl.h"
#include "Containers/Ticker.h"
#include "Layout/Visibility.h"
#include "Widgets/Notifications/SNotificationList.h"

class SSlateAsyncTaskNotificationWidget;

/**
 * Slate asynchronous task notification that uses a notification item.
 */
class FSlateAsyncTaskNotificationImpl : public TSharedFromThis<FSlateAsyncTaskNotificationImpl, ESPMode::ThreadSafe>,  public FCoreAsyncTaskNotificationImpl
{
public:
	//~ IAsyncTaskNotificationImpl
	virtual void Initialize(const FAsyncTaskNotificationConfig& InConfig) override;
	virtual void SetCanCancel(const TAttribute<bool>& InCanCancel) override;
	virtual void SetKeepOpenOnSuccess(const TAttribute<bool>& InKeepOpenOnSuccess) override;
	virtual void SetKeepOpenOnFailure(const TAttribute<bool>& InKeepOpenOnFailure) override;
	virtual EAsyncTaskNotificationPromptAction GetPromptAction() const override;

	FSlateAsyncTaskNotificationImpl();
	virtual ~FSlateAsyncTaskNotificationImpl() override;
	
private:
	//~ FCoreAsyncTaskNotificationImpl
	virtual void UpdateNotification() override;

	/** Run every frame on the game thread to update the notification */
	bool TickNotification(float InDeltaTime);

	/** Set the notification item that owns this widget */
	void SetOwner(TSharedPtr<SNotificationItem> InOwningNotification);

	/** Sync attribute bindings with the cached values (once per-frame from the game thread) */
	void SyncAttributes();

	/** Set the pending completion state of the notification (applied during the next Tick) */
	void SetPendingCompletionState(const EAsyncTaskNotificationState InPendingCompletionState);

	virtual void OnSetCompletionState(SNotificationItem::ECompletionState State);

	/** Cancel button */
	bool IsCancelButtonEnabled() const;
	EVisibility GetCancelButtonVisibility() const;
	void OnCancelButtonClicked();

	/** Prompt button */
	bool IsPromptButtonEnabled() const;
	EVisibility GetPromptButtonVisibility() const;
	void OnPromptButtonClicked();
	FText GetPromptButtonText() const;

	/** Close button */
	EVisibility GetCloseButtonVisibility() const;
	void OnCloseButtonClicked();

	/** Hyperlink */
	void OnHyperlinkClicked() const;
	FText GetHyperlinkText() const;
	EVisibility GetHyperlinkVisibility() const;

	/** Get the current completion state from the parent notification */
	SNotificationItem::ECompletionState GetNotificationCompletionState() const;

	/* Create a new notification item when the state changes */
	void CreateNewNotificationItem(EAsyncTaskNotificationState NewNotificationState);

	/* Create a notification for the Pending state */
	void CreatePendingNotification();

	/* Create a notification for the Success state */
	void CreateSuccessNotification();

	/* Create a notification for the Failure state */
	void CreateFailureNotification();

	/* Create a notification for the Prompt state */
	void CreatePromptNotification();

	/* Helper function to add the "Cancel" button to a notification */
	void AddCancelButton(FNotificationInfo &NotificationInfo, SNotificationItem::ECompletionState VisibleInState);

	/* Helper function to add the "Close" button to a notification */
	void AddCloseButton(FNotificationInfo &NotificationInfo);

	/* Helper function to add a Prompt button to a notification */
	void AddPromptButton(FNotificationInfo &NotificationInfo);

	/* Helper function to setup the information common to notifications in all states */
	TSharedPtr<SNotificationItem> SetupNotificationItem(FNotificationInfo& NotificationInfo);

	/* Cleanly destroy the current notification */
	void DestroyCurrentNotification();

	/* Static function to update the notification from the main thread */
	static bool UpdateNotificationDeferred(float InDeltaTime, TSharedPtr<SNotificationItem> OwningNotification, FText TitleText, FText ProgressText, FSimpleDelegate Hyperlink, FText HyperlinkText);

private:

	/** The Config used for all notifications */
	FAsyncTaskNotificationConfig NotificationConfig;

	/** Handle for TickNotification() */
	FTSTicker::FDelegateHandle TickerHandle;

	/** Action taken for the task, resets to none on notification state change. */
	TAtomic<EAsyncTaskNotificationPromptAction> PromptAction;

	/** Can this task be canceled? Will show a cancel button for in-progress tasks */
	TAttribute<bool> bCanCancelAttr = false;
	bool bCanCancel = false;

	/** Keep this notification open on success? Will show a close button */
	TAttribute<bool> bKeepOpenOnSuccessAttr = false;
	bool bKeepOpenOnSuccess = false;

	/** Keep this notification open on failure? Will show an close button */
	TAttribute<bool> bKeepOpenOnFailureAttr = false;
	bool bKeepOpenOnFailure = false;

	/** The pending completion state of the notification (if any, applied during the next Tick) */
	TOptional<EAsyncTaskNotificationState> PendingCompletionState;

	EAsyncTaskNotificationState PreviousCompletionState;

	/** Pointer to the notification item that owns this widget (this is a deliberate reference cycle as we need this object alive until we choose to expire it, at which point we release our reference to allow everything to be destroyed) */
	TSharedPtr<SNotificationItem> OwningNotification;

	/** Critical section preventing concurrent access to the attributes */
	FCriticalSection AttributesCS;

	/** Critical section preventing the game thread from completing this widget while another thread is in the progress of setting the completion state and cleaning up its UI references */
	FCriticalSection CompletionCS;
};
