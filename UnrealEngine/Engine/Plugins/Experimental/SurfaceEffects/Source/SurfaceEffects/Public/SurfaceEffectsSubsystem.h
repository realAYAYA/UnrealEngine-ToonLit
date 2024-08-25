// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "SurfaceEffectsSubsystem.generated.h"

/**
 * Base data asset used to store what conditions result in a specific surface being returned
 */
UCLASS(Abstract)
class SURFACEEFFECTS_API USurfaceEffectRule : public UDataAsset
{
	GENERATED_BODY()
};

/**
 * Data Table Row that effectively wraps the Surface Effect Rule
 */
USTRUCT()
struct FSurfaceEffectTableRow : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere , Category="Surface Effect")
	TObjectPtr<USurfaceEffectRule> Rule;
};

/**
 * Base context for determining which enum value to return based on a certain rule
 * @tparam TEnum An Enum class with the UENUM Specifier
 */
template <typename TEnum>
struct TSurfaceEffectContext
{
	virtual ~TSurfaceEffectContext() = default;

	explicit TSurfaceEffectContext()
	{
	}

	virtual void GetSurface(TEnum& OutSurface, USurfaceEffectRule* Rule) = 0;
};

/**
 * A system for handling various surface enums based on contexts
 */
UCLASS(MinimalAPI)
class USurfaceEffectsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	template <typename TEnum>
	void GetSurface(TEnum& OutSurface, TSurfaceEffectContext<TEnum>& Context)
	{
		const UEnum* EnumClass = StaticEnum<TEnum>();
		if(SurfaceEffectsData)
		{
			FSurfaceEffectTableRow* Row = SurfaceEffectsData->FindRow<FSurfaceEffectTableRow>(EnumClass->GetFName(), ANSI_TO_TCHAR(__FUNCTION__));
			Context.GetSurface(OutSurface, Row->Rule);
		}
	}

private:
	/*
	 * We store the UEnum name as the row name in a data table to get the rule associated with that surface enum
	 */
	UPROPERTY()
	TObjectPtr<UDataTable> SurfaceEffectsData;
};
