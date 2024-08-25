// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundParamHelper.h"
#include "MetasoundSampleCounter.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundVertex.h"

#include "IAudioProxyInitializer.h"
#include "Harmonix/AudioRenderableProxy.h"

#include "HarmonixMetasound/Common.h"
#include "HarmonixMetasound/DataTypes/MidiStream.h"
#include "HarmonixMetasound/DataTypes/MidiClock.h"
#include "HarmonixMetasound/DataTypes/MidiStepSequence.h"
#include "HarmonixMetasound/DataTypes/MusicTransport.h"

#include "HarmonixMidi/MidiPlayCursor.h"
#include "HarmonixMidi/MidiVoiceId.h"

#define LOCTEXT_NAMESPACE "HarmonixMetaSound"

DEFINE_LOG_CATEGORY_STATIC(LogStepSequencePlayer, Log, All);

namespace HarmonixMetasound
{
	using namespace Metasound;
	using namespace Harmonix;

	class FStepSequencePlayerOperator : public TExecutableOperator<FStepSequencePlayerOperator>, public FMusicTransportControllable, public FMidiVoiceGeneratorBase
	{
	public:
		static const FNodeClassMetadata& GetNodeInfo();
		static const FVertexInterface&   GetVertexInterface();
		static TUniquePtr<IOperator>     CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults);

		FStepSequencePlayerOperator(const FBuildOperatorParams& InParams, 
							        const FMidiStepSequenceAssetReadRef& InSequenceAsset,
							        const FMusicTransportEventStreamReadRef& InTransport,
							        const FMidiClockReadRef& InMidiClockSource,
							        const FFloatReadRef& InSpeedMultiplier,
									const FFloatReadRef& InVelocityMultiplier,
									const FFloatReadRef& InMaxColumns,
									const FFloatReadRef& InAdditionalOctaves,
									const FFloatReadRef& InStepSizeQuarterNotes,
									const FFloatReadRef& InActivePage,
									const FBoolReadRef& InAutoPage,
									const FBoolReadRef& InAutoPagePlaysBlankPages,
									const FBoolReadRef& InLoop,
									const FBoolReadRef& InEnabled);

		virtual ~FStepSequencePlayerOperator() override;

		virtual void BindInputs(FInputVertexInterfaceData& InVertexData) override;
		virtual void BindOutputs(FOutputVertexInterfaceData& InVertexData) override;
		virtual FDataReferenceCollection GetInputs() const override;
		virtual FDataReferenceCollection GetOutputs() const override;
		void Reset(const FResetParams& ResetParams);
		void Execute();

	private:
		void Init();
		void InitSequenceTable();
		
		//** INPUTS
		FMidiStepSequenceAssetReadRef SequenceAssetInPin;
		FMusicTransportEventStreamReadRef TransportInPin;
		FFloatReadRef SpeedMultInPin;
		FFloatReadRef VelocityMultInPin;
		FMidiClockReadRef MidiClockInPin;
		FFloatReadRef MaxColumnsInPin;
		FFloatReadRef AdditionalOctavesInPin;
		FFloatReadRef StepSizeQuarterNotesInPin;
		FFloatReadRef ActivePageInPin;
		FBoolReadRef AutoPageInPin;
		FBoolReadRef AutoPagePlaysBlankPagesInPin;
		FBoolReadRef LoopInPin;
		FBoolReadRef EnabledInPin;

		//** OUTPUTS
		FMidiStreamWriteRef MidiOutPin;

		//** DATA
		TSharedAudioRenderableDataPtr<FStepSequenceTable, TRefCountedAudioRenderableWithQueuedChanges<FStepSequenceTable>> SequenceTable;
		FSampleCount BlockSize			= 0;
		int32 CurrentBlockSpanStart		= 0;
		int32 CurrentPageIndex			= -1;
		int32 CurrentCellIndex			= -1;
		int32 ProcessedThruTick			= -1;
		int32 SequenceStartTick			= -1;
		int32 CurrentStepSkipIndex		= 0;
		bool  bAutoPage					= false;
		bool  bPreviousAutoPage			= false;
		bool  bAutoPagePlaysBlankPages	= false;
		bool  bLoop						= true;
		bool  bEnabled					= true;
		bool  bPlaying					= false;
		bool  bNeedsRebase				= false;
		TArray<FMidiVoiceId> CurrentCellNotes;

		void CheckForUpdatedSequenceTable();
		void ResizeCellStatesForTable();
		void AllNotesOff(int32 AtFrameIndex, int32 AbsMidiTick, bool ResetCellIndex);
		int32 CalculateAutoPageIndex(int32 CurTick, int32 TableTickLength, bool bRound) const;
		int32 CalculatePagesProgressed(const int FromTick, const int ToTick, int32 TableTickLength, const bool bRound) const;
		void CalculatePageProperties(const FStepSequencePage& Page, int32 MaxColumns, float StepSizeQuarterNotes, int32& OutColumns, int32& OutTicksPerCell, int32& OutTableTickLength) const;
		void RebaseSequenceStartTickForLoop(const int32 CurTick, const int32 TableTickLength);
		void EnsureCurrentPageIndexIsValid();

	protected:

