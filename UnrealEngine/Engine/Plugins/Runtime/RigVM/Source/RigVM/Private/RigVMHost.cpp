// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMHost.h"
#include "Engine/UserDefinedEnum.h"
#include "ObjectTrace.h"
#include "RigVMCore/RigVMNativized.h"
#include "RigVMObjectVersion.h"
#include "RigVMTypeUtils.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "UObject/UObjectIterator.h"
#include "UObject/ObjectSaveContext.h"
#include "SceneView.h"

#if WITH_EDITOR
#include "Kismet2/BlueprintEditorUtils.h"
#endif// WITH_EDITOR

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMHost)

#define LOCTEXT_NAMESPACE "RigVMHost"

// CVar to disable all rigvm execution 
static TAutoConsoleVariable<int32> CVarRigVMDisableExecutionAll(TEXT("RigVM.DisableExecutionAll"), 0, TEXT("if nonzero we disable all execution of rigvms."));

// CVar to disable swapping to nativized vms 
static TAutoConsoleVariable<int32> CVarRigVMDisableNativizedVMs(TEXT("RigVM.DisableNativizedVMs"), 1, TEXT("if nonzero we disable swapping to nativized VMs."));

URigVMHost::URigVMHost(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, DeltaTime(0.0f)
	, AbsoluteTime(0.0f)
	, FramesPerSecond(0.0f)
	, bAccumulateTime(true)
#if WITH_EDITOR
	, RigVMLog(nullptr)
	, bEnableLogging(true)
#endif
	, EventQueue()
	, EventQueueToRun()
	, EventsToRunOnce()
	, bRequiresInitExecution(false)
	, InitBracket(0)
	, ExecuteBracket(0)
#if WITH_EDITORONLY_DATA
	, VMSnapshotBeforeExecution(nullptr)
#endif
#if UE_RIGVM_PROFILE_EXECUTE_UNITS_NUM
	, ProfilingRunsLeft(0)
	, AccumulatedCycles(0)
#endif
{
}
 
TArray<URigVMHost*> URigVMHost::FindRigVMHosts(UObject* Outer, TSubclassOf<URigVMHost> OptionalClass)
{
	TArray<URigVMHost*> Result;
	
	if(Outer == nullptr)
	{
		return Result; 
	}
	
	const AActor* OuterActor = Cast<AActor>(Outer);
	if(OuterActor == nullptr)
	{
		OuterActor = Outer->GetTypedOuter<AActor>();
	}
	
	for (TObjectIterator<URigVMHost> Itr; Itr; ++Itr)
	{
		URigVMHost* RigInstance = *Itr;
		if (!RigInstance)
		{
			continue;
		}
		
		const UClass* RigInstanceClass = RigInstance->GetClass();
		if (OptionalClass == nullptr || (RigInstanceClass && RigInstanceClass->IsChildOf(OptionalClass)))
		{
			if(RigInstance->IsInOuter(Outer))
			{
				Result.Add(RigInstance);
				continue;
			}

			if(OuterActor)
			{
				if(RigInstance->IsInOuter(OuterActor))
				{
					Result.Add(RigInstance);
					continue;
				}
			}
		}
	}

	return Result;
}

bool URigVMHost::IsGarbageOrDestroyed(const UObject* InObject)
{
	if(!IsValid(InObject))
	{
		return true;
	}
	return InObject->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) ||
		InObject->HasAnyInternalFlags(EInternalObjectFlags::Garbage);
}

UWorld* URigVMHost::GetWorld() const
{
	if (const UObject* Outer = GetOuter())
	{
		return Outer->GetWorld();
	}
	return nullptr;
}

void URigVMHost::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FRigVMObjectVersion::GUID);

	// advertise dependencies on user defined structs and user defined enums
	// to make sure they are loaded prior to the VM.
	if (Ar.IsObjectReferenceCollector() && VM != nullptr)
	{
		const TArray<const UObject*> UserDefinedDependencies = GetUserDefinedDependencies({ GetDefaultMemoryByType(ERigVMMemoryType::Literal), GetDefaultMemoryByType(ERigVMMemoryType::Work) });
		for (const UObject* UserDefinedDependency : UserDefinedDependencies)
		{
			if (Cast<UUserDefinedStruct>(UserDefinedDependency) ||
				Cast<UUserDefinedEnum>(UserDefinedDependency))
			{
				FSoftObjectPath PathToTypeObject(UserDefinedDependency);
				PathToTypeObject.Serialize(Ar);
			}
		}
	}

	if (Ar.IsLoading())
	{
		RecreateCachedMemory();
	}
}

void URigVMHost::PostLoad()
{
	Super::PostLoad();
	
	FRigVMRegistry::Get().RefreshEngineTypesIfRequired();
	
	FRigVMExtendedExecuteContext& ExtendedExecuteContext = GetRigVMExtendedExecuteContext();

	ExtendedExecuteContext.InvalidateCachedMemory();

	// In packaged builds, initialize the CDO VM
	// In editor, the VM will be recompiled and initialized at URigVMBlueprint::HandlePackageDone::RecompileVM
#if WITH_EDITOR
	if(GetPackage()->bIsCookedForEditor)
#endif
	{
		if (VM != nullptr)
		{
			if (HasAnyFlags(RF_ClassDefaultObject))
			{
				VM->ConditionalPostLoad();
				InitializeCDOVM();
			}

			if (!ensure(VM->ValidateBytecode()))
			{
				UE_LOG(LogRigVM, Warning, TEXT("%s: Invalid bytecode detected. VM will be reset."), *GetPathName());
				VM->Reset(ExtendedExecuteContext);
			}
		}
	}

#if WITH_EDITORONLY_DATA
	if (VMSnapshotBeforeExecution)
	{
		// Some VMSnapshots might have been created without the Transient flag.
		// Assets from that version require the snapshot to be flagged as below.
		VMSnapshotBeforeExecution->SetFlags(VMSnapshotBeforeExecution->GetFlags() | RF_Transient);
	}
#endif

}

void URigVMHost::PreSave(FObjectPreSaveContext SaveContext)
{
	Super::PreSave(SaveContext);

	GenerateUserDefinedDependenciesData(GetRigVMExtendedExecuteContext());
}

