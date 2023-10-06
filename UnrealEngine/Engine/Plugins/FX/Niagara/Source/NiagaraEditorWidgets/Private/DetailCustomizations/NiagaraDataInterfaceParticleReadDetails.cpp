// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceParticleReadDetails.h"
#include "NiagaraDataInterfaceParticleRead.h" 
#include "NiagaraSystem.h" 

#include "Widgets/Input/SSuggestionTextBox.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"

#define LOCTEXT_NAMESPACE "NiagaraDataInterfaceParticleReadDetails"

FNiagaraDataInterfaceParticleReadDetails::~FNiagaraDataInterfaceParticleReadDetails()
{
	if (ReadDataInterfaceWeak.IsValid())
	{
		ReadDataInterfaceWeak->OnChanged().RemoveAll(this);
	}
}

void FNiagaraDataInterfaceParticleReadDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	FNiagaraDataInterfaceDetailsBase::CustomizeDetails(DetailBuilder);
	 
	TArray<TWeakObjectPtr<UObject>> SelectedObjects;
	DetailBuilder.GetObjectsBeingCustomized(SelectedObjects);
	if(SelectedObjects.Num() != 1 || SelectedObjects[0]->IsA<UNiagaraDataInterfaceParticleRead>() == false)
	{
		return;
	}

	UNiagaraDataInterfaceParticleRead* ReadDataInterface = CastChecked<UNiagaraDataInterfaceParticleRead>(SelectedObjects[0].Get());
	ReadDataInterfaceWeak = ReadDataInterface;
	NiagaraSystemWeak = ReadDataInterface->GetTypedOuter<UNiagaraSystem>();

	ReadDataInterface->OnChanged().AddSP(this, &FNiagaraDataInterfaceParticleReadDetails::OnDataInterfaceChanged);

	static const FName ReadCategoryName = TEXT("ParticleRead");
	IDetailCategoryBuilder& CategoryBuilder = DetailBuilder.EditCategory(ReadCategoryName);
	
	TArray<TSharedRef<IPropertyHandle>> Properties;
	CategoryBuilder.GetDefaultProperties(Properties, true, true);
	for (const TSharedRef<IPropertyHandle>& PropertyHandle : Properties)
	{
		FProperty* Property = PropertyHandle->GetProperty();
		if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceParticleRead, EmitterName))
		{
			EmitterNameHandle = PropertyHandle;
			DetailBuilder.HideProperty(PropertyHandle);

			CategoryBuilder.AddCustomRow(FText::GetEmpty())
				.NameContent()
				[
					PropertyHandle->CreatePropertyNameWidget()
				]
				.ValueContent()
				[
					SAssignNew(TextBoxWidget, SSuggestionTextBox)
						.ForegroundColor(FSlateColor::UseForeground())
						.Font(IDetailLayoutBuilder::GetDetailFont())
						.OnTextCommitted(this, &FNiagaraDataInterfaceParticleReadDetails::EmitterName_SetText)
						.HintText(LOCTEXT("EmitterNameHint", "Enter Emitter Name..."))
						.OnShowingSuggestions(this, &FNiagaraDataInterfaceParticleReadDetails::EmitterName_GetSuggestions)
				];

			OnDataInterfaceChanged();
		}
		else
		{
			CategoryBuilder.AddProperty(PropertyHandle);
		}
	}
}

TSharedRef<IDetailCustomization> FNiagaraDataInterfaceParticleReadDetails::MakeInstance()
{
	return MakeShared<FNiagaraDataInterfaceParticleReadDetails>();
}

void FNiagaraDataInterfaceParticleReadDetails::OnDataInterfaceChanged()
{
	if (UNiagaraDataInterfaceParticleRead* ReadDataInterface = ReadDataInterfaceWeak.Get())
	{
		TextBoxWidget->SetText(FText::FromString(ReadDataInterface->EmitterName));
	}
}

void FNiagaraDataInterfaceParticleReadDetails::EmitterName_SetText(const FText& NewText, ETextCommit::Type CommitInfo)
{
	if (EmitterNameHandle)
	{
		EmitterNameHandle->SetValue(NewText.ToString());
	}
}

void FNiagaraDataInterfaceParticleReadDetails::EmitterName_GetSuggestions(const FString& CurrText, TArray<FString>& OutSuggestions)
{
	if (UNiagaraSystem* NiagaraSystem = NiagaraSystemWeak.Get())
	{
		for (const FNiagaraEmitterHandle& EmitterHandle : NiagaraSystem->GetEmitterHandles())
		{
			if (EmitterHandle.GetIsEnabled())
			{
				OutSuggestions.Add(EmitterHandle.GetUniqueInstanceName());
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
