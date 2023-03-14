// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonButtonBase.h"
#include "Components/DynamicEntryBoxBase.h"
#include "CommonBoundActionBar.generated.h"

class IWidgetCompilerLog;
class IConsoleVariable;

/**
 * A box populated with current actions available per CommonUI's Input Handler.
 */
UCLASS(Blueprintable, ClassGroup = UI, meta = (Category = "Common UI"))
class COMMONUI_API UCommonBoundActionBar : public UDynamicEntryBoxBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = CommonBoundActionBar)
	void SetDisplayOwningPlayerActionsOnly(bool bShouldOnlyDisplayOwningPlayerActions);

protected:
	virtual void OnWidgetRebuilt() override;
	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	void ValidateCompiledDefaults(IWidgetCompilerLog& CompileLog) const override;
#endif

private:
	void HandleBoundActionsUpdated(bool bFromOwningPlayer);
	void HandleDeferredDisplayUpdate();
	void HandlePlayerAdded(int32 PlayerIdx);
	
	void MonitorPlayerActions(const ULocalPlayer* NewPlayer);

	UPROPERTY(EditAnywhere, Category = EntryLayout, meta=(MustImplement = "/Script/CommonUI.CommonBoundActionButtonInterface"))
	TSubclassOf<UCommonButtonBase> ActionButtonClass;

	UPROPERTY(EditAnywhere, Category = Display)
	bool bDisplayOwningPlayerActionsOnly = true;

	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Display)
	bool bIgnoreDuplicateActions = true;

	bool bIsRefreshQueued = false;
};