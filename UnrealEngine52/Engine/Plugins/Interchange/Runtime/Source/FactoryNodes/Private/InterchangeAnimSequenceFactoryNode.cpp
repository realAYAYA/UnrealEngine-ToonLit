// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeAnimSequenceFactoryNode.h"
#include "Animation/AnimSequence.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeAnimSequenceFactoryNode)

namespace UE::Interchange::Animation
{
	FFrameRate ConvertSampleRatetoFrameRate(double SampleRate)
	{
		double IntegralPart = FMath::FloorToDouble(SampleRate);
		double FractionalPart = SampleRate - IntegralPart;
		const int32 Tolerance = 1000000;
		double Divisor = static_cast<double>(FMath::GreatestCommonDivisor(FMath::RoundToInt(FractionalPart * Tolerance), Tolerance));
		int32 Denominator = static_cast<int32>(Tolerance / Divisor);
		int32 Numerator = static_cast<int32>(IntegralPart * Denominator + FMath::RoundToDouble(FractionalPart * Tolerance) / Divisor);
		check(Denominator != 0);
		return FFrameRate(Numerator, Denominator);
	}
}

namespace UE::Interchange
{
	const FAttributeKey& FAnimSequenceNodeStaticData::GetAnimatedMorphTargetDependenciesKey()
	{
		static FAttributeKey AttributeKey(TEXT("__AnimatedMeshMorphTargetDependencies__"));
		return AttributeKey;
	}
	
	const FAttributeKey& FAnimSequenceNodeStaticData::GetAnimatedAttributeCurveNamesKey()
	{
		static FAttributeKey AttributeKey(TEXT("__AnimatedAttributeCurveNames__"));
		return AttributeKey;
	}
	
	const FAttributeKey& FAnimSequenceNodeStaticData::GetAnimatedAttributeStepCurveNamesKey()
	{
		static FAttributeKey AttributeKey(TEXT("__AnimatedAttributeStepCurveNames__"));
		return AttributeKey;
	}

	const FAttributeKey& FAnimSequenceNodeStaticData::GetAnimatedMaterialCurveSuffixesKey()
	{
		static FAttributeKey AttributeKey(TEXT("__AnimatedMaterialCurveSuffixes__"));
		return AttributeKey;
	}

	const FAttributeKey& FAnimSequenceNodeStaticData::GetSceneNodeAnimationPayloadKeyMapKey()
	{
		static FAttributeKey AttributeKey(TEXT("__SceneNodeAnimationPayloadKeyMap__"));
		return AttributeKey;
	}

	const FAttributeKey& FAnimSequenceNodeStaticData::GetMorphTargetNodePayloadKeyMapKey()
	{
		static FAttributeKey AttributeKey(TEXT("__MorphTargetNodePayloadKeyMap__"));
		return AttributeKey;
	}
}//ns UE::Interchange


UInterchangeAnimSequenceFactoryNode::UInterchangeAnimSequenceFactoryNode()
{
	AnimatedMorphTargetDependencies.Initialize(Attributes, UE::Interchange::FAnimSequenceNodeStaticData::GetAnimatedMorphTargetDependenciesKey().ToString());
	AnimatedAttributeCurveNames.Initialize(Attributes, UE::Interchange::FAnimSequenceNodeStaticData::GetAnimatedAttributeCurveNamesKey().ToString());
	AnimatedAttributeStepCurveNames.Initialize(Attributes, UE::Interchange::FAnimSequenceNodeStaticData::GetAnimatedAttributeStepCurveNamesKey().ToString());
	AnimatedMaterialCurveSuffixes.Initialize(Attributes, UE::Interchange::FAnimSequenceNodeStaticData::GetAnimatedMaterialCurveSuffixesKey().ToString());

	SceneNodeAnimationPayloadKeyMap.Initialize(Attributes.ToSharedRef(), UE::Interchange::FAnimSequenceNodeStaticData::GetSceneNodeAnimationPayloadKeyMapKey().ToString());
	MorphTargetNodePayloadKeyMap.Initialize(Attributes.ToSharedRef(), UE::Interchange::FAnimSequenceNodeStaticData::GetMorphTargetNodePayloadKeyMapKey().ToString());
}

