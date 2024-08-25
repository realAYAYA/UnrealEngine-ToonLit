// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Online/CoreOnline.h"
#include "Online/OnlineServicesCommon.h"
#include "Online/OnlineServicesRegistry.h"
#include "Online/OnlineServices.h"
#include <catch2/catch_test_macros.hpp>

#ifndef UE_BUILD_DEBUG
#define UE_BUILD_DEBUG 0
#endif

typedef TSharedPtr<UE::Online::IOnlineServices> SubsystemType;
typedef TSharedPtr<TPromise<bool>> FAsyncLambdaResult;

DEFINE_LOG_CATEGORY_STATIC(LogOnlineTests, Log, Log);

#define UE_LOG_ONLINETESTS(Verbosity, Format, ...) \
{ \
	UE_LOG(LogOnlineTests, Verbosity, TEXT("OnlineTests: %s"), *FString::Printf(Format, ##__VA_ARGS__)); \
}

static const FTimespan TICK_DURATION = FTimespan::FromMilliseconds(1);

class FTestDriver;

struct FPipelineTestContextInitOptions
{
	FName ServiceName;
	UE::Online::EOnlineServices ServiceType;
};

class FPipelineTestContext
{

public:
	FPipelineTestContextInitOptions InitOptions;

	FPipelineTestContext(const FName& ServiceName, const UE::Online::EOnlineServices ServiceType)
	{
		InitOptions.ServiceName = ServiceName;
		InitOptions.ServiceType = ServiceType;
	}

};

#define INFO_TEST_STEP() INFO("Test Step Index: " << Index)
#define INFO_TEST_STEP_OF(InTestStep) INFO("Test Step Index: " << InTestStep->Index)

class FTestPipeline
{
public:
	/**
	 * Defines the configuration used when checking for excess tick lengths.
	 * The default setup is relatively lenient.  We should tighten this up
	 * then determine the specific test cases that are allowed to deviate.
	 */
	struct FEvaluateTickConfig
	{
		/** If true then the excess tick check will be used. */
		bool bEvaluateTickCheckActive = true;
		/**
		 * The expected average tick length.
		 * After the MinimumTickCount ticks, if the average tick exceeds this value then a CHECK failure will occurr.
		 * The default is changed for debug.
		 */
		FTimespan ExpectedAverageTick = FTimespan::FromMilliseconds(5);
		/**
		 * The absolute maximum tick length.
		 * If the current tick exceeds this value then a CHECK failure will occurr.
		 * The default is changed for debug.
		 */
		FTimespan AbsoluteMaximumTick = FTimespan::FromMilliseconds(75);
		/**
		 * The minimum number of ticks per test before the average tick is evaluated against ExpectedAvergateTick.
		 * The default is changed for debug.
		 */
		uint32_t MinimumTickCount = 10;

		/**
		 * Basic constructor used as the default with enabled tick check and settings,
		 * or when disabling the tick check.
		 */
		FEvaluateTickConfig(bool bInEvaluateTickCheckActive = true)
			: bEvaluateTickCheckActive(bInEvaluateTickCheckActive)
#if defined(UE_BUILD_DEBUG) && UE_BUILD_DEBUG
			, ExpectedAverageTick(FTimespan::FromMilliseconds(10))
			, AbsoluteMaximumTick(FTimespan::FromMilliseconds(150))
			, MinimumTickCount(10)
#endif
		{}

		/** Full constructor which allows changing all settings for an enabled tick check. */
		FEvaluateTickConfig(FTimespan&& InExpectedAverageTick, FTimespan&& InAbsoluteMaximumTick, uint32_t InMinimumTickCount = 10)
			: bEvaluateTickCheckActive(true)
			, ExpectedAverageTick(MoveTemp(InExpectedAverageTick))
			, AbsoluteMaximumTick(MoveTemp(InAbsoluteMaximumTick))
			, MinimumTickCount(InMinimumTickCount)
		{
		}
	};

	class FStep
	{
	public:
		uint32_t Index = 0;
		enum class EContinuance { ContinueStepping, Done };
		virtual ~FStep() {}
		virtual bool IsOptional() const { return false; }
		virtual bool RequiresDeletePostRelease() const { return false; }
		// Called only if RequestDeletePostRelease is true and the FStep is being stored.
		virtual void OnPreRelease() {}
		virtual EContinuance Tick(SubsystemType Subsystem) = 0;
	};
	using FStepPtr = TUniquePtr<FStep>;

	class FLambdaStep : public FStep
	{
	public:
		FLambdaStep(TUniqueFunction<void(SubsystemType)>&& InLambda)
			: Lambda(MoveTemp(InLambda))
		{}

		virtual EContinuance Tick(SubsystemType Subsystem) override
		{
			Lambda(Subsystem);
			return EContinuance::Done;
		}

	private:
		TUniqueFunction<void(SubsystemType)> Lambda;
	};

	class FAsyncLambdaStep : public FStep
	{
	public:
		FAsyncLambdaStep(TUniqueFunction<void(FAsyncLambdaResult, SubsystemType)>&& InLambda)
			: Lambda(MoveTemp(InLambda))
		{}

		virtual EContinuance Tick(SubsystemType Subsystem) override
		{
			if (!bRan)
			{
				bRan = true;
				FAsyncLambdaResult Result = MakeShared<TPromise<bool>>();
				Result->GetFuture().Next([this](bool Result)
				{
					REQUIRE(Result);
					bComplete = true;
				});
				Lambda(Result, Subsystem);
			}

			if (bComplete == true)
			{
				return EContinuance::Done;
			}
			else
			{
				return EContinuance::ContinueStepping;
			}
		}

	private:
		TUniqueFunction<void(FAsyncLambdaResult, SubsystemType)> Lambda;
		bool bRan = false;
		TAtomic<bool> bComplete = false;
	};

	FTestPipeline(FTestPipeline&&) = default;
	FTestPipeline(const FTestPipeline&) = delete;

	/** Adds an overall test timeout value to the pipeline. */
	FTestPipeline&& WithTimeout(FTimespan&& InTimeout)
	{
		PipelineTimeout = MoveTemp(InTimeout);
		return MoveTemp(*this);
	}

	/** Disables the per tick timer checks. */
	FTestPipeline&& WithoutEvaluateTickCheck()
	{
		EvaluateTickConfig = FEvaluateTickConfig(false);
		return MoveTemp(*this);
	}

	/** Changes and enables the per tick timer checks based on the provided arguments. */
	FTestPipeline&& WithEvaluateTickCheck(FTimespan&& InExpectedAverageTick, FTimespan&& InAbsoluteMaximumTick, uint32_t InMinimumTickCount = 10)
	{
		EvaluateTickConfig = FEvaluateTickConfig(MoveTemp(InExpectedAverageTick), MoveTemp(InAbsoluteMaximumTick), InMinimumTickCount);
		return MoveTemp(*this);
	}

	/** Adds a test step. */
	template <typename T, typename... TArguments>
	FTestPipeline&& EmplaceStep(TArguments&&... Args)
	{
		static_assert(std::is_base_of_v<FStep, T>, "Step type must be derived from FTestPipeline::FStep");
		uint32_t NewIndex = (uint32_t)TestSteps.Num();
		TestSteps.Emplace(MakeUnique<T>(Forward<TArguments>(Args)...));
		TestSteps.Last()->Index = NewIndex;

		return MoveTemp(*this);
	}

	/** Adds a custom test step. This function will run and resolve immediately */
	FTestPipeline&& EmplaceLambda(TUniqueFunction<void(SubsystemType)> Lambda)
	{
		return EmplaceStep<FLambdaStep>(MoveTemp(Lambda));
	}

	FTestPipeline&& EmplaceAsyncLambda(TUniqueFunction<void(FAsyncLambdaResult, SubsystemType)> Lambda)
	{
		return EmplaceStep<FAsyncLambdaStep>(MoveTemp(Lambda));
	}

	/** Generates a string suitable for INFO which will help identify where and when a specific failure has occurred. */
	FString InfoString() const;

	void operator()(SubsystemType Subsystem);

	void OnPreRelease();

	/** Given the time points before Tick and after, perform the configured excess tick check. */
	void EvaluatePlatformTickTime(const FTimespan& InDuration)
	{
		//EvaluatePlatformTickTime(InDuration);
	}

	void Start()
	{
		PipelineStartTime = FPlatformTime::Seconds();
	}

private:
	FTestPipeline(FTestDriver& InDriver, FTimespan&& InTimeout)
		: Driver(InDriver)
		, PipelineTimeout(MoveTemp(InTimeout))
	{
	}

	FTestPipeline(FTestDriver& InDriver, FTimespan&& InTimeout, FEvaluateTickConfig&& InEvaluateTickConfig)
		: Driver(InDriver)
		, PipelineTimeout(MoveTemp(InTimeout))
		, EvaluateTickConfig(MoveTemp(InEvaluateTickConfig))
	{
	}

	/** Given the duration of the call to Tick, perform the configured excess tick check. */
	void EvaluatePlatformTickTime(double&& TickTime);


	FTestDriver& Driver;
	TArray<FStepPtr> TestSteps;
	TArray<FStepPtr> CompletedSteps;
	TArray<FStepPtr> TimedoutSteps;
	TArray<FStepPtr> DeletePostReleaseSteps;
	FTimespan PipelineTimeout;
	double PipelineStartTime;
	/** Current excess tick config. */
	FEvaluateTickConfig EvaluateTickConfig;
	/** Sum of time to process all calls to Subsystem Tick. */
	FTimespan SubsystemTickSum = FTimespan::FromMilliseconds(0);;
	/** Number of calls to Subsystem Tick. */
	uint32 SubsystemTickCount = 0;

	friend class FTestDriver;
};

class FTestDriver
{
public:
	using FSubsystemInstanceMap = TMap<SubsystemType, FTestPipeline>;

	~FTestDriver();

	template <typename Func>
	void ForeachSubsystemInstance(Func&& Fn)
	{
		for (auto& [Subsystem, DriverFunc] : SubsystemInstances)
		{
			Fn(Subsystem, DriverFunc);
		}
	}

	FTestPipeline MakePipeline()
	{
		return MakePipeline(FTimespan::FromSeconds(60));
	}

	template <typename... TArgs>
	FTestPipeline MakePipeline(TArgs&&... Args)
	{
		return FTestPipeline(*this, Forward<TArgs>(Args)...);
	}

	bool AddPipeline(FTestPipeline&& Pipeline, const FPipelineTestContext& TestContext = FPipelineTestContext(TEXT("NULL"), UE::Online::EOnlineServices::Null));

	void MarkComplete(SubsystemType Key);

	void RunToCompletion();

	void SetDriverTimedOut(bool InValue) { UE_DEBUG_BREAK(); bDidTimeout = InValue; };

	FString TimeoutFailedTestInfo() const;

	int32 FailedStepNum = 0;

private:

	void FlushCompleted();

	FSubsystemInstanceMap SubsystemInstances;
	TSet<SubsystemType> CompletedInstances;
	bool bDidTimeout = false;
	double LastTickTime = 0.0;
};


