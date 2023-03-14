// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ParameterDictionary.h"
#include "PlayerTime.h"
#include "TTMLSubtitleHandler.h"

namespace ElectraTTMLParser
{
class ITTMLXMLElement;

class ITTMLSubtitleList : public ITTMLSubtitleHandler
{
public:
	static TSharedPtr<ITTMLSubtitleList, ESPMode::ThreadSafe> Create();
	
	virtual ~ITTMLSubtitleList() = default;
	virtual const FString& GetLastErrorMessage() const = 0;

	virtual bool CreateFrom(TSharedPtr<ITTMLXMLElement, ESPMode::ThreadSafe> InTTMLTT, const Electra::FTimeValue& InDocumentStartTime, const Electra::FTimeValue& InDocumentDuration, const Electra::FParamDict& InOptions) = 0;
};

}
