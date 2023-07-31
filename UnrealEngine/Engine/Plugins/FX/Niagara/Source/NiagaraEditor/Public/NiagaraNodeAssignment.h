// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "NiagaraEditorCommon.h"
#include "NiagaraNode.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeAssignment.generated.h"

class UNiagaraScript;

UCLASS()
class NIAGARAEDITOR_API UNiagaraNodeAssignment : public UNiagaraNodeFunctionCall
{
	GENERATED_BODY()

public:
	int32 NumTargets() const { return AssignmentTargets.Num(); }

	int32 FindAssignmentTarget(const FName& InName);
	int32 FindAssignmentTarget(const FName& InName, const FNiagaraTypeDefinition& TypeDef);

	/** Set the assignment target and default value. This does not call RefreshFromExternalChanges, which you will need to do to update the internal graph. Returns true if values were changed.*/
	bool SetAssignmentTarget(int32 Idx, const FNiagaraVariable& InVar, const FString* InDefaultValue = nullptr);
	int32 AddAssignmentTarget(const FNiagaraVariable& InVar, const FString* InDefaultValue = nullptr);

	/** This will rename an existing input but will not refresh the underlying script.  RefreshFromExternalChanges must be called if this function returns true. */
	bool RenameAssignmentTarget(FName OldName, FName NewName);

	const FNiagaraVariable& GetAssignmentTarget(int32 Idx) const { return AssignmentTargets[Idx]; }

	const TArray<FNiagaraVariable>& GetAssignmentTargets() const { return AssignmentTargets; }
	const TArray<FString>& GetAssignmentDefaults() const { return AssignmentDefaultValues; }

	//~ Begin EdGraphNode Interface
	virtual void PostLoad() override;
#if WITH_EDITORONLY_DATA
	static void DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass);
#endif
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void AllocateDefaultPins() override;

	//~ UNiagaraNodeFunctionCall interface
	virtual bool RefreshFromExternalChanges() override;
	virtual void BuildParameterMapHistory(FNiagaraParameterMapHistoryBuilder& OutHistory, bool bRecursive = true, bool bFilterForCompilation = true) const override;
	virtual void GatherExternalDependencyData(ENiagaraScriptUsage InUsage, const FGuid& InUsageId, TArray<FNiagaraCompileHash>& InReferencedCompileHashes, TArray<FString>& InReferencedObjs) const override;

	void AddParameter(FNiagaraVariable InVar, FString InDefaultValue);
	void RemoveParameter(const FNiagaraVariable& InVar);

	void UpdateUsageBitmaskFromOwningScript();

	virtual FText GetTooltipText() const override;

	FSimpleMulticastDelegate& OnAssignmentTargetsChanged() { return AssignmentTargetsChangedDelegate; }

	TSharedRef<SWidget> CreateAddParameterMenu(const TSharedPtr<SComboButton>& AddButton);

protected:

	UPROPERTY()
	FNiagaraVariable AssignmentTarget_DEPRECATED;

	UPROPERTY()
	FString AssignmentDefaultValue_DEPRECATED;

	UPROPERTY()
	TArray<FNiagaraVariable> AssignmentTargets;

	UPROPERTY()
	TArray<FString> AssignmentDefaultValues;

	UPROPERTY()
	FString OldFunctionCallName;

	FText Title;

	FSimpleMulticastDelegate AssignmentTargetsChangedDelegate;

private:
	void GenerateScript();

	void InitializeScript(UNiagaraScript* NewScript);

	int32 CalculateScriptUsageBitmask();

	void RefreshTitle();
};

