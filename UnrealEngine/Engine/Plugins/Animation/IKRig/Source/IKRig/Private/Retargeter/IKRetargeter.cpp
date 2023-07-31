// Copyright Epic Games, Inc. All Rights Reserved.

#include "Retargeter/IKRetargeter.h"

#include "IKRigObjectVersion.h"
#include "Retargeter/IKRetargetProfile.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IKRetargeter)

#if WITH_EDITOR
const FName UIKRetargeter::GetSourceIKRigPropertyName() { return GET_MEMBER_NAME_STRING_CHECKED(UIKRetargeter, SourceIKRigAsset); };
const FName UIKRetargeter::GetTargetIKRigPropertyName() { return GET_MEMBER_NAME_STRING_CHECKED(UIKRetargeter, TargetIKRigAsset); };
const FName UIKRetargeter::GetSourcePreviewMeshPropertyName() { return GET_MEMBER_NAME_STRING_CHECKED(UIKRetargeter, SourcePreviewMesh); };
const FName UIKRetargeter::GetTargetPreviewMeshPropertyName() { return GET_MEMBER_NAME_STRING_CHECKED(UIKRetargeter, TargetPreviewMesh); }

void UIKRetargeter::GetSpeedCurveNames(TArray<FName>& OutSpeedCurveNames) const
{
	for (const URetargetChainSettings* ChainSetting : ChainSettings)
	{
		if (ChainSetting->Settings.SpeedPlanting.SpeedCurveName != NAME_None)
		{
			OutSpeedCurveNames.Add(ChainSetting->Settings.SpeedPlanting.SpeedCurveName);
		}
	}
}

#endif

UIKRetargeter::UIKRetargeter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RootSettings = CreateDefaultSubobject<URetargetRootSettings>(TEXT("RootSettings"));
	RootSettings->SetFlags(RF_Transactional);

	GlobalSettings = CreateDefaultSubobject<UIKRetargetGlobalSettings>(TEXT("GlobalSettings"));
	GlobalSettings->SetFlags(RF_Transactional);

	CleanAndInitialize();
}

void UIKRetargeter::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FIKRigObjectVersion::GUID);
};

void UIKRetargeter::PostLoad()
{
	Super::PostLoad();

	// load deprecated chain mapping (pre UStruct to UObject refactor)
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (!ChainMapping_DEPRECATED.IsEmpty())
	{
		for (const FRetargetChainMap& OldChainMap : ChainMapping_DEPRECATED)
		{
			if (OldChainMap.TargetChain == NAME_None)
			{
				continue;
			}
			
			TObjectPtr<URetargetChainSettings>* MatchingChain = ChainSettings.FindByPredicate([&](const URetargetChainSettings* Chain)
			{
				return Chain ? Chain->TargetChain == OldChainMap.TargetChain : false;
			});
			
			if (MatchingChain)
			{
				(*MatchingChain)->SourceChain = OldChainMap.SourceChain;
			}
			else
			{
				TObjectPtr<URetargetChainSettings> NewChainMap = NewObject<URetargetChainSettings>(this, URetargetChainSettings::StaticClass(), NAME_None, RF_Transactional);
				NewChainMap->TargetChain = OldChainMap.TargetChain;
				NewChainMap->SourceChain = OldChainMap.SourceChain;
				ChainSettings.Add(NewChainMap);
			}
		}
	}

	#if WITH_EDITORONLY_DATA
		// load deprecated target actor offset
		if (!FMath::IsNearlyZero(TargetActorOffset_DEPRECATED))
		{
			TargetMeshOffset.X = TargetActorOffset_DEPRECATED;
		}

		// load deprecated target actor scale
		if (!FMath::IsNearlyZero(TargetActorScale_DEPRECATED))
		{
			TargetMeshScale = TargetActorScale_DEPRECATED;
		}

		// load deprecated global settings
		if (!bRetargetRoot_DEPRECATED)
		{
			GlobalSettings->Settings.bEnableRoot = false;
		}
		if (!bRetargetFK_DEPRECATED)
		{
			GlobalSettings->Settings.bEnableFK = false;
		}
		if (!bRetargetIK_DEPRECATED)
		{
			GlobalSettings->Settings.bEnableIK = false;
		}
	#endif

	// load deprecated retarget poses (pre adding retarget poses for source)
	if (!RetargetPoses_DEPRECATED.IsEmpty())
	{
		TargetRetargetPoses = RetargetPoses_DEPRECATED;
	}

	// load deprecated current retarget pose (pre adding retarget poses for source)
	if (CurrentRetargetPose_DEPRECATED != NAME_None)
	{
		CurrentTargetRetargetPose = CurrentRetargetPose_DEPRECATED;
	}
	
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	CleanAndInitialize();
}

