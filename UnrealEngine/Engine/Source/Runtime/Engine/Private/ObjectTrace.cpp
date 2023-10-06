// Copyright Epic Games, Inc. All Rights Reserved.

#include "ObjectTrace.h"
#include "Misc/ScopeRWLock.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ObjectTrace)

#if OBJECT_TRACE_ENABLED

#include "SceneView.h"
#if WITH_EDITOR
#include "Editor.h"
#else
#include "UObject/Package.h"
#include "UObject/UObjectAnnotation.h"
#endif

UE_TRACE_CHANNEL(ObjectChannel)

UE_TRACE_EVENT_BEGIN(Object, Class)
	UE_TRACE_EVENT_FIELD(uint64, Id)
	UE_TRACE_EVENT_FIELD(uint64, SuperId)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, Name)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, Path)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(Object, Object)
	UE_TRACE_EVENT_FIELD(uint64, Id)
	UE_TRACE_EVENT_FIELD(uint64, ClassId)
	UE_TRACE_EVENT_FIELD(uint64, OuterId)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, Name)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, Path)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(Object, ObjectLifetimeBegin2)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(double, RecordingTime)
	UE_TRACE_EVENT_FIELD(uint64, Id)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(Object, ObjectLifetimeEnd2)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(double, RecordingTime)
	UE_TRACE_EVENT_FIELD(uint64, Id)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(Object, ObjectEvent)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint64, Id)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, Event)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(Object, PawnPossess)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint64, ControllerId)
	UE_TRACE_EVENT_FIELD(uint64, PawnId)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(Object, World)
	UE_TRACE_EVENT_FIELD(uint64, Id)
	UE_TRACE_EVENT_FIELD(int32, PIEInstanceId)
	UE_TRACE_EVENT_FIELD(uint8, Type)
	UE_TRACE_EVENT_FIELD(uint8, NetMode)
	UE_TRACE_EVENT_FIELD(bool, IsSimulating)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(Object, RecordingInfo)
	UE_TRACE_EVENT_FIELD(uint64, WorldId)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint32, RecordingIndex)
	UE_TRACE_EVENT_FIELD(uint32, FrameIndex)
	UE_TRACE_EVENT_FIELD(double, ElapsedTime)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(Object, View)
	UE_TRACE_EVENT_FIELD(uint64, PlayerId)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)

	UE_TRACE_EVENT_FIELD(double, PosX)
	UE_TRACE_EVENT_FIELD(double, PosY)
	UE_TRACE_EVENT_FIELD(double, PosZ)

	UE_TRACE_EVENT_FIELD(float, Pitch)
	UE_TRACE_EVENT_FIELD(float, Yaw)
	UE_TRACE_EVENT_FIELD(float, Roll)

	UE_TRACE_EVENT_FIELD(float, Fov)
	UE_TRACE_EVENT_FIELD(float, AspectRatio)
UE_TRACE_EVENT_END()

// Object annotations used for tracing
struct FObjectIdAnnotation
{
	FObjectIdAnnotation()
		: Id(0)
	{}

	// Object ID
	uint64 Id;

	/** Determine if this annotation is default - required for annotations */
	FORCEINLINE bool IsDefault() const
	{
		return Id == 0;
	}

	bool operator == (const FObjectIdAnnotation& other) const
	{
		return Id == other.Id;
	}
};

int32 GetTypeHash(const FObjectIdAnnotation& Annotation)
{
	return GetTypeHash(Annotation.Id);
}

// Object annotations used for tracing
// FUObjectAnnotationSparse<FTracedObjectAnnotation, true> GObjectTracedAnnotations;
FRWLock GObjectTracedSetLock;
TSet<uint64> GObjectTracedSet;
FUObjectAnnotationSparseSearchable<FObjectIdAnnotation, true> GObjectIdAnnotations;

// Handle used to hook to world tick
static FDelegateHandle WorldTickStartHandle;

void TickObjectTraceWorldSubsystem(UWorld* InWorld, ELevelTick InTickType, float InDeltaSeconds)
{
	if(InTickType == LEVELTICK_All)
	{
		if (!InWorld->IsPaused())
		{
			if(UObjectTraceWorldSubsystem* Subsystem = UWorld::GetSubsystem<UObjectTraceWorldSubsystem>(InWorld))
			{
				Subsystem->FrameIndex++;
				Subsystem->ElapsedTime += InDeltaSeconds;

				UE_TRACE_LOG(Object, RecordingInfo, ObjectChannel)
		            << RecordingInfo.WorldId(FObjectTrace::GetObjectId(InWorld))
					<< RecordingInfo.Cycle(FPlatformTime::Cycles64())
					<< RecordingInfo.ElapsedTime(Subsystem->ElapsedTime)
					<< RecordingInfo.FrameIndex(Subsystem->FrameIndex)
					<< RecordingInfo.RecordingIndex(Subsystem->RecordingIndex); 
			}
		}
	}
}

