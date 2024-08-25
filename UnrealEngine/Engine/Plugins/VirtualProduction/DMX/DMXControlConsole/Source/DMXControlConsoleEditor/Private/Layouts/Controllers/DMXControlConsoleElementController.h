// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Layouts/Controllers/DMXControlConsoleControllerBase.h"
#include "Tickable.h"

#include "DMXControlConsoleElementController.generated.h"

enum class ECheckBoxState : uint8;
class IDMXControlConsoleFaderGroupElement;
class UDMXControlConsoleFaderBase;
class UDMXControlConsoleFaderGroupController;
class UDMXControlConsoleFloatOscillator;


/**
 * A controller for handling one or more elements at once. 
 * An element can be possessed by one controller at a time.
 */
UCLASS(AutoExpandCategories = ("DMX Element Controller", "DMX Element Controller|Oscillator"))
class UDMXControlConsoleElementController
	: public UDMXControlConsoleControllerBase
	, public FTickableGameObject
{
	GENERATED_BODY()

public:
	/** Possesses the given Element. An Element Controller can possess more than one Element at once. */
	virtual void Possess(const TScriptInterface<IDMXControlConsoleFaderGroupElement>& InElement);

	/** Possesses the given array of Elements. An Element Controller can possess more than one Element at once. */
	virtual void Possess(TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> InElements);

	/** Unpossesses the given Element, if valid */
	virtual void UnPossess(const TScriptInterface<IDMXControlConsoleFaderGroupElement>& InElement);

	/** Clears all the Elements in this Controller */
	void ClearElements();

	/** Sorts all the Elements in this Controller by their starting address */
	void SortElementsByStartingAddress();

	/** Returns the Fader Group Controller this Controller resides in */
	virtual UDMXControlConsoleFaderGroupController& GetOwnerFaderGroupControllerChecked() const;

	/** Returns the index of the Controller in the owner Fader Group Controller */
	virtual int32 GetIndex() const;

	/** Gets the array of Elements in this Controller */
	const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>>& GetElements() const { return Elements; }

	/** Gets the array of Faders in this Controller */
	TArray<UDMXControlConsoleFaderBase*> GetFaders() const;

	/** Gets the name of the Controller */
	const FString& GetUserName() const { return UserName; };

	/** Generates a string using the names of all the Elements in the Controller */
	FString GenerateUserNameByElementsNames() const;

	/** Sets the name of the Controller */
	void SetUserName(const FString& NewName);

	/** Returns the value of the Controller */
	float GetValue() const { return Value; }

	/** Sets the value of the Controller and all its Elements */
	void SetValue(float NewValue, bool bSyncElements = true);

	/** Returns the min value of the Controller */
	float GetMinValue() const { return MinValue; }

	/** Sets the min value of the Controller and all its Elements */
	virtual void SetMinValue(float NewMinValue, bool bSyncElements = true);

	/** Returns the max value of the Controller */
	float GetMaxValue() const { return MaxValue; }

	/** Sets the max value of the Controller and all its Elements */
	virtual void SetMaxValue(float NewMaxValue, bool bSyncElements = true);

	/** Resets all the Elements in this Controller to their default attribute values */
	void ResetToDefault();

	/** Sets the lock state of this Controller */
	void SetLocked(bool bLock);

	/** Gets the activity state of the Controller */
	bool IsActive() const;

	/** True if any of the Elements in the Controller matches the Control Console filtering system */
	bool IsMatchingFilter() const;

	/** True if the Controller is in the currently active layout */
	bool IsInActiveLayout() const;

	/** Gets the enable state of the controller according to the possesed fader groups */
	ECheckBoxState GetEnabledState() const;

	/** Destroys the Controller */
	virtual void Destroy();

	//~ Begin UObject interface
	virtual void PostInitProperties() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject interface

	// Property Name getters
	FORCEINLINE static FName GetUserNamePropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleElementController, UserName); }
	FORCEINLINE static FName GetValuePropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleElementController, Value); }
	FORCEINLINE static FName GetMinValuePropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleElementController, MinValue); }
	FORCEINLINE static FName GetMaxValuePropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleElementController, MaxValue); }
	FORCEINLINE static FName GetElementsPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleElementController, Elements); }
	FORCEINLINE static FName GetFloatOscillatorClassPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleElementController, FloatOscillatorClass); }
	FORCEINLINE static FName GetFloatOscillatorPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleElementController, FloatOscillator); }

protected:
	//~ Begin FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override { return true; };
	virtual ETickableTickType GetTickableTickType() const override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableGameObject interface

	/** The current name of this Controller */
	UPROPERTY(EditAnywhere, Category = "DMX Element Controller")
	FString UserName;

	/** The array of Elements in this Controller */
	UPROPERTY()
	TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> Elements;

private:
	/** The current value of the Controller */
	UPROPERTY(EditAnywhere, meta = (HideEditConditionToggle, EditCondition = "!bIsLocked", UIMin = 0, UIMax = 1), Category = "DMX Element Controller")
	float Value = 0.f;

	/** The minimum Controller Value */
	UPROPERTY(EditAnywhere, meta = (EditCondition = "!bIsLocked", UIMin = 0, UIMax = 1), Category = "DMX Element Controller")
	float MinValue = 0.f;

	/** The maximum Controller Value */
	UPROPERTY(EditAnywhere, meta = (EditCondition = "!bIsLocked", UIMin = 0, UIMax = 1), Category = "DMX Element Controller")
	float MaxValue = 1.f;

	/** Oscillator that is used for this Controller */
	UPROPERTY(EditAnywhere, meta = (DisplayName = "Oscillator Class", ShowDisplayNames), Category = "DMX Element Controller|Oscillator")
	TSoftClassPtr<UDMXControlConsoleFloatOscillator> FloatOscillatorClass;

	/** Float Oscillator applied to this Controller */
	UPROPERTY(VisibleAnywhere, Instanced, Meta = (DisplayName = "Oscillator"), Category = "DMX Element Controller|Oscillator")
	TObjectPtr<UDMXControlConsoleFloatOscillator> FloatOscillator;
};
