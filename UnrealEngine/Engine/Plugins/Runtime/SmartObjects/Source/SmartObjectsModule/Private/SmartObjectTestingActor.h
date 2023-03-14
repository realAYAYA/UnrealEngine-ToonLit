// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "SmartObjectDebugRenderingComponent.h"
#include "SmartObjectSubsystem.h"
#include "SmartObjectTestingActor.generated.h"

class ASmartObjectTestingActor;
class FDebugRenderSceneProxy;

/** Base class for SmartObject tests. */
UCLASS(Abstract, EditInlineNew)
class SMARTOBJECTSMODULE_API USmartObjectTest : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Calls 'Run' method if both Testing actor and SmartObject subsystem are valid.
	 * @return True if a redraw is required; false otherwise
	 */
	bool RunTest();

	/**
	 * Calls 'Reset' method if both Testing actor and SmartObject subsystem are valid.
	 * @return True if a redraw is required; false otherwise.
	 */
	bool ResetTest();

	/**
	 * Calls 'CalcBounds' method if both Testing actor and SmartObject subsystem are valid.
	 * @return The box representing the bounds encapsulating all elements of the test.
	 */
	FBox CalcTestBounds() const;

#if UE_ENABLE_DEBUG_DRAWING
	void DebugDraw(FDebugRenderSceneProxy* DebugProxy) const;
	void DebugDrawCanvas(UCanvas* Canvas, APlayerController*) const;
#endif

protected:
	virtual void PostInitProperties() override;

	/**
	 * Method to override to perform a test.
	 * @param TestingActor A testing actor that will provide a valid SmartObjectSubsystem (i.e. safe to call GetSubsystemRef)
	 * @return True if a redraw is required (e.g. results have changed); false otherwise.
	 */
	virtual bool Run(ASmartObjectTestingActor& TestingActor) { return false; }

	/**
	 * Method to override to reset results of the test.
	 * @param TestingActor A testing actor that will provide a valid SmartObjectSubsystem (i.e. safe to call GetSubsystemRef)*
	 * @return True if a redraw is required (e.g. cleared some results); false otherwise.
	 */
	virtual bool Reset(ASmartObjectTestingActor& TestingActor) { return false; }

	/**
	 * Method to override to provide the bounds of the test, if any.
	 * @param TestingActor A testing actor that will provide a valid SmartObjectSubsystem (i.e. safe to call GetSubsystemRef)*
	 * @return The box representing the bounds encapsulating all elements of the test, if any.
	 */
	virtual FBox CalcBounds(ASmartObjectTestingActor& TestingActor) const { return FBox(ForceInit); }

#if UE_ENABLE_DEBUG_DRAWING
	/**
	 * Method to override to add element to the debug render scene.
	 * @param TestingActor A testing actor that will provide a valid SmartObjectSubsystem (i.e. safe to call GetSubsystemRef)*
	 * @param DebugProxy Scene proxy in which debug shapes can be added.
	 */
	virtual void DebugDraw(ASmartObjectTestingActor& TestingActor, FDebugRenderSceneProxy* DebugProxy) const {}

	/**
	 * Method to override to add element to the 2D canvas.
	 * @param TestingActor A testing actor that will provide a valid SmartObjectSubsystem (i.e. safe to call GetSubsystemRef)*
	 * @param Canvas Canvas where debug text can be added.
	 * @param PlayerController Player controller associated to the debug draw canvas.
	 */
	virtual void DebugDrawCanvas(ASmartObjectTestingActor& TestingActor, UCanvas* Canvas, APlayerController* PlayerController) const {}
#endif

private:
	UPROPERTY(Transient)
	TObjectPtr<ASmartObjectTestingActor> SmartObjectTestingActor;
};


/** Simple test to run a query and draw the results. */
UCLASS()
class SMARTOBJECTSMODULE_API USmartObjectSimpleQueryTest : public USmartObjectTest
{
	GENERATED_BODY()

protected:
	virtual bool Run(ASmartObjectTestingActor& TestingActor) override;
	virtual bool Reset(ASmartObjectTestingActor& TestingActor) override;
	virtual FBox CalcBounds(ASmartObjectTestingActor& TestingActor) const override;

#if UE_ENABLE_DEBUG_DRAWING
	virtual void DebugDraw(ASmartObjectTestingActor& TestingActor, FDebugRenderSceneProxy* DebugProxy) const override;
#endif

	UPROPERTY(EditAnywhere, Category = Test);
	FSmartObjectRequest Request;

	UPROPERTY(VisibleAnywhere, Category = Test, Transient);
	TArray<FSmartObjectRequestResult> Results;
};


/** Debug rendering component for SmartObject tests. */
UCLASS(ClassGroup = Debug, NotPlaceable)
class SMARTOBJECTSMODULE_API USmartObjectTestRenderingComponent : public USmartObjectDebugRenderingComponent
{
	GENERATED_BODY()
protected:
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual void PostInitProperties() override;

#if UE_ENABLE_DEBUG_DRAWING
	virtual void DebugDraw(FDebugRenderSceneProxy* DebugProxy) override;
	virtual void DebugDrawCanvas(UCanvas* Canvas, APlayerController*) override;
#endif
};


/** Debug actor to test SmartObjects. */
UCLASS(HideCategories = (Activation, Actor, AssetUserData, Collision, Cooking, DataLayers, HLOD, Input, Lighting, LOD, Mobile, Mobility, Navigation, Physics, RayTracing, Rendering, Replication, Tags, TextureStreaming, WorldPartition))
class SMARTOBJECTSMODULE_API ASmartObjectTestingActor : public AActor
{
	GENERATED_BODY()
public:
	explicit ASmartObjectTestingActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	FBox CalcTestsBounds() const;
	USmartObjectSubsystem* GetSubsystem() const { return SmartObjectSubsystem; }
	USmartObjectSubsystem& GetSubsystemRef() const { check(SmartObjectSubsystem) return *SmartObjectSubsystem; }

	void ExecuteOnEachTest(TFunctionRef<void(USmartObjectTest&)> ExecFunc);
	void ExecuteOnEachTest(TFunctionRef<void(const USmartObjectTest&)> ExecFunc) const;

protected:

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;

	/** Can be used in Editor world to initialize SmartObject runtime with running the simulation */
	UFUNCTION(CallInEditor, Category = SmartObject, meta = (DisplayName = "Initialize Runtime"))
	void DebugInitializeSubsystemRuntime();

	/** Can be used in Editor world to cleanup SmartObject runtime after using debug initialization */
	UFUNCTION(CallInEditor, Category = SmartObject, meta = (DisplayName = "Cleanup Runtime"))
	void DebugCleanupSubsystemRuntime();
#endif // WITH_EDITOR

	virtual void PostRegisterAllComponents() override;
	virtual bool ShouldTickIfViewportsOnly() const override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = Test)
	void RunTests();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = Test)
	void ResetTests();

	UPROPERTY(EditAnywhere, Category = Test, Instanced)
	TArray<TObjectPtr<USmartObjectTest>> Tests;

	UPROPERTY(Transient)
	TObjectPtr<USmartObjectTestRenderingComponent> RenderingComponent;

	UPROPERTY(Transient)
	TObjectPtr<USmartObjectSubsystem> SmartObjectSubsystem = nullptr;

	UPROPERTY(EditAnywhere, Category = Test)
	bool bRunTestsEachFrame = false;
};
