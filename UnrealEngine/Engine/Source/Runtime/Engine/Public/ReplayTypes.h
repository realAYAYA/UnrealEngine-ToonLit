// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectVersion.h"
#include "Misc/EngineVersion.h"
#include "Misc/NetworkGuid.h"
#include "Misc/NetworkVersion.h"
#include "Net/Common/Packets/PacketTraits.h"
#include "Net/ReplayResult.h"
#include "IPAddress.h"
#include "Serialization/BitReader.h"
#include "Traits/IsCharEncodingCompatibleWith.h"
#include "ReplayTypes.generated.h"

ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogDemo, Log, All);

class UNetConnection;

enum class EReplayHeaderFlags : uint32
{
	None = 0,
	ClientRecorded = (1 << 0),
	HasStreamingFixes = (1 << 1),
	DeltaCheckpoints = (1 << 2),
	GameSpecificFrameData = (1 << 3),
	ReplayConnection = (1 << 4),
	ActorPrioritizationEnabled = (1 << 5),
	NetRelevancyEnabled = (1 << 6),
	AsyncRecorded = (1 << 7),
};

ENUM_CLASS_FLAGS(EReplayHeaderFlags);

const TCHAR* LexToString(EReplayHeaderFlags Flag);

enum class EWriteDemoFrameFlags : uint32
{
	None = 0,
	SkipGameSpecific = (1 << 0),
};

ENUM_CLASS_FLAGS(EWriteDemoFrameFlags);

struct FPlaybackPacket
{
	TArray<uint8>		Data;
	float				TimeSeconds;
	int32				LevelIndex;
	uint32				SeenLevelIndex;

	void CountBytes(FArchive& Ar) const
	{
		Data.CountBytes(Ar);
	}
};

USTRUCT()
struct FLevelNameAndTime
{
	GENERATED_BODY()

	FLevelNameAndTime()
		: LevelChangeTimeInMS(0)
	{}

	FLevelNameAndTime(const FString& InLevelName, uint32 InLevelChangeTimeInMS)
		: LevelName(InLevelName)
		, LevelChangeTimeInMS(InLevelChangeTimeInMS)
	{}

	UPROPERTY()
	FString LevelName;

	UPROPERTY()
	uint32 LevelChangeTimeInMS;

	friend FArchive& operator<<(FArchive& Ar, FLevelNameAndTime& LevelNameAndTime)
	{
		Ar << LevelNameAndTime.LevelName;
		Ar << LevelNameAndTime.LevelChangeTimeInMS;
		return Ar;
	}

	void CountBytes(FArchive& Ar) const
	{
		LevelName.CountBytes(Ar);
	}
};

enum ENetworkVersionHistory
{
	HISTORY_REPLAY_INITIAL = 1,
	HISTORY_SAVE_ABS_TIME_MS = 2,				// We now save the abs demo time in ms for each frame (solves accumulation errors)
	HISTORY_INCREASE_BUFFER = 3,				// Increased buffer size of packets, which invalidates old replays
	HISTORY_SAVE_ENGINE_VERSION = 4,			// Now saving engine net version + InternalProtocolVersion
	HISTORY_EXTRA_VERSION = 5,					// We now save engine/game protocol version, checksum, and changelist
	HISTORY_MULTIPLE_LEVELS = 6,				// Replays support seamless travel between levels
	HISTORY_MULTIPLE_LEVELS_TIME_CHANGES = 7,	// Save out the time that level changes happen
	HISTORY_DELETED_STARTUP_ACTORS = 8,			// Save DeletedNetStartupActors inside checkpoints
	HISTORY_HEADER_FLAGS = 9,					// Save out enum flags with demo header
	HISTORY_LEVEL_STREAMING_FIXES = 10,			// Optional level streaming fixes.
	HISTORY_SAVE_FULL_ENGINE_VERSION = 11,		// Now saving the entire FEngineVersion including branch name
	HISTORY_HEADER_GUID = 12,					// Save guid to demo header
	HISTORY_CHARACTER_MOVEMENT = 13,			// Change to using replicated movement and not interpolation
	HISTORY_CHARACTER_MOVEMENT_NOINTERP = 14,	// No longer recording interpolated movement samples
	HISTORY_GUID_NAMETABLE = 15,				// Added a string table for exported guids
	HISTORY_GUIDCACHE_CHECKSUMS = 16,			// Removing guid export checksums from saved data, they are ignored during playback
	HISTORY_SAVE_PACKAGE_VERSION_UE = 17,		// Save engine and licensee package version as well, in case serialization functions need them for compatibility
	HISTORY_RECORDING_METADATA = 18,			// Adding additional record-time information to the header

