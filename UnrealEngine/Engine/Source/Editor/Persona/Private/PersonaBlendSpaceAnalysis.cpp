// Copyright Epic Games, Inc. All Rights Reserved.
#include "PersonaBlendSpaceAnalysis.h"

#include "AnimPose.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Animation/BlendSpace.h"
#include "Animation/AnimSequence.h"
#include "Animation/BoneSocketReference.h"
#include "Features/IModularFeatures.h"

#define LOCTEXT_NAMESPACE "BlendSpaceAnalysis"

//======================================================================================================================
void UCachedAnalysisProperties::CopyFrom(const UCachedAnalysisProperties& Other)
{
	LinearFunctionAxis = Other.LinearFunctionAxis;
	EulerFunctionAxis = Other.EulerFunctionAxis;
	BoneSocket1 = Other.BoneSocket1;
	BoneSocket2 = Other.BoneSocket2;
	BoneFacingAxis = Other.BoneFacingAxis;
	BoneRightAxis = Other.BoneRightAxis;
	Space = Other.Space;
	SpaceBoneSocket = Other.SpaceBoneSocket;
	CharacterFacingAxis = Other.CharacterFacingAxis;
	CharacterUpAxis = Other.CharacterUpAxis;
	StartTimeFraction = Other.StartTimeFraction;
	EndTimeFraction = Other.EndTimeFraction;
}

//======================================================================================================================
FVector BlendSpaceAnalysis::GetAxisFromTM(const FTransform& TM, EAnalysisLinearAxis Axis)
{
	switch (Axis)
	{
	case EAnalysisLinearAxis::PlusX: return TM.TransformVectorNoScale(FVector(1, 0, 0));
	case EAnalysisLinearAxis::PlusY: return TM.TransformVectorNoScale(FVector(0, 1, 0));
	case EAnalysisLinearAxis::PlusZ: return TM.TransformVectorNoScale(FVector(0, 0, 1));
	case EAnalysisLinearAxis::MinusX: return TM.TransformVectorNoScale(FVector(-1, 0, 0));
	case EAnalysisLinearAxis::MinusY: return TM.TransformVectorNoScale(FVector(0, -1, 0));
	case EAnalysisLinearAxis::MinusZ: return TM.TransformVectorNoScale(FVector(0, 0, -1));
	}
	return FVector(0, 0, 0);
}

//======================================================================================================================
bool BlendSpaceAnalysis::GetBoneInfo(const UAnimSequence&     Animation, 
									 const FBoneSocketTarget& BoneSocket, 
									 FTransform&              BoneOffset, 
									 FName&                   BoneName)
{
	if (BoneSocket.bUseSocket)
	{
		const USkeleton* Skeleton = Animation.GetSkeleton();
		const USkeletalMeshSocket* Socket = Skeleton->FindSocket(BoneSocket.SocketReference.SocketName);
		if (Socket)
		{
			BoneOffset = Socket->GetSocketLocalTransform();
			BoneName = Socket->BoneName;
			return !BoneName.IsNone();
		}
	}
	else
	{
		BoneName = BoneSocket.BoneReference.BoneName;
		return !BoneName.IsNone();
	}
	return false;
}

//======================================================================================================================
FTransform BlendSpaceAnalysis::GetBoneTransform(const UAnimSequence& Animation, int32 Key, const FName& BoneName)
{
	FAnimPose AnimPose;
	UAnimPoseExtensions::GetAnimPoseAtFrame(&Animation, Key, FAnimPoseEvaluationOptions(), AnimPose);
	FTransform BoneTM = UAnimPoseExtensions::GetBonePose(AnimPose, BoneName, EAnimPoseSpaces::World);
	return BoneTM;
}


