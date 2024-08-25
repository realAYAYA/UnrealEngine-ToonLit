// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixDsp/Effects/Delay.h"
#include "HarmonixMetasound/Nodes/DelayNode.h"
#include "Misc/AutomationTest.h"
#include "NodeTestGraphBuilder.h"
#include "HarmonixMetasound/DataTypes/DelayFilterType.h"
#include "HarmonixMetasound/DataTypes/DelayStereoType.h"
#include "HarmonixMetasound/DataTypes/MidiClock.h"
#include "HarmonixMetasound/DataTypes/TimeSyncOption.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace HarmonixMetasoundTests::DelayNode
{
	using GraphBuilder = Metasound::Test::FNodeTestGraphBuilder;
	using namespace HarmonixMetasound;
	using namespace Metasound;
	
	class FTestFixture
	{
	public:
		FTestFixture(float InSampleRate, int32 NumSamplesPerBlock, FAutomationTestBase& InTest, bool WithClock)
			: SampleRate(InSampleRate)
			, Test(InTest)
			, ComparisonBuffer(HarmonixMetasound::DelayNode::Constants::NumChannels, NumSamplesPerBlock, EAudioBufferCleanupMode::Delete)
		{
			{
				const FAudioBufferConfig BufferConfig{
					HarmonixMetasound::DelayNode::Constants::NumChannels,
					NumSamplesPerBlock,
					SampleRate,
					true
				};
				GeneratorBufferInterleaved.Configure(BufferConfig, EAudioBufferCleanupMode::Delete);
			}
			
			DelayForComparison.Prepare(
				InSampleRate,
				HarmonixMetasound::DelayNode::Constants::NumChannels,
				HarmonixMetasound::DelayNode::Constants::MaxDelayTime);

			using namespace Metasound;
			
			GraphBuilder Builder;
			const Frontend::FNodeHandle NodeHandle = Builder.AddNode(
				{HarmonixNodeNamespace, "Delay", "" }, 0);

			check(NodeHandle->IsValid());

			// add the inputs and connect them
			for (const Frontend::FInputHandle& Input : NodeHandle->GetInputs())
			{
				// ...but skip the midi clock if we're not using one
				if (Input->GetDataType() == "MidiClock" && !WithClock)
				{
					continue;
				}
					
				const Frontend::FNodeHandle InputNode = Builder.AddInput(Input->GetName(), Input->GetDataType());
				check(InputNode->IsValid());

				const Frontend::FOutputHandle OutputToConnect = InputNode->GetOutputWithVertexName(Input->GetName());
				const Frontend::FInputHandle InputToConnect = NodeHandle->GetInputWithVertexName(Input->GetName());
					
				if (!InputToConnect->Connect(*OutputToConnect))
				{
					check(false);
					return;
				}
			}

			// add the outputs and connect them
			for (const Frontend::FOutputHandle& Output : NodeHandle->GetOutputs())
			{
				const Frontend::FNodeHandle OutputNode = Builder.AddOutput(Output->GetName(), Output->GetDataType());
	
				check(OutputNode->IsValid());

				Frontend::FOutputHandle OutputToConnect = NodeHandle->GetOutputWithVertexName(Output->GetName());
				const Frontend::FInputHandle InputToConnect = OutputNode->GetInputWithVertexName(Output->GetName());
	
				if (!InputToConnect->Connect(*OutputToConnect))
				{
					check(false);
					return;
				}
			}

			// build the graph
			Generator = Builder.BuildGenerator(SampleRate, NumSamplesPerBlock);
		}

		bool RenderAndCompare(bool AddImpulse)
		{
			using namespace Metasound;

			// zero the input buffers
			ComparisonBuffer.ZeroData();
			TOptional<TDataWriteReference<FAudioBuffer>> InputAudioLeft =
				Generator->GetInputWriteReference<FAudioBuffer>(HarmonixMetasound::DelayNode::Inputs::AudioLeftName);
			TOptional<TDataWriteReference<FAudioBuffer>> InputAudioRight =
				Generator->GetInputWriteReference<FAudioBuffer>(HarmonixMetasound::DelayNode::Inputs::AudioRightName);
			if (!Test.TestTrue("Got input buffers", InputAudioLeft.IsSet() && InputAudioRight.IsSet()))
			{
				return false;
			}
			(*InputAudioLeft)->Zero();
			(*InputAudioRight)->Zero();
			
			// if requested, add an impulse to the input buffers
			if (AddImpulse)
			{
				ComparisonBuffer[0][0] = 1;
				ComparisonBuffer[1][0] = 1;

				check((*InputAudioLeft)->Num() == (*InputAudioRight)->Num());
				(*InputAudioLeft)->GetData()[0] = 1;
				(*InputAudioRight)->GetData()[0] = 1;
			}

			// render
			GeneratorBufferInterleaved.ZeroData();
			Generator->OnGenerateAudio(GeneratorBufferInterleaved.GetRawChannelData(0), GeneratorBufferInterleaved.GetNumTotalValidSamples());
			DelayForComparison.Process(ComparisonBuffer);
			
			// check that the output buffers are equal
			for (int32 ChannelIdx = 0; ChannelIdx < HarmonixMetasound::DelayNode::Constants::NumChannels; ++ChannelIdx)
			{
				TDynamicStridePtr<float> GeneratorPtr = GeneratorBufferInterleaved.GetStridingChannelDataPointer(ChannelIdx);
				TDynamicStridePtr<float> ComparisonPtr = ComparisonBuffer.GetStridingChannelDataPointer(ChannelIdx);
				for (int32 SampleIdx = 0; SampleIdx < ComparisonBuffer.GetNumValidFrames(); ++SampleIdx)
				{
					if (!Test.TestEqual(
						FString::Printf(TEXT("Channel %i samples match at idx %i"), ChannelIdx, SampleIdx),
						GeneratorPtr[SampleIdx],
						ComparisonPtr[SampleIdx]))
					{
						return false;
					}
				}
			}

			return true;
		}

		struct FParams
		{
			ETimeSyncOption DelayTimeType;
			float DelayTime;
			float Feedback;
			float DryLevel;
			float WetLevel;
			bool WetFilterEnabled;
			bool FeedbackFilterEnabled;
			EDelayFilterType FilterType;
			float FilterCutoff;
			float FilterQ;
			bool LFOEnabled;
			ETimeSyncOption LFOTimeType;
			float LFOFrequency;
			float LFODepth;
			EDelayStereoType StereoType;
			float StereoSpreadLeft;
			float StereoSpreadRight;

			FParams()
			{
				const Harmonix::Dsp::Effects::FDelay Def;
				DelayTimeType = Def.GetTimeSyncOption();
				DelayTime = Def.GetDelaySeconds();
				Feedback = Def.GetFeedbackGain();
				DryLevel = Def.GetDryGain();
				WetLevel = Def.GetWetGain();
				WetFilterEnabled = Def.GetWetFilterEnabled();
				FeedbackFilterEnabled = Def.GetFeedbackFilterEnabled();
				FilterType = static_cast<EDelayFilterType>(Def.GetFilterType());
				FilterCutoff = Def.GetFilterFreq();
				FilterQ = Def.GetFilterQ();
				LFOEnabled = Def.GetLfoEnabled();
				LFOTimeType = Def.GetLfoTimeSyncOption();
				LFOFrequency = Def.GetLfoFreq();
				LFODepth = Def.GetLfoDepth();
				StereoType = Def.GetStereoType();
				StereoSpreadLeft = Def.GetStereoSpreadLeft();
				StereoSpreadRight = Def.GetStereoSpreadRight();
			}
		};

		void SetParams(const FParams& Params)
		{
			using namespace Metasound;

			// set the comparison delay's params
			DelayForComparison.SetTimeSyncOption(Params.DelayTimeType);
			DelayForComparison.SetDelaySeconds(Params.DelayTime);
			DelayForComparison.SetFeedbackGain(Params.Feedback);
			DelayForComparison.SetDryGain(Params.DryLevel);
			DelayForComparison.SetWetGain(Params.WetLevel);
			DelayForComparison.SetWetFilterEnabled(Params.WetFilterEnabled);
			DelayForComparison.SetFeedbackFilterEnabled(Params.FeedbackFilterEnabled);
			DelayForComparison.SetFilterType(Params.FilterType);
			DelayForComparison.SetFilterFreq(Params.FilterCutoff);
			DelayForComparison.SetFilterQ(Params.FilterQ);
			DelayForComparison.SetLfoEnabled(Params.LFOEnabled);
			DelayForComparison.SetLfoTimeSyncOption(Params.LFOTimeType);
			DelayForComparison.SetLfoFreq(Params.LFOFrequency);
			DelayForComparison.SetLfoDepth(Params.LFODepth);
			DelayForComparison.SetStereoType(Params.StereoType);
			DelayForComparison.SetStereoSpreadLeft(Params.StereoSpreadLeft);
			DelayForComparison.SetStereoSpreadRight(Params.StereoSpreadRight);
			
			// set the operator's params
			Generator->SetInputValue(HarmonixMetasound::DelayNode::Inputs::DelayTimeTypeName, FEnumTimeSyncOption{ Params.DelayTimeType });
			Generator->SetInputValue(HarmonixMetasound::DelayNode::Inputs::DelayTimeName, Params.DelayTime);
			Generator->SetInputValue(HarmonixMetasound::DelayNode::Inputs::FeedbackName, Params.Feedback);
			Generator->SetInputValue(HarmonixMetasound::DelayNode::Inputs::DryLevelName, Params.DryLevel);
			Generator->SetInputValue(HarmonixMetasound::DelayNode::Inputs::WetLevelName, Params.WetLevel);
			Generator->SetInputValue(HarmonixMetasound::DelayNode::Inputs::WetFilterEnabledName, Params.WetFilterEnabled);
			Generator->SetInputValue(HarmonixMetasound::DelayNode::Inputs::FeedbackFilterEnabledName, Params.FeedbackFilterEnabled);
			Generator->SetInputValue(HarmonixMetasound::DelayNode::Inputs::FilterTypeName, FEnumDelayFilterType{ Params.FilterType });
			Generator->SetInputValue(HarmonixMetasound::DelayNode::Inputs::FilterCutoffName, Params.FilterCutoff);
			Generator->SetInputValue(HarmonixMetasound::DelayNode::Inputs::FilterQName, Params.FilterQ);
			Generator->SetInputValue(HarmonixMetasound::DelayNode::Inputs::LFOEnabledName, Params.LFOEnabled);
			Generator->SetInputValue(HarmonixMetasound::DelayNode::Inputs::LFOTimeTypeName, FEnumTimeSyncOption{ Params.LFOTimeType });
			Generator->SetInputValue(HarmonixMetasound::DelayNode::Inputs::LFOFrequencyName, Params.LFOFrequency);
			Generator->SetInputValue(HarmonixMetasound::DelayNode::Inputs::LFODepthName, Params.LFODepth);
			Generator->SetInputValue(HarmonixMetasound::DelayNode::Inputs::StereoTypeName, FEnumDelayStereoType{ Params.StereoType });
			Generator->SetInputValue(HarmonixMetasound::DelayNode::Inputs::StereoSpreadLeftName, Params.StereoSpreadLeft);
			Generator->SetInputValue(HarmonixMetasound::DelayNode::Inputs::StereoSpreadRightName, Params.StereoSpreadRight);
		}
		

		bool ResetAndStartClock(float Tempo, float Speed, int32 TimeSigNum, int32 TimeSigDenom)
		{
			TOptional<FMidiClockWriteRef> ClockInput = Generator->GetInputWriteReference<FMidiClock>(CommonPinNames::Inputs::MidiClockName);
			if (!Test.TestTrue("Got clock", ClockInput.IsSet()))
			{
				return false;
			}
			const TSharedPtr<FMidiFileData> MidiData = MakeShared<FMidiFileData>();
			check(MidiData);

			FTempoMap& TempoMap = MidiData->SongMaps.GetTempoMap();
			TempoMap.Empty();
			FBarMap& BarMap = MidiData->SongMaps.GetBarMap();
			BarMap.Empty();
			MidiData->Tracks.Empty();

			MidiData->Tracks.Add(FMidiTrack(TEXT("conductor")));
			MidiData->Tracks[0].AddEvent(FMidiEvent(0, FMidiMsg(static_cast<uint8>(TimeSigNum), static_cast<uint8>(TimeSigDenom))));
			BarMap.AddTimeSignatureAtBarIncludingCountIn(0, TimeSigNum, TimeSigDenom);
			const int32 MidiTempo = Harmonix::Midi::Constants::BPMToMidiTempo(Tempo);
			MidiData->Tracks[0].AddEvent(FMidiEvent(0, FMidiMsg(MidiTempo)));
			TempoMap.AddTempoInfoPoint(MidiTempo, 0);
			MidiData->Tracks[0].Sort();
			MidiData->ConformToLength(std::numeric_limits<int32>::max());
			MidiData->SongMaps.GetSongLengthData().LengthTicks = std::numeric_limits<int32>::max();
			MidiData->SongMaps.GetSongLengthData().LengthFractionalBars = std::numeric_limits<float>::max();

			(*ClockInput)->AttachToMidiResource(MidiData);
			(*ClockInput)->ResetAndStart(0);

			SampleRemainder = 0;
			SampleCount = 0;

			DelayForComparison.SetTempo(Tempo);
			DelayForComparison.SetSpeed(Speed);

			return true;
		}

		bool AdvanceClock()
		{
			using namespace Metasound;
			TOptional<FMidiClockWriteRef> ClockInput = Generator->GetInputWriteReference<FMidiClock>(CommonPinNames::Inputs::MidiClockName);
			if (!Test.TestTrue("Got clock", ClockInput.IsSet()))
			{
				return false;
			}
			const int32 NumSamples = ComparisonBuffer.GetNumValidFrames();
			SampleRemainder += NumSamples;
			constexpr int32 MidiGranularity = 128;
			while (SampleRemainder >= MidiGranularity)
			{
				SampleCount += MidiGranularity;
				SampleRemainder -= MidiGranularity;
				const float AdvanceToMs = static_cast<float>(SampleCount) * 1000.0f / SampleRate;
				(*ClockInput)->AdvanceHiResToMs(0, AdvanceToMs, true);
			}

			return true;
		}

		float SampleRate;
		FAutomationTestBase& Test;
		Harmonix::Dsp::Effects::FDelay DelayForComparison;
		TAudioBuffer<float> ComparisonBuffer;
		TUniquePtr<FMetasoundGenerator> Generator;
		TAudioBuffer<float> GeneratorBufferInterleaved;
		Metasound::FSampleCount SampleCount = 0;
		Metasound::FSampleCount SampleRemainder = 0;
	};
	
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FDelayNodeTestRenderNoClockDefaults, 
		"Harmonix.Metasound.Nodes.Delay.Render.NoClock.Defaults", 
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	bool FDelayNodeTestRenderNoClockDefaults::RunTest(const FString&)
	{
		constexpr float SampleRate = 48000;
		constexpr int32 NumSamples = 256;
		FTestFixture TestFixture(SampleRate, NumSamples, *this, false);

		constexpr int32 NumBlocksToRender = 200;
		FTestFixture::FParams Params;

		// test with defaults
		{
			TestFixture.SetParams(Params);
			for (int32 i = 0; i < NumBlocksToRender; ++i)
			{
				UTEST_TRUE(FString::Printf(TEXT("Render test iteration %i"), i), TestFixture.RenderAndCompare(i == 0));
			}
		}
		
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDelayNodeTestRenderWithClockDefaults, 
	"Harmonix.Metasound.Nodes.Delay.Render.WithClock.Defaults", 
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	bool FDelayNodeTestRenderWithClockDefaults::RunTest(const FString&)
	{
		constexpr float SampleRate = 48000;
		constexpr int32 NumSamples = 256;
		FTestFixture TestFixture(SampleRate, NumSamples, *this, true);

		constexpr int32 NumBlocksToRender = 200;
		FTestFixture::FParams Params;
		Params.DelayTimeType = ETimeSyncOption::TempoSync;

		// test with defaults
		{
			TestFixture.SetParams(Params);
			UTEST_TRUE("Started clock", TestFixture.ResetAndStartClock(120, 1, 4, 4));
			for (int32 i = 0; i < NumBlocksToRender; ++i)
			{
				UTEST_TRUE(FString::Printf(TEXT("Advance clock iteration %i"), i), TestFixture.AdvanceClock());
				UTEST_TRUE(FString::Printf(TEXT("Render test iteration %i"), i), TestFixture.RenderAndCompare(i == 0));
			}
		}
		
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FDelayNodeTestRenderNoClockMinDelayLfoEnabled, 
		"Harmonix.Metasound.Nodes.Delay.Render.NoClock.MinDelayLfoEnabled", 
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	bool FDelayNodeTestRenderNoClockMinDelayLfoEnabled::RunTest(const FString&)
	{
		constexpr float SampleRate = 48000;
		constexpr int32 NumSamples = 256;
		FTestFixture TestFixture(SampleRate, NumSamples, *this, false);

		constexpr int32 NumBlocksToRender = 200;
		FTestFixture::FParams Params;
		Params.DelayTime = 0;
		Params.LFOEnabled = true;

		// test with defaults
		{
			TestFixture.SetParams(Params);
			for (int32 i = 0; i < NumBlocksToRender; ++i)
			{
				UTEST_TRUE(FString::Printf(TEXT("Render test iteration %i"), i), TestFixture.RenderAndCompare(i == 0));
			}
		}
		
		return true;
	}
}

#endif