	// -----<new versions can be added before this line>-------------------------------------------------
	HISTORY_PLUS_ONE,
	HISTORY_LATEST = HISTORY_PLUS_ONE - 1
};

static const uint32 NETWORK_DEMO_MAGIC = 0x2CF5A13D;
static const uint32 NETWORK_DEMO_VERSION = HISTORY_LATEST;
static const uint32 MIN_NETWORK_DEMO_VERSION = HISTORY_CHARACTER_MOVEMENT;

static const uint32 NETWORK_DEMO_METADATA_MAGIC = 0x3D06B24E;
static const uint32 NETWORK_DEMO_METADATA_VERSION = 0;

struct FNetworkDemoHeader
{
	uint32	Magic;									// Magic to ensure we're opening the right file.
	uint32	Version;								// Version number to detect version mismatches.
	uint32	NetworkChecksum;						// Network checksum
	uint32	EngineNetworkProtocolVersion;			// Version of the engine internal network format
	uint32	GameNetworkProtocolVersion;				// Version of the game internal network format
	FGuid	Guid;									// Unique identifier

	float MinRecordHz;
	float MaxRecordHz;
	float FrameLimitInMS;
	float CheckpointLimitInMS;

	FString Platform;
	EBuildConfiguration BuildConfig;
	EBuildTargetType BuildTarget;

	FEngineVersion EngineVersion;					// Full engine version on which the replay was recorded
	EReplayHeaderFlags HeaderFlags;					// Replay flags
	TArray<FLevelNameAndTime> LevelNamesAndTimes;	// Name and time changes of levels loaded for demo
	TArray<FString> GameSpecificData;				// Area for subclasses to write stuff
	FPackageFileVersion PackageVersionUE;			// Engine package version on which the replay was recorded
	int32 PackageVersionLicenseeUE;					// Licensee package version on which the replay was recorded

	FNetworkDemoHeader() :
		Magic(NETWORK_DEMO_MAGIC),
		Version(NETWORK_DEMO_VERSION),
		NetworkChecksum(FNetworkVersion::GetLocalNetworkVersion()),
		EngineNetworkProtocolVersion(FNetworkVersion::GetEngineNetworkProtocolVersion()),
		GameNetworkProtocolVersion(FNetworkVersion::GetGameNetworkProtocolVersion()),
		Guid(),
		MinRecordHz(0.0f),
		MaxRecordHz(0.0f),
		FrameLimitInMS(0.0f),
		CheckpointLimitInMS(0.0f),
		BuildConfig(EBuildConfiguration::Unknown),
		BuildTarget(EBuildTargetType::Unknown),
		EngineVersion(FEngineVersion::Current()),
		HeaderFlags(EReplayHeaderFlags::None),
		PackageVersionUE(GPackageFileUEVersion),
		PackageVersionLicenseeUE(GPackageFileLicenseeUEVersion)
	{
	}

