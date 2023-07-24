// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EngineDefines.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "PrimitiveViewRelevance.h"
#include "DebugRenderSceneProxy.h"
#include "EnvironmentQuery/EnvQueryDebugHelpers.h"
#include "Debug/DebugDrawComponent.h"
#include "EQSRenderingComponent.generated.h"

class APlayerController;
class IEQSQueryResultSourceInterface;
class UCanvas;

class AIMODULE_API FEQSSceneProxy final : public FDebugRenderSceneProxy
{
	friend class FEQSRenderingDebugDrawDelegateHelper;
public:
	virtual SIZE_T GetTypeHash() const override;

	explicit FEQSSceneProxy(const UPrimitiveComponent& InComponent, const FString& ViewFlagName = TEXT("DebugAI"), const TArray<FSphere>& Spheres = TArray<FSphere>(), const TArray<FText3d>& Texts = TArray<FText3d>());
	
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;

#if  USE_EQS_DEBUGGER 
	static void CollectEQSData(const UPrimitiveComponent* InComponent, const IEQSQueryResultSourceInterface* QueryDataSource, TArray<FSphere>& Spheres, TArray<FText3d>& Texts, TArray<EQSDebug::FDebugHelper>& DebugItems);
	static void CollectEQSData(const FEnvQueryResult* ResultItems, const FEnvQueryInstance* QueryInstance, float HighlightRangePct, bool ShouldDrawFailedItems, TArray<FSphere>& Spheres, TArray<FText3d>& Texts, TArray<EQSDebug::FDebugHelper>& DebugItems);
#endif
private:
	FEnvQueryResult QueryResult;	
	// can be 0
	AActor* ActorOwner;
	const IEQSQueryResultSourceInterface* QueryDataSource;
	uint32 bDrawOnlyWhenSelected : 1;

	static const FVector3f ItemDrawRadius;

	bool SafeIsActorSelected() const;
};

#if  USE_EQS_DEBUGGER
class FEQSRenderingDebugDrawDelegateHelper : public FDebugDrawDelegateHelper
{
	typedef FDebugDrawDelegateHelper Super;

public:
	FEQSRenderingDebugDrawDelegateHelper()
		: ActorOwner(nullptr)
		, QueryDataSource(nullptr)
		, bDrawOnlyWhenSelected(false)
	{
	}

	void SetupFromProxy(const FEQSSceneProxy* InSceneProxy)
	{
		ActorOwner = InSceneProxy->ActorOwner;
		QueryDataSource = InSceneProxy->QueryDataSource;
		bDrawOnlyWhenSelected = InSceneProxy->bDrawOnlyWhenSelected;
	}

	void Reset()
	{
		ResetTexts();
	}

protected:
	AIMODULE_API virtual void DrawDebugLabels(UCanvas* Canvas, APlayerController*) override;

private:
	// can be 0
	AActor* ActorOwner;
	const IEQSQueryResultSourceInterface* QueryDataSource;
	uint32 bDrawOnlyWhenSelected : 1;
};
#endif

UCLASS(ClassGroup = Debug)
class AIMODULE_API UEQSRenderingComponent : public UDebugDrawComponent
{
	GENERATED_UCLASS_BODY()

	FString DrawFlagName;
	uint32 bDrawOnlyWhenSelected : 1;

	void ClearStoredDebugData();
#if  USE_EQS_DEBUGGER || ENABLE_VISUAL_LOG
	void StoreDebugData(const EQSDebug::FQueryData& DebugData);
#endif

protected:
	virtual FBoxSphereBounds CalcBounds(const FTransform &LocalToWorld) const override;

#if UE_ENABLE_DEBUG_DRAWING && USE_EQS_DEBUGGER
	virtual FDebugRenderSceneProxy* CreateDebugSceneProxy() override;
	virtual FDebugDrawDelegateHelper& GetDebugDrawDelegateHelper() override { return EQSRenderingDebugDrawDelegateHelper; }
	FEQSRenderingDebugDrawDelegateHelper EQSRenderingDebugDrawDelegateHelper;
#endif

	//EQSDebug::FQueryData DebugData;
	TArray<FDebugRenderSceneProxy::FSphere> DebugDataSolidSpheres;
	TArray<FDebugRenderSceneProxy::FText3d> DebugDataTexts;
};
