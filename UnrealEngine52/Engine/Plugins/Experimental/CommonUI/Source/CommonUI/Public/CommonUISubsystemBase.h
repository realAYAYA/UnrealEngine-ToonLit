// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/GameInstanceSubsystem.h"

#include "CommonUISubsystemBase.generated.h"

enum class ECommonInputType : uint8;
struct FDataTableRowHandle;
struct FSlateBrush;

class IAnalyticsProviderET;
class UWidget;
class ULocalPlayer;
class UInputAction;

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

	// Gets Action Button Icon for given action and player, enhanced input API currently does not allow input type specification
	UFUNCTION(BlueprintCallable, Category = CommonUISubsystem)
	FSlateBrush GetEnhancedInputActionButtonIcon(const UInputAction* InputAction, const ULocalPlayer* LocalPlayer) const;

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

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Engine/DataTable.h"
#include "Styling/SlateBrush.h"
#endif
