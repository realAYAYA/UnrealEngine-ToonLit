// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "IObjectChooser.h"
#include "InstancedStruct.h"
#include "IChooserColumn.h"
#include "ChooserPropertyAccess.h"

#include "Chooser.generated.h"

UCLASS(BlueprintType)
class CHOOSER_API UChooserTable : public UObject, public IHasContextClass
{
	GENERATED_UCLASS_BODY()
public:
	UChooserTable() {}

#if WITH_EDITOR
	FChooserOutputObjectTypeChanged OnOutputObjectTypeChanged;
	
	virtual void PostEditUndo() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostLoad() override;
	
	void SetDebugSelectedRow(int32 Index) const { DebugSelectedRow = Index; }
	int32 GetDebugSelectedRow() const { return DebugSelectedRow; }
	bool HasDebugTarget() const { return DebugTarget != nullptr; }
	const UObject* GetDebugTarget() const { return DebugTarget.Get(); }
	void SetDebugTarget(TWeakObjectPtr<const UObject> Target) { DebugTarget = Target; }
	void ResetDebugTarget() { DebugTarget.Reset(); }
	void IterateRecentContextObjects(TFunction<void(const UObject*)> Callback) const;
	void UpdateDebugging(FChooserEvaluationContext& Context) const;
	
	// enable display of which cells pass/fail based on current TestValue for each column
	bool bEnableDebugTesting = false;
	mutable bool bDebugTestValuesValid = false;

private: 
	// caching the OutputObjectType and ContextObjectType so that on Undo, we can tell if we should fire the changed delegate
	UClass* CachedPreviousOutputObjectType = nullptr;
	EObjectChooserResultType CachedPreviousResultType = EObjectChooserResultType::ObjectResult;
	
	// objects this chooser has been recently evaluated on
	mutable TSet<TWeakObjectPtr<const UObject>> RecentContextObjects;
	mutable FCriticalSection DebugLock;
	// reference to the UObject in PIE  which we want to get debug info for
	TWeakObjectPtr<const UObject> DebugTarget;
	// Row which was selected last time this chooser was evaluated on DebugTarget
	mutable int32 DebugSelectedRow = -1;
#endif

public:
#if WITH_EDITORONLY_DATA
	// deprecated UObject Results
	UPROPERTY()
	TArray<TScriptInterface<IObjectChooser>> Results_DEPRECATED;

	// deprecated single context object
	UPROPERTY()
	TObjectPtr<UClass> ContextObjectType_DEPRECATED;
	
	// deprecated UObject Columns
	UPROPERTY()
	TArray<TScriptInterface<IChooserColumn>> Columns_DEPRECATED;
#endif

	// Each possible Result (Rows of chooser table)
	UPROPERTY(EditAnywhere, NoClear, DisplayName = "Results", Meta = (ExcludeBaseStruct, BaseStruct = "/Script/Chooser.ObjectChooserBase"), Category = "Hidden")
	TArray<FInstancedStruct> ResultsStructs;

	// Columns which filter Results
	UPROPERTY(EditAnywhere, NoClear, DisplayName = "Columns", Category = Hidden, meta = (ExcludeBaseStruct, BaseStruct = "/Script/Chooser.ChooserColumnBase"))
	TArray<FInstancedStruct> ColumnsStructs;
	
	UPROPERTY(EditAnywhere, NoClear, Meta = (ExcludeBaseStruct, BaseStruct = "/Script/Chooser.ContextObjectTypeBase"), Category = "Input")
	TArray<FInstancedStruct> ContextData;

	UPROPERTY(EditAnywhere, Category="Output", Meta = (AllowAbstract=true))
	TObjectPtr<UClass> OutputObjectType;
	UPROPERTY(EditAnywhere, Category="Output")
	EObjectChooserResultType ResultType = EObjectChooserResultType::ObjectResult;

	virtual TConstArrayView<FInstancedStruct> GetContextData() const override { return ContextData; }
	
	static FObjectChooserBase::EIteratorStatus EvaluateChooser(FChooserEvaluationContext& Context, const UChooserTable* Chooser, FObjectChooserBase::FObjectChooserIteratorCallback Callback);
};


USTRUCT(BlueprintType, DisplayName = "Evaluate Chooser")
struct CHOOSER_API FEvaluateChooser : public FObjectChooserBase
{
	GENERATED_BODY()

	virtual UObject* ChooseObject(FChooserEvaluationContext& Context) const final override;
	virtual EIteratorStatus ChooseMulti(FChooserEvaluationContext &Context, FObjectChooserIteratorCallback Callback) const final override;
	public:

	FEvaluateChooser() : Chooser(nullptr) {}
	FEvaluateChooser(TObjectPtr<UChooserTable> Table) : Chooser(Table) {}
	
	UPROPERTY(EditAnywhere, Category="Parameters")
	TObjectPtr<UChooserTable> Chooser;
};

// Deprecated class for converting old data
UCLASS(ClassGroup = "LiveLink", deprecated)
class CHOOSER_API UDEPRECATED_ObjectChooser_EvaluateChooser : public UObject, public IObjectChooser
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Parameters")
	TObjectPtr<UChooserTable> Chooser;

	virtual void ConvertToInstancedStruct(FInstancedStruct& OutInstancedStruct) const
	{
		OutInstancedStruct.InitializeAs(FEvaluateChooser::StaticStruct());
		FEvaluateChooser& AssetChooser = OutInstancedStruct.GetMutable<FEvaluateChooser>();
		AssetChooser.Chooser = Chooser;
	}
};


UCLASS()
class CHOOSER_API UChooserColumnMenuContext : public UObject
{
	GENERATED_BODY()
public:
	class FAssetEditorToolkit* Editor;
	TWeakObjectPtr<UChooserTable> Chooser;
	int ColumnIndex;
};