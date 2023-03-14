// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IOptimusNodeFunctionLibraryOwner.h"
#include "IOptimusNodeGraphCollectionOwner.h"
#include "IOptimusPathResolver.h"
#include "OptimusCoreNotify.h"
#include "OptimusDataType.h"
#include "OptimusNodeGraph.h"
#include "OptimusComponentSource.h"
#include "OptimusDataDomain.h"

#include "Animation/MeshDeformer.h"
#include "Interfaces/Interface_PreviewMeshProvider.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/TVariant.h"

#include "OptimusDeformer.generated.h"

class UOptimusComponentDataProvider;
class UOptimusPersistentBufferDataInterface;
class UComputeGraph;
class USkeletalMesh;
class UOptimusActionStack;
class UOptimusComputeGraph;
class UOptimusDeformer;
class UOptimusResourceDescription;
class UOptimusVariableDescription;
enum class EOptimusDiagnosticLevel : uint8;
struct FOptimusCompilerDiagnostic;
struct FOptimusCompoundAction;


DECLARE_MULTICAST_DELEGATE_OneParam(FOptimusCompileBegin, UOptimusDeformer *);
DECLARE_MULTICAST_DELEGATE_OneParam(FOptimusCompileEnd, UOptimusDeformer *);
DECLARE_MULTICAST_DELEGATE_OneParam(FOptimusGraphCompileMessageDelegate, FOptimusCompilerDiagnostic const&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOptimusConstantValueUpdate, FString const&, TArray<uint8> const&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOptimusSetAllInstancesCanbeActive, bool);

USTRUCT()
struct FOptimusComputeGraphInfo
{
	GENERATED_BODY()

	UPROPERTY()
	EOptimusNodeGraphType GraphType = EOptimusNodeGraphType::Update;

	UPROPERTY()
	FName GraphName;

	UPROPERTY()
	TObjectPtr<UOptimusComputeGraph> ComputeGraph = nullptr;
};

/** A container class that owns component source bindings. This is used to ensure we don't end up
  * with a namespace clash between graphs, variables, bindings and resources.
  */
UCLASS()
class UOptimusComponentSourceBindingContainer :
	public UObject
{
	GENERATED_BODY()
	
public:
	UPROPERTY()
	TArray<TObjectPtr<UOptimusComponentSourceBinding>> Bindings;
};

/** A container class that owns variable descriptors. This is used to ensure we don't end up
  * with a namespace clash between graphs, variables and resources.
  */
UCLASS()
class UOptimusVariableContainer :
	public UObject
{
	GENERATED_BODY()
	
public:
	UPROPERTY()
	TArray<TObjectPtr<UOptimusVariableDescription>> Descriptions;
};


/** A container class that owns resource descriptors. This is used to ensure we don't end up
  * with a namespace clash between graphs, variables and resources.
  */
UCLASS()
class UOptimusResourceContainer :
	public UObject
{
	GENERATED_BODY()
	
public:
	UPROPERTY()
	TArray<TObjectPtr<UOptimusResourceDescription>> Descriptions;
};


/**
  * A Deformer Graph is an asset that is used to create and control custom deformations on 
  * skeletal meshes.
  */