	friend FArchive& operator << (FArchive& Ar, FNetworkDemoHeader& Header)
	{
		Ar << Header.Magic;

		// Check magic value
		if (Header.Magic != NETWORK_DEMO_MAGIC)
		{
			UE_LOG(LogDemo, Error, TEXT("Header.Magic != NETWORK_DEMO_MAGIC"));
			Ar.SetError();
			return Ar;
		}

		Ar << Header.Version;

		// Check version
		if (Header.Version < MIN_NETWORK_DEMO_VERSION)
		{
			UE_LOG(LogDemo, Error, TEXT("Header.Version < MIN_NETWORK_DEMO_VERSION. Header.Version: %i, MIN_NETWORK_DEMO_VERSION: %i"), Header.Version, MIN_NETWORK_DEMO_VERSION);
			Ar.SetError();
			return Ar;
		}

		Ar << Header.NetworkChecksum;
		Ar << Header.EngineNetworkProtocolVersion;
		Ar << Header.GameNetworkProtocolVersion;
		Ar << Header.Guid;
		Ar << Header.EngineVersion;

		if (Header.Version >= HISTORY_SAVE_PACKAGE_VERSION_UE)
		{
			Ar << Header.PackageVersionUE;
			Ar << Header.PackageVersionLicenseeUE;
		}
		else
		{
			if (Ar.IsLoading())
			{
				// Fix up for LWC compatibility.
				// Vectors were using operator<< to serialize in some network cases (instead of
				// the NetSerialize function), but this operator uses EUnrealEngineObjectUE5Version
				// and not EEngineNetworkVersionHistory for versioning. Therefore, pre-LWC replays
				// that did not save the EUnrealEngineObjectUE5Version will try to read doubles
				// from these vectors instead of floats.
				//
				// However, EEngineNetworkVersionHistory::HISTORY_SERIALIZE_DOUBLE_VECTORS_AS_DOUBLES was
				// added around the same time as EUnrealEngineObjectUE5Version::LARGE_WORLD_COORDINATES,
				// so we use the network version as an approximation the package version to allow
				// most pre-LWC replays to play back correctly. The compromise is that any replays recorded
				// between when HISTORY_SERIALIZE_DOUBLE_VECTORS_AS_DOUBLES and LARGE_WORLD_COORDINATES
				// were added won't play back correctly since they didn't store accurate version information.
				if (Header.EngineNetworkProtocolVersion < HISTORY_SERIALIZE_DOUBLE_VECTORS_AS_DOUBLES)
				{
					// If the replay was recorded before vectors were serialized with LWC, and before replays
					// saved the UE package version, set the package version to one before LARGE_WORLD_COORDINATES
					// so that vectors will be read properly by operator<<.
					Header.PackageVersionUE = FPackageFileVersion(VER_LATEST_ENGINE_UE4, EUnrealEngineObjectUE5Version::OPTIONAL_RESOURCES);
				}
			}
		}

		Ar << Header.LevelNamesAndTimes;
		Ar << Header.HeaderFlags;
		Ar << Header.GameSpecificData;

		if (Header.Version >= HISTORY_RECORDING_METADATA)
		{
			Ar << Header.MinRecordHz;
			Ar << Header.MaxRecordHz;

			Ar << Header.FrameLimitInMS;
			Ar << Header.CheckpointLimitInMS;

			Ar << Header.Platform;
			Ar << Header.BuildConfig;
			Ar << Header.BuildTarget;
		}

		return Ar;
	}

	void CountBytes(FArchive& Ar) const
	{
		LevelNamesAndTimes.CountBytes(Ar);
		for (const FLevelNameAndTime& LevelNameAndTime : LevelNamesAndTimes)
		{
			LevelNameAndTime.CountBytes(Ar);
		}

		GameSpecificData.CountBytes(Ar);
		for (const FString& Datum : GameSpecificData)
		{
			Datum.CountBytes(Ar);
		}
	}
};

// The type we use to store offsets in the archive
typedef int64 FArchivePos;

struct FDeltaCheckpointData
{
	/** Net startup actors that were destroyed */
	TArray<FString> RecordingDeletedNetStartupActors;
	/** Net startup actors that were destroyed */
	TSet<FString> DestroyedNetStartupActors;

	/** Destroyed dynamic actors that were active in the previous checkpoint */
	TSet<FNetworkGUID> DestroyedDynamicActors;
	/** Channels closed that were open in the previous checkpoint, and the reason why */
	TMap<FNetworkGUID, EChannelCloseReason> ChannelsToClose;

	void CountBytes(FArchive& Ar) const;
};

class FRepActorsCheckpointParams
{
public:
	const double StartCheckpointTime;
	const double CheckpointMaxUploadTimePerFrame;
};

struct FQueuedDemoPacket
{
	/** The packet data to send */
	TArray<uint8> Data;

	/** The size of the packet in bits */
	int32 SizeBits;

	/** The traits applied to the packet, if applicable */
	FOutPacketTraits Traits;

