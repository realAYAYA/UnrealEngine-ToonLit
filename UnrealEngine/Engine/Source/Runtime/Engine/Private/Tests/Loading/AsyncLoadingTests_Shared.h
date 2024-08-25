// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"
#include "UObject/ObjectMacros.h"
#include "AsyncLoadingTests_Shared.generated.h"

UCLASS()
class UAsyncLoadingTests_Shared : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TSoftObjectPtr<UObject> SoftReference;

	UPROPERTY()
	TObjectPtr<UObject> HardReference;

	// Those delegates allow to easily change behavior between tests
	using FOnPostLoadDelegate = TDelegate<void(UAsyncLoadingTests_Shared*), FDefaultTSDelegateUserPolicy>;
	static FOnPostLoadDelegate OnPostLoad;

	using FOnSerializeDelegate = TDelegate<void(FArchive& Ar, UAsyncLoadingTests_Shared*), FDefaultTSDelegateUserPolicy>;
	static FOnSerializeDelegate OnSerialize;

	using FOnIsReadyForAsyncPostLoadDelegate = TDelegate<bool(const UAsyncLoadingTests_Shared*), FDefaultTSDelegateUserPolicy>;
	static FOnIsReadyForAsyncPostLoadDelegate OnIsReadyForAsyncPostLoad;

	using FOnIsPostLoadThreadSafeDelegate = TDelegate<bool(const UAsyncLoadingTests_Shared*), FDefaultTSDelegateUserPolicy>;
	static FOnIsReadyForAsyncPostLoadDelegate OnIsPostLoadThreadSafe;

	virtual void PostLoad() override
	{
		Super::PostLoad();

		OnPostLoad.ExecuteIfBound(this);
	}

	virtual void Serialize(FArchive& Ar) override
	{
		Super::Serialize(Ar);

		OnSerialize.ExecuteIfBound(Ar, this);
	}

	virtual bool IsReadyForAsyncPostLoad() const override
	{
		if (OnIsReadyForAsyncPostLoad.IsBound())
		{
			return OnIsReadyForAsyncPostLoad.Execute(this);
		}

		return Super::IsReadyForAsyncPostLoad();
	}

	virtual bool IsPostLoadThreadSafe() const override
	{
		if (OnIsPostLoadThreadSafe.IsBound())
		{
			return OnIsPostLoadThreadSafe.Execute(this);
		}

		return Super::IsPostLoadThreadSafe();
	}
};

#if WITH_DEV_AUTOMATION_TESTS

class FLoadingTestsScope
{
private:
	FAutomationTestBase& AutomationTest;

	TArray<FString>     PackageNames;
	std::atomic<uint32> PackageIndex;
public:
	static constexpr const TCHAR* ObjectName = TEXT("TestObject");
	static constexpr const TCHAR* PackagePath1 = TEXT("/Engine/LoadingTestsScope_Package1");
	static constexpr const TCHAR* ObjectPath1 = TEXT("/Engine/LoadingTestsScope_Package1.TestObject");
	static constexpr const TCHAR* PackagePath2 = TEXT("/Engine/LoadingTestsScope_Package2");
	static constexpr const TCHAR* ObjectPath2 = TEXT("/Engine/LoadingTestsScope_Package2.TestObject");

	UPackage* Package1 = nullptr;
	UPackage* Package2 = nullptr;

	UAsyncLoadingTests_Shared* Object1 = nullptr;
	UAsyncLoadingTests_Shared* Object2 = nullptr;

	UPackage* CreatePackage()
	{
		FString PackageName = FString::Printf(TEXT("/Engine/LoadingTestsScope_Package%u"), ++PackageIndex);
		UPackage* Package = ::CreatePackage(*PackageName);
		PackageNames.Add(PackageName);
		return Package;
	}

	void CreateObjects();
	void DefaultMutateObjects();
	void SavePackages();
	void LoadObjects();
	void CleanupObjects();
	void GarbageCollect();

	FLoadingTestsScope(FAutomationTestBase* InAutomationTest, TFunction<void (FLoadingTestsScope&)> InMutateObjects = nullptr)
		: AutomationTest(*InAutomationTest)
	{
		// Just make sure the async loading queue is empty before beginning.
		FlushAsyncLoading();

		CreateObjects();

		if (InMutateObjects)
		{
			InMutateObjects(*this);
		}
		else
		{
			DefaultMutateObjects();
		}

		SavePackages();

		GarbageCollect();
	}

	virtual ~FLoadingTestsScope()
	{
		// Just to be sure in case the test fails, don't leave some pending loads behind
		FlushAsyncLoading();

		CleanupObjects();
	}
};

class FLoadingTests_ZenLoaderOnly_Base : public FAutomationTestBase
{
public:
	using FAutomationTestBase::FAutomationTestBase;
protected:
	bool CanRunInEnvironment(const FString& TestParams, FString* OutReason, bool* OutWarn) const override
	{
		ELoaderType LoaderType = GetLoaderType();
		if (LoaderType != ELoaderType::ZenLoader)
		{
			if (OutReason)
			{
				*OutReason = FString::Printf(TEXT("Test %s is for ZenLoader only. Cannot run on non-compliant loader currently active: %s"), *GetTestName(), LexToString(LoaderType));
			}
			return false;
		}

		return true;
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
