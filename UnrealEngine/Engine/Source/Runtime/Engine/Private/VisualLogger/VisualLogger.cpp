// Copyright Epic Games, Inc. All Rights Reserved.

#include "VisualLogger/VisualLogger.h"
#include "EngineStats.h"
#include "Misc/CommandLine.h"
#include "Modules/ModuleManager.h"
#include "AI/NavDataGenerator.h" // IWYU pragma: keep
#include "AI/NavigationSystemBase.h"
#include "Framework/Docking/TabManager.h"
#include "VisualLogger/VisualLoggerBinaryFileDevice.h"
#include "VisualLogger/VisualLoggerCustomVersion.h"
#include "VisualLogger/VisualLoggerTraceDevice.h"
#include "VisualLogger/VisualLoggerDebugSnapshotInterface.h"
#include "UnrealEngine.h"

#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#else
#include "Engine/Engine.h"
#include "Serialization/CustomVersion.h"
#endif

DEFINE_LOG_CATEGORY(LogVisual);
#if ENABLE_VISUAL_LOG

namespace UE::VisLog::Private
{
	/** Denotes the consecutive run number, giving us the ability to differentiate UObjectName_0 in Run 1 from UObjectName_0 in Run 2 */
	static int32 UniqueRunNumber = 0;

	/** Cached prefix string when using bForceUniqueLogNames (denotes UniqueRunNumber) */
	static FString UniqueLogPrefix;

#if WITH_EDITOR
	namespace EditorOnly
	{
		/** When in Editor, use this TimeStamp as a Base time so our values don't grow too large (and are canonically around 0.0). */
		static double EditorBaseTimeStamp = 0.0;
	}
#endif
}

DEFINE_STAT(STAT_VisualLog);

// Unfortunately needs to be a #define since it uses GET_TYPED_VARARGS_RESULT which uses the va_list stuff which operates on the
// current function, so we can't easily call a function
#define COLLAPSED_LOGF(SerializeFunc) \
	SCOPE_CYCLE_COUNTER(STAT_VisualLog); \
	UWorld *World = nullptr; \
	FVisualLogEntry *CurrentEntry = nullptr; \
	if (CheckVisualLogInputInternal(Object, CategoryName, Verbosity, &World, &CurrentEntry) == false) \
	{ \
		return; \
	} \
	int32	BufferSize	= 1024; \
	TCHAR*	Buffer		= nullptr; \
	int32	Result		= -1; \
	/* allocate some stack space to use on the first pass, which matches most strings */ \
	TCHAR	StackBuffer[512]; \
	TCHAR*	AllocatedBuffer = nullptr; \
	\
	/* first, try using the stack buffer */ \
	Buffer = StackBuffer; \
	GET_TYPED_VARARGS_RESULT( TCHAR, Buffer, UE_ARRAY_COUNT(StackBuffer), UE_ARRAY_COUNT(StackBuffer) - 1, Fmt, Fmt, Result ); \
	\
	/* if that fails, then use heap allocation to make enough space */ \
			while(Result == -1) \
						{ \
		FMemory::SystemFree(AllocatedBuffer); \
		/* We need to use malloc here directly as GMalloc might not be safe. */ \
		Buffer = AllocatedBuffer = (TCHAR*) FMemory::SystemMalloc( BufferSize * sizeof(TCHAR) ); \
		GET_TYPED_VARARGS_RESULT( TCHAR, Buffer, BufferSize, BufferSize-1, Fmt, Fmt, Result ); \
		BufferSize *= 2; \
						}; \
	Buffer[Result] = 0; \
	; \
	\
	SerializeFunc; \
	FMemory::SystemFree(AllocatedBuffer);

void FVisualLogger::CategorizedLogfImpl(const UObject* Object, const FName& CategoryName, ELogVerbosity::Type Verbosity, const TCHAR* Fmt, ...)
{
	COLLAPSED_LOGF(
		CurrentEntry->AddText(Buffer, CategoryName, Verbosity);
	);
}

void FVisualLogger::SegmentLogfImpl(const UObject* Object, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FVector& Start, const FVector& End, const FColor& Color, const uint16 Thickness, const TCHAR* Fmt, ...)
{
	COLLAPSED_LOGF(
		CurrentEntry->AddSegment(Start, End, CategoryName, Verbosity, Color, Buffer, Thickness);
	);
}

void FVisualLogger::ArrowLogfImpl(const UObject* Object, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FVector& Start, const FVector& End, const FColor& Color, const TCHAR* Fmt, ...)
{
	COLLAPSED_LOGF(
		CurrentEntry->AddArrow(Start, End, CategoryName, Verbosity, Color, Buffer);
	);
}

void FVisualLogger::CircleLogfImpl(const UObject* Object, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FVector& Center, const FVector& UpAxis, const float Radius, const FColor& Color, const uint16 Thickness, const TCHAR* Fmt, ...)
{
	COLLAPSED_LOGF(
		CurrentEntry->AddCircle(Center, UpAxis, Radius, CategoryName, Verbosity, Color, Buffer, Thickness);
	);
}

void FVisualLogger::LocationLogfImpl(const UObject* Object, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FVector& Location, uint16 Thickness, const FColor& Color, const TCHAR* Fmt, ...)
{
	COLLAPSED_LOGF(
		CurrentEntry->AddLocation(Location, CategoryName, Verbosity, Color, Buffer, Thickness);
	);
}

void FVisualLogger::SphereLogfImpl(const UObject* Object, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FVector& Center, float Radius, const FColor& Color, bool bWireframe, const TCHAR* Fmt, ...)
{
	COLLAPSED_LOGF(
		CurrentEntry->AddSphere(Center, Radius, CategoryName, Verbosity, Color, Buffer, bWireframe);
	);
}

void FVisualLogger::BoxLogfImpl(const UObject* Object, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FBox& Box, const FMatrix& Matrix, const FColor& Color, bool bWireframe, const TCHAR* Fmt, ...)
{
	COLLAPSED_LOGF(
		CurrentEntry->AddBox(Box, Matrix, CategoryName, Verbosity, Color, Buffer, /*Thickness = */0, bWireframe);
	);
}

