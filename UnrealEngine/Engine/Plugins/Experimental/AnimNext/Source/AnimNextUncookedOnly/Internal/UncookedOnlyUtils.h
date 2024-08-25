// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IAnimNextRigVMGraphInterface.h"
#include "AssetRegistry/AssetData.h"
#include "Param/ParamTypeHandle.h"
#include "RigVMCore/RigVMTemplate.h"

#include "UncookedOnlyUtils.generated.h"

struct FAnimNextParam;
class UAnimNextSchedule;
class UAnimNextGraph;
class UAnimNextGraph_EditorData;
class UAnimNextGraph_EdGraph;
class URigVMController;
class URigVMGraph;
class UAnimNextParameterBlock;
class UAnimNextParameterBlock_EditorData;
class UAnimNextGraph_EdGraph;
struct FEdGraphPinType;
class UAnimNextRigVMAsset;
class UAnimNextRigVMAssetEditorData;
class UAnimNextRigVMAssetEntry;

namespace UE
{
	namespace AnimNext
	{
		static const FName ExportsAnimNextAssetRegistryTag = TEXT("AnimNextExports");
	}
}

UENUM()
enum class EAnimNextParameterFlags
{
	NoFlags = 0x0,
	Private = 0x1,
	Read = 0x02,
	Write = 0x04,
	Bound = 0x08,
	Max
};

ENUM_CLASS_FLAGS(EAnimNextParameterFlags)

USTRUCT()
struct FAnimNextParameterAssetRegistryExportEntry
{
	GENERATED_BODY()

	FAnimNextParameterAssetRegistryExportEntry() = default;
	
	FAnimNextParameterAssetRegistryExportEntry(FName InName, const FAnimNextParamType& InType, EAnimNextParameterFlags InFlags = EAnimNextParameterFlags::NoFlags)
		: Name(InName)
		, Type(InType)
		, Flags(InFlags) 
	{}
	
	UPROPERTY()
	FName Name;

	UPROPERTY()
	FAnimNextParamType Type;

	// Asset, found first in asset-registry, that references this parameter entry
	FAssetData ReferencingAsset;

	UPROPERTY()
	EAnimNextParameterFlags Flags = EAnimNextParameterFlags::NoFlags;
};

USTRUCT()
struct FAnimNextParameterProviderAssetRegistryExports
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FAnimNextParameterAssetRegistryExportEntry> Parameters;
};

namespace UE::AnimNext::UncookedOnly
{

struct ANIMNEXTUNCOOKEDONLY_API FUtils
{
	static void Compile(UAnimNextGraph* InGraph);
	
	static UAnimNextGraph_EditorData* GetEditorData(const UAnimNextGraph* InAnimNextGraph);
	
	static UAnimNextGraph* GetGraph(const UAnimNextGraph_EditorData* InEditorData);
	
	static void RecreateVM(UAnimNextGraph* InGraph);

	/**
	 * Get an AnimNext parameter type handle from an FEdGraphPinType.
	 * Note that the returned handle may not be valid, so should be checked using IsValid() before use.
	 **/
	static FParamTypeHandle GetParameterHandleFromPin(const FEdGraphPinType& InPinType);

	static void Compile(UAnimNextParameterBlock* InParameterBlock);

	static void CompileVM(UAnimNextParameterBlock* InParameterBlock);

	static void CompileStruct(UAnimNextParameterBlock* InParameterBlock);
	
	static UAnimNextParameterBlock_EditorData* GetEditorData(const UAnimNextParameterBlock* InParameterBlock);

	static UAnimNextParameterBlock* GetBlock(const UAnimNextParameterBlock_EditorData* InEditorData);

	static FInstancedPropertyBag* GetPropertyBag(UAnimNextParameterBlock* ReferencedBlock);

	static void RecreateVM(UAnimNextParameterBlock* InParameterBlock);

	static UAnimNextRigVMAsset* GetAsset(UAnimNextRigVMAssetEditorData* InEditorData);

	static UAnimNextRigVMAssetEditorData* GetEditorData(UAnimNextRigVMAsset* InAsset);

	/**
	 * Get an AnimNext parameter type from an FEdGraphPinType.
	 * Note that the returned handle may not be valid, so should be checked using IsValid() before use.
	 **/
	static FParamTypeHandle GetParamTypeHandleFromPinType(const FEdGraphPinType& InPinType);
	static FAnimNextParamType GetParamTypeFromPinType(const FEdGraphPinType& InPinType);

	/**
	 * Get an FEdGraphPinType from an AnimNext parameter type/handle.
	 * Note that the returned pin type may not be valid.
	 **/
	static FEdGraphPinType GetPinTypeFromParamTypeHandle(const FParamTypeHandle& InParamTypeHandle);
	static FEdGraphPinType GetPinTypeFromParamType(const FAnimNextParamType& InParamType);

	/**
	 * Get an FRigVMTemplateArgumentType from an AnimNext parameter type/handle.
	 * Note that the returned pin type may not be valid.
	 **/
	static FRigVMTemplateArgumentType GetRigVMArgTypeFromParamTypeHandle(const FParamTypeHandle& InParamTypeHandle);
	static FRigVMTemplateArgumentType GetRigVMArgTypeFromParamType(const FAnimNextParamType& InParamType);

	/** Set up a simple animation graph */
	static void SetupAnimGraph(UAnimNextRigVMAssetEntry* InEntry, URigVMController* InController);
	
	/** Set up a simple parameter graph */
	static void SetupParameterGraph(URigVMController* InController);
	
	/** Converts the Verse-tag-like snake_case_parameter_name to a period-separated display name similar to a gameplay tag */
	static FText GetParameterDisplayNameText(FName InParameterName);
	
	// Gets the parameters that are exported to the asset registry for an asset
	static bool GetExportedParametersForAsset(const FAssetData& InAsset, FAnimNextParameterProviderAssetRegistryExports& OutExports);

	// Gets all the parameters that are exported to the asset registry
	static bool GetExportedParametersFromAssetRegistry(FAnimNextParameterProviderAssetRegistryExports& OutExports);

	// Gets the exported parameters that are used by a RigVM asset
	static void GetAssetParameters(const UAnimNextRigVMAssetEditorData* EditorData, FAnimNextParameterProviderAssetRegistryExports& OutExports);

	// Gets the exported parameters that are used by a RigVM graph
	static void GetGraphParameters(const URigVMGraph* Graph, FAnimNextParameterProviderAssetRegistryExports& OutExports);

	// Gets the parameters that are exported to the asset registry by a schedule
	static void GetScheduleParameters(const UAnimNextSchedule* InSchedule, FAnimNextParameterProviderAssetRegistryExports& OutExports);

	// Gets the parameters that are exported to the asset registry by a blueprint
	static void GetBlueprintParameters(const UBlueprint* InBlueprint, FAnimNextParameterProviderAssetRegistryExports& OutExports);

	// Attempts to determine the type from a parameter name
	// If the name cannot be found, the returned type will be invalid
	// Note that this is expensive and can query the asset registry
	static FAnimNextParamType GetParameterTypeFromName(FName InName);

	// Compiles a schedule
	static void CompileSchedule(UAnimNextSchedule* InSchedule);

	// Sorts the incoming array of parameters, then generates a hash and returns it.
	static uint64 SortAndHashParameters(TArray<FAnimNextParam>& InParameters);
};

}