		void SeekToTick(int32 BlockFrameIndex, int32 Tick);
		void SeekThruTick(int32 BlockFrameIndex, int32 Tick);
		void AdvanceThruTick(int32 BlockFrameIndex, int32 Tick, bool IsPreRoll);

	};

	class FStepSequencePlayerNode : public FNodeFacade
	{
	public:
		FStepSequencePlayerNode(const FNodeInitData& InInitData)
			: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<FStepSequencePlayerOperator>())
		{}
		virtual ~FStepSequencePlayerNode() = default;
	};

	METASOUND_REGISTER_NODE(FStepSequencePlayerNode)

	const FNodeClassMetadata& FStepSequencePlayerOperator::GetNodeInfo()
	{
		auto InitNodeInfo = []() -> FNodeClassMetadata
		{
			FNodeClassMetadata Info;
			Info.ClassName        = { HarmonixNodeNamespace, TEXT("StepSequencePlayer"), TEXT("") };
			Info.MajorVersion     = 0;
			Info.MinorVersion     = 1;
			Info.DisplayName      = METASOUND_LOCTEXT("StepSequencePlayerNode_DisplayName", "Step Sequence Player");
			Info.Description      = METASOUND_LOCTEXT("StepSequencePlayerNode_Description", "Plays a Step Sequence Asset.");
			Info.Author           = PluginAuthor;
			Info.PromptIfMissing  = PluginNodeMissingPrompt;
			Info.DefaultInterface = GetVertexInterface();
			Info.CategoryHierarchy = { MetasoundNodeCategories::Harmonix, NodeCategories::Music };

			return Info;
		};

		static const FNodeClassMetadata Info = InitNodeInfo();

		return Info;
	}

	namespace StepSequencePlayerPinNames
	{
		METASOUND_PARAM(InputSequenceAsset, "Step Sequence Asset", "Step sequence to play.");
		METASOUND_PARAM(InputVelocityMultiplier, "Velocity Multiplier", "Multiplies the current note velocity by this number");
		METASOUND_PARAM(InputMaxColumns, "Max Columns", "The Maximinum Number of cells to play per step sequence row.");
		METASOUND_PARAM(InputAdditionalOctaves, "Additional Octaves", "The number of octaves to add to the authored step sequence note.");
		METASOUND_PARAM(InputStepSizeQuarterNotes, "Step Size Quarter Notes", "The size, in quarter notes, of each step");
		METASOUND_PARAM(InputActivePage, "Active Page", "The page of the step sequence to play (1 indexed)");
		METASOUND_PARAM(InputAutoPage, "Auto Page", "Whether to calculate the page of the step sequence based on current position");
		METASOUND_PARAM(InputAutoPagePlaysBlankPages, "Auto Page Plays Blank Pages", "If autopaging, should blank pages be played?");
		METASOUND_PARAM(InputLoop, "Loop", "If not looping, the sequence will be played as a one-shot starting on the next stepsizebeats interval (autopaging if it's enabled)");
		METASOUND_PARAM(InputEnabled, "Enabled", "Whether the sequencer is enabled. If loop is off, enabling will trigger to start on next stepsizebeats interval");
	}

	const FVertexInterface& FStepSequencePlayerOperator::GetVertexInterface()
	{
		using namespace StepSequencePlayerPinNames;
		using namespace CommonPinNames;

		static const FVertexInterface Interface(
			FInputVertexInterface(
				TInputDataVertex<FMidiStepSequenceAsset>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSequenceAsset)),
				TInputDataVertex<FMusicTransportEventStream>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::Transport)),
				TInputDataVertex<FMidiClock>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::MidiClock)),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::Speed), 1.0f),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputVelocityMultiplier), 1.0f),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputMaxColumns), 64.0f),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputAdditionalOctaves), 0),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputStepSizeQuarterNotes), 0.25f),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputActivePage), 0),
				TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputAutoPage), false),
				TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputAutoPagePlaysBlankPages), true),
				TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputLoop), true),
				TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputEnabled), true)
			),
			FOutputVertexInterface(
				TOutputDataVertex<FMidiStream>(METASOUND_GET_PARAM_NAME_AND_METADATA(Outputs::MidiStream))
			)
		);
		return Interface;
	}

	TUniquePtr<IOperator> FStepSequencePlayerOperator::CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
	{
		using namespace StepSequencePlayerPinNames;
		using namespace CommonPinNames;

		const FStepSequencePlayerNode& PlayerNode = static_cast<const FStepSequencePlayerNode&>(InParams.Node);

		const FInputVertexInterfaceData& InputData = InParams.InputData;

		FMidiStepSequenceAssetReadRef InSequenceAsset = InputData.GetOrConstructDataReadReference<FMidiStepSequenceAsset>(METASOUND_GET_PARAM_NAME(InputSequenceAsset));
		FMusicTransportEventStreamReadRef InTransport = InputData.GetOrConstructDataReadReference<FMusicTransportEventStream>(METASOUND_GET_PARAM_NAME(Inputs::Transport), InParams.OperatorSettings);
		FFloatReadRef InSpeed = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(Inputs::Speed), InParams.OperatorSettings);
		FFloatReadRef InVelocityMult = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputVelocityMultiplier), InParams.OperatorSettings);
		FMidiClockReadRef InMidiClock = InputData.GetOrConstructDataReadReference<FMidiClock>(METASOUND_GET_PARAM_NAME(Inputs::MidiClock), InParams.OperatorSettings);
		FFloatReadRef InMaxColumns = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputMaxColumns), InParams.OperatorSettings);
		FFloatReadRef InAdditionalOctaves = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputAdditionalOctaves), InParams.OperatorSettings);
		FFloatReadRef InStepSize = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputStepSizeQuarterNotes), InParams.OperatorSettings);
		FFloatReadRef InActivePage = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputActivePage), InParams.OperatorSettings);
		FBoolReadRef InAutoPage = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputAutoPage), InParams.OperatorSettings);
		FBoolReadRef InAutoPagePlaysBlankPages = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputAutoPagePlaysBlankPages), InParams.OperatorSettings);
		FBoolReadRef InLoop = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputLoop), InParams.OperatorSettings);
		FBoolReadRef InEnabled = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputEnabled), InParams.OperatorSettings);
		return MakeUnique<FStepSequencePlayerOperator>(InParams, InSequenceAsset, InTransport, InMidiClock, InSpeed, InVelocityMult, InMaxColumns, InAdditionalOctaves, InStepSize, InActivePage, InAutoPage, InAutoPagePlaysBlankPages, InLoop, InEnabled);
	}

	FStepSequencePlayerOperator::FStepSequencePlayerOperator(const FBuildOperatorParams& InParams,
															 const FMidiStepSequenceAssetReadRef& InSequenceAsset,
															 const FMusicTransportEventStreamReadRef& InTransport,
															 const FMidiClockReadRef& InMidiClockSource,
															 const FFloatReadRef& InSpeedMultiplier,
															 const FFloatReadRef& InVelocityMultiplier,
															 const FFloatReadRef& InMaxColumns,
															 const FFloatReadRef& InAdditionalOctaves,
															 const FFloatReadRef& InStepSizeQuarterNotes,
															 const FFloatReadRef& InActivePage,
															 const FBoolReadRef& InAutoPage,
															 const FBoolReadRef& InAutoPagePlaysBlankPages,
															 const FBoolReadRef& InLoop,
															 const FBoolReadRef& InEnabled)
		: FMusicTransportControllable(EMusicPlayerTransportState::Prepared)
		, SequenceAssetInPin(InSequenceAsset)
		, TransportInPin(InTransport)
		, SpeedMultInPin(InSpeedMultiplier)
		, VelocityMultInPin(InVelocityMultiplier)
		, MidiClockInPin(InMidiClockSource)
		, MaxColumnsInPin(InMaxColumns)
		, AdditionalOctavesInPin(InAdditionalOctaves)
		, StepSizeQuarterNotesInPin(InStepSizeQuarterNotes)
		, ActivePageInPin(InActivePage)
		, AutoPageInPin(InAutoPage)
		, AutoPagePlaysBlankPagesInPin(InAutoPagePlaysBlankPages)
		, LoopInPin(InLoop)
		, EnabledInPin(InEnabled)
		, MidiOutPin(FMidiStreamWriteRef::CreateNew())
	{
		Reset(InParams);
		Init();
	}

	FStepSequencePlayerOperator::~FStepSequencePlayerOperator()
	{
	}

	void FStepSequencePlayerOperator::BindInputs(FInputVertexInterfaceData& InVertexData)
	{
		using namespace StepSequencePlayerPinNames;
		using namespace CommonPinNames;
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSequenceAsset), SequenceAssetInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::Transport), TransportInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::MidiClock), MidiClockInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::Speed), SpeedMultInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputVelocityMultiplier), VelocityMultInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputMaxColumns), MaxColumnsInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputAdditionalOctaves), AdditionalOctavesInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputStepSizeQuarterNotes), StepSizeQuarterNotesInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputActivePage), ActivePageInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputAutoPage), AutoPageInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputAutoPagePlaysBlankPages), AutoPagePlaysBlankPagesInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputLoop), LoopInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputEnabled), EnabledInPin);

		Init();
	}

	void FStepSequencePlayerOperator::BindOutputs(FOutputVertexInterfaceData& InVertexData)
	{
		using namespace CommonPinNames;
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Outputs::MidiStream), MidiOutPin);
	}

	FDataReferenceCollection FStepSequencePlayerOperator::GetInputs() const
	{
		// This should never be called. Bind(...) is called instead. This method
		// exists as a stop-gap until the API can be deprecated and removed.
		checkNoEntry();
		return {};
	}

	FDataReferenceCollection FStepSequencePlayerOperator::GetOutputs() const
	{
		// This should never be called. Bind(...) is called instead. This method
		// exists as a stop-gap until the API can be deprecated and removed.
		checkNoEntry();
		return {};
	}

	void FStepSequencePlayerOperator::Reset(const FResetParams& ResetParams)
	{
		BlockSize = ResetParams.OperatorSettings.GetNumFramesPerBlock();
		CurrentBlockSpanStart = 0;
		CurrentPageIndex = -1;
		CurrentCellIndex = -1;
		ProcessedThruTick = -1;
		SequenceStartTick = -1;
		bAutoPage = false;
		bPreviousAutoPage = false;
		bAutoPagePlaysBlankPages = false;
		bLoop = true;
		bEnabled = true;
		bPlaying = false;
		bNeedsRebase = false;
		CurrentCellNotes.Reset();
	}

	void FStepSequencePlayerOperator::Execute()
	{
		MidiOutPin->PrepareBlock();

		// if we have no sequence table there is nothing to do. 
		// Make sure the notes are all off and return.
		if (!SequenceTable)
		{
			if (CurrentCellNotes.Num() > 0)
			{
				AllNotesOff(0, MidiClockInPin->GetCurrentMidiTick(), true);
				CurrentCellNotes.Empty();
			}
			return;
		}

		CheckForUpdatedSequenceTable();

		// We need to cache this to avoid avoid a crash if the value in the 
		// sequence table asset changes while we are in the middle of rendering. 
		CurrentStepSkipIndex = SequenceTable->StepSkipIndex;

		TransportSpanPostProcessor ClockEventHandler = [&](int32 StartFrameIndex, int32 EndFrameIndex, EMusicPlayerTransportState State)
			{
				switch (State)
				{
				case EMusicPlayerTransportState::Playing:
				case EMusicPlayerTransportState::Continuing:
					for (const FMidiClockEvent& Event : MidiClockInPin->GetMidiClockEventsInBlock())
					{
						if (Event.BlockFrameIndex >= EndFrameIndex)
						{
							break;
						}

						if (Event.BlockFrameIndex >= StartFrameIndex)
						{
							switch (Event.Msg.Type)
							{
							case FMidiClockMsg::EType::AdvanceThru:
								AdvanceThruTick(Event.BlockFrameIndex, Event.Msg.ThruTick(), Event.Msg.AsAdvanceThru().IsPreRoll);
								break;
							case FMidiClockMsg::EType::SeekThru:
								SeekThruTick(Event.BlockFrameIndex, Event.Msg.ThruTick());
								break;
							case FMidiClockMsg::EType::SeekTo:
								SeekToTick(Event.BlockFrameIndex, Event.Msg.ToTick());
								break;
							}
						}
					}
				}
			};

		TransportSpanProcessor TransportHandler = [&](int32 StartFrameIndex, int32 EndFrameIndex, EMusicPlayerTransportState CurrentState)
			{
				CurrentBlockSpanStart = StartFrameIndex;
				switch (CurrentState)
				{
				case EMusicPlayerTransportState::Invalid:
				case EMusicPlayerTransportState::Preparing:
					return EMusicPlayerTransportState::Prepared;
				
				case EMusicPlayerTransportState::Prepared:
					return CurrentState;
				
				case EMusicPlayerTransportState::Starting:
					return EMusicPlayerTransportState::Playing;

				case EMusicPlayerTransportState::Playing:
					return EMusicPlayerTransportState::Playing;

				case EMusicPlayerTransportState::Seeking:
					return GetTransportState();

				case EMusicPlayerTransportState::Continuing:
					return EMusicPlayerTransportState::Playing;

				case EMusicPlayerTransportState::Pausing:
					return EMusicPlayerTransportState::Paused;

				case EMusicPlayerTransportState::Paused:
					return EMusicPlayerTransportState::Paused;

				case EMusicPlayerTransportState::Stopping:
					AllNotesOff(StartFrameIndex, MidiClockInPin->GetCurrentMidiTick(), true);
					return EMusicPlayerTransportState::Prepared;

				case EMusicPlayerTransportState::Killing:
					AllNotesOff(StartFrameIndex, MidiClockInPin->GetCurrentMidiTick(), true);
					return EMusicPlayerTransportState::Prepared;

				default:
					checkNoEntry();
					return EMusicPlayerTransportState::Invalid;
				}
			};
		ExecuteTransportSpans(TransportInPin, BlockSize, TransportHandler, ClockEventHandler);
	}

	void FStepSequencePlayerOperator::Init()
	{
		MidiOutPin->SetClock(*MidiClockInPin);
		MidiOutPin->PrepareBlock();

		InitSequenceTable();

		FTransportInitFn InitFn = [this](EMusicPlayerTransportState CurrentState)
		{
			CurrentBlockSpanStart = 0;
			switch (CurrentState)
			{
			case EMusicPlayerTransportState::Invalid:
			case EMusicPlayerTransportState::Preparing:
				return EMusicPlayerTransportState::Prepared;
				
			case EMusicPlayerTransportState::Prepared:
				return CurrentState;
				
			case EMusicPlayerTransportState::Starting:
				return EMusicPlayerTransportState::Playing;

			case EMusicPlayerTransportState::Playing:
				return EMusicPlayerTransportState::Playing;

			case EMusicPlayerTransportState::Seeking:
				return GetTransportState();

			case EMusicPlayerTransportState::Continuing:
				return EMusicPlayerTransportState::Playing;

			case EMusicPlayerTransportState::Pausing:
				return EMusicPlayerTransportState::Paused;

			case EMusicPlayerTransportState::Paused:
				return EMusicPlayerTransportState::Paused;

			case EMusicPlayerTransportState::Stopping:
				AllNotesOff(0, MidiClockInPin->GetCurrentMidiTick(), true);
				return EMusicPlayerTransportState::Prepared;

			case EMusicPlayerTransportState::Killing:
				AllNotesOff(0, MidiClockInPin->GetCurrentMidiTick(), true);
				return EMusicPlayerTransportState::Prepared;

			default:
				checkNoEntry();
				return EMusicPlayerTransportState::Invalid;
			}
		};
		FMusicTransportControllable::Init(*TransportInPin, MoveTemp(InitFn));
	}

	void FStepSequencePlayerOperator::InitSequenceTable()
	{
		SequenceTable = SequenceAssetInPin->GetRenderable();
		if (SequenceTable)
		{
			ResizeCellStatesForTable();
			UE_LOG(LogStepSequencePlayer, Verbose, TEXT("Got a Sequence: %d pages, %d rows, %d columns"),
				SequenceTable->Pages.Num(),
				SequenceTable->Pages.IsEmpty() ? 0 : SequenceTable->Pages[0].Rows.Num(),
				SequenceTable->Pages.IsEmpty() || SequenceTable->Pages[0].Rows.IsEmpty() ? 0 : SequenceTable->Pages[0].Rows[0].Cells.Num());
		}
		else
		{
			UE_LOG(LogStepSequencePlayer, Verbose, TEXT("No Sequence Provided!"));
		}
	}

	void FStepSequencePlayerOperator::SeekToTick(int32 BlockFrameIndex, int32 Tick)
	{
		SeekThruTick(BlockFrameIndex, Tick-1);
	}

	void FStepSequencePlayerOperator::SeekThruTick(int32 BlockFrameIndex ,int32 Tick)
	{
		ProcessedThruTick = FMath::Max(-1, Tick);
		bNeedsRebase = true;
		AllNotesOff(BlockFrameIndex, Tick, true);
	}

	int32 FStepSequencePlayerOperator::CalculatePagesProgressed(const int FromTick, const int ToTick, int32 TableTickLength, const bool bRound) const
	{
		const float PagesProgressed = (static_cast<float>(ToTick - FromTick) / static_cast<float>(TableTickLength));
		return bRound ? FMath::RoundToInt32(PagesProgressed) : FMath::FloorToInt32(PagesProgressed);
	}

	int32 FStepSequencePlayerOperator::CalculateAutoPageIndex(int32 CurTick, int32 TableTickLength, bool bRound) const
	{
		const int32 PagesProgressed = CalculatePagesProgressed(SequenceStartTick, CurTick, TableTickLength, bRound);
		int32 TargetPageIndex = SequenceTable->CalculateAutoPageIndex(PagesProgressed, bAutoPagePlaysBlankPages, bLoop);

		if (TargetPageIndex != INDEX_NONE && !SequenceTable->Pages.IsValidIndex(TargetPageIndex))
		{
			TargetPageIndex = 0;
		}

		// Page Index is 1-based. If INDEX_NONE is returned from the above, this will be 0 and therefore signals to be nonplaying
		++TargetPageIndex;
		return TargetPageIndex;
	}

	void FStepSequencePlayerOperator::CalculatePageProperties(const FStepSequencePage& Page, int32 MaxColumns, float StepSizeQuarterNotes, int32& OutColumns, int32& OutTicksPerCell, int32& OutTableTickLength) const
	{		
		OutColumns = FMath::Min(Page.Rows[0].Cells.Num(), (MaxColumns < 1 ? 1 : MaxColumns));

		int32 LengthColumns = OutColumns;
		
		// Lower the total number of columns to reflect the number of steps skipped
		// This value is used only for calculating the tick length
		if (CurrentStepSkipIndex > 0)
		{
			LengthColumns -= LengthColumns / CurrentStepSkipIndex;
			LengthColumns = FMath::Max(LengthColumns, 1);
		}

		// Min float possible is 64th note triplets.
		OutTicksPerCell = FMath::Max(StepSizeQuarterNotes, 0.0416666f) * Midi::Constants::GTicksPerQuarterNote;
		OutTableTickLength = OutTicksPerCell * LengthColumns;
	}

	void FStepSequencePlayerOperator::RebaseSequenceStartTickForLoop(const int32 CurTick, const int32 TableTickLength)
	{
		if (CurTick == INDEX_NONE)
		{
			SequenceStartTick = 0;
			CurrentPageIndex = -1;
			bNeedsRebase = false;
			return;
		}

		const int32 PagesProgressed = CalculatePagesProgressed(0, CurTick, TableTickLength, false);
		const int32 TickInPage = TableTickLength == 0 ? 0 : CurTick % TableTickLength;
		const int32 NumValidPages = bAutoPage ? SequenceTable->CalculateNumValidPages(bAutoPagePlaysBlankPages) : 1;
		const int32 PagesSinceStart = NumValidPages == 0 ? 0 : PagesProgressed % NumValidPages;

		SequenceStartTick = CurTick - (PagesSinceStart * TableTickLength) - TickInPage;
		CurrentPageIndex = CalculateAutoPageIndex(CurTick, TableTickLength, false);
		bNeedsRebase = false;
	}

	void FStepSequencePlayerOperator::EnsureCurrentPageIndexIsValid()
	{
		if (bAutoPage)
		{
			// Ensure we don't process erroneous -1, conversion to 1-based first page
			CurrentPageIndex = FMath::Abs(CurrentPageIndex);
			if (CurrentPageIndex == 0)
			{
				// The loop ended in a previous tick, bPlaying should be false here.
				// Reset the current index, and we'll wait to start playing again.
				CurrentPageIndex = SequenceTable->GetFirstValidPage(bAutoPagePlaysBlankPages) + 1;
			}
		}
		else
		{
			CurrentPageIndex = FMath::Clamp((int32)*ActivePageInPin, 1, SequenceTable->Pages.Num());
		}
	}

	void FStepSequencePlayerOperator::AdvanceThruTick(int32 BlockFrameIndex, int32 Tick, bool IsPreRoll)
	{
		if (!SequenceTable || SequenceTable->Pages.IsEmpty())
		{
			return;
		}

		// Read input pins
		const int32 CurrentMaxColumns = *MaxColumnsInPin;
		const int32 AdditionalOctaveNotes = (int32)*AdditionalOctavesInPin * 12;
		const float CurrentStepSizeQuarterNotes = *StepSizeQuarterNotesInPin;
		const float CurrentVelocityMultiplierValue = *VelocityMultInPin;

		if (CurrentMaxColumns < 1)
		{
			// Nothing to play
			return;
		}

		if (!bAutoPage && *AutoPageInPin)
		{
			bNeedsRebase = true;
		}

		bAutoPage = *AutoPageInPin;
		bAutoPagePlaysBlankPages = *AutoPagePlaysBlankPagesInPin;

		EnsureCurrentPageIndexIsValid();
		if (!SequenceTable->Pages.IsValidIndex(CurrentPageIndex - 1))
		{
			// WE SHOULD NEVER HIT THIS
			return;
		}
		FStepSequencePage* CurrentPage = &(SequenceTable->Pages[CurrentPageIndex - 1]);

		// Do an initial calc with the page we think we're on
		int32 Columns;
		int32 TicksPerCell;
		int32 TableTickLength;
		CalculatePageProperties(*CurrentPage, CurrentMaxColumns, CurrentStepSizeQuarterNotes, Columns, TicksPerCell, TableTickLength);
		
		bool bNewPlaying = bEnabled;

		if (*EnabledInPin != bEnabled || (*LoopInPin && !bLoop))
		{
			// If our enable state changes or we turn loop on, control playback
			bEnabled = *EnabledInPin;
			bNewPlaying = bEnabled;
		}
		else if (*EnabledInPin && *LoopInPin)
		{
			// Force playing to true if enabled and looping
			bNewPlaying = true;
		}

		if (bPlaying)
		{
			if (!bNewPlaying)
			{
				// We're not playing anymore, make sure we're all off
				AllNotesOff(BlockFrameIndex, Tick, true);
			}
			else if (*LoopInPin != bLoop)
			{
				// If we're already playing and change the loop setting, need to re-sync to the global timeline
				bNeedsRebase = true;
			}
		}
		else if (bNewPlaying)
		{
			// We weren't playing before, but now we are
			if (*LoopInPin)
			{
				bNeedsRebase = true;
			}
			else
			{
				// Begin the oneshot on the next beat subdivision
				SequenceStartTick = ProcessedThruTick + TicksPerCell - (ProcessedThruTick % TicksPerCell);
				if (bAutoPage)
				{
					// Nonlooping autopaging sequences start on the first valid page.
					CurrentPageIndex = SequenceTable->GetFirstValidPage(bAutoPagePlaysBlankPages) + 1;
				}
				bNeedsRebase = false;
			}
		}

		bPlaying = bNewPlaying;
		bLoop = *LoopInPin;

		// TODO When IsPreRoll we should probably be doing some of the handling of note ons/offs differently (more like FMidiPlayCursor::AdvanceThruTick), and therefore potentially our timeline sync as well
		if (bNeedsRebase)
		{
			if (!bLoop)
			{
				// Temp: Don't rebase if loop is off, so we don't lose our start tick on graph rebuilds.
				// NOTE: This introduces a bug where turning loop off in the middle of a sequence will cause a visual desync, but with loop in the Customize menu that's not a major concern.
				bNeedsRebase = false;
			}
			else
			{
				RebaseSequenceStartTickForLoop(ProcessedThruTick, TableTickLength);
				// Our page index may have changed, let's set it again
				EnsureCurrentPageIndexIsValid();
				if (!SequenceTable->Pages.IsValidIndex(CurrentPageIndex - 1))
				{
					// WE SHOULD NEVER HIT THIS
					return;
				}
				CurrentPage = &(SequenceTable->Pages[CurrentPageIndex - 1]);
				// Uncomment if there is ever a time where pages can have different tick lengths
				// CalculatePageProperties(*CurrentPage, CurrentMaxColumns, CurrentStepSizeQuarterNotes, Columns, TicksPerCell, TableTickLength);
			}
		}

		ProcessedThruTick = FMath::Max(-1, ProcessedThruTick);
		
		while (ProcessedThruTick < Tick)
		{
			ProcessedThruTick++;

			if (!bPlaying || ProcessedThruTick < SequenceStartTick)
			{
				// Loop has ended or hasn't started yet
				continue;
			}

			int32 EffectiveTickForLoopPosition = ProcessedThruTick;

			if (!bLoop)
			{
				// If loop is off, we want to position ourselves relative to when the sequence was triggered
				EffectiveTickForLoopPosition -= SequenceStartTick;

				if ((bAutoPage && CurrentPageIndex == 0) || (!bAutoPage && EffectiveTickForLoopPosition >= TableTickLength))
				{
					bPlaying = false;
					continue;
				}
			}

			const int32 TickInTable = (TableTickLength == 0) ? 0 : EffectiveTickForLoopPosition % TableTickLength;
			int32 CellInRow = TickInTable / TicksPerCell;
			const int32 TickInCell = TickInTable - (CellInRow * TicksPerCell);
			int32 SkippedIndex = CurrentStepSkipIndex - 1;

			if (CurrentStepSkipIndex >= 2)
			{
				// Skip past the unused cells
				CellInRow += CellInRow / SkippedIndex;
			}

			if (TickInCell >= TicksPerCell - 1)
			{
				int32 NextCellInRow = (CellInRow + 1) % Columns;

				if (CurrentStepSkipIndex >= 2 && NextCellInRow % SkippedIndex == 0)
				{
					// If the next cell is unused, instead check the cell past this
					NextCellInRow = (NextCellInRow + 1) % Columns;
				}

				if (bAutoPage && (NextCellInRow == 0 || bAutoPage != bPreviousAutoPage))
				{
					CurrentPageIndex = CalculateAutoPageIndex(ProcessedThruTick, TableTickLength, true);

					if (CurrentPageIndex == 0)
					{
						// Loop has ended
						bPlaying = false;
						continue;
					}
					else if (!SequenceTable->Pages.IsValidIndex(CurrentPageIndex - 1))
					{
						// WE SHOULD NEVER HIT THIS
						return;
					}

					CurrentPage = &(SequenceTable->Pages[CurrentPageIndex - 1]);
					CalculatePageProperties(*CurrentPage, CurrentMaxColumns, CurrentStepSizeQuarterNotes, Columns, TicksPerCell, TableTickLength);
				}	
				bPreviousAutoPage = bAutoPage;

				for (int32 i = 0; i < CurrentPage->Rows.Num(); ++i)
				{
					if (CurrentCellNotes[i])
					{
						int32 NoteIndex = i;
						uint8 MidiCh;
						uint8 MidiNote;
						CurrentCellNotes[i].GetChannelAndNote(MidiCh, MidiNote);
						
						// If: 
						// 1. The note that would play is not enabled (it is not a new note);
						// 2. A note with this pitch would start up this tick;
						// 3. The note is marked as a continuation note
						// Then keep this note playing through the next cell
						if (MidiNote == NoteIndex && !CurrentPage->Rows[i].Cells[NextCellInRow].bEnabled && CurrentPage->Rows[i].Cells[NextCellInRow].bContinuation)
						{
							continue;
						}

						// note off!
						// use the "NoteIndex" as the note number for the midi event to track the midi event for our note off message
						// the note number and transposition of the step sequencer can change out from under us, so we can't use those to id events
						// we transpose the note later
						int32 OriginalNote = SequenceTable->Notes[i].NoteNumber;
						int32 TransposedNote = FMath::Clamp(OriginalNote + AdditionalOctaveNotes, 0, 127);
						FMidiStreamEvent MidiEvent(CurrentCellNotes[i].GetGeneratorId(), FMidiMsg::CreateNoteOff(MidiCh, NoteIndex));
						// and then assign the note directly to the midi message after
						MidiEvent.MidiMessage.Data1 = TransposedNote;
						MidiEvent.BlockSampleFrameIndex = BlockFrameIndex;
						MidiEvent.AuthoredMidiTick = ProcessedThruTick;
						MidiEvent.CurrentMidiTick = ProcessedThruTick;
						MidiEvent.TrackIndex = 1;
						MidiOutPin->AddNoteOffEventOrCancelPendingNoteOn(MidiEvent);
						UE_LOG(LogStepSequencePlayer, Verbose, TEXT("0x%x Note-Off %d at %d"), (uint32)(size_t)this, SequenceTable->Notes[i].NoteNumber + AdditionalOctaveNotes, BlockFrameIndex);
						CurrentCellNotes[i] = FMidiVoiceId::None();
					}
				}
			}

			if (CellInRow != CurrentCellIndex)
			{	
				for (int32 i = 0; i < CurrentPage->Rows.Num(); ++i)
				{
					if (CurrentPage->Rows[i].bRowEnabled && CurrentPage->Rows[i].Cells[CellInRow].bEnabled)
					{
						if (!CurrentCellNotes[i])
						{
							// note on!
							int32 NoteIndex = i;
							int32 OriginalNote = SequenceTable->Notes[i].NoteNumber;
							int32 TransposedNote = FMath::Clamp(OriginalNote + AdditionalOctaveNotes, 0, 127);
							// create the midi event with the original note to maintain voice ids.
							FMidiStreamEvent MidiEvent(this, FMidiMsg::CreateNoteOn(0, NoteIndex, SequenceTable->Notes[i].Velocity));
							// and then assign the note directly to the midi message after
							MidiEvent.MidiMessage.Data1 = TransposedNote;
							float NoteOnVelocity = FMath::Clamp(static_cast<float>(SequenceTable->Notes[i].Velocity) * CurrentVelocityMultiplierValue, 0.0f, 127.0f);
							MidiEvent.MidiMessage.SetNoteOnVelocity(static_cast<uint8>(NoteOnVelocity));
							MidiEvent.BlockSampleFrameIndex = BlockFrameIndex;
							MidiEvent.AuthoredMidiTick = ProcessedThruTick;
							MidiEvent.CurrentMidiTick = ProcessedThruTick;
							MidiEvent.TrackIndex = 1;
							MidiOutPin->AddMidiEvent(MidiEvent);
							UE_LOG(LogStepSequencePlayer, Verbose, TEXT("0x%x Note-On %d at %d"), (uint32)(size_t)this, SequenceTable->Notes[i].NoteNumber + AdditionalOctaveNotes, BlockFrameIndex);
							CurrentCellNotes[i] = MidiEvent.GetVoiceId();
						}
					}
				}
				CurrentCellIndex = CellInRow;
			}
		}

		if (bNewPlaying && !bPlaying)
		{
			// Loop ended above, make sure we turn everything off
			AllNotesOff(BlockFrameIndex, Tick, true);
		}
	}

	void FStepSequencePlayerOperator::CheckForUpdatedSequenceTable()
	{
		FStepSequenceTableProxy::NodePtr Tester = SequenceAssetInPin->GetRenderable();
		if (Tester != SequenceTable)
		{
			InitSequenceTable();
			return;
		}

		if (SequenceTable)
		{
			TRefCountedAudioRenderableWithQueuedChanges<FStepSequenceTable>* Table = SequenceTable;
			if (Table->HasUpdate())
			{
				SequenceTable = Table->GetUpdate();
				ResizeCellStatesForTable();
				UE_LOG(LogStepSequencePlayer, Verbose, TEXT("Got a NEW Sequence: %d pages, %d rows, %d columns"),
					SequenceTable->Pages.Num(),
					SequenceTable->Pages.IsEmpty() ? 0 : SequenceTable->Pages[0].Rows.Num(),
					SequenceTable->Pages.IsEmpty() || SequenceTable->Pages[0].Rows.IsEmpty() ? 0 : SequenceTable->Pages[0].Rows[0].Cells.Num());
			}
		}
	}

	void FStepSequencePlayerOperator::ResizeCellStatesForTable()
	{
		if (!SequenceTable || SequenceTable->Notes.Num() == 0)
		{
			return;
		}

		if (SequenceTable->Notes.Num() < CurrentCellNotes.Num())
		{
			// We may have existing notes that need to be stopped.
			for (int32 i = SequenceTable->Notes.Num(); i < CurrentCellNotes.Num(); ++i)
			{
				if (CurrentCellNotes[i])
				{
					uint8 MidiCh;
					uint8 MidiNote;
					CurrentCellNotes[i].GetChannelAndNote(MidiCh, MidiNote);
					FMidiStreamEvent MidiEvent(CurrentCellNotes[i].GetGeneratorId(), FMidiMsg::CreateNoteOff(MidiCh, MidiNote));
					MidiEvent.BlockSampleFrameIndex  = CurrentBlockSpanStart;
					MidiEvent.AuthoredMidiTick       = 0;
					MidiEvent.CurrentMidiTick        = 0;
					MidiEvent.TrackIndex             = 1;
					UE_LOG(LogStepSequencePlayer, Verbose, TEXT("0x%x Note-Off %d (during resize)"), (uint32)(size_t)this, MidiNote);
					MidiOutPin->AddMidiEvent(MidiEvent);
				}
			}
			CurrentCellNotes.SetNum(SequenceTable->Notes.Num());
		}
		else
		{
			while (CurrentCellNotes.Num() < SequenceTable->Notes.Num())
			{
				CurrentCellNotes.Add(FMidiVoiceId());
			}
		}
	}

	void FStepSequencePlayerOperator::AllNotesOff(int32 AtFrameIndex, int32 AbsMidiTick, bool ResetCellIndex)
	{
		for (int i = 0; i < CurrentCellNotes.Num(); ++i)
		{
			if (CurrentCellNotes[i])
			{
				uint8 MidiCh;
				uint8 MidiNote;
				CurrentCellNotes[i].GetChannelAndNote(MidiCh, MidiNote);
				FMidiStreamEvent MidiEvent(CurrentCellNotes[i].GetGeneratorId(), FMidiMsg::CreateNoteOff(MidiCh, MidiNote));
				MidiEvent.BlockSampleFrameIndex  = AtFrameIndex;
				MidiEvent.AuthoredMidiTick       = AbsMidiTick;
				MidiEvent.CurrentMidiTick        = AbsMidiTick;
				MidiEvent.TrackIndex             = 1;
				MidiOutPin->AddMidiEvent(MidiEvent);
				UE_LOG(LogStepSequencePlayer, Verbose, TEXT("0x%x Note-Off %d (during all notes off)"), (uint32)(size_t)this, MidiNote);
				CurrentCellNotes[i] = FMidiVoiceId::None();
			}
		}
		if (ResetCellIndex)
		{
			CurrentCellIndex = -1;
		}
	}
}

#undef LOCTEXT_NAMESPACE // "HarmonixMetaSound"