void FVisualLogger::ConeLogfImpl(const UObject* Object, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FVector& Origin, const FVector& Direction, const float Length, const float Angle, const FColor& Color, bool bWireframe, const TCHAR* Fmt, ...)
{
	COLLAPSED_LOGF(
		CurrentEntry->AddCone(Origin, Direction, Length, Angle, Angle, CategoryName, Verbosity, Color, Buffer, /*Thickness = */0, bWireframe);
	);
}

void FVisualLogger::CylinderLogfImpl(const UObject* Object, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FVector& Start, const FVector& End, const float Radius, const FColor& Color, bool bWireframe, const TCHAR* Fmt, ...)
{
	COLLAPSED_LOGF(
		CurrentEntry->AddCylinder(Start, End, Radius, CategoryName, Verbosity, Color, Buffer, /*Thickness = */0, bWireframe);
	);
}

void FVisualLogger::CapsuleLogfImpl(const UObject* Object, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FVector& Base, float HalfHeight, float Radius, const FQuat& Rotation, const FColor& Color, bool bWireframe, const TCHAR* Fmt, ...)
{
	COLLAPSED_LOGF(
		CurrentEntry->AddCapsule(Base, HalfHeight, Radius, Rotation, CategoryName, Verbosity, Color, Buffer, bWireframe);
	);
}

void FVisualLogger::PulledConvexLogfImpl(const UObject* Object, const FName& CategoryName, ELogVerbosity::Type Verbosity, const TArray<FVector>& ConvexPoints, float MinZ, float MaxZ, const FColor& Color, const TCHAR* Fmt, ...)
{
	COLLAPSED_LOGF(
		CurrentEntry->AddPulledConvex(ConvexPoints, MinZ, MaxZ, CategoryName, Verbosity, Color, Buffer);
	);
}

void FVisualLogger::MeshLogfImpl(const UObject* Object, const FName& CategoryName, ELogVerbosity::Type Verbosity, const TArray<FVector>& Vertices, const TArray<int32>& Indices, const FColor& Color, const TCHAR* Fmt, ...)
{
	COLLAPSED_LOGF(
		CurrentEntry->AddMesh(Vertices, Indices, CategoryName, Verbosity, Color, Buffer);
	);
}

void FVisualLogger::ConvexLogfImpl(const UObject* Object, const FName& CategoryName, ELogVerbosity::Type Verbosity, const TArray<FVector>& Points, const FColor& Color, const TCHAR* Fmt, ...)
{
	COLLAPSED_LOGF(
		CurrentEntry->AddConvexElement(Points, CategoryName, Verbosity, Color, Buffer);
	);
}

void FVisualLogger::HistogramDataLogfImpl(const UObject* Object, const FName& CategoryName, ELogVerbosity::Type Verbosity, FName GraphName, FName DataName, const FVector2D& Data, const FColor& Color, const TCHAR* Fmt, ...)
{
	COLLAPSED_LOGF(
		CurrentEntry->AddHistogramData(Data, CategoryName, Verbosity, GraphName, DataName);
	);
}

#undef COLLAPSED_LOGF


namespace
{
	UWorld* GetWorldForVisualLogger(const UObject* Object)
	{
		UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(Object, EGetWorldErrorMode::ReturnNull) : nullptr;
#if WITH_EDITOR
		UEditorEngine *EEngine = Cast<UEditorEngine>(GEngine);
		if (GIsEditor && EEngine != nullptr && World == nullptr)
		{
			// lets use PlayWorld during PIE/Simulate and regular world from editor otherwise, to draw debug information
			World = EEngine->PlayWorld != nullptr 
				? ToRawPtr(EEngine->PlayWorld) 
				: (EEngine->GetWorldContexts().Num() > 0 ? EEngine->GetEditorWorldContext().World() : nullptr);
		}

#endif
		if (!GIsEditor && World == nullptr && GEngine)
		{
			World =  GEngine->GetWorld();
		}

		return World;
	}
}

TMap<const UWorld*, FVisualLogger::FOwnerToChildrenRedirectionMap> FVisualLogger::WorldToRedirectionMap;
int32 FVisualLogger::bIsRecording = false;
FVisualLogger::FNavigationDataDump FVisualLogger::NavigationDataDumpDelegate;

bool FVisualLogger::CheckVisualLogInputInternal(const UObject* Object, const FName& CategoryName, ELogVerbosity::Type, UWorld** OutWorld, FVisualLogEntry** OutCurrentEntry)
{
	if (IsRecording() == false || !Object || !GEngine || GEngine->bDisableAILogging || Object->HasAnyFlags(RF_ClassDefaultObject))
	{
		return false;
	}

	FVisualLogger& VisualLogger = FVisualLogger::Get();
	if (VisualLogger.IsBlockedForAllCategories() && VisualLogger.IsCategoryAllowed(CategoryName) == false)
	{
		return false;
	}

	if (OutWorld)
	{
		*OutWorld = GetWorldForVisualLogger(Object);
		if (!ensure(*OutWorld != nullptr))
		{
			return false;
		}
	}

	if (OutCurrentEntry)
	{
		*OutCurrentEntry = VisualLogger.GetEntryToWrite(Object, VisualLogger.GetTimeStampForObject(Object));
		if (*OutCurrentEntry == nullptr)
		{
			return false;
		}
	}

	return true;
}

FVisualLogEntry* FVisualLogger::GetEntryToWrite(const UObject* LogOwner, const FLogCategoryBase& LogCategory)
{
	FVisualLogEntry* LogEntry;
	if (CheckVisualLogInputInternal(LogOwner, LogCategory.GetCategoryName(), ELogVerbosity::Log, /*OutWorld*/ nullptr, &LogEntry))
	{
		return LogEntry;
	}

	return nullptr;
}