	/** Index of the level this packet is associated with. 0 indicates no association. */
	uint32 SeenLevelIndex;

public:
	FQueuedDemoPacket(uint8* InData, int32 InSizeBytes, int32 InSizeBits)
		: Data()
		, SizeBits(InSizeBits)
		, Traits()
		, SeenLevelIndex(0)
	{
		Data.AddUninitialized(InSizeBytes);
		FMemory::Memcpy(Data.GetData(), InData, InSizeBytes);
	}

	FQueuedDemoPacket(uint8* InData, int32 InSizeBits, FOutPacketTraits& InTraits)
		: Data()
		, SizeBits(InSizeBits)
		, Traits(InTraits)
		, SeenLevelIndex(0)
	{
		int32 SizeBytes = FMath::DivideAndRoundUp(InSizeBits, 8);

		Data.AddUninitialized(SizeBytes);
		FMemory::Memcpy(Data.GetData(), InData, SizeBytes);
	}

	void CountBytes(FArchive& Ar) const
	{
		Data.CountBytes(Ar);
	}
};

/*------------------------------------------------------------------------------------------
	FInternetAddrDemo - dummy internet addr that can be used for anything that requests it.
--------------------------------------------------------------------------------------------*/
class FInternetAddrDemo : public FInternetAddr
{
public:

	FInternetAddrDemo()
	{
	}

	virtual TArray<uint8> GetRawIp() const override
	{
		return TArray<uint8>();
	}

	virtual void SetRawIp(const TArray<uint8>& RawAddr) override
	{
	}

	void SetIp(uint32 InAddr) override
	{
	}


	void SetIp(const TCHAR* InAddr, bool& bIsValid) override
	{
	}

	void GetIp(uint32& OutAddr) const override
	{
		OutAddr = 0;
	}

	void SetPort(int32 InPort) override
	{
	}


	void GetPort(int32& OutPort) const override
	{
		OutPort = 0;
	}


	int32 GetPort() const override
	{
		return 0;
	}

	void SetAnyAddress() override
	{
	}

	void SetBroadcastAddress() override
	{
	}

	void SetLoopbackAddress() override
	{
	}

	FString ToString(bool bAppendPort) const override
	{
		return FString(TEXT("Demo Internet Address"));
	}

	virtual bool operator==(const FInternetAddr& Other) const override
	{
		return Other.ToString(true) == ToString(true);
	}

	bool operator!=(const FInternetAddrDemo& Other) const
	{
		return !(FInternetAddrDemo::operator==(Other));
	}

	virtual uint32 GetTypeHash() const override
	{
		return GetConstTypeHash();
	}

	uint32 GetConstTypeHash() const
	{
		return ::GetTypeHash(ToString(true));
	}

	friend uint32 GetTypeHash(const FInternetAddrDemo& A)
	{
		return A.GetConstTypeHash();
	}

	virtual bool IsValid() const override
	{
		return true;
	}

	virtual TSharedRef<FInternetAddr> Clone() const override
	{
		return DemoInternetAddr.ToSharedRef();
	}

	static TSharedPtr<FInternetAddr> DemoInternetAddr;
};

class FScopedForceUnicodeInArchive
{
public:
	FScopedForceUnicodeInArchive() = delete;
	FScopedForceUnicodeInArchive(FScopedForceUnicodeInArchive&&) = delete;
	FScopedForceUnicodeInArchive(const FScopedForceUnicodeInArchive&) = delete;
	FScopedForceUnicodeInArchive& operator=(const FScopedForceUnicodeInArchive&) = delete;
	FScopedForceUnicodeInArchive& operator=(FScopedForceUnicodeInArchive&&) = delete;

	FScopedForceUnicodeInArchive(FArchive& InArchive)
		: Archive(InArchive)
		, bWasUnicode(InArchive.IsForcingUnicode())
	{
		EnableFastStringSerialization();
	}

	~FScopedForceUnicodeInArchive()
	{
		RestoreStringSerialization();
	}

private:
	void EnableFastStringSerialization()
	{
		if constexpr (TIsCharEncodingCompatibleWith<WIDECHAR, TCHAR>::Value)
		{
			Archive.SetForceUnicode(true);
		}
	}

