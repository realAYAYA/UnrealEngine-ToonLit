// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Behaviour/RCBehaviour.h"
#include "RCVirtualProperty.h"
#include "RemoteControlEntity.h"

#include "RCSetAssetByPathBehaviour.generated.h"

class URCVirtualPropertyContainerBase;
class URCUserDefinedStruct;
class UTexture;

namespace SetAssetByPathBehaviourHelpers
{
	const FString ContentFolder = FString(TEXT("/Game/"));
	const FString InputToken = FString(TEXT("{INPUT}"));
	const FName TargetProperty = FName(TEXT("Target Property"));
	const FName DefaultInput = FName(TEXT("Default Input"));
	const FName SetAssetByPathBehaviour = FName(TEXT("Set Asset By Path"));
}

/** Struct to help generate Widgts for the DetailsPanel of the Bahviour */
USTRUCT()
struct FRCSetAssetPath
{
	GENERATED_BODY()

	FRCSetAssetPath()
	{
		// Add Empty index 0 when creating 
		PathArray.Add("");
	}
	
	/** An Array of Strings holding the Path of an Asset, seperated in several String. Will concated back together later. */
	UPROPERTY()
	TArray<FString> PathArray;
};

/**
 * Custom behaviour for Set Asset By Path
 */
UCLASS()
class REMOTECONTROLLOGIC_API URCSetAssetByPathBehaviour : public URCBehaviour
{
	GENERATED_BODY()
public:
	URCSetAssetByPathBehaviour();

	//~ Begin URCBehaviour interface
	virtual void Initialize() override;
	//~ End URCBehaviour interface

	/** Given an Input Path, sets the Target Exposed Propterty to the Asset. */
	bool SetAssetByPath(const FString& AssetPath, const FString& DefaultString);

	/** List of Supported Assets SetAssetByPath Logic Behaviour can set and change. */
	TArray<UClass*> GetSupportedClasses() const;

	/** Returns the Path as the completed version, including Token Inputs. */
	FString GetCurrentPath();

	/** Sets the target entity upon which the asset will be set upon. */
	void SetTargetEntity(const TSharedPtr<const FRemoteControlEntity>& InEntity);

	/** Returns a Pointer to the current Target Entityy */
	TWeakPtr<const FRemoteControlEntity> GetTargetEntity() const;

	/** Auxialiary Function to apply to update the Target Texture */
	void UpdateTargetEntity();

	/** Called whenever a change has occured in the Slate */
	void RefreshPathArray();
	
public:
	/** Pointer to property container */
	UPROPERTY()
	TObjectPtr<URCVirtualPropertyContainerBase> PropertyInContainer;

	/** Pointer to the current Class the Asset will use to set the Targeted Exposed Object. */
	UPROPERTY()
	TObjectPtr<UClass> AssetClass = GetSupportedClasses()[0];

	/** Struct holding the Path Information to help load Assets. */
	UPROPERTY()
	FRCSetAssetPath PathStruct;

	/** Bool used to help tell if the path given is an internal or external one. */
	UPROPERTY()
	bool bInternal = true;

private:
	/** Targeted Property Id */
	UPROPERTY()
	FGuid TargetEntityId;

	/** Internal Targeted Property, used for any of the setter operations */
	TSharedPtr<const FRemoteControlEntity> TargetEntity;
	
private:
	/** Auxiliary Function which sets the given SetterObject Asset onto the Exposed Asset with the given PropertyString name. */
	bool SetInternalAsset(UObject* SetterObject);

	/** Given an External Path towards a external location, loads the asset associated and places it onto an object */
	bool SetExternalAsset(FString InExternalPath);

	/** Auxiliary Function to apply a Texture onto a given Property */
	bool SetTextureAsset(TSharedPtr<FRemoteControlProperty> InRemoteControlPropertyPtr, UTexture* InObject);

};
