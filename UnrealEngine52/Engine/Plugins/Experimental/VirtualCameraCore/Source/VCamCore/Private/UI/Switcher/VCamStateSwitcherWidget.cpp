// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Switcher/VCamStateSwitcherWidget.h"

#include "UI/Switcher/WidgetConnectionConfig.h"

DEFINE_LOG_CATEGORY(LogVCamStateSwitcher);

FName UVCamStateSwitcherWidget::DefaultState("Default");

bool UVCamStateSwitcherWidget::SetCurrentState(FName NewState, bool bForceUpdate, bool bReinitializeConnections)
{
	if (NewState == CurrentState && !bForceUpdate)
	{
		return false;
	}
	
	const FVCamWidgetConnectionState* StateConfig = States.Find(NewState);
	if (!StateConfig)
	{
		UE_LOG(LogVCamStateSwitcher, Warning, TEXT("Unknown connection state %s"), *NewState.ToString());
		return false;
	}

	const FName OldState = CurrentState;
	OnPreStateChanged.Broadcast(this, OldState, NewState);
	
	for (int32 i = 0; i < StateConfig->WidgetConfigs.Num(); ++i)
	{
		const FWidgetConnectionConfig& WidgetConfig = StateConfig->WidgetConfigs[i];
		UVCamWidget* Widget = WidgetConfig.ResolveWidget(this);
		if (!Widget)
		{
			UE_CLOG(!WidgetConfig.HasNoWidgetSet(), LogVCamStateSwitcher, Warning, TEXT("Failed to find widget at index %d in state %s"), i, *NewState.ToString());
			continue;
		}

		EConnectionUpdateResult UpdateResult;
		Widget->UpdateConnectionTargets(WidgetConfig.ConnectionTargets, bReinitializeConnections, UpdateResult);
	}
	
	CurrentState = NewState;
	OnPostStateChanged.Broadcast(this, OldState, CurrentState);
	return true;
}

TArray<FName> UVCamStateSwitcherWidget::GetStates() const
{
	TArray<FName> Keys;
	States.GenerateKeyArray(Keys);
	return Keys;
}

bool UVCamStateSwitcherWidget::GetStateInfo(FName State, FVCamWidgetConnectionState& OutStateInfo) const
{
	if (const FVCamWidgetConnectionState* StateInfo = States.Find(State))
	{
		OutStateInfo = *StateInfo;
		return true;
	}
	return false;
}

#if WITH_EDITOR
void UVCamStateSwitcherWidget::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (!States.Contains(DefaultState))
	{
		States.Add(DefaultState, {});
	}
	
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif 

void UVCamStateSwitcherWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// This widget was just created. There is not point in trying to update the current state because it will fail.
	const bool bIsPendingInitialization = GetVCamComponent() == nullptr;
	if (!bIsPendingInitialization)
	{
		SetStateOrFallbackToDefault(CurrentState);
	}
}

void UVCamStateSwitcherWidget::OnInitializeConnections_Implementation(UVCamComponent* VCam)
{
	Super::OnInitializeConnections_Implementation(VCam);

	// OnInitializeConnections is called just before ReinitializeConnections is called ... optimize by not calling it twice.
	constexpr bool bReinitializeConnections = false;
	SetStateOrFallbackToDefault(CurrentState, bReinitializeConnections);
}

void UVCamStateSwitcherWidget::SetStateOrFallbackToDefault(FName NewState, bool bReinitializeConnections)
{
	constexpr bool bForceUpdate = true;
	const bool bIsCurrentStateValid = SetCurrentState(NewState, bForceUpdate, bReinitializeConnections); 
	if (!bIsCurrentStateValid)
	{
		SetCurrentState(DefaultState, bForceUpdate);
	}
}