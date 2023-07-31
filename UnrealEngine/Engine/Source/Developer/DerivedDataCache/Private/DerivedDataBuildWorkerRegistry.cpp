// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerivedDataBuildWorkerRegistry.h"

#include "Algo/Find.h"
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "DerivedDataBuildKey.h"
#include "DerivedDataBuildPrivate.h"
#include "DerivedDataBuildWorker.h"
#include "DerivedDataSharedString.h"
#include "Features/IModularFeatures.h"
#include "HAL/CriticalSection.h"
#include "IO/IoHash.h"
#include "Misc/Guid.h"
#include "Misc/ScopeRWLock.h"
#include "Templates/Function.h"
#include "Templates/Tuple.h"

namespace UE::DerivedData::Private
{

class FBuildWorkerInternal final : public FBuildWorker, public FBuildWorkerBuilder
{
public:
	inline explicit FBuildWorkerInternal(IBuildWorkerFactory* InFactory)
		: Factory(InFactory)
	{
	}

	void Build();

	inline FStringView GetName() const final { return WorkerName; }
	inline FStringView GetPath() const final { return WorkerPath; }
	inline FStringView GetHostPlatform() const final { return HostPlatform; }
	inline FGuid GetBuildSystemVersion() const final { return BuildSystemVersion; }

	void FindFileData(TConstArrayView<FIoHash> RawHashes, IRequestOwner& Owner, FOnBuildWorkerFileDataComplete&& OnComplete) const final;

	void IterateFunctions(TFunctionRef<void (FUtf8StringView Name, const FGuid& Version)> Visitor) const final;
	void IterateFiles(TFunctionRef<void (FStringView Path, const FIoHash& RawHash, uint64 RawSize)> Visitor) const final;
	void IterateExecutables(TFunctionRef<void (FStringView Path, const FIoHash& RawHash, uint64 RawSize)> Visitor) const final;
	void IterateEnvironment(TFunctionRef<void (FStringView Name, FStringView Value)> Visitor) const final;

private:
	inline void SetName(FStringView Name) final { WorkerName = Name; }
	inline void SetPath(FStringView Path) final { WorkerPath = Path; }
	inline void SetHostPlatform(FStringView Name) final { HostPlatform = Name; }
	inline void SetBuildSystemVersion(const FGuid& Version) final { BuildSystemVersion = Version; }

