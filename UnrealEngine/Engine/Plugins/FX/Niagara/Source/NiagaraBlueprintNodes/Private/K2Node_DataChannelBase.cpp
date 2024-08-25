// Copyright Epic Games, Inc. All Rights Reserved.


#include "K2Node_DataChannelBase.h"
#include "EdGraphSchema_K2.h"

#include "NiagaraDataChannelPublic.h"

#define LOCTEXT_NAMESPACE "K2Node_DataChannelBase"

UNiagaraDataChannel* UK2Node_DataChannelBase::GetDataChannel() const
{
	return DataChannel ? DataChannel->Get() : nullptr;
}

bool UK2Node_DataChannelBase::HasValidDataChannel() const
{
	return DataChannel && DataChannel->Get();
}

void UK2Node_DataChannelBase::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	if (HasValidDataChannel() && GetDataChannel()->GetVersion() != DataChannelVersion && HasValidBlueprint())
	{
		ReconstructNode();
	}
#endif
}

void UK2Node_DataChannelBase::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

#if WITH_EDITORONLY_DATA
	if (HasValidDataChannel())
	{
		DataChannelVersion = GetDataChannel()->GetVersion();
	}
#endif
}

void UK2Node_DataChannelBase::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	Super::PinDefaultValueChanged(Pin);

#if WITH_EDITORONLY_DATA
	if (Pin == GetChannelSelectorPin())
	{
		if (UNiagaraDataChannelAsset* ChannelAsset = Cast<UNiagaraDataChannelAsset>(Pin->DefaultObject))
		{
			DataChannel = ChannelAsset;
			ReconstructNode();
		}
	}
#endif
}

void UK2Node_DataChannelBase::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);

#if WITH_EDITORONLY_DATA
	if (Pin == GetChannelSelectorPin())
	{
		if (UNiagaraDataChannelAsset* ChannelAsset = Cast<UNiagaraDataChannelAsset>(Pin->DefaultObject))
		{
			DataChannel = Pin->LinkedTo.Num() == 0 ? ChannelAsset : nullptr;
			ReconstructNode();
		}
	}
#endif
}

FText UK2Node_DataChannelBase::GetMenuCategory() const
{
	static FText MenuCategory = LOCTEXT("MenuCategory", "Niagara Data Channel");
	return MenuCategory;
}

UK2Node::ERedirectType UK2Node_DataChannelBase::DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex) const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	
	ERedirectType Result = UK2Node::DoPinsMatchForReconstruction(NewPin, NewPinIndex, OldPin, OldPinIndex);
	if (ERedirectType_None == Result && K2Schema && K2Schema->ArePinTypesCompatible(NewPin->PinType, OldPin->PinType) && (NewPin->PersistentGuid == OldPin->PersistentGuid) && OldPin->PersistentGuid.IsValid())
	{
		Result = ERedirectType_Name;
	}

	return Result;
}

void UK2Node_DataChannelBase::PreloadRequiredAssets()
{
	Super::PreloadRequiredAssets();

	if (DataChannel)
	{
		PreloadObject(DataChannel);
		PreloadObject(DataChannel->Get());
	}
}

bool UK2Node_DataChannelBase::ShouldShowNodeProperties() const
{
	return true;
}

UEdGraphPin* UK2Node_DataChannelBase::GetChannelSelectorPin() const
{
	return FindPinChecked(FName("Channel"), EGPD_Input);
}

#undef LOCTEXT_NAMESPACE