double FVisualLogger::GetTimeStampForObject(const UObject* Object) const
{
	// Licensees can write their own synchronized clock that can work across the network
	// such an implementation could use the exchanged ServerTime and the relative ClientTime offset.
	if (GetTimeStampFunc)
	{
		return GetTimeStampFunc(Object);
	}

	const UWorld* WorldForTimeStamp = nullptr;
#if WITH_EDITOR
	// When we're in the Editor, use a Global Engine TimeStamp so that we can synchronize between Client and Server instances.
	UEditorEngine* EditorEngine = GIsEditor ? Cast<UEditorEngine>(GEngine) : nullptr;
	if (EditorEngine)
	{
		// We will always have the Editor world to use.  This will ensure a consistent clock since it does not reset
		// when more clients are added or removed and can exist before a PIE session is started.
		WorldForTimeStamp = EditorEngine->GetEditorWorldContext().World();
		if (ensureMsgf(WorldForTimeStamp, TEXT("We always expect to have an EditorWorld in Editor")))
		{
			using namespace UE::VisLog::Private;
			if (EditorOnly::EditorBaseTimeStamp <= 0.0)
			{
				EditorOnly::EditorBaseTimeStamp = WorldForTimeStamp->TimeSeconds;
			}

			return WorldForTimeStamp->TimeSeconds - EditorOnly::EditorBaseTimeStamp;
		}
	}

#endif

	// This will be the fallback mode in standalone.  We do not have a synchronized clock.
	if (!WorldForTimeStamp)
	{
		WorldForTimeStamp = GetWorldForVisualLogger(Object);
	}

	return WorldForTimeStamp ? WorldForTimeStamp->TimeSeconds : 0.0;
}

void FVisualLogger::SetGetTimeStampFunc(const TFunction<double(const UObject*)> Function)
{
	GetTimeStampFunc = Function;
}

void FVisualLogger::AddClassToAllowList(UClass& InClass)
{
	ClassAllowList.AddUnique(&InClass);
}

bool FVisualLogger::IsClassAllowed(const UClass& InClass) const
{
	for (const UClass* AllowedRoot : ClassAllowList)
	{
		if (InClass.IsChildOf(AllowedRoot))
		{
			return true;
		}
	}

	return false;
}

void FVisualLogger::AddObjectToAllowList(const UObject& InObject)
{
	FWriteScopeLock Lock(EntryRWLock);
	bool bIsAlreadyInSet = false;
	ObjectAllowList.Add(&InObject, &bIsAlreadyInSet);

	if (!bIsAlreadyInSet)
	{
		FVisualLogEntry* CurrentEntry = CurrentEntryPerObject.Find(&InObject);
		if (CurrentEntry)
		{
			CurrentEntry->SetPassedObjectAllowList(true);
		}

	    // We are not locking the thread entry map, we do not expect other threads adding new entries while editing allow list
	    // @todo we should add a thread access detector
		for (FVisualLoggerObjectEntryMap* ThreadCurrentEntryMap : ThreadCurrentEntryMaps)
		{
			checkf(ThreadCurrentEntryMap, TEXT("Unexpected null map entry pointer"));
			FVisualLogEntry* ThreadCurrentEntry = ThreadCurrentEntryMap->Find(&InObject);
			if (ThreadCurrentEntry)
			{
				ThreadCurrentEntry->SetPassedObjectAllowList(true);
			}
		}
	}
}

void FVisualLogger::ClearObjectAllowList()
{
	FWriteScopeLock Lock(EntryRWLock);
	for (FObjectKey It : ObjectAllowList)
	{
		FVisualLogEntry* CurrentEntry = CurrentEntryPerObject.Find(It);
		if (CurrentEntry)
		{
			CurrentEntry->SetPassedObjectAllowList(false);
		}

	    // We are not locking the thread entry map, we do not expect other threads adding new entries while editing allow list
	    // @todo we should add a thread access detector
		for (FVisualLoggerObjectEntryMap* ThreadCurrentEntryMap : ThreadCurrentEntryMaps)
		{
			checkf(ThreadCurrentEntryMap, TEXT("Unexpected null map entry pointer"));
			FVisualLogEntry* ThreadCurrentEntry = ThreadCurrentEntryMap->Find(It);
			if (ThreadCurrentEntry)
			{
				ThreadCurrentEntry->SetPassedObjectAllowList(false);
			}
		}
	}

	ObjectAllowList.Empty();
}

bool FVisualLogger::IsObjectAllowed(const UObject* InObject) const
{
	return ObjectAllowList.Contains(InObject);
}

FVisualLogEntry* FVisualLogger::GetLastEntryForObject(const UObject* Object)
{
	const UObject* LogOwner = nullptr;
	{
		FReadScopeLock Lock(RedirectRWLock);
		LogOwner = FindRedirectionInternal(Object);
	}
	if (LogOwner == nullptr)
	{
		return nullptr;
	}

	FVisualLoggerObjectEntryMap& ThreadCurrentEntryPerObject = GetThreadCurrentEntryMap();
	return ThreadCurrentEntryPerObject.Find(LogOwner);
}

