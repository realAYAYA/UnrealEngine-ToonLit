// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/UObjectGlobals.h"
#include "HitProxies.h"
#include "UObject/ObjectPtr.h"

class AActor;
class ABrush;
class FLevelEditorViewportClient;
class UModel;
class USceneComponent;
struct FTypedElementHandle;
struct FViewportClick;
struct HActor;
struct HGeomEdgeProxy;
struct HGeomPolyProxy;
struct HGeomVertexProxy;

/**
 * A hit proxy class for sockets in the main editor viewports.
 */
struct HLevelSocketProxy : public HHitProxy
{
	DECLARE_HIT_PROXY();

	TObjectPtr<AActor> Actor;
	TObjectPtr<USceneComponent> SceneComponent;
	FName SocketName;

	HLevelSocketProxy(AActor* InActor, USceneComponent* InSceneComponent, FName InSocketName)
		:	HHitProxy(HPP_UI)
		,   Actor( InActor )
		,   SceneComponent( InSceneComponent )
		,	SocketName( InSocketName )
	{}
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override
	{
		Collector.AddReferencedObject( Actor );
		Collector.AddReferencedObject( SceneComponent );
	}
};

namespace LevelViewportClickHandlers
{
	bool ClickViewport(FLevelEditorViewportClient* ViewportClient, const FViewportClick& Click);

	bool ClickElement(FLevelEditorViewportClient* ViewportClient, const FTypedElementHandle& HitElement, const FViewportClick& Click);

	bool UNREALED_API ClickActor(FLevelEditorViewportClient* ViewportClient,AActor* Actor,const FViewportClick& Click,bool bAllowSelectionChange);

	bool ClickComponent(FLevelEditorViewportClient* ViewportClient, HActor* ActorHitProxy, const FViewportClick& Click);

	void ClickBrushVertex(FLevelEditorViewportClient* ViewportClient,ABrush* InBrush,FVector* InVertex,const FViewportClick& Click);

	void ClickStaticMeshVertex(FLevelEditorViewportClient* ViewportClient,AActor* InActor,FVector& InVertex,const FViewportClick& Click);
	
	void ClickSurface(FLevelEditorViewportClient* ViewportClient, UModel* Model, int32 iSurf, const FViewportClick& Click);

	void ClickBackdrop(FLevelEditorViewportClient* ViewportClient,const FViewportClick& Click);

	void ClickLevelSocket(FLevelEditorViewportClient* ViewportClient, HHitProxy* HitProxy, const FViewportClick& Click);
};


