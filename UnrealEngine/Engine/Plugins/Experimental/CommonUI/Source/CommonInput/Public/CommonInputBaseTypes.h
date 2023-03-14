// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonInputTypeEnum.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "Templates/SubclassOf.h"
#include "InputCoreTypes.h"
#include "Engine/UserDefinedEnum.h"
#include "Templates/SharedPointer.h"
#include "Styling/SlateBrush.h"
#include "Engine/DataTable.h"
#include "UObject/SoftObjectPtr.h"
#include "Engine/PlatformSettings.h"
#include "CommonInputBaseTypes.generated.h"


class UTexture2D;
class UMaterial;
class UCommonInputSettings;

struct COMMONINPUT_API FCommonInputDefaults
{
	static const FName PlatformPC;
	static const FName GamepadGeneric;
};

USTRUCT(Blueprintable)
struct COMMONINPUT_API FCommonInputKeyBrushConfiguration
{
	GENERATED_BODY()

public:
	FCommonInputKeyBrushConfiguration();

	const FSlateBrush& GetInputBrush() const { return KeyBrush; }

public:
	UPROPERTY(EditAnywhere, Category = "Key Brush Configuration")
	FKey Key;

	UPROPERTY(EditAnywhere, Category = "Key Brush Configuration")
	FSlateBrush KeyBrush;
};

USTRUCT(Blueprintable)
struct COMMONINPUT_API FCommonInputKeySetBrushConfiguration
{
	GENERATED_BODY()

public:
	FCommonInputKeySetBrushConfiguration();

	const FSlateBrush& GetInputBrush() const { return KeyBrush; }

public:
	UPROPERTY(EditAnywhere, Category = "Key Brush Configuration", Meta = (TitleProperty = "KeyName"))
	TArray<FKey> Keys;

	UPROPERTY(EditAnywhere, Category = "Key Brush Configuration")
	FSlateBrush KeyBrush;
};

USTRUCT()
struct FInputDeviceIdentifierPair
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Gamepad")
	FName InputDeviceName;

	UPROPERTY(EditDefaultsOnly, Category = "Gamepad")
	FString HardwareDeviceIdentifier;
};

/* Derive from this class to store the Input data. It is referenced in the Common Input Settings, found in the project settings UI. */
UCLASS(Abstract, Blueprintable, ClassGroup = Input, meta = (Category = "Common Input"))
class COMMONINPUT_API UCommonUIInputData : public UObject
{
	GENERATED_BODY()

public:
	virtual bool NeedsLoadForServer() const override;

public:
	UPROPERTY(EditDefaultsOnly, Category = "Properties", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle DefaultClickAction;

	UPROPERTY(EditDefaultsOnly, Category = "Properties", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle DefaultBackAction;
};

/* Derive from this class to store the Input data. It is referenced in the Common Input Settings, found in the project settings UI. */
UCLASS(Abstract, Blueprintable, ClassGroup = Input, meta = (Category = "Common Input"))
class COMMONINPUT_API UCommonInputBaseControllerData : public UObject
{
	GENERATED_BODY()

public:
	virtual bool NeedsLoadForServer() const override;
	virtual bool TryGetInputBrush(FSlateBrush& OutBrush, const FKey& Key) const;
	virtual bool TryGetInputBrush(FSlateBrush& OutBrush, const TArray<FKey>& Keys) const;

	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
	virtual void PostLoad() override;

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient, EditAnywhere, Category = "Editor")
	int32 SetButtonImageHeightTo = 0;
#endif

public:
	UPROPERTY(EditDefaultsOnly, Category = "Default")
	ECommonInputType InputType;
	
	UPROPERTY(EditDefaultsOnly, Category = "Gamepad", meta=(EditCondition="InputType == ECommonInputType::Gamepad", GetOptions = GetRegisteredGamepads))
	FName GamepadName;

	UPROPERTY(EditDefaultsOnly, Category = "Gamepad", meta = (EditCondition = "InputType == ECommonInputType::Gamepad"))
	FText GamepadDisplayName;

	UPROPERTY(EditDefaultsOnly, Category = "Gamepad", meta=(EditCondition="InputType == ECommonInputType::Gamepad"))
	FText GamepadCategory;

	UPROPERTY(EditDefaultsOnly, Category = "Gamepad", meta = (EditCondition = "InputType == ECommonInputType::Gamepad"))
	FText GamepadPlatformName;

	UPROPERTY(EditDefaultsOnly, Category = "Gamepad", meta=(EditCondition="InputType == ECommonInputType::Gamepad"))
	TArray<FInputDeviceIdentifierPair> GamepadHardwareIdMapping;

	UPROPERTY(EditDefaultsOnly, Category = "Display")
	TSoftObjectPtr<UTexture2D> ControllerTexture;

	UPROPERTY(EditDefaultsOnly, Category = "Display")
	TSoftObjectPtr<UTexture2D> ControllerButtonMaskTexture;

	UPROPERTY(EditDefaultsOnly, Category = "Display", Meta = (TitleProperty = "Key"))
	TArray<FCommonInputKeyBrushConfiguration> InputBrushDataMap;

	UPROPERTY(EditDefaultsOnly, Category = "Display", Meta = (TitleProperty = "Keys"))
	TArray<FCommonInputKeySetBrushConfiguration> InputBrushKeySets;

	UFUNCTION()
	static const TArray<FName>& GetRegisteredGamepads();

private:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

UCLASS(config = Game, defaultconfig)
class COMMONINPUT_API UCommonInputPlatformSettings : public UPlatformSettings
{
	GENERATED_BODY()

	friend class UCommonInputSettings;

public:
	UCommonInputPlatformSettings();

	virtual void PostLoad() override;

	static UCommonInputPlatformSettings* Get()
	{
		return UPlatformSettingsManager::Get().GetSettingsForPlatform<UCommonInputPlatformSettings>();
	}

	bool TryGetInputBrush(FSlateBrush& OutBrush, FKey Key, ECommonInputType InputType, const FName GamepadName) const;
	bool TryGetInputBrush(FSlateBrush& OutBrush, const TArray<FKey>& Keys, ECommonInputType InputType, const FName GamepadName) const;

	FName GetBestGamepadNameForHardware(FName CurrentGamepadName, FName InputDeviceName, const FString& HardwareDeviceIdentifier); 

	ECommonInputType GetDefaultInputType() const
	{
		return DefaultInputType;
	}

	bool SupportsInputType(ECommonInputType InputType) const;

	const FName GetDefaultGamepadName() const
	{
		return DefaultGamepadName;
	}

	bool CanChangeGamepadType() const 
	{
		return bCanChangeGamepadType;
	}

	TArray<TSoftClassPtr<UCommonInputBaseControllerData>> GetControllerData()
	{
		return ControllerData;
	}


#if WITH_EDITOR
	void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent);
#endif

protected:
	void InitializeControllerData() const;
	virtual void InitializePlatformDefaults();

	UPROPERTY(config, EditAnywhere, Category = "Default")
	ECommonInputType DefaultInputType;

	UPROPERTY(config, EditAnywhere, Category = "Default")
	bool bSupportsMouseAndKeyboard;

	UPROPERTY(config, EditAnywhere, Category = "Default")
	bool bSupportsTouch;

	UPROPERTY(config, EditAnywhere, Category = "Default")
	bool bSupportsGamepad;

	UPROPERTY(config, EditAnywhere, Category = "Default", Meta = (EditCondition = "bSupportsGamepad"))
	FName DefaultGamepadName;

	UPROPERTY(config, EditAnywhere, Category = "Default", Meta = (EditCondition = "bSupportsGamepad"))
	bool bCanChangeGamepadType;

	UPROPERTY(config, EditAnywhere, Category = "Default", Meta = (TitleProperty = "InputType"))
	TArray<TSoftClassPtr<UCommonInputBaseControllerData>> ControllerData;

	UPROPERTY(Transient)
	mutable TArray<TSubclassOf<UCommonInputBaseControllerData>> ControllerDataClasses;
};

/* DEPRECATED Legacy! */
USTRUCT()
struct COMMONINPUT_API FCommonInputPlatformBaseData
{
	GENERATED_BODY()

