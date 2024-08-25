// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldMetricsSubsystem.h"

#include "Algo/IndexOf.h"
#include "Engine/World.h"
#include "Misc/CoreDelegates.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "WorldMetricInterface.h"
#include "WorldMetricsExtension.h"
#include "WorldMetricsLog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldMetricsSubsystem)

//---------------------------------------------------------------------------------------------------------------------
// UWorldMetricsSubsystem
//---------------------------------------------------------------------------------------------------------------------

bool UWorldMetricsSubsystem::CanHaveWorldMetrics(const UWorld* World)
{
	return World && (World->IsGameWorld() || World->WorldType == EWorldType::Editor);
}

UWorldMetricsSubsystem* UWorldMetricsSubsystem::Get(const UWorld* World)
{
	if (LIKELY(World))
	{
		return World->GetSubsystem<UWorldMetricsSubsystem>();
	}
	return nullptr;
}

bool UWorldMetricsSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return CanHaveWorldMetrics(Cast<UWorld>(Outer));
}

void UWorldMetricsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	UE_LOG(LogWorldMetrics, Log, TEXT("[%hs]"), __FUNCTION__);

	Super::Initialize(Collection);
}

void UWorldMetricsSubsystem::Deinitialize()
{
	UE_LOG(LogWorldMetrics, Log, TEXT("[%hs]"), __FUNCTION__);

	Clear();

	Super::Deinitialize();
}

void UWorldMetricsSubsystem::BeginDestroy()
{
	if (!ensure(!IsEnabled()))
	{
		UE_LOG(LogWorldMetrics, Error, TEXT("[%hs] Unexpected subsystem state: IsEnabled"), __FUNCTION__);
		Clear();
	}
	Super::BeginDestroy();
}

void UWorldMetricsSubsystem::InitializeMetrics()
{
	for (UWorldMetricInterface* Metric : Metrics)
	{
		check(Metric);
		Metric->Initialize();
	}
	PendingWarmUpFrames = WarmUpFrames;
	if (!ensure(PendingWarmUpFrames >= 0))
	{
		UE_LOG(
			LogWorldMetrics, Warning, TEXT("[%hs] Invalid warm-up frames value: %d, reset to zero."), __FUNCTION__,
			PendingWarmUpFrames);
		PendingWarmUpFrames = 0;
	}
}

void UWorldMetricsSubsystem::DeinitializeMetrics()
{
	for (UWorldMetricInterface* Metric : Metrics)
	{
		check(Metric);
		Metric->Deinitialize();
	}
}

void UWorldMetricsSubsystem::Clear()
{
	RemoveAllMetrics();

	Extensions.Reset();
	IndexedOwners.Reset();

	UE_LOG(LogWorldMetrics, Log, TEXT("[%hs]"), __FUNCTION__);
}

void UWorldMetricsSubsystem::Enable(bool bEnable)
{
	if (bEnable)
	{
		if (IsEnabled())
		{
			return;
		}

		if (!CanHaveWorldMetrics(GetWorld()))
		{
			return;
		}

		if (Metrics.IsEmpty())
		{
			return;
		}

		InitializeMetrics();

		if (!ensure(UpdateRateInSeconds >= 0.f))
		{
			UE_LOG(
				LogWorldMetrics, Warning, TEXT("[%hs] Invalid update rate value: %.02fs, reset to zero."), __FUNCTION__,
				UpdateRateInSeconds);
			UpdateRateInSeconds = 0.f;
		}

		/*
		 * Regarding CreateLambda.
		 *
		 * This subsystem uses an update ticker, although it's not a TickableWorldSubsystem. There are two reasons for
		 * this design decision:
		 *
		 * 1. The subsystem only requires an update ticker for metric updates. Due to the short-lived nature of these,
		 *    it makes sense to implement a finer-grain control over the ticker enabling/disabling mechanism and prevent
		 *    incurring an unnecessary cost to the game update thread whenever the subsystem has no metrics.
		 * 2. At the moment of this writing, the validity checks included in UObject delegates incur a significant cost,
		 *    negatively impacting the performance of metric updates. This subsystem exclusively owns world metrics and
		 *    extensions, and their lifetime is bound to the system's Initialize/Deinitialize methods. For this reason,
		 *    we can assume a raw delegate. Note, however, that UObject methods cannot be bound using CreateRaw; hence,
		 *    CreateLambda is used instead.
		 */
		UpdateTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda(
				[this](float DeltaTimeInSeconds)
				{
					OnUpdate(DeltaTimeInSeconds);
					return true;
				}),
			UpdateRateInSeconds);
	}
	else if (IsEnabled())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(UpdateTickerHandle);
		UpdateTickerHandle.Reset();

		DeinitializeMetrics();
	}
}