FVisualLogEntry* FVisualLogger::GetEntryToWrite(const UObject* Object, const double TimeStamp, ECreateIfNeeded ShouldCreate)
{
	const UObject* LogOwner = nullptr;
	{
		FReadScopeLock Lock(RedirectRWLock);
		LogOwner = FindRedirectionInternal(Object);
	}
	if (LogOwner == nullptr)
	{
		return nullptr;
	}

	FVisualLoggerObjectEntryMap& ThreadCurrentEntryPerObject = GetThreadCurrentEntryMap();
	FVisualLogEntry* ThreadCurrentEntry = ThreadCurrentEntryPerObject.Find(LogOwner);
	bool bInitializeEntry = false;
	if (ThreadCurrentEntry)
	{
		if (ThreadCurrentEntry->ShouldLog(ShouldCreate))
		{
			if (ThreadCurrentEntry->ShouldFlush(TimeStamp))
			{
				FWriteScopeLock Lock(EntryRWLock);
				if (FVisualLogEntry* GlobalCurrentEntry = GetEntryToWriteInternal(LogOwner, ThreadCurrentEntry->TimeStamp, ShouldCreate))
				{
					ThreadCurrentEntry->MoveTo(*GlobalCurrentEntry);
				}
			}

			bInitializeEntry = !ThreadCurrentEntry->bIsInitialized;
		}
	}
	else
	{
		// Check to see if it exists in the global map, 
		// if so then force creation for the current thread
		if (ShouldCreate != ECreateIfNeeded::Create)
		{
			FReadScopeLock Lock(EntryRWLock);
			if (CurrentEntryPerObject.Find(LogOwner))
			{
				ShouldCreate = ECreateIfNeeded::Create;
			}
		}

		if (ShouldCreate == ECreateIfNeeded::Create)
		{
			ThreadCurrentEntry = &ThreadCurrentEntryPerObject.Add(LogOwner);

			CalculateEntryAllowLogging(ThreadCurrentEntry, LogOwner, Object);

			bInitializeEntry = ThreadCurrentEntry->bIsAllowedToLog;
		}
	}

	if (bInitializeEntry)
	{
		checkf(ThreadCurrentEntry != nullptr, TEXT("bInitializeEntry can only be true when CurrentEntry is valid."));
		ThreadCurrentEntry->InitializeEntry(TimeStamp);
	}

	if (ThreadCurrentEntry != nullptr && ThreadCurrentEntry->bIsAllowedToLog)
	{
		bIsFlushRequired = true;
	}
	else
	{
		ThreadCurrentEntry = nullptr;
	}

	return ThreadCurrentEntry;
}


FVisualLogEntry* FVisualLogger::GetEntryToWriteInternal(const UObject* Object, const double TimeStamp, const ECreateIfNeeded ShouldCreate)
{
	using namespace UE::VisLog::Private;

	// No redirection needed, it should have been done at the time of the thread entry was computed
	const UObject* LogOwner = Object;
	if (LogOwner == nullptr)
	{
		return nullptr;
	}

	// Entry can be created or reused (after being flushed) and will need to be initialized
	bool bInitializeEntry = false;
	FVisualLogEntry* CurrentEntry = CurrentEntryPerObject.Find(LogOwner);

	if (CurrentEntry)
	{
		if (CurrentEntry->ShouldLog(ShouldCreate))
		{
			if (CurrentEntry->ShouldFlush(TimeStamp))
			{
				FlushEntry(*CurrentEntry, LogOwner);
			}
	
			bInitializeEntry = !CurrentEntry->bIsInitialized;
		}
	}
	else if (ShouldCreate == ECreateIfNeeded::Create)
	{
		// It's first and only one usage of LogOwner as regular object to get names. We assume once that LogOwner is correct here and only here.
		CurrentEntry = &CurrentEntryPerObject.Add(LogOwner);

		if (bForceUniqueLogNames && UniqueLogPrefix.IsEmpty())
		{
			UniqueLogPrefix = FString::Printf(TEXT("[%d] "), UniqueRunNumber);
		}

		const UWorld* World = GetWorldForVisualLogger(LogOwner);
		const bool bIsStandalone = (World == nullptr || World->GetNetMode() == NM_Standalone);
		const FName LogName(*FString::Printf(TEXT("%s%s%s"),
			*UniqueLogPrefix,
			bIsStandalone ? TEXT("") : *FString::Printf(TEXT("(%s) "), *GetDebugStringForWorld(World)),
			*LogOwner->GetName()));

		ObjectToNameMap.Add(LogOwner, LogName);
		ObjectToClassNameMap.Add(LogOwner, *(LogOwner->GetClass()->GetName()));
		ObjectToWorldMap.Add(LogOwner, World);

		CalculateEntryAllowLogging(CurrentEntry, LogOwner, Object);

		bInitializeEntry = CurrentEntry->bIsAllowedToLog;
	}

	if (bInitializeEntry)
	{
		checkf(CurrentEntry != nullptr, TEXT("bInitializeEntry can only be true when CurrentEntry is valid."));
		CurrentEntry->InitializeEntry(TimeStamp);

		// Let's record the World Time as the local instance sees it.
		if (const TWeakObjectPtr<const UWorld>* WorldWeakPtr = ObjectToWorldMap.Find(LogOwner))
		{
			if (const UWorld* World = WorldWeakPtr->Get())
			{
				CurrentEntry->WorldTimeStamp = World->TimeSeconds;
			}
		}

		if (const AActor* ObjectAsActor = Cast<AActor>(LogOwner))
		{
			CurrentEntry->Location = ObjectAsActor->GetActorLocation();
			CurrentEntry->bIsLocationValid = true;
		}

		FReadScopeLock RedirectScopeLock(RedirectRWLock);
		FOwnerToChildrenRedirectionMap& RedirectionMap = GetRedirectionMap(LogOwner);
		if (const IVisualLoggerDebugSnapshotInterface* DebugSnapshotInterface = Cast<const IVisualLoggerDebugSnapshotInterface>(LogOwner))
		{
			DebugSnapshotInterface->GrabDebugSnapshot(CurrentEntry);
		}
		
		TArray<TWeakObjectPtr<const UObject>>* Children = RedirectionMap.Find(LogOwner);
		if (Children != nullptr)
		{
			for (TWeakObjectPtr<const UObject>& Child : *Children)
			{
				if (const IVisualLoggerDebugSnapshotInterface* DebugSnapshotInterface = Cast<const IVisualLoggerDebugSnapshotInterface>(Child.Get()))
				{
					DebugSnapshotInterface->GrabDebugSnapshot(CurrentEntry);
				}
			}
		}
	}

	if (CurrentEntry != nullptr && CurrentEntry->bIsAllowedToLog)
	{
		bIsFlushRequired = true;
	}
	else
	{
		CurrentEntry = nullptr;
	}

	return CurrentEntry;
}

void FVisualLogger::CalculateEntryAllowLogging(FVisualLogEntry* CurrentEntry, const UObject* LogOwner, const UObject* Object)
{
	// IsClassAllowed isn't super fast, but this gets calculated only once for every 
	// object trying to log something
	CurrentEntry->bPassedClassAllowList = (ClassAllowList.Num() == 0) || IsClassAllowed(*LogOwner->GetClass()) || IsClassAllowed(*Object->GetClass());
	CurrentEntry->bPassedObjectAllowList = IsObjectAllowed(LogOwner);
	CurrentEntry->UpdateAllowedToLog();
}