void UIKRetargeter::CleanAndInitialize()
{
	// remove null settings
	ChainSettings.Remove(nullptr);

	// use default pose as current pose unless set to something else
	if (CurrentSourceRetargetPose == NAME_None)
	{
		CurrentSourceRetargetPose = GetDefaultPoseName();
	}
	if (CurrentTargetRetargetPose == NAME_None)
	{
		CurrentTargetRetargetPose = GetDefaultPoseName();
	}

	// enforce the existence of a default pose
	if (!SourceRetargetPoses.Contains(GetDefaultPoseName()))
	{
		SourceRetargetPoses.Emplace(GetDefaultPoseName());
	}
	if (!TargetRetargetPoses.Contains(GetDefaultPoseName()))
	{
		TargetRetargetPoses.Emplace(GetDefaultPoseName());
	}

	// ensure current pose exists, otherwise set it to the default pose
	if (!SourceRetargetPoses.Contains(CurrentSourceRetargetPose))
	{
		CurrentSourceRetargetPose = GetDefaultPoseName();
	}
	if (!TargetRetargetPoses.Contains(CurrentTargetRetargetPose))
	{
		CurrentTargetRetargetPose = GetDefaultPoseName();
	}
};

void URetargetChainSettings::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FIKRigObjectVersion::GUID);

	if (Ar.IsLoading())
	{
		// load the old chain settings into the new struct format
		if (Ar.CustomVer(FIKRigObjectVersion::GUID) < FIKRigObjectVersion::ChainSettingsConvertedToStruct)
		{
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			Settings.FK.EnableFK =  CopyPoseUsingFK_DEPRECATED;
			Settings.FK.RotationMode =  RotationMode_DEPRECATED;
			Settings.FK.RotationAlpha =  RotationAlpha_DEPRECATED;
			Settings.FK.TranslationMode =  TranslationMode_DEPRECATED;
			Settings.FK.TranslationAlpha =  TranslationAlpha_DEPRECATED;
			Settings.IK.EnableIK =  DriveIKGoal_DEPRECATED;
			Settings.IK.BlendToSource =  BlendToSource_DEPRECATED;
			Settings.IK.BlendToSourceWeights =  BlendToSourceWeights_DEPRECATED;
			Settings.IK.StaticOffset =  StaticOffset_DEPRECATED;
			Settings.IK.StaticLocalOffset =  StaticLocalOffset_DEPRECATED;
			Settings.IK.StaticRotationOffset =  StaticRotationOffset_DEPRECATED;
			Settings.IK.Extension =  Extension_DEPRECATED;
			Settings.SpeedPlanting.EnableSpeedPlanting =  UseSpeedCurveToPlantIK_DEPRECATED;
			Settings.SpeedPlanting.SpeedCurveName =  SpeedCurveName_DEPRECATED;
			Settings.SpeedPlanting.SpeedThreshold =  VelocityThreshold_DEPRECATED;
			Settings.SpeedPlanting.UnplantStiffness =  UnplantStiffness_DEPRECATED;
			Settings.SpeedPlanting.UnplantCriticalDamping =  UnplantCriticalDamping_DEPRECATED;
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}
	}
}

