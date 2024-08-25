// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trace/ChaosVDTraceAnalyzer.h"

#include "ChaosVDModule.h"
#include "Trace/ChaosVDTraceProvider.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "TraceServices/Model/Frames.h"

void FChaosVDTraceAnalyzer::OnAnalysisBegin(const FOnAnalysisContext& Context)
{
	FInterfaceBuilder& Builder = Context.InterfaceBuilder;
	Builder.RouteEvent(RouteId_ChaosVDParticleDestroyed, "ChaosVDLogger", "ChaosVDParticleDestroyed");
	
	Builder.RouteEvent(RouteId_ChaosVDSolverFrameStart, "ChaosVDLogger", "ChaosVDSolverFrameStart");
	Builder.RouteEvent(RouteId_ChaosVDSolverFrameEnd, "ChaosVDLogger", "ChaosVDSolverFrameEnd");
	
	Builder.RouteEvent(RouteId_ChaosVDSolverStepStart, "ChaosVDLogger", "ChaosVDSolverStepStart");
	Builder.RouteEvent(RouteId_ChaosVDSolverStepEnd, "ChaosVDLogger", "ChaosVDSolverStepEnd");
	
	Builder.RouteEvent(RouteId_ChaosVDBinaryDataStart, "ChaosVDLogger", "ChaosVDBinaryDataStart");
	Builder.RouteEvent(RouteId_ChaosVDBinaryDataContent, "ChaosVDLogger", "ChaosVDBinaryDataContent");
	Builder.RouteEvent(RouteId_ChaosVDBinaryDataEnd, "ChaosVDLogger", "ChaosVDBinaryDataEnd");
	Builder.RouteEvent(RouteId_ChaosVDSolverSimulationSpace, "ChaosVDLogger", "ChaosVDSolverSimulationSpace");
	
	Builder.RouteEvent(RouteId_ChaosVDNonSolverLocation, "ChaosVDLogger", "ChaosVDNonSolverLocation");
	Builder.RouteEvent(RouteId_ChaosVDNonSolverTransform, "ChaosVDLogger", "ChaosVDNonSolverTransform");

	Builder.RouteEvent(RouteId_BeginFrame, "Misc", "BeginFrame");
	Builder.RouteEvent(RouteId_EndFrame, "Misc", "EndFrame");

	TraceServices::FAnalysisSessionEditScope _(Session);
	ChaosVDTraceProvider->CreateRecordingInstanceForSession(Session.GetName());
}

void FChaosVDTraceAnalyzer::OnAnalysisEnd()
{
	OnAnalysisComplete().Broadcast();
}

