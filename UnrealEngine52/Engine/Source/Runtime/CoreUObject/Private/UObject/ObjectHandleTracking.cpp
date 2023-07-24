// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/ObjectHandleTracking.h"
#include "Misc/ScopeRWLock.h"

#if UE_WITH_OBJECT_HANDLE_TRACKING

namespace UE::CoreUObject
{
	namespace Private
	{
		struct ObjectHandleCallbacks
		{
			static ObjectHandleCallbacks& Get()
			{
				static ObjectHandleCallbacks Callbacks;
				return Callbacks;
			}

			void OnHandleRead(TArrayView<const UObject* const> Objects)
			{
				FReadScopeLock _(HandleLock);
				for (auto&& Pair : ReadHandleCallbacks)
				{
					Pair.Value(Objects);
				}
			}

			void OnClassReferenceResolved(const FObjectRef& ObjectRef, UPackage* Package, UClass* Class)
			{
				FReadScopeLock _(HandleLock);
				for (auto&& Pair : ClassResolvedCallbacks)
				{
					Pair.Value(ObjectRef, Package, Class);
				}
			}

			void OnReferenceResolved(const FObjectRef& ObjectRef, UPackage* Package, UObject* Object)
			{
				FReadScopeLock _(HandleLock);
				for (auto&& Pair : HandleResolvedCallbacks)
				{
					Pair.Value(ObjectRef, Package, Object);
				}
			}

			void OnReferenceLoaded(const FObjectRef& ObjectRef, UPackage* Package, UObject* Object)
			{
				FReadScopeLock _(HandleLock);
				for (auto&& Pair : HandleLoadedCallbacks)
				{
					Pair.Value(ObjectRef, Package, Object);
				}
			}

			FObjectHandleTrackingCallbackId AddObjectHandleReadCallback(FObjectHandleReadFunc Func)
			{
				FWriteScopeLock _(HandleLock);
				NextHandleId++;
				ReadHandleCallbacks.Add({ NextHandleId, Func });
				return { NextHandleId };
			}

			void RemoveObjectHandleReadCallback(FObjectHandleTrackingCallbackId Handle)
			{
				FWriteScopeLock _(HandleLock);
				for (int32 i = ReadHandleCallbacks.Num() - 1; i >= 0; --i)
				{
					auto& Pair = ReadHandleCallbacks[i];
					if (Pair.Key == Handle.Id)
					{
						ReadHandleCallbacks.RemoveAt(i);
					}
				}
			}

			FObjectHandleTrackingCallbackId AddObjectHandleClassResolvedCallback(FObjectHandleClassResolvedFunc Func)
			{
				FWriteScopeLock _(HandleLock);
				NextHandleId++;
				ClassResolvedCallbacks.Add({ NextHandleId, Func });
				return { NextHandleId };
			}

			void RemoveObjectHandleClassResolvedCallback(FObjectHandleTrackingCallbackId Handle)
			{
				FWriteScopeLock _(HandleLock);
				for (int32 i = ClassResolvedCallbacks.Num() - 1; i >= 0; --i)
				{
					auto& Pair = ClassResolvedCallbacks[i];
					if (Pair.Key == Handle.Id)
					{
						ClassResolvedCallbacks.RemoveAt(i);
					}
				}
			}

			FObjectHandleTrackingCallbackId AddObjectHandleReferenceResolvedCallback(FObjectHandleReferenceResolvedFunc Func)
			{
				FWriteScopeLock _(HandleLock);
				NextHandleId++;
				HandleResolvedCallbacks.Add({ NextHandleId, Func });
				return { NextHandleId };
			}

			void RemoveObjectHandleReferenceResolvedCallback(FObjectHandleTrackingCallbackId Handle)
			{
				FWriteScopeLock _(HandleLock);
				for (int32 i = HandleResolvedCallbacks.Num() - 1; i >= 0; --i)
				{
					auto& Pair = HandleResolvedCallbacks[i];
					if (Pair.Key == Handle.Id)
					{
						HandleResolvedCallbacks.RemoveAt(i);
					}
				}
			}

			FObjectHandleTrackingCallbackId AddObjectHandleReferenceLoadedCallback(FObjectHandleReferenceLoadedFunc Func)
			{
				FWriteScopeLock _(HandleLock);
				NextHandleId++;
				HandleLoadedCallbacks.Add({ NextHandleId, Func });
				return { NextHandleId };
			}

