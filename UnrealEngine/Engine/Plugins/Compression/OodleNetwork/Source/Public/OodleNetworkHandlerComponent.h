// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// @todo #JohnB: Separate module-based header code, from other class implementations, so that you can setup the PCH.h file correctly

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "PacketHandler.h"
#include "UObject/CoreNet.h"
#include "OodleNetworkAnalytics.h"
#include "OodleNetworkArchives.h"
#include "OodleNetworkFaultHandler.h"

#include "oodle2net.h"

DECLARE_LOG_CATEGORY_EXTERN(OodleNetworkHandlerComponentLog, Log, All);

// The maximum packet size that this component can handle - UNetConnection's should never allow MaxPacket to exceed MAX_PACKET_SIZE
#define MAX_OODLE_PACKET_BYTES	MAX_PACKET_SIZE

// The maximum compress/decompress buffer size - overkill, as buffers are statically allocated, and can't use Oodle runtime buffer calc
#define MAX_OODLE_BUFFER	(MAX_OODLE_PACKET_BYTES * 2)

/**
 * Specifies when compression is enabled. Used to make compression optional, for some platforms/clients
 */
UENUM()
enum class EOodleNetworkEnableMode : uint8
{
	AlwaysEnabled,					// Oodle compression is always enabled - forces compression to be enabled remotely
	WhenCompressedPacketReceived	// Oodle compression is only enabled if remotely requested
};


#define CAPTURE_EXT TEXT(".ucap")


/** Stats */

#if STATS

#if !UE_BUILD_SHIPPING
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Reserved Oodle (bits)"), STAT_PacketReservedOodle, STATGROUP_Packet, );
#endif


DECLARE_STATS_GROUP(TEXT("OodleNetwork"), STATGROUP_OodleNetwork, STATCAT_Advanced)

DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Oodle Out Rate Raw (bytes)"), STAT_Oodle_OutRaw, STATGROUP_OodleNetwork, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Oodle Out Rate Compressed (bytes)"), STAT_Oodle_OutCompressed, STATGROUP_OodleNetwork, );
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Oodle Out Rate Savings %"), STAT_Oodle_OutSavings, STATGROUP_OodleNetwork, );
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Oodle Out Total Savings %"), STAT_Oodle_OutTotalSavings, STATGROUP_OodleNetwork, );

DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Oodle In Rate Raw (bytes)"), STAT_Oodle_InRaw, STATGROUP_OodleNetwork, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Oodle In Rate Compressed (bytes)"), STAT_Oodle_InCompressed, STATGROUP_OodleNetwork, );
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Oodle In Rate Savings %"), STAT_Oodle_InSavings, STATGROUP_OodleNetwork, );
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Oodle In Total Savings %"), STAT_Oodle_InTotalSavings, STATGROUP_OodleNetwork, );

DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Oodle Compress Fail Num (0% savings)"), STAT_Oodle_CompressFailSavings, STATGROUP_OodleNetwork, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Oodle Compress Fail Num (size limit)"), STAT_Oodle_CompressFailSize, STATGROUP_OodleNetwork, );

// @todo #JohnB: Implement (e.g. deliberately skipping VOIP)
//DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Oodle Compress Skip Num"), STAT_Oodle_CompressSkip, STATGROUP_OodleNetwork, );

