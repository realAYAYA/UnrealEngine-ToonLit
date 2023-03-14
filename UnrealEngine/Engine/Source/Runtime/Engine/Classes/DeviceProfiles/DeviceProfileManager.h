// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DeviceProfileManager.h: Declares the FDeviceProfileManager class.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "DeviceProfileMatching.h"
#include "CoreGlobals.h"
#include "HAL/IConsoleManager.h"
#include "DeviceProfileManager.generated.h"

class UDeviceProfile;

// Delegate used to refresh the UI when the profiles change
DECLARE_MULTICAST_DELEGATE( FOnDeviceProfileManagerUpdated );

// Delegate used to notify systems when the active device profile changes
DECLARE_MULTICAST_DELEGATE( FOnActiveDeviceProfileChanged );

struct FPushedCVarSetting
{
	FPushedCVarSetting() : SetBy(ECVF_Default)	{}
	FPushedCVarSetting(const FString& InValue, EConsoleVariableFlags InFlags) 
		: Value(InValue)
		, SetBy(EConsoleVariableFlags(InFlags & ECVF_SetByMask))
	{}

	FString Value;
	EConsoleVariableFlags SetBy;
};

/**
 * Implements a helper class that manages all profiles in the Device
 */
 UCLASS( config=DeviceProfiles, transient )
class ENGINE_API UDeviceProfileManager : public UObject
{
public:

	GENERATED_BODY()

	/**
	 * Startup and select the active device profile
	 * Then Init the CVars from this profile and it's Device profile parent tree.
	 */
	static void InitializeCVarsForActiveDeviceProfile(bool bPushSettings = false, bool bIsDeviceProfilePreview = false, bool bForceReload = false);

	/**
	 * Reapplies the device profile. Useful when configs have changed (i.e. hotfix)
	 * Applies base and then any overridden device profile.
	 */
	void ReapplyDeviceProfile(bool bForceReload = false);

	/**
	 * Examine the currently active or overridden profile for references to entries in DeviceProfilesToQuery
	 * @param DeviceProfilesToQuery - Collection of device profiles to check.
	 * 
	 * @return true if any profiles contained in DeviceProfilesToQuery are referenced by active or overridden profile.
	 */
	bool DoActiveProfilesReference(const TSet<FString>& DeviceProfilesToQuery);

	/**
	 * Create a copy of a device profile from a copy.
	 *
	 * @param ProfileName - The profile name.
 	 * @param ProfileToCopy - The profile to copy name.
	 *
	 * @return the created profile.
	 */
	UDeviceProfile* CreateProfile(const FString& ProfileName, const FString& ProfileType, const FString& ParentName=TEXT(""), const TCHAR* ConfigPlatform=nullptr);

	/**
	 * Tests to see if a named device profile is available to call CreateProfile with.
	 *
	 * @param ProfileName - The profile name.
	 * @param ProfileToCopy - The profile to copy name.
	 *
	 * @return the created profile.
	 */
	bool HasLoadableProfileName(const FString& ProfileName, FName OptionalPlatformName = FName());

	/**
	 * Delete a profile.
	 *
	 * @param Profile - The profile to delete.
	 */
	void DeleteProfile( UDeviceProfile* Profile );

	/**
	 * Find a profile based on the name.
	 *
	 * @param ProfileName - The profile name to find.
	 * @return The found profile.
	 */
	UDeviceProfile* FindProfile( const FString& ProfileName, bool bCreateProfileOnFail = true );

	/**
	* Overrides the device profile. The original profile can be restored with RestoreDefaultDeviceProfile
	*/
	void SetOverrideDeviceProfile(UDeviceProfile* DeviceProfile);

	/**
	* Restore the device profile to the default for this device
	*/
	void RestoreDefaultDeviceProfile();

#if ALLOW_OTHER_PLATFORM_CONFIG
	/**
	* Applies the requested device profile's cvars onto the currently active DP.
	* It does not change the actual DP. 
	* Use RestorePreviewDeviceProfile to revert.
	*/
	void SetPreviewDeviceProfile(UDeviceProfile* DeviceProfile);