void URetargetRootSettings::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FIKRigObjectVersion::GUID);

	if (Ar.IsLoading())
	{
		// load the old root settings into the new struct format
		if (Ar.CustomVer(FIKRigObjectVersion::GUID) < FIKRigObjectVersion::ChainSettingsConvertedToStruct)
		{
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			Settings.ScaleHorizontal = GlobalScaleHorizontal_DEPRECATED;
			Settings.ScaleVertical = GlobalScaleVertical_DEPRECATED;
			Settings.BlendToSource = BlendToSource_DEPRECATED.Size();
			Settings.TranslationOffset = StaticOffset_DEPRECATED;
			Settings.RotationOffset = StaticRotationOffset_DEPRECATED;
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}
	}
}

FQuat FIKRetargetPose::GetDeltaRotationForBone(const FName BoneName) const
{
	const FQuat* BoneRotationOffset = BoneRotationOffsets.Find(BoneName);
	return BoneRotationOffset != nullptr ? *BoneRotationOffset : FQuat::Identity;
}

void FIKRetargetPose::SetDeltaRotationForBone(FName BoneName, const FQuat& RotationDelta)
{
	FQuat* RotOffset = BoneRotationOffsets.Find(BoneName);
	if (RotOffset == nullptr)
	{
		// first time this bone has been modified in this pose
		BoneRotationOffsets.Emplace(BoneName, RotationDelta);
		return;
	}

	*RotOffset = RotationDelta;
}

FVector FIKRetargetPose::GetRootTranslationDelta() const
{
	return RootTranslationOffset;
}

void FIKRetargetPose::SetRootTranslationDelta(const FVector& TranslationDelta)
{
	RootTranslationOffset = TranslationDelta;
	// only allow vertical offset of root in retarget pose
	RootTranslationOffset.X = 0.0f;
	RootTranslationOffset.Y = 0.0f;
}

void FIKRetargetPose::AddToRootTranslationDelta(const FVector& TranslateDelta)
{
	RootTranslationOffset += TranslateDelta;
	// only allow vertical offset of root in retarget pose
	RootTranslationOffset.X = 0.0f;
	RootTranslationOffset.Y = 0.0f;
}

void FIKRetargetPose::SortHierarchically(const FIKRigSkeleton& Skeleton)
{
	// sort offsets hierarchically so that they are applied in leaf to root order
	// when generating the component space retarget pose in the processor
	BoneRotationOffsets.KeySort([Skeleton](FName A, FName B)
	{
		return Skeleton.GetBoneIndexFromName(A) > Skeleton.GetBoneIndexFromName(B);
	});
}

const TObjectPtr<URetargetChainSettings> UIKRetargeter::GetChainMapByName(const FName& TargetChainName) const
{
	const TObjectPtr<URetargetChainSettings>* ChainMap = ChainSettings.FindByPredicate(
		[TargetChainName](const TObjectPtr<URetargetChainSettings> ChainMap)
		{
			return ChainMap->TargetChain == TargetChainName;
		});
	
	return !ChainMap ? nullptr : ChainMap->Get();
}

const FTargetChainSettings* UIKRetargeter::GetChainSettingsByName(const FName& TargetChainName) const
{
	const TObjectPtr<URetargetChainSettings> ChainMap = GetChainMapByName(TargetChainName);
	if (ChainMap)
	{
		return &ChainMap->Settings;
	}

	return nullptr;
}

const FIKRetargetPose* UIKRetargeter::GetCurrentRetargetPose(const ERetargetSourceOrTarget& SourceOrTarget) const
{
	return SourceOrTarget == ERetargetSourceOrTarget::Source ? &SourceRetargetPoses[CurrentSourceRetargetPose] : &TargetRetargetPoses[CurrentTargetRetargetPose];
}

FName UIKRetargeter::GetCurrentRetargetPoseName(const ERetargetSourceOrTarget& SourceOrTarget) const
{
	return SourceOrTarget == ERetargetSourceOrTarget::Source ? CurrentSourceRetargetPose : CurrentTargetRetargetPose;
}

