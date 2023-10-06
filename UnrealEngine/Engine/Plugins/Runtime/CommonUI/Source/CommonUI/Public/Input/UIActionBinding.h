// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Ticker.h"
#include "Engine/DataTable.h"
#include "Input/CommonUIInputSettings.h" // IWYU pragma: keep
#include "Input/UIActionBindingHandle.h"

enum EInputEvent : int;
struct FKey;
struct FUIActionKeyMapping;

class FActionRouterBindingCollection;
struct FBindUIActionArgs;
struct FCommonInputActionDataBase;
class UInputAction;

enum class EProcessHoldActionResult
{
	Handled,
	GeneratePress,
	Unhandled
};

struct COMMONUI_API FUIActionBinding
{
	FUIActionBinding() = delete;
	FUIActionBinding(const FUIActionBinding&) = delete;
	FUIActionBinding(FUIActionBinding&&) = delete;

	static FUIActionBindingHandle TryCreate(const UWidget& InBoundWidget, const FBindUIActionArgs& BindArgs);
	
	static TSharedPtr<FUIActionBinding> FindBinding(FUIActionBindingHandle Handle);
	static void CleanRegistrations();

	bool operator==(const FUIActionBindingHandle& OtherHandle) const { return Handle == OtherHandle; }

	// @TODO: Rename non-legacy in 5.3. We no longer have any active plans to remove data tables in CommonUI.
	FCommonInputActionDataBase* GetLegacyInputActionData() const;

	EProcessHoldActionResult ProcessHoldInput(ECommonInputMode ActiveInputMode, FKey Key, EInputEvent InputEvent);
	bool ProcessNormalInput(ECommonInputMode ActiveInputMode, FKey Key, EInputEvent InputEvent);
	FString ToDebugString() const;

	void BeginHold();
	bool UpdateHold(float TargetHoldTime);
	void CancelHold();
	void BeginRollback(float TargetHoldRollbackTime, float HoldTime, FUIActionBindingHandle BindingHandle);
	double GetSecondsHeld() const;
	bool IsHoldActive() const;
	void ResetHold();

	FName ActionName;
	EInputEvent InputEvent;
	bool bConsumesInput = true;
	bool bIsPersistent = false;
	
	TWeakObjectPtr<const UWidget> BoundWidget;
	ECommonInputMode InputMode;

	bool bDisplayInActionBar = false;
	FText ActionDisplayName;
	
	TWeakPtr<FActionRouterBindingCollection> OwningCollection;
	FSimpleDelegate OnExecuteAction;
	FUIActionBindingHandle Handle;

	TArray<FUIActionKeyMapping> NormalMappings;
	TArray<FUIActionKeyMapping> HoldMappings;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnHoldActionProgressedMulticast, float);
	FOnHoldActionProgressedMulticast OnHoldActionProgressed;

	DECLARE_MULTICAST_DELEGATE(FOnHoldActionPressed);
	FOnHoldActionPressed OnHoldActionPressed;

	DECLARE_MULTICAST_DELEGATE(FOnHoldActionReleased);
	FOnHoldActionPressed OnHoldActionReleased;

	// @TODO: Rename non-legacy in 5.3. We no longer have any active plans to remove data tables in CommonUI.
	FDataTableRowHandle LegacyActionTableRow;

	TWeakObjectPtr<const UInputAction> InputAction;

private:
	FUIActionBinding(const UWidget& InBoundWidget, const FBindUIActionArgs& BindArgs);
	
	// At what time in seconds did the hold start?
	double HoldStartTime = -1.0;
    	
	// At what second will the hold start?
	double HoldStartSecond = 0.0;
	
	// At what second is the hold progress at?
	double CurrentHoldSecond = 0.0;
	
	// Multiplier for the time (in seconds) for hold progress to go from 1.0 (completed) to 0.0.
    double HoldRollbackMultiplier = 1.0;
	
	// Target time (in seconds) for the hold progress to go from 0.0 to 1.0 (completed).
	double HoldTime = 0.0;
    	
	// Handle for ticker spawned for button hold rollback
	FTSTicker::FDelegateHandle HoldProgressRollbackTickerHandle;

	static int32 IdCounter;
	static TMap<FUIActionBindingHandle, TSharedPtr<FUIActionBinding>> AllRegistrationsByHandle;
	
	// All keys currently being tracked for a hold action
	static TMap<FKey, FUIActionBindingHandle> CurrentHoldActionKeys;

	friend struct FUIActionBindingHandle;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#endif