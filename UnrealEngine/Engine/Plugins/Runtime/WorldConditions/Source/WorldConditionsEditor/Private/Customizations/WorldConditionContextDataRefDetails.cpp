// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldConditionContextDataRefDetails.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SComboButton.h"
#include "DetailLayoutBuilder.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SComboButton.h"
#include "WorldConditionQuery.h"
#include "WorldConditionSchema.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "WorldCondition"

TSharedRef<IPropertyTypeCustomization> FWorldConditionContextDataRefDetails::MakeInstance()
{
	return MakeShareable(new FWorldConditionContextDataRefDetails);
}

void FWorldConditionContextDataRefDetails::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructProperty = StructPropertyHandle;
	PropUtils = StructCustomizationUtils.GetPropertyUtilities().Get();

	CacheContextData();
	
	HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.VAlign(VAlign_Center)
		[
			SNew(SComboButton)
			.OnGetMenuContent(this, &FWorldConditionContextDataRefDetails::OnGetContent)
			.ContentPadding(FMargin(6.f, 0.f))
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(this, &FWorldConditionContextDataRefDetails::GetCurrentDesc)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		];
}

void FWorldConditionContextDataRefDetails::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}

void FWorldConditionContextDataRefDetails::CacheContextData()
{
	// Base struct
	static const FName BaseStructMetaName(TEXT("BaseStruct")); // TODO: move these names into one central place.
	static const FName BaseClassMetaName(TEXT("BaseClass")); // TODO: move these names into one central place.

	const FString BaseStructName = StructProperty->GetMetaData(BaseStructMetaName);
	const FString BaseClassName = StructProperty->GetMetaData(BaseClassMetaName);
	
	if (!BaseStructName.IsEmpty())
	{
		BaseStruct = UClass::TryFindTypeSlow<UScriptStruct>(BaseStructName);
	}

	if (BaseStruct == nullptr)
	{
		if (!BaseClassName.IsEmpty())
		{
			BaseStruct = UClass::TryFindTypeSlow<UClass>(BaseClassName);
		}
	}

	if (!BaseStruct)
	{
		check(StructProperty.IsValid() && StructProperty->IsValidHandle())
		UE_LOG(LogWorldCondition, Error, TEXT("%s: Could not find BaseStruct '%s' nor BaseClass '%s' based on the property metadata, expecting full struct name."),
			*FString(StructProperty->GetPropertyPath()), *BaseStructName, *BaseClassName);
	}

	// Find schema from outer FWorldConditionQueryDefinition.
	Schema = nullptr;
	
	TSharedPtr<IPropertyHandle> CurrentProperty = StructProperty;
	while (CurrentProperty.IsValid() && !Schema)
	{
		const FProperty* Property = CurrentProperty->GetProperty();
		if (const FStructProperty* CurrentStructProperty = CastField<FStructProperty>(Property))
		{
			if (CurrentStructProperty->Struct == TBaseStructure<FWorldConditionQueryDefinition>::Get())
			{
				// Get schema from definition
				TArray<void*> RawNodeData;
				CurrentProperty->AccessRawData(RawNodeData);
				for (void* Data : RawNodeData)
				{
					if (const FWorldConditionQueryDefinition* QueryDefinition = static_cast<FWorldConditionQueryDefinition*>(Data))
					{
						Schema = QueryDefinition->GetSchemaClass().GetDefaultObject();
						if (Schema)
						{
							break;
						}
					}
				}
			}
		}

		CurrentProperty = CurrentProperty->GetParentHandle();
	}

	if (Schema && BaseStruct)
	{
		CachedContextData.Reset();
		for (const FWorldConditionContextDataDesc& Desc : Schema->GetContextDataDescs())
		{
			if (Desc.Struct->IsChildOf(BaseStruct))
			{
				CachedContextData.Add(Desc.Name);
			}
		}
	}
}

TSharedRef<SWidget> FWorldConditionContextDataRefDetails::OnGetContent() const
{
	FMenuBuilder MenuBuilder(true, NULL);

	FUIAction ClearItemAction(FExecuteAction::CreateSP(const_cast<FWorldConditionContextDataRefDetails*>(this), &FWorldConditionContextDataRefDetails::OnSetContextData, FName()));
	MenuBuilder.AddMenuEntry(LOCTEXT("None", "None"), FText::GetEmpty(), FSlateIcon(), ClearItemAction);

	if (CachedContextData.Num() > 0)
	{
		MenuBuilder.AddSeparator();
	}
	
	for (const FName& ContextDataName : CachedContextData)
	{
		FUIAction ItemAction(FExecuteAction::CreateSP(const_cast<FWorldConditionContextDataRefDetails*>(this), &FWorldConditionContextDataRefDetails::OnSetContextData, ContextDataName));
		MenuBuilder.AddMenuEntry(FText::FromName(ContextDataName), FText::GetEmpty(), FSlateIcon(), ItemAction);
	}

	return MenuBuilder.MakeWidget();
}

FText FWorldConditionContextDataRefDetails::GetCurrentDesc() const
{
	if (!Schema)
	{
		return FText::GetEmpty();
	}
	
	if (const FWorldConditionContextDataRef* DataRef = GetCommonContextDataRef())
	{
		if (const FWorldConditionContextDataDesc* Desc = Schema->GetContextDataDescByName(DataRef->Name, BaseStruct))
		{
			return FText::FromName(Desc->Name);
		}
		return LOCTEXT("None", "None");
	}
	
	return LOCTEXT("MultipleSelected", "Multiple Selected");
}

FWorldConditionContextDataRef* FWorldConditionContextDataRefDetails::GetCommonContextDataRef() const
{
	check(StructProperty);

	FWorldConditionContextDataRef* Result = nullptr;
	
	TArray<void*> RawNodeData;
	StructProperty->AccessRawData(RawNodeData);

	for (void* Data : RawNodeData)
	{
		if (Data)
		{
			Result = static_cast<FWorldConditionContextDataRef*>(Data);
			break;
		}
	}

	return Result;
}

void FWorldConditionContextDataRefDetails::OnSetContextData(const FName ContextDataName) const
{
	check(StructProperty);

	TArray<void*> RawNodeData;
	StructProperty->AccessRawData(RawNodeData);

	GEditor->BeginTransaction(LOCTEXT("SelectReference", "Select Reference"));

	StructProperty->NotifyPreChange();

	for (void* Data : RawNodeData)
	{
		if (FWorldConditionContextDataRef* DataRef = static_cast<FWorldConditionContextDataRef*>(Data))
		{
			DataRef->Name = ContextDataName;
			DataRef->Index = FWorldConditionContextDataRef::InvalidIndex;
		}
	}

	StructProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
	StructProperty->NotifyFinishedChangingProperties();

	GEditor->EndTransaction();
}

#undef LOCTEXT_NAMESPACE
