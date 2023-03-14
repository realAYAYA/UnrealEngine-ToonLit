// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace UE::PixelStreaming
{
	template <typename KeyType, typename ValueType>
	class TThreadSafeMap
	{
	public:
		bool Add(KeyType Key, const ValueType& Value)
		{
			FScopeLock Lock(&Mutex);
			if (!InnerMap.Contains(Key))
			{
				InnerMap.Add(Key, Value);
				return true;
			}
			return false;
		}

		bool Remove(KeyType Key)
		{
			FScopeLock Lock(&Mutex);
			return InnerMap.Remove(Key) != 0;
		}

		ValueType& GetOrAdd(KeyType Key)
		{
			FScopeLock Lock(&Mutex);
			if (InnerMap.Contains(Key))
			{
				return InnerMap[Key];
			}
			return InnerMap.Add(Key);
		}

		ValueType* Find(KeyType Key)
		{
			FScopeLock Lock(&Mutex);
			return InnerMap.Find(Key);
		}

		const ValueType* Find(KeyType Key) const
		{
			FScopeLock Lock(&Mutex);
			return InnerMap.Find(Key);
		}

		void Clear()
		{
			FScopeLock Lock(&Mutex);
			InnerMap.Empty();
		}

		bool IsEmpty() const
		{
			FScopeLock Lock(&Mutex);
			return InnerMap.IsEmpty();
		}

		// Note: Do not call a map method inside a visitor or you will deadlock
		template <typename T>
		void Apply(T&& Visitor)
		{
			// work on a copy of the map so we dont lock while calling the visitor
			TMap<KeyType, ValueType> MapCopy;
			{
				FScopeLock Lock(&Mutex);
				MapCopy = InnerMap;
			}
			for (auto&& [Key, Value] : MapCopy)
			{
				Visitor(Key, Value);
			}
		}

		// Note: Do not call a map method inside a visitor or you will deadlock
		template <typename T>
		void ApplyUntil(T&& Visitor)
		{
			// work on a copy of the map so we dont lock while calling the visitor
			TMap<KeyType, ValueType> MapCopy;
			{
				FScopeLock Lock(&Mutex);
				MapCopy = InnerMap;
			}
			for (auto&& [Key, Value] : MapCopy)
			{
				if (Visitor(Key, Value))
				{
					break;
				}
			}
		}

	private:
		TMap<KeyType, ValueType> InnerMap;
		mutable FCriticalSection Mutex;
	};
} // namespace UE::PixelStreaming
