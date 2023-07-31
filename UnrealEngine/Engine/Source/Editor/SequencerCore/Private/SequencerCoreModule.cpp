// Copyright Epic Games, Inc. All Rights Reserved.

#include "SequencerCoreModule.h"
#include "Modules/ModuleManager.h"

namespace UE
{
namespace Sequencer
{


/**
 * Interface for the Sequencer module.
 */
class FSequencerCoreModule
	: public ISequencerCoreModule
{

};


} // namespace Sequencer
} // namespace UE


IMPLEMENT_MODULE(UE::Sequencer::FSequencerCoreModule, SequencerCore);
