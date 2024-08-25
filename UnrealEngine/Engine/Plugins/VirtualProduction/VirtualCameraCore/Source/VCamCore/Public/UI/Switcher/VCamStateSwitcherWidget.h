// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UI/VCamWidget.h"
#include "VCamWidgetConnectionState.h"
#include "VCamStateSwitcherWidget.generated.h"

class UVCamStateSwitcherWidget;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FChangeConnectionStateEvent, UVCamStateSwitcherWidget*, Widget, FName, OldState, FName, NewState);

DECLARE_LOG_CATEGORY_EXTERN(LogVCamStateSwitcher, Log, All);

/**
 * A widget that has a set of states you can switch between using SetCurrentState.
 * A state is a collection of VCamWidgets whose connections should be rebound to new connection points.
 */
UCLASS()
class VCAMCORE_API UVCamStateSwitcherWidget : public UVCamWidget
{
	GENERATED_BODY()
	static FName DefaultState;
public:

	UFUNCTION(BlueprintCallable, Category = "Connections", meta = (BlueprintInternalUseOnly = "true"))
	void K2_SetCurrentState(FName NewState) { SetCurrentState(NewState); }

	/**
	 * Switches to given state - if the state transition is valid, UpdateConnectionTargets will be called.
	 * If CurrentState == NewState, then this call will be ignored (unless bForceUpdate == true).
	 * 
	 * @param NewState The new state to switch to
	 * @param bForceUpdate Call UpdateConnectionTargets even if the CurrentState == NewState
	 * @param bReinitializeConnections Parameter to pass to UpdateConnectionTargets. If true, ReinitializeConnections will be called.
	 */
	UFUNCTION(BlueprintCallable, Category = "Connections")
	bool SetCurrentState(FName NewState, bool bForceUpdate = false, bool bReinitializeConnections = true);
	
	UFUNCTION(BlueprintPure, Category = "Connections")
	FName GetCurrentState() const { return CurrentState; }

	UFUNCTION(BlueprintPure, Category = "Connections")
	TArray<FName> GetStates() const;

	UFUNCTION(BlueprintPure, Category = "Connections")
	bool GetStateInfo(FName State, FVCamWidgetConnectionState& OutStateInfo) const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	FORCEINLINE static FName GetCurrentStatePropertyName() { return GET_MEMBER_NAME_CHECKED(UVCamStateSwitcherWidget, CurrentState); }

protected:

	//~ Begin UUserWidget Interface
	virtual void NativePreConstruct() override;
	//~ End UUserWidget Interface

	//~ Begin UVCamWidget Interface
	virtual void OnInitializeConnections_Implementation(UVCamComponent* VCam) override;
	//~ End UVCamWidget Interface
	
private:

	/** Executes when the state is about to be changed */
	UPROPERTY(BlueprintAssignable, Category = "Connections")
	FChangeConnectionStateEvent OnPreStateChanged;

	/** Executes when after the state has been changed */
	UPROPERTY(BlueprintAssignable, Category = "Connections")
	FChangeConnectionStateEvent OnPostStateChanged;

	/** The states */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Connections", meta = (AllowPrivateAccess = "true"))
	TMap<FName, FVCamWidgetConnectionState> States { { DefaultState, {} } };

	UPROPERTY(EditAnywhere, BlueprintGetter = "GetCurrentState", BlueprintSetter = "K2_SetCurrentState", Category = "Connections")
	FName CurrentState = DefaultState;

	void SetStateOrFallbackToDefault(FName NewState, bool bReinitializeConnections = true);
};