//======================================================================================================================
void ULinearAnalysisProperties::InitializeFromCache(TObjectPtr<UCachedAnalysisProperties> Cache)
{
	if (Cache)
	{
		UCachedAnalysisProperties* CachePtr = Cache.Get();
		FunctionAxis = CachePtr->LinearFunctionAxis;
		BoneSocket = CachePtr->BoneSocket1;
		Space = CachePtr->Space;
		SpaceBoneSocket = CachePtr->SpaceBoneSocket;
		StartTimeFraction = CachePtr->StartTimeFraction;
		EndTimeFraction = CachePtr->EndTimeFraction;
	}
}

//======================================================================================================================
void ULinearAnalysisProperties::MakeCache(TObjectPtr<UCachedAnalysisProperties>& Cache, UBlendSpace* BlendSpace)
{
	UCachedAnalysisProperties* CachePtr = Cache.Get();
	if (!Cache)
	{
		Cache = NewObject<UCachedAnalysisProperties>(BlendSpace);
		CachePtr = Cache.Get();
	}
	CachePtr->LinearFunctionAxis = FunctionAxis;
	CachePtr->BoneSocket1 = BoneSocket;
	CachePtr->Space = Space;
	CachePtr->SpaceBoneSocket = SpaceBoneSocket;
	CachePtr->StartTimeFraction = StartTimeFraction;
	CachePtr->EndTimeFraction = EndTimeFraction;
}

//======================================================================================================================
void UEulerAnalysisProperties::InitializeFromCache(TObjectPtr<UCachedAnalysisProperties> Cache)
{
	if (Cache)
	{
		UCachedAnalysisProperties* CachePtr = Cache.Get();
		FunctionAxis = CachePtr->EulerFunctionAxis;
		BoneSocket = CachePtr->BoneSocket1;
		BoneFacingAxis = CachePtr->BoneFacingAxis;
		BoneRightAxis = CachePtr->BoneRightAxis;
		Space = CachePtr->Space;
		SpaceBoneSocket = CachePtr->SpaceBoneSocket;
		CharacterFacingAxis = CachePtr->CharacterFacingAxis;
		CharacterUpAxis = CachePtr->CharacterUpAxis;
		StartTimeFraction = CachePtr->StartTimeFraction;
		EndTimeFraction = CachePtr->EndTimeFraction;
	}
}

//======================================================================================================================
void UEulerAnalysisProperties::MakeCache(TObjectPtr<UCachedAnalysisProperties>& Cache, UBlendSpace* BlendSpace)
{
	UCachedAnalysisProperties* CachePtr = Cache.Get();
	if (!Cache)
	{
		Cache = NewObject<UCachedAnalysisProperties>(BlendSpace);
		CachePtr = Cache.Get();
	}
	CachePtr->EulerFunctionAxis = FunctionAxis;
	CachePtr->BoneSocket1 = BoneSocket;
	CachePtr->BoneFacingAxis = BoneFacingAxis;
	CachePtr->BoneRightAxis = BoneRightAxis;
	CachePtr->Space = Space;
	CachePtr->SpaceBoneSocket = SpaceBoneSocket;
	CachePtr->CharacterFacingAxis = CharacterFacingAxis;
	CachePtr->CharacterUpAxis = CharacterUpAxis;
	CachePtr->StartTimeFraction = StartTimeFraction;
	CachePtr->EndTimeFraction = EndTimeFraction;
}

//======================================================================================================================
class FCoreBlendSpaceAnalysisFeature : public IBlendSpaceAnalysisFeature
{
public:
	// This should process the animation according to the analysis properties, or return false if that is not possible.
	bool CalculateSampleValue(float&                     Result,
							  const UBlendSpace&         BlendSpace,
							  const UAnalysisProperties* AnalysisProperties,
							  const UAnimSequence&       Animation,
							  const float                RateScale) const override;

	// This should return an instance derived from UAnalysisProperties that is suitable for the FunctionName
	UAnalysisProperties* MakeAnalysisProperties(UObject* Outer, const FString& FunctionName) const override;

	// This should return the names of the functions handled
	TArray<FString> GetAnalysisFunctions() const override;
};

static FCoreBlendSpaceAnalysisFeature CoreBlendSpaceAnalysisFeature;

