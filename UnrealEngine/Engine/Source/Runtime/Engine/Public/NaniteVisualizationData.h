// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UWorld;
class FNaniteVisualizationData
{
public:
	enum class FModeType : uint8
	{
		Overview,
		Standard,
		Advanced,
	};

	/** Describes a single available visualization mode. */
	struct FModeRecord
	{
		FString   ModeString;
		FName     ModeName;
		FText     ModeText;
		FText     ModeDesc;
		FModeType ModeType;
		int32     ModeID;

		// Whether or not this mode (by default) composites with regular scene depth.
		bool      DefaultComposited;
	};

	/** Mapping of FName to a visualization mode record. */
	typedef TMultiMap<FName, FModeRecord> TModeMap;

public:
	FNaniteVisualizationData()
	: bIsInitialized(false)
	{
	}

	/** Initialize the system. */
	void Initialize();

	/** Check if system was initialized. */
	inline bool IsInitialized() const { return bIsInitialized; }

	/** Check if visualization is active. */
	ENGINE_API bool IsActive() const;

	/** Update state and check if visualization is active. */
	ENGINE_API bool Update(const FName& InViewMode);

	/** Get the display name of a named mode from the available mode map. **/
	ENGINE_API FText GetModeDisplayName(const FName& InModeName) const;

	ENGINE_API int32 GetModeID(const FName& InModeName) const;

	ENGINE_API bool GetModeDefaultComposited(const FName& InModeName) const;

	/** We cache the overview mode name list from the console command here, so all dynamically created views can re-use the existing cached list of modes. */
	void SetCurrentOverviewModeList(const FString& InNameList);
	bool IsDifferentToCurrentOverviewModeList(const FString& InNameList);

	inline int32 GetActiveModeID() const
	{
		return ActiveVisualizationModeID;
	}

	inline const FName& GetActiveModeName() const
	{
		return ActiveVisualizationModeName;
	}

	inline bool GetActiveModeDefaultComposited() const
	{
		return bActiveVisualizationModeComposited;
	}

	/** Access the list of modes currently in use by the Nanite visualization overview. */
	inline const TArray<FName, TInlineAllocator<32>>& GetOverviewModeNames() const
	{
		return CurrentOverviewModeNames;
	}

	inline const TArray<int32, TInlineAllocator<32>>& GetOverviewModeIDs() const
	{
		return CurrentOverviewModeIDs;
	}

	inline const TModeMap& GetModeMap() const
	{
		return ModeMap;
	}

	/** Return the console command name for enabling single mode visualization. */
	static const TCHAR* GetVisualizeConsoleCommandName()
	{
		return TEXT("r.Nanite.Visualize");
	}

	/** Return the console command name for enabling multi mode visualization. */
	static const TCHAR* GetOverviewConsoleCommandName()
	{
		return TEXT("r.Nanite.VisualizeOverview");
	}

	ENGINE_API void Pick(UWorld* World);

	inline const FVector2f& GetPickingMousePos() const
	{
		return MousePos;
	}

	inline const FIntPoint& GetPickingScreenSize() const
	{
		return ScreenSize;
	}

private:
	/** Internal helper function for creating the Nanite visualization system console commands. */
	void ConfigureConsoleCommand();

	void AddVisualizationMode(
		const TCHAR* ModeString,
		const FText& ModeText,
		const FModeType ModeType,
		int32 ModeID,
		bool DefaultComposited
	);

	void SetActiveMode(int32 ModeID, const FName& ModeName, bool bDefaultComposited);

private:
	/** The name->mode mapping table */
	TModeMap ModeMap;

	int32 ActiveVisualizationModeID = INDEX_NONE;
	FName ActiveVisualizationModeName = NAME_None;
	bool bActiveVisualizationModeComposited = true;

	/** List of modes names to use in the Nanite visualization overview. */
	FString CurrentOverviewModeList;

	/** Tokenized Nanite visualization mode names. */
	TArray<FName, TInlineAllocator<32>> CurrentOverviewModeNames;
	TArray<int32, TInlineAllocator<32>> CurrentOverviewModeIDs;
	bool bOverviewListEmpty = true;
	
	/** Storage for console variable documentation strings. **/
	FString ConsoleDocumentationVisualizationMode;
	FString ConsoleDocumentationOverviewTargets;

	/** Mouse picking information */
	FVector2f MousePos;
	FIntPoint ScreenSize;

	/** Flag indicating if system is initialized. **/
	bool bIsInitialized;
};

ENGINE_API FNaniteVisualizationData& GetNaniteVisualizationData();
