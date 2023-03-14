// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Delegates/Delegate.h"
#include "Tasks/Task.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UE { namespace TSDelegatesTest 
{
	using namespace Tasks;

	void Func() {}

	void OneParamFunc(bool* bExecuted)
	{
		*bExecuted = true;
	}

	void AnotherOneParamFunc(bool* bExecuted)
	{
		*bExecuted = true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTSMulticastDelegateBasicTest, "System.Core.Delegates.MulticastBasic", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter);

	bool FTSMulticastDelegateBasicTest::RunTest(const FString& Parameters)
	{
		DECLARE_TS_MULTICAST_DELEGATE(FTestDelegate);

		{	// single thread
			FTestDelegate TestDelegate;
			bool bExecuted = false;
			FDelegateHandle DelegateHandle = TestDelegate.AddStatic(&OneParamFunc, &bExecuted);
			TestDelegate.AddStatic(&AnotherOneParamFunc, &bExecuted);
			TestDelegate.Broadcast();
			check(bExecuted);
			TestDelegate.Remove(DelegateHandle);
		}

		{	// concurrent Add/Remove and Execute
			FTestDelegate TestDelegate;
			bool bExecuted = false;
			FTask AddStaticTask = Launch(UE_SOURCE_LOCATION, [&TestDelegate, &bExecuted] { TestDelegate.AddStatic(&AnotherOneParamFunc, &bExecuted); });
			FTask BroadcastTask = Launch(UE_SOURCE_LOCATION, [&TestDelegate] { TestDelegate.Broadcast(); });
			FDelegateHandle DelegateHandle = TestDelegate.AddStatic(&OneParamFunc, &bExecuted);
			TestDelegate.Remove(DelegateHandle);
			Wait(TArray{ AddStaticTask, BroadcastTask });
		}

		{ // same as above, but Add/Remove from different threads (not concurrently)
			FTestDelegate TestDelegate;
			bool bExecuted = false;
			TTask<FDelegateHandle> AddStaticTask = Launch(UE_SOURCE_LOCATION, [&TestDelegate, &bExecuted] { return TestDelegate.AddStatic(&OneParamFunc, &bExecuted); });
			FTask RemoveTask = Launch(UE_SOURCE_LOCATION, [&TestDelegate, &AddStaticTask] { TestDelegate.Remove(AddStaticTask.GetResult()); }, AddStaticTask);
			TestDelegate.Broadcast();
			RemoveTask.Wait();
			TestDelegate.Remove(AddStaticTask.GetResult());
		}

		{	// recursive locking: removing a delegate handle from inside broadcasting
			FTestDelegate TestDelegate;
			FDelegateHandle DelegateHandle;
			auto DelegateLambda = [&TestDelegate, &DelegateHandle] { TestDelegate.Remove(DelegateHandle); };
			DelegateHandle = TestDelegate.AddLambda(MoveTemp(DelegateLambda));
			check(TestDelegate.IsBound());
			TestDelegate.Broadcast();
			check(!TestDelegate.IsBound());
		}

		// test that all macros compile
		{
			DECLARE_TS_MULTICAST_DELEGATE_OneParam(FDelegate, int);
			FDelegate Delegate;
			Delegate.AddLambda([](int) {});
			Delegate.Broadcast(0);
		}
		{
			DECLARE_TS_MULTICAST_DELEGATE_TwoParams(FDelegate, int, int);
			FDelegate Delegate;
			Delegate.AddLambda([](int, int) {});
			Delegate.Broadcast(0, 0);
		}
		{
			DECLARE_TS_MULTICAST_DELEGATE_ThreeParams(FDelegate, int, int, int);
			FDelegate Delegate;
			Delegate.AddLambda([](int, int, int) {});
			Delegate.Broadcast(0, 0, 0);
		}
		{
			DECLARE_TS_MULTICAST_DELEGATE_FourParams(FDelegate, int, int, int, int);
			FDelegate Delegate;
			Delegate.AddLambda([](int, int, int, int) {});
			Delegate.Broadcast(0, 0, 0, 0);
		}
		{
			DECLARE_TS_MULTICAST_DELEGATE_FiveParams(FDelegate, int, int, int, int, int);
			FDelegate Delegate;
			Delegate.AddLambda([](int, int, int, int, int) {});
			Delegate.Broadcast(0, 0, 0, 0, 0);
		}
		{
			DECLARE_TS_MULTICAST_DELEGATE_SixParams(FDelegate, int, int, int, int, int, int);
			FDelegate Delegate;
			Delegate.AddLambda([](int, int, int, int, int, int) {});
			Delegate.Broadcast(0, 0, 0, 0, 0, 0);
		}
		{
			DECLARE_TS_MULTICAST_DELEGATE_SevenParams(FDelegate, int, int, int, int, int, int, int);
			FDelegate Delegate;
			Delegate.AddLambda([](int, int, int, int, int, int, int) {});
			Delegate.Broadcast(0, 0, 0, 0, 0, 0, 0);
		}
		{
			DECLARE_TS_MULTICAST_DELEGATE_EightParams(FDelegate, int, int, int, int, int, int, int, int);
			FDelegate Delegate;
			Delegate.AddLambda([](int, int, int, int, int, int, int, int) {});
			Delegate.Broadcast(0, 0, 0, 0, 0, 0, 0, 0);
		}
		{
			DECLARE_TS_MULTICAST_DELEGATE_NineParams(FDelegate, int, int, int, int, int, int, int, int, int);
			FDelegate Delegate;
			Delegate.AddLambda([](int, int, int, int, int, int, int, int, int) {});
			Delegate.Broadcast(0, 0, 0, 0, 0, 0, 0, 0, 0);
		}

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTSMulticastDelegateStressTest, "System.Core.Delegates.MulticastStress", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter);

	bool FTSMulticastDelegateStressTest::RunTest(const FString& Parameters)
	{
		DECLARE_TS_MULTICAST_DELEGATE(FTestDelegate);

		{	// one thread repeatedly adds/removes delegates, another broadcasts
			FTestDelegate Delegate;
			std::atomic<bool> bQuit{ false };

			FTask Binding = Launch(UE_SOURCE_LOCATION,
				[&bQuit, &Delegate]
				{
					while (!bQuit)
					{
						FDelegateHandle Handle = Delegate.AddStatic(&Func);
						Delegate.Remove(Handle);
					}
				}
			);

			FTask Executing = Launch(UE_SOURCE_LOCATION,
				[&bQuit, &Delegate]
				{
					while (!bQuit)
					{
						Delegate.Broadcast();
					}
				}
			);

			FPlatformProcess::Sleep(1.0);
			bQuit = true;
			Wait(TArray<FTask>{ Binding, Executing });
		}

		return true;
	}
}}

#endif