void FVisualLogger::Tick(float DeltaTime)
{
	if (bIsFlushRequired)
	{
		Flush();
		bIsFlushRequired = false;
	}

	if (bContainsInvalidRedirects)
	{
		CleanupRedirects();
		bContainsInvalidRedirects = false;
	}
}

void FVisualLogger::FlushThreadsEntries()
{
	// We are not locking the thread entry map, we do not expect other threads adding new entries while flushing
	// @todo we should add a thread access detector
	for (FVisualLoggerObjectEntryMap* ThreadCurrentEntryMap : ThreadCurrentEntryMaps)
	{
		checkf(ThreadCurrentEntryMap, TEXT("Unexpected null map entry pointer"));
		for (auto& ThreadCurrentEntry : *ThreadCurrentEntryMap)
		{
			if (ThreadCurrentEntry.Value.bIsInitialized)
			{
				const UObject* OwnerObject = ThreadCurrentEntry.Key.ResolveObjectPtrEvenIfGarbage();
				if (FVisualLogEntry* GlobalCurrentEntry = GetEntryToWriteInternal(OwnerObject, ThreadCurrentEntry.Value.TimeStamp, ECreateIfNeeded::Create))
				{
					ThreadCurrentEntry.Value.MoveTo(*GlobalCurrentEntry);
				}
			}
		}
	}
}

void FVisualLogger::Flush()
{
	FWriteScopeLock Lock(EntryRWLock);

	FlushThreadsEntries();

	for (auto &CurrentEntry : CurrentEntryPerObject)
	{
		if (CurrentEntry.Value.bIsInitialized)
		{
			FlushEntry(CurrentEntry.Value, CurrentEntry.Key);
		}
	}
}

void FVisualLogger::FlushEntry(FVisualLogEntry& Entry, const FObjectKey& ObjectKey)
{
	ensureMsgf(Entry.bIsInitialized, TEXT("FlushEntry should only be called with an initialized entry."));

	const UObject* OwnerObject = ObjectKey.ResolveObjectPtrEvenIfGarbage();
	for (FVisualLogDevice* Device : OutputDevices)
	{
		Device->Serialize(OwnerObject, ObjectToNameMap[ObjectKey], ObjectToClassNameMap[ObjectKey], Entry);
	}
	Entry.Reset();
}

thread_local FVisualLogger::FVisualLoggerObjectEntryMap* GThreadCurrentEntryPerObject = nullptr;
FVisualLogger::FVisualLoggerObjectEntryMap& FVisualLogger::GetThreadCurrentEntryMap()
{
	if (GThreadCurrentEntryPerObject == nullptr)
	{
		GThreadCurrentEntryPerObject = new FVisualLoggerObjectEntryMap();
		FWriteScopeLock Lock(EntryRWLock);
		ThreadCurrentEntryMaps.Add(GThreadCurrentEntryPerObject);
	}
	checkf(GThreadCurrentEntryPerObject, TEXT("Unexpected null map entry pointer"));
	return *GThreadCurrentEntryPerObject;
}

void FVisualLogger::EventLog(const UObject* Object, const FName EventTag1, const FVisualLogEventBase& Event1, const FVisualLogEventBase& Event2, const FVisualLogEventBase& Event3, const FVisualLogEventBase& Event4, const FVisualLogEventBase& Event5, const FVisualLogEventBase& Event6)
{
	EventLog(Object, EventTag1, Event1, Event2, Event3, Event4, Event5);
	EventLog(Object, EventTag1, Event6);
}


void FVisualLogger::EventLog(const UObject* Object, const FName EventTag1, const FVisualLogEventBase& Event1, const FVisualLogEventBase& Event2, const FVisualLogEventBase& Event3, const FVisualLogEventBase& Event4, const FVisualLogEventBase& Event5)
{
	EventLog(Object, EventTag1, Event1, Event2, Event3, Event4);
	EventLog(Object, EventTag1, Event5);
}


void FVisualLogger::EventLog(const UObject* Object, const FName EventTag1, const FVisualLogEventBase& Event1, const FVisualLogEventBase& Event2, const FVisualLogEventBase& Event3, const FVisualLogEventBase& Event4)
{
	EventLog(Object, EventTag1, Event1, Event2, Event3);
	EventLog(Object, EventTag1, Event4);
}


void FVisualLogger::EventLog(const UObject* Object, const FName EventTag1, const FVisualLogEventBase& Event1, const FVisualLogEventBase& Event2, const FVisualLogEventBase& Event3)
{
	EventLog(Object, EventTag1, Event1, Event2);
	EventLog(Object, EventTag1, Event3);
}


void FVisualLogger::EventLog(const UObject* Object, const FName EventTag1, const FVisualLogEventBase& Event1, const FVisualLogEventBase& Event2)
{
	EventLog(Object, EventTag1, Event1);
	EventLog(Object, EventTag1, Event2);
}


void FVisualLogger::EventLog(const UObject* LogOwner, const FVisualLogEventBase& Event1, const FName EventTag1, const FName EventTag2, const FName EventTag3, const FName EventTag4, const FName EventTag5, const FName EventTag6)
{
	EventLog(LogOwner, EventTag1, Event1, EventTag2, EventTag3, EventTag4, EventTag5, EventTag6);
}


