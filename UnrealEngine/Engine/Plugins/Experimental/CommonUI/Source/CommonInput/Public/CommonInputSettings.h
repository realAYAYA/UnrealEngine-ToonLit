// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "Templates/SubclassOf.h"
#include "InputCoreTypes.h"
#include "Engine/StreamableManager.h"
#include "Templates/SharedPointer.h"
#include "CommonInputActionDomain.h"
#include "CommonInputSubsystem.h"
#include "CommonInputBaseTypes.h"

#include "Engine/DataTable.h"
#include "Engine/PlatformSettings.h"
#include "CommonInputSettings.generated.h"

UCLASS(config = Game, defaultconfig)
class COMMONINPUT_API UCommonInputSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UCommonInputSettings(const FObjectInitializer& Initializer);

	// Called to load CommonUISetting data, if bAutoLoadData if set to false then game code must call LoadData().
	void LoadData();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
    
    // Called to check that the data we have previously attempted to load is actually loaded and will attempt to load if it is not.
    void ValidateData();

	FDataTableRowHandle GetDefaultClickAction() const;
	FDataTableRowHandle GetDefaultBackAction() const;

	bool GetEnableInputMethodThrashingProtection() const { return bEnableInputMethodThrashingProtection; }

	int32 GetInputMethodThrashingLimit() const { return InputMethodThrashingLimit; }

	double GetInputMethodThrashingWindowInSeconds() const {	return InputMethodThrashingWindowInSeconds;	}

	double GetInputMethodThrashingCooldownInSeconds() const { return InputMethodThrashingCooldownInSeconds; }

	bool GetAllowOutOfFocusDeviceInput() const { return bAllowOutOfFocusDeviceInput; }

	bool GetEnableDefaultInputConfig() const { return bEnableDefaultInputConfig; }

	TObjectPtr<UCommonInputActionDomainTable> GetActionDomainTable() const { return ActionDomainTablePtr; }

private:
	virtual void PostInitProperties() override;

	/** Create a derived asset from UCommonUIInputData to store Input data for your game.*/
	UPROPERTY(config, EditAnywhere, Category = "Input", Meta=(AllowAbstract=false))
	TSoftClassPtr<UCommonUIInputData> InputData;
	
	UPROPERTY(EditAnywhere, Category = "Input")
	FPerPlatformSettings PlatformInput;

	UPROPERTY(config)
	TMap<FName, FCommonInputPlatformBaseData> CommonInputPlatformData_DEPRECATED;

	UPROPERTY(config, EditAnywhere, Category = "Thrashing Settings")
	bool bEnableInputMethodThrashingProtection = true;

	UPROPERTY(config, EditAnywhere, Category = "Thrashing Settings")
	int32 InputMethodThrashingLimit = 30;

	UPROPERTY(config, EditAnywhere, Category = "Thrashing Settings")
	double InputMethodThrashingWindowInSeconds = 3.0;

	UPROPERTY(config, EditAnywhere, Category = "Thrashing Settings")
	double InputMethodThrashingCooldownInSeconds = 1.0;

	UPROPERTY(config, EditAnywhere, Category = "Input")
	bool bAllowOutOfFocusDeviceInput = false;

	/**
	* Controls whether a default Input Config will be set when the active CommonActivatableWidgets do not specify a desired one.
	* Disable this if you want to control the Input Mode via alternative means.
	*/
	UPROPERTY(config, EditAnywhere, Category = "Input")
	bool bEnableDefaultInputConfig = true;

	/** Create a derived asset from UCommonInputActionDomainTable to store ordered ActionDomain data for your game */
	UPROPERTY(config, EditAnywhere, Category = "Action Domain")
	TSoftObjectPtr<UCommonInputActionDomainTable> ActionDomainTable;

private:
	void LoadInputData();
	void LoadActionDomainTable();

	bool bInputDataLoaded;
	bool bActionDomainTableLoaded;

	UPROPERTY(Transient)
	TSubclassOf<UCommonUIInputData> InputDataClass;

	UPROPERTY(Transient)
	TObjectPtr<UCommonInputActionDomainTable> ActionDomainTablePtr;
};