UCLASS(Blueprintable, BlueprintType)
class OPTIMUSCORE_API UOptimusDeformer :
	public UMeshDeformer,
	public IInterface_PreviewMeshProvider,
	public IOptimusPathResolver,
	public IOptimusNodeGraphCollectionOwner,
	public IOptimusNodeFunctionLibraryOwner 
{
	GENERATED_BODY()

public:
	UOptimusDeformer();

	UOptimusActionStack *GetActionStack();

	/** Returns the global delegate used to notify on global operations (e.g. graph, variable,
	 *  resource lifecycle events).
	 */
	FOptimusGlobalNotifyDelegate& GetNotifyDelegate() { return GlobalNotifyDelegate; }

	/** Add a setup graph. This graph is executed once when the deformer is first run from a
	  * mesh component. If the graph already exists, this function does nothing and returns 
	  * nullptr.
	  */
	UOptimusNodeGraph* AddSetupGraph();
	

	/** Add a trigger graph. This graph will be scheduled to execute on next tick, prior to the
	  * update graph being executed, after being triggered from a blueprint.
	  * @param InName The name to give the graph. The name "Setup" cannot be used, since it's a
	  *  reserved name.
	  */
	UOptimusNodeGraph* AddTriggerGraph(const FString &InName);

	/// Returns the update graph. The update graph will always exist, and there is only one.
	UOptimusNodeGraph* GetUpdateGraph() const;
	/** Remove a graph and delete it. */
	bool RemoveGraph(UOptimusNodeGraph* InGraph);

	// Variables
	UOptimusVariableDescription* AddVariable(
		FOptimusDataTypeRef InDataTypeRef,
	    FName InName = NAME_None
		);

	bool RemoveVariable(
	    UOptimusVariableDescription* InVariableDesc
		);

	bool RenameVariable(
	    UOptimusVariableDescription* InVariableDesc,
	    FName InNewName,
		bool bInForceChange = false
	    );
	    
	bool SetVariableDataType(
		UOptimusVariableDescription* InVariableDesc,
		FOptimusDataTypeRef InDataType,
		bool bInForceChange = false
		);

	UFUNCTION(BlueprintGetter)
	const TArray<UOptimusVariableDescription*>& GetVariables() const { return Variables->Descriptions; }

	UOptimusVariableDescription* ResolveVariable(
		FName InVariableName
		) const override;

	// Resources
	UOptimusResourceDescription* AddResource(
		FOptimusDataTypeRef InDataTypeRef,
	    FName InName = NAME_None
		);

	bool RemoveResource(
	    UOptimusResourceDescription* InResourceDesc
		);

	bool RenameResource(
	    UOptimusResourceDescription* InResourceDesc,
	    FName InNewName,
	    bool bInForceChange = false
	    );

	bool SetResourceDataType(
		UOptimusResourceDescription* InResourceDesc,
		FOptimusDataTypeRef InDataType,
		bool bInForceChange = false
		);

	bool SetResourceDataDomain(
		UOptimusResourceDescription* InResourceDesc,
		const FOptimusDataDomain& InDataDomain,
		bool bInForceChange = false
		);
	
	UFUNCTION(BlueprintGetter)
	const TArray<UOptimusResourceDescription*>& GetResources() const { return Resources->Descriptions; }

	UOptimusResourceDescription* ResolveResource(
		FName InResourceName
		) const override;

	// Component Bindings
	UOptimusComponentSourceBinding* AddComponentBinding(
		const UOptimusComponentSource *InComponentSource,
		FName InName = NAME_None
		);

	bool RemoveComponentBinding(
		UOptimusComponentSourceBinding* InBinding
		);

	bool RenameComponentBinding(
		UOptimusComponentSourceBinding* InBinding,
		FName InNewName,
		bool bInForceChange = false
		);

	bool SetComponentBindingSource(
		UOptimusComponentSourceBinding* InBinding,
		const UOptimusComponentSource *InComponentSource,
		bool bInForceChange = false
		);
	
	UFUNCTION(BlueprintGetter)
	const TArray<UOptimusComponentSourceBinding*>& GetComponentBindings() const { return Bindings->Bindings; }

	UOptimusComponentSourceBinding* ResolveComponentBinding(
		FName InBindingName
		) const override;

	
	/// Graph compilation
	bool Compile();

	/** Returns a multicast delegate that can be subscribed to listen for the start of compilation. */
	FOptimusCompileBegin& GetCompileBeginDelegate()  { return CompileBeginDelegate; }
	/** Returns a multicast delegate that can be subscribed to listen for the end of compilation but before shader compilation is complete. */
	FOptimusCompileEnd& GetCompileEndDelegate() { return CompileEndDelegate; }
	/** Returns a multicast delegate that can be subscribed to listen compilation results. Note that the shader compilation results are async and can be returned after the CompileEnd delegate. */
	FOptimusGraphCompileMessageDelegate& GetCompileMessageDelegate() { return CompileMessageDelegate; }

	void SetAllInstancesCanbeActive(bool bInCanBeActive) const;
	
	/// UObject overrides
	void Serialize(FArchive& Ar) override;
	void PostLoad() override;
	void BeginDestroy() override;
	// Whenever the asset is renamed/moved, generated classes parented to the old package
	// are not moved to the new package automatically (see FAssetRenameManager), so we
	// have to manually perform the move/rename, to avoid invalid reference to the old package
	void PostRename(UObject* OldOuter, const FName OldName) override;
	
#if WITH_EDITOR
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// UMeshDeformer overrides
	UMeshDeformerInstanceSettings* CreateSettingsInstance(
		UMeshComponent* InMeshComponent
		) override;
	
	UMeshDeformerInstance* CreateInstance(
		UMeshComponent* InMeshComponent,
		UMeshDeformerInstanceSettings* InSettings
		) override;
	
	// IInterface_PreviewMeshProvider overrides
	void SetPreviewMesh(USkeletalMesh* PreviewMesh, bool bMarkAsDirty = true) override;
	USkeletalMesh* GetPreviewMesh() const override;

	// IOptimusPathResolver overrides
	IOptimusNodeGraphCollectionOwner* ResolveCollectionPath(const FString& InPath) override;
	UOptimusNodeGraph* ResolveGraphPath(const FString& InGraphPath) override;
	UOptimusNode* ResolveNodePath(const FString& InNodePath) override;
	UOptimusNodePin* ResolvePinPath(const FString& InPinPath) override;

	// IOptimusNodeGraphCollectionOwner overrides
	IOptimusNodeGraphCollectionOwner* GetCollectionOwner() const override { return nullptr; }
	IOptimusNodeGraphCollectionOwner* GetCollectionRoot() const override { return const_cast<UOptimusDeformer*>(this); }
	FString GetCollectionPath() const override { return FString(); }

	const TArray<UOptimusNodeGraph*> &GetGraphs() const override { return Graphs; }

	UOptimusNodeGraph* CreateGraph(
		EOptimusNodeGraphType InType,
		FName InName)
	{ return CreateGraph(InType, InName, TOptional<int32>()); }
	
	UOptimusNodeGraph* CreateGraph(
	    EOptimusNodeGraphType InType,
	    FName InName,
	    TOptional<int32> InInsertBefore) override;
	
	bool AddGraph(
	    UOptimusNodeGraph* InGraph,
		int32 InInsertBefore) override;
	
	bool RemoveGraph(
	    UOptimusNodeGraph* InGraph,
		bool bDeleteGraph) override;

	bool MoveGraph(
	    UOptimusNodeGraph* InGraph,
	    int32 InInsertBefore) override;

	bool RenameGraph(
	    UOptimusNodeGraph* InGraph,
	    const FString& InNewName) override;
	
	UPROPERTY(EditAnywhere, Category=Preview)
	TObjectPtr<USkeletalMesh> Mesh = nullptr;

protected:
	friend class UOptimusComponentSourceBinding;
	friend class UOptimusDeformerInstance;
	friend class UOptimusNodeGraph;
	friend class UOptimusResourceDescription;
	friend class UOptimusVariableDescription;
	friend struct FOptimusComponentBindingAction_AddBinding;
	friend struct FOptimusComponentBindingAction_RemoveBinding;
	friend struct FOptimusComponentBindingAction_RenameBinding;
	friend struct FOptimusComponentBindingAction_SetComponentSource;
	friend struct FOptimusResourceAction_AddResource;
	friend struct FOptimusResourceAction_RemoveResource;
	friend struct FOptimusResourceAction_RenameResource;
	friend struct FOptimusResourceAction_SetDataType;
	friend struct FOptimusResourceAction_SetDataDomain;
	friend struct FOptimusVariableAction_AddVariable;
	friend struct FOptimusVariableAction_RemoveVariable;
	friend struct FOptimusVariableAction_RenameVariable;
	friend struct FOptimusVariableAction_SetDataType;

	/** Create a resource owned by this deformer but does not add it to the list of known
	  * resources. Call AddResource for that */
	UOptimusResourceDescription* CreateResourceDirect(
		FName InName
		);

	/** Adds a resource that was created by this deformer and is owned by it. */
	bool AddResourceDirect(
		UOptimusResourceDescription* InResourceDesc
		);

	bool RemoveResourceDirect(
		UOptimusResourceDescription* InResourceDesc
		);

	bool RenameResourceDirect(
		UOptimusResourceDescription* InResourceDesc,
		FName InNewName
		);
		
	bool SetResourceDataTypeDirect(
		UOptimusResourceDescription* InResourceDesc,
		FOptimusDataTypeRef InDataType
		);

	bool SetResourceDataDomainDirect(
		UOptimusResourceDescription* InResourceDesc,
		const FOptimusDataDomain& InDataDomain
		);

	
	/** Create a resource owned by this deformer but does not add it to the list of known
	  * resources. Call AddResource for that */
	UOptimusVariableDescription* CreateVariableDirect(
		FName InName
		);

	/** Adds a resource that was created by this deformer and is owned by it. */
	bool AddVariableDirect(
		UOptimusVariableDescription* InVariableDesc
		);

	bool RemoveVariableDirect(
		UOptimusVariableDescription* InVariableDesc
		);

	bool RenameVariableDirect(
		UOptimusVariableDescription* InVariableDesc,
		FName InNewName
		);

	bool SetVariableDataTypeDirect(
		UOptimusVariableDescription* InResourceDesc,
		FOptimusDataTypeRef InDataType
		);

	UOptimusComponentSourceBinding* CreateComponentBindingDirect(
		const UOptimusComponentSource *InComponentSource,
		FName InName = NAME_None
		);

	bool AddComponentBindingDirect(
		UOptimusComponentSourceBinding* InComponentBinding
		);

	bool RemoveComponentBindingDirect(
		UOptimusComponentSourceBinding* InBinding
		);

	bool RenameComponentBindingDirect(
		UOptimusComponentSourceBinding* InBinding,
		FName InNewName);
	
	bool SetComponentBindingSourceDirect(
		UOptimusComponentSourceBinding* InBinding,
		const UOptimusComponentSource *InComponentSource
		);

	
	void Notify(EOptimusGlobalNotifyType InNotifyType, UObject *InObject) const;

	// The compute graphs to execute.
	UPROPERTY()
	TArray<FOptimusComputeGraphInfo> ComputeGraphs;
	
private:
	void PostLoadFixupMissingComponentBindingsCompat();
	void PostLoadFixupMismatchedResourceDataDomains();

	/** Find a compatible binding with the given data interface. Returns nullptr if no such binding exists */
	UOptimusComponentSourceBinding* FindCompatibleBindingWithInterface(
		const UOptimusComputeDataInterface* InDataInterface
		) const;
	
	UOptimusNodeGraph* ResolveGraphPath(const FStringView InPath, FStringView& OutRemainingPath) const;
	UOptimusNode* ResolveNodePath(const FStringView InPath, FStringView& OutRemainingPath) const;
	int32 GetUpdateGraphIndex() const;

	TArray<UOptimusNode*> GetAllNodesOfClass(UClass* InNodeClass) const;
	
	/// Compile a node graph to a compute graph. Returns a complete compute graph if compilation succeeded. 
	UOptimusComputeGraph* CompileNodeGraphToComputeGraph(
		const UOptimusNodeGraph *InNodeGraph,
		TFunction<void(EOptimusDiagnosticLevel, FText, const UObject*)> InErrorReporter
		);

	void OnDataTypeChanged(FName InTypeName);
	
	UPROPERTY(transient)
	TObjectPtr<UOptimusActionStack> ActionStack = nullptr;

	UPROPERTY()
	TArray<TObjectPtr<UOptimusNodeGraph>> Graphs;

	UPROPERTY()
	TObjectPtr<UOptimusComponentSourceBindingContainer> Bindings;

	UPROPERTY()
	TObjectPtr<UOptimusVariableContainer> Variables;

	UPROPERTY()
	TObjectPtr<UOptimusResourceContainer> Resources;

	FOptimusGlobalNotifyDelegate GlobalNotifyDelegate;

	FOptimusCompileBegin CompileBeginDelegate;
	
	FOptimusCompileEnd CompileEndDelegate;

	FOptimusGraphCompileMessageDelegate CompileMessageDelegate;

	FOptimusConstantValueUpdate ConstantValueUpdateDelegate;

	FOptimusSetAllInstancesCanbeActive SetAllInstancesCanbeActiveDelegate;
};
