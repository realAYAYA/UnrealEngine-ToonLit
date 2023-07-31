// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/AsyncWork.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "HAL/PlatformAtomics.h"
#include "Misc/AssertionMacros.h"
#include "SocketTypes.h"
#include "Stats/Stats.h"
#include "Stats/Stats2.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealNames.h"

/**
 * Represents an internet address. All data is in network byte order
 */
class SOCKETS_API FInternetAddr
{
protected:
	/** Hidden on purpose */
	FInternetAddr()
	{
	}

public:

	virtual ~FInternetAddr() 
	{
	}

	/**
	 * Compares FInternetAddrs together, comparing the logical net addresses (endpoints)
	 * of the data stored, rather than doing a memory comparison like the equality operator does.
	 * If it is not explicitly implemented, this falls back to just doing the same behavior of the
	 * comparison operator.
	 *
	 * @Param InAddr The address to compare with.
	 *
	 * @return true if the endpoint stored in this FInternetAddr is the same as the input.
	 */
	virtual bool CompareEndpoints(const FInternetAddr& InAddr) const
	{
		return *this == InAddr;
	}

	/**
	 * Sets the ip address from a host byte order uint32
	 *
	 * @param InAddr the new address to use (must convert to network byte order)
	 */
	virtual void SetIp(uint32 InAddr) = 0;
	
	/**
	 * Sets the ip address from a string ("A.B.C.D")
	 *
	 * @param InAddr the string containing the new ip address to use
	 */
	virtual void SetIp(const TCHAR* InAddr, bool& bIsValid) = 0;

	/**
	 * Copies the network byte order ip address to a host byte order dword
	 *
	 * @param OutAddr the out param receiving the ip address
	 */
	virtual void GetIp(uint32& OutAddr) const = 0;

	/**
	 * Sets the port number from a host byte order int
	 *
	 * @param InPort the new port to use (must convert to network byte order)
	 */
	virtual void SetPort(int32 InPort) = 0;

	/**
	 * Copies the port number from this address and places it into a host byte order int
	 *
	 * @param OutPort the host byte order int that receives the port
	 */
	virtual void GetPort(int32& OutPort) const
	{
		OutPort = GetPort();
	}

	/**
	 * Returns the port number from this address in host byte order
	 */
	virtual int32 GetPort() const = 0;

	/** 
	 * Set Platform specific port data
	 */
	virtual void SetPlatformPort(int32 InPort)
	{
		SetPort(InPort);
	}

	/** 
	 * Get platform specific port data.
	 */
	virtual int32 GetPlatformPort() const
	{
		return GetPort();
	}

	/**
	 * Sets the ip address from a raw network byte order array.
	 *
	 * @param RawAddr the new address to use (must be converted to network byte order)
	 */
	virtual void SetRawIp(const TArray<uint8>& RawAddr) = 0;

	/**
	 * Gets the ip address in a raw array stored in network byte order.
	 *
	 * @return The raw address stored in an array
	 */
	virtual TArray<uint8> GetRawIp() const = 0;

	/** Sets the address to be any address */
	virtual void SetAnyAddress() = 0;

	/** Sets the address to broadcast */
	virtual void SetBroadcastAddress() = 0;

	/** Sets the address to loopback */
	virtual void SetLoopbackAddress() = 0;

	/**
	 * Converts this internet ip address to string form
	 *
	 * @param bAppendPort whether to append the port information or not
	 */
	virtual FString ToString(bool bAppendPort) const = 0;

	/**
	 * Compares two internet ip addresses for equality
	 *
	 * @param Other the address to compare against
	 */
	virtual bool operator==(const FInternetAddr& Other) const
	{
		TArray<uint8> ThisIP = GetRawIp();
		TArray<uint8> OtherIP = Other.GetRawIp();
		return ThisIP == OtherIP && GetPort() == Other.GetPort();
	}

	/**
	 * Hash function for use with TMap's - exposed through FInternetAddrMapRef
	 *
	 * @return	The value to use for the hash
	 */
	virtual uint32 GetTypeHash() const = 0;

	/**
	 * Is this a well formed internet address
	 *
	 * @return true if a valid IP, false otherwise
	 */
	virtual bool IsValid() const = 0;
	
	/**
	 * Creates a new structure with the same data as this structure
	 *
	 * @return The new structure
	 */
	virtual TSharedRef<FInternetAddr> Clone() const = 0;

	/**
	 * Returns the protocol type name of the address data currently stored in this struct
	 *
	 * @return The type of the address. 
	 *         If it's not known or overridden in a derived class, the return is None.
	 */
	virtual FName GetProtocolType() const
	{
		return NAME_None;
	}

	virtual void DumpAddrData() const;
};

/**
 * Abstract interface used by clients to get async host name resolution to work in a
 * cross-platform way
 */
class FResolveInfo
{
protected:
	/** Hidden on purpose */
	FResolveInfo()
	{
	}

public:
	/** Virtual destructor for child classes to overload */
	virtual ~FResolveInfo()
	{
	}

	/**
	 * Whether the async process has completed or not
	 *
	 * @return true if it completed successfully, false otherwise
	 */
	virtual bool IsComplete() const = 0;

	/**
	 * The error that occurred when trying to resolve
	 *
	 * @return error code from the operation
	 */
	virtual int32 GetErrorCode() const = 0;

	/**
	 * Returns a copy of the resolved address
	 *
	 * @return the resolved IP address
	 */
	virtual const FInternetAddr& GetResolvedAddress() const = 0;
};

