// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IChooserColumn.h"
#include "IChooserParameterEnum.h"
#include "InstancedStruct.h"
#include "EnumColumn.generated.h"

struct FBindingChainElement;

USTRUCT(DisplayName = "Enum Property Binding")
struct CHOOSER_API FEnumContextProperty : public FChooserParameterEnumBase
{
	GENERATED_BODY()
	
	UPROPERTY()
	TArray<FName> PropertyBindingChain;

	virtual bool GetValue(const UObject* ContextObject, uint8& OutResult) const override;

#if WITH_EDITOR
	static bool CanBind(const FProperty& Property)
	{
		if (Property.IsA<FEnumProperty>())
		{
			return true;
		}

		if (const FByteProperty* ByteProperty = CastField<FByteProperty>(&Property))
		{
			return ByteProperty->Enum != nullptr;
		}

		return false;
	}

	void SetBinding(const TArray<FBindingChainElement>& InBindingChain);

	virtual void GetDisplayName(FText& OutName) const override
	{
		if (!PropertyBindingChain.IsEmpty())
		{
			OutName = FText::FromName(PropertyBindingChain.Last());
		}
	}

	virtual const UEnum* GetEnum() const override { return Enum; }
#endif

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<const UEnum> Enum = nullptr;
#endif
};



UENUM()
enum class EChooserEnumComparison : uint8
{
	Equal UMETA(DisplayName = "Value =="),
	NotEqual UMETA(DisplayName = "Value !="),
	GreaterThan UMETA(DisplayName = "Value >"),
	GreaterThanEqual UMETA(DisplayName = "Value >="),
	LessThan UMETA(DisplayName = "Value <"),
	LessThanEqual UMETA(DisplayName = "Value <="),
};

USTRUCT()
struct FChooserEnumRowData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Runtime)
	EChooserEnumComparison Comparison = EChooserEnumComparison::Equal;

	UPROPERTY(EditAnywhere, Category = Runtime)
	uint8 Value = 0;

	bool Evaluate(const uint8 LeftHandSide) const;
};


USTRUCT()
struct CHOOSER_API FEnumColumn : public FChooserColumnBase
{
	GENERATED_BODY()
public:
	FEnumColumn();

	UPROPERTY(EditAnywhere, Meta = (ExcludeBaseStruct, BaseStruct = "/Script/Chooser.ChooserParameterEnumBase"), Category = "Hidden")
	FInstancedStruct InputValue;

	UPROPERTY(EditAnywhere, Category = Runtime)
	// array of results (cells for this column for each row in the table)
	// should match the length of the Results array
	TArray<FChooserEnumRowData> RowValues;

	virtual void Filter(const UObject* ContextObject, const TArray<uint32>& IndexListIn, TArray<uint32>& IndexListOut) const override;
	
	CHOOSER_COLUMN_BOILERPLATE_NoSetInputType(FChooserParameterEnumBase);
	
#if WITH_EDITOR
	virtual void SetInputType(const UScriptStruct* Type) override
	{
		InputValue.InitializeAs(Type);
		InputChanged();
	};

	FSimpleMulticastDelegate OnEnumChanged;
	
	void InputChanged()
	{
		if (InputValue.IsValid())
		{
			InputValue.GetMutable<FChooserParameterEnumBase>().OnEnumChanged.AddLambda([this](){ OnEnumChanged.Broadcast(); });
		}
		OnEnumChanged.Broadcast();
	}

	virtual void PostLoad() override
	{
		Super::PostLoad();
		InputChanged();
	}
#endif
};

// deprecated class version for converting old data
UCLASS(ClassGroup = "LiveLink", deprecated)
class CHOOSER_API UDEPRECATED_ChooserParameterEnum_ContextProperty : public UObject, public IChooserParameterEnum
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TArray<FName> PropertyBindingChain;
	
	virtual void ConvertToInstancedStruct(FInstancedStruct& OutInstancedStruct) const override
	{
		OutInstancedStruct.InitializeAs(FEnumContextProperty::StaticStruct());
		FEnumContextProperty& Property = OutInstancedStruct.GetMutable<FEnumContextProperty>();
		Property.PropertyBindingChain = PropertyBindingChain;
	}
};

UCLASS(ClassGroup = "LiveLink", deprecated)
class CHOOSER_API UDEPRECATED_ChooserColumnEnum : public UObject, public IChooserColumn
{
	GENERATED_BODY()
public:
	UDEPRECATED_ChooserColumnEnum() = default;
	UDEPRECATED_ChooserColumnEnum(const FObjectInitializer& ObjectInitializer)
	{
		InputValue = ObjectInitializer.CreateDefaultSubobject<UDEPRECATED_ChooserParameterEnum_ContextProperty>(this, "InputValue");
	}	

	UPROPERTY(EditAnywhere, Category = "Input")
	TScriptInterface<IChooserParameterEnum> InputValue;

	UPROPERTY(EditAnywhere, Category = Runtime)
	TArray<FChooserEnumRowData> RowValues;
	
	virtual void ConvertToInstancedStruct(FInstancedStruct& OutInstancedStruct) const override
	{
		OutInstancedStruct.InitializeAs(FEnumColumn::StaticStruct());
		FEnumColumn& Column = OutInstancedStruct.GetMutable<FEnumColumn>();
		if (IChooserParameterEnum* InputValueInterface = InputValue.GetInterface())
		{
			InputValueInterface->ConvertToInstancedStruct(Column.InputValue);
		}
		Column.RowValues = RowValues;
	}
};