// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/NetworkVersion.h"

#include "BuildSettings.h"
#include "HAL/IConsoleManager.h"
#include "Logging/LogCategory.h"
#include "Misc/App.h"
#include "Misc/AssertionMacros.h"
#include "Misc/CString.h"
#include "Misc/CommandLine.h"
#include "Misc/Crc.h"
#include "Misc/EngineVersion.h"
#include "Misc/NetworkGuid.h" // IWYU pragma: keep
#include "Misc/Parse.h"
#include "Trace/Detail/Channel.h"
#include "Misc/StringBuilder.h"

DEFINE_LOG_CATEGORY( LogNetVersion );

FNetworkVersion::FGetLocalNetworkVersionOverride FNetworkVersion::GetLocalNetworkVersionOverride;
FNetworkVersion::FIsNetworkCompatibleOverride FNetworkVersion::IsNetworkCompatibleOverride;
FNetworkVersion::FGetReplayCompatibleChangeListOverride FNetworkVersion::GetReplayCompatibleChangeListOverride;

FString& FNetworkVersion::GetProjectVersion_Internal()
{
	static FString ProjectVersion = TEXT("1.0.0");
	return ProjectVersion;
}

bool FNetworkVersion::bHasCachedNetworkChecksum			= false;
uint32 FNetworkVersion::CachedNetworkChecksum			= 0;

uint32 FNetworkVersion::EngineNetworkProtocolVersion	= HISTORY_ENGINENETVERSION_LATEST;
uint32 FNetworkVersion::GameNetworkProtocolVersion		= 0;

uint32 FNetworkVersion::EngineCompatibleNetworkProtocolVersion		= HISTORY_REPLAY_BACKWARDS_COMPAT;
uint32 FNetworkVersion::GameCompatibleNetworkProtocolVersion		= 0;

void FNetworkVersion::SetProjectVersion(const TCHAR* InVersion)
{
	if (ensureMsgf(InVersion != nullptr && FCString::Strlen(InVersion), TEXT("ProjectVersion used for network version must be a valid string!")))
	{
		FString& ProjectVersion = GetProjectVersion_Internal();

		ProjectVersion = InVersion;
		bHasCachedNetworkChecksum = false;

		UE_LOG(LogNetVersion, Log, TEXT("Set ProjectVersion to %s. Version Checksum will be recalculated on next use."), *ProjectVersion);
	}	
}

void FNetworkVersion::SetGameNetworkProtocolVersion(const uint32 InGameNetworkProtocolVersion)
{
	GameNetworkProtocolVersion = InGameNetworkProtocolVersion;
	bHasCachedNetworkChecksum = false;

	UE_LOG(LogNetVersion, Log, TEXT("Set GameNetworkProtocolVersion to %ud. Version Checksum will be recalculated on next use."), GameNetworkProtocolVersion);
}

void FNetworkVersion::SetGameCompatibleNetworkProtocolVersion(const uint32 InGameCompatibleNetworkProtocolVersion)
{
	GameCompatibleNetworkProtocolVersion = InGameCompatibleNetworkProtocolVersion;
	bHasCachedNetworkChecksum = false;

	UE_LOG(LogNetVersion, Log, TEXT("Set GameCompatibleNetworkProtocolVersion to %ud. Version Checksum will be recalculated on next use."), GameCompatibleNetworkProtocolVersion);
}

uint32 FNetworkVersion::GetNetworkCompatibleChangelist()
{
	static int32 ReturnedVersion = ENGINE_NET_VERSION;
	static bool bStaticCheck = false;

	// add a cvar so it can be modified at runtime
	static FAutoConsoleVariableRef CVarNetworkVersionOverride(
		TEXT("networkversionoverride"), ReturnedVersion,
		TEXT("Sets network version used for multiplayer "),
		ECVF_Default);

	if (!bStaticCheck)
	{
		bStaticCheck = true;
		FParse::Value(FCommandLine::Get(), TEXT("networkversionoverride="), ReturnedVersion);
	}

	// If we have a version set explicitly, use that. Otherwise fall back to the regular engine version changelist, since it might be set at runtime (via Build.version).
	if (ReturnedVersion == 0)
	{
		return ENGINE_NET_VERSION ? ENGINE_NET_VERSION : BuildSettings::GetCompatibleChangelist();
	}

	return (uint32)ReturnedVersion;
}

