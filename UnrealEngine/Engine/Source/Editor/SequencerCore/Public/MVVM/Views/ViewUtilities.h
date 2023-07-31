// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Framework/SlateDelegates.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SWidget.h"

class FText;
class SWidget;
template< typename ObjectType > class TAttribute;

namespace UE
{
namespace Sequencer
{

SEQUENCERCORE_API TSharedRef<SWidget> MakeAddButton(FText HoverText, FOnGetContent MenuContent, const TAttribute<bool>& HoverState, const TAttribute<bool>& IsEnabled);

} // namespace Sequencer
} // namespace UE