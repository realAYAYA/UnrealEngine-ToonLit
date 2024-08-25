// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Dataflow/DataflowObjectInterface.h"
#include "Components/PrimitiveComponent.h"
#include "Templates/SharedPointer.h"
#include "Dataflow/DataflowEdNode.h"
#include "DataflowContent.generated.h"

class FDataflowEditorToolkit;
class UDataflow;
class USkeletalMesh;
class USkeleton;
class USkeletalMeshComponent;
class UAnimationAsset;
class UDataflowBaseContent;
class FPreviewScene;
class UAnimSingleNodeInstance;
class AActor;

namespace Dataflow
{
	enum class EDataflowPatternVertexType : uint8
	{
		Sim2D = 0,
		Sim3D = 1,
		Render = 2
	};
}

/** 
 * Context object used for selection/rendering 
 */

UCLASS()
class DATAFLOWENGINE_API UDataflowContextObject : public UObject
{
	GENERATED_BODY()
public:

	/** Selection Collection Access */
	void SetPrimarySelectedNode(TObjectPtr<UDataflowEdNode> InSelectedNode) { PrimarySelectedNode = InSelectedNode; }
	TObjectPtr<UDataflowEdNode> GetPrimarySelectedNode() const { return PrimarySelectedNode; }

	/** Render Collection used to generate the DynamicMesh3D on the PrimarySelection */
	void SetPrimaryRenderCollection(const TSharedPtr<FManagedArrayCollection>& InCollection) { PrimaryRenderCollection = InCollection; }
	TSharedPtr<const FManagedArrayCollection> GetPrimaryRenderCollection() const { return PrimaryRenderCollection; }

	/** ViewMode Access */
	void SetConstructionViewMode(Dataflow::EDataflowPatternVertexType InMode) {ConstructionViewMode = InMode;}
	Dataflow::EDataflowPatternVertexType GetConstructionViewMode() const { return ConstructionViewMode; }

	/** Get a single selected node of the specified type. Return nullptr if the specified node is not selected, or if multiple nodes are selected*/
	template<typename NodeType>
	NodeType* GetPrimarySelectedNodeOfType() const 
	{
		if (PrimarySelectedNode && PrimarySelectedNode->GetDataflowNode()) 
		{
			return PrimarySelectedNode->GetDataflowNode()->AsType<NodeType>();
		}
		return nullptr;
	}

protected:

	/** Render collection to be used */
	TSharedPtr<FManagedArrayCollection> PrimaryRenderCollection = nullptr;

	/** Primary node that is selected in the graph */
	TObjectPtr<UDataflowEdNode> PrimarySelectedNode = nullptr;

	/** Construction view mode for the context object @todo(michael) : is it only for construction or for simulation as well*/
	Dataflow::EDataflowPatternVertexType ConstructionViewMode = Dataflow::EDataflowPatternVertexType::Sim3D;
};

/** 
 * Dataflow content owning dataflow asset that that will be used to evaluate the graph
 */
UCLASS()
class DATAFLOWENGINE_API UDataflowBaseContent : public UDataflowContextObject
{
	GENERATED_BODY()

public:
	UDataflowBaseContent();

	/** Data flow asset that we will edit */
	UPROPERTY(EditAnywhere, Category = "Dataflow")
	TObjectPtr<UDataflow> DataflowAsset = nullptr;

	/** Data flow terminal path for evaluation */
	UPROPERTY(EditAnywhere, Category = "Dataflow")
	FString DataflowTerminal = "";
	
	/** 
	*	Dirty - State Invalidation
	*   Check if non-graph specific data has been changed, this usually requires a re-render 
	*/
	bool IsDirty() const { return bIsDirty; }
	void SetIsDirty(bool InDirty) { bIsDirty = InDirty; }

	/** 
	*	LastModifiedTimestamp - State Invalidation 
	*   Dataflow timestamp accessors can be used to see if the EvaluationContext has been invalidated. 
	*/
	void SetLastModifiedTimestamp(Dataflow::FTimestamp InTimestamp);
	const Dataflow::FTimestamp& GetLastModifiedTimestamp() const { return LastModifiedTimestamp; }

	/**  
	*	Context - Dataflow Evaluation State
	*   Dataflow context stores the evaluated state of the graph. 
	*/
	void SetDataflowContext(const TSharedPtr<Dataflow::FEngineContext>& InContext) { DataflowContext = InContext; bIsDirty = true; }
	const TSharedPtr<Dataflow::FEngineContext>& GetDataflowContext() const { return DataflowContext; }

	/** Return the simulation time range to be used in the simulation viewport */
	virtual FVector2f GetSimulationRange() const { return FVector2f(0.0f, 100.0f); } 
	