uint32 FNetworkVersion::GetReplayCompatibleChangelist()
{
	// Use the override if it's bound
	if (GetReplayCompatibleChangeListOverride.IsBound())
	{
		const uint32 Changelist = GetReplayCompatibleChangeListOverride.Execute();

		UE_LOG(LogNetVersion, Log, TEXT("Replay changelist override: %d"), Changelist);
		return Changelist;
	}

	return FEngineVersion::CompatibleWith().GetChangelist();
}

uint32 FNetworkVersion::GetEngineNetworkProtocolVersion()
{
	return EngineNetworkProtocolVersion;
}

uint32 FNetworkVersion::GetEngineCompatibleNetworkProtocolVersion()
{
	return EngineCompatibleNetworkProtocolVersion;
}

uint32 FNetworkVersion::GetGameNetworkProtocolVersion()
{
	return GameNetworkProtocolVersion;
}

uint32 FNetworkVersion::GetGameCompatibleNetworkProtocolVersion()
{
	return GameCompatibleNetworkProtocolVersion;
}

uint32 FNetworkVersion::GetLocalNetworkVersion( bool AllowOverrideDelegate /*=true*/ )
{
	if ( bHasCachedNetworkChecksum )
	{
		return CachedNetworkChecksum;
	}

	if ( AllowOverrideDelegate && GetLocalNetworkVersionOverride.IsBound() )
	{
		CachedNetworkChecksum = GetLocalNetworkVersionOverride.Execute();

		UE_LOG( LogNetVersion, Log, TEXT( "Checksum from delegate: %u" ), CachedNetworkChecksum );

		bHasCachedNetworkChecksum = true;

		return CachedNetworkChecksum;
	}

	FString VersionString = FString::Printf(TEXT("%s %s, NetCL: %d, EngineNetVer: %d, GameNetVer: %d"),
		FApp::GetProjectName(),
		*FNetworkVersion::GetProjectVersion(),
		GetNetworkCompatibleChangelist(),
		FNetworkVersion::GetEngineNetworkProtocolVersion(),
		FNetworkVersion::GetGameNetworkProtocolVersion());

	CachedNetworkChecksum = FCrc::StrCrc32(*VersionString.ToLower());

	UE_LOG(LogNetVersion, Log, TEXT("%s (Checksum: %u)"), *VersionString, CachedNetworkChecksum);

	bHasCachedNetworkChecksum = true;

	return CachedNetworkChecksum;
}

bool FNetworkVersion::IsNetworkCompatible( const uint32 LocalNetworkVersion, const uint32 RemoteNetworkVersion )
{
	if ( IsNetworkCompatibleOverride.IsBound() )
	{
		return IsNetworkCompatibleOverride.Execute( LocalNetworkVersion, RemoteNetworkVersion );
	}

	return LocalNetworkVersion == RemoteNetworkVersion;
}

FNetworkReplayVersion FNetworkVersion::GetReplayVersion()
{
	const uint32 ReplayVersion = ( GameCompatibleNetworkProtocolVersion << 16 ) | EngineCompatibleNetworkProtocolVersion;

	return FNetworkReplayVersion( FApp::GetProjectName(), ReplayVersion, GetReplayCompatibleChangelist() );
}

bool FNetworkVersion::AreNetworkRuntimeFeaturesCompatible(EEngineNetworkRuntimeFeatures LocalFeatures, EEngineNetworkRuntimeFeatures RemoteFeatures)
{
	return LocalFeatures == RemoteFeatures;
}

void FNetworkVersion::DescribeNetworkRuntimeFeaturesBitset(EEngineNetworkRuntimeFeatures FeaturesBitflag, FStringBuilderBase& OutVerboseDescription)
{
	if (EnumHasAnyFlags(FeaturesBitflag, EEngineNetworkRuntimeFeatures::IrisEnabled))
	{
		OutVerboseDescription.Append(TEXT("IrisReplication"));
	}
	else
	{
		OutVerboseDescription.Append(TEXT("LegacyReplication"));
	}
}

