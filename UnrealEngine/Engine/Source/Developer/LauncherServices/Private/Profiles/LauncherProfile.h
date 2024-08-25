// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Modules/ModuleManager.h"
#include "ILauncherProfile.h"
#include "ILauncherServicesModule.h"
#include "Misc/Paths.h"
#include "Launcher/LauncherProjectPath.h"
#include "Misc/CommandLine.h"
#include "Internationalization/Culture.h"
#include "Misc/App.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "ITargetDeviceProxy.h"
#include "ITargetDeviceServicesModule.h"
#include "GameProjectHelper.h"
#include "Profiles/LauncherProfileLaunchRole.h"
#include "PlatformInfo.h"
#include "TargetReceipt.h"
#include "DesktopPlatformModule.h"
#include "GameProjectHelper.h"

DEFINE_LOG_CATEGORY_STATIC(LogLauncherProfile, Log, All);

class Error;

enum ELauncherVersion
{
	LAUNCHERSERVICES_MINPROFILEVERSION=10,
	LAUNCHERSERVICES_ADDEDINCREMENTALDEPLOYVERSION=11,
	LAUNCHERSERVICES_ADDEDPATCHSOURCECONTENTPATH=12,
	LAUNCHERSERVICES_ADDEDRELEASEVERSION=13,
	LAUNCHERSERVICES_REMOVEDPATCHSOURCECONTENTPATH=14,
	LAUNCHERSERVICES_ADDEDDLCINCLUDEENGINECONTENT=15,
	LAUNCHERSERVICES_ADDEDGENERATECHUNKS=16,
	LAUNCHERSERVICES_ADDEDNUMCOOKERSTOSPAWN=17,
	LAUNCHERSERVICES_ADDEDSKIPCOOKINGEDITORCONTENT=18,
	LAUNCHERSERVICES_ADDEDDEFAULTDEPLOYPLATFORM=19,
	LAUNCHERSERVICES_FIXCOMPRESSIONSERIALIZE=20,
	LAUNCHERSERVICES_SHAREABLEPROJECTPATHS = 21,
	LAUNCHERSERVICES_FILEFORMATCHANGE = 22,
	LAUNCHERSERVICES_ADDARCHIVE = 23,
	LAUNCHERSERVICES_ADDEDENCRYPTINIFILES = 24,
	LAUNCHERSERVICES_ADDEDMULTILEVELPATCHING = 25,
	LAUNCHERSERVICES_ADDEDADDITIONALCOMMANDLINE = 26,
	LAUNCHERSERVICES_ADDEDINCLUDEPREREQUISITES = 27,
	LAUNCHERSERVICES_ADDEDBUILDMODE = 28,
	LAUNCHERSERVICES_ADDEDUSEIOSTORE = 29,
	LAUNCHERSERVICES_ADDEDMAKEBINARYCONFIG = 30,
	LAUNCHERSERVICES_ADDEDREFERENCECONTAINERS = 31,
	LAUNCHERSERVICES_REMOVEDNUMCOOKERSTOSPAWN = 32,
	LAUNCHERSERVICES_ADDEDORIGINALRELEASEVERSION = 33,
	LAUNCHERSERVICES_ADDBUILDTARGETNAME = 34,
	LAUNCHERSERVICED_ADDEDRETAINSTAGEDDIRECTORY = 35,
	LAUNCHERSERVICED_REMOVEDRETAINSTAGEDDIRECTORY = 36,
	//ADD NEW STUFF HERE


	LAUNCHERSERVICES_FINALPLUSONE,
	LAUNCHERSERVICES_FINAL = LAUNCHERSERVICES_FINALPLUSONE-1,
};

enum ESimpleLauncherVersion
{
	LAUNCHERSERVICES_SIMPLEPROFILEVERSION=1,
	LAUNCHERSERVICES_SIMPLEFILEFORMATCHANGE = 2,
};

LAUNCHERSERVICES_API bool HasPromotedTarget(const TCHAR* BaseDir, const TCHAR* TargetName, const TCHAR* Platform, EBuildConfiguration Configuration, const TCHAR* Architecture);

inline bool TryGetDefaultTargetName(const FString& ProjectFile, EBuildTargetType TargetType, FString& OutTargetName)
{
	const TArray<FTargetInfo>& Targets = FDesktopPlatformModule::Get()->GetTargetsForProject(ProjectFile);
	for (const FTargetInfo& Target : Targets)
	{
		if (Target.Type == TargetType && Target.DefaultTarget.Get(true))
		{
			OutTargetName = Target.Name;
			return true;
		}
	}
	return false;
}

/**
* Implements a simple profile which controls the desired output of the Launcher for simple
*/
class FLauncherSimpleProfile final
	: public ILauncherSimpleProfile
{
public:

	FLauncherSimpleProfile(const FString& InDeviceName)
		: DeviceName(InDeviceName)
	{
		SetDefaults();
	}

	//~ Begin ILauncherSimpleProfile Interface

	virtual const FString& GetDeviceName() const override
	{
		return DeviceName;
	}

	virtual FName GetDeviceVariant() const  override
	{
		return Variant;
	}

	virtual EBuildConfiguration GetBuildConfiguration() const override
	{
		return BuildConfiguration;
	}

	virtual ELauncherProfileCookModes::Type GetCookMode() const override
	{
		return CookMode;
	}

	virtual void SetDeviceName(const FString& InDeviceName) override
	{
		if (DeviceName != InDeviceName)
		{
			DeviceName = InDeviceName;
		}
	}

	virtual void SetDeviceVariant(FName InVariant) override
	{
		Variant = InVariant;
	}

	virtual void SetBuildConfiguration(EBuildConfiguration InConfiguration) override
	{
		BuildConfiguration = InConfiguration;
	}

	virtual void SetCookMode(ELauncherProfileCookModes::Type InMode) override
	{
		CookMode = InMode;
	}

	virtual bool Serialize(FArchive& Archive) override
	{
		int32 Version = LAUNCHERSERVICES_SIMPLEPROFILEVERSION;

		Archive << Version;

		if (Version != LAUNCHERSERVICES_SIMPLEPROFILEVERSION)
		{
			return false;
		}

		// IMPORTANT: make sure to bump LAUNCHERSERVICES_SIMPLEPROFILEVERSION when modifying this!
		Archive << DeviceName
			<< Variant
			<< BuildConfiguration
			<< CookMode;

		return true;
	}

	virtual void Save(TJsonWriter<>& Writer) override
	{
		int32 Version = LAUNCHERSERVICES_SIMPLEFILEFORMATCHANGE;

		Writer.WriteObjectStart();
		Writer.WriteValue(TEXT("Version"), Version);
		Writer.WriteValue(TEXT("DeviceName"), DeviceName);
		Writer.WriteValue(TEXT("Variant"), Variant.ToString());
		Writer.WriteValue(TEXT("BuildConfiguration"), (int32)BuildConfiguration);
		Writer.WriteValue(TEXT("CookMode"), CookMode);
		Writer.WriteObjectEnd();
	}

	virtual bool Load(const FJsonObject& Object) override
	{
		int32 Version = (int32)Object.GetNumberField(TEXT("Version"));
		if (Version < LAUNCHERSERVICES_SIMPLEFILEFORMATCHANGE)
		{
			return false;
		}

		DeviceName = Object.GetStringField(TEXT("DeviceName"));
		Variant = *(Object.GetStringField(TEXT("Variant")));
		BuildConfiguration = (EBuildConfiguration)((int32)Object.GetNumberField(TEXT("BuildConfiguration")));
		CookMode = (TEnumAsByte<ELauncherProfileCookModes::Type>)((int32)Object.GetNumberField(TEXT("CookMode")));

		return true;
	}

	virtual void SetDefaults() override
	{
		// None will mean the preferred Variant for the device is used.
		Variant = NAME_None;
		
		// I don't use FApp::GetBuildConfiguration() because i don't want the act of running in debug the first time to cause the simple
		// profiles created for your persistent devices to be in debug. The user might not see this if they don't expand the Advanced options.
		BuildConfiguration = EBuildConfiguration::Development;
		
		CookMode = ELauncherProfileCookModes::OnTheFly;
	}

private:

	// Holds the name of the device this simple profile is for
	FString DeviceName;

	// Holds the name of the device variant.
	FName Variant;

	// Holds the desired build configuration (only used if creating new builds).
	EBuildConfiguration BuildConfiguration;

	// Holds the cooking mode.
	TEnumAsByte<ELauncherProfileCookModes::Type> CookMode;
};


/**
 * Implements a profile which controls the desired output of the Launcher
 */
