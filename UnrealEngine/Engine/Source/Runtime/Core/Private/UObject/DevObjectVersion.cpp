// Copyright Epic Games, Inc. All Rights Reserved.
#include "UObject/DevObjectVersion.h"
#include "Logging/LogMacros.h"
#include "Misc/ScopeLock.h"
#include "UObject/BlueprintsObjectVersion.h"
#include "UObject/BuildObjectVersion.h"
#include "UObject/CoreObjectVersion.h"
#include "UObject/EditorObjectVersion.h"
#include "UObject/EnterpriseObjectVersion.h"
#include "UObject/FrameworkObjectVersion.h"
#include "UObject/MobileObjectVersion.h"
#include "UObject/NetworkingObjectVersion.h"
#include "UObject/OnlineObjectVersion.h"
#include "UObject/PhysicsObjectVersion.h"
#include "UObject/PlatformObjectVersion.h"
#include "UObject/RenderingObjectVersion.h"
#include "UObject/SequencerObjectVersion.h"
#include "UObject/VRObjectVersion.h"
#include "UObject/GeometryObjectVersion.h"
#include "UObject/AnimPhysObjectVersion.h"
#include "UObject/AnimObjectVersion.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "UObject/FortniteValkyrieBranchObjectVersion.h"
#include "UObject/FortniteSeasonBranchObjectVersion.h"
#include "UObject/FortniteReleaseBranchCustomObjectVersion.h"
#include "UObject/FortniteShaderworkObjectVersion.h"
#include "UObject/ReflectionCaptureObjectVersion.h"
#include "UObject/LoadTimesObjectVersion.h"
#include "UObject/AutomationObjectVersion.h"
#include "UObject/NiagaraObjectVersion.h"
#include "UObject/DestructionObjectVersion.h"
#include "UObject/ExternalPhysicsCustomObjectVersion.h"
#include "UObject/ExternalPhysicsMaterialCustomObjectVersion.h"
#include "UObject/CineCameraObjectVersion.h"
#include "UObject/VirtualProductionObjectVersion.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "UObject/UE5ReleaseStreamObjectVersion.h"
#include "UObject/UE5PrivateFrostyStreamObjectVersion.h"
#include "UObject/UE5CookerObjectVersion.h"
#include "UObject/UE5LWCRenderingStreamObjectVersion.h"
#include "UObject/MediaFrameWorkObjectVersion.h"

DEFINE_LOG_CATEGORY_STATIC(LogDevObjectVersion, Log, All);

#if !UE_BUILD_SHIPPING
static TArray<FGuid, TInlineAllocator<64>> GDevVersions;

// A multi map of all Guids registered for a system to identify and report duplicates
static TMultiMap<FGuid, FGuid> GRegisteredSystemGuids;
#endif

struct FDevSystemGuidRegistry
{
	void RegisterSystemGuids(const TMap<FGuid, FGuid>& SystemGuids);
	FGuid GetSystemGuid(FGuid System);

private:

	struct FRegisteredGuid
	{
		FGuid Guid;
		bool bHasBeenRead = false;
	};

	void RegisterSystemGuid(FGuid System, FGuid Guid);

	FCriticalSection Lock;
	TMap<FGuid, FRegisteredGuid> RegisteredSystemGuids;
};

static FDevSystemGuidRegistry& GetSystemGuidRegistry()
{
	// Pass FDevSystemGuidRegistry singleton via a function to force its initialization
	// before it is used by global static variables and prevent static initialization order fiasco
	static FDevSystemGuidRegistry GSystemGuidRegistry;
	return GSystemGuidRegistry;
}

FDevSystemGuidRegistration::FDevSystemGuidRegistration(const TMap<FGuid, FGuid>& SystemGuids)
{
	GetSystemGuidRegistry().RegisterSystemGuids(SystemGuids);
}

