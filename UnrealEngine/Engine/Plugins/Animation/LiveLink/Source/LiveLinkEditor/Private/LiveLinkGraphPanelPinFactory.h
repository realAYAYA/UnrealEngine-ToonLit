// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "EdGraphUtilities.h"
#include "EdGraphSchema_K2.h"
#include "SGraphPin.h"

#include "LiveLinkRole.h"
#include "SLiveLinkSubjectNameGraphPin.h"
#include "SLiveLinkSubjectRepresentationGraphPin.h"


class FLiveLinkGraphPanelPinFactory : public FGraphPanelPinFactory
{
	virtual TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* InPin) const override
	{
		if (InPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
		{
			if (UScriptStruct* PinStructType = Cast<UScriptStruct>(InPin->PinType.PinSubCategoryObject.Get()))
			{
				if (PinStructType == FLiveLinkSubjectRepresentation::StaticStruct())
				{
					return SNew(SLiveLinkSubjectRepresentationGraphPin, InPin);
				}
				else if (PinStructType == FLiveLinkSubjectName::StaticStruct())
				{
					return SNew(SLiveLinkSubjectNameGraphPin, InPin);
				}
			}
		}

		return nullptr;
	}
};