void FVisualLogger::EventLog(const UObject* Object, const FName EventTag1, const FVisualLogEventBase& Event, const FName EventTag2, const FName EventTag3, const FName EventTag4, const FName EventTag5, const FName EventTag6)
{
	SCOPE_CYCLE_COUNTER(STAT_VisualLog);
	UWorld *World = nullptr;
	FVisualLogEntry *CurrentEntry = nullptr;
	const FName CategoryName(*Event.Name);
	if (CheckVisualLogInputInternal(Object, CategoryName, ELogVerbosity::Log, &World, &CurrentEntry) == false)
	{
		return;
	}

	int32 Index = CurrentEntry->Events.Find(FVisualLogEvent(Event));
	if (Index != INDEX_NONE)
	{
		CurrentEntry->Events[Index].Counter++;
	}
	else
	{
		Index = CurrentEntry->AddEvent(Event);
	}

	CurrentEntry->Events[Index].EventTags.FindOrAdd(EventTag1)++;
	CurrentEntry->Events[Index].EventTags.FindOrAdd(EventTag2)++;
	CurrentEntry->Events[Index].EventTags.FindOrAdd(EventTag3)++;
	CurrentEntry->Events[Index].EventTags.FindOrAdd(EventTag4)++;
	CurrentEntry->Events[Index].EventTags.FindOrAdd(EventTag5)++;
	CurrentEntry->Events[Index].EventTags.FindOrAdd(EventTag6)++;
	CurrentEntry->Events[Index].EventTags.Remove(NAME_None);
}

void FVisualLogger::NavigationDataDump(const UObject* Object, const FLogCategoryBase& Category, const ELogVerbosity::Type Verbosity, const FBox& Box)
{
	NavigationDataDump(Object, Category.GetCategoryName(), Verbosity, Box);
}

void FVisualLogger::NavigationDataDump(const UObject* Object, const FName& CategoryName, const ELogVerbosity::Type Verbosity, const FBox& Box)
{
	SCOPE_CYCLE_COUNTER(STAT_VisualLog);

	UWorld* World = nullptr;
	FVisualLogEntry* CurrentEntry = nullptr;
	if (CheckVisualLogInputInternal(Object, CategoryName, Verbosity, &World, &CurrentEntry) == false
		|| CurrentEntry == nullptr)
	{
		return;
	}

	NavigationDataDumpDelegate.Broadcast(Object, CategoryName, Verbosity, Box, *World, *CurrentEntry);
}

FVisualLogger& FVisualLogger::Get()
{
	static FVisualLogger GVisLog;
	return GVisLog;
}

FVisualLogger::FVisualLogger()
{
	bForceUniqueLogNames = true;
	bIsRecordingToFile = false;
	bIsRecordingToTrace = false;
	bIsFlushRequired = false;

	BlockAllCategories(false);
	AddDevice(&FVisualLoggerBinaryFileDevice::Get());
	SetIsRecording(GEngine ? !!GEngine->bEnableVisualLogRecordingOnStart : false);
	SetIsRecordingOnServer(false);

	if (FParse::Param(FCommandLine::Get(), TEXT("EnableAILogging")))
	{
		SetIsRecording(true);
		SetIsRecordingToFile(true);
	}

	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(TEXT("VisualLogger"), 0.0f, [this](const float DeltaTime)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FVisualLogger_Tick);

		Tick(DeltaTime);

		return true;
	});

#if WITH_EDITOR
	PIEStartedHandle = FWorldDelegates::OnPIEStarted.AddLambda(
		[this](UGameInstance*) {
			// We only want to do this if we are using unique log names, otherwise it makes more sense
			// to just continue logging at later timestamps on the same timeline as the previous one.
			if (bForceUniqueLogNames)
			{
				if (GEngine->IsSettingUpPlayWorld())
				{
					int32 OldUniqueNumber = UE::VisLog::Private::UniqueRunNumber;
					OnDataReset();
					UE::VisLog::Private::UniqueRunNumber = OldUniqueNumber + 1;
				}

			}
		});
#endif
}

void FVisualLogger::TearDown()
{
#if WITH_EDITOR
	FWorldDelegates::OnPIEStarted.Remove(PIEStartedHandle);
#endif

	SetIsRecording(false);
	SetIsRecordingToFile(false);
	RemoveDevice(&FVisualLoggerBinaryFileDevice::Get());
}

void FVisualLogger::OnDataReset()
{
	using namespace UE::VisLog::Private;

	UniqueRunNumber = 0;
	UniqueLogPrefix.Empty();

#if WITH_EDITOR
	UEditorEngine* EditorEngine = GIsEditor ? Cast<UEditorEngine>(GEngine) : nullptr;
	if (EditorEngine)
	{
		const UWorld* EditorWorld = EditorEngine->GetEditorWorldContext().World();
		if (EditorWorld)
		{
			// Reset the base timestamp to zero so the next (aka first) log entry will set this variable to determine the global offset in GetTimeStampForObject
			// This ensures that when you start recording, the first entry appears to be at 0.0 (computed as TimeStamp - EditorBaseTimeStamp)
			EditorOnly::EditorBaseTimeStamp = 0.0;
		}
	}
#endif
}