void FObjectTrace::Init()
{
	WorldTickStartHandle = FWorldDelegates::OnWorldTickStart.AddStatic(&TickObjectTraceWorldSubsystem);
}

void FObjectTrace::Destroy()
{
	FWorldDelegates::OnWorldTickStart.Remove(WorldTickStartHandle);
}

void FObjectTrace::Reset()
{
	GObjectIdAnnotations.RemoveAllAnnotations();
	
	{
		FRWScopeLock ScopeLock(GObjectTracedSetLock,SLT_Write);
		GObjectTracedSet.Empty();
	}
}

uint64 FObjectTrace::GetObjectId(const UObject* InObject)
{
	// An object ID uses a combination of its own and its outer's index
	// We do this to represent objects that get renamed into different outers 
	// as distinct traces (we don't attempt to link them).

	auto GetObjectIdInner = [](const UObject* InObjectInner)
	{
		static uint64 CurrentId = 1;

		FObjectIdAnnotation Annotation = GObjectIdAnnotations.GetAnnotation(InObjectInner);
		if(Annotation.Id == 0)
		{
			Annotation.Id = CurrentId++;
			GObjectIdAnnotations.AddAnnotation(InObjectInner, MoveTemp(Annotation));
		}

		return Annotation.Id;
	};

	uint64 Id = 0;
	uint64 OuterId = 0;
	if(InObject)
	{
		Id = GetObjectIdInner(InObject);

		if(const UObject* Outer = InObject->GetOuter())
		{
			OuterId = GetObjectIdInner(Outer);
		}
	}

	return Id | (OuterId << 32);
}

UObject* FObjectTrace::GetObjectFromId(uint64 Id)
{
	FObjectIdAnnotation FindAnnotation;
	// Id used for annotation map doesn't include the parent id in the upper bits, so zero those first
	FindAnnotation.Id = Id & 0x00000000FFFFFFFFll;
	if (FindAnnotation.IsDefault())
	{
		return nullptr;
	}
	
	return GObjectIdAnnotations.Find(FindAnnotation);
}

void FObjectTrace::ResetWorldElapsedTime(const UWorld* World)
{
	if(UObjectTraceWorldSubsystem* WorldSubsystem = UWorld::GetSubsystem<UObjectTraceWorldSubsystem>(World))
	{
		WorldSubsystem->ElapsedTime = 0;
	}
}

double FObjectTrace::GetWorldElapsedTime(const UWorld* World)
{
	if(UObjectTraceWorldSubsystem* WorldSubsystem = UWorld::GetSubsystem<UObjectTraceWorldSubsystem>(World))
	{
		return WorldSubsystem->ElapsedTime;
	}
	return 0;
}

double FObjectTrace::GetObjectWorldElapsedTime(const UObject* InObject)
{
	if(InObject != nullptr)
	{
		if(UWorld* World = InObject->GetWorld())
		{
			if(UObjectTraceWorldSubsystem* WorldSubsystem = UWorld::GetSubsystem<UObjectTraceWorldSubsystem>(World))
			{
				return WorldSubsystem->ElapsedTime;
			}
		}
	}

	return 0;
}

void FObjectTrace::SetWorldRecordingIndex(const UWorld* World, uint16 Index)
{
	if(UObjectTraceWorldSubsystem* WorldSubsystem = UWorld::GetSubsystem<UObjectTraceWorldSubsystem>(World))
	{
		WorldSubsystem->RecordingIndex = Index;
	}
}

uint16 FObjectTrace::GetWorldRecordingIndex(const UWorld* World)
{
	if(UObjectTraceWorldSubsystem* WorldSubsystem = UWorld::GetSubsystem<UObjectTraceWorldSubsystem>(World))
	{
		return WorldSubsystem->RecordingIndex;
	}
	return 0;
}

uint16 FObjectTrace::GetObjectWorldRecordingIndex(const UObject* InObject)
{
	if(InObject != nullptr)
	{
		if(UWorld* World = InObject->GetWorld())
		{
			if(UObjectTraceWorldSubsystem* WorldSubsystem = UWorld::GetSubsystem<UObjectTraceWorldSubsystem>(World))
			{
				return WorldSubsystem->RecordingIndex;
			}
		}
	}

	return 0;
}

