// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Misc/Timespan.h"
#include "Math/IntPoint.h"
#include "Templates/SharedPointer.h"
#include "Templates/RefCounting.h"
#include "Containers/Queue.h"
#include "Misc/ScopeLock.h"

#include "ParameterDictionary.h"


struct FDecoderTimeStamp
{
	FDecoderTimeStamp() {}
	FDecoderTimeStamp(FTimespan InTime, int64 InSequenceIndex) : Time(InTime), SequenceIndex(InSequenceIndex) {}

	FTimespan Time;
	int64 SequenceIndex;
};


class IDecoderOutputPoolable : public TSharedFromThis<IDecoderOutputPoolable, ESPMode::ThreadSafe>
{
public:
	virtual void InitializePoolable() { }
	virtual void ShutdownPoolable() { }
	virtual bool IsReadyForReuse() { return true; }

public:
	virtual ~IDecoderOutputPoolable() { }
};


template<typename ObjectType> class TElectraPoolDefaultObjectFactory
{
public:
	static ObjectType* Create() { return new ObjectType; }
};

template<typename ObjectType, typename ObjectFactory = TElectraPoolDefaultObjectFactory<ObjectType>>
class TDecoderOutputObjectPool
{
	static_assert(TPointerIsConvertibleFromTo<ObjectType, IDecoderOutputPoolable>::Value, "Poolable objects must implement the IDecoderOutputPoolable interface.");

	/** Object pool storage. */
	class TStorage
	{
	public:
		TStorage(ObjectFactory* InObjectFactoryInstance)
			: ObjectFactoryInstance(InObjectFactoryInstance)
		{}

		/** Destructor. */
		~TStorage()
		{
			Reserve(0);
			ObjectType* Object;
			while (WaitReadyForReuse.Dequeue(Object))
			{
				Object->ShutdownPoolable();
				delete Object;
			}
		}

	public:

		/** Acquire an object from the pool. */
		ObjectType* Acquire()
		{
			ObjectType* Result = nullptr;
			{
				FScopeLock Lock(&CriticalSection);

				if (Pool.Num() > 0)
				{
					Result = Pool.Pop(false);
				}
				else
				{
					if (WaitReadyForReuse.Peek(Result))
					{
						if (Result->IsReadyForReuse())
						{
							WaitReadyForReuse.Pop();
							Result->ShutdownPoolable();
						}
						else
						{
							Result = nullptr;
						}
					}
				}
			}

			if (Result == nullptr)
			{
				Result = ObjectFactoryInstance->Create();
			}

			Result->InitializePoolable();

			return Result;
		}

		/** Get the number of objects stored. */
		int32 Num() const
		{
			FScopeLock Lock(&CriticalSection);
			return Pool.Num();
		}

		/** Return the given object to the pool. */
		void Release(ObjectType* Object)
		{
			if (Object == nullptr)
			{
				return;
			}

			FScopeLock Lock(&CriticalSection);

			if (Object->IsReadyForReuse())
			{
				Object->ShutdownPoolable();
				Pool.Push(Object);
			}
			else
			{
				WaitReadyForReuse.Enqueue(Object);
			}
		}

		/** Reserve the specified number of objects. */
		void Reserve(uint32 NumObjects)
		{
			FScopeLock Lock(&CriticalSection);

			while (NumObjects < (uint32)Pool.Num())
			{
				delete Pool.Pop(false);
			}

			while (NumObjects > (uint32)Pool.Num())
			{
				Pool.Push(ObjectFactoryInstance->Create());
			}
		}

		/** Regular tick call */
		void Tick()
		{
			if (WaitReadyForReuse.IsEmpty())
			{
				// Conservative early out to avoid CS: we will get any items missed the next time around
				return;
			}

			FScopeLock Lock(&CriticalSection);

			ObjectType* Object;
			while (WaitReadyForReuse.Peek(Object))
			{
				if (!Object->IsReadyForReuse())
				{
					break;
				}

				Object->ShutdownPoolable();
				Pool.Push(Object);
				WaitReadyForReuse.Pop();
			}
		}

		void PrepareForDecoderShutdown()
		{
			// [...]
		}

	private:

		/** Critical section for synchronizing access to the free list. */
		mutable FCriticalSection CriticalSection;

		/** List of unused objects. */
		TArray<ObjectType*> Pool;

		/** List of unused objects, waiting for reuse-ability. */
		TQueue<ObjectType*> WaitReadyForReuse;

		/** Object factory instance (nullptr by default) */
		ObjectFactory* ObjectFactoryInstance;
	};


	/** Deleter for pooled objects. */
	class TDeleter
	{
	public:

		/** Create and initialize a new instance. */
		TDeleter(const TSharedRef<TStorage, ESPMode::ThreadSafe>& InStorage)
			: StoragePtr(InStorage)
		{ }

		/** Function operator to execute deleter. */
		void operator()(ObjectType* ObjectToDelete)
		{
			TSharedPtr<TStorage, ESPMode::ThreadSafe> PinnedStorage = StoragePtr.Pin();

			if (PinnedStorage.IsValid())
			{
				PinnedStorage->Release(ObjectToDelete);
			}
			else
			{
				delete ObjectToDelete;
			}
		}

	private:

		/** Weak pointer to object pool storage. */
		TWeakPtr<TStorage, ESPMode::ThreadSafe> StoragePtr;
	};

public:

	TDecoderOutputObjectPool(ObjectFactory* ObjectFactoryInstance = nullptr)
		: Storage(MakeShareable(new TStorage(ObjectFactoryInstance)))
	{ }

	TDecoderOutputObjectPool(uint32 NumReserve, ObjectFactory* ObjectFactoryInstance = nullptr)
		: Storage(MakeShareable(new TStorage(ObjectFactoryInstance)))
	{
		Storage->Reserve(NumReserve);
	}

public:

	TSharedRef<ObjectType, ESPMode::ThreadSafe> AcquireShared()
	{
		ObjectType* Object = Storage->Acquire();
		check(Object != nullptr);

		return MakeShareable(Object, TDeleter(Storage));
	}

	int32 Num() const
	{
		return Storage->Num();
	}

	void Release(ObjectType* Object)
	{
		Storage->Release(Object);
	}

	void Reset(uint32 NumObjects = 0)
	{
		Storage->Reserve(NumObjects);
	}

	void Tick()
	{
		Storage->Tick();
	}

	void PrepareForDecoderShutdown()
	{
		Storage->PrepareForDecoderShutdown();
	}

private:

	/** Storage for pooled objects. */
	TSharedRef<TStorage, ESPMode::ThreadSafe> Storage;
};


class IDecoderOutput;

class IDecoderOutputOwner
{
public:
	virtual void SampleReleasedToPool(IDecoderOutput* InDecoderOutput) = 0;
};


class IDecoderOutput : public IDecoderOutputPoolable
{
public:
	virtual void SetOwner(const TSharedPtr<IDecoderOutputOwner, ESPMode::ThreadSafe>& Renderer) = 0;
	virtual FDecoderTimeStamp GetTime() const = 0;
	virtual FTimespan GetDuration() const = 0;

	virtual Electra::FParamDict& GetMutablePropertyDictionary()
	{ return PropertyDictionary; }
private:
	Electra::FParamDict PropertyDictionary;
};