	void RestoreStringSerialization()
	{
		if constexpr (TIsCharEncodingCompatibleWith<WIDECHAR, TCHAR>::Value)
		{
			Archive.SetForceUnicode(bWasUnicode);
		}
	}

	FArchive& Archive;
	bool bWasUnicode;
};

/**
 * Helps track Offsets in an Archive before the actual size of the offset is known.
 * This relies on serialization always used a fixed number of bytes for primitive types,
 * and Sane implementations of Seek and Tell.
 */
class FScopedStoreArchiveOffset
{
public:
	FScopedStoreArchiveOffset() = delete;
	FScopedStoreArchiveOffset(FScopedStoreArchiveOffset&&) = delete;
	FScopedStoreArchiveOffset(const FScopedStoreArchiveOffset&) = delete;
	FScopedStoreArchiveOffset& operator=(const FScopedStoreArchiveOffset&) = delete;
	FScopedStoreArchiveOffset& operator=(FScopedStoreArchiveOffset&&) = delete;

	FScopedStoreArchiveOffset(FArchive& InAr) :
		Ar(InAr),
		StartPosition(Ar.Tell())
	{
		// Save room for the offset here.
		FArchivePos TempOffset = 0;
		Ar << TempOffset;
	}

	~FScopedStoreArchiveOffset()
	{
		const FArchivePos CurrentPosition = Ar.Tell();
		FArchivePos Offset = CurrentPosition - (StartPosition + sizeof(FArchivePos));
		Ar.Seek(StartPosition);
		Ar << Offset;
		Ar.Seek(CurrentPosition);
	}

private:

	FArchive& Ar;
	const FArchivePos StartPosition;
};

class FReplayExternalData
{
public:
	FReplayExternalData() : TimeSeconds(0.0f)
	{
	}

	FReplayExternalData(FBitReader&& InReader, const float InTimeSeconds) 
		: Reader(MoveTemp(InReader))
		, TimeSeconds(InTimeSeconds)
	{
	}

	FBitReader	Reader;
	float		TimeSeconds;

	void CountBytes(FArchive& Ar) const
	{
		Reader.CountMemory(Ar);
	}
};

// Using an indirect array here since FReplayExternalData stores an FBitReader, and it's not safe to store an FArchive directly in a TArray.
typedef TIndirectArray<FReplayExternalData> FReplayExternalDataArray;


// Can be used to override Version Data in a Replay's Header either Right Before Writing a Replay Header or Right After Reading a Replay Header.
struct FOverridableReplayVersionData
{
public:
	uint32 Version;                       // Version number to detect version mismatches.
	uint32 EngineNetworkProtocolVersion;  // Version of the engine internal network format
	uint32 GameNetworkProtocolVersion;    // Version of the game internal network format
	FEngineVersion EngineVersion;         // Full engine version on which the replay was recorded
	FPackageFileVersion PackageVersionUE; // Engine package version on which the replay was recorded
	int32 PackageVersionLicenseeUE;       // Licensee package version on which the replay was recorded

	// Init with Demo Header Version Data
	FOverridableReplayVersionData(const FNetworkDemoHeader& DemoHeader)
		: Version                     (DemoHeader.Version)
		, EngineNetworkProtocolVersion(DemoHeader.EngineNetworkProtocolVersion)
		, GameNetworkProtocolVersion  (DemoHeader.GameNetworkProtocolVersion)
		, EngineVersion               (DemoHeader.EngineVersion)
		, PackageVersionUE            (DemoHeader.PackageVersionUE)
		, PackageVersionLicenseeUE    (DemoHeader.PackageVersionLicenseeUE)
	{
	}

	// Apply Version Data to Demo Header Passed In
	void ApplyVersionDataToDemoHeader(FNetworkDemoHeader& DemoHeader)
	{
		DemoHeader.Version                      = Version;
		DemoHeader.EngineNetworkProtocolVersion = EngineNetworkProtocolVersion;
		DemoHeader.GameNetworkProtocolVersion   = GameNetworkProtocolVersion;
		DemoHeader.EngineVersion                = EngineVersion;
		DemoHeader.PackageVersionUE             = PackageVersionUE;
		DemoHeader.PackageVersionLicenseeUE     = PackageVersionLicenseeUE;
	}
};