// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonButtonBase.h"
#include "Components/DynamicEntryBoxBase.h"
#include "Tickable.h"
#include "CommonBoundActionBar.generated.h"

class ICommonBoundActionButtonInterface;
class IConsoleVariable;
class IWidgetCompilerLog;

struct FUIActionBindingHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FActionBarUpdated);

/**
 * A box populated with current actions available per CommonUI's Input Handler.
 */
UCLASS(Blueprintable, ClassGroup = UI, meta = (Category = "Common UI"))
class COMMONUI_API UCommonBoundActionBar : public UDynamicEntryBoxBase, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = CommonBoundActionBar)
	void SetDisplayOwningPlayerActionsOnly(bool bShouldOnlyDisplayOwningPlayerActions);

	//~ FTickableGameObject Begin
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override;
	//~ FTickableGameObject End

protected:
	virtual void OnWidgetRebuilt() override;
	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	virtual void NativeOnActionButtonCreated(ICommonBoundActionButtonInterface* ActionButton, const FUIActionBindingHandle& RepresentedAction) { }

	virtual void ActionBarUpdateBeginImpl() {}
	virtual void ActionBarUpdateEndImpl() {}
	
	virtual UUserWidget* CreateActionButton(const FUIActionBindingHandle& BindingHandle);

	TSubclassOf<UCommonButtonBase> GetActionButtonClass() { return ActionButtonClass; }

#if WITH_EDITOR
	void ValidateCompiledDefaults(IWidgetCompilerLog& CompileLog) const override;
#endif

private:
	void HandleBoundActionsUpdated(bool bFromOwningPlayer);
	void HandleDeferredDisplayUpdate();
	void HandlePlayerAdded(int32 PlayerIdx);
	
	void MonitorPlayerActions(const ULocalPlayer* NewPlayer);

	void ActionBarUpdateBegin();
	void ActionBarUpdateEnd();

	UPROPERTY(EditAnywhere, Category = EntryLayout, meta=(MustImplement = "/Script/CommonUI.CommonBoundActionButtonInterface"))
	TSubclassOf<UCommonButtonBase> ActionButtonClass;

	UPROPERTY(EditAnywhere, Category = Display)
	bool bDisplayOwningPlayerActionsOnly = true;

	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Display)
	bool bIgnoreDuplicateActions = true;

	UPROPERTY(BlueprintAssignable, Category = "Events", meta = (AllowPrivateAccess = true))
	FActionBarUpdated OnActionBarUpdated;

	bool bIsRefreshQueued = false;
};