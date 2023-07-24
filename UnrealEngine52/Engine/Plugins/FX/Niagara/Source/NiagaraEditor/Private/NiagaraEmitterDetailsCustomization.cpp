// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraEmitterDetailsCustomization.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "NiagaraEmitter.h"
#include "NiagaraEditorModule.h"
#include "Toolkits/NiagaraSystemToolkit.h"
#include "ViewModels/NiagaraEmitterViewModel.h"
#include "ViewModels/TNiagaraViewModelManager.h"

TSharedRef<IDetailCustomization> FNiagaraEmitterDetails::MakeInstance()
{
	return MakeShared<FNiagaraEmitterDetails>();
}

void CustomizeEmitterData(IDetailLayoutBuilder& InDetailLayout, UNiagaraEmitter* EmitterBeingCustomized, TMap<FName, TSet<FProperty*>> CategoryPropertyMap)
{
	TArray<TSharedPtr<FNiagaraEmitterViewModel>> ExistingViewModels;
	TNiagaraViewModelManager<UNiagaraEmitter, FNiagaraEmitterViewModel>::GetAllViewModelsForObject(EmitterBeingCustomized, ExistingViewModels);
	FVersionedNiagaraEmitter VersionedNiagaraEmitter = ExistingViewModels[0]->GetEmitter();
	FVersionedNiagaraEmitterData* EmitterData = VersionedNiagaraEmitter.GetEmitterData();
	TSharedPtr<FStructOnScope> StructData = MakeShareable(new FStructOnScope(FVersionedNiagaraEmitterData::StaticStruct(), reinterpret_cast<uint8*>(EmitterData)));
	for (const auto& Entry : CategoryPropertyMap)
	{
		IDetailCategoryBuilder& CategoryBuilder = InDetailLayout.EditCategory(Entry.Key);
		TArray<IDetailPropertyRow*> OutRows;
			
		// We want only one structonscope, otherwise edit conditions don't work correctly. Unfortuately this means we need to add each category separately and hide properties not assigned to the category.
		TArray<TSharedPtr<IPropertyHandle>> Handles = CategoryBuilder.AddAllExternalStructureProperties(StructData.ToSharedRef(), EPropertyLocation::Default, &OutRows);
			
		for (IDetailPropertyRow* PropertyRow : OutRows)
		{
			TSharedPtr<IPropertyHandle> PropertyHandle = PropertyRow->GetPropertyHandle();
			if (!Entry.Value.Contains(PropertyHandle->GetProperty()))
			{
				PropertyHandle->MarkHiddenByCustomization();
				PropertyRow->Visibility(EVisibility::Collapsed);
			}
			else
			{
				FProperty* ChildProperty = PropertyHandle->GetProperty();
				const auto& PostEditChangeLambda = [VersionedNiagaraEmitter, ChildProperty]
				{
					FPropertyChangedEvent ChangeEvent(ChildProperty);
					VersionedNiagaraEmitter.Emitter->PostEditChangeVersionedProperty(ChangeEvent, VersionedNiagaraEmitter.Version);
				};

				PropertyRow->GetPropertyHandle()->SetOnChildPropertyValueChanged(FSimpleDelegate::CreateLambda(PostEditChangeLambda));
				PropertyRow->GetPropertyHandle()->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda(PostEditChangeLambda));
			}
		}
	}
}

void FNiagaraEmitterDetails::CustomizeDetails(IDetailLayoutBuilder& InDetailLayout)
{
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	InDetailLayout.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	
	if (UNiagaraEmitter* EmitterBeingCustomized = CastChecked<UNiagaraEmitter>(ObjectsBeingCustomized[0]))
	{
		TMap<FName, TSet<FProperty*>> CategoryPropertyMap;
		for (FProperty* ChildProperty : TFieldRange<FProperty>(FVersionedNiagaraEmitterData::StaticStruct()))
		{
			if (ChildProperty->HasAllPropertyFlags(CPF_Edit))
			{
				FName Category = FName(ChildProperty->GetMetaData(TEXT("Category")));

				// we display the scalability category within scalability mode, which is why we hide it here
				if (Category != FName("Scalability"))
				{
					CategoryPropertyMap.FindOrAdd(Category).Add(ChildProperty);
				}
			}
		}

		CustomizeEmitterData(InDetailLayout, EmitterBeingCustomized, CategoryPropertyMap);
	}
}

TSharedRef<IDetailCustomization> FNiagaraEmitterScalabilityDetails::MakeInstance()
{
	return MakeShared<FNiagaraEmitterScalabilityDetails>();
}

void FNiagaraEmitterScalabilityDetails::CustomizeDetails(IDetailLayoutBuilder& InDetailLayout)
{
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	InDetailLayout.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	
	if (UNiagaraEmitter* EmitterBeingCustomized = CastChecked<UNiagaraEmitter>(ObjectsBeingCustomized[0]))
	{		
		TMap<FName, TSet<FProperty*>> CategoryPropertyMap;
		for (FProperty* ChildProperty : TFieldRange<FProperty>(FVersionedNiagaraEmitterData::StaticStruct()))
		{
			if (ChildProperty->HasAllPropertyFlags(CPF_Edit) && ChildProperty->HasMetaData(TEXT("DisplayInScalabilityContext")))
			{
				FName Category = FName(ChildProperty->GetMetaData(TEXT("Category")));
				CategoryPropertyMap.FindOrAdd(Category).Add(ChildProperty);
			}
		}

		CustomizeEmitterData(InDetailLayout, EmitterBeingCustomized, CategoryPropertyMap);
	}
}
