// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/MaterialStageBlends/DMMSBNormal.h"
#include "Templates/SharedPointer.h"
#include "UObject/Object.h"
#include "Widgets/SCompoundWidget.h"
#include "SDMSourceBlendType.generated.h"

class SDMSourceBlendType;
class UDMMaterialStageBlend;
class UToolMenu;

UCLASS(MinimalAPI)
class UDMSourceBlendTypeContextObject : public UObject
{
	GENERATED_BODY()
	
public:
	UDMSourceBlendTypeContextObject() = default;
	
	TSharedPtr<SDMSourceBlendType> GetBlendTypeWidget() const { return BlendTypeWidgetWeak.Pin(); }
	void SetBlendTypeWidget(const TSharedPtr<SDMSourceBlendType>& InBlendTypeWidget) { BlendTypeWidgetWeak = InBlendTypeWidget; }
	
private:
	TWeakPtr<SDMSourceBlendType> BlendTypeWidgetWeak;
};

DECLARE_DELEGATE_OneParam(FDMOnSourceBlendTypeChanged, const TSubclassOf<UDMMaterialStageBlend>)

struct FDMBlendNameClass
{
	FText BlendName;
	TSubclassOf<UDMMaterialStageBlend> BlendClass;
};

class SDMSourceBlendType : public SCompoundWidget
{
public:
	static TSharedRef<SWidget> MakeSourceBlendMenu(TAttribute<TSubclassOf<UDMMaterialStageBlend>> InSelectedItem, FDMOnSourceBlendTypeChanged InOnSelectedItemChanged);

	SLATE_BEGIN_ARGS(SDMSourceBlendType)
		: _SelectedItem(UDMMaterialStageBlendNormal::StaticClass())
		{}
		SLATE_ATTRIBUTE(TSubclassOf<UDMMaterialStageBlend>, SelectedItem)
		SLATE_EVENT(FDMOnSourceBlendTypeChanged, OnSelectedItemChanged)
	SLATE_END_ARGS()

	virtual ~SDMSourceBlendType() = default;

	void Construct(const FArguments& InArgs);

protected:
	static TArray<TStrongObjectPtr<UClass>> SupportedBlendClasses;
	static TMap<FName, FDMBlendNameClass> BlendMap;

	static void EnsureBlendMap();
	static void EnsureMenuRegistered();
	static void MakeSourceBlendMenu(UToolMenu* InToolMenu);

	TAttribute<TSubclassOf<UDMMaterialStageBlend>> SelectedItem;
	FDMOnSourceBlendTypeChanged OnSelectedItemChanged;

	TSharedRef<SWidget> OnGenerateWidget(const FName InItem);

	void OnSelectionChanged(const FName InNewItem, const ESelectInfo::Type InSelectInfoType);

	FText GetSelectedItemText() const;

	TSharedRef<SWidget> MakeSourceBlendMenuWidget();

	void OnBlendTypeSelected(UClass* InBlendClass);
	bool CanSelectBlendType(UClass* InBlendClass);
	bool InBlendTypeSelected(UClass* InBlendClass);
};