void URigVMHost::BeginDestroy()
{
	Super::BeginDestroy();

	InitializedEvent.Clear();
	ExecutedEvent.Clear();
#if WITH_EDITOR
	DebugInfo.Reset();
#endif

	if (RigVMExtendedExecuteContext != nullptr)
	{
		RigVMExtendedExecuteContext->ExecutionReachedExit().RemoveAll(this);
	}

#if WITH_EDITORONLY_DATA
	if (VMSnapshotBeforeExecution)
	{
		VMSnapshotBeforeExecution = nullptr;
	}
#endif

	TRACE_OBJECT_LIFETIME_END(this);
}

void URigVMHost::SetDeltaTime(float InDeltaTime)
{
	DeltaTime = InDeltaTime;
}

void URigVMHost::SetAbsoluteTime(float InAbsoluteTime, bool InSetDeltaTimeZero)
{
	if(InSetDeltaTimeZero)
	{
		DeltaTime = 0.f;
	}
	AbsoluteTime = InAbsoluteTime;
	bAccumulateTime = false;
}

void URigVMHost::SetAbsoluteAndDeltaTime(float InAbsoluteTime, float InDeltaTime)
{
	AbsoluteTime = InAbsoluteTime;
	DeltaTime = InDeltaTime;
}

void URigVMHost::SetFramesPerSecond(float InFramesPerSecond)
{
	FramesPerSecond = InFramesPerSecond;	
}

float URigVMHost::GetCurrentFramesPerSecond() const
{
	if(FramesPerSecond > SMALL_NUMBER)
	{
		return FramesPerSecond;
	}
	if(DeltaTime > SMALL_NUMBER)
	{
		return 1.f / DeltaTime;
	}
	return 60.f;
}

bool URigVMHost::CanExecute() const
{
	return DisableExecution() == false;
}

void URigVMHost::Initialize(bool bRequestInit)
{
	TRACE_OBJECT_LIFETIME_BEGIN(this);

	if(IsInitializing())
	{
		UE_LOG(LogRigVM, Warning, TEXT("%s: Initialize is being called recursively."), *GetPathName());
		return;
	}

	if (IsTemplate())
	{
		// don't initialize template class 
		return;
	}

	InitializeFromCDO();
	InstantiateVMFromCDO();

	if (bRequestInit)
	{
		RequestInit();
	}
}

bool URigVMHost::InitializeVM(const FName& InEventName)
{
	FRigVMExtendedExecuteContext& ExtendedExecuteContext = GetRigVMExtendedExecuteContext();

	const TArray<FRigVMExternalVariable> ExternalVariables = GetExternalVariablesImpl(false);
	if (VM->GetExternalVariableDefs().Num() != ExternalVariables.Num())
	{
		return false;	// The rig did compile with errors
	}

	// update the VM's external variables
	VM->SetExternalVariablesInstanceData(ExtendedExecuteContext, ExternalVariables);

	const bool bResult = VM->InitializeInstance(ExtendedExecuteContext);
	if(bResult)
	{
		bRequiresInitExecution = false;
	}

	// reset the time and caches during init
	AbsoluteTime = DeltaTime = 0.f;
		
	ExtendedExecuteContext.GetPublicDataSafe<>().GetNameCache()->Reset();

	if (InitializedEvent.IsBound())
	{
		FRigVMBracketScope BracketScope(InitBracket);
		InitializedEvent.Broadcast(this, InEventName);
	}

	return bResult;
}

void URigVMHost::Evaluate_AnyThread()
{
	// we can have other systems trying to poke into running instances of Control Rigs
	// on the anim thread and query data, such as
	// URigVMHostSkeletalMeshComponent::RebuildDebugDrawSkeleton,
	// using a lock here to prevent them from having an inconsistent view of the rig at some
	// intermediate stage of evaluation, for example, during evaluate, we can have a call
	// to copy hierarchy, which empties the hierarchy for a short period of time
	// and we don't want other systems to see that.
	FScopeLock EvaluateLock(&GetEvaluateMutex());
	
	// The EventQueueToRun should only be modified in this function
	ensureMsgf(EventQueueToRun.IsEmpty(), TEXT("Detected a recursive call to the control rig evaluation function %s"), *GetPackage()->GetPathName());
	
	// create a copy since we need to change it here temporarily,
	// and UI / the rig may change the event queue while it is running
	TGuardValue<TArray<FName>> EventQueueToRunGuard(EventQueueToRun, EventQueue);

	AdaptEventQueueForEvaluate(EventQueueToRun);
	
	// insert the events queued to run once
	for(const TPair<FName,int32>& Pair : EventsToRunOnce)
	{
		if(!EventQueueToRun.Contains(Pair.Key))
		{
			EventQueueToRun.Insert(Pair.Key, Pair.Value);
		}
	}

#if WITH_EDITOR
	FName FirstEvent = NAME_None;
	if (!EventQueueToRun.IsEmpty())
	{
		FirstEvent = EventQueueToRun[0];
	}
	FFirstEntryEventGuard FirstEntryEventGuard(&InstructionVisitInfo, FirstEvent);
#endif
	
	for (const FName& EventName : EventQueueToRun)
	{
		Execute(EventName);

#if WITH_EDITOR
		if (VM)
		{
			const FRigVMBreakpoint& Breakpoint = GetHaltedAtBreakpoint(); 
			if (Breakpoint.IsValid())
			{
				// make sure that the instruction index for the breakpoint is part
				// of the current entry.
				const FRigVMByteCode& ByteCode = VM->GetByteCode();
				const int32 CurrentEntryIndex = ByteCode.FindEntryIndex(EventName);
				if(CurrentEntryIndex != INDEX_NONE)
				{
					const FRigVMByteCodeEntry& CurrentEntry = ByteCode.GetEntry(CurrentEntryIndex);
					if(Breakpoint.InstructionIndex >= CurrentEntry.InstructionIndex)
					{
						int32 LastInstructionIndexInEntry;
						if(CurrentEntryIndex == ByteCode.NumEntries() - 1)
						{
							LastInstructionIndexInEntry = ByteCode.GetNumInstructions();
						}
						else
						{
							const int32 NextEntryIndex = CurrentEntryIndex + 1;
							LastInstructionIndexInEntry = ByteCode.GetEntry(NextEntryIndex).InstructionIndex - 1;
						}

						if(Breakpoint.InstructionIndex <= LastInstructionIndexInEntry)
						{
							break;
						}
					}
				}
			}
		}
#endif
	}
}

