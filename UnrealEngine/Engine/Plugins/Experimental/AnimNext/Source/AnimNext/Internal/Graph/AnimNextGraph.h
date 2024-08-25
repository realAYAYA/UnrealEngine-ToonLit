// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextGraphEntryPoint.h"
#include "AnimNextRigVMAsset.h"
#include "RigUnit_AnimNextGraphRoot.h"
#include "RigVMCore/RigVM.h"
#include "DecoratorBase/DecoratorPtr.h"
#include "DecoratorBase/DecoratorHandle.h"
#include "DecoratorBase/EntryPointHandle.h"
#include "Graph/GraphInstanceComponent.h"
#include "Graph/RigUnit_AnimNextGraphEvaluator.h"
#include "Param/ParamId.h"
#include "Scheduler/IAnimNextScheduleTermInterface.h"
#include "RigVMHost.h"

#include "AnimNextGraph.generated.h"

class UEdGraph;
class UAnimNextGraph;
class UAnimGraphNode_AnimNextGraph;
struct FAnimNode_AnimNextGraph;
struct FRigUnit_AnimNextGraphEvaluator;
struct FAnimNextGraphInstancePtr;
struct FAnimNextGraphInstance;
class UAnimNextSchedule;
struct FAnimNextScheduleGraphTask;
struct FAnimNextParam;

namespace UE::AnimNext
{
	struct FContext;
	struct FExecutionContext;
	class FModule;
	struct FTestUtils;
}

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

namespace UE::AnimNext::Editor
{
	class FGraphEditor;
}

namespace UE::AnimNext::Graph
{
	extern ANIMNEXT_API const FName EntryPointName;
	extern ANIMNEXT_API const FName ResultName;
}

// A user-created graph of logic used to supply data
UCLASS(BlueprintType)
class ANIMNEXT_API UAnimNextGraph : public UAnimNextRigVMAsset, public IAnimNextScheduleTermInterface
{
	GENERATED_BODY()

public:
	static const UE::AnimNext::FParamId DefaultReferencePoseId;
	static const UE::AnimNext::FParamId DefaultCurrentLODId;

	UAnimNextGraph(const FObjectInitializer& ObjectInitializer);

	// UObject interface
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;

	// IAnimNextScheduleTermInterface interface
	virtual TConstArrayView<UE::AnimNext::FScheduleTerm> GetTerms() const override;

	// Allocates an instance of the graph
	// @param	OutInstance		The instance to allocate data for
	// @param	InEntryPoint	The entry point to use. If this is NAME_None then the default entry point for this graph is used
	void AllocateInstance(FAnimNextGraphInstancePtr& OutInstance, FName InEntryPoint = NAME_None) const;

	// Allocates an instance of the graph with the specified parent graph instance
	// @param	InOutParentGraphInstance	The parent graph instance to use
	// @param	OutInstance					The instance to allocate data for
	// @param	InEntryPoint				The entry point to use. If this is NAME_None then the default entry point for this graph is used
	void AllocateInstance(FAnimNextGraphInstance& InOutParentGraphInstance, FAnimNextGraphInstancePtr& OutInstance, FName InEntryPoint = NAME_None) const;

	// Get the parameter to use to access the reference pose
	UE::AnimNext::FParamId GetReferencePoseParam() const { return ReferencePoseId; }

	// Get the parameter to use to access the current LOD
	UE::AnimNext::FParamId GetCurrentLODParam() const { return CurrentLODId; }

protected:

	// Loads the graph data from the provided archive buffer and returns true on success, false otherwise
	bool LoadFromArchiveBuffer(const TArray<uint8>& SharedDataArchiveBuffer);

	// Allocates an instance of the graph with an optional parent graph instance
	void AllocateInstanceImpl(FAnimNextGraphInstance* InOutParentGraphInstance, FAnimNextGraphInstancePtr& OutInstance, FName InEntryPoint) const;

#if WITH_EDITORONLY_DATA
	// During graph compilation, if we have existing graph instances, we freeze them by releasing their memory before thawing them
	// Freezing is a partial release of resources that retains the necessary information to re-create things safely
	void FreezeGraphInstances();

	// During graph compilation, once compilation is done we thaw existing graph instances to reallocate their memory
	void ThawGraphInstances();
#endif

	friend class UAnimNextGraphFactory;
	friend class UAnimNextGraph_EditorData;
	friend struct UE::AnimNext::UncookedOnly::FUtils;
	friend class UE::AnimNext::Editor::FGraphEditor;
	friend struct UE::AnimNext::FTestUtils;
	friend FAnimNextGraphInstancePtr;
	friend FAnimNextGraphInstance;
	friend class UAnimGraphNode_AnimNextGraph;
	friend UE::AnimNext::FExecutionContext;
	friend class UAnimNextSchedule;
	friend struct FAnimNextScheduleGraphTask;
	friend UE::AnimNext::FModule;
	
#if WITH_EDITORONLY_DATA
	mutable FCriticalSection GraphInstancesLock;

	// This is a list of live graph instances that have been allocated, used in the editor to reset instances when we re-compile/live edit
	mutable TSet<FAnimNextGraphInstance*> GraphInstances;
#endif

	// This is the execute method definition used by a graph to evaluate latent pins
	UPROPERTY()
	FAnimNextGraphEvaluatorExecuteDefinition ExecuteDefinition;

	// Data for each entry point in this graph
	UPROPERTY()
	TArray<FAnimNextGraphEntryPoint> EntryPoints;

	// This is a resolved handle to the root decorator in our graph, for each entry point 
	TMap<FName, FAnimNextDecoratorHandle> ResolvedRootDecoratorHandles;

	// This is the graph shared data used by the decorator system, the output of FDecoratorReader
	// We de-serialize manually into this buffer from the archive buffer, this is never saved on disk
	TArray<uint8> SharedDataBuffer;

	// This is a list of all referenced UObjects in the graph shared data
	// We collect all the references here to make it quick and easy for the GC to query them
	// It means that object references in the graph shared data are not visited at runtime by the GC (they are immutable)
	// The shared data serialization archive stores indices to these to perform UObject serialization
	UPROPERTY()
	TArray<TObjectPtr<UObject>> GraphReferencedObjects;

	// The entry point that this graph defaults to using
	UPROPERTY(EditAnywhere, Category = "Graph")
	FName DefaultEntryPoint = FRigUnit_AnimNextGraphRoot::DefaultEntryPoint;

	// The parameter to use to access the reference pose
	UPROPERTY(EditAnywhere, Category = "Graph", meta=(CustomWidget = "ParamName", AllowedParamType = "FAnimNextGraphReferencePose"))
	FName ReferencePose = DefaultReferencePoseId.GetName();

	// The parameter to use to access the current LOD
	UPROPERTY(EditAnywhere, Category = "Graph", meta=(CustomWidget = "ParamName", AllowedParamType = "int32"))
	FName CurrentLOD = DefaultCurrentLODId.GetName();

	UE::AnimNext::FParamId ReferencePoseId = UE::AnimNext::FParamId(ReferencePose);
	UE::AnimNext::FParamId CurrentLODId = UE::AnimNext::FParamId(CurrentLOD);

	// Hash of required parameters
	UPROPERTY()
	uint64 RequiredParametersHash = 0;

	// All the parameters that are required for this graph to run
	UPROPERTY()
	TArray<FAnimNextParam> RequiredParameters;

#if WITH_EDITORONLY_DATA
	// This buffer holds the output of the FDecoratorWriter post compilation
	// We serialize it manually and it is discarded at runtime
	TArray<uint8> SharedDataArchiveBuffer;
#endif
};
