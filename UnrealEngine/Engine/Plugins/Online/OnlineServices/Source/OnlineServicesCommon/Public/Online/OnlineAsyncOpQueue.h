// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Online/OnlineAsyncOp.h"

#include "Containers/Queue.h"
#include "Containers/SpscQueue.h"

namespace UE::Online {

class FOnlineAsyncOpQueue
{
public:
	FOnlineAsyncOpQueue()
	{
	}

	virtual ~FOnlineAsyncOpQueue()
	{
		for (TOptional<TSharedRef<IWrappedOp>> Operation = QueuedOperations.Dequeue(); Operation.IsSet(); Operation = QueuedOperations.Dequeue())
		{
			Operation.GetValue()->Cancel(Errors::Cancelled());
		}
	}

	template <typename OpType>
	void Enqueue(TOnlineAsyncOp<OpType>& Op)
	{
		QueuedOperations.Enqueue(MakeShared<TWrappedOp<OpType>>(Op));
		TryStartOperations();
	}

	virtual void TryStartOperations() = 0;

protected:
	class IWrappedOp
	{
	public:
		virtual ~IWrappedOp() {}
		virtual void Start() = 0;
		virtual void Cancel(const FOnlineError& Reason) = 0;
		virtual bool IsComplete() const = 0;
		virtual TOnlineEvent<void(void)> OnComplete() = 0;
	};

	template <typename OpType>
	class TWrappedOp : public IWrappedOp, public TSharedFromThis<TWrappedOp<OpType>>
	{
	public:
		virtual ~TWrappedOp() {}

		TWrappedOp(TOnlineAsyncOp<OpType>& InOp)
			: Op(InOp.AsShared())
		{
		}

		virtual void Start() override final
		{
			OnCompleteHandle = Op->OnComplete().Add([this, WeakThis = TWeakPtr<TWrappedOp<OpType>>(this->AsShared())](const TOnlineAsyncOp<OpType>&, const TOnlineResult<OpType>&)
			{
				TSharedPtr<TWrappedOp<OpType>> PinnedThis = WeakThis.Pin();
				if (PinnedThis.IsValid())
				{
					OnCompleteEvent.Broadcast();
				}
			});
			Op->Start();
		}

		virtual void Cancel(const FOnlineError& Reason) override final
		{
			Op->Cancel(Reason);
		}

		virtual bool IsComplete() const override final
		{
			return Op->IsComplete();
		}

		virtual TOnlineEvent<void(void)> OnComplete() override final
		{
			return OnCompleteEvent;
		}

	private:
		TSharedRef<TOnlineAsyncOp<OpType>> Op;
		TOnlineEventCallable<void(void)> OnCompleteEvent;
		FOnlineEventDelegateHandle OnCompleteHandle;
	};

	TSpscQueue<TSharedRef<IWrappedOp>> QueuedOperations;
};

class FOnlineAsyncOpQueueParallel : public FOnlineAsyncOpQueue
{
public:
	virtual void TryStartOperations() override
	{
	}

	using FOnlineAsyncOpQueue::Enqueue;

	void Enqueue(TSharedRef<IWrappedOp>& Operation)
	{
		QueuedOperations.Enqueue(Operation);
	}

	void Tick(float DeltaSeconds)
	{
		// TODO: Limit for number of in-flight operations
		for (TOptional<TSharedRef<IWrappedOp>> Operation = QueuedOperations.Dequeue(); Operation.IsSet(); Operation = QueuedOperations.Dequeue())
		{
			InFlightOperations.Add(Operation.GetValue());
			Operation.GetValue()->Start();
		}
	}

protected:
	TArray<TSharedRef<IWrappedOp>> InFlightOperations;
};

class FOnlineAsyncOpQueueSerial : public FOnlineAsyncOpQueue
{
public:
	FOnlineAsyncOpQueueSerial(FOnlineAsyncOpQueueParallel& InParentQueue)
		: ParentQueue(InParentQueue)
	{
	}

	virtual void TryStartOperations() override
	{
		if (!InFlightOperation.IsValid() || InFlightOperation->IsComplete())
		{
			if (TOptional<TSharedRef<IWrappedOp>> Operation = QueuedOperations.Dequeue())
			{
				InFlightOperation = Operation.GetValue();
				// push to the parent parallel queue
				ParentQueue.Enqueue(Operation.GetValue());
				// when the operation completes, start the next one
				ContinuationDelegateHandle = InFlightOperation->OnComplete().Add([this]()
					{
						InFlightOperation.Reset();
						TryStartOperations();
					});
			}
		}
	}

protected:
	FOnlineAsyncOpQueueParallel& ParentQueue;

	TSharedPtr<IWrappedOp> InFlightOperation;
	FOnlineEventDelegateHandle ContinuationDelegateHandle;
};

/* UE::Online */ }
