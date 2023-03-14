// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IFilterPreset.h"
#include "UObject/ObjectMacros.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "ITraceObject.h"

#include "FilterPresets.generated.h"

/** Structure representing an individual preset in configuration (ini) files */
USTRUCT()
struct FFilterData
{
	GENERATED_BODY()

	UPROPERTY()
	FString Name;

	UPROPERTY()
	TArray<FString> AllowlistedNames;
	
	bool operator==(const FFilterData& Other) const
	{
		return Name == Other.Name && AllowlistedNames == Other.AllowlistedNames;
	}
};

struct FFilterPresetHelpers
{
	/** Creates a new filtering preset according to the specific object names */
	static void CreateNewPreset(const TArray<TSharedPtr<ITraceObject>>& InObjects);
	/** Creates a set of strings, corresponding to set of non-filtered out object as part of InObjects */
	static void ExtractEnabledObjectNames(const TArray<TSharedPtr<ITraceObject>>& InObjects, TArray<FString>& OutNames);
	/** Returns whether or not shared presets can be modified, requires write-flag on default confing files */
	static bool CanModifySharedPreset();
};

/** Base implementation of a filter preset */
struct FFilterPreset : public IFilterPreset
{
public:
	FFilterPreset(const FString& InName, FFilterData& InFilterData) : Name(InName), FilterData(InFilterData) {}

	/** Begin IFilterPreset overrides */
	virtual FString GetName() const override;
	virtual FText GetDisplayText() const;
	virtual FText GetDescription() const;
	virtual void GetAllowlistedNames(TArray<FString>& OutNames) const override;
	virtual bool CanDelete() const override;
	virtual void Rename(const FString& InNewName) override;
	virtual bool Delete() override;
	virtual bool MakeShared() override;
	virtual bool MakeLocal() override;
	virtual bool IsLocal() const override;
	virtual void Save(const TArray<TSharedPtr<ITraceObject>>& InObjects) override {}
	virtual void Save() override {}
	/** End IFilterPreset overrides */
protected:
	FString Name;
	FFilterData& FilterData;
};

/** Engine level preset is simply a basic one */
typedef FFilterPreset FEngineFilterPreset;

/** User filter preset, allows for deletion / transitioning INI ownership */
struct FUserFilterPreset : public FFilterPreset
{
public:
	FUserFilterPreset(const FString& InName, FFilterData& InFilterData, bool bInLocal = false) : FFilterPreset(InName, InFilterData), bIsLocalPreset(bInLocal) {}

	/** Begin IFilterPreset overrides */
	virtual bool CanDelete() const override;
	virtual bool Delete() override;
	virtual bool MakeShared() override;
	virtual bool MakeLocal() override;
	virtual bool IsLocal() const override;
	virtual void Save(const TArray<TSharedPtr<ITraceObject>>& InObjects) override;
	virtual void Save() override;
	/** End IFilterPreset overrides */
protected:
	bool bIsLocalPreset;
};

/** UObject containers for the preset data */
UCLASS(Config = TraceDataFilters)
class ULocalFilterPresetContainer: public UObject
{
	GENERATED_BODY()

	friend struct FFilterPresetHelpers;
public:	
	void GetUserPresets(TArray<TSharedPtr<IFilterPreset>>& OutPresets);

	static void AddFilterData(const FFilterData& InFilterData);
	static bool RemoveFilterData(const FFilterData& InFilterData);
	static void Save();
protected:	
	UPROPERTY(Config)
	TArray<FFilterData> UserPresets;
};

UCLASS(Config = TraceDataFilters, DefaultConfig)
class USharedFilterPresetContainer : public UObject
{
	GENERATED_BODY()

	friend struct FFilterPresetHelpers;
public:
	void GetSharedUserPresets(TArray<TSharedPtr<IFilterPreset>>& OutPresets);

	static void AddFilterData(const FFilterData& InFilterData);
	static bool RemoveFilterData(const FFilterData& InFilterData);
	static void Save();
protected:
	UPROPERTY(Config)
	TArray<FFilterData> SharedPresets;
};

UCLASS(Config = TraceDataFilters, DefaultConfig)
class UEngineFilterPresetContainer : public UObject
{
	GENERATED_BODY()

	friend struct FFilterPresetHelpers;
public:
	void GetEnginePresets(TArray<TSharedPtr<IFilterPreset>>& OutPresets);
protected:
	UPROPERTY(Config)
	TArray<FFilterData> EnginePresets;
};