TArray<FRigVMExternalVariable> URigVMHost::GetExternalVariables() const
{
	return GetExternalVariablesImpl(true);
}

TArray<FRigVMExternalVariable> URigVMHost::GetPublicVariables() const
{
	return GetExternalVariables().FilterByPredicate([] (const FRigVMExternalVariable& Variable) -> bool
	{
		return Variable.bIsPublic;
	});
}

FRigVMExternalVariable URigVMHost::GetPublicVariableByName(const FName& InVariableName) const
{
	if (const FProperty* Property = GetPublicVariableProperty(InVariableName))
	{
		return FRigVMExternalVariable::Make(Property, (UObject*)this);
	}
	return FRigVMExternalVariable();
}

TArray<FName> URigVMHost::GetScriptAccessibleVariables() const
{
	TArray<FRigVMExternalVariable> PublicVariables = GetPublicVariables();
	TArray<FName> Names;
	for (const FRigVMExternalVariable& PublicVariable : PublicVariables)
	{
		Names.Add(PublicVariable.Name);
	}
	return Names;
}

FName URigVMHost::GetVariableType(const FName& InVariableName) const
{
	const FRigVMExternalVariable PublicVariable = GetPublicVariableByName(InVariableName);
	if (PublicVariable.IsValid(true /* allow nullptr */))
	{
		return PublicVariable.TypeName;
	}
	return NAME_None;
}

FString URigVMHost::GetVariableAsString(const FName& InVariableName) const
{
#if WITH_EDITOR
	if (const FProperty* Property = GetClass()->FindPropertyByName(InVariableName))
	{
		FString Result;
		const uint8* Container = (const uint8*)this;
		if (FBlueprintEditorUtils::PropertyValueToString(Property, Container, Result, nullptr))
		{
			return Result;
		}
	}
#endif
	return FString();
}

bool URigVMHost::SetVariableFromString(const FName& InVariableName, const FString& InValue)
{
#if WITH_EDITOR
	if (const FProperty* Property = GetClass()->FindPropertyByName(InVariableName))
	{
		uint8* Container = (uint8*)this;
		return FBlueprintEditorUtils::PropertyValueFromString(Property, InValue, Container, nullptr);
	}
#endif
	return false;
}

void URigVMHost::InvalidateCachedMemory()
{
	if (VM)
	{
		VM->InvalidateCachedMemory(GetRigVMExtendedExecuteContext());
	}
}

void URigVMHost::RecreateCachedMemory()
{
	if (VM)
	{
		RequestInit();
	}
}

bool URigVMHost::Execute(const FName& InEventName)
{
	if(!CanExecute())
	{
		return false;
	}

	bool bJustRanInit = false;
	if(bRequiresInitExecution)
	{
		const TGuardValue<float> AbsoluteTimeGuard(AbsoluteTime, AbsoluteTime);
		const TGuardValue<float> DeltaTimeGuard(DeltaTime, DeltaTime);
		if(!InitializeVM(InEventName))
		{
			return false;
		}
		bJustRanInit = true;
	}

	if(EventQueueToRun.IsEmpty())
	{
		EventQueueToRun = EventQueue;
	}

	URigVMHost* CDO = Cast<URigVMHost>(GetClass()->GetDefaultObject());
	
	const bool bIsEventInQueue = EventQueueToRun.Contains(InEventName);
	const bool bIsEventFirstInQueue = !EventQueueToRun.IsEmpty() && EventQueueToRun[0] == InEventName; 
	const bool bIsEventLastInQueue = !EventQueueToRun.IsEmpty() && EventQueueToRun.Last() == InEventName;

	ensure(!HasAnyFlags(RF_ClassDefaultObject));
	
	FRigVMExtendedExecuteContext& ExtendedExecuteContext = GetRigVMExtendedExecuteContext();

	FRigVMExecuteContext& PublicContext = ExtendedExecuteContext.GetPublicData<>();
	PublicContext.SetDeltaTime(DeltaTime);
	PublicContext.SetAbsoluteTime(AbsoluteTime);
	PublicContext.SetFramesPerSecond(GetCurrentFramesPerSecond());
	PublicContext.SetOwningComponent(GetOwningSceneComponent());
#if UE_RIGVM_DEBUG_EXECUTION
	PublicContext.bDebugExecution = bDebugExecutionEnabled;
#endif

#if WITH_EDITOR
	ExtendedExecuteContext.SetInstructionVisitInfo(&InstructionVisitInfo);

	if (IsInDebugMode())
	{
		if (CDO)
		{
			// Copy the breakpoints. This will not override the state of the breakpoints
			DebugInfo.SetBreakpoints(CDO->DebugInfo.GetBreakpoints());

			// If there are any breakpoints, create the Snapshot VM if it hasn't been created yet
			if (DebugInfo.GetBreakpoints().Num() > 0)
			{
				GetSnapshotVM();
			}
		}

		ExtendedExecuteContext.SetDebugInfo(&DebugInfo);
	}
	else
	{
		ExtendedExecuteContext.SetDebugInfo(nullptr);
	}
	
	if (IsProfilingEnabled())
	{
		ExtendedExecuteContext.SetProfilingInfo(&ProfilingInfo);
	}
	else
	{
		ExtendedExecuteContext.SetProfilingInfo(nullptr);
	}
#endif

	// setup the draw interface for debug drawing
	if(!bIsEventInQueue || bIsEventFirstInQueue)
	{
		DrawInterface.Reset();
	}
	PublicContext.SetDrawInterface(&DrawInterface);

	// draw container contains persistent draw instructions, 
	// so we cannot call Reset(), which will clear them,
	// instead, we re-initialize them from the CDO
	if (CDO && !HasAnyFlags(RF_ClassDefaultObject))
	{
		DrawContainer = CDO->DrawContainer;
	}
	PublicContext.SetDrawContainer(&DrawContainer);

	// guard against recursion
	if(IsExecuting())
	{
		UE_LOG(LogRigVM, Warning, TEXT("%s: Execute is being called recursively."), *GetPathName());
		return false;
	}

	const bool bSuccess = Execute_Internal(InEventName);

#if WITH_EDITOR

	// for the last event in the queue - clear the log message queue
	if (RigVMLog != nullptr && bEnableLogging)
	{
		if (bJustRanInit)
		{
			RigVMLog->KnownMessages.Reset();
			LoggedMessages.Reset();
		}
		else if(bIsEventLastInQueue)
		{
			for (const FRigVMLog::FLogEntry& Entry : RigVMLog->Entries)
			{
				if (Entry.FunctionName == NAME_None || Entry.InstructionIndex == INDEX_NONE || Entry.Message.IsEmpty())
				{
					continue;
				}

				FString PerInstructionMessage = 
					FString::Printf(
						TEXT("Instruction[%d] '%s': '%s'"),
						Entry.InstructionIndex,
						*Entry.FunctionName.ToString(),
						*Entry.Message
					);

				LogOnce(Entry.Severity, Entry.InstructionIndex, PerInstructionMessage);
			}
		}
	}
#endif

	if(!bIsEventInQueue || bIsEventLastInQueue) 
	{
		DeltaTime = 0.f;
	}

	if (ExecutedEvent.IsBound())
	{
		FRigVMBracketScope BracketScope(ExecuteBracket);
		ExecutedEvent.Broadcast(this, InEventName);
	}

	if (PublicContext.GetDrawInterface() && PublicContext.GetDrawContainer() && bIsEventLastInQueue) 
	{
		PublicContext.GetDrawInterface()->Instructions.Append(PublicContext.GetDrawContainer()->Instructions);
	}

	return bSuccess;
}

