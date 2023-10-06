// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeExtensionDataSwitch.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSource.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceFloat.h"
#include "MuCOE/ExtensionDataCompilerInterface.h"
#include "MuCOE/GraphTraversal.h"
#include "MuT/NodeExtensionDataSwitch.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CustomizableObjectNodeExtensionDataSwitch)


bool UCustomizableObjectNodeExtensionDataSwitch::IsAffectedByLOD() const
{
	return false;
}

bool UCustomizableObjectNodeExtensionDataSwitch::ShouldAddToContextMenu(FText& OutCategory) const
{
	OutCategory = FText::FromName(GetCategory());
	return true;
}

FString UCustomizableObjectNodeExtensionDataSwitch::GetPinPrefix() const
{
	return FString::Format(TEXT("{0} "), { GetOutputPinName() });
}

mu::NodeExtensionDataPtr UCustomizableObjectNodeExtensionDataSwitch::GenerateMutableNode(FExtensionDataCompilerInterface& InCompilerInterface) const
{
	const int32 NumParameters = FollowInputPinArray(*SwitchParameter()).Num();

	const UEdGraphPin* EnumPin = FollowInputPin(*SwitchParameter());
	mu::NodeScalarPtr SwitchParam = EnumPin ? GenerateMutableSourceFloat(EnumPin, InCompilerInterface.GenerationContext) : nullptr;

	const int32 NumSwitchOptions = GetNumElements();

	mu::NodeExtensionDataSwitchPtr SwitchNode = new mu::NodeExtensionDataSwitch;
	SwitchNode->SetParameter(SwitchParam);
	SwitchNode->SetOptionCount(NumSwitchOptions);

	for (int32 OptionIndex = 0; OptionIndex < NumSwitchOptions; ++OptionIndex)
	{
		if (const UEdGraphPin* ExtensionDataPin = GetElementPin(OptionIndex))
		{
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*ExtensionDataPin))
			{
				if (const ICustomizableObjectExtensionNode* ExtensionNode = Cast<ICustomizableObjectExtensionNode>(ConnectedPin->GetOwningNode()))
				{
					if (mu::NodeExtensionDataPtr ExtensionMutableNode = ExtensionNode->GenerateMutableNode(InCompilerInterface))
					{
						SwitchNode->SetOption(OptionIndex, ExtensionMutableNode);
					}
				}
			}
		}
	}

	InCompilerInterface.AddGeneratedNode(this);

	return SwitchNode;
}
