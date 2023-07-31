// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "Debug/DebugDrawComponent.h"
#include "DebugRenderSceneProxy.h"
#include "ZoneGraphAnnotationTestingActor.generated.h"

class UZoneGraphAnnotationTestingComponent;
class FDebugRenderSceneProxy;

/** Base class for ZoneGraph Annotation tests. */
UCLASS(Abstract, EditInlineNew)
class ZONEGRAPHANNOTATIONS_API UZoneGraphAnnotationTest : public UObject
{
	GENERATED_BODY()

public:
	virtual void Trigger() {}

#if UE_ENABLE_DEBUG_DRAWING
	virtual FBox CalcBounds(const FTransform& LocalToWorld) const { return FBox(ForceInit); };
	virtual void DebugDraw(FDebugRenderSceneProxy* DebugProxy) {}
	virtual void DebugDrawCanvas(UCanvas* Canvas, APlayerController*) {}
#endif
	
	void SetOwner(UZoneGraphAnnotationTestingComponent* Owner) { OwnerComponent = Owner; OnOwnerSet(); }
	const UZoneGraphAnnotationTestingComponent* GetOwner() const { return OwnerComponent; }

protected:
	virtual void OnOwnerSet() {}

	UPROPERTY()
	TObjectPtr<UZoneGraphAnnotationTestingComponent> OwnerComponent;
};


/** Debug component to test Mass ZoneGraph Annotations. Handles tests and rendering. */
UCLASS(ClassGroup = Debug)
class ZONEGRAPHANNOTATIONS_API UZoneGraphAnnotationTestingComponent : public UDebugDrawComponent
{
	GENERATED_BODY()
public:
	UZoneGraphAnnotationTestingComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

	void Trigger();

protected:
	
#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

	virtual void OnRegister() override;
	virtual void OnUnregister() override;


#if UE_ENABLE_DEBUG_DRAWING
	virtual FDebugRenderSceneProxy* CreateDebugSceneProxy() override;
	virtual void DebugDraw(FDebugRenderSceneProxy* DebugProxy);
	virtual void DebugDrawCanvas(UCanvas* Canvas, APlayerController*);

	FDelegateHandle CanvasDebugDrawDelegateHandle;
#endif

	UPROPERTY(EditAnywhere, Category = "Test", Instanced)
	TArray<TObjectPtr<UZoneGraphAnnotationTest>> Tests;
};


/** Debug actor to test Mass ZoneGraph Annotations. */
UCLASS(HideCategories = (Actor, Input, Collision, Rendering, Replication, Partition, HLOD, Cooking))
class ZONEGRAPHANNOTATIONS_API AZoneGraphAnnotationTestingActor : public AActor
{
	GENERATED_BODY()
public:
	AZoneGraphAnnotationTestingActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Simple trigger function to trigger something on the tests.
	 * Ideally this would be part of each test, but it does not work there.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Test")
	void Trigger();

protected:
#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
#endif

	UPROPERTY(Category = Default, VisibleAnywhere, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UZoneGraphAnnotationTestingComponent> TestingComp;
};
