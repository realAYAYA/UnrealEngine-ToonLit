// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassCommandBuffer.h"
#include "MassObserverManager.h"
#include "MassEntityUtils.h"
#include "HAL/IConsoleManager.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "VisualLogger/VisualLogger.h"


CSV_DEFINE_CATEGORY(MassEntities, true);
CSV_DEFINE_CATEGORY(MassEntitiesCounters, true);

namespace UE::Mass::Command {

#if CSV_PROFILER
bool bEnableDetailedStats = false;

FAutoConsoleVariableRef CVarEnableDetailedCommandStats(TEXT("massentities.EnableCommandDetailedStats"), bEnableDetailedStats,
	TEXT("Set to true create a dedicated stat per type of command."), ECVF_Default);

/** CSV stat names */
static FString DefaultBatchedName = TEXT("BatchedCommand");
static TArray<FName> CommandBatchedFNames;

/** CSV custom stat names (ANSI) */
static const int32 MaxNameLength = 64;
typedef ANSICHAR ANSIName[MaxNameLength];
static ANSIName DefaultANSIBatchedName = "BatchedCommand";
static TArray<ANSIName> CommandANSIBatchedNames;

/**
 * Provides valid names for CSV profiling.
 * @param Command is the command instance
 * @param OutName is the name to use for csv custom stats
 * @param OutANSIName is the name to use for csv stats
 */
void GetCommandStatNames(FMassBatchedCommand& Command, FString& OutName, ANSIName*& OutANSIName)
{
	OutANSIName = &DefaultANSIBatchedName;
	OutName = DefaultANSIBatchedName;
	if (!bEnableDetailedStats)
	{
		return;
	}

	const FName CommandFName = Command.GetFName();
	OutName = CommandFName.ToString();

	const int32 Index = CommandBatchedFNames.Find(CommandFName);
	if (Index == INDEX_NONE)
	{
		CommandBatchedFNames.Emplace(CommandFName);
		OutANSIName = &CommandANSIBatchedNames.AddZeroed_GetRef();
		// Use prefix for easier parsing in reports
		//const FString CounterName = FString::Printf(TEXT("Num%s"), *OutName);
		//FMemory::Memcpy(OutANSIName, StringCast<ANSICHAR>(*CounterName).Get(), FMath::Min(CounterName.Len(), MaxNameLength - 1) * sizeof(ANSICHAR));
		FMemory::Memcpy(OutANSIName, StringCast<ANSICHAR>(*OutName).Get(), FMath::Min(OutName.Len(), MaxNameLength - 1) * sizeof(ANSICHAR));
	}
	else
	{
		OutANSIName = &CommandANSIBatchedNames[Index];
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

		// array used to group commands depending on their operations. Based on EMassCommandOperationType
		// @todo I'm opened to suggestions on how to better implement this
		constexpr int32 CommandTypeOrder[] =
		{
			MAX_int32 - 1, // None
			0, // Create
			3, // Add
			1, // Remove
			2, // ChangeComposition
			3, // Set
			4, // Destroy
		};
		static_assert((sizeof(CommandTypeOrder) / sizeof(CommandTypeOrder[0])) == (int)EMassCommandOperationType::MAX, "CommandTypeOrder needs to correspond to all EMassCommandOperationType\'s entries");

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
		CommandsOrder.Sort();
				
		for (int32 k = 0; k < CommandsOrder.Num() && CommandsOrder[k].IsValid(); ++k)
		{
			FMassBatchedCommand* Command = CommandInstances[CommandsOrder[k].Index];
			check(Command)

#if CSV_PROFILER
			using namespace UE::Mass::Command;

			// Extract name (default or detailed)
			ANSIName* ANSIName = &DefaultANSIBatchedName;
			FString Name = DefaultBatchedName;
			GetCommandStatNames(*Command, Name, ANSIName);

			// Push stats
			FScopedCsvStat ScopedCsvStat(*ANSIName, CSV_CATEGORY_INDEX(MassEntities));
			FCsvProfiler::RecordCustomStat(*Name, CSV_CATEGORY_INDEX(MassEntitiesCounters), Command->GetNumOperationsStat(), ECsvCustomStatOp::Accumulate);
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