bool URigVMHost::DisableExecution()
{
	return CVarRigVMDisableExecutionAll->GetInt() == 1;
}

bool URigVMHost::InitializeCDOVM()
{
	check(VM != nullptr);
	check(VM->HasAnyFlags(RF_ClassDefaultObject | RF_DefaultSubObject));

	FRigVMExtendedExecuteContext& ExtendedExecuteContext = GetRigVMExtendedExecuteContext();

	// update the VM's external variables
	VM->ClearExternalVariables(ExtendedExecuteContext);
	VM->SetExternalVariableDefs(GetExternalVariablesImpl(false));
	return VM->Initialize(ExtendedExecuteContext);
}

bool URigVMHost::Execute_Internal(const FName& InEventName)
{
	if (VM == nullptr)
	{
		return false;
	}

	FRigVMExtendedExecuteContext& ExtendedExecuteContext = GetRigVMExtendedExecuteContext();
	
	static constexpr TCHAR InvalidatedVMFormat[] = TEXT("%s: Invalidated VM - aborting execution.");
	if(VM->IsNativized())
	{
		if(!IsValidLowLevel() ||
			!VM->IsValidLowLevel())
		{
			UE_LOG(LogRigVM, Warning, InvalidatedVMFormat, *GetClass()->GetName());
			return false;
		}
	}
	else
	{
		// sanity check the validity of the VM to ensure stability.
		if(!VM->IsContextValidForExecution(ExtendedExecuteContext)
			|| !IsValidLowLevel()
			|| !VM->IsValidLowLevel()
		)
		{
			UE_LOG(LogRigVM, Warning, InvalidatedVMFormat, *GetClass()->GetName());
			return false;
		}
	}

#if UE_RIGVM_PROFILE_EXECUTE_UNITS_NUM
	const uint64 StartCycles = FPlatformTime::Cycles64();
	if(ProfilingRunsLeft <= 0)
	{
		ProfilingRunsLeft = UE_RIGVM_PROFILE_EXECUTE_UNITS_NUM;
		AccumulatedCycles = 0;
	}
#endif
	
	const bool bUseDebuggingSnapshots = !VM->IsNativized();
	
#if WITH_EDITOR
	if(bUseDebuggingSnapshots)
	{
		if(URigVM* SnapShotVM = GetSnapshotVM(false)) // don't create it for normal runs
		{
			const bool bIsEventFirstInQueue = !EventQueueToRun.IsEmpty() && EventQueueToRun[0] == InEventName; 
			const bool bIsEventLastInQueue = !EventQueueToRun.IsEmpty() && EventQueueToRun.Last() == InEventName;

			if (GetHaltedAtBreakpoint().IsValid())
			{
				if(bIsEventFirstInQueue)
				{
					CopyVMMemory(GetRigVMExtendedExecuteContext(), GetSnapshotContext());
				}
			}
			else if(bIsEventLastInQueue)
			{
				CopyVMMemory(GetSnapshotContext(), GetRigVMExtendedExecuteContext());
			}
		}
	}
#endif

	const bool bSuccess = VM->ExecuteVM(ExtendedExecuteContext, InEventName) != ERigVMExecuteResult::Failed;

#if UE_RIGVM_PROFILE_EXECUTE_UNITS_NUM
	const uint64 EndCycles = FPlatformTime::Cycles64();
	const uint64 Cycles = EndCycles - StartCycles;
	AccumulatedCycles += Cycles;
	ProfilingRunsLeft--;
	if(ProfilingRunsLeft == 0)
	{
		const double Milliseconds = FPlatformTime::ToMilliseconds64(AccumulatedCycles);
		UE_LOG(LogRigVM, Display, TEXT("%s: %d runs took %.03lfms."), *GetClass()->GetName(), UE_RIGVM_PROFILE_EXECUTE_UNITS_NUM, Milliseconds);
	}
#endif

	return bSuccess;
}

bool URigVMHost::SupportsEvent(const FName& InEventName) const
{
	if (VM)
	{
		return VM->ContainsEntry(InEventName);
	}
	return false;
}

const TArray<FName>& URigVMHost::GetSupportedEvents() const
{
	if (VM)
	{
		return VM->GetEntryNames();
	}

	static const TArray<FName> EmptyEvents; 
	return EmptyEvents;
}

bool URigVMHost::ExecuteEvent(const FName& InEventName)
{
	if(SupportsEvent(InEventName))
	{
		TGuardValue<TArray<FName>> EventQueueGuard(EventQueue, {InEventName});
		Evaluate_AnyThread();
		return true;
	}
	return false;
}

void URigVMHost::RequestInit()
{
	bRequiresInitExecution = true;
}

void URigVMHost::RequestRunOnceEvent(const FName& InEventName, int32 InEventIndex)
{
	EventsToRunOnce.FindOrAdd(InEventName) = InEventIndex;
}

bool URigVMHost::RemoveRunOnceEvent(const FName& InEventName)
{
	return EventsToRunOnce.Remove(InEventName) > 0;
}

