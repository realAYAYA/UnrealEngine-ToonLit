//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "TickableNotification.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "LevelEditor.h"
#include "Misc/ScopeLock.h"

namespace SteamAudio
{
	FWorkItem::FWorkItem()
		: Task(nullptr)
		, FinalState(SNotificationItem::CS_Success)
		, bIsFinalItem(false)
	{}

	FWorkItem::FWorkItem(const TFunction<void(FText&)>& Task, const SNotificationItem::ECompletionState InFinalState, const bool bIsFinalItem)
		: Task(Task)
		, FinalState(InFinalState)
		, bIsFinalItem(bIsFinalItem)
	{}

	FTickableNotification::FTickableNotification()
		: bIsTicking(false)
	{
	}

	void FTickableNotification::CreateNotification()
	{
		FNotificationInfo Info(DisplayText);
		Info.bFireAndForget = false;
		Info.FadeOutDuration = 4.0f;
		Info.ExpireDuration = 0.0f;

		NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
		NotificationPtr.Pin()->SetCompletionState(SNotificationItem::CS_Pending);

		bIsTicking = true;
	}
	
	void FTickableNotification::CreateNotificationWithCancel(const FSimpleDelegate& CancelDelegate)
	{
		FNotificationInfo Info(DisplayText);
		Info.bFireAndForget = false;
		Info.FadeOutDuration = 4.0f;
		Info.ExpireDuration = 0.0f;
		Info.ButtonDetails.Add(FNotificationButtonInfo(NSLOCTEXT("SteamAudio", "Cancel", "Cancel"), FText::GetEmpty(), CancelDelegate));

		NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
		NotificationPtr.Pin()->SetCompletionState(SNotificationItem::CS_Pending);

		bIsTicking = true;
	}

	void FTickableNotification::DestroyNotification(const SNotificationItem::ECompletionState InFinalState)
	{
		bIsTicking = false;
		this->FinalState = InFinalState;
	}

	void FTickableNotification::SetDisplayText(const FText& InDisplayText)
	{
		FScopeLock Lock(&CriticalSection);

		this->DisplayText = InDisplayText;
	}

	void FTickableNotification::QueueWorkItem(const FWorkItem& WorkItem)
	{
		FScopeLock Lock(&CriticalSection);

		WorkQueue.Enqueue(WorkItem);
	}

	void FTickableNotification::NotifyDestruction()
	{
		if (!NotificationPtr.Pin().IsValid())
		{
			return;
		}

		NotificationPtr.Pin()->SetText(DisplayText);
		NotificationPtr.Pin()->SetCompletionState(FinalState);
		NotificationPtr.Pin()->ExpireAndFadeout();
		NotificationPtr.Reset();
	}

	void FTickableNotification::Tick(float DeltaTime)
	{
		FScopeLock Lock(&CriticalSection);

		if (bIsTicking && NotificationPtr.Pin().IsValid())
		{
			if (!WorkQueue.IsEmpty())
			{
				FWorkItem WorkItem;
				WorkQueue.Dequeue(WorkItem);
				WorkItem.Task(DisplayText);
				FinalState = WorkItem.FinalState;
				bIsTicking = !WorkItem.bIsFinalItem;
			}

			NotificationPtr.Pin()->SetText(DisplayText);
		}
		else
		{
			NotifyDestruction();
		}
	}

	TStatId FTickableNotification::GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FTickableNotification, STATGROUP_Tickables);
	}
}
