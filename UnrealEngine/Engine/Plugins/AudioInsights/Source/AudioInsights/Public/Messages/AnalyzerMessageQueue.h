// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/MpscQueue.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/ScopeLock.h"


namespace UE::Audio::Insights
{
	template <typename T>
	struct TAnalyzerMessageQueue
	{
		static constexpr double MaxHistoryLimitSec = 5.0;

		explicit TAnalyzerMessageQueue(double InHistoryLimitSec = MaxHistoryLimitSec)
			: HistoryLimitSec(FMath::Clamp(InHistoryLimitSec, UE_DOUBLE_KINDA_SMALL_NUMBER, MaxHistoryLimitSec))
		{
		}

	private:
		double HistoryLimitSec = 0.0;
		TMpscQueue<T> Data;

		mutable FCriticalSection AccessCritSec;

	public:
		bool IsEmpty() const
		{
			return Data.IsEmpty();
		}

		TArray<T> DequeueAll()
		{
			TArray<T> Output;

			{
				FScopeLock Lock(&AccessCritSec);

				if (!Data.IsEmpty())
				{
					double FirstTimeStamp = Data.Peek()->Timestamp;

					T Message;
					while (Data.Dequeue(Message))
					{
						if (HistoryLimitSec > Message.Timestamp - FirstTimeStamp)
						{
							Output.Add(MoveTemp(Message));
						}

						if (!Data.IsEmpty())
						{
							FirstTimeStamp = Data.Peek()->Timestamp;
						}
					}
				}
			}

			return Output;
		}

		void Enqueue(T&& InMessage)
		{
			{
				// Clear queue if queue time limit hit.
				// Have to lock as queues do not support
				// removal from producer thread. Should rarely
				// be expensive, as that would hit only under
				// conditions where consuming thread is stalled.
				FScopeLock Lock(&AccessCritSec);

				if (!Data.IsEmpty())
				{
					double FirstTimeStamp = Data.Peek()->Timestamp;
					while (InMessage.Timestamp - FirstTimeStamp > MaxHistoryLimitSec)
					{
						if (!Data.Dequeue().IsSet() || Data.IsEmpty())
						{
							break;
						}
						FirstTimeStamp = Data.Peek()->Timestamp;
					}
				}
			}

			Data.Enqueue(MoveTemp(InMessage));
		}

		void Empty()
		{
			FScopeLock Lock(&AccessCritSec);
			Data.Empty();
		}
	};
} // namespace UE::Audio::Insights