void UInterchangeAnimSequenceFactoryNode::InitializeAnimSequenceNode(const FString& UniqueID, const FString& DisplayLabel)
{
	InitializeNode(UniqueID, DisplayLabel, EInterchangeNodeContainerType::FactoryData);
}

FString UInterchangeAnimSequenceFactoryNode::GetTypeName() const
{
	const FString TypeName = TEXT("AnimSequenceNode");
	return TypeName;
}

FString UInterchangeAnimSequenceFactoryNode::GetKeyDisplayName(const UE::Interchange::FAttributeKey& NodeAttributeKey) const
{
	FString KeyDisplayName = NodeAttributeKey.ToString();
	if (NodeAttributeKey == Macro_CustomSkeletonFactoryNodeUidKey)
	{
		KeyDisplayName = TEXT("Skeleton Uid");
	}
	else if (NodeAttributeKey == Macro_CustomSkeletonSoftObjectPathKey)
	{
		KeyDisplayName = TEXT("Specified Existing Skeleton");
	}
	else
	{
		KeyDisplayName = Super::GetKeyDisplayName(NodeAttributeKey);
	}
	return KeyDisplayName;
}

UClass* UInterchangeAnimSequenceFactoryNode::GetObjectClass() const
{
	return UAnimSequence::StaticClass();
}

bool UInterchangeAnimSequenceFactoryNode::GetCustomSkeletonFactoryNodeUid(FString& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(SkeletonFactoryNodeUid, FString);
}

bool UInterchangeAnimSequenceFactoryNode::SetCustomSkeletonFactoryNodeUid(const FString& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(SkeletonFactoryNodeUid, FString);
}

bool UInterchangeAnimSequenceFactoryNode::GetCustomImportBoneTracks(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(ImportBoneTracks, bool);
}

bool UInterchangeAnimSequenceFactoryNode::SetCustomImportBoneTracks(const bool& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(ImportBoneTracks, bool);
}

bool UInterchangeAnimSequenceFactoryNode::GetCustomImportBoneTracksSampleRate(double& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(ImportBoneTracksSampleRate, double);
}

bool UInterchangeAnimSequenceFactoryNode::SetCustomImportBoneTracksSampleRate(const double& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(ImportBoneTracksSampleRate, double);
}

bool UInterchangeAnimSequenceFactoryNode::GetCustomImportBoneTracksRangeStart(double& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(ImportBoneTracksRangeStart, double);
}

bool UInterchangeAnimSequenceFactoryNode::SetCustomImportBoneTracksRangeStart(const double& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(ImportBoneTracksRangeStart, double);
}

bool UInterchangeAnimSequenceFactoryNode::GetCustomImportBoneTracksRangeStop(double& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(ImportBoneTracksRangeStop, double);
}

bool UInterchangeAnimSequenceFactoryNode::SetCustomImportBoneTracksRangeStop(const double& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(ImportBoneTracksRangeStop, double);
}

bool UInterchangeAnimSequenceFactoryNode::GetCustomImportAttributeCurves(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(ImportAttributeCurves, bool);
}

bool UInterchangeAnimSequenceFactoryNode::SetCustomImportAttributeCurves(const bool& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(ImportAttributeCurves, bool);
}

bool UInterchangeAnimSequenceFactoryNode::GetCustomDoNotImportCurveWithZero(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(DoNotImportCurveWithZero, bool);
}

bool UInterchangeAnimSequenceFactoryNode::SetCustomDoNotImportCurveWithZero(const bool& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(DoNotImportCurveWithZero, bool);
}

bool UInterchangeAnimSequenceFactoryNode::GetCustomRemoveCurveRedundantKeys(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(RemoveCurveRedundantKeys, bool);
}

