// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "IObjectChooser.h"
#include "InstancedStruct.h"
#include "IChooserColumn.h"

#include "Chooser.generated.h"


UCLASS(MinimalAPI, BlueprintType)
class UChooserTable : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	UChooserTable() {}
	
	// deprecated UObject Results
	UPROPERTY()
	TArray<TScriptInterface<IObjectChooser>> Results_DEPRECATED;
	
	// deprecated UObject Columns
	UPROPERTY()
	TArray<TScriptInterface<IChooserColumn>> Columns_DEPRECATED;

	// Each possible Result (Rows of chooser table)
	UPROPERTY(EditAnywhere, DisplayName = "Results", Meta = (ExcludeBaseStruct, BaseStruct = "/Script/Chooser.ObjectChooserBase"), Category = "Hidden")
	TArray<FInstancedStruct> ResultsStructs;

	// Columns which filter Results
	UPROPERTY(EditAnywhere, DisplayName = "Columns", Category = Hidden, meta = (ExcludeBaseStruct, BaseStruct = "/Script/Chooser.ChooserColumnBase"))
	TArray<FInstancedStruct> ColumnsStructs;

	UPROPERTY(EditAnywhere, Category="Input", Meta = (AllowAbstract=true))
	TObjectPtr<UClass> ContextObjectType;
	
	UPROPERTY(EditAnywhere, Category="Output", Meta = (AllowAbstract=true))
	TObjectPtr<UClass> OutputObjectType;

#if WITH_EDITOR
	virtual void PostLoad() override;
#endif
};


USTRUCT(DisplayName = "Evaluate Chooser")
struct CHOOSER_API FEvaluateChooser : public FObjectChooserBase
{
	GENERATED_BODY()

	virtual UObject* ChooseObject(const UObject* ContextObject) const final override;
	virtual EIteratorStatus ChooseMulti(const UObject* ContextObject, FObjectChooserIteratorCallback Callback) const final override;
	public:
	
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