	/**
	* Revert the preview state.
	*/
	void RestorePreviewDeviceProfile();
#endif

	/**
	 * Load the device profiles from the config file.
	 */
	void LoadProfiles();

	/**
	 * Returns a delegate that is invoked when manager is updated.
	 *
	 * @return The delegate.
	 */
	FOnDeviceProfileManagerUpdated& OnManagerUpdated();

	/**
	 * Returns a delegate that is invoked when the active device profile changes
	 *
	 * @return The delegate.
	 */
	FOnActiveDeviceProfileChanged& OnActiveDeviceProfileChanged();

	/**
	 * Returns the config files for all loaded device profiles, this will include platform-specific ones 
	 */
	void GetProfileConfigFiles(OUT TArray<FString>& OutConfigFiles);

	/**
	 * Save the device profiles.
	 */
	void SaveProfiles(bool bSaveToDefaults = false);

	/**
	 * Get the selected device profile
	 *
	 * @return The selected profile.
	 */
	UDeviceProfile* GetActiveProfile() const;

	/**
	 * Get the currently previewing device profile. Can be null.
	 *
	 * @return The preview profile.
	 */
	UDeviceProfile* GetPreviewDeviceProfile() const;

	/**
	* Get a list of all possible parent profiles for a given device profile
	*
	* @param ChildProfile				- The profile we are looking for potential parents
	* @param PossibleParentProfiles	- The list of profiles which would be suitable as a parent for the given profile
	*/
	void GetAllPossibleParentProfiles(const UDeviceProfile* ChildProfile, OUT TArray<UDeviceProfile*>& PossibleParentProfiles) const;

	/**
	* Get the current active profile name.
	*
	* @return The selected profile.
	*/
	const FString GetActiveDeviceProfileName();

	/**
	* Get a string containing the current matched fragment list.
	*
	* e.g. "Fragment1,Fragment2,[tag]Fragment3"
	* 
	* @param bEnabledOnly				- Only Enabled fragments will be present in the returned string.
	* @param bIncludeTags				- If true the Fragment string will include the tag.
	* @param bAlphaSort					- If true the Fragments will be in alphabetical order as opposed to application order.
	* @return csv string of the current matched fragments.
	*/
	const FString GetActiveDeviceProfileMatchedFragmentsString(bool bEnabledOnly, bool bIncludeTags, bool bAlphaSort);

	/**
	* Get the selected device profile name, either the platform name, or the name
	* provided by a Device Profile Selector Module.
	*
	* @return The selected profile.
	*/
	UE_DEPRECATED(4.25, "Use either GetActiveDeviceProfileName to have the current active device profile or GetPlatformDeviceProfileName to have the default one. Note, GetActiveDeviceProfileName will fallback on GetPlatformDeviceProfileName, if there is no active device profile ")
	static const FString GetActiveProfileName();

	/**
	* Get the selected device profile name, either the platform name, or the name
	* provided by a Device Profile Selector Module.
	*
	* @return The selected profile.
	*/
	static const FString GetPlatformDeviceProfileName();
	
	/** Retrieves the value of a scalability group cvar if it was set by the active device profile. */
	static bool GetScalabilityCVar(const FString& CvarName, int32& OutValue);
	static bool GetScalabilityCVar(const FString& CvarName, float& OutValue);

	/**
	* Enable/Disable a tagged fragment of the active device profile.
	* This unsets the entire cvar DP state, then re-sets the new DP+fragment state.
	*/
	void ChangeTaggedFragmentState(FName FragmentTag, bool bNewState);

	/**
	* Return the selected fragment property from the currently active device profile.
	* null if the tag is not found.
	*/
	const FSelectedFragmentProperties* GetActiveDeviceProfileFragmentByTag(FName& FragmentTag) const;


	enum class EDeviceProfileMode : uint8
	{
		DPM_SetCVars,
		DPM_CacheValues,
	};

