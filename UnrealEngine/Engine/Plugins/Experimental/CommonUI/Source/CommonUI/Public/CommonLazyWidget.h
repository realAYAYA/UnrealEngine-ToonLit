// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonLoadGuard.h"
#include "Components/Widget.h"

#include "CommonLazyWidget.generated.h"

class UCommonMcpItemDefinition;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnLazyContentChangedEvent, UUserWidget*);

/**
 * A special Image widget that can show unloaded images and takes care of the loading for you!
 */
UCLASS()
class COMMONUI_API UCommonLazyWidget : public UWidget
{
	GENERATED_UCLASS_BODY()

public:
	/**  */
	UFUNCTION(BlueprintCallable, Category = LazyContent)
	void SetLazyContent(const TSoftClassPtr<UUserWidget> SoftWidget);

	/**  */
	UFUNCTION(BlueprintCallable, Category = LazyContent)
	UUserWidget* GetContent() const { return Content; }

	UFUNCTION(BlueprintCallable, Category = LazyContent)
	bool IsLoading() const;

	FOnLazyContentChangedEvent& OnContentChanged() { return OnContentChangedEvent; }
	FOnLoadGuardStateChangedEvent& OnLoadingStateChanged() { return OnLoadingStateChangedEvent; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void OnWidgetRebuilt() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void SynchronizeProperties() override;

	void SetForceShowSpinner(bool bShowLoading);

	void CancelStreaming();
	void OnStreamingStarted(TSoftClassPtr<UObject> SoftObject);
	void OnStreamingComplete(TSoftClassPtr<UObject> LoadedSoftObject);

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif	

private:
	void SetLoadedContent(UUserWidget* InContent);
	void RequestAsyncLoad(TSoftClassPtr<UObject> SoftObject, TFunction<void()>&& Callback);
	void RequestAsyncLoad(TSoftClassPtr<UObject> SoftObject, FStreamableDelegate DelegateToCall);
	void HandleLoadGuardStateChanged(bool bIsLoading);

	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush LoadingBackgroundBrush;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> Content;

	TSharedPtr<FStreamableHandle> StreamingHandle;
	FSoftObjectPath StreamingObjectPath;

	UPROPERTY(BlueprintAssignable, Category = LazyImage, meta = (DisplayName = "On Loading State Changed", ScriptName = "OnLoadingStateChanged"))
	FOnLoadGuardStateChangedDynamic BP_OnLoadingStateChanged;

	TSharedPtr<SLoadGuard> MyLoadGuard;
	FOnLoadGuardStateChangedEvent OnLoadingStateChangedEvent;

	FOnLazyContentChangedEvent OnContentChangedEvent;
};