// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "MVVM/ViewModelPtr.h"

class FName;

namespace UE
{
namespace Sequencer
{

class IOutlinerExtension;
class ITrackExtension;

/**
 * Takes a display node and traverses it's parents to find the nearest track node if any.  Also collects the names of the nodes which make
 * up the path from the track node to the display node being checked.  The name path includes the name of the node being checked, but not
 * the name of the track node.
 */
TViewModelPtr<ITrackExtension> GetParentTrackNodeAndNamePath(const TViewModelPtr<IOutlinerExtension>& Node, TArray<FName>& OutNamePath);

} // namespace Sequencer
} // namespace UE