bool UInterchangeAnimSequenceFactoryNode::SetCustomRemoveCurveRedundantKeys(const bool& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(RemoveCurveRedundantKeys, bool);
}

bool UInterchangeAnimSequenceFactoryNode::GetCustomDeleteExistingMorphTargetCurves(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(DeleteExistingMorphTargetCurves, bool);
}

bool UInterchangeAnimSequenceFactoryNode::SetCustomDeleteExistingMorphTargetCurves(const bool& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(DeleteExistingMorphTargetCurves, bool);
}

bool UInterchangeAnimSequenceFactoryNode::GetCustomSkeletonSoftObjectPath(FSoftObjectPath& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(SkeletonSoftObjectPath, FSoftObjectPath)
}

bool UInterchangeAnimSequenceFactoryNode::SetCustomSkeletonSoftObjectPath(const FSoftObjectPath& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(SkeletonSoftObjectPath, FSoftObjectPath)
}

int32 UInterchangeAnimSequenceFactoryNode::GetAnimatedMorphTargetDependeciesCount() const
{
	return AnimatedMorphTargetDependencies.GetCount();
}

void UInterchangeAnimSequenceFactoryNode::GetAnimatedMorphTargetDependencies(TArray<FString>& OutDependencies) const
{
	AnimatedMorphTargetDependencies.GetItems(OutDependencies);
}

void UInterchangeAnimSequenceFactoryNode::GetAnimatedMorphTargetDependency(const int32 Index, FString& OutDependency) const
{
	AnimatedMorphTargetDependencies.GetItem(Index, OutDependency);
}

bool UInterchangeAnimSequenceFactoryNode::SetAnimatedMorphTargetDependencyUid(const FString& DependencyUid)
{
	return AnimatedMorphTargetDependencies.AddItem(DependencyUid);
}

bool UInterchangeAnimSequenceFactoryNode::RemoveAnimatedMorphTargetDependencyUid(const FString& DependencyUid)
{
	return AnimatedMorphTargetDependencies.RemoveItem(DependencyUid);
}


int32 UInterchangeAnimSequenceFactoryNode::GetAnimatedAttributeCurveNamesCount() const
{
	return AnimatedAttributeCurveNames.GetCount();
}

void UInterchangeAnimSequenceFactoryNode::GetAnimatedAttributeCurveNames(TArray<FString>& OutAttributeCurveNames) const
{
	AnimatedAttributeCurveNames.GetItems(OutAttributeCurveNames);
}

void UInterchangeAnimSequenceFactoryNode::GetAnimatedAttributeCurveName(const int32 Index, FString& OutAttributeCurveName) const
{
	AnimatedAttributeCurveNames.GetItem(Index, OutAttributeCurveName);
}

bool UInterchangeAnimSequenceFactoryNode::SetAnimatedAttributeCurveName(const FString& AttributeCurveName)
{
	return AnimatedAttributeCurveNames.AddItem(AttributeCurveName);
}

bool UInterchangeAnimSequenceFactoryNode::RemoveAnimatedAttributeCurveName(const FString& AttributeCurveName)
{
	return AnimatedAttributeCurveNames.RemoveItem(AttributeCurveName);
}

bool UInterchangeAnimSequenceFactoryNode::GetCustomMaterialDriveParameterOnCustomAttribute(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(MaterialDriveParameterOnCustomAttribute, bool);
}

bool UInterchangeAnimSequenceFactoryNode::SetCustomMaterialDriveParameterOnCustomAttribute(const bool& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(MaterialDriveParameterOnCustomAttribute, bool);
}

int32 UInterchangeAnimSequenceFactoryNode::GetAnimatedMaterialCurveSuffixesCount() const
{
	return AnimatedMaterialCurveSuffixes.GetCount();
}

