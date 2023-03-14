// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommonLazyWidget.h"
#include "CommonActivatableWidget.h"
#include "CommonUISettings.h"
#include "CommonWidgetPaletteCategories.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonLazyWidget)

UCommonLazyWidget::UCommonLazyWidget(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	LoadingBackgroundBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	SetVisibilityInternal(ESlateVisibility::SelfHitTestInvisible);
}

TSharedRef<SWidget> UCommonLazyWidget::RebuildWidget()
{
	MyLoadGuard = SNew(SLoadGuard)
		.GuardBackgroundBrush(&LoadingBackgroundBrush)
		.OnLoadingStateChanged_UObject(this, &ThisClass::HandleLoadGuardStateChanged)
		[
			Content ? Content->TakeWidget() : SNullWidget::NullWidget
		];

	return MyLoadGuard.ToSharedRef();
}

void UCommonLazyWidget::OnWidgetRebuilt()
{
	Super::OnWidgetRebuilt();

	if (StreamingHandle.IsValid() && StreamingHandle->IsLoadingInProgress())
	{
		MyLoadGuard->SetForceShowSpinner(true);
	}
}

void UCommonLazyWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();

#if WITH_EDITOR
	if (IsDesignTime() && MyLoadGuard.IsValid())
	{
		MyLoadGuard->SetForceShowSpinner(true);
	}
#endif
}

void UCommonLazyWidget::SetForceShowSpinner(bool bShowLoading)
{
	if (MyLoadGuard.IsValid())
	{
		MyLoadGuard->SetForceShowSpinner(bShowLoading);
	}
}

void UCommonLazyWidget::CancelStreaming()
{
	if (MyLoadGuard.IsValid())
	{
		MyLoadGuard->SetForceShowSpinner(false);
	}
}

void UCommonLazyWidget::OnStreamingStarted(TSoftClassPtr<UObject> SoftObject)
{
	if (MyLoadGuard.IsValid())
	{
		MyLoadGuard->SetForceShowSpinner(true);
	}
}

void UCommonLazyWidget::OnStreamingComplete(TSoftClassPtr<UObject> LoadedSoftObject)
{
	if (MyLoadGuard.IsValid())
	{
		MyLoadGuard->SetForceShowSpinner(false);
	}
}

bool UCommonLazyWidget::IsLoading() const
{
	return MyLoadGuard.IsValid() && MyLoadGuard->IsLoading();
}

void UCommonLazyWidget::SetLazyContent(const TSoftClassPtr<UUserWidget> SoftWidget)
{
	if (SoftWidget.IsNull())
	{
		CancelStreaming();
		SetLoadedContent(nullptr);
	}

	TWeakObjectPtr<UCommonLazyWidget> WeakThis(this);

	RequestAsyncLoad(SoftWidget,
		[WeakThis, SoftWidget]() {
			if (ThisClass* StrongThis = WeakThis.Get())
			{
				if (ensureMsgf(SoftWidget.Get(), TEXT("Failed to load %s"), *SoftWidget.ToSoftObjectPath().ToString()))
				{
					// Don't reload the class if we're already this class.
					if (StrongThis->Content && StrongThis->Content->GetClass() == SoftWidget.Get())
					{
						StrongThis->OnContentChangedEvent.Broadcast(StrongThis->Content);
						return;
					}

					UUserWidget* UserWidget = CreateWidget(StrongThis->GetOwningPlayer(), SoftWidget.Get());
					StrongThis->SetLoadedContent(UserWidget);
				}
			}
		}
	);
}

void UCommonLazyWidget::SetLoadedContent(UUserWidget* InContent)
{
	if (UCommonActivatableWidget* OutgoingActivatable = Cast<UCommonActivatableWidget>(Content))
	{
		OutgoingActivatable->DeactivateWidget();
	}

	Content = InContent;

	if (MyLoadGuard.IsValid())
	{
		MyLoadGuard->SetContent(InContent ? InContent->TakeWidget() : SNullWidget::NullWidget);
	}

	if (UCommonActivatableWidget* IncomingActivatable = Cast<UCommonActivatableWidget>(Content))
	{
		IncomingActivatable->ActivateWidget();
	}

	OnContentChangedEvent.Broadcast(Content);
}

void UCommonLazyWidget::RequestAsyncLoad(TSoftClassPtr<UObject> SoftObject, TFunction<void()>&& Callback)
{
	RequestAsyncLoad(SoftObject, FStreamableDelegate::CreateLambda(MoveTemp(Callback)));
}

void UCommonLazyWidget::RequestAsyncLoad(TSoftClassPtr<UObject> SoftObject, FStreamableDelegate DelegateToCall)
{
	CancelStreaming();

	if (UObject* StrongObject = SoftObject.Get())
	{
		DelegateToCall.ExecuteIfBound();
		return;  // No streaming was needed, complete immediately.
	}

	OnStreamingStarted(SoftObject);

	TWeakObjectPtr<UCommonLazyWidget> WeakThis(this);
	StreamingObjectPath = SoftObject.ToSoftObjectPath();
	StreamingHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		StreamingObjectPath,
		[WeakThis, DelegateToCall, SoftObject]() {
			if (ThisClass* StrongThis = WeakThis.Get())
			{
				// If the object paths don't match, then this delegate was interrupted, but had already been queued for a callback
				// so ignore everything and abort.
				if (StrongThis->StreamingObjectPath != SoftObject.ToSoftObjectPath())
				{
					return; // Abort!
				}

				// Call the delegate to do whatever is needed, probably set the new image.
				DelegateToCall.ExecuteIfBound();

				// Note that the streaming has completed.
				StrongThis->OnStreamingComplete(SoftObject);
			}
		},
		FStreamableManager::AsyncLoadHighPriority);
}

#if WITH_EDITOR
const FText UCommonLazyWidget::GetPaletteCategory()
{
	return CommonWidgetPaletteCategories::Default;
}
#endif

void UCommonLazyWidget::HandleLoadGuardStateChanged(bool bIsLoading)
{
	OnLoadingStateChangedEvent.Broadcast(bIsLoading);
	BP_OnLoadingStateChanged.Broadcast(bIsLoading);
}

void UCommonLazyWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyLoadGuard.Reset();
}