const FIKRetargetPose* UIKRetargeter::GetRetargetPoseByName(
	const ERetargetSourceOrTarget& SourceOrTarget,
	const FName PoseName) const
{
	return SourceOrTarget == ERetargetSourceOrTarget::Source ? SourceRetargetPoses.Find(PoseName) : TargetRetargetPoses.Find(PoseName);
}

const FName UIKRetargeter::GetDefaultPoseName()
{
	static const FName DefaultPoseName = "Default Pose";
	return DefaultPoseName;
}

const FRetargetProfile* UIKRetargeter::GetCurrentProfile() const
{
	return GetProfileByName(CurrentProfile);
}

const FRetargetProfile* UIKRetargeter::GetProfileByName(const FName& ProfileName) const
{
	return Profiles.Find(ProfileName);
}

FTargetChainSettings UIKRetargeter::GetChainUsingGoalFromRetargetAsset(
	const UIKRetargeter* RetargetAsset,
	const FName IKGoalName)
{
	FTargetChainSettings EmptySettings;

	if (!RetargetAsset)
	{
		return EmptySettings;
	}

	const UIKRigDefinition* IKRig = RetargetAsset->GetTargetIKRig();
	if (!IKRig)
	{
		return EmptySettings;
	}

	const TArray<FBoneChain>& RetargetChains = IKRig->GetRetargetChains();
	const FBoneChain* ChainWithGoal = nullptr;
	for (const FBoneChain& RetargetChain : RetargetChains)
	{
		if (RetargetChain.IKGoalName == IKGoalName)
		{
			ChainWithGoal = &RetargetChain;
			break;
		}
	}

	if (!ChainWithGoal)
	{
		return EmptySettings;
	}

	// found a chain using the specified goal, return a copy of it's settings
	const FTargetChainSettings* ChainSettings = RetargetAsset->GetChainSettingsByName(ChainWithGoal->ChainName);
	return ChainSettings ? *ChainSettings : EmptySettings;
}

FTargetChainSettings UIKRetargeter::GetChainSettingsFromRetargetAsset(
	const UIKRetargeter* RetargetAsset,
	const FName TargetChainName,
	const FName OptionalProfileName)
{
	FTargetChainSettings OutSettings;
	
	if (!RetargetAsset)
	{
		return OutSettings;
	}
	
	// optionally get the chain settings from a profile
	if (OptionalProfileName != NAME_None)
	{
		if (const FRetargetProfile* RetargetProfile = RetargetAsset->GetProfileByName(OptionalProfileName))
		{
			if (const FTargetChainSettings* ProfileChainSettings = RetargetProfile->ChainSettings.Find(TargetChainName))
			{
				return *ProfileChainSettings;
			}
		}

		// no profile with this chain found, return default settings
		return OutSettings;
	}
	
	// return the chain settings stored in the retargeter (if it has one matching specified name)
	if (const FTargetChainSettings* AssetChainSettings = RetargetAsset->GetChainSettingsByName(TargetChainName))
	{
		return *AssetChainSettings;
	}

	// no chain map with the given target chain, so return default settings
	return OutSettings;
}

FTargetChainSettings UIKRetargeter::GetChainSettingsFromRetargetProfile(
	FRetargetProfile& RetargetProfile,
	const FName TargetChainName)
{
	return RetargetProfile.ChainSettings.FindOrAdd(TargetChainName);
}

void UIKRetargeter::GetRootSettingsFromRetargetAsset(
	const UIKRetargeter* RetargetAsset,
	const FName OptionalProfileName,
	FTargetRootSettings& OutSettings)
{
	if (!RetargetAsset)
	{
		OutSettings = FTargetRootSettings();
		return;
	}
	
	// optionally get the root settings from a profile
	if (OptionalProfileName != NAME_None)
	{
		if (const FRetargetProfile* RetargetProfile = RetargetAsset->GetProfileByName(OptionalProfileName))
		{
			if (RetargetProfile->bApplyRootSettings)
			{
				OutSettings =  RetargetProfile->RootSettings;
				return;
			}
		}
		
		// could not find profile, so return default settings
		OutSettings = FTargetRootSettings();
		return;
	}

	// return the base root settings
	OutSettings =  RetargetAsset->GetRootSettingsUObject()->Settings;
}

