// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixMetasoundModule.h"

#include "MetasoundDataTypeRegistrationMacro.h"
#include "MetasoundGeneratorHandle.h"
#include "Analysis/MetasoundFrontendAnalyzerRegistry.h"

#include "HarmonixMetasound/Analysis/MidiClockVertexAnalyzer.h"
#include "HarmonixMetasound/Analysis/MidiStreamVertexAnalyzer.h"
#include "HarmonixMetasound/Analysis/FFTAnalyzerResultVertexAnalyzer.h"
#include "HarmonixMetasound/DataTypes/FFTAnalyzerResult.h"
#include "HarmonixMetasound/DataTypes/MidiStream.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"
#include "UObject/CoreRedirects.h"

DEFINE_LOG_CATEGORY_STATIC(LogHarmonixMetasoundModule, Log, Log)

void FHarmonixMetasoundModule::StartupModule()
{
	using namespace Metasound;

	FMetasoundFrontendRegistryContainer::Get()->RegisterPendingNodes();

	// Register passthrough analyzers for output watching
	UMetasoundGeneratorHandle::RegisterPassthroughAnalyzerForType(
		GetMetasoundDataTypeName<HarmonixMetasound::FMidiStream>(),
		HarmonixMetasound::Analysis::FMidiStreamVertexAnalyzer::GetAnalyzerName(),
		HarmonixMetasound::Analysis::FMidiStreamVertexAnalyzer::FOutputs::GetValue().Name);
	UMetasoundGeneratorHandle::RegisterPassthroughAnalyzerForType(
		GetMetasoundDataTypeName<HarmonixMetasound::FMidiClock>(),
		HarmonixMetasound::Analysis::FMidiClockVertexAnalyzer::GetAnalyzerName(),
		HarmonixMetasound::Analysis::FMidiClockVertexAnalyzer::FOutputs::GetValue().Name);
	UMetasoundGeneratorHandle::RegisterPassthroughAnalyzerForType(
		GetMetasoundDataTypeName<FHarmonixFFTAnalyzerResults>(),
		HarmonixMetasound::Analysis::FFFTAnalyzerResultVertexAnalyzer::GetAnalyzerName(),
		HarmonixMetasound::Analysis::FFFTAnalyzerResultVertexAnalyzer::FOutputs::GetValue().Name);

	// Register vertex analyzer factories
	METASOUND_REGISTER_VERTEX_ANALYZER_FACTORY(HarmonixMetasound::Analysis::FMidiStreamVertexAnalyzer)
	METASOUND_REGISTER_VERTEX_ANALYZER_FACTORY(HarmonixMetasound::Analysis::FMidiClockVertexAnalyzer)
	METASOUND_REGISTER_VERTEX_ANALYZER_FACTORY(HarmonixMetasound::Analysis::FFFTAnalyzerResultVertexAnalyzer)

	// The first redirect for thie module
	TArray<FCoreRedirect> Redirects;
	Redirects.Emplace(ECoreRedirectFlags::Type_Function, TEXT("MusicClockComponent.CreateMusicClockComponent"), TEXT("MusicClockComponent.CreateMetasoundDrivenMusicClock"));
	FCoreRedirects::AddRedirectList(Redirects, TEXT("HarmonixMetasoundModule"));

}

void FHarmonixMetasoundModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FHarmonixMetasoundModule, HarmonixMetasound);