#if !UE_BUILD_SHIPPING
DECLARE_CYCLE_STAT_EXTERN(TEXT("Oodle Out Compress Time"), STAT_Oodle_OutCompressTime, STATGROUP_OodleNetwork, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Oodle In Decompress Time"), STAT_Oodle_InDecompressTime, STATGROUP_OodleNetwork, );
#endif

DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Oodle Dictionary Count"), STAT_Oodle_DictionaryCount, STATGROUP_OodleNetwork, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("Oodle Dictionary Bytes"), STAT_Oodle_DictionaryBytes, STATGROUP_OodleNetwork, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("Oodle Shared Bytes"), STAT_Oodle_SharedBytes, STATGROUP_OodleNetwork, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("Oodle State Bytes"), STAT_Oodle_StateBytes, STATGROUP_OodleNetwork, );

#endif // STATS


/** Globals */

/** The directory Oodle packet captures are saved to */
extern FString GOodleSaveDir;

/** The directory Oodle dictionaries are saved/loaded to/from */
extern FString GOodleContentDir;


#if STATS
// @todo #JohnB: The stats collecting is a bit crude, and should probably be giving per-dictionary stats

/**
 * Stores Oodle net traffic stats, accumulated over the past second, before passing it to the stats system
 */
class FOodleNetStats
{
private:
	/** Accumulated stats since last update */

	/** Input traffic compressed packet length */
	uint32 InCompressedLength;

	/** Input traffic decompressed packet length */
	uint32 InDecompressedLength;

	/** Output traffic compressed packet length */
	uint32 OutCompressedLength;

	/** Output traffic uncompressed packet length */
	uint32 OutUncompressedLength;


	/** Time of the last stats update */
	double LastStatsUpdate;


	/** Process lifetime stats */

	/** Total compressed input packets length */
	uint64 TotalInCompressedLength;

	/** Total decompressed input packets length */
	uint64 TotalInDecompressedLength;

	/** Total compressed output packets length */
	uint64 TotalOutCompressedLength;

	/** Total uncompressed output packets length */
	uint64 TotalOutUncompressedLength;

public:
	/**
	 * Base constructor
	 */
	FOodleNetStats()
		: InCompressedLength(0)
		, InDecompressedLength(0)
		, OutCompressedLength(0)
		, OutUncompressedLength(0)
		, LastStatsUpdate(0.0)
		, TotalInCompressedLength(0)
		, TotalInDecompressedLength(0)
		, TotalOutCompressedLength(0)
		, TotalOutUncompressedLength(0)
	{
	}

	/**
	 * Process incoming packet stats
	 *
	 * @param CompressedLength		The compressed size of the input packet
	 * @param DecompressedLength	The decompressed size of the input packet
	 */
	FORCEINLINE void IncomingStats(uint32 CompressedLength, uint32 DecompressedLength)
	{
		InCompressedLength += CompressedLength;
		TotalInCompressedLength += CompressedLength;
		InDecompressedLength += DecompressedLength;
		TotalInDecompressedLength += DecompressedLength;

		CheckForUpdate();
	}

	/**
	 * Process outgoing packet stats
	 *
	 * @param CompressedLength		The compressed size of the output packet
	 * @param UncompressedLength	The uncompressed size of the output packets
	 */
	FORCEINLINE void OutgoingStats(uint32 CompressedLength, uint32 UncompressedLength)
	{
		OutCompressedLength += CompressedLength;
		TotalOutCompressedLength += CompressedLength;
		OutUncompressedLength += UncompressedLength;
		TotalOutUncompressedLength += UncompressedLength;

		CheckForUpdate();
	}

	/**
	 * Checks to see if the main stats are due an update, and triggers an update if so
	 */
	FORCEINLINE void CheckForUpdate()
	{
		float DeltaTime = FPlatformTime::Seconds() - LastStatsUpdate;

		if (DeltaTime > 1.f)
		{
			UpdateStats(DeltaTime);
			LastStatsUpdate = FPlatformTime::Seconds();
		}
	}

	/**
	 * Passes up the accumulated stats, to the main engine stats tracking
	 *
	 * @param DeltaTime		The exact time since last stats update
	 */
	void UpdateStats(float DeltaTime);

	/**
	 * Resets the stat values
	 */
	void ResetStats();
};
#endif // STATS


/**
 * The mode that the Oodle packet handler should operate in
 */
enum EOodleNetworkHandlerMode
{
	Capturing,	// Stores packet captures for the server
	Release		// Compresses packet data, based on the dictionary file
};

/**
 * Encapsulates Oodle dictionary data loaded from file, to be wrapped in a shared pointer (auto-deleting when no longer in use)
 */
struct FOodleNetworkDictionary
{
	/** Size of the hash table used for the dictionary */
	uint32 HashTableSize;

	/** The raw dictionary data */
	uint8* DictionaryData;

	/** The size of the dictionary */
	uint32 DictionarySize;

	/** Shared dictionary state */
	OodleNetwork1_Shared* SharedDictionary;

	/** The size of the shared dictionary data (stored only for memory accounting) */
	uint32 SharedDictionarySize;

	/** The uncompacted compressor state */
	OodleNetwork1UDP_State* CompressorState;

	/** The size of CompressorState */
	uint32 CompressorStateSize;


private:
	FOodleNetworkDictionary()
	{
	}

	FOodleNetworkDictionary(const FOodleNetworkDictionary&) = delete;
	FOodleNetworkDictionary& operator=(const FOodleNetworkDictionary&) = delete;

public:

	/**
	 * Base constructor
	 */
	FOodleNetworkDictionary(uint32 InHashTableSize, uint8* InDictionaryData, uint32 InDictionarySize, OodleNetwork1_Shared* InSharedDictionary,
						uint32 InSharedDictionarySize, OodleNetwork1UDP_State* InInitialCompressorState, uint32 InCompressorStateSize);

	/**
	 * Base destructor
	 */
	~FOodleNetworkDictionary();
};


/**
 * PacketHandler component for implementing Oodle support.
 *
 * Implementation uses trained/dictionary-based UDP compression.
 */
class OODLENETWORKHANDLERCOMPONENT_API OodleNetworkHandlerComponent : public HandlerComponent
{
public:
	/** Initializes default data */
	OodleNetworkHandlerComponent();

	/** Default Destructor */
	~OodleNetworkHandlerComponent();

	virtual void CountBytes(FArchive& Ar) const override;

	/**
	 * Initializes first-run config settings
	 */
	static void InitFirstRunConfig();


	/**
	 * Initializes all required dictionaries
	 */
	void InitializeDictionaries();

	/**
	 * Lazy dictionary initialization, triggered by receiving a compressed packet from the remote connection
	 */
	void RemoteInitializeDictionaries();

	/**
	 * Initializes FOodleNetworkDictionary data, from the specified dictionary file
	 *
	 * @param FilePath			The dictionary file path
	 * @param OutDictionary		The FOodleNetworkDictionary shared pointer to write to
	 */
	void InitializeDictionary(FString FilePath, TSharedPtr<FOodleNetworkDictionary>& OutDictionary);

	/**
	 * Frees the local reference to FOodleNetworkDictionary data, and removes it from memory if it was the last reference
	 *
	 * @param InDictionary		The FOodleNetworkDictionary shared pointer being freed
	 */
	void FreeDictionary(TSharedPtr<FOodleNetworkDictionary>& InDictionary);

	/**
	 * Resolves and returns the default dictionary file paths.
	 *
	 * @param OutServerDictionary	The server dictionary path
	 * @param OutClientDictionary	The client dictionary path
	 * @param bFailFatal			Whether or not failure to set the dictionary paths, should be fatal
	 * @return						Whether or not the dictionary paths were successfully set
	 */
	bool GetDictionaryPaths(FString& OutServerDictionary, FString& OutClientDictionary, bool bFailFatal=true);

#if !UE_BUILD_SHIPPING || OODLE_DEV_SHIPPING
	/**
	 * Searches the game directory for alternate/fallback dictionary files, using the *.udic file extension.
	 * NOTE: This is non-shipping-only, as release games MUST have well-determined dictionary files (for net-binary-compatibility)
	 *
	 * @param OutServerDictionary	The server dictionary path
	 * @param OutClientDictionary	The client dictionary path
	 * @param bTestOnly				Whether this is being used to test for presence of alternate dictionaries (disables some logging)
	 * @return						Whether or not alternate dictionaries were found
	 */
	bool FindFallbackDictionaries(FString& OutServerDictionary, FString& OutClientDictionary, bool bTestOnly=false);


	/**
	 * Initializes the packet capture archives
	 */
	void InitializePacketLogs();

	/**
	 * Frees the packet capture archives
	 */
	void FreePacketLogs();
#endif

	/** 
	 * Check if component is currently compressing packets
	 *
	 * @return	Whether or not compression is active
	 */
	bool IsCompressionActive() const;

	virtual void Initialize() override;
	virtual void InitFaultRecovery(UE::Net::FNetConnectionFaultRecoveryBase* InFaultRecovery) override;
	virtual bool IsValid() const override;
	virtual void Incoming(FIncomingPacketRef PacketRef) override;
	virtual void Outgoing(FBitWriter& Packet, FOutPacketTraits& Traits) override;
	virtual int32 GetReservedPacketBits() const override;
	virtual void NotifyAnalyticsProvider() override;


protected:
	/** Whether or not Oodle, and its additions to the packet protocol, are enabled */
	bool bEnableOodle;

	/** When to enable compression on the server */
	EOodleNetworkEnableMode ServerEnableMode;

	/** When to enable compression on the client */
	EOodleNetworkEnableMode ClientEnableMode;

#if !UE_BUILD_SHIPPING || OODLE_DEV_SHIPPING
	/** File to log input packets to */
	FPacketCaptureArchive* InPacketLog;

	/** File to log output packets to */
	FPacketCaptureArchive* OutPacketLog;

	/** Search for dictionary files and use them if present - switching mode to Release in process - don't use in shipping */
	bool bUseDictionaryIfPresent;

	/** Whether or not packet capturing is currently enabled (outputs uncompressed packets to file) */
	bool bCaptureMode;
#endif

	/** Cached reserved packet bits for Oodle */
	uint32 OodleReservedPacketBits;

	/** The net analytics aggregator data, which will take the above locally tracked variables, once they are complete */
	TNetAnalyticsDataPtr<FOodleNetAnalyticsData> NetAnalyticsData;

	/** Whether or not Oodle analytics is enabled - cached from NetAnalyticsData, for fast checking */
	bool bOodleNetworkAnalytics;

#if !UE_BUILD_SHIPPING
public:
#endif

	/** Server (Outgoing) dictionary data */
	TSharedPtr<FOodleNetworkDictionary> ServerDictionary;

	/** Client (Incoming - relative to server) dictionary data */
	TSharedPtr<FOodleNetworkDictionary> ClientDictionary;

	/** Whether or not InitializeDictionaries was ever called */
	bool bInitializedDictionaries;


private:
	/** Fault handler for Oodle-Network-specific errors, that may trigger NetConnection Close */
	FOodleNetworkFaultHandler OodleNetworkFaultHandler;
};


/**
 * Oodle Module Interface
 */
class FOodleComponentModuleInterface : public FPacketHandlerComponentModuleInterface
{
private:

public:
	FOodleComponentModuleInterface()
	{
	}

	virtual TSharedPtr<HandlerComponent> CreateComponentInstance(FString& Options) override;
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
