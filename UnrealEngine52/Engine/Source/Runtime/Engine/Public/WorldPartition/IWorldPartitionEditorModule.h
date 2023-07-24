// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Templates/SubclassOf.h"

class UWorldPartitionBuilder;

/**
 * The module holding all of the UI related pieces for WorldPartition
 */
class IWorldPartitionEditorModule : public IModuleInterface
{
public:
	virtual ~IWorldPartitionEditorModule() {}

	virtual bool ConvertMap(const FString& InLongPackageName) = 0;

	UE_DEPRECATED(5.1, "Use RunBuilder with UWorld* instead.")
	virtual bool RunBuilder(TSubclassOf<UWorldPartitionBuilder> BuilderClass, const FString& InLongPackageName) { return false; }
		
	struct FRunBuilderParams
	{
		TSubclassOf<UWorldPartitionBuilder> BuilderClass = nullptr;
		UWorld* World = nullptr;
		FString ExtraArgs;
		FText OperationDescription;
		bool bUnloadMap = true;
	};

	virtual bool RunBuilder(TSubclassOf<UWorldPartitionBuilder> BuilderClass, UWorld* InWorld);
	virtual bool RunBuilder(const FRunBuilderParams& Params) = 0;

	virtual int32 GetPlacementGridSize() const = 0;
	virtual int32 GetInstancedFoliageGridSize() const = 0;
	virtual int32 GetMinimapLowQualityWorldUnitsPerPixelThreshold() const = 0;
	
	virtual bool GetDisableLoadingInEditor() const = 0;
	virtual void SetDisableLoadingInEditor(bool bInDisableLoadingInEditor) = 0;

	virtual bool GetDisablePIE() const = 0;
	virtual void SetDisablePIE(bool bInDisablePIE) = 0;

	virtual bool GetDisableBugIt() const = 0;
	virtual void SetDisableBugIt(bool bInDisableBugIt) = 0;

	virtual bool IsEditingContentBundle() const = 0;
	virtual bool IsEditingContentBundle(const FGuid& ContentBundleGuid) const = 0;

	/** Triggered when a world is added. */
	DECLARE_EVENT_OneParam(IWorldPartitionEditorModule, FWorldPartitionCreated, UWorld*);

	/** Triggered when the editor is about to launch a commandlet. Can be used to modify the builder params. */
	DECLARE_EVENT_OneParam(IWorldPartitionEditorModule, FOnPreExecuteCommandlet, FRunBuilderParams&);

	/** Triggered when the editor launches a commandlet. Can be used to provide project specific arguments. */
	DECLARE_EVENT_OneParam(IWorldPartitionEditorModule, FOnExecuteCommandlet, TArray<FString>&);

	/** Triggered when the editor has launched a commandlet. */
	DECLARE_EVENT(IWorldPartitionEditorModule, FOnPostExecuteCommandlet);

	/** Return the world added event. */
	virtual FWorldPartitionCreated& OnWorldPartitionCreated() = 0;

	/** Return the commandlet pre-execution event */
	virtual FOnPreExecuteCommandlet& OnPreExecuteCommandlet() = 0;

	/** Return the commandlet execution event */
	virtual FOnExecuteCommandlet& OnExecuteCommandlet() = 0;

	/** Return the commandlet post-execution event */
	virtual FOnPostExecuteCommandlet& OnPostExecuteCommandlet() = 0;
};