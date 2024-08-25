// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundParamHelper.h"
#include "MetasoundSampleCounter.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundVertex.h"

#include "HarmonixMetasound/Common.h"
#include "HarmonixMetasound/DataTypes/MidiClock.h"
#include "HarmonixMetasound/DataTypes/MusicTransport.h"
#include <algorithm>

#include "HAL/PlatformMath.h"

#define LOCTEXT_NAMESPACE "HarmonixMetaSound"

DEFINE_LOG_CATEGORY_STATIC(LogMetronomeNode, Log, All);

namespace HarmonixMetasound
{
	using namespace Metasound;

	class FMetronomeOperator : public TExecutableOperator<FMetronomeOperator>, public FMusicTransportControllable
	{
	public:
		static const FNodeClassMetadata& GetNodeInfo();
		static const FVertexInterface& GetVertexInterface();
		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults);

		FMetronomeOperator(const FBuildOperatorParams& InParams,
						   const FMusicTransportEventStreamReadRef& InTransport,
						   const bool  InLoop,
						   const int32 InLoopLengthBars, 
						   const FInt32ReadRef& InTimSigNumerator,
						   const FInt32ReadRef& InTimeSigDenominator,
						   const FFloatReadRef& InTempo,
		                   const FFloatReadRef& InSpeedMultiplier,
		                   const int32 InSeekPrerollBars);

		virtual void BindInputs(FInputVertexInterfaceData& InVertexData) override;
		virtual void BindOutputs(FOutputVertexInterfaceData& InVertexData) override;
		virtual FDataReferenceCollection GetInputs() const override;
		virtual FDataReferenceCollection GetOutputs() const override;

		void Reset(const FResetParams& Params);
		void Execute();

	private:
		void Init();
		
		//** INPUTS
		FMusicTransportEventStreamReadRef TransportInPin;
		const bool  LoopInPin;
		const int32 LoopLengthBarsInPin;
		FInt32ReadRef TimeSigNumInPin;
		FInt32ReadRef TimeSigDenomInPin;
		FFloatReadRef TempoInPin;
		FFloatReadRef SpeedMultInPin;
		const int32 SeekPreRollBarsInPin;

		//** OUTPUTS
		FMidiClockWriteRef MidiClockOutPin;

		//** DATA
		FMidiClock MetronomeClock;
		TSharedPtr<FMidiFileData> MidiData;
		FSampleCount BlockSize;
		float        SampleRate;
		float        CurrentTempo;
		int32        CurrentTimeSigNum;
		int32        CurrentTimeSigDenom;
		int32		 LastClockTickUpdate = -1;

		void BuildMidiData(bool ResetToStart = true);
		void UpdateMidi();
		void AddTempoChangeForMidi(float TempoBPM);
		void AddTimeSigChangeForMidi(int32 TimeSigNum, int32 TimeSigDenom);
		void HandleTransportChange(int32 StartFrameIndex, EMusicPlayerTransportState NewTransportState);