void FDevSystemGuidRegistry::RegisterSystemGuid(FGuid System, FGuid Guid)
{
#if !UE_BUILD_SHIPPING
	TArray<FGuid> RegisteredGuids;
	GRegisteredSystemGuids.MultiFind(System, RegisteredGuids);
	if (ensureMsgf(!RegisteredGuids.Contains(Guid), TEXT("Registering a duplicate guid for the same system. Validate that each system has a unique Guid and you are not registering a system twice.")))
	{
		GRegisteredSystemGuids.Add(System, Guid);
	}
#endif

	FRegisteredGuid& RegisteredGuid = RegisteredSystemGuids.FindOrAdd(System);

	ensureMsgf(!RegisteredGuid.bHasBeenRead, TEXT("Registering a system guid after it has already been read."));

	RegisteredGuid.Guid.A ^= Guid.A;
	RegisteredGuid.Guid.B ^= Guid.B;
	RegisteredGuid.Guid.C ^= Guid.C;
	RegisteredGuid.Guid.D ^= Guid.D;
}

void FDevSystemGuidRegistry::RegisterSystemGuids(const TMap<FGuid, FGuid>& SystemGuids)
{
	FScopeLock Scope(&Lock);

	for (const TPair<FGuid, FGuid>& SystemGuid : SystemGuids)
	{
		check(SystemGuid.Key.IsValid());
		check(SystemGuid.Value.IsValid());
		RegisterSystemGuid(SystemGuid.Key, SystemGuid.Value);
	}
}

FGuid FDevSystemGuidRegistry::GetSystemGuid(FGuid System)
{
	FScopeLock Scope(&Lock);

	FRegisteredGuid& RegisteredGuid = RegisteredSystemGuids.FindOrAdd(System);
	RegisteredGuid.bHasBeenRead = true;
	return RegisteredGuid.Guid;
}

FDevSystemGuids::FDevSystemGuids()
	: GLOBALSHADERMAP_DERIVEDDATA_VER(0x7BB10A3C, 0xAC4E46F3, 0xAC78F4C1, 0xEFB3E34F)
	, GROOM_BINDING_DERIVED_DATA_VERSION(0x30769E53, 0x0C574C7B, 0xA15C56F2, 0x24A64E32)
	, GROOM_DERIVED_DATA_VERSION(0x05A37379, 0xF8A049D5, 0x986824E9, 0xAAA83F41)
	, LANDSCAPE_MOBILE_COOK_VERSION(0x0E9ADF72, 0xD6B64E0D, 0x81C4A92B, 0x081A37AB)
	, MATERIALSHADERMAP_DERIVEDDATA_VER(0x9131A169, 0x3E004B0D, 0x864390EF, 0xA1F12934)
	, NANITE_DERIVEDDATA_VER(0xBEB0226A, 0x070E4ECA, 0x972C1E7D, 0xD8599E68)
	, NIAGARASHADERMAP_DERIVEDDATA_VER(0x7BBD9913, 0xC1554D20, 0xADAE9F17, 0xB006299E)
	, Niagara_LatestScriptCompileVersion(0x6D32B8EE, 0x909FCA7E, 0xA5CE4F17, 0x066A5F25)
	, POSESEARCHDB_DERIVEDDATA_VER(0x389117E4, 0x807A4CC0, 0x9F37C2E6, 0xD808A78D)
	, SkeletalMeshDerivedDataVersion(0x9B5F4544, 0x76D7481C, 0x9AD3F614, 0xA6C07904)
	, STATICMESH_DERIVEDDATA_VER(0x2C6C400C, 0x2EDF47B3, 0x9BD36689, 0x7B77B208)
	, MaterialTranslationDDCVersion(0x59E1F296, 0x27064757, 0x8CA7EB9B, 0xFC366BD2)
{
}

const FDevSystemGuids& FDevSystemGuids::Get()
{
	static FDevSystemGuids Guids;
	return Guids;
}

FGuid FDevSystemGuids::GetSystemGuid(FGuid System)
{
	return GetSystemGuidRegistry().GetSystemGuid(System);
}

void FDevVersionRegistration::RecordDevVersion(FGuid Key)
{
#if !UE_BUILD_SHIPPING
	GDevVersions.Add(Key);
#endif
}
void FDevVersionRegistration::DumpVersionsToLog()
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogDevObjectVersion, Log, TEXT("Number of dev versions registered: %d"), GDevVersions.Num());
	for (FGuid& Guid : GDevVersions)
	{
		FCustomVersion Version = FCurrentCustomVersions::Get(Guid).GetValue();
		UE_LOG(LogDevObjectVersion, Log, TEXT("  %s (%s): %d"), *Version.GetFriendlyName().ToString(), *Version.Key.ToString(EGuidFormats::DigitsWithHyphens), Version.Version);
	}
