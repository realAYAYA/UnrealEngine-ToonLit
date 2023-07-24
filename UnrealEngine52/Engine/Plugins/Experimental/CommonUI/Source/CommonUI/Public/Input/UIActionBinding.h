// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

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
	double GetSecondsHeld() const;
	bool IsHoldActive() const;

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

	// @TODO: Rename non-legacy in 5.3. We no longer have any active plans to remove data tables in CommonUI.
	FDataTableRowHandle LegacyActionTableRow;

	TWeakObjectPtr<const UInputAction> InputAction;

private:
	FUIActionBinding(const UWidget& InBoundWidget, const FBindUIActionArgs& BindArgs);
	
	double HoldStartTime = -1.0;

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