		FMidiClock& GetDrivingMidiClock() { return LoopInPin ? MetronomeClock : (*MidiClockOutPin); }
	};

	class FMetronomeNode : public FNodeFacade
	{
	public:
		FMetronomeNode(const FNodeInitData& InInitData)
			: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<FMetronomeOperator>())
		{}
		virtual ~FMetronomeNode() = default;
	};

	METASOUND_REGISTER_NODE(FMetronomeNode)

	const FNodeClassMetadata& FMetronomeOperator::GetNodeInfo()
	{
		auto InitNodeInfo = []() -> FNodeClassMetadata
		{
			FNodeClassMetadata Info;
			Info.ClassName        = { HarmonixNodeNamespace, TEXT("Metronome"), TEXT("") };
			Info.MajorVersion     = 0;
			Info.MinorVersion     = 1;
			Info.DisplayName      = METASOUND_LOCTEXT("MetronomeNode_DisplayName", "Metronome MIDI Clock Generator");
			Info.Description      = METASOUND_LOCTEXT("MetronomeNode_Description", "Provides a MIDI clock at the specified tempo and speed.");
			Info.Author           = PluginAuthor;
			Info.PromptIfMissing  = PluginNodeMissingPrompt;
			Info.DefaultInterface = GetVertexInterface();
			Info.CategoryHierarchy = { MetasoundNodeCategories::Harmonix, NodeCategories::Music };

			return Info;
		};

		static const FNodeClassMetadata Info = InitNodeInfo();

		return Info;
	}

	const FVertexInterface& FMetronomeOperator::GetVertexInterface()
	{
		using namespace CommonPinNames;

		static const FVertexInterface Interface(
			FInputVertexInterface(
				TInputDataVertex<FMusicTransportEventStream>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::Transport)),
				TInputConstructorVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::Loop), false),
				TInputConstructorVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::LoopLengthBars), 4),
				TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::TimeSigNumerator), 4),
				TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::TimeSigDenominator), 4),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::Tempo), 120.0f),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::Speed), 1.0f),
				TInputConstructorVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::PrerollBars), 8)
			),
			FOutputVertexInterface(
				TOutputDataVertex<FMidiClock>(METASOUND_GET_PARAM_NAME_AND_METADATA(Outputs::MidiClock))
			)
		);

		return Interface;
	}

	TUniquePtr<IOperator> FMetronomeOperator::CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
	{
		using namespace CommonPinNames;

		const FMetronomeNode& TempoClockNode = static_cast<const FMetronomeNode&>(InParams.Node);

		const FInputVertexInterfaceData& InputData = InParams.InputData;
		const FOperatorSettings& Settings = InParams.OperatorSettings;

		FMusicTransportEventStreamReadRef InTransport = InputData.GetOrConstructDataReadReference<FMusicTransportEventStream>(METASOUND_GET_PARAM_NAME(Inputs::Transport), Settings);
		bool InLoop = InputData.GetOrCreateDefaultValue<bool>(METASOUND_GET_PARAM_NAME(Inputs::Loop), Settings);
		int32 InLoopLengthBars = InputData.GetOrCreateDefaultValue<int32>(METASOUND_GET_PARAM_NAME(Inputs::LoopLengthBars), Settings);
		FInt32ReadRef InTimeSigNumerator   = InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(Inputs::TimeSigNumerator), Settings);
		FInt32ReadRef InTimeSigDenominator = InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(Inputs::TimeSigDenominator), Settings);
		FFloatReadRef InTempo = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(Inputs::Tempo), Settings);
		FFloatReadRef InSpeed = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(Inputs::Speed), Settings);
		int32 InPreRollBars = InputData.GetOrCreateDefaultValue<int32>(METASOUND_GET_PARAM_NAME(Inputs::PrerollBars), Settings);

		return MakeUnique<FMetronomeOperator>(InParams, InTransport, InLoop, InLoopLengthBars, InTimeSigNumerator, InTimeSigDenominator, InTempo, InSpeed, InPreRollBars);
	}

	FMetronomeOperator::FMetronomeOperator(const FBuildOperatorParams& InParams, 
	                                       const FMusicTransportEventStreamReadRef& InTransport,
										   const bool  InLoop,
										   const int32 InLoopLengthBars,
	                                       const FInt32ReadRef& InTimSigNumerator,
	                                       const FInt32ReadRef& InTimeSigDenominator,
	                                       const FFloatReadRef& InTempo,
	                                       const FFloatReadRef& InSpeedMultiplier,
		                                   const int32 InPreRollBars)
		: FMusicTransportControllable(EMusicPlayerTransportState::Prepared) 
		, TransportInPin(InTransport)
		, LoopInPin(InLoop)
		, LoopLengthBarsInPin(InLoopLengthBars)
		, TimeSigNumInPin(InTimSigNumerator)
		, TimeSigDenomInPin(InTimeSigDenominator)
		, TempoInPin(InTempo)
		, SpeedMultInPin(InSpeedMultiplier)
		, SeekPreRollBarsInPin(InPreRollBars)
		, MidiClockOutPin(FMidiClockWriteRef::CreateNew(InParams.OperatorSettings))
		, MetronomeClock(InParams.OperatorSettings)
		, BlockSize(InParams.OperatorSettings.GetNumFramesPerBlock())
		, SampleRate(InParams.OperatorSettings.GetSampleRate())
		, CurrentTempo(*TempoInPin)
		, CurrentTimeSigNum(FMath::Clamp(*TimeSigNumInPin, 1, 64))
		, CurrentTimeSigDenom(FMath::Clamp(*TimeSigDenomInPin, 1, 64))
	{
		if (LoopInPin)
		{
			MidiClockOutPin->AttachToTimeAuthority(MetronomeClock);
		}

		Reset(InParams);

		Init();
	}

	void FMetronomeOperator::BindInputs(FInputVertexInterfaceData& InVertexData)
	{
		using namespace CommonPinNames;
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::Transport), TransportInPin);
		InVertexData.SetValue(METASOUND_GET_PARAM_NAME(Inputs::Loop), LoopInPin);
		InVertexData.SetValue(METASOUND_GET_PARAM_NAME(Inputs::LoopLengthBars), LoopLengthBarsInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::TimeSigNumerator), TimeSigNumInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::TimeSigDenominator), TimeSigDenomInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::Tempo), TempoInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::Speed), SpeedMultInPin);
		InVertexData.SetValue(METASOUND_GET_PARAM_NAME(Inputs::PrerollBars), SeekPreRollBarsInPin);

		Init();
	}

	void FMetronomeOperator::BindOutputs(FOutputVertexInterfaceData& InVertexData)
	{
		using namespace CommonPinNames;
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Outputs::MidiClock), MidiClockOutPin);
	}

	FDataReferenceCollection FMetronomeOperator::GetInputs() const
	{
		// This should never be called. Bind(...) is called instead. This method
		// exists as a stop-gap until the API can be deprecated and removed.
		checkNoEntry();
		return {};
	}

	FDataReferenceCollection FMetronomeOperator::GetOutputs() const
	{
		// This should never be called. Bind(...) is called instead. This method
		// exists as a stop-gap until the API can be deprecated and removed.
		checkNoEntry();
		return {};
	}

	void FMetronomeOperator::Reset(const FResetParams& Params)
	{
		BlockSize = Params.OperatorSettings.GetNumFramesPerBlock();
		SampleRate = Params.OperatorSettings.GetSampleRate();
		
		LastClockTickUpdate = -1;
	}

	void FMetronomeOperator::Init()
	{
		BuildMidiData();
		
		FTransportInitFn InitFn = [this](EMusicPlayerTransportState CurrentState)
		{
			FMidiClock& DrivingMidiClock = GetDrivingMidiClock();
			switch (CurrentState)
			{
			case EMusicPlayerTransportState::Starting:
				DrivingMidiClock.ResetAndStart(0, true);
				break;

			case EMusicPlayerTransportState::Playing:
				DrivingMidiClock.WriteAdvance(0, 0, *SpeedMultInPin);
				break;
			}

			const EMusicPlayerTransportState NextState = GetNextTransportState(CurrentState);
			HandleTransportChange(0, NextState);
			return NextState;
		};
		
		FMusicTransportControllable::Init(*TransportInPin, MoveTemp(InitFn));
	}

	void FMetronomeOperator::Execute()
	{
		MidiClockOutPin->PrepareBlock();

		FMidiClock& DrivingMidiClock = GetDrivingMidiClock();
		DrivingMidiClock.PrepareBlock();

		if (*SpeedMultInPin != DrivingMidiClock.GetSpeedAtEndOfBlock())
		{
			DrivingMidiClock.AddSpeedChangeToBlock({ 0, 0.0f, *SpeedMultInPin });
		}

		// only update our midi data if the clock is advancing
		// update the tempo and time sig before we advance our clock 
		int32 ClockTick = DrivingMidiClock.GetCurrentHiResTick();
		if (ClockTick >= 0 && ClockTick > LastClockTickUpdate)
		{
			UpdateMidi();
			LastClockTickUpdate = ClockTick;
		}

		TransportSpanPostProcessor HandleMidiClockEvents = [this](int32 StartFrameIndex, int32 EndFrameIndex, EMusicPlayerTransportState CurrentState)
		{
			int32 NumFrames = EndFrameIndex - StartFrameIndex;
			HandleTransportChange(StartFrameIndex, CurrentState);
			if (MidiClockOutPin->DoesLoop())
			{
				MetronomeClock.Process(StartFrameIndex, NumFrames, SeekPreRollBarsInPin, *SpeedMultInPin);
				MidiClockOutPin->Process(MetronomeClock, StartFrameIndex, NumFrames, SeekPreRollBarsInPin, *SpeedMultInPin);
			}
			else
			{
				MidiClockOutPin->Process(StartFrameIndex, NumFrames, SeekPreRollBarsInPin, *SpeedMultInPin);
			}
		};

		TransportSpanProcessor TransportHandler = [this, &DrivingMidiClock](int32 StartFrameIndex, int32 EndFrameIndex, EMusicPlayerTransportState CurrentState)
		{
			switch (CurrentState)
			{
			case EMusicPlayerTransportState::Starting:
				// Play from the beginning if we haven't received a seek call while we were stopped...
				if (!ReceivedSeekWhileStopped())
				{
					BuildMidiData(true);
					LastClockTickUpdate = -1;
				}
				DrivingMidiClock.ResetAndStart(StartFrameIndex, !ReceivedSeekWhileStopped());
				return EMusicPlayerTransportState::Playing;
				
			case EMusicPlayerTransportState::Seeking:
				BuildMidiData(false);
				DrivingMidiClock.SeekTo(StartFrameIndex, TransportInPin->GetNextSeekDestination(), SeekPreRollBarsInPin);
				LastClockTickUpdate = DrivingMidiClock.GetCurrentMidiTick();
				// Here we will return that we want to be in the same state we were in before this request to 
				// seek since we can seek "instantaneously"...
				return GetTransportState();
			}

			return GetNextTransportState(CurrentState);
		};
		ExecuteTransportSpans(TransportInPin, BlockSize, TransportHandler, HandleMidiClockEvents);

		if (LoopInPin)
		{
			MidiClockOutPin->CopySpeedAndTempoChanges(&MetronomeClock);
		}
	}

	void FMetronomeOperator::BuildMidiData(bool ResetToStart)
	{
		// make sure we have valid values
		CurrentTempo = FMath::Max(1.0f, *TempoInPin);
		CurrentTimeSigNum = FMath::Max(1, *TimeSigNumInPin);
		CurrentTimeSigDenom = FMath::Max(1, *TimeSigDenomInPin);
	
		MidiData = FMidiClock::MakeClockConductorMidiData(CurrentTempo, CurrentTimeSigNum, CurrentTimeSigDenom);
		
		if (LoopInPin)
		{
			MetronomeClock.AttachToMidiResource(MidiData, ResetToStart);

			// midi clock out will follow the tempo of the metronome
			// so just assign it to some reasonable values. They will be ignored
			TSharedPtr<FMidiFileData> MidiDataOut = FMidiClock::MakeClockConductorMidiData(120.0f, CurrentTimeSigNum, CurrentTimeSigDenom);
			MidiClockOutPin->AttachToMidiResource(MidiDataOut, ResetToStart);
			
			int32 LoopEndTick = MidiDataOut->SongMaps.GetBarMap().BarIncludingCountInToTick(FMath::Max(LoopLengthBarsInPin, 1));
			MidiClockOutPin->SetLoop(0, LoopEndTick);
		}
		else
		{
			// if we're not looping, then the MidiClockOut is going to be the driving midi clock
			// so attach the midi data directly to it
			MidiClockOutPin->AttachToMidiResource(MidiData);
			MidiClockOutPin->ClearLoop();
		}
	}

	void FMetronomeOperator::UpdateMidi()
	{
		FMidiClock& DrivingMidiClock = GetDrivingMidiClock();
		bool HasMidiChanges = false;
		if (*TempoInPin > 0 && !FMath::IsNearlyEqual(CurrentTempo, *TempoInPin))
		{
			if (!HasMidiChanges)
			{
				DrivingMidiClock.LockForMidiDataChanges();
			}
			AddTempoChangeForMidi(*TempoInPin);
			HasMidiChanges = true;
		};

		int32 InTimeSigNum = FMath::Clamp(*TimeSigNumInPin, 1, 64);
		int32 InTimeSigDenom = FMath::Clamp(*TimeSigDenomInPin, 1, 64);
		if (InTimeSigNum != CurrentTimeSigNum || InTimeSigDenom != CurrentTimeSigDenom)
		{
			CurrentTimeSigNum = InTimeSigNum;
			CurrentTimeSigDenom = InTimeSigDenom;
			if (ensure(!MidiClockOutPin->DoesLoop()))
			{
				if (!HasMidiChanges)
				{
					DrivingMidiClock.LockForMidiDataChanges();
				}
				AddTimeSigChangeForMidi(InTimeSigNum, InTimeSigDenom);
				HasMidiChanges = true;
			}
			else
			{
				UE_LOG(LogMetronomeNode, Warning, TEXT("Changing Time Sig. on looping metronome not supported." 
					"Changing time signature will require changing loop length which is currently not supported."))
			}
		}

		if (HasMidiChanges)
		{
			DrivingMidiClock.MidiDataChangesComplete();
		}
	}

	void FMetronomeOperator::AddTempoChangeForMidi(float InTempoBPM)
	{
		CurrentTempo = InTempoBPM;
		int32 AtTick = GetDrivingMidiClock().GetCurrentHiResTick() + 1;
		MidiData->AddTempoChange(0, AtTick, CurrentTempo);
	}

	void FMetronomeOperator::AddTimeSigChangeForMidi(int32 InTimeSigNum, int32 InTimeSigDenom)
	{
		CurrentTimeSigNum = InTimeSigNum;
		CurrentTimeSigDenom = InTimeSigDenom;
		int32 AtTick = GetDrivingMidiClock().GetCurrentHiResTick() + 1;
		// round to the next bar boundary, the bar we're actually going to apply the time sig change to
		int32 AtBar = FMath::CeilToInt32(MidiData->SongMaps.GetBarIncludingCountInAtTick(AtTick));
		int32 NumTimeSigPoints = MidiData->SongMaps.GetBarMap().GetNumTimeSignaturePoints();

		// check if there's already a time signature point at the bar we're trying to update
		// the metronome clock increases monotonically, so we just have to check the _last_ point
		FTimeSignaturePoint& Point = MidiData->SongMaps.GetBarMap().GetTimeSignaturePoint(NumTimeSigPoints - 1);
		if (Point.BarIndex == AtBar)
		{
			// update that instead of adding a new one
			Point.TimeSignature.Numerator = InTimeSigNum;
			Point.TimeSignature.Denominator = InTimeSigDenom;
		}
		else
		{
			MidiData->AddTimeSigChange(0, AtTick, CurrentTimeSigNum, CurrentTimeSigDenom);
		}
	}

	void FMetronomeOperator::HandleTransportChange(int32 StartFrameIndex, EMusicPlayerTransportState NewTransportState)
	{
		MidiClockOutPin->HandleTransportChange(StartFrameIndex, NewTransportState);
		if (LoopInPin)
		{
			MetronomeClock.HandleTransportChange(StartFrameIndex, NewTransportState);
		}
	}
}

#undef LOCTEXT_NAMESPACE // "HarmonixMetaSound"