//======================================================================================================================
TArray<FString> FCoreBlendSpaceAnalysisFeature::GetAnalysisFunctions() const
{
	TArray<FString> Functions = 
	{
		TEXT("None"),
		TEXT("Position"),
		TEXT("Velocity"),
		TEXT("DeltaPosition"),
		TEXT("Orientation"),
		TEXT("OrientationRate"),
		TEXT("DeltaOrientation"),
		TEXT("AngularVelocity")
	};
	return Functions;
}

//======================================================================================================================
UAnalysisProperties* FCoreBlendSpaceAnalysisFeature::MakeAnalysisProperties(UObject* Outer, const FString& FunctionName) const
{
	UAnalysisProperties* Result = nullptr;
	if (FunctionName.Equals(TEXT("Position")) ||
		FunctionName.Equals(TEXT("Velocity")) ||
		FunctionName.Equals(TEXT("DeltaPosition")) ||
		FunctionName.Equals(TEXT("AngularVelocity")))
	{
		Result = NewObject<ULinearAnalysisProperties>(Outer);
	}
	else if (FunctionName.Equals(TEXT("Orientation")) ||
			 FunctionName.Equals(TEXT("OrientationRate")) ||
			 FunctionName.Equals(TEXT("DeltaOrientation")))
	{
		Result = NewObject<UEulerAnalysisProperties>(Outer);
	}

	if (Result)
	{
		Result->Function = FunctionName;
	}
	return Result;
}

//======================================================================================================================
bool FCoreBlendSpaceAnalysisFeature::CalculateSampleValue(float&                     Result,
														  const UBlendSpace&         BlendSpace,
														  const UAnalysisProperties* AnalysisProperties,
														  const UAnimSequence&       Animation,
														  const float                RateScale) const
{
	if (!AnalysisProperties)
	{
		return false;
	}
	const FString& FunctionName = AnalysisProperties->Function;
	if (FunctionName.Equals(TEXT("Position")))
	{
		return BlendSpaceAnalysis::CalculateComponentSampleValue(
			Result, BlendSpaceAnalysis::CalculatePosition<ULinearAnalysisProperties>, BlendSpace, 
			Cast<ULinearAnalysisProperties>(AnalysisProperties), Animation, RateScale);
	}
	else if (FunctionName.Equals(TEXT("Velocity")))
	{
		return BlendSpaceAnalysis::CalculateComponentSampleValue(
			Result, BlendSpaceAnalysis::CalculateVelocity<ULinearAnalysisProperties>, BlendSpace, 
			Cast<ULinearAnalysisProperties>(AnalysisProperties), Animation, RateScale);
	}
	else if (FunctionName.Equals(TEXT("DeltaPosition")))
	{
		return BlendSpaceAnalysis::CalculateComponentSampleValue(
			Result, BlendSpaceAnalysis::CalculateDeltaPosition<ULinearAnalysisProperties>, BlendSpace, 
			Cast<ULinearAnalysisProperties>(AnalysisProperties), Animation, RateScale);
	}
	else if (FunctionName.Equals(TEXT("AngularVelocity")))
	{
		return BlendSpaceAnalysis::CalculateComponentSampleValue(
			Result, BlendSpaceAnalysis::CalculateAngularVelocity<ULinearAnalysisProperties>, BlendSpace, 
			Cast<ULinearAnalysisProperties>(AnalysisProperties), Animation, RateScale);
	}
	else if (FunctionName.Equals(TEXT("Orientation")))
	{
		return BlendSpaceAnalysis::CalculateComponentSampleValue(
			Result, BlendSpaceAnalysis::CalculateOrientation<UEulerAnalysisProperties>, BlendSpace, 
			Cast<UEulerAnalysisProperties>(AnalysisProperties), Animation, RateScale);
	}
	else if (FunctionName.Equals(TEXT("OrientationRate")))
	{
		return BlendSpaceAnalysis::CalculateComponentSampleValue(
			Result, BlendSpaceAnalysis::CalculateOrientationRate<UEulerAnalysisProperties>, BlendSpace, 
			Cast<UEulerAnalysisProperties>(AnalysisProperties), Animation, RateScale);
	}
	else if (FunctionName.Equals(TEXT("DeltaOrientation")))
	{
		return BlendSpaceAnalysis::CalculateComponentSampleValue(
			Result, BlendSpaceAnalysis::CalculateDeltaOrientation<UEulerAnalysisProperties>, BlendSpace, 
			Cast<UEulerAnalysisProperties>(AnalysisProperties), Animation, RateScale);
	}
	return false;
}

