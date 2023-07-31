// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "IPropertyTypeCustomization.h"
#include "Input/Reply.h"

class FNiagaraScriptViewModel;
class INiagaraParameterCollectionViewModel;
class FNiagaraMetaDataCustomNodeBuilder;
class IPropertyHandle;
class FDetailWidgetRow;
class IDetailChildrenBuilder;
class IPropertyTypeCustomizationUtils;

class FNiagaraScriptDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();
	
	FNiagaraScriptDetails();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder);

	FReply OnRefreshMetadata();

private:
	TSharedPtr<FNiagaraScriptViewModel> ScriptViewModel;
	TSharedPtr<FNiagaraMetaDataCustomNodeBuilder> MetaDataBuilder;
};