#endif
}
// Unique Blueprints Object version id
const FGuid FBlueprintsObjectVersion::GUID(0xB0D832E4, 0x1F894F0D, 0xACCF7EB7, 0x36FD4AA2);
// Register Blueprints custom version with Core
FDevVersionRegistration GRegisterBlueprintsObjectVersion(FBlueprintsObjectVersion::GUID, FBlueprintsObjectVersion::LatestVersion, TEXT("Dev-Blueprints"));

// Unique Build Object version id
const FGuid FBuildObjectVersion::GUID(0xE1C64328, 0xA22C4D53, 0xA36C8E86, 0x6417BD8C);
// Register Build custom version with Core
FDevVersionRegistration GRegisterBuildObjectVersion(FBuildObjectVersion::GUID, FBuildObjectVersion::LatestVersion, TEXT("Dev-Build"));

// Unique Core Object version id
const FGuid FCoreObjectVersion::GUID(0x375EC13C, 0x06E448FB, 0xB50084F0, 0x262A717E);
// Register Core custom version with Core
FDevVersionRegistration GRegisterCoreObjectVersion(FCoreObjectVersion::GUID, FCoreObjectVersion::LatestVersion, TEXT("Dev-Core"));

// Unique Editor Object version id
const FGuid FEditorObjectVersion::GUID(0xE4B068ED, 0xF49442E9, 0xA231DA0B, 0x2E46BB41);
// Register Editor custom version with Core
FDevVersionRegistration GRegisterEditorObjectVersion(FEditorObjectVersion::GUID, FEditorObjectVersion::LatestVersion, TEXT("Dev-Editor"));

// Unique Framework Object version id
const FGuid FFrameworkObjectVersion::GUID(0xCFFC743F, 0x43B04480, 0x939114DF, 0x171D2073);
// Register Framework custom version with Core
FDevVersionRegistration GRegisterFrameworkObjectVersion(FFrameworkObjectVersion::GUID, FFrameworkObjectVersion::LatestVersion, TEXT("Dev-Framework"));

// Unique Mobile Object version id
const FGuid FMobileObjectVersion::GUID(0xB02B49B5, 0xBB2044E9, 0xA30432B7, 0x52E40360);
// Register Mobile custom version with Core
FDevVersionRegistration GRegisterMobileObjectVersion(FMobileObjectVersion::GUID, FMobileObjectVersion::LatestVersion, TEXT("Dev-Mobile"));

// Unique Networking Object version id
const FGuid FNetworkingObjectVersion::GUID(0xA4E4105C, 0x59A149B5, 0xA7C540C4, 0x547EDFEE);
// Register Networking custom version with Core
FDevVersionRegistration GRegisterNetworkingObjectVersion(FNetworkingObjectVersion::GUID, FNetworkingObjectVersion::LatestVersion, TEXT("Dev-Networking"));

// Unique Online Object version id
const FGuid FOnlineObjectVersion::GUID(0x39C831C9, 0x5AE647DC, 0x9A449C17, 0x3E1C8E7C);
// Register Online custom version with Core
FDevVersionRegistration GRegisterOnlineObjectVersion(FOnlineObjectVersion::GUID, FOnlineObjectVersion::LatestVersion, TEXT("Dev-Online"));

// Unique Physics Object version id
const FGuid FPhysicsObjectVersion::GUID(0x78F01B33, 0xEBEA4F98, 0xB9B484EA, 0xCCB95AA2);
// Register Physics custom version with Core
FDevVersionRegistration GRegisterPhysicsObjectVersion(FPhysicsObjectVersion::GUID, FPhysicsObjectVersion::LatestVersion, TEXT("Dev-Physics"));

// Unique Platform Object version id
const FGuid FPlatformObjectVersion::GUID(0x6631380F, 0x2D4D43E0, 0x8009CF27, 0x6956A95A);
// Register Platform custom version with Core
FDevVersionRegistration GRegisterPlatformObjectVersion(FPlatformObjectVersion::GUID, FPlatformObjectVersion::LatestVersion, TEXT("Dev-Platform"));

