// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HAL/IConsoleManager.h"
#include "Subsystems/EngineSubsystem.h"
#include "Engine/DeveloperSettings.h"
#include "DataDrivenCVars.generated.h"

class UUserDefinedStruct;

/**
 *
 */
UCLASS(DisplayName = "DataDrivenCVars")
class ENGINE_API UDataDrivenCVarEngineSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:

	UDataDrivenCVarEngineSubsystem() {}

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDataDrivenCVarChanged, FString, CVarName);

	UPROPERTY(BlueprintAssignable, Category = "DataDrivenCVar")
	FOnDataDrivenCVarChanged OnDataDrivenCVarDelegate;
};

/**
 *
 */
UENUM(BlueprintType)
enum class FDataDrivenCVarType : uint8
{
	CVarFloat,
	CVarInt,
	CVarBool
};

USTRUCT(BlueprintType)
struct ENGINE_API FDataDrivenConsoleVariable
{
	GENERATED_BODY()

	FDataDrivenConsoleVariable() {}
	~FDataDrivenConsoleVariable();

	UPROPERTY(EditAnywhere, config, Category = "DataDrivenCVar", meta = (InlineCategoryProperty))
	FDataDrivenCVarType Type = FDataDrivenCVarType::CVarInt;

	UPROPERTY(EditAnywhere, config, Category = "DataDrivenCVar")
	FString Name;

	UPROPERTY(EditAnywhere, config, Category = "DataDrivenCVar")
	FString ToolTip;

	UPROPERTY(EditAnywhere, config, Category = "DataDrivenCVar", meta = (EditCondition = "Type == FDataDrivenCVarType::CVarFloat", EditConditionHides))
	float DefaultValueFloat = 0.0f;

	UPROPERTY(EditAnywhere, config, Category = "DataDrivenCVar", meta = (EditCondition = "Type == FDataDrivenCVarType::CVarInt", EditConditionHides))
	int32 DefaultValueInt = 0;

	UPROPERTY(EditAnywhere, config, Category = "DataDrivenCVar", meta = (EditCondition = "Type == FDataDrivenCVarType::CVarBool", EditConditionHides))
	bool DefaultValueBool = false;

#if WITH_EDITOR
	void Refresh();
#endif

	void Register();
	void UnRegister(bool bUseShadowName = false);

	FString ShadowName;
	FDataDrivenCVarType ShadowType;
};

/**
 * 
 */
UCLASS(config = Engine, defaultconfig, meta = (DisplayName = "Data Driven CVars"))
class ENGINE_API UDataDrivenConsoleVariableSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	DECLARE_EVENT_OneParam(UDataDrivenConsoleVariableSettings, FOnStageChanged, const FString&);
	FOnStageChanged OnStageChanged;

	UPROPERTY(config, EditAnywhere, Category = "DataDrivenCVar")
	TArray<FDataDrivenConsoleVariable> CVarsArray;

	static void OnDataDrivenChange(IConsoleVariable* CVar);

	//~ Begin UObject Interface
	virtual void PostInitProperties() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	virtual FName GetCategoryName() const;

protected:

	TArray<FString> ShadowCVars;
};