	/**
	 * Walk the device profile/fragment chain to get the final set ot CVars in a unified way
	 */
	static TMap<FName, FString> GatherDeviceProfileCVars(const FString& DeviceProfileName, EDeviceProfileMode GatherMode);


private:
	/**
	 * Set the active device profile - set via the device profile blueprint.
	 *
	 * @param DeviceProfileName - The profile name.
	 */
	void SetActiveDeviceProfile( UDeviceProfile* DeviceProfile );

	/**
	* Override CVar value change callback
	*/
	void HandleDeviceProfileOverrideChange();

	/** Sees if two profiles are considered identical for saving */
	bool AreProfilesTheSame(UDeviceProfile* Profile1, UDeviceProfile* Profile2) const;

	/** Sees if the texture settings are the same between two profiles */
	bool AreTextureGroupsTheSame(UDeviceProfile* Profile1, UDeviceProfile* Profile2) const;

	/**
	 * Perform the processing of ini sections, going up to parents, etc. Depending on runtime vs editor processing of another platform,
	 * the Mode will control how the settings are handled
	 */
	static void SetDeviceProfileCVars(const FString& DeviceProfileName);


	/** Read and process all of the fragment matching rules. Returns an array containing the names of fragments selected. */
	static TArray<FSelectedFragmentProperties> FindMatchingFragments(const FString& ParentDP, class FConfigCacheIni* PreviewConfigSystem);

	/** Convert a FSelectedFragmentProperties array to a string */
	static const FString FragmentPropertyArrayToFragmentString(const TArray<FSelectedFragmentProperties>& FragmentProperties, bool bEnabledOnly, bool bIncludeTags, bool bAlphaSort);

	/** Get the current platform's the selector module. Can return null */
	static class IDeviceProfileSelectorModule* GetDeviceProfileSelectorModule();

#if ALLOW_OTHER_PLATFORM_CONFIG && WITH_EDITOR
	/** Get another platform's selector module. Can return null */
	static class IDeviceProfileSelectorModule* GetPreviewDeviceProfileSelectorModule(class FConfigCacheIni* PreviewConfigSystem);
#endif
public:

	static class UDeviceProfileManager* DeviceProfileManagerSingleton;
	static UDeviceProfileManager& Get(bool bFromPostCDOContruct = false);

	virtual void PostCDOContruct() override
	{
		Get(true); // get this taken care of now
	}


public:

	// Holds the collection of managed profiles.
	UPROPERTY( EditAnywhere, Category=Properties )
	TArray< TObjectPtr<UDeviceProfile> > Profiles;

private:
	// Cached copy of profiles at load
	UPROPERTY()
	TArray< TObjectPtr<UDeviceProfile> > BackupProfiles;

	// Holds a delegate to be invoked profiles are updated.
	FOnDeviceProfileManagerUpdated ManagerUpdatedDelegate;

	// Holds a delegate to be invoked when the active deviceprofile changes
	FOnActiveDeviceProfileChanged ActiveDeviceProfileChangedDelegate;

	// Holds the selected device profile
	UDeviceProfile* ActiveDeviceProfile;

	// Add to profile to get load time backup
	static FString BackupSuffix;

	// Original values of all the CVars modified by the DP.
	// Used to undo the DP before applying new state.
	static TMap<FString, FPushedCVarSetting> PushedSettings;

#if ALLOW_OTHER_PLATFORM_CONFIG
	// Original values of the CVars modified by SetPreviewDeviceProfile.
	TMap<FString, FPushedCVarSetting> PreviewPushedSettings;
#endif
	// Holds the device profile that has been overridden, null no override active.
	UDeviceProfile* BaseDeviceProfile = nullptr;

	// Holds the device profile that we are previewing.
	UDeviceProfile* PreviewDeviceProfile = nullptr;

	// Stores any scalability group settings set by the active device profile.
	static TMap<FString, FString> DeviceProfileScalabilityCVars;
	
	// The list of fragments that have been selected by the active profile.
	static TArray<FSelectedFragmentProperties> PlatformFragmentsSelected;
};