void UWorldMetricsSubsystem::SetUpdateRateInSeconds(float InSeconds)
{
	if (!ensure(InSeconds >= 0.f))
	{
		UE_LOG(
			LogWorldMetrics, Warning, TEXT("[%hs] Invalid UpdateRateInSeconds input value %.04f: ignored."),
			__FUNCTION__, InSeconds);
		return;
	}

	if (UpdateRateInSeconds != InSeconds)
	{
		UpdateRateInSeconds = InSeconds;

		if (IsEnabled())
		{
			Enable(false);
			Enable(true);
		}
	}
}

int32 UWorldMetricsSubsystem::NumExtensions() const
{
	return Extensions.Num();
}

bool UWorldMetricsSubsystem::HasAnyExtension() const
{
	return !Extensions.IsEmpty();
}

int32 UWorldMetricsSubsystem::NumMetrics() const
{
	return Metrics.Num();
}

bool UWorldMetricsSubsystem::HasAnyMetric() const
{
	return !Metrics.IsEmpty();
}

UWorldMetricInterface* UWorldMetricsSubsystem::CreateMetric(const TSubclassOf<UWorldMetricInterface>& InMetricClass)
{
	if (UNLIKELY(!InMetricClass))
	{
		UE_LOG(LogWorldMetrics, Warning, TEXT("[%hs] Unexpected null metric class"), __FUNCTION__);
		return nullptr;
	}

	if (InMetricClass->HasAnyClassFlags(CLASS_Abstract))
	{
		UE_LOG(
			LogWorldMetrics, Warning, TEXT("[%hs] Parameter metric class is abstract: %s"), __FUNCTION__,
			*InMetricClass->GetFName().ToString());
		return nullptr;
	}

	UWorldMetricInterface* Metric = NewObject<UWorldMetricInterface>(this, InMetricClass, NAME_None, RF_Transient);
	if (UNLIKELY(!Metric))
	{
		UE_LOG(
			LogWorldMetrics, Error, TEXT("[%hs] Failed to create metric of class: %s"), __FUNCTION__,
			*InMetricClass->GetFName().ToString());
	}
	return Metric;
}

bool UWorldMetricsSubsystem::ContainsMetric(UWorldMetricInterface* InMetric) const
{
	if (UNLIKELY(!InMetric))
	{
		UE_LOG(LogWorldMetrics, Warning, TEXT("[%hs] Unexpected null metric instance"), __FUNCTION__);
		return false;
	}
	return Algo::IndexOf(Metrics, InMetric) != INDEX_NONE;
}

UWorldMetricInterface* UWorldMetricsSubsystem::AddMetric(const TSubclassOf<UWorldMetricInterface>& InMetricClass)
{
	UWorldMetricInterface* Metric = CreateMetric(InMetricClass);
	AddMetric(Metric);
	return Metric;
}

bool UWorldMetricsSubsystem::AddMetric(UWorldMetricInterface* InMetric)
{
	if (UNLIKELY(!InMetric))
	{
		UE_LOG(LogWorldMetrics, Warning, TEXT("[%hs] Unexpected null metric instance"), __FUNCTION__);
		return false;
	}

	const int32 MetricIndex = Algo::IndexOf(Metrics, InMetric);
	if (MetricIndex != INDEX_NONE)
	{
		return false;
	}

	Metrics.Emplace(InMetric);
	if (IsEnabled())
	{
		InMetric->Initialize();
	}
	else
	{
		Enable(true);
	}

	UE_LOG(
		LogWorldMetrics, Log, TEXT("[%hs] Added metric of class %s."), __FUNCTION__,
		*InMetric->GetClass()->GetFName().ToString());

	return true;
}

bool UWorldMetricsSubsystem::RemoveMetric(UWorldMetricInterface* InMetric)
{
	if (UNLIKELY(!InMetric))
	{
		UE_LOG(LogWorldMetrics, Warning, TEXT("[%hs] Unexpected null metric instance"), __FUNCTION__);
		return false;
	}

	const int32 MetricIndex = Algo::IndexOf(Metrics, InMetric);
	if (MetricIndex == INDEX_NONE)
	{
		return false;
	}

	if (IsEnabled())
	{
		InMetric->Deinitialize();
		VerifyMetricReleasedAllExtensions(InMetric);
	}
	Metrics.RemoveAt(MetricIndex);
	if (Metrics.IsEmpty())
	{
		VerifyRemoveOrphanExtensions();
		Enable(false);
	}

	UE_LOG(
		LogWorldMetrics, Log, TEXT("[%hs] Removed metric of class %s"), __FUNCTION__,
		*InMetric->GetClass()->GetFName().ToString());

	return true;
}

