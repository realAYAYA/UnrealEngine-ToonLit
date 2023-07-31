// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerivedDataBuildFunctionRegistry.h"

#include "Containers/Map.h"
#include "DerivedDataBuildFunction.h"
#include "DerivedDataBuildFunctionFactory.h"
#include "DerivedDataBuildPrivate.h"
#include "DerivedDataSharedString.h"
#include "Features/IModularFeatures.h"
#include "HAL/CriticalSection.h"
#include "Misc/AsciiSet.h"
#include "Misc/Guid.h"
#include "Misc/ScopeRWLock.h"
#include "Misc/StringBuilder.h"

namespace UE::DerivedData::Private
{

class FBuildFunctionRegistry : public IBuildFunctionRegistry
{
public:
	FBuildFunctionRegistry();
	~FBuildFunctionRegistry();

	const IBuildFunction* FindFunction(FUtf8StringView Function) const;
	FGuid FindFunctionVersion(FUtf8StringView Function) const;
	void IterateFunctionVersions(TFunctionRef<void(FUtf8StringView Function, const FGuid& Version)> Visitor) const;

private:
	void OnModularFeatureRegistered(const FName& Type, IModularFeature* ModularFeature);
	void OnModularFeatureUnregistered(const FName& Type, IModularFeature* ModularFeature);

	void AddFunctionNoLock(IBuildFunctionFactory* Factory);
	void RemoveFunction(IBuildFunctionFactory* Factory);

private:
	mutable FRWLock Lock;
	TMap<FUtf8SharedString, IBuildFunctionFactory*> Functions;
};

FBuildFunctionRegistry::FBuildFunctionRegistry()
{
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	for (IBuildFunctionFactory* Factory : ModularFeatures.GetModularFeatureImplementations<IBuildFunctionFactory>(IBuildFunctionFactory::FeatureName))
	{
		AddFunctionNoLock(Factory);
	}
	ModularFeatures.OnModularFeatureRegistered().AddRaw(this, &FBuildFunctionRegistry::OnModularFeatureRegistered);
	ModularFeatures.OnModularFeatureUnregistered().AddRaw(this, &FBuildFunctionRegistry::OnModularFeatureUnregistered);
}

FBuildFunctionRegistry::~FBuildFunctionRegistry()
{
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	ModularFeatures.OnModularFeatureUnregistered().RemoveAll(this);
	ModularFeatures.OnModularFeatureRegistered().RemoveAll(this);
}

void FBuildFunctionRegistry::OnModularFeatureRegistered(const FName& Type, IModularFeature* ModularFeature)
{
	if (Type == IBuildFunctionFactory::FeatureName)
	{
		FWriteScopeLock WriteLock(Lock);
		AddFunctionNoLock(static_cast<IBuildFunctionFactory*>(ModularFeature));
	}
}

void FBuildFunctionRegistry::OnModularFeatureUnregistered(const FName& Type, IModularFeature* ModularFeature)
{
	if (Type == IBuildFunctionFactory::FeatureName)
	{
		RemoveFunction(static_cast<IBuildFunctionFactory*>(ModularFeature));
	}
}

void FBuildFunctionRegistry::AddFunctionNoLock(IBuildFunctionFactory* Factory)
{
	const IBuildFunction& Function = Factory->GetFunction();
	const FUtf8SharedString& FunctionName = Function.GetName();
	const uint32 FunctionHash = GetTypeHash(FunctionName);
	UE_CLOG(!Function.GetVersion().IsValid(), LogDerivedDataBuild, Error,
		TEXT("Version of zero is not allowed in build function with the name %s."), *WriteToString<32>(FunctionName));
	UE_CLOG(Functions.FindByHash(FunctionHash, FunctionName), LogDerivedDataBuild, Error,
		TEXT("More than one build function has been registered with the name %s."), *WriteToString<32>(FunctionName));
	Functions.EmplaceByHash(FunctionHash, FunctionName, Factory);
}

void FBuildFunctionRegistry::RemoveFunction(IBuildFunctionFactory* Factory)
{
	const FUtf8SharedString& Function = Factory->GetFunction().GetName();
	const uint32 FunctionHash = GetTypeHash(Function);
	FWriteScopeLock WriteLock(Lock);
	Functions.RemoveByHash(FunctionHash, Function);
}

const IBuildFunction* FBuildFunctionRegistry::FindFunction(FUtf8StringView Function) const
{
	const uint32 FunctionHash = GetTypeHash(Function);
	FReadScopeLock ReadLock(Lock);
	if (IBuildFunctionFactory* const* Factory = Functions.FindByHash(FunctionHash, Function))
	{
		return &(**Factory).GetFunction();
	}
	return nullptr;
}

FGuid FBuildFunctionRegistry::FindFunctionVersion(FUtf8StringView Function) const
{
	const uint32 FunctionHash = GetTypeHash(Function);
	FReadScopeLock ReadLock(Lock);
	if (IBuildFunctionFactory* const* Factory = Functions.FindByHash(FunctionHash, Function))
	{
		return (**Factory).GetFunction().GetVersion();
	}
	return FGuid();
}

void FBuildFunctionRegistry::IterateFunctionVersions(TFunctionRef<void(FUtf8StringView Function, const FGuid& Version)> Visitor) const
{
	TArray<TPair<FUtf8SharedString, FGuid>> FunctionVersions;
	{
		FReadScopeLock ReadLock(Lock);
		FunctionVersions.Reserve(Functions.Num());
		for (const TPair<FUtf8SharedString, IBuildFunctionFactory*>& Function : Functions)
		{
			FunctionVersions.Emplace(Function.Key, Function.Value->GetFunction().GetVersion());
		}
	}

	for (const TPair<FUtf8SharedString, FGuid>& Function : FunctionVersions)
	{
		Visitor(Function.Key, Function.Value);
	}
}

IBuildFunctionRegistry* CreateBuildFunctionRegistry()
{
	return new FBuildFunctionRegistry();
}

bool IsValidBuildFunctionName(FUtf8StringView Function)
{
	constexpr FAsciiSet Valid("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
	return !Function.IsEmpty() && FAsciiSet::HasOnly(Function, Valid);
}

void AssertValidBuildFunctionName(FUtf8StringView Function, FStringView Name)
{
	checkf(IsValidBuildFunctionName(Function),
		TEXT("A build function name must be alphanumeric and non-empty for build of '%.*s' by '%.*s'."),
		Name.Len(), Name.GetData(), *WriteToString<32>(Function));
}

} // UE::DerivedData::Private