// Unique Rendering Object version id
const FGuid FRenderingObjectVersion::GUID(0x12F88B9F, 0x88754AFC, 0xA67CD90C, 0x383ABD29);
// Register Rendering custom version with Core
FDevVersionRegistration GRegisterRenderingObjectVersion(FRenderingObjectVersion::GUID, FRenderingObjectVersion::LatestVersion, TEXT("Dev-Rendering"));

// Unique Sequencer Object version id
const FGuid FSequencerObjectVersion::GUID(0x7B5AE74C, 0xD2704C10, 0xA9585798, 0x0B212A5A);
// Register Sequencer custom version with Core
FDevVersionRegistration GRegisterSequencerObjectVersion(FSequencerObjectVersion::GUID, FSequencerObjectVersion::LatestVersion, TEXT("Dev-Sequencer"));

// Unique VR Object version id
const FGuid FVRObjectVersion::GUID(0xD7296918, 0x1DD64BDD, 0x9DE264A8, 0x3CC13884);
// Register VR custom version with Core
FDevVersionRegistration GRegisterVRObjectVersion(FVRObjectVersion::GUID, FVRObjectVersion::LatestVersion, TEXT("Dev-VR"));

// Unique Load Times version id
const FGuid FLoadTimesObjectVersion::GUID(0xC2A15278, 0xBFE74AFE, 0x6C1790FF, 0x531DF755);
// Register LoadTimes custom version with Core
FDevVersionRegistration GRegisterLoadTimesObjectVersion(FLoadTimesObjectVersion::GUID, FLoadTimesObjectVersion::LatestVersion, TEXT("Dev-LoadTimes"));

// Unique Geometry Object version id
const FGuid FGeometryObjectVersion::GUID(0x6EACA3D4, 0x40EC4CC1, 0xb7868BED, 0x9428FC5);
// Register Geometry custom version with Core
FDevVersionRegistration GRegisterGeometryObjectVersion(FGeometryObjectVersion::GUID, FGeometryObjectVersion::LatestVersion, TEXT("Private-Geometry"));

// Unique AnimPhys Object version id
const FGuid FAnimPhysObjectVersion::GUID(0x29E575DD, 0xE0A34627, 0x9D10D276, 0x232CDCEA);
// Register AnimPhys custom version with Core
FDevVersionRegistration GRegisterAnimPhysObjectVersion(FAnimPhysObjectVersion::GUID, FAnimPhysObjectVersion::LatestVersion, TEXT("Dev-AnimPhys"));

// Unique Anim Object version id
const FGuid FAnimObjectVersion::GUID(0xAF43A65D, 0x7FD34947, 0x98733E8E, 0xD9C1BB05);
// Register AnimPhys custom version with Core
FDevVersionRegistration GRegisterAnimObjectVersion(FAnimObjectVersion::GUID, FAnimObjectVersion::LatestVersion, TEXT("Dev-Anim"));

// Unique ReflectionCapture Object version id
const FGuid FReflectionCaptureObjectVersion::GUID(0x6B266CEC, 0x1EC74B8F, 0xA30BE4D9, 0x0942FC07);
// Register Rendering custom version with Core
FDevVersionRegistration GRegisterReflectionCaptureObjectVersion(FReflectionCaptureObjectVersion::GUID, FReflectionCaptureObjectVersion::LatestVersion, TEXT("Dev-ReflectionCapture"));

// Unique Automation Object version id
const FGuid FAutomationObjectVersion::GUID(0x0DF73D61, 0xA23F47EA, 0xB72789E9, 0x0C41499A);
// Register Automation custom version with Core
FDevVersionRegistration GRegisterAutomationObjectVersion(FAutomationObjectVersion::GUID, FAutomationObjectVersion::LatestVersion, TEXT("Dev-Automation"));

// Unique Fortnite Main Object version id
const FGuid FFortniteMainBranchObjectVersion::GUID(0x601D1886, 0xAC644F84, 0xAA16D3DE, 0x0DEAC7D6);
// Register Fortnite Main custom version with Core
FDevVersionRegistration GRegisterFortniteMainBranchObjectVersion(FFortniteMainBranchObjectVersion::GUID, FFortniteMainBranchObjectVersion::LatestVersion, TEXT("FortniteMain"));
FDevSystemGuidRegistration GRegisterFortniteMainBranchSystemGuids(FFortniteMainBranchObjectVersion::GetSystemGuids());