bool URigVMHost::IsRunOnceEvent(const FName& InEventName) const
{
	return EventsToRunOnce.Contains(InEventName);
}

void URigVMHost::SetEventQueue(const TArray<FName>& InEventNames)
{
	EventQueue = InEventNames;
}

void URigVMHost::UpdateVMSettings()
{
	if (VM)
	{
#if WITH_EDITOR
		// setup array handling and error reporting on the VM
		VMRuntimeSettings.SetLogFunction([this](const FRigVMLogSettings& InLogSettings, const FRigVMExecuteContext* InContext, const FString& Message)
			{
				check(InContext);

				if (RigVMLog)
				{
					RigVMLog->Report(InLogSettings, InContext->GetFunctionName(), InContext->GetInstructionIndex(), Message);
				}
				else
				{
					LogOnce(InLogSettings.Severity, InContext->GetInstructionIndex(), Message);
				}
			});
#endif

		GetRigVMExtendedExecuteContext().SetRuntimeSettings(VMRuntimeSettings);
	}
}

URigVM* URigVMHost::GetVM()
{
	if (VM == nullptr)
	{
		Initialize(true);
		check(VM);
	}
	return VM;
}

const FRigVMMemoryStorageStruct* URigVMHost::GetDefaultMemoryByType(ERigVMMemoryType InMemoryType) const
{
	check(VM);
	return VM->GetDefaultMemoryByType(InMemoryType);
}

FRigVMMemoryStorageStruct* URigVMHost::GetMemoryByType(ERigVMMemoryType InMemoryType)
{
	check(VM);
	return VM->GetMemoryByType(GetRigVMExtendedExecuteContext(), InMemoryType);
}

const FRigVMMemoryStorageStruct* URigVMHost::GetMemoryByType(ERigVMMemoryType InMemoryType) const
{
	check(VM);
	return VM->GetMemoryByType(GetRigVMExtendedExecuteContext(), InMemoryType);
}

void URigVMHost::DrawIntoPDI(FPrimitiveDrawInterface* PDI, const FTransform& InTransform)
{
	for (const FRigVMDrawInstruction& Instruction : DrawInterface)
	{
		if (!Instruction.IsValid())
		{
			continue;
		}

		FTransform InstructionTransform = Instruction.Transform * InTransform;
		switch (Instruction.PrimitiveType)
		{
			case ERigVMDrawSettings::Points:
			{
				for (const FVector& Point : Instruction.Positions)
				{
					PDI->DrawPoint(InstructionTransform.TransformPosition(Point), Instruction.Color, Instruction.Thickness, SDPG_Foreground);
				}
				break;
			}
			case ERigVMDrawSettings::Lines:
			{
				const TArray<FVector>& Points = Instruction.Positions;
				PDI->AddReserveLines(SDPG_Foreground, Points.Num() / 2, false, Instruction.Thickness > SMALL_NUMBER);
				for (int32 PointIndex = 0; PointIndex < Points.Num() - 1; PointIndex += 2)
				{
					PDI->DrawLine(InstructionTransform.TransformPosition(Points[PointIndex]), InstructionTransform.TransformPosition(Points[PointIndex + 1]), Instruction.Color, SDPG_Foreground, Instruction.Thickness);
				}
				break;
			}
			case ERigVMDrawSettings::LineStrip:
			{
				const TArray<FVector>& Points = Instruction.Positions;
				PDI->AddReserveLines(SDPG_Foreground, Points.Num() - 1, false, Instruction.Thickness > SMALL_NUMBER);
				for (int32 PointIndex = 0; PointIndex < Points.Num() - 1; PointIndex++)
				{
					PDI->DrawLine(InstructionTransform.TransformPosition(Points[PointIndex]), InstructionTransform.TransformPosition(Points[PointIndex + 1]), Instruction.Color, SDPG_Foreground, Instruction.Thickness);
				}
				break;
			}
			case ERigVMDrawSettings::DynamicMesh:
			{
				FDynamicMeshBuilder MeshBuilder(PDI->View->GetFeatureLevel());
				MeshBuilder.AddVertices(Instruction.MeshVerts);
				MeshBuilder.AddTriangles(Instruction.MeshIndices);
				MeshBuilder.Draw(PDI, InstructionTransform.ToMatrixWithScale(), Instruction.MaterialRenderProxy, SDPG_World/*SDPG_Foreground*/);
				break;
			}
			default:
			{
				break;
			}
		}
	}
}

USceneComponent* URigVMHost::GetOwningSceneComponent()
{
	return GetTypedOuter<USceneComponent>();
}

