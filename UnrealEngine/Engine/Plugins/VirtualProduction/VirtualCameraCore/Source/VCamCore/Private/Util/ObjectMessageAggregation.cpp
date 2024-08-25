// Copyright Epic Games, Inc. All Rights Reserved.

#include "ObjectMessageAggregation.h"

#include "LogVCamCore.h"
#include "Engine/World.h"

#if WITH_EDITOR
#include "CoreGlobals.h"
#include "Editor.h"
#include "Framework/Notifications/NotificationManager.h"
#include "UObject/UObjectAnnotation.h"
#include "Widgets/Notifications/SNotificationList.h"
#endif

namespace UE::VCamCore
{
	const FName NotificationKey_MissingTargetViewport(TEXT("NotificationKey_MissingTargetViewport"));

	namespace Private
	{
#if WITH_EDITOR
		static void ShowObjectNotification(TArray<FAggregatedNotification, TInlineAllocator<4>>&& Notifications)
		{
			TMap<FName, TPair<FText, TArray<FText>>> Aggregation;
			for (FAggregatedNotification& Notification : Notifications)
			{
				TPair<FText, TArray<FText>>& Data = Aggregation.FindOrAdd(Notification.Identifier);
				const bool bWasJustAdded = Data.Value.IsEmpty();
				if (bWasJustAdded)
				{
					Data.Key = MoveTemp(Notification.Title);
				}
				Data.Value.Add(MoveTemp(Notification.Subtext));
			}

			for (const TPair<FName, TPair<FText, TArray<FText>>>& Data : Aggregation)
			{
				const FText& Title = Data.Value.Key;
				const TArray<FText>& Subtexts = Data.Value.Value;
					
				FNotificationInfo Notification(Title);
				Notification.SubText = FText::Join(FText::FromString(TEXT("\n")), Subtexts);
				Notification.bFireAndForget = true;
				Notification.ExpireDuration = 6.f;
				FSlateNotificationManager::Get().AddNotification(Notification)
					->SetCompletionState(SNotificationItem::CS_Fail);
			}
		}
#endif
	}
	
	void AddAggregatedNotification(UObject& ContextObject, FAggregatedNotification Notification)
	{
		UE_LOG(LogVCamCore, Warning, TEXT("%s: %s - %s"), *ContextObject.GetPathName(), *Notification.Title.ToString(), *Notification.Subtext.ToString());
		
#if WITH_EDITOR
		// Happens e.g. with -game command line.
		// In this case FSlateNotificationManager::AddNotification is pointless. Moreover, GEditor is needed for setting the next tick timer.
		if (!GIsEditor || !GEditor)
		{
			return;
		}
		
		struct FNotificationAnnotation
		{
			TArray<FAggregatedNotification, TInlineAllocator<4>> Notifications;
			FTimerHandle TimerHandle;

			// Needed by FUObjectAnnotationSparse
			bool IsDefault() const { return !TimerHandle.IsValid(); }
		};
		static FUObjectAnnotationSparse<FNotificationAnnotation, true> Annotations;
		
		FNotificationAnnotation Annotation = Annotations.GetAndRemoveAnnotation(&ContextObject);
		Annotation.Notifications.Emplace(MoveTemp(Notification));
		if (!Annotation.TimerHandle.IsValid())
		{
			Annotation.TimerHandle = GEditor->GetTimerManager()->SetTimerForNextTick([ContextObjectPtr = &ContextObject]()
			{
				FNotificationAnnotation Annotation = Annotations.GetAndRemoveAnnotation(ContextObjectPtr);
				Private::ShowObjectNotification(MoveTemp(Annotation.Notifications));
			});
		}
		Annotations.AddAnnotation(&ContextObject, MoveTemp(Annotation));
#endif
	}
}
