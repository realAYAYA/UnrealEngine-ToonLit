// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MetasoundFrontendDocument.h"
#include "UObject/NoExportTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "PerPlatformProperties.h"

#include "MetasoundSettings.generated.h"


// Forward Declarations
struct FMetasoundFrontendClassName;
struct FPropertyChangedChainEvent;

UENUM()
enum class EMetaSoundMessageLevel : uint8
{
	Error,
	Warning,
	Info
};

USTRUCT()
struct METASOUNDENGINE_API FDefaultMetaSoundAssetAutoUpdateSettings
{
	GENERATED_BODY()

	/** MetaSound to prevent from AutoUpdate. */
	UPROPERTY(EditAnywhere, Category = "AutoUpdate", meta = (AllowedClasses = "/Script/MetasoundEngine.MetaSound, /Script/MetasoundEngine.MetaSoundSource"))
	FSoftObjectPath MetaSound;
};

UCLASS(Hidden)
class METASOUNDENGINE_API UMetaSoundQualityHelper : public UObject
{
	GENERATED_BODY()

public:
	/**
	* Returns a list of quality settings to present to a combobox
	* */
	UFUNCTION()
	static TArray<FName> GetQualityList();
};

USTRUCT()
struct METASOUNDENGINE_API FMetaSoundQualitySettings
{
	GENERATED_BODY()
	
#if WITH_EDITORONLY_DATA

	/** A hidden GUID that will be generated once when adding a new entry. This prevents orphaning of renamed entries. **/
	UPROPERTY()
	FGuid UniqueId = {};

	/** Name of this quality setting. This will appear in the quality dropdown list.
		The names should be unique and adequately describe the Entry. "High", "Low" etc. **/
	UPROPERTY(EditAnywhere, Category = "Quality")
	FName Name = {};
	
#endif //WITH_EDITORONLY_DATA	

	/** Sample Rate (in Hz). NOTE: A Zero value will have no effect and use the Device Rate. **/
	UPROPERTY(EditAnywhere, config, Category = "Quality", meta = (ClampMin = "0", ClampMax="96000"))
	FPerPlatformInt SampleRate = 0;

	/** Block Rate (in Hz). NOTE: A Zero value will have no effect and use the Default (100)  **/
	UPROPERTY(EditAnywhere, config, Category = "Quality", meta = (ClampMin = "0", ClampMax="1000"))
	FPerPlatformFloat BlockRate = 0.f;
};


UCLASS(config = MetaSound, defaultconfig, meta = (DisplayName = "MetaSounds"))
class METASOUNDENGINE_API UMetaSoundSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** If true, AutoUpdate is enabled, increasing load times.  If false, skips AutoUpdate on load, but can result in MetaSounds failing to load, 
	  * register, and execute if interface differences are present. */
	UPROPERTY(EditAnywhere, config, Category = AutoUpdate)
	bool bAutoUpdateEnabled = true;

	/** List of native MetaSound classes whose node references should not be AutoUpdated. */
	UPROPERTY(EditAnywhere, config, Category = AutoUpdate, meta = (DisplayName = "DenyList", EditCondition = "bAutoUpdateEnabled"))
	TArray<FMetasoundFrontendClassName> AutoUpdateDenylist;

	/** List of MetaSound assets whose node references should not be AutoUpdated. */
	UPROPERTY(EditAnywhere, config, Category = AutoUpdate, meta = (DisplayName = "Asset DenyList", EditCondition = "bAutoUpdateEnabled"))
	TArray<FDefaultMetaSoundAssetAutoUpdateSettings> AutoUpdateAssetDenylist;

	/** If true, warnings will be logged if updating a node results in existing connections being discarded. */
	UPROPERTY(EditAnywhere, config, Category = AutoUpdate, meta = (DisplayName = "Log Warning on Dropped Connection", EditCondition = "bAutoUpdateEnabled"))
	bool bAutoUpdateLogWarningOnDroppedConnection = true;

	/** Directories to scan & automatically register MetaSound post initial asset scan on engine start-up.
	  * May speed up subsequent calls to playback MetaSounds post asset scan but increases application load time.
	  * See 'MetaSoundAssetSubsystem::RegisterAssetClassesInDirectories' to dynamically register or 
	  * 'MetaSoundAssetSubsystem::UnregisterAssetClassesInDirectories' to unregister asset classes.
	  */
	UPROPERTY(EditAnywhere, config, Category = Registration, meta = (RelativePath, LongPackageName))
	TArray<FDirectoryPath> DirectoriesToRegister;
		
	UPROPERTY(Transient)
	int32 DenyListCacheChangeID = 0;	

#if WITH_EDITORONLY_DATA
	const TArray<FMetaSoundQualitySettings>& GetQualitySettings() const { return QualitySettings; }
	static FName GetQualitySettingPropertyName(); 
#endif //WITH_EDITORONLY_DATA

private:

	/** Array of possible quality settings for Metasounds to chose from */
	// NOTE: Ideally this would be wrapped with WITH_EDITORONLY_DATA, but standalone "-game" requires
	// it to exist. Access is limited to the accessor above, which enforces it correctly.
	UPROPERTY(EditAnywhere, config, Category = Quality)
	TArray<FMetaSoundQualitySettings> QualitySettings;

#if WITH_EDITOR
private:

	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
};