void UInterchangeAnimSequenceFactoryNode::GetAnimatedMaterialCurveSuffixes(TArray<FString>& OutMaterialCurveSuffixes) const
{
	AnimatedMaterialCurveSuffixes.GetItems(OutMaterialCurveSuffixes);
}

void UInterchangeAnimSequenceFactoryNode::GetAnimatedMaterialCurveSuffixe(const int32 Index, FString& OutMaterialCurveSuffixe) const
{
	AnimatedMaterialCurveSuffixes.GetItem(Index, OutMaterialCurveSuffixe);
}

bool UInterchangeAnimSequenceFactoryNode::SetAnimatedMaterialCurveSuffixe(const FString& MaterialCurveSuffixe)
{
	return AnimatedMaterialCurveSuffixes.AddItem(MaterialCurveSuffixe);
}

bool UInterchangeAnimSequenceFactoryNode::RemoveAnimatedMaterialCurveSuffixe(const FString& MaterialCurveSuffixe)
{
	return AnimatedMaterialCurveSuffixes.RemoveItem(MaterialCurveSuffixe);
}


int32 UInterchangeAnimSequenceFactoryNode::GetAnimatedAttributeStepCurveNamesCount() const
{
	return AnimatedAttributeStepCurveNames.GetCount();
}

void UInterchangeAnimSequenceFactoryNode::GetAnimatedAttributeStepCurveNames(TArray<FString>& OutAttributeStepCurveNames) const
{
	AnimatedAttributeStepCurveNames.GetItems(OutAttributeStepCurveNames);
}

void UInterchangeAnimSequenceFactoryNode::GetAnimatedAttributeStepCurveName(const int32 Index, FString& OutAttributeStepCurveName) const
{
	AnimatedAttributeStepCurveNames.GetItem(Index, OutAttributeStepCurveName);
}

bool UInterchangeAnimSequenceFactoryNode::SetAnimatedAttributeStepCurveName(const FString& AttributeStepCurveName)
{
	return AnimatedAttributeStepCurveNames.AddItem(AttributeStepCurveName);
}

bool UInterchangeAnimSequenceFactoryNode::RemoveAnimatedAttributeStepCurveName(const FString& AttributeStepCurveName)
{
	return AnimatedAttributeStepCurveNames.RemoveItem(AttributeStepCurveName);
}


bool UInterchangeAnimSequenceFactoryNode::GetCustomDeleteExistingCustomAttributeCurves(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(DeleteExistingCustomAttributeCurves, bool);
}

bool UInterchangeAnimSequenceFactoryNode::SetCustomDeleteExistingCustomAttributeCurves(const bool& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(DeleteExistingCustomAttributeCurves, bool);
}

bool UInterchangeAnimSequenceFactoryNode::GetCustomDeleteExistingNonCurveCustomAttributes(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(DeleteExistingNonCurveCustomAttributes, bool);
}

bool UInterchangeAnimSequenceFactoryNode::SetCustomDeleteExistingNonCurveCustomAttributes(const bool& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(DeleteExistingNonCurveCustomAttributes, bool);
}

void UInterchangeAnimSequenceFactoryNode::GetSceneNodeAnimationPayloadKeys(TMap<FString, FString>& OutSceneNodeAnimationPayloads) const
{
	OutSceneNodeAnimationPayloads = SceneNodeAnimationPayloadKeyMap.ToMap();
}

bool UInterchangeAnimSequenceFactoryNode::GetAnimationPayloadKeyFromSceneNodeUid(const FString& SceneNodeUid, FString& OutPayloadKey) const
{
	return SceneNodeAnimationPayloadKeyMap.GetValue(SceneNodeUid, OutPayloadKey);
}

bool UInterchangeAnimSequenceFactoryNode::SetAnimationPayloadKeyForSceneNodeUid(const FString& SceneNodeUid, const FString& PayloadKey)
{
	return SceneNodeAnimationPayloadKeyMap.SetKeyValue(SceneNodeUid, PayloadKey);
}