/** A non-async resolve info for returning cached results */
class FResolveInfoCached : public FResolveInfo
{
protected:
	/** The address that was resolved */
	TSharedPtr<FInternetAddr> Addr;

	/** Hidden on purpose */
	FResolveInfoCached() {}

public:
	/**
	 * Sets the address to return to the caller
	 *
	 * @param InAddr the address that is being cached
	 */
	FResolveInfoCached(const FInternetAddr& InAddr);

// FResolveInfo interface

	/**
	 * Whether the async process has completed or not
	 *
	 * @return true if it completed successfully, false otherwise
	 */
	virtual bool IsComplete() const
	{
		return true;
	}

	/**
	 * The error that occurred when trying to resolve
	 *
	 * @return error code from the operation
	 */
	virtual int32 GetErrorCode() const
	{
		return 0;
	}

	/**
	 * Returns a copy of the resolved address
	 *
	 * @return the resolved IP address
	 */
	virtual const FInternetAddr& GetResolvedAddress() const
	{
		return *Addr;
	}
};

//
// Class for creating a background thread to resolve a host.
//
class FResolveInfoAsync :
	public FResolveInfo
{
	//
	// A simple wrapper task that calls back to FResolveInfoAsync to do the actual work
	//
	class FResolveInfoAsyncWorker
	{
	public:
		/** Pointer to FResolveInfoAsync to call for async work*/
		FResolveInfoAsync* Parent;

		/** Constructor
		* @param InParent the FResolveInfoAsync to route the async call to
		*/
		FResolveInfoAsyncWorker(FResolveInfoAsync* InParent)
			: Parent(InParent)
		{
		}
		
		/** Call DoWork on the parent */
		void DoWork()
		{
			Parent->DoWork();
		}

		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FResolveInfoAsyncWorker, STATGROUP_ThreadPoolAsyncTasks);
		}

		/** Indicates to the thread pool that this task is abandonable */
		bool CanAbandon()
		{
			return true;
		}

		/** Effects the ending of the async resolve */
		void Abandon()
		{
			FPlatformAtomics::InterlockedExchange(&Parent->bShouldAbandon,true);
		}
	};
	// Variables.
	TSharedPtr<FInternetAddr>	Addr;
	ANSICHAR	HostName[256];
	/** Error code returned by GetHostByName. */
	int32			ErrorCode;
	/** Tells the worker thread whether it should abandon it's work or not */
	volatile int32 bShouldAbandon;
	/** Async task for async resolve */
	FAsyncTask<FResolveInfoAsyncWorker> AsyncTask;

public:
	/**
	 * Copies the host name for async resolution
	 *
	 * @param InHostName the host name to resolve
	 */
	FResolveInfoAsync(const ANSICHAR* InHostName);

	/**
	 * Start the async work and perform it synchronously if no thread pool is available
	 */
	void StartAsyncTask()
	{
		check(AsyncTask.GetTask().Parent == this); // need to make sure these aren't memcpy'd around after construction
		AsyncTask.StartBackgroundTask();
	}

	/**
	 * Resolves the specified host name
	 */
	void DoWork();

// FResolveInfo interface

	/**
	 * Whether the async process has completed or not
	 *
	 * @return true if it completed successfully, false otherwise
	 */
	virtual bool IsComplete() const
	{
		// this semantically const, but IsDone syncs the async task, and that causes writes
		return const_cast<FAsyncTask<FResolveInfoAsyncWorker> &>(AsyncTask).IsDone();
	}

	/**
	 * The error that occurred when trying to resolve
	 *
	 * @return error code from the operation
	 */
	virtual int32 GetErrorCode() const
	{
		return ErrorCode;
	}

	/**
	 * Returns a copy of the resolved address
	 *
	 * @return the resolved IP address
	 */
	virtual const FInternetAddr& GetResolvedAddress() const
	{
		return *Addr;
	}
};

/**
 * KeyFuncs for mapping/hashing FInternetAddr shared references
 *
 * NOTE: Implements TSharedRef over TSharedPtr, as this is required for performance critical code
 */
template<typename ValueType>
struct FInternetAddrKeyMapFuncs : public BaseKeyFuncs<ValueType, TSharedRef<FInternetAddr>, false>
{
	static FORCEINLINE const TSharedRef<FInternetAddr>& GetSetKey(const TPair<TSharedRef<FInternetAddr>, ValueType>& Element)
	{
		return Element.Key;
	}

	static FORCEINLINE bool Matches(const TSharedRef<FInternetAddr>& A, const TSharedRef<FInternetAddr>& B)
	{
		return *A == *B;
	}

	static FORCEINLINE uint32 GetKeyHash(const TSharedRef<FInternetAddr>& Key)
	{
		return Key->GetTypeHash();
	}
};

template<typename ValueType>
struct FInternetAddrConstKeyMapFuncs : public BaseKeyFuncs<ValueType, TSharedRef<const FInternetAddr>, false>
{
	static FORCEINLINE const TSharedRef<const FInternetAddr>& GetSetKey(const TPair<TSharedRef<const FInternetAddr>, ValueType>& Element)
	{
		return Element.Key;
	}

	static FORCEINLINE bool Matches(const TSharedRef<const FInternetAddr>& A, const TSharedRef<const FInternetAddr>& B)
	{
		return *A == *B;
	}

	static FORCEINLINE uint32 GetKeyHash(const TSharedRef<const FInternetAddr>& Key)
	{
		return Key->GetTypeHash();
	}
};

inline uint32 GetTypeHash(const FInternetAddr& InAddr)
{
	return InAddr.GetTypeHash();
}
