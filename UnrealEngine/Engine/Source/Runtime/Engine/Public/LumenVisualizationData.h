// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FLumenVisualizationData
{
public:
	enum class FModeType : uint8
	{
		Overview,
		Standard
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
	FLumenVisualizationData()
	: bIsInitialized(false)
	{
	}

	/** Initialize the system. */
	void Initialize();

	/** Check if system was initialized. */
	inline bool IsInitialized() const { return bIsInitialized; }

	/** Check if visualization is active. */
	ENGINE_API bool IsActive() const;

	/** Get the display name of a named mode from the available mode map. **/
	ENGINE_API FText GetModeDisplayName(const FName& InModeName) const;

	ENGINE_API int32 GetModeID(const FName& InModeName) const;

	ENGINE_API bool GetModeDefaultComposited(const FName& InModeName) const;

	inline const TModeMap& GetModeMap() const
	{
		return ModeMap;
	}

	/** Return the console command name for enabling single mode visualization. */
	static const TCHAR* GetVisualizeConsoleCommandName()
	{
		return TEXT("r.Lumen.Visualize.ViewMode");
	}

private:
	/** Internal helper function for creating the Lumen visualization system console commands. */
	void ConfigureConsoleCommand();

	void AddVisualizationMode(
		const TCHAR* ModeString,
		const FText& ModeText,
		const FText& ModeDesc,
		const FModeType ModeType,
		int32 ModeID,
		bool DefaultComposited
	);

	void SetActiveMode(int32 ModeID, const FName& ModeName, bool bDefaultComposited);

private:
	/** The name->mode mapping table */
	TModeMap ModeMap;

	/** Storage for console variable documentation strings. **/
	FString ConsoleDocumentationVisualizationMode;

	/** Flag indicating if system is initialized. **/
	bool bIsInitialized;
};

ENGINE_API FLumenVisualizationData& GetLumenVisualizationData();
