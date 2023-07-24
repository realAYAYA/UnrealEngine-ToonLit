// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMHost.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "ObjectTrace.h"
#include "RigVMCore/RigVMNativized.h"
#include "RigVMTypeUtils.h"
#include "UObject/UObjectIterator.h"

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
		const UClass* RigInstanceClass = RigInstance ? RigInstance->GetClass() : nullptr;
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
	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);
}

void URigVMHost::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	if (VMSnapshotBeforeExecution)
	{
		// Some VMSnapshots might have been created without the Transient flag.
		// Assets from that version require the snapshot to be flagged as below.
		VMSnapshotBeforeExecution->SetFlags(VMSnapshotBeforeExecution->GetFlags() | RF_Transient);
	}
#endif
}

void URigVMHost::BeginDestroy()
{
	Super::BeginDestroy();
	InitializedEvent.Clear();
	ExecutedEvent.Clear();

	if (VM)
	{
		VM->ExecutionReachedExit().RemoveAll(this);
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
	return CVarRigVMDisableExecutionAll->GetInt() == 0;
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
	// update the VM's external variables
	VM->ClearExternalVariables();
	TArray<FRigVMExternalVariable> ExternalVariables = GetExternalVariablesImpl(false);
	for (FRigVMExternalVariable ExternalVariable : ExternalVariables)
	{
		VM->AddExternalVariable(ExternalVariable);
	}

	TArray<URigVMMemoryStorage*> LocalMemory = VM->GetLocalMemoryArray();
	const bool bResult = VM->Initialize(LocalMemory);
	if(bResult)
	{
		bRequiresInitExecution = false;
	}

	// reset the time and caches during init
	AbsoluteTime = DeltaTime = 0.f;
		
	if(VM)
	{
		VM->GetContext().GetPublicDataSafe<>().GetNameCache()->Reset();
	}

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
	
	// create a copy since we need to change it here temporarily,
	// and UI / the rig may change the event queue while it is running
	EventQueueToRun = EventQueue;

	AdaptEventQueueForEvaluate(EventQueueToRun);
	
	// insert the events queued to run once
	for(const TPair<FName,int32>& Pair : EventsToRunOnce)
	{
		if(!EventQueueToRun.Contains(Pair.Key))
		{
			EventQueueToRun.Insert(Pair.Key, Pair.Value);
		}
	}
	
	for (const FName& EventName : EventQueueToRun)
	{
		Execute(EventName);

#if WITH_EDITOR
		if (VM)
		{
			const FRigVMBreakpoint& Breakpoint = VM->GetHaltedAtBreakpoint(); 
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

	EventQueueToRun.Reset();
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
	
	FRigVMExtendedExecuteContext& ExtendedExecuteContext = GetVM()->GetContext();
	FRigVMExecuteContext& PublicContext = ExtendedExecuteContext.GetPublicData<>();
	PublicContext.SetDeltaTime(DeltaTime);
	PublicContext.SetAbsoluteTime(AbsoluteTime);
	PublicContext.SetFramesPerSecond(GetCurrentFramesPerSecond());
	PublicContext.SetOwningComponent(GetOwningSceneComponent());

	if (VM)
	{
		if (VM->GetOuter() != this)
		{
			InstantiateVMFromCDO();
		}

#if WITH_EDITOR
		// default to always clear data after each execution
		// only set a valid first entry event later when execution
		// has passed the initialization stage and there are multiple events present in one evaluation
		// first entry event is used to determined when to clear data during an evaluation
		VM->SetFirstEntryEventInEventQueue(NAME_None);
#endif
	}

#if WITH_EDITOR
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

		if(VM)
		{
			VM->SetDebugInfo(&DebugInfo);
		}
	}
	else if(VM)
	{
		VM->SetDebugInfo(nullptr);
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

bool URigVMHost::Execute_Internal(const FName& InEventName)
{
	if (VM == nullptr)
	{
		return false;
	}
	
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
		if(!IsValidLowLevel() ||
			!VM->IsValidLowLevel() ||
			!VM->GetLiteralMemory()->IsValidLowLevel() ||
			!VM->GetWorkMemory()->IsValidLowLevel())
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
	
	TArray<URigVMMemoryStorage*> LocalMemory = VM->GetLocalMemoryArray();

#if WITH_EDITOR
	if(bUseDebuggingSnapshots)
	{
		if(URigVM* SnapShotVM = GetSnapshotVM(false)) // don't create it for normal runs
		{
			const bool bIsEventFirstInQueue = !EventQueueToRun.IsEmpty() && EventQueueToRun[0] == InEventName; 
			const bool bIsEventLastInQueue = !EventQueueToRun.IsEmpty() && EventQueueToRun.Last() == InEventName;

			if (VM->GetHaltedAtBreakpoint().IsValid())
			{
				if(bIsEventFirstInQueue)
				{
					VM->CopyFrom(SnapShotVM, false, false, false, true, true);
				}
			}
			else if(bIsEventLastInQueue)
			{
				SnapShotVM->CopyFrom(VM, false, false, false, true, true);
			}
		}
	}
#endif

	const bool bSuccess = VM->Execute(LocalMemory, InEventName) != ERigVMExecuteResult::Failed;

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
	if(VM)
	{
#if WITH_EDITOR
		// setup array handling and error reporting on the VM
		VMRuntimeSettings.SetLogFunction([this](EMessageSeverity::Type InSeverity, const FRigVMExecuteContext* InContext, const FString& Message)
		{
			check(InContext);

			if(RigVMLog)
			{
				RigVMLog->Report(InSeverity, InContext->GetFunctionName(), InContext->GetInstructionIndex(), Message);
			}
			else
			{
				LogOnce(InSeverity, InContext->GetInstructionIndex(), Message);
			}
		});
#endif
		
		VM->SetRuntimeSettings(VMRuntimeSettings);
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
						InNativizedClass = CDO->VM->GetNativizedClass(GetExternalVariables());
					}
				}
			}
		}
		else
		{
			InNativizedClass = VM->GetNativizedClass(GetExternalVariablesImpl(true));
		}
	}	

	if(VM->IsNativized())
	{
		if((InNativizedClass == nullptr) || bNativizedVMDisabled)
		{
			const EObjectFlags PreviousFlags = VM->GetFlags();
			VM->Rename(nullptr, GetTransientPackage(), REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
			VM->MarkAsGarbage();
			VM = NewObject<URigVM>(this, TEXT("VM"), PreviousFlags);
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
			VM = NewObject<URigVM>(this, InNativizedClass, TEXT("VM"), PreviousFlags);
			VM->ExecutionReachedExit().AddUObject(this, &URigVMHost::HandleExecutionReachedExit);
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
}
#endif

void URigVMHost::PostInitInstance(URigVMHost* InCDO)
{
	const EObjectFlags SubObjectFlags =
		HasAnyFlags(RF_ClassDefaultObject) ?
			RF_Public | RF_DefaultSubObject :
			RF_Transient | RF_Transactional;

	// set up the VM
	VM = NewObject<URigVM>(this, TEXT("VM"), SubObjectFlags);
	VM->SetContextPublicDataStruct(GetPublicContextStruct());

	// Cooked platforms will load these pointers from disk.
	// In certain scenarios RequiresCookedData wil be false but the PKG_FilterEditorOnly will still be set (UEFN)
	if (!FPlatformProperties::RequiresCookedData() && !GetClass()->RootPackageHasAnyFlags(PKG_FilterEditorOnly))
	{
		VM->GetMemoryByType(ERigVMMemoryType::Work, true);
		VM->GetMemoryByType(ERigVMMemoryType::Literal, true);
		VM->GetMemoryByType(ERigVMMemoryType::Debug, true);
	}

	VM->ExecutionReachedExit().AddUObject(this, &URigVMHost::HandleExecutionReachedExit);

#if WITH_EDITOR
	GetVM()->GetContext().GetPublicData<>().SetLog(RigVMLog); // may be nullptr
#endif
	UpdateVMSettings();

	if(!HasAnyFlags(RF_ClassDefaultObject) && InCDO)
	{
		InCDO->PostInitInstanceIfRequired();
		VM->CopyFrom(InCDO->GetVM());
	}
	else // we are the CDO
	{
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

void URigVMHost::HandleExecutionReachedExit(const FName& InEventName)
{
#if WITH_EDITOR
	if (EventQueueToRun.Last() == InEventName)
	{
		if(URigVM* SnapShotVM = GetSnapshotVM(false))
		{
			SnapShotVM->CopyFrom(VM, false, false, false, true, true);
		}
		DebugInfo.ResetState();
		VM->SetBreakpointAction(ERigVMBreakpointAction::None);
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
				FRigVMExternalVariable ExternalVariable = RigVMTypeUtils::ExternalVariableFromBPVariableDescription(VariableDescription);
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

		URigVMHost* CDO = GetClass()->GetDefaultObject<URigVMHost>();
		if (VM && CDO && CDO->VM)
		{
			if(!VM->IsNativized())
			{
				// reference the literal memory + byte code
				// only defer if called from worker thread,
				// which should be unlikely
				VM->CopyFrom(CDO->VM, !IsInGameThread(), true);
			}
		}
		else if (VM)
		{
			VM->Reset();
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

		PostInitInstanceIfRequired();

		// copy draw container
		DrawContainer = CDO->DrawContainer;

		// copy vm settings
		VMRuntimeSettings = CDO->VMRuntimeSettings;
	}
}

void URigVMHost::AddAssetUserData(UAssetUserData* InUserData)
{
	if (InUserData != NULL)
	{
		UAssetUserData* ExistingData = GetAssetUserDataOfClass(InUserData->GetClass());
		if (ExistingData != NULL)
		{
			AssetUserData.Remove(ExistingData);
		}
		AssetUserData.Add(InUserData);
	}
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
}

UAssetUserData* URigVMHost::GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != NULL && Datum->IsA(InUserDataClass))
		{
			return Datum;
		}
	}
	return NULL;
}

const TArray<UAssetUserData*>* URigVMHost::GetAssetUserDataArray() const
{
	return &ToRawPtrTArrayUnsafe(AssetUserData);
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
	if (VM->GetHaltedAtBreakpoint().IsValid())
	{
		VM->SetBreakpointAction(BreakpointAction);
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

#endif

#undef LOCTEXT_NAMESPACE
