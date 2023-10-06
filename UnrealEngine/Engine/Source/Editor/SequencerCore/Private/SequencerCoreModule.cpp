// Copyright Epic Games, Inc. All Rights Reserved.

#include "SequencerCoreModule.h"
#include "Modules/ModuleManager.h"
#include "SequencerCoreLog.h"

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


DEFINE_LOG_CATEGORY(LogSequencerCore);
IMPLEMENT_MODULE(UE::Sequencer::FSequencerCoreModule, SequencerCore);
