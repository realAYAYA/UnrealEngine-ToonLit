// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once


// Include
#include "CoreMinimal.h"

#include "Runtime/Launch/Resources/Version.h"
#include "Net/Core/Analytics/NetAnalytics.h"


/**
 * Simple container class for separating the analytics related variables from OodleNetworkHandlerComponent
 */
struct FOodleNetworkAnalyticsVars : public FLocalNetAnalyticsStruct
{
public:
	/**
	 * Default Constructor
	 */
	FOodleNetworkAnalyticsVars();

	bool operator == (const FOodleNetworkAnalyticsVars& A) const;

	/**
	 * Implements the TThreadedNetAnalyticsData CommitAnalytics interface
	 */
	void CommitAnalytics(FOodleNetworkAnalyticsVars& AggregatedData);


public:
	/** The number of incoming compressed packets */
	uint64 InCompressedNum;

	/** The number of incoming packets that were not compressed */
	uint64 InNotCompressedNum;


	/** The compressed length + decompression data overhead, of all incoming packets. The most accurate measure of compression savings. */
	uint64 InCompressedWithOverheadLengthTotal;

	/** The compressed length of all incoming packets. Measures Oodle algorithm compression, minus overhead reducing final savings. */
	uint64 InCompressedLengthTotal;

	/** The decompressed length of all incoming packets. */
	uint64 InDecompressedLengthTotal;

	/** The length of all incoming packets, which were not compressed. */
	uint64 InNotCompressedLengthTotal;


	/** The number of outgoing compressed packets. */
	uint64 OutCompressedNum;

	/** The number of outgoing packets that were not compressed, due to Oodle failing to compress enough. */
	uint64 OutNotCompressedFailedNum;

		/** The number of outgoing packets that were not compressed, due to Oodle failing to compress - which exclusively contained ack data */
		uint64 OutNotCompressedFailedAckOnlyNum;

		/** The number of outgoing packets that were not compressed, due to Oodle failing to compress - which were KeepAlive packets */
		uint64 OutNotCompressedFailedKeepAliveNum;

	/** The number of outgoing packets that were not compressed, due to byte rounding of compressed packets, exceeding size limits. */
	uint64 OutNotCompressedBoundedNum;

	/** The number of outgoing packets that were not compressed, due to a higher level flag requesting they be sent uncompressed. */
	uint64 OutNotCompressedFlaggedNum;

	/** The number of outgoing packets that were not compressed, due to the client having disabled compression. */
	uint64 OutNotCompressedClientDisabledNum;

	/** The number of outgoing packets that were not compressed, due to the packet being smaller that the CVar 'net.OodleMinSizeForCompression'. */
	uint64 OutNotCompressedTooSmallNum;


	/** The compressed length + decompression data overhead, of all outgoing packets. The most accurate measure of compression savings. */
	uint64 OutCompressedWithOverheadLengthTotal;

	/** The compressed length of all outgoing packets. Measures Oodle algorithm compression, minus overhead reducing final savings. */
	uint64 OutCompressedLengthTotal;

	/** The length prior to compression, of all outgoing packets (only counted for successfully compressed packets). */
	uint64 OutBeforeCompressedLengthTotal;

	/** The length of all outgoing packets, which failed to compress. */
	uint64 OutNotCompressedFailedLengthTotal;

	/** The length of all outgoing packets, where compression was skipped. */
	uint64 OutNotCompressedSkippedLengthTotal;


	/** The number of OodleNetworkHandlerComponent's running during the lifetime of the analytics aggregator (i.e. NetDriver lifetime). */
	uint32 NumOodleNetworkHandlers;

	/** The number of OodleNetworkHandlerComponent's that had packet compression enabled. */
	uint32 NumOodleNetworkHandlersCompressionEnabled;
};

/**
 * Oodle implementation for (serverside) threaded net analytics data - threading is taken care of, just need to send off the analytics
 */
struct FOodleNetAnalyticsData :
#if NET_ANALYTICS_MULTITHREADING
	public TThreadedNetAnalyticsData<FOodleNetworkAnalyticsVars>
#else
	public FNetAnalyticsData, public FOodleNetworkAnalyticsVars
#endif
{
public:
	virtual void SendAnalytics() override;


#if !NET_ANALYTICS_MULTITHREADING
	FOodleNetworkAnalyticsVars* GetLocalData()
	{
		return this;
	}
#endif

protected:
	/**
	 * Returns the analytics event name to use - to be overridden in subclasses
	 */
	virtual const TCHAR* GetAnalyticsEventName() const;
};

/**
 * Clientside version of the above net analytics data (typically only used for debugging)
 */
struct FClientOodleNetAnalyticsData : public FOodleNetAnalyticsData
{
	virtual const TCHAR* GetAnalyticsEventName() const override;
};