bool UInterchangeAnimSequenceFactoryNode::RemoveAnimationPayloadKeyForSceneNodeUid(const FString& SceneNodeUid)
{
	return SceneNodeAnimationPayloadKeyMap.RemoveKey(SceneNodeUid);
}

void UInterchangeAnimSequenceFactoryNode::GetMorphTargetNodeAnimationPayloadKeys(TMap<FString, FString>& OutMorphTargetAnimationPayloads) const
{
	OutMorphTargetAnimationPayloads = MorphTargetNodePayloadKeyMap.ToMap();
}

bool UInterchangeAnimSequenceFactoryNode::GetAnimationPayloadKeyFromMorphTargetNodeUid(const FString& MorphTargetNodeUid, FString& OutPayloadKey) const
{
	return MorphTargetNodePayloadKeyMap.GetValue(MorphTargetNodeUid, OutPayloadKey);
}

bool UInterchangeAnimSequenceFactoryNode::SetAnimationPayloadKeyForMorphTargetNodeUid(const FString& MorphTargetNodeUid, const FString& PayloadKey)
{
	return MorphTargetNodePayloadKeyMap.SetKeyValue(MorphTargetNodeUid, PayloadKey);
}

bool UInterchangeAnimSequenceFactoryNode::RemoveAnimationPayloadKeyForMorphTargetNodeUid(const FString& MorphTargetNodeUid)
{
	return MorphTargetNodePayloadKeyMap.RemoveKey(MorphTargetNodeUid);
}

