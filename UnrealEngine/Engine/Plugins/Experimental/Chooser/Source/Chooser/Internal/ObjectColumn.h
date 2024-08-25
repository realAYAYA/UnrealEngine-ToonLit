// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ChooserPropertyAccess.h"
#include "CoreMinimal.h"
#include "IChooserColumn.h"
#include "IChooserParameterObject.h"
#include "InstancedStruct.h"
#include "Serialization/MemoryReader.h"
#include "ObjectColumn.generated.h"

struct FBindingChainElement;

USTRUCT(DisplayName = "Object Property Binding")
struct CHOOSER_API FObjectContextProperty : public FChooserParameterObjectBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Meta = (BindingType = "object", BindingColor = "ObjectPinTypeColor"), Category = "Binding")
	FChooserObjectPropertyBinding Binding;

	virtual bool GetValue(FChooserEvaluationContext& Context, FSoftObjectPath& OutResult) const override;

	CHOOSER_PARAMETER_BOILERPLATE();

#if WITH_EDITOR
	virtual UClass* GetAllowedClass() const override { return Binding.AllowedClass; }
#endif
};

UENUM()
enum class EObjectColumnCellValueComparison
{
	MatchEqual,
	MatchNotEqual,
	MatchAny,

	Modulus // used for cycling through the other values
};

USTRUCT()
struct FChooserObjectRowData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Runtime, Meta = (ValidEnumValues = "MatchEqual, MatchNotEqual, MatchAny"))
	EObjectColumnCellValueComparison Comparison = EObjectColumnCellValueComparison::MatchEqual;

	UPROPERTY(EditAnywhere, Category = "Runtime")
	TSoftObjectPtr<UObject> Value;

	bool Evaluate(const FSoftObjectPath& LeftHandSide) const;
};

USTRUCT()
struct CHOOSER_API FObjectColumn : public FChooserColumnBase
{
	GENERATED_BODY()

	FObjectColumn();

	UPROPERTY(EditAnywhere, NoClear, Meta = (ExcludeBaseStruct, BaseStruct = "/Script/Chooser.ChooserParameterObjectBase"), Category = "Data")
	FInstancedStruct InputValue;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Data")
	FChooserObjectRowData DefaultRowValue;
#endif

	UPROPERTY(EditAnywhere, Category = "Data")
	// array of results (cells for this column for each row in the table)
	// should match the length of the Results array
	TArray<FChooserObjectRowData> RowValues;

	virtual void Filter(FChooserEvaluationContext& Context, const FChooserIndexArray& IndexListIn, FChooserIndexArray& IndexListOut) const override;

#if WITH_EDITOR
	mutable FSoftObjectPath TestValue;
	virtual bool EditorTestFilter(int32 RowIndex) const override
	{
		return RowValues.IsValidIndex(RowIndex) && RowValues[RowIndex].Evaluate(TestValue);
	}
	virtual void SetTestValue(TArrayView<const uint8> Value) override
	{
		FMemoryReaderView Reader(Value);
		FString Path;
		Reader << Path;
		TestValue.SetPath(Path);
	}

	virtual void AddToDetails(FInstancedPropertyBag& PropertyBag, int32 ColumnIndex, int32 RowIndex) override;
	virtual void SetFromDetails(FInstancedPropertyBag& PropertyBag, int32 ColumnIndex, int32 RowIndex) override;
#endif

	CHOOSER_COLUMN_BOILERPLATE(FChooserParameterObjectBase);

#if WITH_EDITOR
	virtual void PostLoad() override
	{
		Super::PostLoad();

		if (InputValue.IsValid())
		{
			InputValue.GetMutable<FChooserParameterBase>().PostLoad();
		}
	}
#endif
};