			void RemoveObjectHandleReferenceLoadedCallback(FObjectHandleTrackingCallbackId Handle)
			{
				FWriteScopeLock _(HandleLock);
				for (int32 i = HandleLoadedCallbacks.Num() - 1; i >= 0; --i)
				{
					auto& Pair = HandleLoadedCallbacks[i];
					if (Pair.Key == Handle.Id)
					{
						HandleLoadedCallbacks.RemoveAt(i);
					}
				}
			}

		private:
			int32 NextHandleId = 0;
			TArray<TTuple<int32, FObjectHandleReadFunc>> ReadHandleCallbacks;
			TArray<TTuple<int32, FObjectHandleClassResolvedFunc>> ClassResolvedCallbacks;
			TArray<TTuple<int32, FObjectHandleReferenceResolvedFunc>> HandleResolvedCallbacks;
			TArray<TTuple<int32, FObjectHandleReferenceLoadedFunc>> HandleLoadedCallbacks;

			//using a single lock as currently there is not any contention. could add separate locks for each list. 
			FRWLock HandleLock;
		};

		void OnHandleRead(TArrayView<const UObject* const> Objects)
		{
			ObjectHandleCallbacks::Get().OnHandleRead(Objects);
		}

		void OnClassReferenceResolved(const FObjectRef& ObjectRef, UPackage* Package, UClass* Class)
		{
			ObjectHandleCallbacks::Get().OnClassReferenceResolved(ObjectRef, Package, Class);
		}

		void OnReferenceResolved(const FObjectRef& ObjectRef, UPackage* Package, UObject* Object)
		{
			ObjectHandleCallbacks::Get().OnReferenceResolved(ObjectRef, Package, Object);
		}

		void OnReferenceLoaded(const FObjectRef& ObjectRef, UPackage* Package, UObject* Object)
		{
			ObjectHandleCallbacks::Get().OnReferenceLoaded(ObjectRef, Package, Object);
		}

	}

	FObjectHandleTrackingCallbackId AddObjectHandleReadCallback(FObjectHandleReadFunc Func)
	{
		using namespace UE::CoreUObject::Private;
		return ObjectHandleCallbacks::Get().AddObjectHandleReadCallback(Func);
	}

	void RemoveObjectHandleReadCallback(FObjectHandleTrackingCallbackId Handle)
	{
		using namespace UE::CoreUObject::Private;
		ObjectHandleCallbacks::Get().RemoveObjectHandleReadCallback(Handle);
	}

	FObjectHandleTrackingCallbackId AddObjectHandleClassResolvedCallback(FObjectHandleClassResolvedFunc Func)
	{
		using namespace UE::CoreUObject::Private;
		return ObjectHandleCallbacks::Get().AddObjectHandleClassResolvedCallback(Func);
	}

	void RemoveObjectHandleClassResolvedCallback(FObjectHandleTrackingCallbackId Handle)
	{
		using namespace UE::CoreUObject::Private;
		ObjectHandleCallbacks::Get().RemoveObjectHandleClassResolvedCallback(Handle);
	}

	FObjectHandleTrackingCallbackId AddObjectHandleReferenceResolvedCallback(FObjectHandleReferenceResolvedFunc Func)
	{
		using namespace UE::CoreUObject::Private;
		return ObjectHandleCallbacks::Get().AddObjectHandleReferenceResolvedCallback(Func);
	}

	void RemoveObjectHandleReferenceResolvedCallback(FObjectHandleTrackingCallbackId Handle)
	{
		using namespace UE::CoreUObject::Private;
		ObjectHandleCallbacks::Get().RemoveObjectHandleReferenceResolvedCallback(Handle);
	}

	FObjectHandleTrackingCallbackId AddObjectHandleReferenceLoadedCallback(FObjectHandleReferenceLoadedFunc Func)
	{
		using namespace UE::CoreUObject::Private;
		return ObjectHandleCallbacks::Get().AddObjectHandleReferenceLoadedCallback(Func);
	}

	void RemoveObjectHandleReferenceLoadedCallback(FObjectHandleTrackingCallbackId Handle)
	{
		using namespace UE::CoreUObject::Private;
		ObjectHandleCallbacks::Get().RemoveObjectHandleReferenceLoadedCallback(Handle);
	}
}
#endif // UE_WITH_OBJECT_HANDLE_TRACKING
