// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassCommandBuffer.h"
#include "Containers/AnsiString.h"
#include "MassObserverManager.h"
#include "MassEntityUtils.h"
#include "HAL/IConsoleManager.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "VisualLogger/VisualLogger.h"


CSV_DEFINE_CATEGORY(MassEntities, true);
CSV_DEFINE_CATEGORY(MassEntitiesCounters, true);
DECLARE_CYCLE_STAT(TEXT("Mass Flush Commands"), STAT_Mass_FlushCommands, STATGROUP_Mass);

namespace UE::Mass::Command {

#if CSV_PROFILER
bool bEnableDetailedStats = false;

FAutoConsoleVariableRef CVarEnableDetailedCommandStats(TEXT("massentities.EnableCommandDetailedStats"), bEnableDetailedStats,
	TEXT("Set to true create a dedicated stat per type of command."), ECVF_Default);

/** CSV stat names */
static FString DefaultBatchedName = TEXT("BatchedCommand");
static TMap<FName, TPair<FString, FAnsiString>> CommandBatchedFNames;

/** CSV custom stat names (ANSI) */
static FAnsiString DefaultANSIBatchedName = "BatchedCommand";

/**
 * Provides valid names for CSV profiling.
 * @param Command is the command instance
 * @param OutName is the name to use for csv custom stats
 * @param OutANSIName is the name to use for csv stats
 */
void GetCommandStatNames(FMassBatchedCommand& Command, FString*& OutName, FAnsiString*& OutANSIName)
{
	OutANSIName = &DefaultANSIBatchedName;
	OutName     = &DefaultBatchedName;
	if (!bEnableDetailedStats)
	{
		return;
	}

	const FName CommandFName = Command.GetFName();

	TPair<FString, FAnsiString>& Names = CommandBatchedFNames.FindOrAdd(CommandFName);
	OutName     = &Names.Get<FString>();
	OutANSIName = &Names.Get<FAnsiString>();
	if (OutName->IsEmpty())
	{
		*OutName     = CommandFName.ToString();
		*OutANSIName = **OutName;
	}
}

#endif
} // UE::Mass::Command

//////////////////////////////////////////////////////////////////////
// FMassBatchedCommand
std::atomic<uint32> FMassBatchedCommand::CommandsCounter;

//////////////////////////////////////////////////////////////////////
// FMassCommandBuffer

FMassCommandBuffer::~FMassCommandBuffer()
{
	ensureMsgf(HasPendingCommands() == false, TEXT("Destroying FMassCommandBuffer while there are still unprocessed commands. These operations will never be performed now."));

	for (FMassBatchedCommand*& Command : CommandInstances)
	{
		delete Command;
	}
}

void FMassCommandBuffer::Flush(FMassEntityManager& EntityManager)
{
	check(!bIsFlushing);
	TGuardValue FlushingGuard(bIsFlushing, true);

	// short-circuit exit
	if (HasPendingCommands() == false)
	{
		return;
	}

	{
		UE_MT_SCOPED_WRITE_ACCESS(PendingBatchCommandsDetector);
		LLM_SCOPE_BYNAME(TEXT("Mass/FlushCommands"));
		SCOPE_CYCLE_COUNTER(STAT_Mass_FlushCommands);

		// array used to group commands depending on their operations. Based on EMassCommandOperationType
		constexpr int32 CommandTypeOrder[] =
		{
			MAX_int32 - 1, // None
			0, // Create
			3, // Add
			1, // Remove
			2, // ChangeComposition
			4, // Set
			5, // Destroy
		};
		static_assert(UE_ARRAY_COUNT(CommandTypeOrder) == (int)EMassCommandOperationType::MAX, "CommandTypeOrder needs to correspond to all EMassCommandOperationType\'s entries");

		struct FBatchedCommandsSortedIndex
		{
			FBatchedCommandsSortedIndex(const int32 InIndex, const int32 InGroupOrder)
				: Index(InIndex), GroupOrder(InGroupOrder)
			{}

			const int32 Index = -1;
			const int32 GroupOrder = MAX_int32;
			bool IsValid() const { return GroupOrder < MAX_int32; }
			bool operator<(const FBatchedCommandsSortedIndex& Other) const { return GroupOrder < Other.GroupOrder; }
		};
		TArray<FBatchedCommandsSortedIndex> CommandsOrder;
		CommandsOrder.Reserve(CommandInstances.Num());
		for (int32 i = 0; i < CommandInstances.Num(); ++i)
		{
			const FMassBatchedCommand* Command = CommandInstances[i];
			CommandsOrder.Add(FBatchedCommandsSortedIndex(i, (Command && Command->HasWork())? CommandTypeOrder[(int)Command->GetOperationType()] : MAX_int32));
		}
		CommandsOrder.StableSort();
				
		for (int32 k = 0; k < CommandsOrder.Num() && CommandsOrder[k].IsValid(); ++k)
		{
			FMassBatchedCommand* Command = CommandInstances[CommandsOrder[k].Index];
			check(Command)

#if CSV_PROFILER
			using namespace UE::Mass::Command;

			// Extract name (default or detailed)
			FAnsiString* ANSIName = nullptr;
			FString*     Name     = nullptr;
			GetCommandStatNames(*Command, Name, ANSIName);

			// Push stats
			FScopedCsvStat ScopedCsvStat(**ANSIName, CSV_CATEGORY_INDEX(MassEntities));
			FCsvProfiler::RecordCustomStat(**Name, CSV_CATEGORY_INDEX(MassEntitiesCounters), Command->GetNumOperationsStat(), ECsvCustomStatOp::Accumulate);
#endif // CSV_PROFILER

			Command->Execute(EntityManager);
			Command->Reset();
		}
	}
}
 
void FMassCommandBuffer::CleanUp()
{
	for (FMassBatchedCommand* Command : CommandInstances)
	{
		if (Command)
		{
			Command->Reset();
		}
	}
}

void FMassCommandBuffer::MoveAppend(FMassCommandBuffer& Other)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(MassCommandBuffer_MoveAppend);

	// @todo optimize, there surely a way to do faster then this.
	UE_MT_SCOPED_READ_ACCESS(Other.PendingBatchCommandsDetector);
	if (Other.HasPendingCommands())
	{
		FScopeLock Lock(&AppendingCommandsCS);
		UE_MT_SCOPED_WRITE_ACCESS(PendingBatchCommandsDetector);
		CommandInstances.Append(MoveTemp(Other.CommandInstances));
	}
}

SIZE_T FMassCommandBuffer::GetAllocatedSize() const
{
	SIZE_T TotalSize = 0;
	for (FMassBatchedCommand* Command : CommandInstances)
	{
		TotalSize += Command ? Command->GetAllocatedSize() : 0;
	}

	TotalSize += CommandInstances.GetAllocatedSize();
	
	return TotalSize;
}

