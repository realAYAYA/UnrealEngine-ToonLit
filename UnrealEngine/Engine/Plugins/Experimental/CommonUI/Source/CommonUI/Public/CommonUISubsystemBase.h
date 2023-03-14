// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/DataTable.h"
#include "Styling/SlateBrush.h"

#include "CommonUISubsystemBase.generated.h"

class IAnalyticsProviderET;
class UWidget;
class ULocalPlayer;

UCLASS(DisplayName = "CommonUI")
class COMMONUI_API UCommonUISubsystemBase : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UCommonUISubsystemBase* Get(const UWidget& Widget);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** set the analytic provider for the CommonUI Widgets to use */
	void SetAnalyticProvider(const TSharedPtr<IAnalyticsProviderET>& AnalyticProvider);

	// Gets Action Button Icon for current gamepad
	UFUNCTION(BlueprintCallable, Category = CommonUISubsystem)
	FSlateBrush GetInputActionButtonIcon(const FDataTableRowHandle& InputActionRowHandle, ECommonInputType InputType, const FName& GamepadName) const;

	/** Analytic Events **/

	//CommonUI.ButtonClicked
	void FireEvent_ButtonClicked(const FString& InstanceName, const FString& ABTestName, const FString& ExtraData) const;

	//CommonUI.PanelPushed
	void FireEvent_PanelPushed(const FString& PanelName) const;
	
	virtual void SetInputAllowed(bool bEnabled, const FName& Reason, const ULocalPlayer& LocalPlayer);
	virtual bool IsInputAllowed(const ULocalPlayer* LocalPlayer) const;

private:

	void HandleInputMethodChanged(ECommonInputType bNewInputType);

	TWeakPtr<class IAnalyticsProviderET> AnalyticProviderWeakPtr;
};