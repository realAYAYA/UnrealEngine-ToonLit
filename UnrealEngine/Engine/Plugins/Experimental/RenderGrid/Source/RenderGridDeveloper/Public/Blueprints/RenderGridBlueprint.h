// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Blueprint.h"
#include "RenderGridBlueprint.generated.h"


/**
 * A UBlueprint child class for the RenderGrid modules.
 *
 * Required in order for a RenderGrid to be able to have a blueprint graph.
 */
UCLASS(BlueprintType, Meta = (IgnoreClassThumbnail))
class RENDERGRIDDEVELOPER_API URenderGridBlueprint : public UBlueprint
{
	GENERATED_BODY()

public:
	//~ Begin UBlueprint Interface
	virtual bool SupportedByDefaultBlueprintFactory() const override { return false; }
	virtual bool IsValidForBytecodeOnlyRecompile() const override { return false; }
	virtual bool SupportsGlobalVariables() const override { return true; }
	virtual bool SupportsLocalVariables() const override { return true; }
	virtual bool SupportsFunctions() const override { return true; }
	virtual bool SupportsMacros() const override { return true; }
	virtual bool SupportsDelegates() const override { return true; }
	virtual bool SupportsEventGraphs() const override { return true; }
	virtual bool SupportsAnimLayers() const override { return false; }

	virtual UClass* GetBlueprintClass() const override;
	virtual void PostLoad() override;
	//~ End UBlueprint Interface

private:
	virtual void OnPreVariablesChange(UObject* InObject);
	virtual void OnPostVariablesChange(UBlueprint* InBlueprint);

protected:
	virtual void OnVariableAdded(FBPVariableDescription& InVar);
	virtual void OnVariableRemoved(FBPVariableDescription& InVar) {}
	virtual void OnVariableRenamed(FBPVariableDescription& InVar, const FName& InOldVarName, const FName& InNewVarName) {}
	virtual void OnVariableTypeChanged(FBPVariableDescription& InVar, const FEdGraphPinType& InOldVarType, const FEdGraphPinType& InNewVarType) {}
	virtual void OnVariablePropertyFlagsChanged(FBPVariableDescription& InVar, const uint64 InOldVarPropertyFlags, const uint64 InNewVarPropertyFlags);

protected:
	virtual void MakeVariableTransientUnlessInstanceEditable(FBPVariableDescription& InVar);

private:
	TArray<FBPVariableDescription> LastNewVariables;
};