void URigVMHost::SwapVMToNativizedIfRequired(UClass* InNativizedClass)
{
	if (HasAnyFlags(RF_NeedPostLoad))
	{
		return;
	}
	if(VM == nullptr)
	{
		return;
	}

	const bool bNativizedVMDisabled = AreNativizedVMsDisabled();

	// GetNativizedClass can be pretty costly, let's try to skip this if it is not absolutely necessary
	if(InNativizedClass == nullptr && !bNativizedVMDisabled)
	{
		if(!HasAnyFlags(RF_ClassDefaultObject))
		{
			if(URigVMHost* CDO = GetClass()->GetDefaultObject<URigVMHost>())
			{
				if(CDO->VM)
				{
					if(CDO->VM->IsNativized())
					{
						InNativizedClass = CDO->VM->GetClass();
					}
					else
					{
						InNativizedClass = CDO->VM->GetNativizedClass(RigVMTypeUtils::GetExternalVariableDefs(GetExternalVariables()));
					}
				}
			}
		}
		else
		{
			InNativizedClass = VM->GetNativizedClass(RigVMTypeUtils::GetExternalVariableDefs(GetExternalVariables()));
		}
	}	

	if(VM->IsNativized())
	{
		if((InNativizedClass == nullptr) || bNativizedVMDisabled)
		{
			const EObjectFlags PreviousFlags = VM->GetFlags();
			VM->Rename(nullptr, GetTransientPackage(), REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
			VM->MarkAsGarbage();
			VM = NewObject<URigVM>(this, TEXT("RigVM_NVMA"), PreviousFlags);
#if UE_RIGVM_PROFILE_EXECUTE_UNITS_NUM
			ProfilingRunsLeft = 0;
			AccumulatedCycles = 0;
#endif
		}
	}
	else
	{
		if(InNativizedClass && !bNativizedVMDisabled)
		{
			const EObjectFlags PreviousFlags = VM->GetFlags();
			VM->Rename(nullptr, GetTransientPackage(), REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
			VM->MarkAsGarbage();
			VM = NewObject<URigVM>(this, InNativizedClass, TEXT("RigVM_NVMB"), PreviousFlags);
			GetRigVMExtendedExecuteContext().ExecutionReachedExit().AddUObject(this, &URigVMHost::HandleExecutionReachedExit);
#if UE_RIGVM_PROFILE_EXECUTE_UNITS_NUM
			ProfilingRunsLeft = 0;
			AccumulatedCycles = 0;
#endif
		}
	}
	
#if WITH_EDITOR

	// if we are a nativized VM,
	// let's set the bytecode for UI purposes.
	// this is only used for traversing node from execute stack to node and back etc. 
	// since the hash between nativized VM and current matches we assume the bytecode is identical as well.
	if(VM->IsNativized())
	{
		if(URigVMNativized* NativizedVM = Cast<URigVMNativized>(VM))
		{
			URigVMHost* CDO = GetClass()->GetDefaultObject<URigVMHost>();
			if (CDO && CDO->VM)
			{
				NativizedVM->SetByteCode(CDO->VM->GetByteCode());
			}
		}
	}
#endif
}

bool URigVMHost::AreNativizedVMsDisabled()
{
	return (CVarRigVMDisableNativizedVMs->GetInt() != 0);
}

#if WITH_EDITORONLY_DATA
void URigVMHost::DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass)
{
	Super::DeclareConstructClasses(OutConstructClasses, SpecificSubclass);
	OutConstructClasses.Add(FTopLevelAssetPath(URigVM::StaticClass()));
	OutConstructClasses.Add(FTopLevelAssetPath(URigVMMemoryStorage::StaticClass()));
}

#if UE_RIGVM_DEBUG_EXECUTION
const FString URigVMHost::GetDebugExecutionString()
{
	TGuardValue<bool> DebugExecutionGuard(bDebugExecutionEnabled, true);
	FRigVMExecuteContext& PublicContext = GetRigVMExtendedExecuteContext().GetPublicData<FRigVMExecuteContext>();
	PublicContext.DebugMemoryString.Reset();
	
	Evaluate_AnyThread();

	return PublicContext.DebugMemoryString;
}
#endif
#endif


UObject* URigVMHost::ResolveUserDefinedTypeById(const FString& InTypeName) const
{
	const FSoftObjectPath* ResultPathPtr = UserDefinedStructGuidToPathName.Find(InTypeName);
	if (ResultPathPtr == nullptr)
	{
		ResultPathPtr = UserDefinedEnumToPathName.Find(InTypeName);
	}

	if (ResultPathPtr == nullptr)
	{
		return nullptr;
	}

	if (UObject* TypeObject = ResultPathPtr->TryLoad())
	{
		// Ensure we have a hold on this type so it doesn't get nixed on the next GC.
		const_cast<URigVMHost*>(this)->UserDefinedTypesInUse.Add(TypeObject);
		return TypeObject;
	}

	return nullptr;
}

void URigVMHost::PostInitInstance(URigVMHost* InCDO)
{
	const EObjectFlags SubObjectFlags =
		HasAnyFlags(RF_ClassDefaultObject) ?
		RF_Public | RF_DefaultSubObject :
		RF_Transient | RF_Transactional;

	FRigVMExtendedExecuteContext& ExtendedExecuteContext = GetRigVMExtendedExecuteContext();

	ExtendedExecuteContext.SetContextPublicDataStruct(GetPublicContextStruct());

	ExtendedExecuteContext.ExecutionReachedExit().RemoveAll(this);
	ExtendedExecuteContext.ExecutionReachedExit().AddUObject(this, &URigVMHost::HandleExecutionReachedExit);

#if WITH_EDITOR
	ExtendedExecuteContext.GetPublicData<>().SetLog(RigVMLog); // may be nullptr
#endif

	UpdateVMSettings();

	if(!HasAnyFlags(RF_ClassDefaultObject))
	{
		if (ensure(InCDO))
		{
			ensure(VM == nullptr || VM == InCDO->GetVM());
			if (VM == nullptr)	// some Engine Tests does not have the VM
			{
				VM = InCDO->GetVM();
			}
		}
	}
	else // we are the CDO
	{
		// set up the VM
		if (VM == nullptr)
		{
			VM = NewObject<URigVM>(this, TEXT("RigVM_VM"), SubObjectFlags);
		}

		// for default objects we need to check if the CDO is rooted. specialized Control Rigs
		// such as the FK control rig may not have a root since they are part of a C++ package.

		// since the sub objects are created after the constructor
		// GC won't consider them part of the CDO, even if they have the sub object flags
		// so even if CDO is rooted and references these sub objects, 
		// it is not enough to keep them alive.
		// Hence, we have to add them to root here.
		if(GetClass()->IsNative())
		{
			VM->AddToRoot();
		}
	}

	RequestInit();
}

void URigVMHost::GenerateUserDefinedDependenciesData(FRigVMExtendedExecuteContext& Context)
{
	if (VM)
	{
		const TArray<const UObject*> UserDefinedDependencies = GetUserDefinedDependencies({ GetDefaultMemoryByType(ERigVMMemoryType::Literal), GetDefaultMemoryByType(ERigVMMemoryType::Work) });
		UserDefinedStructGuidToPathName.Reset();
		UserDefinedEnumToPathName.Reset();
		UserDefinedTypesInUse.Reset();

		for (const UObject* UserDefinedDependency : UserDefinedDependencies)
		{
			if (const UUserDefinedStruct* UserDefinedStruct = Cast<UUserDefinedStruct>(UserDefinedDependency))
			{
				const FString GuidBasedName = RigVMTypeUtils::GetUniqueStructTypeName(UserDefinedStruct);
				UserDefinedStructGuidToPathName.Add(GuidBasedName, UserDefinedStruct);
			}
			else if (const UUserDefinedEnum* UserDefinedEnum = Cast<UUserDefinedEnum>(UserDefinedDependency))
			{
				const FString EnumName = RigVMTypeUtils::CPPTypeFromEnum(UserDefinedEnum);
				UserDefinedEnumToPathName.Add(EnumName, UserDefinedEnum);
			}
		}
	}
}

TArray<const UObject*> URigVMHost::GetUserDefinedDependencies(const TArray<const FRigVMMemoryStorageStruct*> InMemory)
{
	TArray<const UObject*> Dependencies;
	auto ProcessMemory = [&Dependencies](const FRigVMMemoryStorageStruct* Memory)
	{
		if (Memory == nullptr)
		{
			return;
		}

		const TArray<const FProperty*>& Properties = Memory->GetProperties();

		for (const FProperty* Property : Properties)
		{
			const FProperty* PropertyToVisit = Property;
			while (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(PropertyToVisit))
			{
				PropertyToVisit = ArrayProperty->Inner;
			}
			if (const FStructProperty* StructProperty = CastField<FStructProperty>(PropertyToVisit))
			{
				if (const UUserDefinedStruct* UserDefinedStruct = Cast<UUserDefinedStruct>(StructProperty->Struct))
				{
					Dependencies.AddUnique(UserDefinedStruct);
				}
			}
			else if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(PropertyToVisit))
			{
				if (const UUserDefinedEnum* UserDefinedEnum = Cast<UUserDefinedEnum>(EnumProperty->GetEnum()))
				{
					Dependencies.AddUnique(UserDefinedEnum);
				}
			}
			else if (const FByteProperty* ByteProperty = CastField<FByteProperty>(PropertyToVisit))
			{
				if (const UUserDefinedEnum* UserDefinedEnum = Cast<UUserDefinedEnum>(ByteProperty->Enum))
				{
					Dependencies.AddUnique(UserDefinedEnum);
				}
			}
		}
	};

	for (const FRigVMMemoryStorageStruct* MemoryStorage : InMemory)
	{
		ProcessMemory(MemoryStorage);
	}

	const TArray<const FRigVMFunction*>& Functions = VM->GetFunctions();
	for (const FRigVMFunction* Function : Functions)
	{
		const FRigVMRegistry& Registry = FRigVMRegistry::Get();
		const TArray<TRigVMTypeIndex>& TypeIndices = Function->GetArgumentTypeIndices();
		for (const TRigVMTypeIndex& TypeIndex : TypeIndices)
		{
			const FRigVMTemplateArgumentType& Type = Registry.GetType(TypeIndex);
			if (Cast<UUserDefinedStruct>(Type.CPPTypeObject) ||
				Cast<UUserDefinedEnum>(Type.CPPTypeObject))
			{
				Dependencies.AddUnique(Type.CPPTypeObject);
			}
		}
	}

	return Dependencies;
}