/************************************************************************/
/* Automation tests                                                     */
/************************************************************************/
#if WITH_EDITOR
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInterchangeAnimSequenceTest, "System.Runtime.Interchange.ConvertSampleRatetoFrameRate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInterchangeAnimSequenceTest::RunTest(const FString& Parameters)
{
	FFrameRate FrameRate;
	FrameRate = UE::Interchange::Animation::ConvertSampleRatetoFrameRate(120.0);
	TestEqual(TEXT("`ConvertSampleRatetoFrameRate` Error converting 120.0 to FFrameRate"), FrameRate.Numerator, 120);
	TestEqual(TEXT("`ConvertSampleRatetoFrameRate` Error converting 120.0 to FFrameRate"), FrameRate.Denominator, 1);

	FrameRate = UE::Interchange::Animation::ConvertSampleRatetoFrameRate(100.0);
	TestEqual(TEXT("`ConvertSampleRatetoFrameRate` Error converting 100.0 to FFrameRate"), FrameRate.Numerator, 100);
	TestEqual(TEXT("`ConvertSampleRatetoFrameRate` Error converting 100.0 to FFrameRate"), FrameRate.Denominator, 1);

	FrameRate = UE::Interchange::Animation::ConvertSampleRatetoFrameRate(60.0);
	TestEqual(TEXT("`ConvertSampleRatetoFrameRate` Error converting 60.0 to FFrameRate"), FrameRate.Numerator, 60);
	TestEqual(TEXT("`ConvertSampleRatetoFrameRate` Error converting 60.0 to FFrameRate"), FrameRate.Denominator, 1);

	FrameRate = UE::Interchange::Animation::ConvertSampleRatetoFrameRate(50.0);
	TestEqual(TEXT("`ConvertSampleRatetoFrameRate` Error converting 50.0 to FFrameRate"), FrameRate.Numerator, 50);
	TestEqual(TEXT("`ConvertSampleRatetoFrameRate` Error converting 50.0 to FFrameRate"), FrameRate.Denominator, 1);

	FrameRate = UE::Interchange::Animation::ConvertSampleRatetoFrameRate(48.0);
	TestEqual(TEXT("`ConvertSampleRatetoFrameRate` Error converting 48.0 to FFrameRate"), FrameRate.Numerator, 48);
	TestEqual(TEXT("`ConvertSampleRatetoFrameRate` Error converting 48.0 to FFrameRate"), FrameRate.Denominator, 1);

	FrameRate = UE::Interchange::Animation::ConvertSampleRatetoFrameRate(30.0);
	TestEqual(TEXT("`ConvertSampleRatetoFrameRate` Error converting 30.0 to FFrameRate"), FrameRate.Numerator, 30);
	TestEqual(TEXT("`ConvertSampleRatetoFrameRate` Error converting 30.0 to FFrameRate"), FrameRate.Denominator, 1);

	FrameRate = UE::Interchange::Animation::ConvertSampleRatetoFrameRate(29.97);
	TestEqual(TEXT("`ConvertSampleRatetoFrameRate` Error converting 29.97 to FFrameRate"), FrameRate.Numerator, 2997);
	TestEqual(TEXT("`ConvertSampleRatetoFrameRate` Error converting 29.97 to FFrameRate"), FrameRate.Denominator, 100);

	FrameRate = UE::Interchange::Animation::ConvertSampleRatetoFrameRate(25.0);
	TestEqual(TEXT("`ConvertSampleRatetoFrameRate` Error converting 25.0 to FFrameRate"), FrameRate.Numerator, 25);
	TestEqual(TEXT("`ConvertSampleRatetoFrameRate` Error converting 25.0 to FFrameRate"), FrameRate.Denominator, 1);

	FrameRate = UE::Interchange::Animation::ConvertSampleRatetoFrameRate(24.0);
	TestEqual(TEXT("`ConvertSampleRatetoFrameRate` Error converting 24.0 to FFrameRate"), FrameRate.Numerator, 24);
	TestEqual(TEXT("`ConvertSampleRatetoFrameRate` Error converting 24.0 to FFrameRate"), FrameRate.Denominator, 1);

	FrameRate = UE::Interchange::Animation::ConvertSampleRatetoFrameRate(23.976);
	TestEqual(TEXT("`ConvertSampleRatetoFrameRate` Error converting 23.976 to FFrameRate"), FrameRate.Numerator, 2997);
	TestEqual(TEXT("`ConvertSampleRatetoFrameRate` Error converting 23.976 to FFrameRate"), FrameRate.Denominator, 125);

	FrameRate = UE::Interchange::Animation::ConvertSampleRatetoFrameRate(96.0);
	TestEqual(TEXT("`ConvertSampleRatetoFrameRate` Error converting 96.0 to FFrameRate"), FrameRate.Numerator, 96);
	TestEqual(TEXT("`ConvertSampleRatetoFrameRate` Error converting 96.0 to FFrameRate"), FrameRate.Denominator, 1);

	FrameRate = UE::Interchange::Animation::ConvertSampleRatetoFrameRate(72.0);
	TestEqual(TEXT("`ConvertSampleRatetoFrameRate` Error converting 72.0 to FFrameRate"), FrameRate.Numerator, 72);
	TestEqual(TEXT("`ConvertSampleRatetoFrameRate` Error converting 72.0 to FFrameRate"), FrameRate.Denominator, 1);

	FrameRate = UE::Interchange::Animation::ConvertSampleRatetoFrameRate(59.94);
	TestEqual(TEXT("`ConvertSampleRatetoFrameRate` Error converting 59.94 to FFrameRate"), FrameRate.Numerator, 2997);
	TestEqual(TEXT("`ConvertSampleRatetoFrameRate` Error converting 59.94 to FFrameRate"), FrameRate.Denominator, 50);

	FrameRate = UE::Interchange::Animation::ConvertSampleRatetoFrameRate(119.88);
	TestEqual(TEXT("`ConvertSampleRatetoFrameRate` Error converting 119.88 to FFrameRate"), FrameRate.Numerator, 2997);
	TestEqual(TEXT("`ConvertSampleRatetoFrameRate` Error converting 119.88 to FFrameRate"), FrameRate.Denominator, 25);

	return true;
}

#endif //WITH_DEV_AUTOMATION_TESTS
#endif //WITH_EDITOR