void UWorldMetricsSubsystem::RemoveAllMetrics()
{
	for (int32 MetricIndex = 0; MetricIndex < Metrics.Num(); ++MetricIndex)
	{
		UWorldMetricInterface* Metric = Metrics[MetricIndex];
		if (IsEnabled())
		{
			Metric->Deinitialize();
			VerifyMetricReleasedAllExtensions(Metric);
			Metric->MarkAsGarbage();
		}
	}
	Metrics.Reset();
	VerifyRemoveOrphanExtensions();
	Enable(false);
}

void UWorldMetricsSubsystem::ForEachMetric(const TFunctionRef<bool(const UWorldMetricInterface*)>& Func) const
{
	for (const UWorldMetricInterface* Metric : Metrics)
	{
		if (!Func(Metric))
		{
			break;
		}
	}
}

void UWorldMetricsSubsystem::OnUpdate(float DeltaTimeInSeconds)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldMetricsSubsystem::OnUpdate);

	if (PendingWarmUpFrames > 0)
	{
		--PendingWarmUpFrames;
		return;
	}

	for (UWorldMetricInterface* Metric : Metrics)
	{
		check(Metric);
		Metric->Update(DeltaTimeInSeconds);
	}
}

UWorldMetricsExtension* UWorldMetricsSubsystem::AcquireExtension(
	UWorldMetricInterface* InMetricOwner,
	const TSubclassOf<UWorldMetricsExtension>& InExtensionClass)
{
	return AcquireExtensionInternal(InMetricOwner, InExtensionClass);
}

UWorldMetricsExtension* UWorldMetricsSubsystem::AcquireExtension(
	UWorldMetricsExtension* InExtensionOwner,
	const TSubclassOf<UWorldMetricsExtension>& InExtensionClass)
{
	return AcquireExtensionInternal(InExtensionOwner, InExtensionClass);
}

bool UWorldMetricsSubsystem::ReleaseExtension(
	UWorldMetricInterface* InMetricOwner,
	const TSubclassOf<UWorldMetricsExtension>& InExtensionClass)
{
	return ReleaseExtensionInternal(InMetricOwner, InExtensionClass);
}

bool UWorldMetricsSubsystem::ReleaseExtension(
	UWorldMetricsExtension* InExtensionOwner,
	const TSubclassOf<UWorldMetricsExtension>& InExtensionClass)
{
	return ReleaseExtensionInternal(InExtensionOwner, InExtensionClass);
}

int32 UWorldMetricsSubsystem::GetExtensionIndex(const TSubclassOf<UWorldMetricsExtension>& InExtensionClass) const
{
	return Algo::IndexOfByPredicate(
		Extensions,
		[InExtensionClass](UWorldMetricsExtension* Extension) { return Extension->GetClass() == InExtensionClass; });
}

UWorldMetricsExtension* UWorldMetricsSubsystem::AcquireExtensionInternal(
	UObject* InOwner,
	const TSubclassOf<UWorldMetricsExtension>& InExtensionClass)
{
	if (UNLIKELY(!InOwner))
	{
		UE_LOG(LogWorldMetrics, Warning, TEXT("[%hs] Unexpected invalid owner"), __FUNCTION__);
		return nullptr;
	}

	if (UNLIKELY(!InExtensionClass))
	{
		UE_LOG(LogWorldMetrics, Warning, TEXT("[%hs] Unexpected null extension class"), __FUNCTION__);
		return nullptr;
	}

	if (InExtensionClass->HasAnyClassFlags(CLASS_Abstract))
	{
		UE_LOG(
			LogWorldMetrics, Warning, TEXT("[%hs] Parameter extension class is abstract: %s"), __FUNCTION__,
			*InExtensionClass->GetFName().ToString());
		return nullptr;
	}

	UWorldMetricsExtension* Extension = AcquireExistingExtension(InOwner, InExtensionClass);
	if (!Extension)
	{
		Extension = AddExtension(InOwner, InExtensionClass);
	}

	if (LIKELY(Extension))
	{
		Extension->OnAcquire(InOwner);
	}
	return Extension;
}

UWorldMetricsExtension* UWorldMetricsSubsystem::AcquireExistingExtension(
	UObject* InOwner,
	const TSubclassOf<UWorldMetricsExtension>& InExtensionClass)
{
	const int32 ExtensionIndex = GetExtensionIndex(InExtensionClass);
	if (ExtensionIndex == INDEX_NONE)
	{
		return nullptr;
	}

	if (!ensure(IndexedOwners.IsValidIndex(ExtensionIndex)))
	{
		UE_LOG(LogWorldMetrics, Error, TEXT("[%hs] Unexpected invalid extension's owner list"), __FUNCTION__);
		return nullptr;
	}
	IndexedOwners[ExtensionIndex].Emplace(InOwner);

	return Extensions[ExtensionIndex];
}