//======================================================================================================================
static TArray<IBlendSpaceAnalysisFeature*> GetAnalysisFeatures(bool bCoreFeaturesLast)
{
	TArray<IBlendSpaceAnalysisFeature*> ModularFeatures;

	if (!bCoreFeaturesLast)
	{
		ModularFeatures.Push(&CoreBlendSpaceAnalysisFeature);
	}

	TArray<IBlendSpaceAnalysisFeature*> ExtraModularFeatures = 
		IModularFeatures::Get().GetModularFeatureImplementations<IBlendSpaceAnalysisFeature>(
			IBlendSpaceAnalysisFeature::GetModuleFeatureName());
	ModularFeatures += ExtraModularFeatures;

	if (bCoreFeaturesLast)
	{
		ModularFeatures.Push(&CoreBlendSpaceAnalysisFeature);
	}
	return ModularFeatures;
}

//======================================================================================================================
FVector BlendSpaceAnalysis::CalculateSampleValue(const UBlendSpace& BlendSpace, const UAnimSequence& Animation,
                                                 const float RateScale, const FVector& OriginalPosition, bool bAnalyzed[3])
{
	FVector AdjustedPosition = OriginalPosition;
	TArray<IBlendSpaceAnalysisFeature*> ModularFeatures = GetAnalysisFeatures(true);
	for (int32 Index = 0; Index != 2; ++Index)
	{
		bAnalyzed[Index] = false;
		const UAnalysisProperties* AnalysisProperties = BlendSpace.AnalysisProperties[Index].Get();
		for (const IBlendSpaceAnalysisFeature* Feature : ModularFeatures)
		{
			float NewPosition = float(AdjustedPosition[Index]);
			bAnalyzed[Index] = Feature->CalculateSampleValue(
				NewPosition, BlendSpace, AnalysisProperties, Animation, RateScale);
			if (bAnalyzed[Index])
			{
				AdjustedPosition[Index] = NewPosition;
				break;
			}
		}
	}
	return AdjustedPosition;
}

//======================================================================================================================
UAnalysisProperties* BlendSpaceAnalysis::MakeAnalysisProperties(UObject* Outer, const FString& FunctionName)
{
	UAnalysisProperties* Result = nullptr;

	TArray<IBlendSpaceAnalysisFeature*> ModularFeatures = GetAnalysisFeatures(true);
	for (const IBlendSpaceAnalysisFeature* Feature : ModularFeatures)
	{
		Result = Feature->MakeAnalysisProperties(Outer, FunctionName);
		if (Result)
		{
			// Need to explicitly set flags to make undo work on the new object
			Result->SetFlags(RF_Transactional);
			return Result;
		}
	}
	return Result;
}

//======================================================================================================================
TArray<FString> BlendSpaceAnalysis::GetAnalysisFunctions()
{
	TArray<FString> FunctionNames;
	TArray<IBlendSpaceAnalysisFeature*> ModularFeatures = GetAnalysisFeatures(false);
	for (const IBlendSpaceAnalysisFeature* Feature : ModularFeatures)
	{
		TArray<FString> FeatureFunctionNames = Feature->GetAnalysisFunctions();
		for (const FString& FeatureFunctionName : FeatureFunctionNames)
		{
			FunctionNames.AddUnique(FeatureFunctionName);
		}
	}
	return FunctionNames;
}

#undef LOCTEXT_NAMESPACE
