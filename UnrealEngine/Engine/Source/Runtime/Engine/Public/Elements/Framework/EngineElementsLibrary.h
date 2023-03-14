// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Framework/TypedElementHandle.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EngineElementsLibrary.generated.h"

class UObject;
struct FObjectElementData;

class AActor;
struct FActorElementData;

class UActorComponent;
struct FComponentElementData;

class UInstancedStaticMeshComponent;
struct FSMInstanceElementData;

struct FSMInstanceId;
struct FSMInstanceElementId;

UCLASS()
class ENGINE_API UEngineElementsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UEngineElementsLibrary();

	static TTypedElementOwner<FObjectElementData> CreateObjectElement(const UObject* InObject);
	static void DestroyObjectElement(const UObject* InObject, TTypedElementOwner<FObjectElementData>& InOutObjectElement);
#if WITH_EDITOR
	static void CreateEditorObjectElement(const UObject* Object);
	static void DestroyEditorObjectElement(const UObject* Object);
	static FTypedElementHandle AcquireEditorObjectElementHandle(const UObject* Object, const bool bAllowCreate = true);
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category="TypedElementFramework|Object", meta=(DisplayName="Acquire Editor Object Element Handle", ScriptMethod="AcquireEditorElementHandle"))
	static FScriptTypedElementHandle K2_AcquireEditorObjectElementHandle(const UObject* Object, const bool bAllowCreate = true);
	static void ReplaceEditorObjectElementHandles(const TMap<const UObject*, const UObject*>& ReplacementObjects);
#endif

	static TTypedElementOwner<FActorElementData> CreateActorElement(const AActor* InActor);
	static void DestroyActorElement(const AActor* InActor, TTypedElementOwner<FActorElementData>& InOutActorElement);
#if WITH_EDITOR
	static void CreateEditorActorElement(const AActor* Actor);
	static void DestroyEditorActorElement(const AActor* Actor);
	static FTypedElementHandle AcquireEditorActorElementHandle(const AActor* Actor, const bool bAllowCreate = true);
	UFUNCTION(BlueprintCallable, Category="TypedElementFramework|Actor", meta=(DisplayName="Acquire Editor Actor Element Handle", ScriptMethod="AcquireEditorElementHandle"))
	static FScriptTypedElementHandle K2_AcquireEditorActorElementHandle(const AActor* Actor, const bool bAllowCreate = true);
	static void ReplaceEditorActorElementHandles(const TMap<const AActor*, const AActor*>& ReplacementActors);
#endif

	static TTypedElementOwner<FComponentElementData> CreateComponentElement(const UActorComponent* InComponent);
	static void DestroyComponentElement(const UActorComponent* InComponent, TTypedElementOwner<FComponentElementData>& InOutComponentElement);
#if WITH_EDITOR
	static void CreateEditorComponentElement(const UActorComponent* Component);
	static void DestroyEditorComponentElement(const UActorComponent* Component);
	static FTypedElementHandle AcquireEditorComponentElementHandle(const UActorComponent* Component, const bool bAllowCreate = true);
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category="TypedElementFramework|Component", meta=(DisplayName="Acquire Editor Component Element Handle", ScriptMethod = "AcquireEditorElementHandle"))
	static FScriptTypedElementHandle K2_AcquireEditorComponentElementHandle(const UActorComponent* Component, const bool bAllowCreate = true);
	static void ReplaceEditorComponentElementHandles(const TMap<const UActorComponent*, const UActorComponent*>& ReplacementComponents);
#endif

	static TTypedElementOwner<FSMInstanceElementData> CreateSMInstanceElement(const FSMInstanceId& InSMInstanceId);
	static void DestroySMInstanceElement(const FSMInstanceElementId& InSMInstanceElementId, TTypedElementOwner<FSMInstanceElementData>& InOutSMInstanceElement);
#if WITH_EDITOR
	static void CreateEditorSMInstanceElement(const FSMInstanceId& SMInstanceId);
	static void DestroyEditorSMInstanceElement(const FSMInstanceElementId& SMInstanceElementId);
	static FTypedElementHandle AcquireEditorSMInstanceElementHandle(const UInstancedStaticMeshComponent* ISMComponent, const int32 InstanceIndex, const bool bAllowCreate = true);
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category="TypedElementFramework|StaticMeshInstance", meta=(DisplayName="Acquire Editor SMInstance Element Handle", ScriptMethod = "AcquireEditorElementHandle"))
	static FScriptTypedElementHandle K2_AcquireEditorSMInstanceElementHandle(const UInstancedStaticMeshComponent* ISMComponent, const int32 InstanceIndex, const bool bAllowCreate = true);
	static FTypedElementHandle AcquireEditorSMInstanceElementHandle(const FSMInstanceId& SMInstanceId, const bool bAllowCreate = true);
	static FTypedElementHandle AcquireEditorSMInstanceElementHandle(const FSMInstanceElementId& SMInstanceElementId);
	static void ReplaceEditorSMInstanceElementHandles(const TMap<FSMInstanceId, FSMInstanceId>& ReplacementSMInstanceIds);
#endif

private:
	static TTypedElementOwner<FSMInstanceElementData> CreateSMInstanceElementImpl(const FSMInstanceId& InSMInstanceId, const FSMInstanceElementId& InSMInstanceElementId);
#if WITH_EDITOR
	static void DestroyUnreachableEditorObjectElements();
	static void OnObjectsReplaced(const TMap<UObject*, UObject*>& InReplacementObjects);
	static void ReplaceEditorObjectElementHandlesImpl(const TMap<const UObject*, const UObject*>& ReplacementObjects, TArray<FTypedElementHandle>& OutUpdatedElements);
	static void ReplaceEditorActorElementHandlesImpl(const TMap<const AActor*, const AActor*>& ReplacementActors, TArray<FTypedElementHandle>& OutUpdatedElements);
	static void ReplaceEditorComponentElementHandlesImpl(const TMap<const UActorComponent*, const UActorComponent*>& ReplacementComponents, TArray<FTypedElementHandle>& OutUpdatedElements);
	static void ReplaceEditorSMInstanceElementHandlesImpl(const TMap<FSMInstanceElementId, FSMInstanceElementId>& ReplacementSMInstanceIds, TArray<FTypedElementHandle>& OutUpdatedElements);
#endif
};
