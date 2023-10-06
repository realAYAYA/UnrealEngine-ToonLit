// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Misc/StringBuilder.h"
#include "MVVM/ViewModelTypeID.h"

struct FGuid;

namespace UE
{
namespace Sequencer
{

class SEQUENCERCORE_API IGroupableExtension
{
public:

	UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(IGroupableExtension)

	virtual ~IGroupableExtension(){}

	virtual void GetIdentifierForGrouping(TStringBuilder<128>& OutString) const = 0;

	FString GetIdentifierForGrouping() const
	{
		TStringBuilder<128> Builder;
		GetIdentifierForGrouping(Builder);
		return Builder.ToString();
	}
};


} // namespace Sequencer
} // namespace UE