	friend class UCommonInputSettings;

public:
	FCommonInputPlatformBaseData()
	{
		DefaultInputType = ECommonInputType::Gamepad;
		bSupportsMouseAndKeyboard = false;
		bSupportsGamepad = true;
		bCanChangeGamepadType = true;
		bSupportsTouch = false;
		DefaultGamepadName = FCommonInputDefaults::GamepadGeneric;
	}
	virtual ~FCommonInputPlatformBaseData() = default;

	virtual bool TryGetInputBrush(FSlateBrush& OutBrush, FKey Key, ECommonInputType InputType, const FName& GamepadName) const;
	virtual bool TryGetInputBrush(FSlateBrush& OutBrush, const TArray<FKey>& Keys, ECommonInputType InputType,  const FName& GamepadName) const;


	ECommonInputType GetDefaultInputType() const
	{
		return DefaultInputType;
	};

	bool SupportsInputType(ECommonInputType InputType) const 
	{
		switch (InputType)
		{
		case ECommonInputType::MouseAndKeyboard:
		{
			return bSupportsMouseAndKeyboard;
		}
		break;
		case ECommonInputType::Gamepad:
		{
			return bSupportsGamepad;
		}
		break;
		case ECommonInputType::Touch:
		{
			return bSupportsTouch;
		}
		break;
		}
		return false;
	}

	const FName GetDefaultGamepadName() const
	{
		return DefaultGamepadName;
	};

	bool CanChangeGamepadType() const 
	{
		return bCanChangeGamepadType;
	}

	TArray<TSoftClassPtr<UCommonInputBaseControllerData>> GetControllerData()
	{
		return ControllerData;
	}

	static const TArray<FName>& GetRegisteredPlatforms();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Properties")
	ECommonInputType DefaultInputType;

	UPROPERTY(EditDefaultsOnly, Category = "Properties")
	bool bSupportsMouseAndKeyboard;

	UPROPERTY(EditDefaultsOnly, Category = "Gamepad")
	bool bSupportsGamepad;

	UPROPERTY(EditDefaultsOnly, Category = "Gamepad", Meta = (EditCondition = "bSupportsGamepad"))
	FName DefaultGamepadName;

	UPROPERTY(EditDefaultsOnly, Category = "Gamepad", Meta = (EditCondition = "bSupportsGamepad"))
	bool bCanChangeGamepadType;

	UPROPERTY(EditDefaultsOnly, Category = "Properties")
	bool bSupportsTouch;

	UPROPERTY(EditDefaultsOnly, Category = "Properties", Meta = (TitleProperty = "GamepadName"))
	TArray<TSoftClassPtr<UCommonInputBaseControllerData>> ControllerData;

	UPROPERTY(Transient)
	TArray<TSubclassOf<UCommonInputBaseControllerData>> ControllerDataClasses;
};

class FCommonInputBase
{
public:
	COMMONINPUT_API static FName GetCurrentPlatformName();

	COMMONINPUT_API static UCommonInputSettings* GetInputSettings();

	COMMONINPUT_API static void GetCurrentPlatformDefaults(ECommonInputType& OutDefaultInputType, FName& OutDefaultGamepadName);
};