void URigVMHost::HandleExecutionReachedExit(const FName& InEventName)
{
#if WITH_EDITOR
	if (EventQueueToRun.IsEmpty() || EventQueueToRun.Last() == InEventName)
	{
		if(URigVM* SnapShotVM = GetSnapshotVM(false))
		{
			CopyVMMemory(GetSnapshotContext(), GetRigVMExtendedExecuteContext());
		}
		DebugInfo.ResetState();
		SetBreakpointAction(ERigVMBreakpointAction::None);
	}
#endif
	
	if (bAccumulateTime)
	{
		AbsoluteTime += DeltaTime;
	}
}

TArray<FRigVMExternalVariable> URigVMHost::GetExternalVariablesImpl(bool bFallbackToBlueprint) const
{
	TArray<FRigVMExternalVariable> ExternalVariables;

	for (TFieldIterator<FProperty> PropertyIt(GetClass()); PropertyIt; ++PropertyIt)
	{
		FProperty* Property = *PropertyIt;
		if(Property->IsNative())
		{
			continue;
		}

		FRigVMExternalVariable ExternalVariable = FRigVMExternalVariable::Make(Property, (UObject*)this);
		if(!ExternalVariable.IsValid())
		{
			UE_LOG(LogRigVM, Warning, TEXT("%s: Property '%s' of type '%s' is not supported."), *GetClass()->GetName(), *Property->GetName(), *Property->GetCPPType());
			continue;
		}

		ExternalVariables.Add(ExternalVariable);
	}

#if WITH_EDITOR

	if (bFallbackToBlueprint)
	{
		// if we have a difference in the blueprint variables compared to us - let's 
		// use those instead. the assumption here is that the blueprint is dirty and
		// hasn't been compiled yet.
		if (UBlueprint* Blueprint = Cast<UBlueprint>(GetClass()->ClassGeneratedBy))
		{
			TArray<FRigVMExternalVariable> BlueprintVariables;
			for (const FBPVariableDescription& VariableDescription : Blueprint->NewVariables)
			{
				FRigVMExternalVariable ExternalVariable = RigVMTypeUtils::ExternalVariableFromBPVariableDescription(VariableDescription, (UObject*)this);
				if (ExternalVariable.TypeName.IsNone())
				{
					continue;
				}

				ExternalVariable.Memory = nullptr;
				BlueprintVariables.Add(ExternalVariable);
			}

			if (ExternalVariables.Num() != BlueprintVariables.Num())
			{
				return BlueprintVariables;
			}

			TMap<FName, int32> NameMap;
			for (int32 Index = 0; Index < ExternalVariables.Num(); Index++)
			{
				NameMap.Add(ExternalVariables[Index].Name, Index);
			}

			for (FRigVMExternalVariable BlueprintVariable : BlueprintVariables)
			{
				const int32* Index = NameMap.Find(BlueprintVariable.Name);
				if (Index == nullptr)
				{
					return BlueprintVariables;
				}

				FRigVMExternalVariable ExternalVariable = ExternalVariables[*Index];
				if (ExternalVariable.bIsArray != BlueprintVariable.bIsArray ||
					ExternalVariable.bIsPublic != BlueprintVariable.bIsPublic ||
					ExternalVariable.TypeName != BlueprintVariable.TypeName ||
					ExternalVariable.TypeObject != BlueprintVariable.TypeObject)
				{
					return BlueprintVariables;
				}
			}
		}
	}
#endif

	return ExternalVariables;
}