// Unique Fortnite Dev-Valkyrie Object version id
const FGuid FFortniteValkyrieBranchObjectVersion::GUID(0x8DBC2C5B, 0x54A743E0, 0xA768FCBB, 0x7DA29060);
// Register Fortnite Main custom version with Core
FDevVersionRegistration GRegisterFortniteValkyrieBranchObjectVersion(FFortniteValkyrieBranchObjectVersion::GUID, FFortniteValkyrieBranchObjectVersion::LatestVersion, TEXT("FortniteValkyrie"));

// Unique Fortnite Season Object version id
const FGuid FFortniteSeasonBranchObjectVersion::GUID(0x5B4C06B7, 0x24634AF8, 0x805BBF70, 0xCDF5D0DD);
// Register Fortnite Season version with Core
FDevVersionRegistration GRegisterFortniteSeasonBranchObjectVersion(FFortniteSeasonBranchObjectVersion::GUID, FFortniteSeasonBranchObjectVersion::LatestVersion, TEXT("FortniteSeason"));
FDevSystemGuidRegistration GRegisterFortniteSeasonBranchSystemGuids(FFortniteSeasonBranchObjectVersion::GetSystemGuids());

// Register Fortnite Shaderwork custom version with Core
FDevSystemGuidRegistration GRegisterFortniteShaderworkBranchSystemGuids(FFortniteShaderworkObjectVersion::GetSystemGuids());

// Unique Fortnite Release Object version id
const FGuid FFortniteReleaseBranchCustomObjectVersion::GUID(0xE7086368, 0x6B234C58, 0x84391B70, 0x16265E91);
// Register Fortnite Release custom version with Core
FDevVersionRegistration GRegisterFortniteReleaseBranchCustomObjectVersion(FFortniteReleaseBranchCustomObjectVersion::GUID, FFortniteReleaseBranchCustomObjectVersion::LatestVersion, TEXT("FortniteRelease"));
FDevSystemGuidRegistration GRegisterFortniteReleaseBranchSystemGuids(FFortniteReleaseBranchCustomObjectVersion::GetSystemGuids());

// Unique Enterprise Object version id
const FGuid FEnterpriseObjectVersion::GUID(0x9DFFBCD6, 0x494F0158, 0xE2211282, 0x3C92A888);
// Register Enterprise custom version with Core
FDevVersionRegistration GRegisterEnterpriseObjectVersion(FEnterpriseObjectVersion::GUID, FEnterpriseObjectVersion::LatestVersion, TEXT("Dev-Enterprise"));

// Unique Niagara Object version id
const FGuid FNiagaraObjectVersion::GUID(0xF2AED0AC, 0x9AFE416F, 0x8664AA7F, 0xFA26D6FC);
// Register Niagara custom version with Core
FDevVersionRegistration GRegisterNiagaraObjectVersion(FNiagaraObjectVersion::GUID, FNiagaraObjectVersion::LatestVersion, TEXT("Dev-Niagara"));

// Unique Destruction Object version id
const FGuid FDestructionObjectVersion::GUID(0x174F1F0B, 0xB4C645A5, 0xB13F2EE8, 0xD0FB917D);
// Register Destruction custom version with Core
FDevVersionRegistration GRegisterDestructionObjectVersion(FDestructionObjectVersion::GUID, FDestructionObjectVersion::LatestVersion, TEXT("Dev-Destruction"));

// Unique Physics Object version id
const FGuid FExternalPhysicsCustomObjectVersion::GUID(0x35F94A83, 0xE258406C, 0xA31809F5, 0x9610247C);
// Register Physics custom version with Core
FDevVersionRegistration GRegisterExternalPhysicsCustomVersion(FExternalPhysicsCustomObjectVersion::GUID, FExternalPhysicsCustomObjectVersion::LatestVersion, TEXT("Dev-Physics-Ext"));
// Unique Physics Object version id

// Unique Physics Material Object version id
const FGuid FExternalPhysicsMaterialCustomObjectVersion::GUID(0xB68FC16E, 0x8B1B42E2, 0xB453215C, 0x058844FE);
// Register Physics custom version with Core
FDevVersionRegistration GRegisterExternalPhysicsMaterialCustomVersion(FExternalPhysicsMaterialCustomObjectVersion::GUID, FExternalPhysicsMaterialCustomObjectVersion::LatestVersion, TEXT("Dev-PhysicsMaterial-Chaos"));
// Unique PhysicsMaterial  Object version id