FTargetRootSettings UIKRetargeter::GetRootSettingsFromRetargetProfile(FRetargetProfile& RetargetProfile)
{
	return RetargetProfile.RootSettings;
}

void UIKRetargeter::GetGlobalSettingsFromRetargetAsset(
	const UIKRetargeter* RetargetAsset,
	const FName OptionalProfileName,
	FRetargetGlobalSettings& OutSettings)
{
	if (!RetargetAsset)
	{
		OutSettings = FRetargetGlobalSettings();
		return;
	}
	
	// optionally get the root settings from a profile
	if (OptionalProfileName != NAME_None)
	{
		if (const FRetargetProfile* RetargetProfile = RetargetAsset->GetProfileByName(OptionalProfileName))
		{
			if (RetargetProfile->bApplyGlobalSettings)
			{
				OutSettings = RetargetProfile->GlobalSettings;
				return;
			}
		}
		
		// could not find profile, so return default settings
		OutSettings = FRetargetGlobalSettings();
		return;
	}

	// return the base root settings
	OutSettings = RetargetAsset->GetGlobalSettings();
}

FRetargetGlobalSettings UIKRetargeter::GetGlobalSettingsFromRetargetProfile(FRetargetProfile& RetargetProfile)
{
	return RetargetProfile.GlobalSettings;
}

void UIKRetargeter::SetGlobalSettingsInRetargetProfile(
	FRetargetProfile& RetargetProfile,
	const FRetargetGlobalSettings& GlobalSettings)
{
	RetargetProfile.GlobalSettings = GlobalSettings;
	RetargetProfile.bApplyGlobalSettings = true;
}

void UIKRetargeter::SetRootSettingsInRetargetProfile(
	FRetargetProfile& RetargetProfile,
	const FTargetRootSettings& RootSettings)
{
	RetargetProfile.RootSettings = RootSettings;
	RetargetProfile.bApplyRootSettings = true;
}

void UIKRetargeter::SetChainSettingsInRetargetProfile(
	FRetargetProfile& RetargetProfile,
	const FTargetChainSettings& ChainSettings,
	const FName TargetChainName)
{
	RetargetProfile.ChainSettings.Add(TargetChainName, ChainSettings);
	RetargetProfile.bApplyChainSettings = true;
}

void UIKRetargeter::SetChainFKSettingsInRetargetProfile(
	FRetargetProfile& RetargetProfile,
	const FTargetChainFKSettings& FKSettings,
	const FName TargetChainName)
{
	FTargetChainSettings& ChainSettings = RetargetProfile.ChainSettings.FindOrAdd(TargetChainName);
	ChainSettings.FK = FKSettings;
	RetargetProfile.bApplyChainSettings = true;
}

void UIKRetargeter::SetChainIKSettingsInRetargetProfile(
	FRetargetProfile& RetargetProfile,
	const FTargetChainIKSettings& IKSettings,
	const FName TargetChainName)
{
	FTargetChainSettings& ChainSettings = RetargetProfile.ChainSettings.FindOrAdd(TargetChainName);
	ChainSettings.IK = IKSettings;
	RetargetProfile.bApplyChainSettings = true;
}

void UIKRetargeter::SetChainSpeedPlantSettingsInRetargetProfile(
	FRetargetProfile& RetargetProfile,
	const FTargetChainSpeedPlantSettings& SpeedPlantSettings,
	const FName TargetChainName)
{
	FTargetChainSettings& ChainSettings = RetargetProfile.ChainSettings.FindOrAdd(TargetChainName);
	ChainSettings.SpeedPlanting = SpeedPlantSettings;
	RetargetProfile.bApplyChainSettings = true;
}