uint16 FObjectTrace::GetObjectWorldTickCounter(const UObject* InObject)
{
	if(InObject != nullptr)
	{
		if(UWorld* World = InObject->GetWorld())
		{
			if(UObjectTraceWorldSubsystem* WorldSubsystem = UWorld::GetSubsystem<UObjectTraceWorldSubsystem>(World))
			{
				return WorldSubsystem->FrameIndex;
			}
		}
	}

	return 0;
}

void FObjectTrace::OutputClass(const UClass* InClass)
{
	bool bChannelEnabled = UE_TRACE_CHANNELEXPR_IS_ENABLED(ObjectChannel);
	if (!bChannelEnabled || InClass == nullptr)
	{
		return;
	}
	uint64 ObjectId = GetObjectId(InClass);

	{
		FRWScopeLock ScopeLock(GObjectTracedSetLock, SLT_ReadOnly);
		if (GObjectTracedSet.Contains(ObjectId))
		{
			// Already traced, so skip
			return;
		}
	}
	{
		FRWScopeLock ScopeLock(GObjectTracedSetLock, SLT_Write);
		GObjectTracedSet.Add(ObjectId);
	}

	OutputClass(InClass->GetSuperClass());

	FString ClassPathName = InClass->GetPathName();
	TCHAR ClassName[FName::StringBufferSize];
	uint32 ClassNameLength = InClass->GetFName().ToString(ClassName);

	UE_TRACE_LOG(Object, Class, ObjectChannel)
		<< Class.Id(ObjectId)
		<< Class.SuperId(GetObjectId(InClass->GetSuperClass()))
		<< Class.Name(ClassName, ClassNameLength)
		<< Class.Path(*ClassPathName, ClassPathName.Len());
}

void FObjectTrace::OutputView(const UObject* InPlayer, const FSceneView* InView)
{
	bool bChannelEnabled = UE_TRACE_CHANNELEXPR_IS_ENABLED(ObjectChannel);
	if (!bChannelEnabled || InPlayer == nullptr)
	{
		return;
	}

	if (CANNOT_TRACE_OBJECT(InPlayer->GetWorld()))
	{
		return;
	}

	const FIntRect& ViewRect = InView->CameraConstrainedViewRect;
	float AspectRatio = (float)ViewRect.Width()/(float)ViewRect.Height();

	FMatrix ProjMatrix = InView->ViewMatrices.GetProjectionMatrix();
	float Fov = atan(1.0 / ProjMatrix.M[0][0]) * 2.0 * 180.0 / UE_DOUBLE_PI;

	UE_TRACE_LOG(Object, View, ObjectChannel)
		<< View.Cycle(FPlatformTime::Cycles64())
		<< View.PlayerId(GetObjectId(InPlayer))
		<< View.PosX(InView->ViewLocation.X)
		<< View.PosY(InView->ViewLocation.Y)
		<< View.PosZ(InView->ViewLocation.Z)
		<< View.Pitch(InView->ViewRotation.Pitch)
		<< View.Yaw(InView->ViewRotation.Yaw)
		<< View.Roll(InView->ViewRotation.Roll)
		<< View.Fov(Fov)
		<< View.AspectRatio(AspectRatio);
}

