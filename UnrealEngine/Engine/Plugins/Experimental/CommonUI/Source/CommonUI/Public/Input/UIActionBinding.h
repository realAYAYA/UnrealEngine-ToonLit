// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Input/CommonUIInputSettings.h"

class FActionRouterBindingCollection;
struct FBindUIActionArgs;
struct FCommonInputActionDataBase;

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

	// @TODO: DarenC - Remove legacy.
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

	// @TODO: DarenC - Remove legacy.
	FDataTableRowHandle LegacyActionTableRow;

private:
	FUIActionBinding(const UWidget& InBoundWidget, const FBindUIActionArgs& BindArgs);
	
	double HoldStartTime = -1.0;

	static int32 IdCounter;
	static TMap<FUIActionBindingHandle, TSharedPtr<FUIActionBinding>> AllRegistrationsByHandle;
	
	// All keys currently being tracked for a hold action
	static TMap<FKey, FUIActionBindingHandle> CurrentHoldActionKeys;

	friend struct FUIActionBindingHandle;
};