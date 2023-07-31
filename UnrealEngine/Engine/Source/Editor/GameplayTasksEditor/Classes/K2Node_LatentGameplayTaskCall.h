// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "GameplayTask.h"
#include "HAL/PlatformCrt.h"
#include "K2Node_BaseAsyncTask.h"
#include "Templates/SubclassOf.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "K2Node_LatentGameplayTaskCall.generated.h"

class FBlueprintActionDatabaseRegistrar;
class FName;
class UClass;
class UEdGraph;
class UEdGraphPin;
class UEdGraphSchema;
class UEdGraphSchema_K2;
class UGameplayTask;
class UObject;

UCLASS()
class GAMEPLAYTASKSEDITOR_API UK2Node_LatentGameplayTaskCall : public UK2Node_BaseAsyncTask
{
	GENERATED_BODY()

public:
	UK2Node_LatentGameplayTaskCall(const FObjectInitializer& ObjectInitializer);

	// UEdGraphNode interface
	virtual bool CanCreateUnderSpecifiedSchema(const UEdGraphSchema* DesiredSchema) const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	// End of UEdGraphNode interface

	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	bool ConnectSpawnProperties(UClass* ClassToSpawn, const UEdGraphSchema_K2* Schema, class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UEdGraphPin*& LastThenPin, UEdGraphPin* SpawnedActorReturnPin);		//Helper
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;

	void CreatePinsForClass(UClass* InClass);
	UEdGraphPin* GetClassPin(const TArray<UEdGraphPin*>* InPinsToSearch = NULL) const;
	UClass* GetClassToSpawn(const TArray<UEdGraphPin*>* InPinsToSearch = NULL) const;
	UEdGraphPin* GetResultPin() const;
	bool IsSpawnVarPin(UEdGraphPin* Pin);
	bool ValidateActorSpawning(class FKismetCompilerContext& CompilerContext, bool bGenerateErrors);
	bool ValidateActorArraySpawning(class FKismetCompilerContext& CompilerContext, bool bGenerateErrors);

	UPROPERTY()
	TArray<FName> SpawnParamPins;

	static void RegisterSpecializedTaskNodeClass(TSubclassOf<UK2Node_LatentGameplayTaskCall> NodeClass);
protected:
	static bool HasDedicatedNodeClass(TSubclassOf<UGameplayTask> TaskClass);

	virtual bool IsHandling(TSubclassOf<UGameplayTask> TaskClass) const { return true; }

private:
	static TArray<TWeakObjectPtr<UClass> > NodeClasses;
};
