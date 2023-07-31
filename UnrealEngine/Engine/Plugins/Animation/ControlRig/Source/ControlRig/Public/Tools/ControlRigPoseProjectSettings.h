// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
/**
* Per Project user settings for Control Rig Poses(and maybe animations etc).
*/
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "ControlRigPoseProjectSettings.generated.h"


UCLASS(config = EditorPerProjectUserSettings)
class CONTROLRIG_API UControlRigPoseProjectSettings : public UObject
{

public:
	GENERATED_BODY()

	UControlRigPoseProjectSettings();

	/** The pose asset path  */
	TArray<FString> GetAssetPaths() const;

	/** The root of the directory in which to save poses */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Control Rig Poses", meta = (ContentDir))
	TArray<FDirectoryPath> RootSaveDirs;

	/** Not used but may put bad if we support other types.
	bool bFilterPoses;

	bool bFilterAnimations;

	bool bFilterSelectionSets;
	*/
};
