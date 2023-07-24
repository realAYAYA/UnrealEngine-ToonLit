// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Engine/AssetUserData.h"
#include "AssetGuideline.generated.h"

// Helper struct for setting string console ini values.
USTRUCT()
struct UNREALED_API FIniStringValue
{
	GENERATED_BODY()
		
	/** From .ini. Ex: /Script/Engine.RendererSettings */
	UPROPERTY(EditAnywhere, Category = General)
	FString Section;

	/** From .ini. Ex: r.GPUSkin.Support16BitBoneIndex */
	UPROPERTY(EditAnywhere, Category = General)
	FString Key;

	/** From .ini. Ex: True */
	UPROPERTY(EditAnywhere, Category = General)
	FString Value;

	/** From .ini, relative to {PROJECT}. Ex: /Config/DefaultEngine.ini */
	UPROPERTY(EditAnywhere, Category = General)
	FString Filename;
};

/**
* User data that can be attached to assets to check on load for guidlelines (plugins, project settings, etc).
*
* This class intentionally does not accept FText arguments. The project using your bundled asset would need to have
* your localization tables, and we currently do not support text table referencing.
*/
UCLASS(Blueprintable)
class UNREALED_API UAssetGuideline : public UAssetUserData
{
	GENERATED_BODY()

public:

#if WITH_EDITORONLY_DATA
	/** Plugins to check for on load */
	UPROPERTY(EditAnywhere, Category = General)
	TArray<FString> Plugins;

	/** Project settings to check for on load. Look at your .ini's to populate this. */
	UPROPERTY(EditAnywhere, Category = General, Meta = (TitleProperty = "Key"))
	TArray<FIniStringValue> ProjectSettings;

	/** Name of this guideline, we will only check once per unique guideline name. */
	UPROPERTY(EditAnywhere, Category = General)
	FName GuidelineName;

private:

	TWeakPtr<class SNotificationItem> NotificationPtr;
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
public:
	/** Begin UObject interface */
	virtual bool IsPostLoadThreadSafe() const override;
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
	/** End UObject interface */

private:
	void EnableMissingGuidelines(TArray<FString> IncorrectPlugins, TArray<FIniStringValue> IncorrectProjectSettings);
	void DismissNotifications();
	void RemoveAssetGuideline();
#endif // WITH_EDITOR
};