void URigVMHost::InstantiateVMFromCDO()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		SwapVMToNativizedIfRequired();

		FRigVMExtendedExecuteContext& ExtendedExecuteContext = GetRigVMExtendedExecuteContext();

		URigVMHost* CDO = GetClass()->GetDefaultObject<URigVMHost>();
		if (VM && CDO && CDO->VM)
		{
			if(!VM->IsNativized())
			{
				ExtendedExecuteContext.WorkMemoryStorage = CDO->VM->GetDefaultWorkMemory();
				ExtendedExecuteContext.DebugMemoryStorage = CDO->VM->GetDefaultDebugMemory();
				ExtendedExecuteContext.VMHash = CDO->VM->GetVMHash();

#if WITH_EDITOR
				// Fix AutoCompile while stopped in a breakpoint
				CopyVMMemory(GetSnapshotContext(), ExtendedExecuteContext);
#endif // WITH_EDITOR
			}
		}
		else if (VM)
		{
			VM->Reset(ExtendedExecuteContext);
			ExtendedExecuteContext.Reset();
		}
		else
		{
			ensure(false);
		}
	}

	RequestInit();
}

void URigVMHost::CopyExternalVariableDefaultValuesFromCDO()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		URigVMHost* CDO = GetClass()->GetDefaultObject<URigVMHost>();
		TArray<FRigVMExternalVariable> CurrentVariables = GetExternalVariablesImpl(false);
		TArray<FRigVMExternalVariable> CDOVariables = CDO->GetExternalVariablesImpl(false);
		if (ensure(CurrentVariables.Num() == CDOVariables.Num()))
		{
			for (int32 i=0; i<CurrentVariables.Num(); ++i)
			{
				FRigVMExternalVariable& Variable = CurrentVariables[i];
				FRigVMExternalVariable& CDOVariable = CDOVariables[i];
				Variable.Property->CopyCompleteValue(Variable.Memory, CDOVariable.Memory);
			}
		}
	}
}

void URigVMHost::InitializeFromCDO()
{
	// copy CDO property you need to here
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		// similar to FControlRigBlueprintCompilerContext::CopyTermDefaultsToDefaultObject,
		// where CDO is initialized from BP there,
		// we initialize all other instances of Control Rig from the CDO here
		URigVMHost* CDO = GetClass()->GetDefaultObject<URigVMHost>();

		ensure(VM == CDO->VM);

		PostInitInstanceIfRequired();

		// copy draw container
		DrawContainer = CDO->DrawContainer;

		// copy vm settings
		VMRuntimeSettings = CDO->VMRuntimeSettings;
	}
}

void URigVMHost::CopyVMMemory(FRigVMExtendedExecuteContext& TargetContext, const FRigVMExtendedExecuteContext& SourceContext)
{
	TargetContext.CopyMemoryStorage(SourceContext);
}

void URigVMHost::AddAssetUserData(UAssetUserData* InUserData)
{
	if (InUserData != NULL)
	{
		RemoveUserDataOfClass(InUserData->GetClass());
		AssetUserData.Add(InUserData);
	}
}

UAssetUserData* URigVMHost::GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	const TArray<UAssetUserData*>* ArrayPtr = GetAssetUserDataArray();
	for (int32 DataIdx = 0; DataIdx < ArrayPtr->Num(); DataIdx++)
	{
		UAssetUserData* Datum = (*ArrayPtr)[DataIdx];
		if (Datum != NULL && Datum->IsA(InUserDataClass))
		{
			return Datum;
		}
	}
	return NULL;
}

void URigVMHost::RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != NULL && Datum->IsA(InUserDataClass))
		{
			AssetUserData.RemoveAt(DataIdx);
			return;
		}
	}
#if WITH_EDITOR
	for (int32 DataIdx = 0; DataIdx < AssetUserDataEditorOnly.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserDataEditorOnly[DataIdx];
		if (Datum != NULL && Datum->IsA(InUserDataClass))
		{
			AssetUserDataEditorOnly.RemoveAt(DataIdx);
			return;
		}
	}
#endif
}

const TArray<UAssetUserData*>* URigVMHost::GetAssetUserDataArray() const
{
#if WITH_EDITOR
	if (IsRunningCookCommandlet())
	{
		return &ToRawPtrTArrayUnsafe(AssetUserData);
	}
	else
	{
		static thread_local TArray<TObjectPtr<UAssetUserData>> CachedAssetUserData;
		CachedAssetUserData.Reset();
		CachedAssetUserData.Append(AssetUserData);
		CachedAssetUserData.Append(AssetUserDataEditorOnly);
		return &ToRawPtrTArrayUnsafe(CachedAssetUserData);
	}
#else
	return &ToRawPtrTArrayUnsafe(AssetUserData);
#endif
}

#if WITH_EDITOR	

void URigVMHost::LogOnce(EMessageSeverity::Type InSeverity, int32 InInstructionIndex, const FString& InMessage)
{
	if(LoggedMessages.Contains(InMessage))
	{
		return;
	}

	switch (InSeverity)
	{
		case EMessageSeverity::Error:
		{
			UE_LOG(LogRigVM, Error, TEXT("%s"), *InMessage);
			break;
		}
		case EMessageSeverity::PerformanceWarning:
		case EMessageSeverity::Warning:
		{
			UE_LOG(LogRigVM, Warning, TEXT("%s"), *InMessage);
			break;
		}
		case EMessageSeverity::Info:
		{
			UE_LOG(LogRigVM, Display, TEXT("%s"), *InMessage);
			break;
		}
		default:
		{
			break;
		}
	}

	LoggedMessages.Add(InMessage, true);
}

void URigVMHost::AddBreakpoint(int32 InstructionIndex, UObject* InSubject, uint16 InDepth)
{
	DebugInfo.AddBreakpoint(InstructionIndex, InSubject, InDepth);
}

bool URigVMHost::ExecuteBreakpointAction(const ERigVMBreakpointAction BreakpointAction)
{
	if (GetHaltedAtBreakpoint().IsValid())
	{
		SetBreakpointAction(BreakpointAction);
		return true;
	}
	return false;
}

URigVM* URigVMHost::GetSnapshotVM(bool bCreateIfNeeded)
{
	if ((VMSnapshotBeforeExecution == nullptr) && bCreateIfNeeded)
	{
		VMSnapshotBeforeExecution = NewObject<URigVM>(GetTransientPackage(), NAME_None, RF_Transient);
	}
	return VMSnapshotBeforeExecution;
}

FRigVMExtendedExecuteContext& URigVMHost::GetSnapshotContext()
{
	return SnapshotContext;
}
#endif

#undef LOCTEXT_NAMESPACE