class FLauncherProfile final
	: public ILauncherProfile
{
public:

	/**
	* Gets the folder in which profile files are stored.
	*
	* @return The folder path.
	*/
	static FString GetProfileFolder(bool bNotForLicensees)
	{
		if (bNotForLicensees)
		{
			return FPaths::EngineDir() / TEXT("Restricted/NotForLicensees/Programs/UnrealFrontend/Profiles");
		}
		return FPaths::EngineDir() / TEXT("Programs/UnrealFrontend/Profiles");
	}

	/**
	 * Default constructor.
	 */
	FLauncherProfile(ILauncherProfileManagerRef ProfileManager)
		: LauncherProfileManager(ProfileManager)
		, DefaultLaunchRole(MakeShareable(new FLauncherProfileLaunchRole()))
	{ 
		SetDefaults();
		ProjectChangedDelegate.AddRaw(this, &FLauncherProfile::OnSelectedProjectChanged);
	}

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InProfileName - The name of the profile.
	 */
	FLauncherProfile(ILauncherProfileManagerRef ProfileManager, FGuid InId, const FString& InProfileName)
		: LauncherProfileManager(ProfileManager)
		, DefaultLaunchRole(MakeShareable(new FLauncherProfileLaunchRole()))
		, Id(InId)
		, Name(InProfileName)
	{
		SetDefaults();
		ProjectChangedDelegate.AddRaw(this, &FLauncherProfile::OnSelectedProjectChanged);
	}

	/**
	 * Destructor.
	 */
	~FLauncherProfile( ) 
	{
		if (DeployedDeviceGroup.IsValid())
		{
			DeployedDeviceGroup->OnDeviceAdded().Remove(OnLauncherDeviceGroupDeviceAddedDelegateHandle);
			DeployedDeviceGroup->OnDeviceRemoved().Remove(OnLauncherDeviceGroupDeviceRemoveDelegateHandle);
		}
	}

	/**
	 * Gets the identifier of the device group to deploy to.
	 *
	 * This method is used internally by the profile manager to read the device group identifier after
	 * loading this profile from a file. The profile manager will use this identifier to locate the
	 * actual device group to deploy to.
	 *
	 * @return The device group identifier, or an invalid GUID if no group was set or deployment is disabled.
	 */
	const FGuid& GetDeployedDeviceGroupId( ) const
	{
		return DeployedDeviceGroupId;
	}

	//~ Begin ILauncherProfile Interface

	virtual void AddCookedCulture( const FString& CultureName ) override
	{
		CookedCultures.AddUnique(CultureName);

		Validate();
	}

	virtual void AddCookedMap( const FString& MapName ) override
	{
		CookedMaps.AddUnique(MapName);

		Validate();
	}

	virtual void AddCookedPlatform( const FString& PlatformName ) override
	{
		CookedPlatforms.AddUnique(PlatformName);

		RefreshValidBuildTargets();
		Validate();
	}

	virtual void SetDefaultDeployPlatform(const FName PlatformName) override
	{
		
		DefaultDeployPlatform = PlatformName;	

		if (DeployedDeviceGroup.IsValid())
		{
			DeployedDeviceGroup->RemoveAllDevices();

			if (DefaultDeployPlatform != NAME_None)
			{
				TArray<TSharedPtr<ITargetDeviceProxy>> PlatformDeviceProxies;
				ITargetDeviceServicesModule& TargetDeviceServicesModule = FModuleManager::LoadModuleChecked<ITargetDeviceServicesModule>("TargetDeviceServices");
				const TSharedRef<ITargetDeviceProxyManager>& InDeviceProxyManager = TargetDeviceServicesModule.GetDeviceProxyManager();

				InDeviceProxyManager->GetProxies(NAME_None, true, PlatformDeviceProxies);

				TSharedPtr<ITargetDeviceProxy> DefaultPlatformDevice;
				for (int32 ProxyIndex = 0; ProxyIndex < PlatformDeviceProxies.Num(); ++ProxyIndex)
				{
					TSharedPtr<ITargetDeviceProxy> DeviceProxy = PlatformDeviceProxies[ProxyIndex];

					if (DeviceProxy->GetVanillaPlatformId(NAME_None) == DefaultDeployPlatform)
					{
						DefaultPlatformDevice = DeviceProxy;
						break;
					}
				}

				if (DefaultPlatformDevice.IsValid())
				{
					DeployedDeviceGroup->AddDevice(DefaultPlatformDevice->GetTargetDeviceId(NAME_None));
				}
			}
		}

		Validate();
	}

	virtual void ClearCookedCultures( ) override
	{
		if (CookedCultures.Num() > 0)
		{
			CookedCultures.Reset();

			Validate();
		}
	}

	virtual void ClearCookedMaps( ) override
	{
		if (CookedMaps.Num() > 0)
		{
			CookedMaps.Reset();

			Validate();
		}
	}

	virtual void ClearCookedPlatforms( ) override
	{
		if (CookedPlatforms.Num() > 0)
		{
			CookedPlatforms.Reset();

			RefreshValidBuildTargets();
			Validate();
		}
	}

	virtual ILauncherProfileLaunchRolePtr CreateLaunchRole( ) override
	{
		ILauncherProfileLaunchRolePtr Role = MakeShareable(new FLauncherProfileLaunchRole());
			
		LaunchRoles.Add(Role);

		Validate();

		return Role;
	}

	virtual EBuildConfiguration GetBuildConfiguration( ) const override
	{
		return BuildConfiguration;
	}

	virtual bool HasBuildTargetSpecified() const override
	{
		return BuildTargetSpecified;
	}

	virtual FString GetBuildTarget() const override
	{
		if (BuildTargetSpecified)
		{
			return BuildTargetName;
		}
		else
		{
			return LauncherProfileManager->GetBuildTarget();
		}
	}

	virtual EBuildConfiguration GetCookConfiguration( ) const override
	{
		return CookConfiguration;
	}

	virtual ELauncherProfileCookModes::Type GetCookMode( ) const override
	{
		return CookMode;
	}

	virtual const FString& GetCookOptions( ) const override
	{
		return CookOptions;
	}

	virtual const TArray<FString>& GetCookedCultures( ) const override
	{
		return CookedCultures;
	}

	virtual const bool GetSkipCookingEditorContent() const override
	{
		return bSkipCookingEditorContent;
	}

	virtual const TArray<FString>& GetCookedMaps( ) const override
	{
		return CookedMaps;
	}

	virtual const TArray<FString>& GetCookedPlatforms( ) const override
	{
		return CookedPlatforms;
	}

	virtual const ILauncherProfileLaunchRoleRef& GetDefaultLaunchRole( ) const override
	{
		return DefaultLaunchRole;
	}

	virtual ILauncherDeviceGroupPtr GetDeployedDeviceGroup( ) override
	{
		// setting the default platform will update the device group.  always do this when getting the group because
		// devices come in lazily through messages and can't be added properly at profile load.
		if (DefaultDeployPlatform != NAME_None)
		{
			SetDefaultDeployPlatform(DefaultDeployPlatform);
		}
		return DeployedDeviceGroup;
	}

	virtual const FName GetDefaultDeployPlatform() const override
	{
		return DefaultDeployPlatform;
	}

	virtual bool IsGeneratingPatch() const override
	{
		return GeneratePatch;
	}

	virtual bool ShouldAddPatchLevel() const override
	{
		return AddPatchLevel;
	}

	virtual bool ShouldStageBaseReleasePaks() const override
	{
		return StageBaseReleasePaks;
	}

	virtual bool IsCreatingDLC() const override
	{
		return CreateDLC;
	}
	virtual void SetCreateDLC(bool InBuildDLC) override
	{
		CreateDLC = InBuildDLC;
	}

	virtual FString GetDLCName() const override
	{
		return DLCName;
	}
	virtual void SetDLCName(const FString& InDLCName) override
	{
		DLCName = InDLCName;
	}

	virtual bool IsDLCIncludingEngineContent() const
	{
		return DLCIncludeEngineContent;
	}
	virtual void SetDLCIncludeEngineContent(bool InDLCIncludeEngineContent)
	{
		DLCIncludeEngineContent = InDLCIncludeEngineContent;
	}



	virtual bool IsCreatingReleaseVersion() const override
	{
		return CreateReleaseVersion;
	}

	virtual void SetCreateReleaseVersion(bool InCreateReleaseVersion) override
	{
		CreateReleaseVersion = InCreateReleaseVersion;
	}

	virtual FString GetCreateReleaseVersionName() const override
	{
		return CreateReleaseVersionName;
	}

	virtual void SetCreateReleaseVersionName(const FString& InCreateReleaseVersionName) override
	{
		CreateReleaseVersionName = InCreateReleaseVersionName;
	}


	virtual FString GetBasedOnReleaseVersionName() const override
	{
		return BasedOnReleaseVersionName;
	}

	virtual void SetBasedOnReleaseVersionName(const FString& InBasedOnReleaseVersionName) override
	{
		BasedOnReleaseVersionName = InBasedOnReleaseVersionName;
	}

	virtual FString GetOriginalReleaseVersionName() const override
	{
		return OriginalReleaseVersionName; 
	}

	virtual void SetOriginalReleaseVersionName(const FString& InOriginalReleaseVersionName) override
	{
		OriginalReleaseVersionName = InOriginalReleaseVersionName;
	}

	virtual FString GetReferenceContainerGlobalFileName() const override
	{
		return ReferenceContainerGlobalFileName;
	}
	virtual void SetReferenceContainerGlobalFileName(const FString& InReferenceContainerGlobalFileName) override
	{
		ReferenceContainerGlobalFileName = InReferenceContainerGlobalFileName;
	}
	virtual FString GetReferenceContainerCryptoKeysFileName() const override
	{
		return ReferenceContainerCryptoKeysFileName;
	}
	virtual void SetReferenceContainerCryptoKeysFileName(const FString& InReferenceContainerCryptoKeysFileName) override
	{
		ReferenceContainerCryptoKeysFileName = InReferenceContainerCryptoKeysFileName;
	}

	virtual ELauncherProfileDeploymentModes::Type GetDeploymentMode( ) const override
	{
		return DeploymentMode;
	}

    virtual bool GetForceClose() const override
    {
        return ForceClose;
    }
    
	virtual FGuid GetId( ) const override
	{
		return Id;
	}

	virtual FString GetFileName() const override
	{
		//toupper for filename so that filepaths can be compared the same on case sensitive and case-insensitive platforms
		return GetName().ToUpper() + TEXT("_") + GetId().ToString() + TEXT(".ulp2");
	}

	virtual FString GetFilePath() const override
	{
		return GetProfileFolder(bNotForLicensees) / GetFileName();
	}

	virtual ELauncherProfileLaunchModes::Type GetLaunchMode( ) const override
	{
		return LaunchMode;
	}

	virtual const TArray<ILauncherProfileLaunchRolePtr>& GetLaunchRoles( ) const override
	{
		return LaunchRoles;
	}

	virtual const int32 GetLaunchRolesFor( const FString& DeviceId, TArray<ILauncherProfileLaunchRolePtr>& OutRoles ) override
	{
		OutRoles.Empty();

		if (LaunchMode == ELauncherProfileLaunchModes::CustomRoles)
		{
			for (TArray<ILauncherProfileLaunchRolePtr>::TConstIterator It(LaunchRoles); It; ++It)
			{
				ILauncherProfileLaunchRolePtr Role = *It;

				if (Role->GetAssignedDevice() == DeviceId)
				{
					OutRoles.Add(Role);
				}
			}
		}
		else if (LaunchMode == ELauncherProfileLaunchModes::DefaultRole)
		{
			OutRoles.Add(DefaultLaunchRole);
		}

		return OutRoles.Num();
	}

	virtual FString GetName( ) const override
	{
		return Name;
	}

	virtual FString GetDescription() const override
	{
		return Description;
	}

	virtual ELauncherProfilePackagingModes::Type GetPackagingMode( ) const override
	{
		return PackagingMode;
	}

	virtual FString GetPackageDirectory( ) const override
	{
		return PackageDir;
	}

	virtual bool IsArchiving( ) const override
	{
		return bArchive;
	}

	virtual FString GetArchiveDirectory( ) const override
	{
		return ArchiveDir;
	}

	virtual bool HasProjectSpecified() const override
	{
		return ProjectSpecified;
	}

	virtual FString GetProjectName() const override
	{
		if (ProjectSpecified)
		{
			FString Path = FLauncherProjectPath::GetProjectName(FullProjectPath);
			return Path;
		}
		return LauncherProfileManager->GetProjectName();
	}

	virtual FString GetProjectBasePath() const override
	{
		if (ProjectSpecified)
		{
			return FLauncherProjectPath::GetProjectBasePath(FullProjectPath);
		}
		return LauncherProfileManager->GetProjectBasePath();
	}

	virtual FString GetProjectPath() const override
	{
		if (ProjectSpecified)
		{
			return FullProjectPath;
		}
		return LauncherProfileManager->GetProjectPath();
	}

    virtual uint32 GetTimeout() const override
    {
        return Timeout;
    }

	virtual bool HasValidationError() const override
	{
		return ValidationErrors.Num() > 0;
	}
    
	virtual bool HasValidationError( ELauncherProfileValidationErrors::Type Error ) const override
	{
		return ValidationErrors.Contains(Error);
	}

	virtual FString GetInvalidPlatform() const override
	{
		return InvalidPlatform;
	}

	virtual ELauncherProfileBuildModes::Type GetBuildMode() const override
	{
		return BuildMode;
	}

	virtual bool ShouldBuild() override
	{
		bool bBuild = true;
		if (BuildMode == ELauncherProfileBuildModes::DoNotBuild)
		{
			bBuild = false;
		}
		else if (BuildMode == ELauncherProfileBuildModes::Auto)
		{
			if (FApp::GetEngineIsPromotedBuild())
			{
				bBuild = false;

				TArray<FString> TargetPlatformNames = FindPlatforms();
				for (const FString& TargetPlatformName : TargetPlatformNames)
				{
					// Get the target we're building for
					const ITargetPlatform* TargetPlatform = GetTargetPlatformManager()->FindTargetPlatform(TargetPlatformName);
					const PlatformInfo::FTargetPlatformInfo& PlatformInfo = TargetPlatform->GetTargetPlatformInfo();

					// Figure out which target we're building
					FString ReceiptDir;
					FString TargetName;
					if (TryGetDefaultTargetName(FPaths::GetProjectFilePath(), PlatformInfo.PlatformType, TargetName))
					{
						ReceiptDir = FPaths::GetPath(FPaths::GetProjectFilePath());
					}
					else if (TryGetDefaultTargetName(FString(), PlatformInfo.PlatformType, TargetName))
					{
						FText Reason;
						if (TargetPlatform->RequiresTempTarget(false, BuildConfiguration, false, Reason))
						{
							UE_LOG(LogLauncherProfile, Log, TEXT("Project requires temp target (%s)"), *Reason.ToString());
							ReceiptDir = FPaths::GetPath(FPaths::GetProjectFilePath());
						}
						else
						{
							UE_LOG(LogLauncherProfile, Log, TEXT("Project does not require temp target"));
							ReceiptDir = FPaths::EngineDir();
						}
					}
					else
					{
						UE_LOG(LogLauncherProfile, Log, TEXT("Unable to find any targets for platform %s - forcing build"), *TargetPlatformName);
						bBuild = true;
						break;
					}

					// Check if the existing target is valid
					FString BuildPlatform = PlatformInfo.DataDrivenPlatformInfo->UBTPlatformString;
					if (!HasPromotedTarget(*ReceiptDir, *TargetName, *BuildPlatform, BuildConfiguration, nullptr))
					{
						bBuild = true;
						break;
					}
				}
			}
		}
		return bBuild;
	}

	virtual bool IsBuildingUAT() const override
	{
		return BuildUAT;
	}

	virtual FString GetAdditionalCommandLineParameters() const override
	{
		return AdditionalCommandLineParameters;
	}

	virtual bool IsCookingIncrementally( ) const override
	{
		if ( CookMode != ELauncherProfileCookModes::DoNotCook )
		{
			return CookIncremental;
		}
		return false;
	}

	virtual bool IsIterateSharedCookedBuild() const override
	{
		if ( CookMode != ELauncherProfileCookModes::DoNotCook )
		{
			return IterateSharedCookedBuild;
		}
		return false;
	}

	virtual bool IsCompressed() const override
	{
		return Compressed;
	}

	virtual bool IsEncryptingIniFiles() const override
	{
		return EncryptIniFiles;
	}

	virtual bool IsForDistribution() const override
	{
		return ForDistribution;
	}

	virtual bool IsCookingUnversioned( ) const override
	{
		return CookUnversioned;
	}

	virtual bool IsDeployablePlatform( const FString& PlatformName ) override
	{
		if (CookMode == ELauncherProfileCookModes::ByTheBook || CookMode == ELauncherProfileCookModes::ByTheBookInEditor)
		{
			return CookedPlatforms.Contains(PlatformName);
		}

		return true;
	}

	virtual bool IsDeployingIncrementally( ) const override
	{
		return DeployIncremental;
	}

	virtual bool IsFileServerHidden( ) const override
	{
		return HideFileServerWindow;
	}

	virtual bool IsFileServerStreaming( ) const override
	{
		return DeployStreamingServer;
	}

	virtual bool IsPackingWithUnrealPak( ) const  override
	{
		return DeployWithUnrealPak;
	}

	virtual bool IsIncludingPrerequisites() const override
	{
		return IncludePrerequisites;
	}

	virtual bool IsGeneratingChunks() const override
	{
		return bGenerateChunks;
	}

	virtual bool IsGenerateHttpChunkData() const override
	{
		return bGenerateHttpChunkData;
	}

	virtual FString GetHttpChunkDataDirectory() const override
	{
		return HttpChunkDataDirectory;
	}

	virtual FString GetHttpChunkDataReleaseName() const override
	{
		return HttpChunkDataReleaseName;
	}

	virtual bool IsValidForLaunch( ) override
	{
		return (ValidationErrors.Num() == 0);
	}

	virtual void RemoveCookedCulture( const FString& CultureName ) override
	{
		CookedCultures.Remove(CultureName);

		Validate();
	}

	virtual void RemoveCookedMap( const FString& MapName ) override
	{
		CookedMaps.Remove(MapName);

		Validate();
	}

	virtual void RemoveCookedPlatform( const FString& PlatformName ) override
	{
		CookedPlatforms.Remove(PlatformName);

		RefreshValidBuildTargets();
		Validate();
	}

	virtual void RemoveLaunchRole( const ILauncherProfileLaunchRoleRef& Role ) override
	{
		LaunchRoles.Remove(Role);

		Validate();
	}

	virtual bool Serialize( FArchive& Archive ) override
	{
		int32 Version = LAUNCHERSERVICES_FINAL;

		Archive	<< Version;

		if (Version < LAUNCHERSERVICES_MINPROFILEVERSION)
		{
			return false;
		}

		if (Version > LAUNCHERSERVICES_FINAL)
		{
			return false;
		}

		if (Archive.IsSaving())
		{
			if (DeployedDeviceGroup.IsValid())
			{
				DeployedDeviceGroupId = DeployedDeviceGroup->GetId();
			}
			else
			{
				DeployedDeviceGroupId = FGuid();
			}
		}

		// IMPORTANT: make sure to bump LAUNCHERSERVICES_PROFILEVERSION when modifying this!
		bool BuildGame = false;
		Archive << Id
				<< Name
				<< Description
				<< BuildConfiguration
				<< ProjectSpecified
				<< ShareableProjectPath
				<< CookConfiguration
				<< CookIncremental
				<< CookOptions
				<< CookMode
				<< CookUnversioned
				<< CookedCultures
				<< CookedMaps
				<< CookedPlatforms
				<< DeployStreamingServer
				<< DeployWithUnrealPak
				<< DeployedDeviceGroupId
				<< DeploymentMode
				<< HideFileServerWindow
				<< LaunchMode
				<< PackagingMode
				<< PackageDir
				<< BuildGame
                << ForceClose
                << Timeout;

		if (Version >= LAUNCHERSERVICES_SHAREABLEPROJECTPATHS)
		{
			FullProjectPath = FPaths::ConvertRelativePathToFull(FPaths::RootDir(), ShareableProjectPath);
		}

		FString DeployPlatformString = DefaultDeployPlatform.ToString();
		if (Version >= LAUNCHERSERVICES_FIXCOMPRESSIONSERIALIZE)
		{
			Archive << Compressed;
		}
		if ( Version>= LAUNCHERSERVICES_ADDEDENCRYPTINIFILES)
		{
			Archive << EncryptIniFiles;
			Archive << ForDistribution;
		}
		if (Version >= LAUNCHERSERVICES_ADDEDDEFAULTDEPLOYPLATFORM)
		{
			Archive << DeployPlatformString;
		}		
		if (Version >= LAUNCHERSERVICES_ADDEDNUMCOOKERSTOSPAWN && Version < LAUNCHERSERVICES_REMOVEDNUMCOOKERSTOSPAWN)
		{
			int32 OldNumCookersToSpawn;
			Archive << OldNumCookersToSpawn;
		}
		if (Version >= LAUNCHERSERVICES_ADDEDSKIPCOOKINGEDITORCONTENT)
		{
			Archive << bSkipCookingEditorContent;
		}
		if (Version >= LAUNCHERSERVICES_ADDEDINCREMENTALDEPLOYVERSION)
		{
			Archive << DeployIncremental;
		}
		if ( Version >= LAUNCHERSERVICES_REMOVEDPATCHSOURCECONTENTPATH )
		{
			Archive << GeneratePatch;
		}
		if ( Version >= LAUNCHERSERVICES_ADDEDMULTILEVELPATCHING )
		{
			Archive << AddPatchLevel;
			Archive << StageBaseReleasePaks;
		}
		else if ( Version >= LAUNCHERSERVICES_ADDEDPATCHSOURCECONTENTPATH)
		{
			FString Temp;
			Archive << Temp;
			Archive << GeneratePatch;
		}
		if (Version >= LAUNCHERSERVICES_ADDEDDLCINCLUDEENGINECONTENT)
		{
			Archive << DLCIncludeEngineContent;
		}
		
		if ( Version >= LAUNCHERSERVICES_ADDEDRELEASEVERSION )
		{
			Archive << CreateReleaseVersion;
			Archive << CreateReleaseVersionName;
			Archive << BasedOnReleaseVersionName;
			
			Archive << CreateDLC;
			Archive << DLCName;
		}
		if (Version >= LAUNCHERSERVICES_ADDEDGENERATECHUNKS)
		{
			Archive << bGenerateChunks;
			Archive << bGenerateHttpChunkData;
			Archive << HttpChunkDataDirectory;
			Archive << HttpChunkDataReleaseName;
		}
		if (Version >= LAUNCHERSERVICES_ADDARCHIVE)
		{
			Archive << bArchive;
			Archive << ArchiveDir;
		}
		if (Version >= LAUNCHERSERVICES_ADDEDADDITIONALCOMMANDLINE)
		{
			Archive << AdditionalCommandLineParameters;
		}
		if (Version >= LAUNCHERSERVICES_ADDEDINCLUDEPREREQUISITES)
		{
			Archive << IncludePrerequisites;
		}

		if (Version >= LAUNCHERSERVICES_ADDEDBUILDMODE)
		{
			Archive << BuildMode;
		}
		else if(Archive.IsLoading())
		{
			BuildMode = BuildGame ? ELauncherProfileBuildModes::Build : ELauncherProfileBuildModes::DoNotBuild;
		}

		if (Version >= LAUNCHERSERVICES_ADDEDUSEIOSTORE)
		{
			Archive << bUseIoStore;
		}

		if (Version >= LAUNCHERSERVICES_ADDEDMAKEBINARYCONFIG)
		{
			Archive << bMakeBinaryConfig;
		}

		if (Version >= LAUNCHERSERVICES_ADDEDREFERENCECONTAINERS)
		{
			Archive << ReferenceContainerGlobalFileName;
			Archive << ReferenceContainerCryptoKeysFileName;
		}
		if (Version >= LAUNCHERSERVICES_ADDEDORIGINALRELEASEVERSION)
		{
			Archive << OriginalReleaseVersionName;
		}

		if (Version >= LAUNCHERSERVICES_ADDBUILDTARGETNAME)
		{
			Archive << BuildTargetSpecified;
			Archive << BuildTargetName;
		}

		if (Version >= LAUNCHERSERVICED_ADDEDRETAINSTAGEDDIRECTORY && Version < LAUNCHERSERVICED_REMOVEDRETAINSTAGEDDIRECTORY)
		{
			bool bRetainStagedDirectory = false;
			Archive << bRetainStagedDirectory;
		}

		DefaultLaunchRole->Serialize(Archive);

		// serialize launch roles
		if (Archive.IsLoading())
		{
			DeployedDeviceGroup.Reset();
			LaunchRoles.Reset();
		}

		int32 NumLaunchRoles = LaunchRoles.Num();

		Archive << NumLaunchRoles;

		for (int32 RoleIndex = 0; RoleIndex < NumLaunchRoles; ++RoleIndex)
		{
			if (Archive.IsLoading())
			{
				LaunchRoles.Add(MakeShareable(new FLauncherProfileLaunchRole(Archive)));				
			}
			else
			{
				LaunchRoles[RoleIndex]->Serialize(Archive);
			}
		}

		if (Archive.IsLoading())
		{
			DefaultDeployPlatform = FName(*DeployPlatformString);
		}

		if (DefaultDeployPlatform != NAME_None)
		{
			SetDefaultDeployPlatform(DefaultDeployPlatform);
		}

		if (Archive.IsLoading())
		{
			RefreshValidBuildTargets();
		}
		Validate();

		return true;
	}

	virtual void Save(TJsonWriter<>& Writer) override
	{
		int32 Version = LAUNCHERSERVICES_FINAL;

		if (DeployedDeviceGroup.IsValid())
		{
			DeployedDeviceGroupId = DeployedDeviceGroup->GetId();
		}
		else
		{
			DeployedDeviceGroupId = FGuid();
		}

		Writer.WriteObjectStart();
		Writer.WriteValue("Version", Version);
		Writer.WriteValue("Id", Id.ToString());
		Writer.WriteValue("Name", Name);
		Writer.WriteValue("Description", Description);
		Writer.WriteValue("BuildConfiguration", (int32)BuildConfiguration);
		Writer.WriteValue("ProjectSpecified", ProjectSpecified);
		Writer.WriteValue("ShareableProjectPath", ShareableProjectPath);
		Writer.WriteValue("CookConfiguration", (int32)CookConfiguration);
		Writer.WriteValue("CookIncremental", CookIncremental);
		Writer.WriteValue("CookOptions", CookOptions);
		Writer.WriteValue("CookMode", CookMode);
		Writer.WriteValue("CookUnversioned", CookUnversioned);

		// write the cultures
		if (CookedCultures.Num() > 0)
		{
			Writer.WriteArrayStart("CookedCultures");
			for (auto Value : CookedCultures)
			{
				Writer.WriteValue(Value);
			}
			Writer.WriteArrayEnd();
		}

		// write the maps
		if (CookedMaps.Num() > 0)
		{
			Writer.WriteArrayStart("CookedMaps");
			for (auto Value : CookedMaps)
			{
				Writer.WriteValue(Value);
			}
			Writer.WriteArrayEnd();
		}

		// write the platforms
		if (CookedPlatforms.Num() > 0)
		{
			Writer.WriteArrayStart("CookedPlatforms");
			for (auto Value : CookedPlatforms)
			{
				Writer.WriteValue(Value);
			}
			Writer.WriteArrayEnd();
		}

		Writer.WriteValue("DeployStreamingServer", DeployStreamingServer);
		Writer.WriteValue("DeployWithUnrealPak", DeployWithUnrealPak);
		Writer.WriteValue("DeployedDeviceGroupId", DeployedDeviceGroupId.ToString());
		Writer.WriteValue("DeploymentMode", DeploymentMode);
		Writer.WriteValue("HideFileServerWindow", HideFileServerWindow);
		Writer.WriteValue("LaunchMode", LaunchMode);
		Writer.WriteValue("PackagingMode", PackagingMode);
		Writer.WriteValue("PackageDir", PackageDir);
		Writer.WriteValue("BuildMode", BuildMode);
		Writer.WriteValue("ForceClose", ForceClose);
		Writer.WriteValue("Timeout", (int32)Timeout);
		Writer.WriteValue("Compressed", Compressed);
		Writer.WriteValue("EncryptIniFiles", EncryptIniFiles);
		Writer.WriteValue("ForDistribution", ForDistribution);
		Writer.WriteValue("DeployPlatform", DefaultDeployPlatform.ToString());
		Writer.WriteValue("SkipCookingEditorContent", bSkipCookingEditorContent);
		Writer.WriteValue("DeployIncremental", DeployIncremental);
		Writer.WriteValue("GeneratePatch", GeneratePatch);
		Writer.WriteValue("AddPatchLevel", AddPatchLevel);
		Writer.WriteValue("StageBaseReleasePaks", StageBaseReleasePaks);
		Writer.WriteValue("DLCIncludeEngineContent", DLCIncludeEngineContent);
		Writer.WriteValue("CreateReleaseVersion", CreateReleaseVersion);
		Writer.WriteValue("CreateReleaseVersionName", CreateReleaseVersionName);
		Writer.WriteValue("BasedOnReleaseVersionName", BasedOnReleaseVersionName);
		Writer.WriteValue("ReferenceContainerGlobalFileName", ReferenceContainerGlobalFileName);
		Writer.WriteValue("ReferenceContainerCryptoKeysFileName", ReferenceContainerCryptoKeysFileName);
		Writer.WriteValue("OriginalReleaseVersionName", OriginalReleaseVersionName);
		Writer.WriteValue("CreateDLC", CreateDLC);
		Writer.WriteValue("DLCName", DLCName);
		Writer.WriteValue("GenerateChunks", bGenerateChunks);
		Writer.WriteValue("GenerateHttpChunkData", bGenerateHttpChunkData);
		Writer.WriteValue("HttpChunkDataDirectory", HttpChunkDataDirectory);
		Writer.WriteValue("HttpChunkDataReleaseName", HttpChunkDataReleaseName);
		Writer.WriteValue("Archive", bArchive);
		Writer.WriteValue("ArchiveDirectory", ArchiveDir);
		Writer.WriteValue("AdditionalCommandLineParameters", AdditionalCommandLineParameters);
		Writer.WriteValue("IncludePrerequisites", IncludePrerequisites);
		Writer.WriteValue("UseIoStore", bUseIoStore);
		Writer.WriteValue("MakeBinaryConfig", bMakeBinaryConfig);
		Writer.WriteValue("BuildTargetSpecified", BuildTargetSpecified);
		Writer.WriteValue("BuildTargetName", BuildTargetName);

		// serialize the default launch role
		DefaultLaunchRole->Save(Writer, TEXT("DefaultRole"));

		// serialize the launch roles
		if (LaunchRoles.Num())
		{
			Writer.WriteArrayStart("LaunchRoles");
			for (auto Value : LaunchRoles)
			{
				Value->Save(Writer);
			}
			Writer.WriteArrayEnd();
		}

		// write out the UAT project params
		SaveUATParams(Writer);
		Writer.WriteObjectEnd();
	}

	void SaveUATParams(TJsonWriter<>& Writer)
	{
		Writer.WriteArrayStart("scripts");
		Writer.WriteObjectStart();

		TArray<FString> Platforms = FindPlatforms();

		// script to run
		Writer.WriteValue("script", TEXT("BuildCookRun"));

		// project to operate on
		Writer.WriteValue("project", ProjectSpecified ? ShareableProjectPath : "");

		// ancillary arguments
		Writer.WriteValue("noP4", true);
		Writer.WriteValue("nocompile", !IsBuildingUAT());
		Writer.WriteValue("nocompileeditor", FApp::IsEngineInstalled());
		Writer.WriteValue("unrealexe", GetEditorExe());
		Writer.WriteValue("utf8output", true);

		// client configurations
		Writer.WriteArrayStart("clientconfig");
		Writer.WriteValue(LexToString(BuildConfiguration));
		Writer.WriteArrayEnd();

		// server configurations
		Writer.WriteArrayStart("serverconfig");
		Writer.WriteValue(LexToString(BuildConfiguration));
		Writer.WriteArrayEnd();

		// platforms
		TArray<FString> ServerPlatforms;
		TArray<FString> ClientPlatforms;
		FString OptionalParams;
		bool ClosesAfterLaunch = FindAllPlatforms(ServerPlatforms, ClientPlatforms, OptionalParams);
		if (ServerPlatforms.Num() > 0)
		{
			Writer.WriteValue("server", true);
			Writer.WriteArrayStart("serverplatform");
			for (int32 Idx = 0; Idx < ServerPlatforms.Num(); Idx++)
			{
				Writer.WriteValue(ServerPlatforms[Idx]);
			}
			Writer.WriteArrayEnd();
		}

		if (ClientPlatforms.Num() > 0)
		{
			Writer.WriteArrayStart("platform");
			for (int32 Idx = 0; Idx < ClientPlatforms.Num(); Idx++)
			{
				Writer.WriteValue(ClientPlatforms[Idx]);
			}
			Writer.WriteArrayEnd();
		}

		// optional params
		TMap<FString, FString> OptionalCommands = ParseCommands(OptionalParams);
		for (TMap<FString, FString>::TIterator Iter = OptionalCommands.CreateIterator(); Iter; ++Iter)
		{
			Writer.WriteValue(Iter.Key(), Iter.Value());
		}

		// game command line
		FString InitialMap = GetDefaultLaunchRole()->GetInitialMap();
		if (InitialMap.IsEmpty() && GetCookedMaps().Num() == 1)
		{
			InitialMap = GetCookedMaps()[0];
		}
		Writer.WriteObjectStart("cmdline");
		Writer.WriteValue("", InitialMap);
		Writer.WriteValue("messaging", true);
		Writer.WriteObjectEnd();

		// devices
		ITargetDeviceServicesModule& DeviceServiceModule = FModuleManager::LoadModuleChecked<ITargetDeviceServicesModule>(TEXT("TargetDeviceServices"));
		TSharedRef<ITargetDeviceProxyManager> DeviceProxyManager = DeviceServiceModule.GetDeviceProxyManager();
		ILauncherDeviceGroupPtr DeviceGroup = GetDeployedDeviceGroup();
		TMap<FString, FString> RoleCommands;
		FString CommandLine = GetDefaultLaunchRole()->GetUATCommandLine();
		if (CommandLine.Len() > 0)
		{
			// parse out the commands
			TMap<FString, FString> Commands = ParseCommands(CommandLine);
			RoleCommands.Append(Commands);
		}
		if (DeviceGroup.IsValid())
		{
			const TArray<FString>& Devices = DeviceGroup->GetDeviceIDs();
			bool bUseVsync = false;

			Writer.WriteArrayStart("device");

			// for each deployed device...
			for (int32 DeviceIndex = 0; DeviceIndex < Devices.Num(); ++DeviceIndex)
			{
				const FString& DeviceId = Devices[DeviceIndex];
				TSharedPtr<ITargetDeviceProxy> DeviceProxy = DeviceProxyManager->FindProxyDeviceForTargetDevice(DeviceId);
				Writer.WriteValue(DeviceId);
				if (DeviceProxy.IsValid())
				{
					TArray<ILauncherProfileLaunchRolePtr> Roles;
					if (GetLaunchRolesFor(DeviceId, Roles) > 0)
					{
						for (int32 RoleIndex = 0; RoleIndex < Roles.Num(); RoleIndex++)
						{
							if (!bUseVsync && Roles[RoleIndex]->IsVsyncEnabled())
							{
								bUseVsync = true;
							}
							CommandLine = Roles[RoleIndex]->GetUATCommandLine();
							TMap<FString, FString> Commands = ParseCommands(CommandLine);
							RoleCommands.Append(Commands);
						}
					}
				}
			}
			Writer.WriteArrayEnd();
		}

		// write out the additional command-line arguments
		static FGuid SessionId(FGuid::NewGuid());
		Writer.WriteObjectStart("addcmdline");
		Writer.WriteValue("sessionid", SessionId.ToString());
		Writer.WriteValue("sessionowner", FPlatformProcess::UserName(true));
		Writer.WriteValue("sessionname", GetName());
		for (TMap<FString, FString>::TIterator Iter = RoleCommands.CreateIterator(); Iter; ++Iter)
		{
			Writer.WriteValue(Iter.Key(), Iter.Value());
		}
		Writer.WriteObjectEnd();

		// map list
		Writer.WriteArrayStart("map");
		const TArray<FString>& CookedMapsArray = GetCookedMaps();
		if (CookedMapsArray.Num() > 0 && (GetCookMode() == ELauncherProfileCookModes::ByTheBook || GetCookMode() == ELauncherProfileCookModes::ByTheBookInEditor))
		{
			for (int32 MapIndex = 0; MapIndex < CookedMapsArray.Num(); ++MapIndex)
			{
				Writer.WriteValue(CookedMapsArray[MapIndex]);
			}
		}
		else
		{
			Writer.WriteValue(InitialMap);
		}
		Writer.WriteArrayEnd();

		// staging directory
		auto PackageDirectory = GetPackageDirectory();
		if (PackageDirectory.Len() > 0)
		{
			Writer.WriteValue("stagingdirectory", PackageDirectory);
		}

		// build
		Writer.WriteValue("build", ShouldBuild());

		// cook
		switch (GetCookMode())
		{
		case ELauncherProfileCookModes::ByTheBook:
			{
				Writer.WriteValue("cook", true);
				Writer.WriteValue("unversionedcookedcontent", IsCookingUnversioned());
				Writer.WriteValue("pak", IsPackingWithUnrealPak());

				if (IsCreatingReleaseVersion())
				{
					Writer.WriteValue("createreleaseversion", GetCreateReleaseVersionName());
				}

				if (IsCreatingDLC())
				{
					Writer.WriteValue("dlcname", GetDLCName());
				}

				Writer.WriteValue("generatepatch", IsGeneratingPatch());

				if (IsGeneratingPatch() || IsCreatingReleaseVersion() || IsCreatingDLC())
				{
					if (GetBasedOnReleaseVersionName().IsEmpty() == false)
					{
						Writer.WriteValue("basedonreleaseversion", GetBasedOnReleaseVersionName());
						Writer.WriteValue("stagebasereleasepaks", ShouldStageBaseReleasePaks());
					}

					if (GetOriginalReleaseVersionName().IsEmpty() == false)
					{
						Writer.WriteValue("originalreleaseversion", GetOriginalReleaseVersionName());
					}
				}

				if (IsGeneratingPatch())
				{
					Writer.WriteValue("addpatchlevel", ShouldAddPatchLevel());
				}

				Writer.WriteValue("manifests", IsGeneratingChunks());

				if (IsGenerateHttpChunkData())
				{
					Writer.WriteValue("createchunkinstall", true);
					Writer.WriteValue("chunkinstalldirectory", GetHttpChunkDataDirectory());
					Writer.WriteValue("chunkinstallversion", GetHttpChunkDataReleaseName());
				}

				if (IsArchiving())
				{
					Writer.WriteValue("archive", true);
					Writer.WriteValue("archivedirectory", GetArchiveDirectory());
				}

				TMap<FString, FString> CookCommands = ParseCommands(GetCookOptions());
				for (TMap<FString, FString>::TIterator Iter = CookCommands.CreateIterator(); Iter; ++Iter)
				{
					Writer.WriteValue(Iter.Key(), Iter.Value());
				}
			}
			break;
		case ELauncherProfileCookModes::OnTheFly:
			{
				Writer.WriteValue("cookonthefly", true);

				//if UAT doesn't stick around as long as the process we are going to run, then we can't kill the COTF server when UAT goes down because the program
				//will still need it.  If UAT DOES stick around with the process then we DO want the COTF server to die with UAT so the next time we launch we don't end up
				//with two COTF servers.
				if (ClosesAfterLaunch)
				{
					Writer.WriteValue("nokill", true);
				}
			}
			break;
		case ELauncherProfileCookModes::OnTheFlyInEditor:
			Writer.WriteValue("skipcook", true);
			Writer.WriteValue("cookonthefly", true);
			break;
		case ELauncherProfileCookModes::ByTheBookInEditor:
			Writer.WriteValue("skipcook", true);
			break;
		case ELauncherProfileCookModes::DoNotCook:
			Writer.WriteValue("skipcook", true);
			break;
		}

		Writer.WriteValue("iterativecooking", IsCookingIncrementally());
		Writer.WriteValue("iteratesharedcookedbuild", IsIterateSharedCookedBuild() );
		Writer.WriteValue("skipcookingeditorcontent", GetSkipCookingEditorContent());
		Writer.WriteValue("compressed", IsCompressed());
		Writer.WriteValue("EncryptIniFiles", IsEncryptingIniFiles());
		Writer.WriteValue("ForDistribution", IsForDistribution());

		// stage/package/deploy
		bool bIsStaging = false;
		if (GetDeploymentMode() != ELauncherProfileDeploymentModes::DoNotDeploy)
		{
			switch (GetDeploymentMode())
			{
			case ELauncherProfileDeploymentModes::CopyRepository:
				{
					Writer.WriteValue("skipstage", true);
					Writer.WriteValue("deploy", true);
				}
				break;

			case ELauncherProfileDeploymentModes::CopyToDevice:
				{
					Writer.WriteValue("iterativedeploy", IsDeployingIncrementally());
				}
			case ELauncherProfileDeploymentModes::FileServer:
				{
					Writer.WriteValue("stage", true);
					bIsStaging = true;
					Writer.WriteValue("deploy", true);
				}
				break;
			}

			// run
			if (GetLaunchMode() != ELauncherProfileLaunchModes::DoNotLaunch)
			{
				Writer.WriteValue("run", true);
			}
		}
		else
		{
			if (GetPackagingMode() == ELauncherProfilePackagingModes::Locally)
			{
				Writer.WriteValue("stage", true);
				bIsStaging = true;
				Writer.WriteValue("package", true);
			}
		}

		if (bIsStaging &&
			GetReferenceContainerGlobalFileName().Len())
		{
			// (only iostore uses this) - pass reference chunk database
			Writer.WriteValue("ReferenceContainerGlobalFileName", GetReferenceContainerGlobalFileName());
			if (GetReferenceContainerCryptoKeysFileName().Len())
			{
				Writer.WriteValue("ReferenceContainerCryptoKeys", GetReferenceContainerCryptoKeysFileName());
			}
		}

		/*
		"script", ""
		"project", ""
		"i18npreset", ""
		"cookcultures", ["", "", ""]
		"targetplatform, ["". "", ""]
		"servertargetplatform, ["", "", ""]
		"build", "true/false"
		"run", "true/false"
		"cook, "true/false"
		"cookflavor, ""
		"createreleaseversionroot", ""
		"basedonreleaseversionroot", ""
		"createreleaseversion", ""
		"baseonreleaseversion", ""
		"generatepatch", "true/false"
		"additionalcookeroptions", ["","",""]
		"dlcname", ""
		"diffcookedcontentpath", ""
		"dlcincludeengine", "true/false"
		"skipcook", "true/false"
		"clean", "true/false"
		"signpak", ""
		"signedpak", "true/false"
		"pak", "true/false"
		"skippak", "true/false"
		"noxge", "true/false"
		"cookonthefly", "true/false"
		"cookontheflystreaming", "true/false"
		"unversionedcookcontent", "true/false"
		"skipcookingeditorcontent", "true/false"
		"compressed", "true/false"
		"iterativecooking", "true/false"
		"skipcookonthefly", "true/false"
		"cookall", "true/false"
		"cookmapsonly", "true/false"
		"fileserver", "true/false"
		"dedicatedserver", "true/false"
		"client", "true/false"
		"noclient", "true/false"
		"logwindow", "true/false"
		"stage", "true/false"
		"skipstage", "true/false"
		"stagingdirectory", ""
		"stagenonmonolithic", "true/false"
		"codesign", "true/false"
		"manifests", "true/false"
		"createchunkinstall", "true/false"
		"chunkinstalldirectory", ""
		"chunkinstallversion", ""
		"archive", "true/false"
		"archivedirectory", ""
		"archivemetadata", "true/false"
		"distrbution", "true/false"
		"prereqs", "true/false"
		"nobootstrapexe", "true/false"
		"prebuilt", "true/false"
		"nodebuginfo", "true/false"
		"nocleanstage", "true/false"
		"maptorun", ""
		"additionalservermapparams", ["", "", ""]
		"foreign", "true/false"
		"foreigncode", "true/false"
		"cmdline", ["", "", ""]
		"bundlename", ""
		"addcmdline", ["", "", ""]
		"package", "true/false"
		"deploy", "true/false"
		"iterativedeploy", "true/false"
		"fastcook", "true/false"
		"ignorecookerrors", "true/false"
		"uploadsymbols", "true/false"
		"device", ""
		"serverdevice", ""
		"nullrhi", "true/false"
		"fakeclient", "true/false"
		"editortests", "true/false"
		"runautomationtest", ""
		"runautomationtests", "true/false"
		"skipserver", "true/false"
		"unrealexe", ""
		"unattended", "true/false"
		"deviceuser", ""
		"devicepass", ""
		"crashreporter", "true/false"
		"specifiedarchitecture", ""
		"clientconfig", ["", "", ""]
		"serverconfig", ["", "", ""]
		"port", ["", "", ""]
		"mapstocook", ["", "", ""]
		"numclients", "8"
		"crashindex", "8"
		"runtimeoutseconds", "8"
		*/
		Writer.WriteObjectEnd();
		Writer.WriteArrayEnd();
	}

	TMap<FString, FString> ParseCommands(FString CommandLine)
	{
		TMap<FString, FString> RoleCommands;
		FString Left;
		FString Right;
		while (CommandLine.Split(TEXT(" "), &Left, &Right))
		{
			if(Left.Len() > 0)
			{
				FString Key;
				FString Value;
				if (!Left.Split(TEXT("="), &Key, &Value))
				{
					Key = Left;
					Value = TEXT("true");
				}
				Key.RemoveFromStart(TEXT("-"));
				RoleCommands.Add(Key, Value);
			}
			CommandLine = Right;
		}
		if (CommandLine.Len() > 0)
		{
			if (!CommandLine.Split(TEXT("="), &Left, &Right))
			{
				Right = TEXT("true");
			}
			Left.RemoveFromStart(TEXT("-"));
			RoleCommands.Add(Left, Right);
		}

		return RoleCommands;
	}

	TArray<FString> FindPlatforms()
	{
		TArray<FString> Platforms;
		if (GetCookMode() == ELauncherProfileCookModes::ByTheBook)
		{
			Platforms = GetCookedPlatforms();
		}

		// determine deployment platforms
		ILauncherDeviceGroupPtr DeviceGroup = GetDeployedDeviceGroup();
		FName Variant = NAME_None;

		// Loading the Device Proxy Manager to get the needed Device Manager.
		ITargetDeviceServicesModule& DeviceServiceModule = FModuleManager::LoadModuleChecked<ITargetDeviceServicesModule>(TEXT("TargetDeviceServices"));
		TSharedRef<ITargetDeviceProxyManager> DeviceProxyManager = DeviceServiceModule.GetDeviceProxyManager();

		if (DeviceGroup.IsValid() && Platforms.Num() < 1)
		{
			const TArray<FString>& Devices = DeviceGroup->GetDeviceIDs();
			// for each deployed device...
			for (int32 DeviceIndex = 0; DeviceIndex < Devices.Num(); ++DeviceIndex)
			{
				const FString& DeviceId = Devices[DeviceIndex];

				TSharedPtr<ITargetDeviceProxy> DeviceProxy = DeviceProxyManager->FindProxyDeviceForTargetDevice(DeviceId);

				if (DeviceProxy.IsValid())
				{
					// add the platform
					Variant = DeviceProxy->GetTargetDeviceVariant(DeviceId);
					Platforms.AddUnique(DeviceProxy->GetTargetPlatformName(Variant));
				}
			}
		}

		return Platforms;
	}

	bool FindAllPlatforms(TArray<FString>& ServerPlatforms, TArray<FString>& ClientPlatforms, FString& OptionalParams)
	{
		bool bUATClosesAfterLaunch = false;
		TArray<FString> InPlatforms = FindPlatforms();
		for (int32 PlatformIndex = 0; PlatformIndex < InPlatforms.Num(); ++PlatformIndex)
		{
			// Platform info for the given platform
			const PlatformInfo::FTargetPlatformInfo* PlatformInfo = PlatformInfo::FindPlatformInfo(FName(*InPlatforms[PlatformIndex]));
			if (PlatformInfo == nullptr)
			{
				return false;
			}

			// add the UBT platform name to the appropriate list
			TArray<FString>& Platforms = PlatformInfo->PlatformType == EBuildTargetType::Server ? ServerPlatforms : ClientPlatforms;
			Platforms.Add(PlatformInfo->DataDrivenPlatformInfo->UBTPlatformName.ToString());

			// Append any extra UAT flags specified for this platform flavor
			if (!PlatformInfo->UATCommandLine.IsEmpty())
			{
				OptionalParams += PlatformInfo->UATCommandLine;
			}

			bUATClosesAfterLaunch |= PlatformInfo->DataDrivenPlatformInfo->bUATClosesAfterLaunch;
		}

		// If both Client and Server are desired to be built avoid Server causing clients to not be built PlatformInfo wise
		if (OptionalParams.Contains(TEXT("-client")) && OptionalParams.Contains(TEXT("-noclient")))
		{
			OptionalParams = OptionalParams.Replace(TEXT("-noclient"), TEXT(""));
		}

		return bUATClosesAfterLaunch;
	}

	virtual bool Load(const FJsonObject& Object) override
	{
		int32 Version = (int32)Object.GetNumberField(TEXT("Version"));
		if (Version < LAUNCHERSERVICES_FILEFORMATCHANGE || Version > LAUNCHERSERVICES_FINAL)
		{
			return false;
		}

		FGuid::Parse(Object.GetStringField(TEXT("Id")), Id);
		Name = Object.GetStringField(TEXT("Name"));
		Description = Object.GetStringField(TEXT("Description"));
		BuildConfiguration = (EBuildConfiguration)((int32)Object.GetNumberField(TEXT("BuildConfiguration")));
		ProjectSpecified = Object.GetBoolField(TEXT("ProjectSpecified"));
		ShareableProjectPath = Object.GetStringField(TEXT("ShareableProjectPath"));
		CookConfiguration = (EBuildConfiguration)((int32)Object.GetNumberField(TEXT("CookConfiguration")));
		CookIncremental = Object.GetBoolField(TEXT("CookIncremental"));
		CookOptions = Object.GetStringField(TEXT("CookOptions"));
		CookMode = (TEnumAsByte<ELauncherProfileCookModes::Type>)((int32)Object.GetNumberField(TEXT("CookMode")));
		CookUnversioned = Object.GetBoolField(TEXT("CookUnversioned"));

		CookedCultures.Reset();
		const TArray<TSharedPtr<FJsonValue>>* Cultures = NULL;
		if (Object.TryGetArrayField(TEXT("CookedCultures"), Cultures))
		{
			for (auto Value : *Cultures)
			{
				CookedCultures.Add(Value->AsString());
			}
		}

		CookedMaps.Reset();
		const TArray<TSharedPtr<FJsonValue>>* Maps = NULL;
		if (Object.TryGetArrayField(TEXT("CookedMaps"), Maps))
		{
			for (auto Value : *Maps)
			{
				CookedMaps.Add(Value->AsString());
			}
		}

		CookedPlatforms.Reset();
		const TArray<TSharedPtr<FJsonValue>>* Platforms = NULL;
		if (Object.TryGetArrayField(TEXT("CookedPlatforms"), Platforms))
		{
			for (auto Value : *Platforms)
			{
				CookedPlatforms.Add(Value->AsString());
			}
		}

		DeployStreamingServer = Object.GetBoolField(TEXT("DeployStreamingServer"));
		DeployWithUnrealPak = Object.GetBoolField(TEXT("DeployWithUnrealPak"));
		FGuid::Parse(Object.GetStringField(TEXT("DeployedDeviceGroupId")), DeployedDeviceGroupId);
		DeploymentMode = (TEnumAsByte<ELauncherProfileDeploymentModes::Type>)((int32)Object.GetNumberField(TEXT("DeploymentMode")));
		HideFileServerWindow = Object.GetBoolField(TEXT("HideFileServerWindow"));
		LaunchMode = (TEnumAsByte<ELauncherProfileLaunchModes::Type>)((int32)Object.GetNumberField(TEXT("LaunchMode")));
		PackagingMode = (TEnumAsByte<ELauncherProfilePackagingModes::Type>)((int32)Object.GetNumberField(TEXT("PackagingMode")));
		PackageDir = Object.GetStringField(TEXT("PackageDir"));

		int64 BuildModeValue;
		if (Object.TryGetNumberField(TEXT("BuildMode"), BuildModeValue))
		{
			BuildMode = (TEnumAsByte<ELauncherProfileBuildModes::Type>)(int32)BuildModeValue;
		}
		else
		{
			BuildMode = Object.GetBoolField(TEXT("BuildGame")) ? ELauncherProfileBuildModes::Build : ELauncherProfileBuildModes::DoNotBuild;
		}

		ForceClose = Object.GetBoolField(TEXT("ForceClose"));
		Timeout = (uint32)Object.GetNumberField(TEXT("Timeout"));
		Compressed = Object.GetBoolField(TEXT("Compressed"));

		if (Version >= LAUNCHERSERVICES_ADDEDENCRYPTINIFILES)
		{
			EncryptIniFiles = Object.GetBoolField(TEXT("EncryptIniFiles"));
			ForDistribution = Object.GetBoolField(TEXT("ForDistribution"));
		}
		else
		{
			EncryptIniFiles = false;
			ForDistribution = false;
		}

		DefaultDeployPlatform = *(Object.GetStringField(TEXT("DeployPlatform")));
		bSkipCookingEditorContent = Object.GetBoolField(TEXT("SkipCookingEditorContent"));
		DeployIncremental = Object.GetBoolField(TEXT("DeployIncremental"));
		GeneratePatch = Object.GetBoolField(TEXT("GeneratePatch"));

		if (Version >= LAUNCHERSERVICES_ADDEDMULTILEVELPATCHING)
		{
			AddPatchLevel = Object.GetBoolField(TEXT("AddPatchLevel"));
			StageBaseReleasePaks = Object.GetBoolField(TEXT("StageBaseReleasePaks"));
		}
		else
		{
			AddPatchLevel = false;
			StageBaseReleasePaks = false;
		}

		DLCIncludeEngineContent = Object.GetBoolField(TEXT("DLCIncludeEngineContent"));
		CreateReleaseVersion = Object.GetBoolField(TEXT("CreateReleaseVersion"));
		CreateReleaseVersionName = Object.GetStringField(TEXT("CreateReleaseVersionName"));
		BasedOnReleaseVersionName = Object.GetStringField(TEXT("BasedOnReleaseVersionName"));

		if (Version >= LAUNCHERSERVICES_ADDEDREFERENCECONTAINERS)
		{
			ReferenceContainerCryptoKeysFileName = Object.GetStringField(TEXT("ReferenceContainerCryptoKeysFileName"));
			ReferenceContainerGlobalFileName = Object.GetStringField(TEXT("ReferenceContainerGlobalFileName"));
		}
		else
		{
			ReferenceContainerCryptoKeysFileName.Empty();
			ReferenceContainerGlobalFileName.Empty();
		}

		if (Version >= LAUNCHERSERVICES_ADDEDORIGINALRELEASEVERSION)
		{
			OriginalReleaseVersionName = Object.GetStringField(TEXT("OriginalReleaseVersionName"));
		}
		else
		{
			OriginalReleaseVersionName.Empty();
		}

		CreateDLC = Object.GetBoolField(TEXT("CreateDLC"));
		DLCName = Object.GetStringField(TEXT("DLCName"));
		bGenerateChunks = Object.GetBoolField(TEXT("GenerateChunks"));
		bGenerateHttpChunkData = Object.GetBoolField(TEXT("GenerateHttpChunkData"));
		HttpChunkDataDirectory = Object.GetStringField(TEXT("HttpChunkDataDirectory"));
		HttpChunkDataReleaseName = Object.GetStringField(TEXT("HttpChunkDataReleaseName"));

		if (Version >= LAUNCHERSERVICES_ADDARCHIVE)
		{
			bArchive = Object.GetBoolField(TEXT("Archive"));
			ArchiveDir = Object.GetStringField(TEXT("ArchiveDirectory"));
		}
		else
		{
			bArchive = false;
			ArchiveDir = TEXT("");
		}

		if (Version >= LAUNCHERSERVICES_ADDEDADDITIONALCOMMANDLINE)
		{
			AdditionalCommandLineParameters = Object.GetStringField(TEXT("AdditionalCommandLineParameters"));
		}
		else
		{
			AdditionalCommandLineParameters = TEXT("");
		}

		if (Version >= LAUNCHERSERVICES_ADDEDINCLUDEPREREQUISITES)
		{
			IncludePrerequisites = Object.GetBoolField(TEXT("IncludePrerequisites"));
		}

		if (Version >= LAUNCHERSERVICES_ADDEDUSEIOSTORE)
		{
			bUseIoStore = Object.GetBoolField(TEXT("UseIoStore"));
		}

		if (Version >= LAUNCHERSERVICES_ADDEDMAKEBINARYCONFIG)
		{
			bMakeBinaryConfig = Object.GetBoolField(TEXT("MakeBinaryConfig"));
		}

		if (Version >= LAUNCHERSERVICES_ADDBUILDTARGETNAME)
		{
			BuildTargetSpecified = Object.GetBoolField(TEXT("BuildTargetSpecified"));
			BuildTargetName = Object.GetStringField(TEXT("BuildTargetName"));
		}

		// load the default launch role
		TSharedPtr<FJsonObject> Role = Object.GetObjectField(TEXT("DefaultRole"));
		DefaultLaunchRole->Load(*(Role.Get()));

		// serialize the launch roles
		DeployedDeviceGroup.Reset();
		LaunchRoles.Reset();
		const TArray<TSharedPtr<FJsonValue>>* Roles = NULL;
		if (Object.TryGetArrayField(TEXT("LaunchRoles"), Roles))
		{
			for (auto Value : *Roles)
			{
				LaunchRoles.Add(MakeShareable(new FLauncherProfileLaunchRole(*(Value->AsObject().Get()))));
			}
		}

		// if (Version >= LAUNCHERSERVICES_SHAREABLEPROJECTPATHS) Always true due to early out at top of function
		{
			FullProjectPath = FPaths::ConvertRelativePathToFull(FPaths::RootDir(), ShareableProjectPath);
		}

		if (DefaultDeployPlatform != NAME_None)
		{
			SetDefaultDeployPlatform(DefaultDeployPlatform);
		}

		RefreshValidBuildTargets();
		Validate();

		return true;
	}

	virtual void SetDefaults( ) override
	{
		ProjectSpecified = false;
		AdditionalCommandLineParameters = FString();

		// default project settings
		if (FPaths::IsProjectFilePathSet())
		{
			FullProjectPath = FPaths::GetProjectFilePath();
		}
		else if (FGameProjectHelper::IsGameAvailable(FApp::GetProjectName()))
		{
			FullProjectPath = FPaths::RootDir() / FApp::GetProjectName() / FApp::GetProjectName() + TEXT(".uproject");
		}
		else
		{
			FullProjectPath = FString();
		}

		FString RelativeProjectPath = FullProjectPath;
		bool bRelative = FPaths::MakePathRelativeTo(RelativeProjectPath, *FPaths::RootDir());

		bool bIsUnderUERoot = bRelative && !(RelativeProjectPath.StartsWith(FString("../"), ESearchCase::CaseSensitive));
		if (bIsUnderUERoot)
		{
			ShareableProjectPath = RelativeProjectPath;
		}
		else
		{
			ShareableProjectPath = FullProjectPath;
		}

		// Use the locally specified project path is resolving through the root isn't working
		ProjectSpecified = !FullProjectPath.IsEmpty();
		
		// I don't use FApp::GetBuildConfiguration() because i don't want the act of running in debug the first time to cause 
		// profiles the user creates to be in debug. This will keep consistency.
		BuildConfiguration = EBuildConfiguration::Development;

		FInternationalization& I18N = FInternationalization::Get();

		// default build settings
		BuildMode = ELauncherProfileBuildModes::Auto;
		BuildUAT = !FApp::GetEngineIsPromotedBuild() && !FApp::IsEngineInstalled();
		BuildTargetSpecified = false;

		// default cook settings
		CookConfiguration = FApp::GetBuildConfiguration();
		CookMode = ELauncherProfileCookModes::OnTheFly;
		CookOptions = FString();
		CookIncremental = false;
		IterateSharedCookedBuild=false;
		CookUnversioned = true;
		Compressed = true;
		EncryptIniFiles = false;
		ForDistribution = false;
		CookedCultures.Reset();
		CookedCultures.Add(I18N.GetCurrentCulture()->GetName());
		CookedMaps.Reset();
		CookedPlatforms.Reset();
		bSkipCookingEditorContent = false;
        ForceClose = true;
        Timeout = 60;

/*		if (GetTargetPlatformManager()->GetRunningTargetPlatform() != NULL)
		{
			CookedPlatforms.Add(GetTargetPlatformManager()->GetRunningTargetPlatform()->PlatformName());
		}*/	

		bArchive = false;
		ArchiveDir = TEXT("");

		// default deploy settings
		DeployedDeviceGroup.Reset();
		DeploymentMode = ELauncherProfileDeploymentModes::CopyToDevice;
		DeployStreamingServer = false;
		IncludePrerequisites = false;
		DeployWithUnrealPak = false;
		DeployedDeviceGroupId = FGuid();
		HideFileServerWindow = false;
		DeployIncremental = false;

		CreateReleaseVersion = false;
		GeneratePatch = false;
		AddPatchLevel = false;
		StageBaseReleasePaks = false;
		CreateDLC = false;
		DLCIncludeEngineContent = false;

		bGenerateChunks = false;
		bGenerateHttpChunkData = false;
		HttpChunkDataDirectory = TEXT("");
		HttpChunkDataReleaseName = TEXT("");

		// default launch settings
		DefaultDeployPlatform = NAME_None;
		LaunchMode = ELauncherProfileLaunchModes::DefaultRole;
		DefaultLaunchRole->SetCommandLine(FString());
		DefaultLaunchRole->SetInitialCulture(I18N.GetCurrentCulture()->GetName());
		DefaultLaunchRole->SetInitialMap(FString());
		DefaultLaunchRole->SetName(TEXT("Default Role"));
		DefaultLaunchRole->SetInstanceType(ELauncherProfileRoleInstanceTypes::StandaloneClient);
		DefaultLaunchRole->SetVsyncEnabled(false);
		LaunchRoles.Reset();

		// default packaging settings
		PackagingMode = ELauncherProfilePackagingModes::DoNotPackage;

		// default UAT settings
		ILauncherServicesModule& LauncherServicesModule = FModuleManager::GetModuleChecked<ILauncherServicesModule>(TEXT("LauncherServices"));
		EditorExe = LauncherServicesModule.GetExecutableForCommandlets();

		bNotForLicensees = false;
		bUseIoStore = false;
		bUseZenStore = false;
		bShouldUpdateFlash = false;
		bIsDeviceASimulator = false;
		bMakeBinaryConfig = false;

		RefreshValidBuildTargets();
		Validate();
	}

	virtual void SetBuildMode(ELauncherProfileBuildModes::Type Mode) override
	{
		if (BuildMode != Mode)
		{
			BuildMode = Mode;

			Validate();
		}
	}

	virtual void SetBuildUAT(bool Build) override
	{
		if (BuildUAT != Build)
		{
			BuildUAT = Build;

			Validate();
		}
	}

	virtual void SetAdditionalCommandLineParameters(const FString& Params) override
	{
		if (AdditionalCommandLineParameters != Params)
		{
			AdditionalCommandLineParameters = Params;

			Validate();
		}
	}

	virtual void SetBuildConfiguration( EBuildConfiguration Configuration ) override
	{
		if (BuildConfiguration != Configuration)
		{
			BuildConfiguration = Configuration;

			Validate();
		}
	}

	virtual void SetBuildTargetSpecified(bool Specified) override
	{
		if (BuildTargetSpecified != Specified)
		{
			BuildTargetSpecified = Specified;
			Validate();
		}
	}

	virtual void FallbackBuildTargetUpdated() override
	{
		if (!HasBuildTargetSpecified())
		{
			Validate();
		}
	}


	virtual void SetBuildTarget( const FString& TargetName ) override
	{
		if (BuildTargetName != TargetName)
		{
			BuildTargetName = TargetName;

			Validate();
		}
	}

	virtual void SetCookConfiguration( EBuildConfiguration Configuration ) override
	{
		if (CookConfiguration != Configuration)
		{
			CookConfiguration = Configuration;

			Validate();
		}
	}

	virtual void SetCookMode( ELauncherProfileCookModes::Type Mode ) override
	{
		if (CookMode != Mode)
		{
			CookMode = Mode;

			Validate();
		}
	}

	virtual void SetCookOptions(const FString& Options) override
	{
		if (CookOptions != Options)
		{
			CookOptions = Options;

			Validate();
		}
	}

	virtual void SetSkipCookingEditorContent(const bool InSkipCookingEditorContent) override
	{
		if (bSkipCookingEditorContent != InSkipCookingEditorContent)
		{
			bSkipCookingEditorContent = InSkipCookingEditorContent;
			Validate();
		}
	}

	virtual void SetDeployWithUnrealPak( bool UseUnrealPak ) override
	{
		if (DeployWithUnrealPak != UseUnrealPak)
		{
			DeployWithUnrealPak = UseUnrealPak;

			Validate();
		}
	}

	virtual void SetGenerateChunks(bool bInGenerateChunks) override
	{
		if (bGenerateChunks != bInGenerateChunks)
		{
			bGenerateChunks = bInGenerateChunks;
			Validate();
		}
	}

	virtual void SetGenerateHttpChunkData(bool bInGenerateHttpChunkData) override
	{
		if (bGenerateHttpChunkData != bInGenerateHttpChunkData)
		{
			bGenerateHttpChunkData = bInGenerateHttpChunkData;
			Validate();
		}
	}

	virtual void SetHttpChunkDataDirectory(const FString& InHttpChunkDataDirectory) override
	{
		if (HttpChunkDataDirectory != InHttpChunkDataDirectory)
		{
			HttpChunkDataDirectory = InHttpChunkDataDirectory;
			Validate();
		}
	}

	virtual void SetHttpChunkDataReleaseName(const FString& InHttpChunkDataReleaseName) override
	{
		if (HttpChunkDataReleaseName != InHttpChunkDataReleaseName)
		{
			HttpChunkDataReleaseName = InHttpChunkDataReleaseName;
			Validate();
		}
	}

	virtual void SetDeployedDeviceGroup( const ILauncherDeviceGroupPtr& DeviceGroup ) override
	{
		if(DeployedDeviceGroup.IsValid())
		{
			DeployedDeviceGroup->OnDeviceAdded().Remove(OnLauncherDeviceGroupDeviceAddedDelegateHandle);
			DeployedDeviceGroup->OnDeviceRemoved().Remove(OnLauncherDeviceGroupDeviceRemoveDelegateHandle);
		}
		DeployedDeviceGroup = DeviceGroup;
		if (DeployedDeviceGroup.IsValid())
		{
			OnLauncherDeviceGroupDeviceAddedDelegateHandle   = DeployedDeviceGroup->OnDeviceAdded().AddRaw(this, &FLauncherProfile::OnLauncherDeviceGroupDeviceAdded);
			OnLauncherDeviceGroupDeviceRemoveDelegateHandle  = DeployedDeviceGroup->OnDeviceRemoved().AddRaw(this, &FLauncherProfile::OnLauncherDeviceGroupDeviceRemove);
			DeployedDeviceGroupId = DeployedDeviceGroup->GetId();
		}
		else
		{
			DeployedDeviceGroupId = FGuid();
		}

		if (DefaultDeployPlatform != NAME_None)
		{
			SetDefaultDeployPlatform(DefaultDeployPlatform);
		}

		Validate();
	}

	virtual FIsCookFinishedDelegate& OnIsCookFinished() override
	{
		return IsCookFinishedDelegate;
	}

	virtual FCookCanceledDelegate& OnCookCanceled() override
	{
		return CookCanceledDelegate;
	}

	virtual void SetDeploymentMode( ELauncherProfileDeploymentModes::Type Mode ) override
	{
		if (DeploymentMode != Mode)
		{
			DeploymentMode = Mode;

			Validate();
		}
	}

    virtual void SetForceClose( bool Close ) override
    {
        if (ForceClose != Close)
        {
            ForceClose = Close;
            Validate();
        }
    }
    
	virtual void SetHideFileServerWindow( bool Hide ) override
	{
		HideFileServerWindow = Hide;
	}

	virtual void SetIncrementalCooking( bool Incremental ) override
	{
		if (CookIncremental != Incremental)
		{
			CookIncremental = Incremental;

			Validate();
		}
	}

	virtual void SetIterateSharedCookedBuild( bool SharedCookedBuild ) override
	{
		if (IterateSharedCookedBuild != SharedCookedBuild)
		{
			IterateSharedCookedBuild = SharedCookedBuild;

			Validate();
		}
	}

	virtual void SetCompressed( bool Enabled ) override
	{
		if (Compressed != Enabled)
		{
			Compressed = Enabled;

			Validate();
		}
	}

	virtual void SetForDistribution(bool Enabled)override
	{
		if (ForDistribution != Enabled)
		{
			ForDistribution = Enabled;

			Validate();
		}
	}

	virtual void SetEncryptingIniFiles(bool Enabled) override
	{
		if (EncryptIniFiles != Enabled)
		{
			EncryptIniFiles = Enabled;

			Validate();
		}
	}

	virtual void SetIncrementalDeploying( bool Incremental ) override
	{
		if (DeployIncremental != Incremental)
		{
			DeployIncremental = Incremental;

			Validate();
		}
	}

	virtual void SetLaunchMode( ELauncherProfileLaunchModes::Type Mode ) override
	{
		if (LaunchMode != Mode)
		{
			LaunchMode = Mode;

			Validate();
		}
	}

	virtual void SetName( const FString& NewName ) override
	{
		if (Name != NewName)
		{
			Name = NewName;

			Validate();
		}
	}

	virtual void SetDescription(const FString& NewDescription) override
	{
		if (Description != NewDescription)
		{
			Description = NewDescription;

			Validate();
		}
	}

	virtual void SetNotForLicensees() override
	{
		bNotForLicensees = true;
	}

	virtual void SetPackagingMode( ELauncherProfilePackagingModes::Type Mode ) override
	{
		if (PackagingMode != Mode)
		{
			PackagingMode = Mode;

			Validate();
		}
	}

	virtual void SetPackageDirectory( const FString& Dir ) override
	{
		if (PackageDir != Dir)
		{
			PackageDir = Dir;

			Validate();
		}
	}

	virtual void SetArchive( bool bInArchive ) override
	{
		if (bInArchive != bArchive)
		{
			bArchive = bInArchive;

			Validate();
		}
	}

	virtual void SetArchiveDirectory( const FString& Dir ) override
	{
		if (ArchiveDir != Dir)
		{
			ArchiveDir = Dir;

			Validate();
		}
	}

	virtual void SetProjectSpecified(bool Specified) override
	{
		if (ProjectSpecified != Specified)
		{
			ProjectSpecified = Specified;

			RefreshValidBuildTargets();
			Validate();

			ProjectChangedDelegate.Broadcast();
		}
	}

	virtual void FallbackProjectUpdated() override
	{
		if (!HasProjectSpecified())
		{
			RefreshValidBuildTargets();
			Validate();

			ProjectChangedDelegate.Broadcast();
		}
	}

	virtual void SetProjectPath( const FString& Path ) override
	{
		if (FullProjectPath != Path)
		{
			if(Path.IsEmpty())
			{
				FullProjectPath = Path;
			}
			else
			{
				FullProjectPath = FPaths::ConvertRelativePathToFull(Path);

				FString RelativeProjectPath = Path;
				bool bRelative = FPaths::MakePathRelativeTo(RelativeProjectPath, *FPaths::RootDir());

				bool bIsUnderUERoot = bRelative && !(RelativeProjectPath.StartsWith(FString("../"), ESearchCase::CaseSensitive));
				if (bIsUnderUERoot)
				{
					ShareableProjectPath = RelativeProjectPath;
				}
				else
				{
					ShareableProjectPath = FullProjectPath;
				}				
			}
			CookedMaps.Reset();

			RefreshValidBuildTargets();
			Validate();

			ProjectChangedDelegate.Broadcast();
		}
	}

	virtual void SetStreamingFileServer( bool Streaming ) override
	{
		if (DeployStreamingServer != Streaming)
		{
			DeployStreamingServer = Streaming;

			Validate();
		}
	}

	virtual void SetIncludePrerequisites(bool InValue) override
	{
		if (IncludePrerequisites != InValue)
		{
			IncludePrerequisites = InValue;

			Validate();
		}
	}

    virtual void SetTimeout( uint32 InTime ) override
    {
        if (Timeout != InTime)
        {
            Timeout = InTime;
            
            Validate();
        }
    }
    
	virtual void SetUnversionedCooking( bool Unversioned ) override
	{
		if (CookUnversioned != Unversioned)
		{
			CookUnversioned = Unversioned;

			Validate();
		}
	}

	virtual void SetGeneratePatch( bool InGeneratePatch ) override
	{
		GeneratePatch = InGeneratePatch;
	}

	virtual void SetAddPatchLevel( bool InAddPatchLevel) override
	{
		AddPatchLevel = InAddPatchLevel;
	}

	virtual void SetStageBaseReleasePaks(bool InStageBaseReleasePaks) override
	{
		StageBaseReleasePaks = InStageBaseReleasePaks;
	}

	virtual bool SupportsEngineMaps( ) const override
	{
		return false;
	}

	virtual FOnProfileProjectChanged& OnProjectChanged() override
	{
		return ProjectChangedDelegate;
	}

	virtual FOnProfileBuildTargetOptionsChanged& OnBuildTargetOptionsChanged() override
	{
		return BuildTargetOptionsChangedDelegate;
	}

	virtual void SetEditorExe( const FString& InEditorExe ) override
	{
		EditorExe = InEditorExe;
	}

	virtual FString GetEditorExe( ) const override
	{
		return EditorExe;
	}

	virtual void SetUseIoStore(bool bInUseIoStore) override
	{
		bUseIoStore = bInUseIoStore;

		if (bUseIoStore)
		{
			SetDeployWithUnrealPak(true);
		}
	}

	virtual bool IsUsingIoStore() const override
	{
		return bUseIoStore;
	}

	virtual void SetUseZenStore(bool bInUseZenStore) override
	{
		bUseZenStore = bInUseZenStore;
	}

	virtual bool IsUsingZenStore() const override
	{
		return bUseZenStore;
	}

	virtual void SetShouldUpdateDeviceFlash(bool bInShouldUpdateFlash) override
	{
		bShouldUpdateFlash = bInShouldUpdateFlash;
	}

	/**
	 * Whether or not the flash image/software on the device should attempt to be updated before running
	 */
	virtual bool ShouldUpdateDeviceFlash() const override
	{
		return bShouldUpdateFlash;
	}

	virtual void SetDeviceIsASimulator(bool bInIsDeviceASimualtor) override
	{
		bIsDeviceASimulator = bInIsDeviceASimualtor;
	}

	/**
	 * Is the Launch device actually a simulator?
	 */
	virtual bool IsDeviceASimulator() const override
	{
		return bIsDeviceASimulator;
	}

	virtual void SetMakeBinaryConfig(bool bInMakeBinaryConfig) override
	{
		bMakeBinaryConfig = bInMakeBinaryConfig;
	}

	virtual bool MakeBinaryConfig() const override
	{
		return bMakeBinaryConfig;
	}

	virtual TArray<FString> GetExplicitBuildTargetNames() const override
	{
		return ExplictBuildTargetNames;
	}

	virtual bool RequiresExplicitBuildTargetName() const override
	{
		return ExplictBuildTargetNames.Num() > 0;
	}


	//~ End ILauncherProfile Interface

protected:

	/**
	 * Validates the profile's current settings.
	 *
	 * Possible validation errors and warnings can be retrieved using the HasValidationError() method.
	 *
	 * @return true if the profile passed validation, false otherwise.
	 */
	void Validate( )
	{
		ValidationErrors.Reset();

		// Build: a build configuration must be selected
		if (BuildConfiguration == EBuildConfiguration::Unknown)
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::NoBuildConfigurationSelected);
		}

		// Build: a project must be selected
		if (GetProjectPath().IsEmpty())
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::NoProjectSelected);
		}

		// Cook: at least one platform must be selected when cooking by the book
		if ((CookMode == ELauncherProfileCookModes::ByTheBook) && (CookedPlatforms.Num() == 0))
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::NoPlatformSelected);
		}

		// Cook: at least one culture must be selected when cooking by the book
		if ((CookMode == ELauncherProfileCookModes::ByTheBook) && (CookedCultures.Num() == 0))
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::NoCookedCulturesSelected);
		}

		// Deploy: a device group must be selected when deploying builds
		if ((DeploymentMode == ELauncherProfileDeploymentModes::CopyToDevice) && !DeployedDeviceGroupId.IsValid())
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::DeployedDeviceGroupRequired);
		}

		// Deploy: deployment by copying to devices requires cooking by the book
		if ((DeploymentMode == ELauncherProfileDeploymentModes::CopyToDevice) && ((CookMode != ELauncherProfileCookModes::ByTheBook)&&(CookMode!=ELauncherProfileCookModes::ByTheBookInEditor)))
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::CopyToDeviceRequiresCookByTheBook);
		}

		// Deploy: deployment by copying to devices requires no packaging
		if ((DeploymentMode == ELauncherProfileDeploymentModes::CopyToDevice) && (PackagingMode != ELauncherProfilePackagingModes::DoNotPackage))
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::CopyToDeviceRequiresNoPackaging);
		}

		// Deploy: deployment by copying a packaged build to devices requires a package dir
		if ((DeploymentMode == ELauncherProfileDeploymentModes::CopyRepository) && (PackageDir == TEXT("")))
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::NoPackageDirectorySpecified);
		}

		// Launch: custom launch roles are not supported yet
		if (LaunchMode == ELauncherProfileLaunchModes::CustomRoles)
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::CustomRolesNotSupportedYet);
		}

		// Launch: when using custom launch roles, all roles must have a device assigned
		if (LaunchMode == ELauncherProfileLaunchModes::CustomRoles)
		{
			for (int32 RoleIndex = 0; RoleIndex < LaunchRoles.Num(); ++RoleIndex)
			{
				if (LaunchRoles[RoleIndex]->GetAssignedDevice().IsEmpty())
				{
					ValidationErrors.Add(ELauncherProfileValidationErrors::NoLaunchRoleDeviceAssigned);
				
					break;
				}
			}
		}

		if (CookUnversioned && CookIncremental && ((CookMode == ELauncherProfileCookModes::ByTheBook) || (CookMode == ELauncherProfileCookModes::ByTheBookInEditor)))
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::UnversionedAndIncrimental);
		}


		if ( (IsGeneratingPatch() || ShouldAddPatchLevel()) && (CookMode != ELauncherProfileCookModes::ByTheBook) )
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::GeneratingPatchesCanOnlyRunFromByTheBookCookMode);
		}

		if (ShouldAddPatchLevel() && !IsGeneratingPatch() )
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::GeneratingMultiLevelPatchesRequiresGeneratePatch);
		}

		if (ShouldStageBaseReleasePaks() && BasedOnReleaseVersionName.IsEmpty())
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::StagingBaseReleasePaksWithoutABaseReleaseVersion);
		}

		if ( IsGeneratingChunks() && (CookMode != ELauncherProfileCookModes::ByTheBook) )
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::GeneratingChunksRequiresCookByTheBook);
		}

		if (IsGeneratingChunks() && !IsPackingWithUnrealPak())
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::GeneratingChunksRequiresUnrealPak);
		}

		if (IsGenerateHttpChunkData() && !IsGeneratingChunks() && !IsCreatingDLC())
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::GeneratingHttpChunkDataRequiresGeneratingChunks);
		}

		if (IsGenerateHttpChunkData() && (GetHttpChunkDataReleaseName().IsEmpty() || !FPaths::DirectoryExists(*GetHttpChunkDataDirectory())))
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::GeneratingHttpChunkDataRequiresValidDirectoryAndName);
		}

		// Launch: when launching, all devices that the build is launched on must have content cooked for their platform
		if (LaunchMode != ELauncherProfileLaunchModes::DoNotLaunch && CookMode != ELauncherProfileCookModes::OnTheFly && CookMode != ELauncherProfileCookModes::OnTheFlyInEditor)
		{
			// @todo ensure that launched devices have cooked content
		}
		
		if ((CookMode == ELauncherProfileCookModes::OnTheFly) || (CookMode == ELauncherProfileCookModes::OnTheFlyInEditor))
		{
			if (BuildConfiguration == EBuildConfiguration::Shipping)
			{
				// shipping doesn't support commandline options
				ValidationErrors.Add(ELauncherProfileValidationErrors::ShippingDoesntSupportCommandlineOptionsCantUseCookOnTheFly);
			}

		}

		if (CookMode == ELauncherProfileCookModes::OnTheFly)
		{

			for (auto const& CookedPlatform : CookedPlatforms)
			{
				if (CookedPlatform.Contains("Server"))
				{
					ValidationErrors.Add(ELauncherProfileValidationErrors::CookOnTheFlyDoesntSupportServer);
				}
			}
			

		}

		if (bArchive && ArchiveDir.IsEmpty())
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::NoArchiveDirectorySpecified);
		}

		if (bUseIoStore && !DeployWithUnrealPak)
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::IoStoreRequiresPakFiles);
		}

		ValidateBuildTarget();
		ValidatePlatformSDKs();
		ValidateDeviceStatus();
	}
	
	void ValidatePlatformSDKs(void)
	{
		ValidationErrors.Remove(ELauncherProfileValidationErrors::NoPlatformSDKInstalled);
		
		// Cook: ensure that all platform SDKs are installed
		if (CookedPlatforms.Num() > 0)
		{
			bool bProjectHasCode = false; // @todo: Does the project have any code?
			FString NotInstalledDocLink;
			for (auto PlatformName : CookedPlatforms)
			{
				const ITargetPlatform* Platform = GetTargetPlatformManager()->FindTargetPlatform(PlatformName);
				if(!Platform || !Platform->IsSdkInstalled(bProjectHasCode, NotInstalledDocLink))
				{
					ValidationErrors.Add(ELauncherProfileValidationErrors::NoPlatformSDKInstalled);
					ILauncherServicesModule& LauncherServicesModule = FModuleManager::GetModuleChecked<ILauncherServicesModule>(TEXT("LauncherServices"));
					LauncherServicesModule.BroadcastLauncherServicesSDKNotInstalled(PlatformName, NotInstalledDocLink);
					if (!Platform)
					{
						CookedPlatforms.Remove(PlatformName);
					}
					else
					{
						InvalidPlatform = PlatformName;
					}
					return;
				}
			}
		}
		
		// Deploy: ensure that all the target device SDKs are installed
		if ((DeploymentMode != ELauncherProfileDeploymentModes::DoNotDeploy) && DeployedDeviceGroup.IsValid())
		{
			const TArray<FString>& Devices = DeployedDeviceGroup->GetDeviceIDs();
			for(auto DeviceId : Devices)
			{
				ITargetDeviceServicesModule* TargetDeviceServicesModule = static_cast<ITargetDeviceServicesModule*>(FModuleManager::Get().LoadModule(TEXT("TargetDeviceServices")));
				
				if (TargetDeviceServicesModule)
				{
					TSharedPtr<ITargetDeviceProxy> DeviceProxy = TargetDeviceServicesModule->GetDeviceProxyManager()->FindProxy(DeviceId);
					
					if(DeviceProxy.IsValid())
					{
						FString const& PlatformName = DeviceProxy->GetTargetPlatformName(DeviceProxy->GetTargetDeviceVariant(DeviceId));
						bool bProjectHasCode = false; // @todo: Does the project have any code?
						FString NotInstalledDocLink;
						const ITargetPlatform* Platform = GetTargetPlatformManager()->FindTargetPlatform(PlatformName);
						if(!Platform || !Platform->IsSdkInstalled(bProjectHasCode, NotInstalledDocLink))
						{
							ValidationErrors.Add(ELauncherProfileValidationErrors::NoPlatformSDKInstalled);
							ILauncherServicesModule& LauncherServicesModule = FModuleManager::GetModuleChecked<ILauncherServicesModule>(TEXT("LauncherServices"));
							LauncherServicesModule.BroadcastLauncherServicesSDKNotInstalled(PlatformName, NotInstalledDocLink);
							DeployedDeviceGroup->RemoveDevice(DeviceId);
							return;
						}
					}
				}
			}
		}
	}

	void ValidateDeviceStatus(void)
	{
		ValidationErrors.Remove(ELauncherProfileValidationErrors::LaunchDeviceIsUnauthorized);

		if (DeployedDeviceGroup.IsValid())
		{
			const TArray<FString>& Devices = DeployedDeviceGroup->GetDeviceIDs();
			for (auto DeviceId : Devices)
			{
				ITargetDeviceServicesModule* TargetDeviceServicesModule = static_cast<ITargetDeviceServicesModule*>(FModuleManager::Get().LoadModule(TEXT("TargetDeviceServices")));
				if (TargetDeviceServicesModule)
				{
					TSharedPtr<ITargetDeviceProxy> DeviceProxy = TargetDeviceServicesModule->GetDeviceProxyManager()->FindProxyDeviceForTargetDevice(DeviceId);
					if (DeviceProxy.IsValid())
					{
						if (!DeviceProxy->IsAuthorized())
						{
							ValidationErrors.Add(ELauncherProfileValidationErrors::LaunchDeviceIsUnauthorized);
							return;
						}
					}
				}
			}
		}
	}

	void ValidateBuildTarget()
	{
		bool bBuildTargetIsRequired = false;
		bool bBuildTargetIsSelected = false;

		FString BuildTarget = GetBuildTarget();
		TSet<EBuildTargetType> CookTargetTypes = GetCookTargetTypes();
		if (CookTargetTypes.Num() == 0)
		{
			CookTargetTypes.Add(EBuildTargetType::Game); // UAT defaults to 'Game' too
		}

		// check all build targets
		const TArray<FString>& BuildTargetNames = HasProjectSpecified() ? ExplictBuildTargetNames : LauncherProfileManager->GetAllExplicitBuildTargetNames();	
		const TArray<FTargetInfo>& Targets = FDesktopPlatformModule::Get()->GetTargetsForProject(GetProjectPath());
		for (const FTargetInfo& Target : Targets)
		{
			if (CookTargetTypes.Contains(Target.Type))
			{
				if (BuildTarget.IsEmpty())
				{
					// most target types can have a default build target specified in engine ini so no need to enforce this (UAT will give informative error if the ini isn't set up)
					bool bSupportsDefaultBuildTarget = (Target.Type != EBuildTargetType::Program); 

					if (!bSupportsDefaultBuildTarget && BuildTargetNames.Contains(Target.Name))
					{
						bBuildTargetIsRequired = true;
						break;
					}
				}
				else
				{
					if (Target.Name == BuildTarget)
					{
						bBuildTargetIsSelected = true;
						break;
					}
				}
			}
		}

		if (!BuildTarget.IsEmpty() && !bBuildTargetIsSelected)
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::BuildTargetCookVariantMismatch);
		}

		if (bBuildTargetIsRequired)
		{
			if (BuildTargetSpecified)
			{
				ValidationErrors.Add(ELauncherProfileValidationErrors::BuildTargetIsRequired);
			}
			else
			{
				ValidationErrors.Add(ELauncherProfileValidationErrors::FallbackBuildTargetIsRequired);
			}
		}
	}


	void RefreshValidBuildTargets()
	{
		TArray<FString> LatestExplicitBuildTargetNames;

		// collect the build targets for the current project, filtered to what we are currently wanting to cook. Do not show fallback project's build targets
		if (HasProjectSpecified())
		{
			TSet<EBuildTargetType> CookTargetTypes = GetCookTargetTypes();
			LatestExplicitBuildTargetNames = FGameProjectHelper::GetExplicitBuildTargetsForProject( GetProjectPath(), &CookTargetTypes );
		}

		// notify listeners if the explicitly-required build targets have changed
		if (ExplictBuildTargetNames != LatestExplicitBuildTargetNames)
		{
			ExplictBuildTargetNames = LatestExplicitBuildTargetNames;
			BuildTargetOptionsChangedDelegate.Broadcast();
		}
	}


	TSet<EBuildTargetType> GetCookTargetTypes() const
	{
		TSet<EBuildTargetType> CookTargetTypes;
		for ( const FString& Variant : GetCookedPlatforms() )
		{
			if (Variant.EndsWith(TEXT("Client")))
			{
				CookTargetTypes.Add(EBuildTargetType::Client);
			}
			else if (Variant.EndsWith(TEXT("Server")))
			{
				CookTargetTypes.Add(EBuildTargetType::Server);
			}
			else if (Variant.EndsWith(TEXT("Editor")))
			{
				CookTargetTypes.Add(EBuildTargetType::Editor);
			}
			else
			{
				CookTargetTypes.Add(EBuildTargetType::Game);
			}
		}

		return MoveTemp(CookTargetTypes);
	}


	void OnLauncherDeviceGroupDeviceAdded(const ILauncherDeviceGroupRef& DeviceGroup, const FString& DeviceId)
	{
		if( DeviceGroup == DeployedDeviceGroup )
		{
			ValidatePlatformSDKs();
		}
	}
	
	void OnLauncherDeviceGroupDeviceRemove(const ILauncherDeviceGroupRef& DeviceGroup, const FString& DeviceId)
	{
		if( DeviceGroup == DeployedDeviceGroup )
		{
			ValidatePlatformSDKs();
		}
	}

	void OnSelectedProjectChanged()
	{
		RefreshValidBuildTargets();

		BuildTargetName.Empty();
		BuildTargetSpecified = ProjectSpecified; // if this is for 'Any Project' then it should use the fallback build target
	}