bool FChaosVDTraceAnalyzer::OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context)
{
	LLM_SCOPE_BYNAME(TEXT("Insights/FChaosVDTraceAnalyzer"));

	TraceServices::FAnalysisSessionEditScope _(Session);

	const FEventData& EventData = Context.EventData;
	
	switch (RouteId)
	{
	case RouteId_BeginFrame:
		{
			uint8 FrameType = EventData.GetValue<uint8>("FrameType");
			if (static_cast<ETraceFrameType>(FrameType) == TraceFrameType_Game)
			{
				TSharedPtr<FChaosVDGameFrameData> FrameData = MakeShared<FChaosVDGameFrameData>();
				FrameData->FirstCycle = EventData.GetValue<uint64>("Cycle");
				FrameData->StartTime = Context.EventTime.AsSeconds(FrameData->FirstCycle);
				ChaosVDTraceProvider->StartGameFrame(FrameData);
			}

			break;
		}

	case RouteId_EndFrame:
		{
			uint8 FrameType = EventData.GetValue<uint8>("FrameType");
			if (static_cast<ETraceFrameType>(FrameType) == TraceFrameType_Game)
			{
				if (TSharedPtr<FChaosVDGameFrameData> CurrentFrameData = ChaosVDTraceProvider->GetCurrentGameFrame().Pin())
				{
					CurrentFrameData->LastCycle = EventData.GetValue<uint64>("Cycle");
					CurrentFrameData->EndTime = Context.EventTime.AsSeconds(CurrentFrameData->LastCycle);	
				}
			}
			break;
		}
	case RouteId_ChaosVDSolverFrameStart:
		{
			FChaosVDSolverFrameData NewFrameData;

			NewFrameData.SolverID = EventData.GetValue<int32>("SolverID");
			NewFrameData.FrameCycle = EventData.GetValue<uint64>("Cycle");
			NewFrameData.bIsKeyFrame = EventData.GetValue<bool>("IsKeyFrame");
			NewFrameData.bIsResimulated = EventData.GetValue<bool>("IsReSimulated");
			NewFrameData.StartTime = Context.EventTime.AsSeconds(NewFrameData.FrameCycle);

			FWideStringView DebugNameView;
			EventData.GetString("DebugName", DebugNameView);
			NewFrameData.DebugName = DebugNameView;

			// Currently not all solvers have an end frame event, so lets just set the end frame time of the previous frame, with the start of this new one.
			{
				if (FChaosVDSolverFrameData* PrevFrameData = ChaosVDTraceProvider->GetCurrentSolverFrame(NewFrameData.SolverID))
				{
					PrevFrameData->EndTime = NewFrameData.StartTime;
				}
			}

			// Add an empty frame. It will be filled out by the solver trace events
			ChaosVDTraceProvider->StartSolverFrame(NewFrameData.SolverID, MoveTemp(NewFrameData));
			break;
		}
	case RouteId_ChaosVDSolverFrameEnd:
		{
			break;
		}
	case RouteId_ChaosVDSolverStepStart:
		{
			const int32 SolverID = EventData.GetValue<int32>("SolverID");

			// This can be null if the recording started Mid-Frame. In this case we just discard the data for now
			if (FChaosVDSolverFrameData* FrameData = ChaosVDTraceProvider->GetCurrentSolverFrame(SolverID))
			{
				// Add an empty step. It will be filled out by the particle (and later on other objects/elements) events
				FChaosVDStepData& StepData = FrameData->SolverSteps.AddDefaulted_GetRef();

				FWideStringView DebugNameView;
				EventData.GetString("StepName", DebugNameView);
				StepData.StepName = DebugNameView;
			}
	
			break;
		}
	case RouteId_ChaosVDSolverStepEnd:
		{
			break;
		}
	case RouteId_ChaosVDParticleDestroyed:
		{
			const int32 SolverID = EventData.GetValue<int32>("SolverID");

			if (FChaosVDSolverFrameData* FrameData = ChaosVDTraceProvider->GetCurrentSolverFrame(SolverID))
			{
				int32 ParticleDestroyedID = EventData.GetValue<int32>("ParticleID");

				// We need to add all particles that were destroyed in any step of this frame to the Frame data structure
				// So we can properly process these events when not all the steps are played back
				// Either because of the lock sub-step feature or because we are manually scrubbing from frame to frame
				FrameData->ParticlesDestroyedIDs.Add(ParticleDestroyedID);
				
				if (FrameData->SolverSteps.Num() > 0)
				{
					FrameData->SolverSteps.Last().ParticlesDestroyedIDs.Add(ParticleDestroyedID);
				}
			}

			break;
		}
	case RouteId_ChaosVDBinaryDataStart:
		{
			const int32 DataID = EventData.GetValue<int32>("DataID");
			
			FChaosVDBinaryDataContainer& DataContainer = ChaosVDTraceProvider->FindOrAddUnprocessedData(DataID);
			DataContainer.bIsCompressed = EventData.GetValue<bool>("IsCompressed");
			DataContainer.UncompressedSize = EventData.GetValue<uint32>("OriginalSize");
			DataContainer.DataID = EventData.GetValue<int32>("DataID");

			EventData.GetString("TypeName", DataContainer.TypeName);

			const uint32 DataSize = EventData.GetValue<uint32>("DataSize");
			DataContainer.RawData.Reserve(DataSize);

			break;
		}
	case RouteId_ChaosVDBinaryDataContent:
		{
			const int32 DataID = EventData.GetValue<int32>("DataID");	

			FChaosVDBinaryDataContainer& DataContainer = ChaosVDTraceProvider->FindOrAddUnprocessedData(DataID);

			const TArrayView<const uint8> SerializedDataChunk = EventData.GetArrayView<uint8>("RawData");
			DataContainer.RawData.Append(SerializedDataChunk.GetData(), SerializedDataChunk.Num());

			break;
		}
	case RouteId_ChaosVDBinaryDataEnd:
		{
			const int32 DataID = EventData.GetValue<int32>("DataID");

			if (!ChaosVDTraceProvider->ProcessBinaryData(DataID))
			{
				// This can happen during live debugging as we miss some of the events at the beginning.
				// Loading a trace file that was recorded as part of a live session, will have the same issue.
				UE_LOG(LogChaosVDEditor, Verbose, TEXT("[%s] FailedToProcess Binary Data with ID [%d]"), ANSI_TO_TCHAR(__FUNCTION__), DataID);
			}

			break;
		}
	case RouteId_ChaosVDSolverSimulationSpace:
		{
			const int32 SolverID = EventData.GetValue<int32>("SolverID");

			FVector Position;
			CVD_READ_TRACE_VECTOR(Position, Position, float, EventData);

			FQuat Rotation;
			CVD_READ_TRACE_QUAT(Rotation, Rotation, float, EventData);

			// This can be null if the recording started Mid-Frame. In this case we just discard the data for now
			if (FChaosVDSolverFrameData* FrameData = ChaosVDTraceProvider->GetCurrentSolverFrame(SolverID))
			{
				FrameData->SimulationTransform.SetLocation(Position);
				FrameData->SimulationTransform.SetRotation(Rotation);
			}
			break;
		}
	case RouteId_ChaosVDNonSolverLocation:
		{
			FChaosVDTrackedLocation TrackedLocation;
			CVD_READ_TRACE_VECTOR(TrackedLocation.Location, Position, float, EventData);

			EventData.GetString("DebugName", TrackedLocation.DebugName);
			
			if (TSharedPtr<FChaosVDGameFrameData> CurrentFrameData = ChaosVDTraceProvider->GetCurrentGameFrame().Pin())
			{
				CurrentFrameData->RecordedNonSolverLocationsByID.Add(FName(TrackedLocation.DebugName), MoveTemp(TrackedLocation));
			}

			break;
		}
	case RouteId_ChaosVDNonSolverTransform:
		{
			FChaosVDTrackedTransform TrackedTransform;
			CVD_READ_TRACE_TRANSFORM(TrackedTransform.Transform, float, EventData);

			EventData.GetString("DebugName", TrackedTransform.DebugName);
			if (TSharedPtr<FChaosVDGameFrameData> CurrentFrameData = ChaosVDTraceProvider->GetCurrentGameFrame().Pin())
			{
				CurrentFrameData->RecordedNonSolverTransformsByID.Add(FName(TrackedTransform.DebugName), MoveTemp(TrackedTransform));
			}

			break;
		}

	default:
		break;
	}

	return true;
}