UWorldMetricsExtension* UWorldMetricsSubsystem::AddExtension(
	UObject* InOwner,
	const TSubclassOf<UWorldMetricsExtension>& InExtensionClass)
{
	check(Extensions.Num() == IndexedOwners.Num());
	UWorldMetricsExtension* Extension =
		NewObject<UWorldMetricsExtension>(this, InExtensionClass, NAME_None, RF_Transient);	
	if (UNLIKELY(!Extension))
	{
		UE_LOG(
			LogWorldMetrics, Error, TEXT("[%hs] Failed to create extension of class: %s"), __FUNCTION__,
			*InExtensionClass->GetFName().ToString());
		return nullptr;
	}

	Extensions.Emplace(Extension);
	IndexedOwners.Emplace_GetRef().Emplace(InOwner);
	Extension->Initialize();

	UE_LOG(
		LogWorldMetrics, Log, TEXT("[%hs] Added extension of class: %s"), __FUNCTION__,
		*InExtensionClass->GetFName().ToString());

	return Extension;
}

bool UWorldMetricsSubsystem::ReleaseExtensionInternal(
	UObject* InOwner,
	const TSubclassOf<UWorldMetricsExtension>& InExtensionClass)
{
	if (UNLIKELY(!InOwner))
	{
		return false;
	}

	if (UNLIKELY(!InExtensionClass))
	{
		UE_LOG(LogWorldMetrics, Warning, TEXT("[%hs] Unexpected null extension class"), __FUNCTION__);
		return false;
	}

	const int32 ExtensionIndex = GetExtensionIndex(InExtensionClass);
	if (ExtensionIndex == INDEX_NONE)
	{
		return false;
	}

	if (UNLIKELY(!IndexedOwners[ExtensionIndex].Remove(InOwner)))
	{
		UE_LOG(
			LogWorldMetrics, Error, TEXT("[%hs] Parameter object doesn't own this extension: %s"), __FUNCTION__,
			*InExtensionClass->GetFName().ToString());
	}
	Extensions[ExtensionIndex]->OnRelease(InOwner);

	TryRemoveExtensionAt(ExtensionIndex);

	return true;
}

bool UWorldMetricsSubsystem::TryRemoveExtensionAt(int32 ExtensionIndex)
{
	if (IndexedOwners[ExtensionIndex].IsEmpty())
	{
		UWorldMetricsExtension* Extension = Extensions[ExtensionIndex];
		Extension->Deinitialize();
		Extension->MarkAsGarbage();

		Extensions.RemoveAtSwap(ExtensionIndex);
		IndexedOwners.RemoveAtSwap(ExtensionIndex);
		UE_LOG(
			LogWorldMetrics, Log, TEXT("[%hs] Removed extension of class %s"), __FUNCTION__,
			*Extension->GetClass()->GetFName().ToString());

		return true;
	}
	return false;
}

void UWorldMetricsSubsystem::VerifyMetricReleasedAllExtensions(UWorldMetricInterface* InMetric)
{
	check(Extensions.Num() == IndexedOwners.Num());

	TArray<int32, TInlineAllocator<DefaultExtensionCapacity>> StaleExtensionIndices;
	for (int32 ExtensionIndex = 0; ExtensionIndex < Extensions.Num(); ++ExtensionIndex)
	{
		if (UNLIKELY(IndexedOwners[ExtensionIndex].Remove(InMetric)))
		{
			StaleExtensionIndices.Emplace(ExtensionIndex);
			UWorldMetricsExtension* Extension = Extensions[ExtensionIndex];
			UE_LOG(
				LogWorldMetrics, Warning, TEXT("[%hs] World metric %s did not release extension %s in Deinitialize"),
				__FUNCTION__, *InMetric->GetFName().ToString(), *Extension->GetName());
		}
	}

	// Reverse iterate stale extension indices in case the extension can be removed
	for (int32 i = StaleExtensionIndices.Num(); i-- > 0;)
	{
		TryRemoveExtensionAt(StaleExtensionIndices[i]);
	}
}

void UWorldMetricsSubsystem::VerifyRemoveOrphanExtensions()
{
	check(Extensions.Num() == IndexedOwners.Num());

	if (Metrics.IsEmpty())
	{
		if (UNLIKELY(!Extensions.IsEmpty()))
		{
			for (UWorldMetricsExtension* Extension : Extensions)
			{
				check(Extension);

				UE_LOG(
					LogWorldMetrics, Warning,
					TEXT("[%hs] World extension %s was acquired by another extension but never released."),
					__FUNCTION__, *Extension->GetName());

				Extension->Deinitialize();
				Extension->MarkAsGarbage();
			}
			Extensions.Reset();
			IndexedOwners.Reset();
		}
	}
}