private:

	//  Holds a reference to the launcher profile manager.
	ILauncherProfileManagerRef LauncherProfileManager;

	// Holds the desired build configuration (only used if creating new builds).
	EBuildConfiguration BuildConfiguration;

	// Holds a flag indicating whether the build target is specified by this profile.
	bool BuildTargetSpecified;

	// Holds the name of the target (matching a .cs file) to build. Needed when multiple targets of a type exist
	FString BuildTargetName;

	// Holds the build mode.
	// Holds the build configuration name of the cooker.
	EBuildConfiguration CookConfiguration;

	// Holds additional cooker command line options.
	FString CookOptions;

	// Holds the cooking mode.
	TEnumAsByte<ELauncherProfileCookModes::Type> CookMode;

	// Holds the build mode
	TEnumAsByte<ELauncherProfileBuildModes::Type> BuildMode;

	// Holds a flag indicating whether UAT should be built
	bool BuildUAT;

	// Generate compressed content
	bool Compressed;

	// encrypt ini files in the pak file
	bool EncryptIniFiles;
	// is this build for distribution
	bool ForDistribution;
	// Holds a flag indicating whether only modified content should be cooked.
	bool CookIncremental;

	// hold a flag indicating if we want to iterate on the shared cooked build
	bool IterateSharedCookedBuild;

	// Holds a flag indicating whether packages should be saved without a version.
	bool CookUnversioned;

	bool bSkipCookingEditorContent;

	// This setting is used only if cooking by the book (only used if cooking by the book).
	TArray<FString> CookedCultures;

	// Holds the collection of maps to cook (only used if cooking by the book).
	TArray<FString> CookedMaps;

	// Holds the platforms to include in the build (only used if creating new builds).
	TArray<FString> CookedPlatforms;

	// Holds the platforms to deploy to if no specific devices were chosen for deploy.
	FName DefaultDeployPlatform;

	// Holds the default role (only used if launch mode is DefaultRole).
	ILauncherProfileLaunchRoleRef DefaultLaunchRole;

	// Holds a flag indicating whether a streaming file server should be used.
	bool DeployStreamingServer;

	// Holds a flag indicating whether content should be packaged with UnrealPak.
	bool DeployWithUnrealPak;

	// Flag to indicate if game prerequisites should be included
	bool IncludePrerequisites;

	// Flag indicating if content should be split into chunks
	bool bGenerateChunks;
	
	// Flag indicating if chunked content should be used to generate HTTPChunkInstall data
	bool bGenerateHttpChunkData;
	
	// Where to store HTTPChunkInstall data
	FString HttpChunkDataDirectory;
	
	// Version name of the HTTPChunkInstall data
	FString HttpChunkDataReleaseName;

	// if present, iostore container creation will try to use existing compressed blocks
	// instead of compressing new ones, to avoid patches from compressor version changes,
	// and to speed up iostore container creation time. See IoStoreUtilities.cpp ReferenceContainerGlobalFileName.
	FString ReferenceContainerGlobalFileName;

	// If ReferenceContainerGlobalFileName refers to encrypted containers, this is the filename of
	// the json file containing the keys.
	FString ReferenceContainerCryptoKeysFileName;

	// create a release version of the content (this can be used to base dlc / patches from)
	bool CreateReleaseVersion;

	// name of the release version
	FString CreateReleaseVersionName;

	// name of the release version to base this dlc / patch on
	FString BasedOnReleaseVersionName;

	// name of the original release version
	FString OriginalReleaseVersionName;

	// This build generate a patch based on some source content seealso PatchSourceContentPath
	bool GeneratePatch;

	// This build generates a new tier patch file for modified content
	bool AddPatchLevel;

	// This build stages pak files from the release version it is based on
	bool StageBaseReleasePaks;

	// This build will cook content for dlc See also DLCName
	bool CreateDLC;

	// name of the dlc we are going to build (the name of the dlc plugin)
	FString DLCName;

	// should the dlc include engine content in the current dlc 
	//  engine content which was not referenced by original release
	//  otherwise error on any access of engine content during dlc cook
	bool DLCIncludeEngineContent;

	// Holds a flag indicating whether to use incremental deployment
	bool DeployIncremental;

	// Holds the device group to deploy to.
	ILauncherDeviceGroupPtr DeployedDeviceGroup;

	// Delegate handles for registered DeployedDeviceGroup event handlers.
	FDelegateHandle OnLauncherDeviceGroupDeviceAddedDelegateHandle;
	FDelegateHandle OnLauncherDeviceGroupDeviceRemoveDelegateHandle;

	// Holds the identifier of the deployed device group.
	FGuid DeployedDeviceGroupId;

	// Holds the deployment mode.
	TEnumAsByte<ELauncherProfileDeploymentModes::Type> DeploymentMode;

	// Holds a flag indicating whether the file server's console window should be hidden.
	bool HideFileServerWindow;

	// Holds the profile's unique identifier.
	FGuid Id;

	// Holds the launch mode.
	TEnumAsByte<ELauncherProfileLaunchModes::Type> LaunchMode;

	// Holds the launch roles (only used if launch mode is UsingRoles).
	TArray<ILauncherProfileLaunchRolePtr> LaunchRoles;

	// Holds the profile's name.
	FString Name;

	// Holds the profile's description.
	FString Description;

	// Holds the packaging mode.
	TEnumAsByte<ELauncherProfilePackagingModes::Type> PackagingMode;

	// Holds the package directory
	FString PackageDir;

	// Whether to run archive step	
	bool bArchive;

	// Holds the archive directory
	FString ArchiveDir;

	// Holds a flag indicating whether the project is specified by this profile.
	bool ProjectSpecified;

	// Holds the full absolute path to the Unreal project used by this profile.
	FString FullProjectPath;

	// Holds the path that might be shareable between people.  Only works if the project is under the UE root.
	// otherwise this is an absolute path.
	FString ShareableProjectPath;

	// Holds the collection of validation errors.
	TArray<ELauncherProfileValidationErrors::Type> ValidationErrors;
	FString InvalidPlatform;

    // Holds the time out time for the cook on the fly server
    uint32 Timeout;

    // Holds the close value for the cook on the fly server
    bool ForceClose;

	// Path to the editor executable to pass to UAT, for cooking, etc... May be empty.
	FString EditorExe;

	// Profile is for an internal project
	bool bNotForLicensees;

	// Additional command line parameters to set for the application when it launches
	FString AdditionalCommandLineParameters;

	// Use I/O store.
	bool bUseIoStore;
	
	// Use Zen storage server
	bool bUseZenStore;

	// Make binary config.
	bool bMakeBinaryConfig;

	// Update flash on device before running
	bool bShouldUpdateFlash;

	// Is the launch device actually a Simulator
	bool bIsDeviceASimulator;

private:

	// cook in the editor callbacks (not valid for any other cook mode)
	FIsCookFinishedDelegate IsCookFinishedDelegate;
	FCookCanceledDelegate CookCanceledDelegate;

	// Holds a delegate to be invoked when changing the device group to deploy to.
	FOnLauncherProfileDeployedDeviceGroupChanged DeployedDeviceGroupChangedDelegate;

	// Holds a delegate to be invoked when the project has changed
	FOnProfileProjectChanged ProjectChangedDelegate;

	// Holds a delegate to be invoked when the project build target options have changed
	FOnProfileBuildTargetOptionsChanged BuildTargetOptionsChangedDelegate;

	// Cached build target options (not serialized)
	TArray<FString> ExplictBuildTargetNames;
};