void FObjectTrace::OutputObject(const UObject* InObject)
{
	bool bChannelEnabled = UE_TRACE_CHANNELEXPR_IS_ENABLED(ObjectChannel);
	if (!bChannelEnabled || InObject == nullptr)
	{
		return;
	}

	if(InObject->HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	if (CANNOT_TRACE_OBJECT(InObject->GetWorld()))
	{
		return;
	}
	uint64 ObjectId = GetObjectId(InObject);
	
	{
		FRWScopeLock ScopeLock(GObjectTracedSetLock, SLT_ReadOnly);
		if (GObjectTracedSet.Contains(ObjectId))
		{
			// Already traced, so skip
			return;
		}
	}
	{
		FRWScopeLock ScopeLock(GObjectTracedSetLock, SLT_Write);
		GObjectTracedSet.Add(ObjectId);
	}

	OutputObject(InObject->GetOuter());

	// Trace the object's class first
	TRACE_CLASS(InObject->GetClass());

	TCHAR ObjectName[FName::StringBufferSize];
	uint32 ObjectNameLength = InObject->GetFName().ToString(ObjectName);
	FString ObjectPathName = InObject->GetPathName();

	UE_TRACE_LOG(Object, Object, ObjectChannel)
		<< Object.Id(ObjectId)
		<< Object.ClassId(GetObjectId(InObject->GetClass()))
		<< Object.OuterId(GetObjectId(InObject->GetOuter()))
		<< Object.Name(ObjectName, ObjectNameLength)
		<< Object.Path(*ObjectPathName, ObjectPathName.Len());

	UE_TRACE_LOG(Object, ObjectLifetimeBegin2, ObjectChannel)
		<< ObjectLifetimeBegin2.Cycle(FPlatformTime::Cycles64())
		<< ObjectLifetimeBegin2.RecordingTime(FObjectTrace::GetWorldElapsedTime(InObject->GetWorld()))
		<< ObjectLifetimeBegin2.Id(GetObjectId(InObject));
}

void FObjectTrace::OutputObjectEvent(const UObject* InObject, const TCHAR* InEvent)
{
	bool bChannelEnabled = UE_TRACE_CHANNELEXPR_IS_ENABLED(ObjectChannel);
	if (!bChannelEnabled || InObject == nullptr)
	{
		return;
	}

	if(InObject->HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	if (CANNOT_TRACE_OBJECT(InObject->GetWorld()))
	{
		return;
	}

	TRACE_OBJECT(InObject);

	UE_TRACE_LOG(Object, ObjectEvent, ObjectChannel)
		<< ObjectEvent.Cycle(FPlatformTime::Cycles64())
		<< ObjectEvent.Id(GetObjectId(InObject))
		<< ObjectEvent.Event(InEvent);
}

void FObjectTrace::OutputObjectLifetimeBegin(const UObject* InObject)
{
	TRACE_OBJECT(InObject);
}

void FObjectTrace::OutputObjectLifetimeEnd(const UObject* InObject)
{
	const bool bChannelEnabled = UE_TRACE_CHANNELEXPR_IS_ENABLED(ObjectChannel);
	if (!bChannelEnabled || InObject == nullptr)
	{
		return;
	}

	if(InObject->HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	if (CANNOT_TRACE_OBJECT(InObject->GetWorld()))
	{
		return;
	}

	TRACE_OBJECT(InObject);

	UE_TRACE_LOG(Object, ObjectLifetimeEnd2, ObjectChannel)
		<< ObjectLifetimeEnd2.Cycle(FPlatformTime::Cycles64())
		<< ObjectLifetimeEnd2.RecordingTime(FObjectTrace::GetWorldElapsedTime(InObject->GetWorld()))
		<< ObjectLifetimeEnd2.Id(GetObjectId(InObject));
}

void FObjectTrace::OutputPawnPossess(const UObject* InController, const UObject* InPawn)
{
	bool bChannelEnabled = UE_TRACE_CHANNELEXPR_IS_ENABLED(ObjectChannel);
	if (!bChannelEnabled || InController == nullptr)
	{
		return;
	}

	TRACE_OBJECT(InController);
	if (InPawn)
	{
		TRACE_OBJECT(InPawn);
	}
	
	UE_TRACE_LOG(Object, PawnPossess, ObjectChannel)
		<< PawnPossess.Cycle(FPlatformTime::Cycles64())
		<< PawnPossess.ControllerId(GetObjectId(InController))
		<< PawnPossess.PawnId(GetObjectId(InPawn));
}

void FObjectTrace::OutputWorld(const UWorld* InWorld)
{
	bool bChannelEnabled = UE_TRACE_CHANNELEXPR_IS_ENABLED(ObjectChannel);
	if (!bChannelEnabled || InWorld == nullptr)
	{
		return;
	}

	if (CANNOT_TRACE_OBJECT(InWorld))
	{
		return;
	}

#if WITH_EDITOR
	bool bIsSimulating = GEditor ? GEditor->bIsSimulatingInEditor : false;
#else
	bool bIsSimulating = false;
#endif

	UE_TRACE_LOG(Object, World, ObjectChannel)
		<< World.Id(GetObjectId(InWorld))
		<< World.PIEInstanceId(InWorld->GetOutermost()->GetPIEInstanceID())
		<< World.Type((uint8)InWorld->WorldType)
		<< World.NetMode((uint8)InWorld->GetNetMode())
		<< World.IsSimulating(bIsSimulating);

	// Trace object AFTER world info so we dont risk world info not being present in the trace
	TRACE_OBJECT(InWorld);
}

#endif