	void AddFunction(FUtf8StringView Name, const FGuid& Version) final;
	void AddFile(FStringView Path, const FIoHash& RawHash, uint64 RawSize) final;
	void AddExecutable(FStringView Path, const FIoHash& RawHash, uint64 RawSize) final;
	void SetEnvironment(FStringView Name, FStringView Value) final;

private:
	FString WorkerName;
	FString WorkerPath;
	FString HostPlatform;
	FGuid BuildSystemVersion;
	TArray<TTuple<FUtf8SharedString, FGuid>> Functions;
	TArray<TTuple<FString, FIoHash, uint64>> Files;
	TArray<TTuple<FString, FIoHash, uint64>> Executables;
	TArray<TTuple<FString, FString>> Environment;
	IBuildWorkerFactory* Factory;
};

void FBuildWorkerInternal::Build()
{
	Functions.Sort();
	Files.Sort();
	Executables.Sort();
	Environment.Sort();
}

void FBuildWorkerInternal::FindFileData(TConstArrayView<FIoHash> RawHashes, IRequestOwner& Owner, FOnBuildWorkerFileDataComplete&& OnComplete) const
{
	return Factory->FindFileData(RawHashes, Owner, MoveTemp(OnComplete));
}

void FBuildWorkerInternal::IterateFunctions(TFunctionRef<void (FUtf8StringView Name, const FGuid& Version)> Visitor) const
{
	for (const TTuple<FUtf8SharedString, FGuid>& Function : Functions)
	{
		Function.ApplyAfter(Visitor);
	}
}

void FBuildWorkerInternal::IterateFiles(TFunctionRef<void (FStringView Path, const FIoHash& RawHash, uint64 RawSize)> Visitor) const
{
	for (const TTuple<FString, FIoHash, uint64>& File : Files)
	{
		File.ApplyAfter(Visitor);
	}
}

void FBuildWorkerInternal::IterateExecutables(TFunctionRef<void (FStringView Path, const FIoHash& RawHash, uint64 RawSize)> Visitor) const
{
	for (const TTuple<FString, FIoHash, uint64>& Executable : Executables)
	{
		Executable.ApplyAfter(Visitor);
	}
}

void FBuildWorkerInternal::IterateEnvironment(TFunctionRef<void (FStringView Name, FStringView Value)> Visitor) const
{
	for (const TTuple<FString, FString>& Variable : Environment)
	{
		Variable.ApplyAfter(Visitor);
	}
}

void FBuildWorkerInternal::AddFunction(FUtf8StringView Name, const FGuid& Version)
{
	UE_CLOG(!Version.IsValid(), LogDerivedDataBuild, Error,
		TEXT("Version of zero is not allowed in build function with the name %s in build worker '%s'."),
		*WriteToString<32>(Name), *WorkerName);
	Functions.Emplace(Name, Version);
}

void FBuildWorkerInternal::AddFile(FStringView Path, const FIoHash& RawHash, uint64 RawSize)
{
	Files.Emplace(Path, RawHash, RawSize);
}

void FBuildWorkerInternal::AddExecutable(FStringView Path, const FIoHash& RawHash, uint64 RawSize)
{
	Executables.Emplace(Path, RawHash, RawSize);
}

void FBuildWorkerInternal::SetEnvironment(FStringView Name, FStringView Value)
{
	Environment.Emplace(Name, Value);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FBuildWorkerRegistry final : public IBuildWorkerRegistry
{
public:
	FBuildWorkerRegistry();
	~FBuildWorkerRegistry();

	FBuildWorker* FindWorker(
		const FUtf8SharedString& Function,
		const FGuid& FunctionVersion,
		const FGuid& BuildSystemVersion,
		IBuildWorkerExecutor*& OutWorkerExecutor) const final;

private:
	void OnModularFeatureRegistered(const FName& Type, IModularFeature* ModularFeature);
	void OnModularFeatureUnregistered(const FName& Type, IModularFeature* ModularFeature);

	void AddWorker(IBuildWorkerFactory* Factory);
	void RemoveWorker(IBuildWorkerFactory* Factory);

private:
	mutable FRWLock Lock;
	IBuildWorkerExecutor* Executor = nullptr;
	TMap<IBuildWorkerFactory*, TUniquePtr<FBuildWorker>> Workers;
	TMultiMap<TTuple<FUtf8SharedString, FGuid>, FBuildWorker*> Functions;
};

FBuildWorkerRegistry::FBuildWorkerRegistry()
{
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	if (ModularFeatures.IsModularFeatureAvailable(IBuildWorkerExecutor::FeatureName))
	{
		Executor = &ModularFeatures.GetModularFeature<IBuildWorkerExecutor>(IBuildWorkerExecutor::FeatureName);
	}
	for (IBuildWorkerFactory* Worker : ModularFeatures.GetModularFeatureImplementations<IBuildWorkerFactory>(IBuildWorkerFactory::FeatureName))
	{
		AddWorker(Worker);
	}
	ModularFeatures.OnModularFeatureRegistered().AddRaw(this, &FBuildWorkerRegistry::OnModularFeatureRegistered);
	ModularFeatures.OnModularFeatureUnregistered().AddRaw(this, &FBuildWorkerRegistry::OnModularFeatureUnregistered);
}

FBuildWorkerRegistry::~FBuildWorkerRegistry()
{
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	ModularFeatures.OnModularFeatureUnregistered().RemoveAll(this);
	ModularFeatures.OnModularFeatureRegistered().RemoveAll(this);
}

void FBuildWorkerRegistry::OnModularFeatureRegistered(const FName& Type, IModularFeature* ModularFeature)
{
	if (!Executor && Type == IBuildWorkerExecutor::FeatureName)
	{
		FWriteScopeLock WriteLock(Lock);
		Executor = static_cast<IBuildWorkerExecutor*>(ModularFeature);
	}
	else if (Type == IBuildWorkerFactory::FeatureName)
	{
		AddWorker(static_cast<IBuildWorkerFactory*>(ModularFeature));
	}
}

void FBuildWorkerRegistry::OnModularFeatureUnregistered(const FName& Type, IModularFeature* ModularFeature)
{
	if (Executor == ModularFeature && Type == IBuildWorkerExecutor::FeatureName)
	{
		IModularFeature* NextExecutor = nullptr;
		IModularFeatures& ModularFeatures = IModularFeatures::Get();
		if (ModularFeatures.IsModularFeatureAvailable(IBuildWorkerExecutor::FeatureName))
		{
			NextExecutor = &ModularFeatures.GetModularFeature<IBuildWorkerExecutor>(IBuildWorkerExecutor::FeatureName);
		}
		FWriteScopeLock WriteLock(Lock);
		Executor = static_cast<IBuildWorkerExecutor*>(NextExecutor);
	}
	else if (Type == IBuildWorkerFactory::FeatureName)
	{
		RemoveWorker(static_cast<IBuildWorkerFactory*>(ModularFeature));
	}
}

void FBuildWorkerRegistry::AddWorker(IBuildWorkerFactory* Factory)
{
	TUniquePtr<FBuildWorkerInternal> Worker = MakeUnique<FBuildWorkerInternal>(Factory);
	Factory->Build(*Worker);
	Worker->Build();

	FWriteScopeLock WriteLock(Lock);
	Worker->IterateFunctions([this, Worker = Worker.Get()](FUtf8StringView Name, const FGuid& Version)
	{
		Functions.Emplace(MakeTuple(FUtf8SharedString(Name), Version), Worker);
	});
	Workers.Emplace(Factory, MoveTemp(Worker));
}

void FBuildWorkerRegistry::RemoveWorker(IBuildWorkerFactory* Factory)
{
	FWriteScopeLock WriteLock(Lock);
	TUniquePtr<FBuildWorker>& Worker = Workers.FindChecked(Factory);
	Worker->IterateFunctions([this, Worker = Worker.Get()](FUtf8StringView Name, const FGuid& Version)
	{
		Functions.Remove(MakeTuple(FUtf8SharedString(Name), Version), Worker);
	});
	Workers.Remove(Factory);
}

FBuildWorker* FBuildWorkerRegistry::FindWorker(
	const FUtf8SharedString& Function,
	const FGuid& FunctionVersion,
	const FGuid& BuildSystemVersion,
	IBuildWorkerExecutor*& OutWorkerExecutor) const
{
	FReadScopeLock ReadLock(Lock);
	if (Executor)
	{
		TConstArrayView<FStringView> ExecutorHostPlatforms = Executor->GetHostPlatforms();
		TArray<FBuildWorker*, TInlineAllocator<8>> FunctionWorkers;
		Functions.MultiFind(MakeTuple(Function, FunctionVersion), FunctionWorkers);
		for (FBuildWorker* Worker : FunctionWorkers)
		{
			if (Worker->GetBuildSystemVersion() == BuildSystemVersion &&
				Algo::Find(ExecutorHostPlatforms, Worker->GetHostPlatform()))
			{
				OutWorkerExecutor = Executor;
				return Worker;
			}
		}
	}
	OutWorkerExecutor = nullptr;
	return nullptr;
}

IBuildWorkerRegistry* CreateBuildWorkerRegistry()
{
	return new FBuildWorkerRegistry();
}

} // UE::DerivedData::Private
