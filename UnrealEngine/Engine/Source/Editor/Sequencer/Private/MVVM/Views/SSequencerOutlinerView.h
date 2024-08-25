// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVM/Views/SOutlinerView.h"

namespace UE::Sequencer
{

class SSequencerOutlinerView : public SOutlinerView
{
public:

	TSharedRef<ITableRow> OnGenerateRow(TWeakViewModelPtr<IOutlinerExtension> InWeakModel, const TSharedRef<STableViewBase>& OwnerTable) override;
};


} // namespace UE::Sequencer