void FVisualLogger::Cleanup(UWorld* OldWorld, const bool bReleaseMemory)
{
	const bool WasRecordingToFile = IsRecordingToFile();
	if (WasRecordingToFile)
	{
		SetIsRecordingToFile(false);
	}

	Flush();
	for (FVisualLogDevice* Device : FVisualLogger::Get().OutputDevices)
	{
		Device->Cleanup(bReleaseMemory);
	}

	if (OldWorld != nullptr)
	{
		// perform cleanup only if provided world is valid and was registered
		if (WorldToRedirectionMap.Remove(OldWorld))
		{
		    if (WorldToRedirectionMap.Num() == 0)
            {
                WorldToRedirectionMap.Reset();
                ObjectToWorldMap.Reset();
                ChildToOwnerMap.Reset();
                CurrentEntryPerObject.Reset();
				for (FVisualLoggerObjectEntryMap* ThreadCurrentEntryMap : ThreadCurrentEntryMaps)
				{
					checkf(ThreadCurrentEntryMap, TEXT("Unexpected null map entry pointer"));
					ThreadCurrentEntryMap->Reset();
				}
                ObjectToNameMap.Reset();
                ObjectToClassNameMap.Reset();
            }
            else
            {
                for (auto It = ObjectToWorldMap.CreateIterator(); It; ++It)
                {
                    if (It.Value() == OldWorld)
                    {
						FObjectKey Obj = It.Key();
                        ObjectToWorldMap.Remove(Obj);
                        CurrentEntryPerObject.Remove(Obj);
						for (FVisualLoggerObjectEntryMap* ThreadCurrentEntryMap : ThreadCurrentEntryMaps)
						{
							checkf(ThreadCurrentEntryMap, TEXT("Unexpected null map entry pointer"));
							ThreadCurrentEntryMap->Remove(Obj);
						}
                        ObjectToNameMap.Remove(Obj);
                        ObjectToClassNameMap.Remove(Obj);
                    }
                }

                for (FChildToOwnerRedirectionMap::TIterator It = ChildToOwnerMap.CreateIterator(); It; ++It)
                {
					UObject* Object = It->Key.ResolveObjectPtrEvenIfGarbage();
					if (Object == nullptr || Object->GetWorld() == OldWorld)
                    {
                        It.RemoveCurrent();
                    }
                }
            }
		}
	}
	else
	{
		WorldToRedirectionMap.Reset();
		ObjectToWorldMap.Reset();
		ChildToOwnerMap.Reset();
		CurrentEntryPerObject.Reset();
		for (FVisualLoggerObjectEntryMap* ThreadCurrentEntryMap : ThreadCurrentEntryMaps)
		{
			checkf(ThreadCurrentEntryMap, TEXT("Unexpected null map entry pointer"));
			ThreadCurrentEntryMap->Reset();
		}
		ObjectToNameMap.Reset();
		ObjectToClassNameMap.Reset();
	}

	LastUniqueIds.Reset();

	if (WasRecordingToFile)
	{
		SetIsRecordingToFile(true);
	}
}

int32 FVisualLogger::GetUniqueId(const double Timestamp)
{
	return LastUniqueIds.FindOrAdd(Timestamp)++;
}

FVisualLogger::FOwnerToChildrenRedirectionMap& FVisualLogger::GetRedirectionMap(const UObject* InObject)
{
	const UWorld* World = nullptr;
	if (const TWeakObjectPtr<const UWorld>* Entry = FVisualLogger::Get().ObjectToWorldMap.Find(InObject))
	{
		World = Entry->Get();
	}

	if (World == nullptr)
	{
		World = GetWorldForVisualLogger(InObject);
	}

	return WorldToRedirectionMap.FindOrAdd(World);
}

UObject* FVisualLogger::RedirectInternal(const UObject* FromObject, const UObject* ToObject)
{
	if (FromObject == ToObject || FromObject == nullptr || ToObject == nullptr)
	{
		return nullptr;
	}

	const TWeakObjectPtr<const UObject> FromWeakPtr(FromObject);

	TWeakObjectPtr<const UObject>& WeakOwnerEntry = ChildToOwnerMap.FindOrAdd(FromObject);
	const UObject* DirectOwner = WeakOwnerEntry.Get();
	const UObject* NewDirectOwner = ToObject;

	if (DirectOwner != ToObject)
	{
		FOwnerToChildrenRedirectionMap& OwnerToChildrenMap = GetRedirectionMap(FromObject);
		const TArray<TWeakObjectPtr<const UObject>>* ObjectChildren = OwnerToChildrenMap.Find(FromObject);

		// A valid direct owner indicates that FromObject was in one or many list(s)
		// of children so remove it and its children from the owner hierarchy
		if (DirectOwner != nullptr)
		{
			const UObject* Owner = DirectOwner;
			while (Owner != nullptr)
			{
				if (TArray<TWeakObjectPtr<const UObject>>* DirectOwnerChildren = OwnerToChildrenMap.Find(Owner))
				{
					DirectOwnerChildren->RemoveSingleSwap(FromWeakPtr);
					if (ObjectChildren != nullptr)
					{
						for (const TWeakObjectPtr<const UObject>& Child : *ObjectChildren)
						{
							DirectOwnerChildren->RemoveSingleSwap(Child);
						}
					}
				}

				const TWeakObjectPtr<const UObject>* WeakOwner = ChildToOwnerMap.Find(Owner);
				Owner = WeakOwner ? WeakOwner->Get() : nullptr;
			}
		}

		// Add this child with its children (if any) to the owner hierarchy
		const UObject* Owner = NewDirectOwner;
		while (Owner != nullptr)
		{
			TArray<TWeakObjectPtr<const UObject>>* OwnerChildren = OwnerToChildrenMap.Find(Owner);
			if (OwnerChildren == nullptr)
			{
				// Entry not found, add it and refresh current object children since it might have been reallocated
				OwnerChildren = &OwnerToChildrenMap.Emplace(Owner);
				ObjectChildren = ObjectChildren ? OwnerToChildrenMap.Find(FromObject) : nullptr;
			}

			check(OwnerChildren);
			OwnerChildren->Add(FromWeakPtr);
			if (ObjectChildren != nullptr)
			{
				OwnerChildren->Append(*ObjectChildren);
			}

			const TWeakObjectPtr<const UObject>* WeakOwner = ChildToOwnerMap.Find(Owner);
			Owner = WeakOwner ? WeakOwner->Get() : nullptr;
		}
	}

	WeakOwnerEntry = ToObject;

	UObject* NewRedirection = FindRedirectionInternal(ToObject);
	return NewRedirection;
}

UObject* FVisualLogger::FindRedirectionInternal(const UObject* Object) const
{
	TWeakObjectPtr<const UObject> TargetWeakPtr(Object);
	const TWeakObjectPtr<const UObject>* Parent = &TargetWeakPtr;

	while (Parent)
	{
		Parent = ChildToOwnerMap.Find(TargetWeakPtr.Get(/*bEvenIfPendingKill*/true));
		if (Parent)
		{
			if (Parent->IsValid())
			{
				TargetWeakPtr = *Parent;
			}
			else
			{
				Parent = nullptr;
				bContainsInvalidRedirects = true;
			}
		}
	}

	const UObject* const Target = TargetWeakPtr.Get(/*bEvenIfPendingKill*/true);
	return const_cast<UObject*>(Target);
}

