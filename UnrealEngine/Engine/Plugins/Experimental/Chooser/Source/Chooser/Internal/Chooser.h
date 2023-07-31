// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "IDataInterface.h"
#include "DataInterface_Object.h"
#include "Chooser.generated.h"

UINTERFACE(NotBlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class CHOOSER_API UChooserColumn : public UInterface
{
	GENERATED_BODY()
};

class CHOOSER_API IChooserColumn 
{
	GENERATED_BODY()

public:
	virtual void Filter(const UE::DataInterface::FContext& Context, const TArray<uint32>& IndexListIn, TArray<uint32>& IndexListOut) {};
	virtual FName GetDisplayName() { return "ChooserColumn"; }
	virtual void SetDisplayName(FName Name) { }
	virtual void SetNumRows(uint32 NumRows) {}
	virtual void DeleteRows(const TArray<uint32> & RowIndices) {}
};

UCLASS()
class CHOOSER_API UChooserColumnBool : public UObject, public IChooserColumn
{
	GENERATED_BODY()
	public:
	UChooserColumnBool() { }
	
	UPROPERTY(EditAnywhere, Category = "Editor")
	FName DisplayName = "Bool Column";
	
	UPROPERTY(EditAnywhere, Meta=(DataInterfaceType="bool"), Category = "Input")
	TScriptInterface<IDataInterface> Value;

	UPROPERTY(EditAnywhere, Category=Runtime)
	// array of results (cells for this column for each row in the table)
	// should match the length of the Results array 
	TArray<bool> RowValues;
	
	virtual void Filter(const UE::DataInterface::FContext& Context, const TArray<uint32>& IndexListIn, TArray<uint32>& IndexListOut) override;

	// todo: macro boilerplate
	virtual void SetNumRows(uint32 NumRows) override { RowValues.SetNum(NumRows); }
	virtual FName GetDisplayName() override { return DisplayName; }
	virtual void SetDisplayName(FName Name) override { DisplayName = Name; }
	virtual void DeleteRows(const TArray<uint32> & RowIndices )
	{
		for(uint32 Index : RowIndices)
		{
			RowValues.RemoveAt(Index);
		}
	}
};

USTRUCT()
struct FChooserFloatRangeRowData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category=Runtime)
	float Min=0;
	
	UPROPERTY(EditAnywhere, Category=Runtime)
	float Max=0;
};

UCLASS()
class CHOOSER_API UChooserColumnFloatRange : public UObject, public IChooserColumn
{
	GENERATED_BODY()
	public:
	UChooserColumnFloatRange() { }
	
	UPROPERTY(EditAnywhere, Category = "Editor")
	FName DisplayName = "Float Range Column";
	
	UPROPERTY(EditAnywhere, Meta=(DataInterfaceType="float"), Category = "Input")
	TScriptInterface<IDataInterface> Value;

	UPROPERTY(EditAnywhere, Category=Runtime)
	// array of results (cells for this column for each row in the table)
	// should match the length of the Results array 
	TArray<FChooserFloatRangeRowData> RowValues;
	
	virtual void Filter(const UE::DataInterface::FContext& Context, const TArray<uint32>& IndexListIn, TArray<uint32>& IndexListOut) override;
	virtual void SetNumRows(uint32 NumRows) { RowValues.SetNum(NumRows); }
	virtual FName GetDisplayName() override { return DisplayName; }
	virtual void SetDisplayName(FName Name) override { DisplayName = Name; }
	virtual void DeleteRows(const TArray<uint32> & RowIndices )
	{
		for(uint32 Index : RowIndices)
		{
			RowValues.RemoveAt(Index);
		}
	}
};

UCLASS(MinimalAPI)
class UChooserTable : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	UChooserTable() {}
	
	UPROPERTY(EditAnywhere, Meta=(EditInline="true"), Category = "Editor")
	FName ResultType = "Object"; // todo: drop-down, populated by all possible return type names from all implemented data interfaces
	// todo: struct type for Object return types

	UPROPERTY(EditAnywhere, Meta=(EditInline="true"), Category = "Runtime")
	TArray<TScriptInterface<IChooserColumn>> Columns;

	// array of results (rows of table)
	// todo: DataInterfaceType shouldn't be hard coded (Should be based on Result Type above) needed for Details customization to work
	UPROPERTY(EditAnywhere, Meta=(DataInterfaceType="Asset"), Category = "Runtime")
	TArray<TScriptInterface<IDataInterface>> Results;
};

UCLASS()
class CHOOSER_API UDataInterface_EvaluateChooser : public UObject, public IDataInterface
{
	GENERATED_BODY()

	// virtual bool GetObject(const UE::DataInterface::FContext& DataContext) const final override;

	/** Get the return type name, used for dynamic type checking */
    virtual FName GetReturnTypeNameImpl() const { return Chooser ? Chooser->ResultType : NAME_None; } 

    /** Get the return type struct, used for dynamic type checking. If the result is not a struct, this should return nullptr */
    virtual const UScriptStruct* GetReturnTypeStructImpl() const { return nullptr; } // todo, get type struct from chooser once it's added
    
    /** Get the value for this interface. @return true if successful, false if unsuccessful. */
    virtual bool GetDataImpl(const UE::DataInterface::FContext& Context) const;

	public:
	
	UPROPERTY(EditAnywhere, Category="Parameters")
	TObjectPtr<UChooserTable> Chooser;
};