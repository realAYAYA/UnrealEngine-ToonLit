// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraDataInterfaceDetails.h"
#include "Types/SlateEnums.h"
#include "Widgets/Input/SComboBox.h"

class IDetailLayoutBuilder;
class IDetailChildrenBuilder;
class IPropertyHandle;
class SWidget;
class FName;
class UNiagaraDataInterfaceParticleRead;
class UNiagaraSystem;
class SNiagaraNamePropertySelector;

/** Details customization for Niagara particle read data interface. */
class FNiagaraDataInterfaceParticleReadDetails : public FNiagaraDataInterfaceDetailsBase
{
public:
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	static TSharedRef<IDetailCustomization> MakeInstance();

private:
	void EmitterName_SetText(const FText& NewText, ETextCommit::Type CommitInfo);
	void EmitterName_GetSuggestions(const FString& CurrText, TArray<FString>& OutSuggestions);

	TWeakObjectPtr<UNiagaraDataInterfaceParticleRead> ReadDataInterfaceWeak;
	TWeakObjectPtr<UNiagaraSystem> NiagaraSystemWeak;
	TSharedPtr<IPropertyHandle> EmitterNameHandle;
};