	/** Register components to the simulation world */
	virtual void RegisterWorldContent(FPreviewScene* PreviewScene, AActor* RootActor) {}
	
	/** Unregister components to the simulation world */
	virtual void UnregisterWorldContent(FPreviewScene* PreviewScene) {}

	/** Build the content context, timestamp*/
	void BuildBaseContent(TObjectPtr<UObject> ContentOwner);

	/** Collect reference objects for GC */
	virtual void AddContentObjects(FReferenceCollector& Collector) {}
	
	/** Data flow owner accessors */
	void SetDataflowOwner(const TObjectPtr<UObject>& InOwner) { if(DataflowContext) { DataflowContext->Owner = InOwner;  bIsDirty = true; }}
	TObjectPtr<UObject> GetDataflowOwner() const { return DataflowContext ? DataflowContext->Owner : nullptr; }

	/** Data flow asset accessors */
	void SetDataflowAsset(const TObjectPtr<UDataflow>& InAsset) { DataflowAsset = InAsset;  bIsDirty = true;}
	const TObjectPtr<UDataflow>& GetDataflowAsset() const { return DataflowAsset; }

	/** Data flow terminal accessors */
	void SetDataflowTerminal(const FString& InPath) { DataflowTerminal = InPath;  bIsDirty = true;}
	const FString& GetDataflowTerminal() const { return DataflowTerminal; }

	
protected :
	
	/**  Engine context to be used for dataflow evaluation */
    TSharedPtr<Dataflow::FEngineContext> DataflowContext = nullptr;

    /** Last data flow evaluated node time stamp */
    Dataflow::FTimestamp LastModifiedTimestamp = Dataflow::FTimestamp::Invalid;

    /** Dirty flag to trigger rendering. Do we need that? since when accessing the member by non const ref we will not dirty it */
    bool bIsDirty = true;
};

/** 
 * Dataflow content owning dataflow and skelmesh assets that that will be used to evaluate the graph
 */
UCLASS()
class DATAFLOWENGINE_API UDataflowSkeletalContent : public UDataflowBaseContent
{
	GENERATED_BODY()

public:
	UDataflowSkeletalContent();
	virtual ~UDataflowSkeletalContent() override{}

	/** Data flow skeletal mesh*/
	UPROPERTY(EditAnywhere, Category = "Preview")
	TObjectPtr<USkeletalMesh> SkeletalMesh = nullptr;

	/** Animation asset to be used to preview simulation */
	UPROPERTY(EditAnywhere, Category = "Preview")
	TObjectPtr<UAnimationAsset> AnimationAsset;

	/** Data flow skeleton */
	UPROPERTY(EditAnywhere, Category = "Skeleton")
	TObjectPtr<USkeleton> Skeleton = nullptr;

	/** Return the simulation time range to be used in the simulation viewport */
	virtual FVector2f GetSimulationRange() const override; 

	/** Register components to the scene world */
	virtual void RegisterWorldContent(FPreviewScene* PreviewScene, AActor* RootActor) override;
	
	/** Unregister components to the scene world */
	virtual void UnregisterWorldContent(FPreviewScene* PreviewScene) override;
	
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif //if WITH_EDITOR

	/** Collect reference objects for GC */
	virtual void AddContentObjects(FReferenceCollector& Collector) override;

	/** Update the animation node instance and set it to the skelmesh component*/
	void UpdateAnimationInstance();

	/** Data flow animation instance accessors */
	const TObjectPtr<UAnimSingleNodeInstance>& GetAnimationInstance() const {return AnimationNodeInstance;}
	TObjectPtr<UAnimSingleNodeInstance>& GetAnimationInstance() {return AnimationNodeInstance;}

	/** Data flow skeletal mesh accessors */
	void SetSkeletalMesh(const TObjectPtr<USkeletalMesh>& InMesh);
	const TObjectPtr<USkeletalMesh>& GetSkeletalMesh() const { return SkeletalMesh; }

	/** Data flow skeleton accessors */
	void SetSkeleton(const TObjectPtr<USkeleton>& InSkeleton);
	const TObjectPtr<USkeleton>& GetSkeleton() const { return Skeleton; }

	/** Data flow animation asset accessors */
	void SetAnimationAsset(const TObjectPtr<UAnimationAsset>& InAnimation);
	const TObjectPtr<UAnimationAsset>& GetAnimationAsset() const { return AnimationAsset; }
	
protected :
	
	/** Skeletal mesh component used in the preview scene */
	TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent;

	/** Anim node instance used with skelmesh component */
	TObjectPtr<UAnimSingleNodeInstance> AnimationNodeInstance;
};