// Unique CineCamera Object version id
const FGuid FCineCameraObjectVersion::GUID(0xB2E18506, 0x4273CFC2, 0xA54EF4BB, 0x758BBA07);
// Register CineCamera custom version with Core
FDevVersionRegistration GRegisterCineCameraObjectVersion(FCineCameraObjectVersion::GUID, FCineCameraObjectVersion::LatestVersion, TEXT("Dev-CineCamera"));

// Unique VirtualProduction Object version id
const FGuid FVirtualProductionObjectVersion::GUID(0x64F58936, 0xFD1B42BA, 0xBA967289, 0xD5D0FA4E);
// Register VirtualProduction custom version with Core
FDevVersionRegistration GRegisterVirtualProductionObjectVersion(FVirtualProductionObjectVersion::GUID, FVirtualProductionObjectVersion::LatestVersion, TEXT("Dev-VirtualProduction"));

// Unique UE5 version id
const FGuid FUE5MainStreamObjectVersion::GUID(0x697DD581, 0xE64f41AB, 0xAA4A51EC, 0xBEB7B628);
// Register UE5 main stream custom version with Core
FDevVersionRegistration GRegisterUE5MainStreamObjectVersion(FUE5MainStreamObjectVersion::GUID, FUE5MainStreamObjectVersion::LatestVersion, TEXT("UE5-Main"));
FDevSystemGuidRegistration GRegisterUE5MainBranchSystemGuids(FUE5MainStreamObjectVersion::GetSystemGuids());

// Unique UE5 release version id
const FGuid FUE5ReleaseStreamObjectVersion::GUID(0xD89B5E42, 0x24BD4D46, 0x8412ACA8, 0xDF641779);
// Register UE5 release stream custom version with Core
FDevVersionRegistration GRegisterUE5ReleaseStreamObjectVersion(FUE5ReleaseStreamObjectVersion::GUID, FUE5ReleaseStreamObjectVersion::LatestVersion, TEXT("UE5-Release"));
FDevSystemGuidRegistration GRegisterUE5ReleaseBranchSystemGuids(FUE5ReleaseStreamObjectVersion::GetSystemGuids());

// Unique UE5 private frosty version id
const FGuid FUE5PrivateFrostyStreamObjectVersion::GUID(0x59DA5D52, 0x12324948, 0xB8785978, 0x70B8E98B);
// Register UE5 private frosty stream custom version with Core
FDevVersionRegistration GRegisterUE5PrivateFrostyStreamObjectVersion(FUE5PrivateFrostyStreamObjectVersion::GUID, FUE5PrivateFrostyStreamObjectVersion::LatestVersion, TEXT("UE5-PrivateFrosty"));

// Unique Cooker version id
const FGuid FUE5CookerObjectVersion::GUID(0x26075A32, 0x730F4708, 0x88E98C32, 0xF1599D05);
// Register Cooker custom version with Core
FDevVersionRegistration GRegisterCookerObjectVersion(FUE5CookerObjectVersion::GUID, FUE5CookerObjectVersion::LatestVersion, TEXT("UE5-Dev-Cooker"));

// Unique MediaFramework Object version id
const FGuid FMediaFrameworkObjectVersion::GUID(0x6f0ed827, 0xa6094895, 0x9c91998d, 0x90180ea4);
// Register MediaFramework custom version with Core
FDevVersionRegistration GRegisterMediaFrameworkObjectVersion(FMediaFrameworkObjectVersion::GUID, FMediaFrameworkObjectVersion::LatestVersion, TEXT("Dev-MediaFramework"));

const FGuid FUE5LWCRenderingStreamObjectVersion::GUID(0x30D58BE3, 0x95EA4282, 0xA6E3B159, 0xD8EBB06A);
// Register MediaFramework custom version with Core
FDevVersionRegistration GRegisterLWCRenderingStreamObjectVersion(FUE5LWCRenderingStreamObjectVersion::GUID, FUE5LWCRenderingStreamObjectVersion::LatestVersion, TEXT("UE5-Dev-LWCRendering"));
