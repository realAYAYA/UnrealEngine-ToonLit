// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVM/ViewModels/OutlinerColumns/OutlinerColumnBase.h"

namespace UE::Sequencer
{

class FKeyFrameOutlinerColumn
	: public FOutlinerColumnBase
{
public:

	SEQUENCERCORE_API FKeyFrameOutlinerColumn();

	bool IsColumnVisibleByDefault() const override;
};

} // namespace UE::Sequencer