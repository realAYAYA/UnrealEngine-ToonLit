// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Async/Async.h"
#include "Containers/Queue.h"
#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "HAL/ThreadSafeCounter64.h"

namespace BuildPatchServices
{
	/**
	 * Helper functions for wrapping async functionality.
	 */
	namespace AsyncHelpers
	{
		template<typename ResultType, typename... Args>
		static TFunction<void()> MakePromiseKeeper(const TSharedRef<TPromise<ResultType>, ESPMode::ThreadSafe>& Promise, const TFunction<ResultType(Args...)>& Function, Args... FuncArgs)
		{
			return [Promise, Function, FuncArgs...]()
			{
				Promise->SetValue(Function(FuncArgs...));
			};
		}

		template<typename... Args>
		static TFunction<void()> MakePromiseKeeper(const TSharedRef<TPromise<void>, ESPMode::ThreadSafe>& Promise, const TFunction<void(Args...)>& Function, Args... FuncArgs)
		{
			return [Promise, Function, FuncArgs...]()
			{
				Function(FuncArgs...);
				Promise->SetValue();
			};
		}

		template<typename ResultType, typename... Args>
		static TFuture<ResultType> ExecuteOnGameThread(const TFunction<ResultType(Args...)>& Function, Args... FuncArgs)
		{
			TSharedRef<TPromise<ResultType>, ESPMode::ThreadSafe> Promise = MakeShareable(new TPromise<ResultType>());
			TFunction<void()> PromiseKeeper = MakePromiseKeeper(Promise, Function, FuncArgs...);
			if (!IsInGameThread())
			{
				AsyncTask(ENamedThreads::GameThread, MoveTemp(PromiseKeeper));
			}
			else
			{
				PromiseKeeper();
			}
			return Promise->GetFuture();
		}

		template<typename ResultType>
		static TFuture<ResultType> ExecuteOnGameThread(const TFunction<ResultType()>& Function)
		{
			TSharedRef<TPromise<ResultType>, ESPMode::ThreadSafe> Promise = MakeShareable(new TPromise<ResultType>());
			TFunction<void()> PromiseKeeper = MakePromiseKeeper(Promise, Function);
			if (!IsInGameThread())
			{
				AsyncTask(ENamedThreads::GameThread, MoveTemp(PromiseKeeper));
			}
			else
			{
				PromiseKeeper();
			}
			return Promise->GetFuture();
		}

		template<typename ResultType>
		static TFuture<ResultType> ExecuteOnCustomThread(const TFunction<ResultType()>& Function, TQueue<TFunction<void()>, EQueueMode::Spsc>& CustomThreadQueue)
		{
			TSharedRef<TPromise<ResultType>, ESPMode::ThreadSafe> Promise = MakeShareable(new TPromise<ResultType>());
			TFunction<void()> PromiseKeeper = MakePromiseKeeper(Promise, Function);
			if (!IsInGameThread())
			{
				CustomThreadQueue.Enqueue(MoveTemp(PromiseKeeper));
			}
			else
			{
				PromiseKeeper();
			}
			return Promise->GetFuture();
		}
	}

	/**
	 * Additional atomic functionality
	 */
	namespace AsyncHelpers
	{
		/**
		 * LockFreePeak will set the destination value, to NewSample, if NewSample is higher.
		 * This works by spinning on InterlockedCompareExchange. For other usage examples see reference code in GenericPlatformAtomics.h
		 * @param PeakValue     The destination variable to set.
		 * @param NewSample     The sample to set with if higher.
		 */
		template<typename IntegerType>
		void LockFreePeak(volatile IntegerType* PeakValue, IntegerType NewSample)
		{
			IntegerType CurrentPeak;
			do
			{
				// Read the current value.
				CurrentPeak = *PeakValue;
			}
			// If the value was lower than the sample, try to update it to NewSample if the current value has not been set by another thread.
			// If the current value change since we read it, try again to see if our sample is still higher.
			while (CurrentPeak < NewSample && FPlatformAtomics::InterlockedCompareExchange(PeakValue, NewSample, CurrentPeak) != CurrentPeak);
		}
	}

	typedef FThreadSafeCounter64 FThreadSafeInt64;
	typedef FThreadSafeCounter FThreadSafeInt32;
}