void FVisualLogger::CleanupRedirects()
{
	FWriteScopeLock Lock(RedirectRWLock);
	for (auto It = ChildToOwnerMap.CreateIterator(); It; ++It)
	{
		if(!It.Value().IsValid())
		{
			FObjectKey Obj = It.Key();
			ChildToOwnerMap.Remove(Obj);
		}
	}
}

void FVisualLogger::SetIsRecording(const bool InIsRecording)
{
	if (InIsRecording == false && InIsRecording != !!bIsRecording && FParse::Param(FCommandLine::Get(), TEXT("LogNavOctree")))
	{
		FVisualLogger::NavigationDataDump(GetWorldForVisualLogger(nullptr), LogNavigation, ELogVerbosity::Log, FBox());
	}
	if (IsRecordingToFile())
	{
		SetIsRecordingToFile(false);
	}
	bIsRecording = InIsRecording;
};

void FVisualLogger::SetIsRecordingToFile(const bool InIsRecording)
{
	if (!bIsRecording && InIsRecording)
	{
		SetIsRecording(true);
	}

	UWorld* World = GEngine ? GEngine->GetWorld() : nullptr;

	const FString BaseFileName = LogFileNameGetter.IsBound() ? LogFileNameGetter.Execute() : TEXT("VisualLog");
	const FString MapName = World ? World->GetMapName() : TEXT("");

	const FString OutputFileName = FString::Printf(TEXT("%s_%s"), *BaseFileName, *MapName);

	if (bIsRecordingToFile && !InIsRecording)
	{
		for (FVisualLogDevice* Device : OutputDevices)
		{
			if (Device->HasFlags(EVisualLoggerDeviceFlags::CanSaveToFile))
			{
				Device->SetFileName(OutputFileName);
				Device->StopRecordingToFile(World ? World->TimeSeconds : StartRecordingToFileTime);
			}
		}
	}
	else if (!bIsRecordingToFile && InIsRecording)
	{
		StartRecordingToFileTime = World ? World->TimeSeconds : 0;
		for (FVisualLogDevice* Device : OutputDevices)
		{
			if (Device->HasFlags(EVisualLoggerDeviceFlags::CanSaveToFile))
			{
				Device->StartRecordingToFile(StartRecordingToFileTime);
			}
		}
	}

	bIsRecordingToFile = InIsRecording;
}

void FVisualLogger::SetIsRecordingToTrace(const bool InIsRecording)
{
	if (!bIsRecording && InIsRecording)
	{
		SetIsRecording(true);
	}

	FVisualLoggerTraceDevice& Device = FVisualLoggerTraceDevice::Get();
	if (bIsRecordingToTrace && !InIsRecording)
	{
		Device.StopRecordingToFile(0.0);
		RemoveDevice(&Device);
	}
	else if (!bIsRecordingToTrace && InIsRecording)
	{
		Device.StartRecordingToFile(0.0);
		AddDevice(&Device);
	}

	bIsRecordingToTrace = InIsRecording;
}

void FVisualLogger::SetUseUniqueNames(const bool bEnable)
{
	bForceUniqueLogNames = bEnable;
	UE::VisLog::Private::UniqueLogPrefix.Empty();
}

void FVisualLogger::DiscardRecordingToFile()
{
	if (bIsRecordingToFile)
	{
		for (FVisualLogDevice* Device : OutputDevices)
		{
			if (Device->HasFlags(EVisualLoggerDeviceFlags::CanSaveToFile))
			{
				Device->DiscardRecordingToFile();
			}
		}

		bIsRecordingToFile = false;
	}
}

bool FVisualLogger::IsCategoryLogged(const FLogCategoryBase& Category) const
{
	if ((GEngine && GEngine->bDisableAILogging) || IsRecording() == false)
	{
		return false;
	}

	const FName CategoryName = Category.GetCategoryName();
	if (IsBlockedForAllCategories() && IsCategoryAllowed(CategoryName) == false)
	{
		return false;
	}

	return true;
}

#endif //ENABLE_VISUAL_LOG

const FGuid EVisualLoggerVersion::GUID = FGuid(0xA4237A36, 0xCAEA41C9, 0x8FA218F8, 0x58681BF3);
FCustomVersionRegistration GVisualLoggerVersion(EVisualLoggerVersion::GUID, EVisualLoggerVersion::LatestVersion, TEXT("VisualLogger"));

#if ENABLE_VISUAL_LOG

class FLogVisualizerExec : private FSelfRegisteringExec
{
protected:
	/** Console commands, see embedded usage statement **/
	virtual bool Exec_Dev(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
	{
		if (FParse::Command(&Cmd, TEXT("VISLOG")))
		{
			const FString Command = FParse::Token(Cmd, /*UseEscape*/ false);
			if (Command == TEXT("record"))
			{
				if (GIsEditor)
				{
					FVisualLogger::Get().SetIsRecording(true);
				}
				else
				{
					FVisualLogger::Get().SetIsRecordingToFile(true);
				}
				return true;
			}
			else if (Command == TEXT("stop"))
			{
				if (GIsEditor)
				{
					FVisualLogger::Get().SetIsRecording(false);
				}
				else
				{
					FVisualLogger::Get().SetIsRecordingToFile(false);
				}
				return true;
			}
			else if (Command == TEXT("disableallbut"))
			{
				const FString Category = FParse::Token(Cmd, /*UseEscape*/ true);
				FVisualLogger::Get().BlockAllCategories(true);
				FVisualLogger::Get().AddCategoryToAllowList(*Category);
				return true;
			}
			else if (Command.IsEmpty() && FModuleManager::Get().LoadModulePtr<IModuleInterface>("LogVisualizer"))
			{
				FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("VisualLogger")));
				return true;
			}
		}
		else if (FParse::Command(&Cmd, TEXT("LogNavOctree")))
		{
			FVisualLogger::NavigationDataDump(GetWorldForVisualLogger(InWorld), LogNavigation, ELogVerbosity::Log, FBox());
		}

		return false;
	}
} LogVisualizerExec;

#endif // ENABLE_VISUAL_LOG
