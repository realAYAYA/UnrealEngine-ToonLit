// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimDataController.h"
#include "AnimDataControllerActions.h"
#include "MovieSceneTimeHelpers.h"

#include "Animation/AnimData/AnimDataModel.h"
#include "Animation/AnimData/CurveIdentifier.h"

#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimSequence.h"


#include "Algo/Transform.h"
#include "UObject/NameTypes.h"
#include "Animation/AnimCurveTypes.h"
#include "Math/UnrealMathUtility.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimDataController)

#define LOCTEXT_NAMESPACE "AnimDataController"

#if WITH_EDITOR

namespace UE {
namespace Anim {
	bool CanTransactChanges()
	{
		return GEngine && GEngine->CanTransact() && !GIsTransacting;
	}

	struct FScopedCompoundTransaction
	{
		FScopedCompoundTransaction(UE::FChangeTransactor& InTransactor, const FText& InDescription) : Transactor(InTransactor), bCreated(false)
		{
			if (CanTransactChanges() && !Transactor.IsTransactionPending())
			{
				Transactor.OpenTransaction(InDescription);
				bCreated = true;
			}
		}

		~FScopedCompoundTransaction()
		{
			if (bCreated)
			{
				Transactor.CloseTransaction();
			}
		}

		UE::FChangeTransactor& Transactor;
		bool bCreated;
	};
}}

#define CONDITIONAL_TRANSACTION(Text) \
	TUniquePtr<UE::Anim::FScopedCompoundTransaction> Transaction; \
	if (UE::Anim::CanTransactChanges() && bShouldTransact) \
	{ \
		Transaction = MakeUnique<UE::Anim::FScopedCompoundTransaction>(ChangeTransactor, Text); \
	}

#define CONDITIONAL_BRACKET(Text) IAnimationDataController::FScopedBracket Transaction(this, Text, UE::Anim::CanTransactChanges() && bShouldTransact);

#define CONDITIONAL_ACTION(ActionClass, ...) \
	if (UE::Anim::CanTransactChanges() && bShouldTransact) \
	{ \
		ChangeTransactor.AddTransactionChange<ActionClass>(__VA_ARGS__); \
	}

void UAnimDataController::SetModel(UAnimDataModel* InModel)
{	
	if (Model != nullptr)
	{
		Model->GetModifiedEvent().RemoveAll(this);
	}

	Model = InModel;
	
	ChangeTransactor.SetTransactionObject(InModel);
}

void UAnimDataController::OpenBracket(const FText& InTitle, bool bShouldTransact /*= true*/)
{
	ValidateModel();

	if (UE::Anim::CanTransactChanges() && !ChangeTransactor.IsTransactionPending())
	{
		ChangeTransactor.OpenTransaction(InTitle);

		CONDITIONAL_ACTION(UE::Anim::FCloseBracketAction, InTitle.ToString());
	}

	if (BracketDepth == 0)
	{
		FBracketPayload Payload;
		Payload.Description = InTitle.ToString();

		Model->Notify(EAnimDataModelNotifyType::BracketOpened, Payload);
	}

	++BracketDepth;
}

void UAnimDataController::CloseBracket(bool bShouldTransact /*= true*/)
{
	ValidateModel();

	if (BracketDepth == 0)
	{
		Report(ELogVerbosity::Error, LOCTEXT("NoExistingBracketError", "Attempt to close bracket that was not previously opened"));
		return;
	}

	--BracketDepth;

	if (BracketDepth == 0)
	{
		if (UE::Anim::CanTransactChanges())
		{
			ensure(ChangeTransactor.IsTransactionPending());

			CONDITIONAL_ACTION(UE::Anim::FOpenBracketAction, TEXT("Open Bracket"));

			ChangeTransactor.CloseTransaction();
		}
		
		Model->Notify(EAnimDataModelNotifyType::BracketClosed);
    }
}

void UAnimDataController::SetPlayLength(float Length, bool bShouldTransact /*= true*/)
{
	ValidateModel();

	// Calculate whether or new play length is shorter or longer than current, set-up T0; T1 accordingly
	// Assumption is made that time is always added or removed at/from the end
	// Added: T0 = current length, T1 = new length
	// Removed: T0 = current length - removed length, T1 = current length
	const float Delta = FMath::Abs(Length - Model->PlayLength);
	const float T0 = Length > Model->PlayLength ? Model->PlayLength : Model->PlayLength - Delta;
	const float T1 = Length > Model->PlayLength ? Length : Model->PlayLength;
	ResizePlayLength(Length, T0, T1, bShouldTransact);	
}

void UAnimDataController::Resize(float Length, float T0, float T1, bool bShouldTransact /*= true*/)
{
	ValidateModel();
	
	const TRange<float> PlayRange(TRange<float>::BoundsType::Inclusive(0.f), TRange<float>::BoundsType::Inclusive(Model->PlayLength));
	if (!FMath::IsNearlyZero(Length) && Length > 0.f)
	{
		if (Length != Model->PlayLength)
		{
			// Ensure that T0 is within the curent play range
			if (PlayRange.Contains(T0))
			{
				// Ensure that the start and end length of either removal or insertion are valid
				if (T0 < T1)
				{
					CONDITIONAL_BRACKET(LOCTEXT("ResizeModel", "Resizing Animation Data"));
					const bool bInserted = Length > Model->PlayLength;
					ResizePlayLength(Length, T0, T1, bShouldTransact);
					ResizeCurves(Length, bInserted, T0, T1, bShouldTransact);
					ResizeAttributes(Length, bInserted, T0, T1, bShouldTransact);
				}
				else
				{
					ReportErrorf(LOCTEXT("InvalidEndTimeError", "Invalid T1, smaller that T0 value: T0 {0}, T1 {1}"), FText::AsNumber(T0), FText::AsNumber(T1));
				}
			}
			else
			{
				ReportErrorf(LOCTEXT("InvalidStartTimeError", "Invalid T0, not within existing play range: T0 {0}, Play Length {1}"), FText::AsNumber(T0), FText::AsNumber(Model->PlayLength));
			}			
		}
		else
		{
			ReportWarningf(LOCTEXT("SamePlayLengthWarning", "New play length is same as existing one: {0} seconds"), FText::AsNumber(Length));
		}
	}
	else
	{
		ReportErrorf(LOCTEXT("InvalidPlayLengthError", "Invalid play length value provided: {0} seconds"), FText::AsNumber(Length));
	}
}

void UAnimDataController::SetFrameRate(FFrameRate FrameRate, bool bShouldTransact /*= true*/)
{
	ValidateModel();

	// Disallow invalid frame-rates, or 0.0 intervals
	const float FrameRateInterval = FrameRate.AsInterval();
	if ( FrameRate.IsValid() && !FMath::IsNearlyZero(FrameRateInterval) && FrameRateInterval > 0.f)
	{
		CONDITIONAL_TRANSACTION(LOCTEXT("SetFrameRate", "Setting Frame Rate"));

		CONDITIONAL_ACTION(UE::Anim::FSetFrameRateAction, Model.Get());

		FFrameRateChangedPayload Payload;
		Payload.PreviousFrameRate = Model->FrameRate;
			
			Model->FrameRate = FrameRate;
		Model->NumberOfFrames = Model->FrameRate.AsFrameTime(Model->PlayLength).RoundToFrame().Value;
			Model->NumberOfKeys = Model->NumberOfFrames + 1;
			
		Model->Notify(EAnimDataModelNotifyType::FrameRateChanged, Payload);
	}
	else
	{
		ReportErrorf(LOCTEXT("InvalidFrameRateError", "Invalid frame rate provided: {0}"), FrameRate.ToPrettyText());
	}
}


void UAnimDataController::UpdateCurveNamesFromSkeleton(const USkeleton* Skeleton, ERawCurveTrackTypes SupportedCurveType, bool bShouldTransact /*= true*/)
{
	ValidateModel();

	if (Skeleton)
	{
		if (IsSupportedCurveType(SupportedCurveType))
		{
			CONDITIONAL_BRACKET(LOCTEXT("ValidateRawCurves", "Validating Animation Curve Names"));
			switch (SupportedCurveType)
			{
			case ERawCurveTrackTypes::RCT_Float:
			{
				const FSmartNameMapping* NameMapping = Skeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName);
				for (FFloatCurve& FloatCurve : Model->CurveData.FloatCurves)
				{
					FSmartName NewSmartName = FloatCurve.Name;
					NameMapping->GetName(FloatCurve.Name.UID, NewSmartName.DisplayName);
					if (NewSmartName != FloatCurve.Name)
					{
						const FAnimationCurveIdentifier CurrentId(FloatCurve.Name, SupportedCurveType);
						const FAnimationCurveIdentifier NewId(NewSmartName, SupportedCurveType);
						RenameCurve(CurrentId, NewId, bShouldTransact);
					}
				}
				break;
			}
			case ERawCurveTrackTypes::RCT_Transform:
			{
				const FSmartNameMapping* NameMapping = Skeleton->GetSmartNameContainer(USkeleton::AnimTrackCurveMappingName);
				for (FTransformCurve& TransformCurve : Model->CurveData.TransformCurves)
				{
					FSmartName NewSmartName = TransformCurve.Name;
					NameMapping->GetName(TransformCurve.Name.UID, NewSmartName.DisplayName);
					if (NewSmartName != TransformCurve.Name)
					{
						const FAnimationCurveIdentifier CurrentId(TransformCurve.Name, SupportedCurveType);
						const FAnimationCurveIdentifier NewId(NewSmartName, SupportedCurveType);
						RenameCurve(CurrentId, NewId, bShouldTransact);
					}
				}
				break;
			}
			}
		}
		else
		{
			const FString CurveTypeAsString = GetCurveTypeValueName(SupportedCurveType);
			ReportWarningf(LOCTEXT("InvalidCurveTypeWarning", "Invalid curve type provided: {0} ({1})"), FText::FromString(CurveTypeAsString), FText::AsNumber((int32)SupportedCurveType));
		}
	}
	else
	{
		Report(ELogVerbosity::Error, LOCTEXT("UpdateCurveInvalidSkeletonError", "Invalid USkeleton supplied"));
	}
}

void UAnimDataController::FindOrAddCurveNamesOnSkeleton(USkeleton* Skeleton, ERawCurveTrackTypes SupportedCurveType, bool bShouldTransact)
{
	ValidateModel();
	
	if (Skeleton)
	{
		if (IsSupportedCurveType(SupportedCurveType))
		{
			CONDITIONAL_BRACKET(LOCTEXT("FindOrAddRawCurveNames", "Updating Skeleton with Animation Curve Names"));
			switch (SupportedCurveType)
			{
			case ERawCurveTrackTypes::RCT_Float:
			{
				for (FFloatCurve& FloatCurve : Model->CurveData.FloatCurves)
				{
					FSmartName NewSmartName = FloatCurve.Name;
					Skeleton->VerifySmartName(USkeleton::AnimCurveMappingName, NewSmartName);
					if (NewSmartName != FloatCurve.Name)
					{
						const FAnimationCurveIdentifier CurrentId(FloatCurve.Name, SupportedCurveType);
						const FAnimationCurveIdentifier NewId(NewSmartName, SupportedCurveType);
						RenameCurve(CurrentId, NewId, bShouldTransact);
					}
				}
				break;
			}
			case ERawCurveTrackTypes::RCT_Transform:
			{
				for (FTransformCurve& TransformCurve : Model->CurveData.TransformCurves)
				{
					FSmartName NewSmartName = TransformCurve.Name;
					Skeleton->VerifySmartName(USkeleton::AnimTrackCurveMappingName, NewSmartName);
					if (NewSmartName != TransformCurve.Name)
					{
						const FAnimationCurveIdentifier CurrentId(TransformCurve.Name, SupportedCurveType);
						const FAnimationCurveIdentifier NewId(NewSmartName, SupportedCurveType);
						RenameCurve(CurrentId, NewId, bShouldTransact);
					}
				}
				break;
			}
			}
		}
		else
		{
			const FString CurveTypeAsString = GetCurveTypeValueName(SupportedCurveType);
			ReportWarningf(LOCTEXT("InvalidCurveTypeWarning", "Invalid curve type provided: {0} ({1})"), FText::FromString(CurveTypeAsString), FText::AsNumber((int32)SupportedCurveType));
		}
	}
	else
	{
		Report(ELogVerbosity::Error, LOCTEXT("FindOrAddCurveInvalidSkeletonError", "Invalid USkeleton supplied "));
	}
}

bool UAnimDataController::RemoveBoneTracksMissingFromSkeleton(const USkeleton* Skeleton, bool bShouldTransact /*= true*/)
{
	if (!CheckOuterClass(UAnimSequence::StaticClass()))
	{
		return false;
	}

	if (Skeleton)
	{
		TArray<FName> TracksToBeRemoved;
		TArray<FName> TracksUpdated;
		const FReferenceSkeleton& ReferenceSkeleton = Skeleton->GetReferenceSkeleton();

		for (FBoneAnimationTrack& Track : Model->BoneAnimationTracks)
		{
			// Try find correct bone index
			const int32 BoneIndex = ReferenceSkeleton.FindBoneIndex(Track.Name);

			if (BoneIndex != INDEX_NONE && BoneIndex != Track.BoneTreeIndex)
			{
				// Update bone index
				Track.BoneTreeIndex = BoneIndex;
				TracksUpdated.Add(Track.Name);
			}
			else if (BoneIndex == INDEX_NONE)
			{				
				// Remove track
				TracksToBeRemoved.Add(Track.Name);
				Reportf(ELogVerbosity::Display, LOCTEXT("InvalidBoneIndexWarning", "Unable to find bone index, animation track will be removed: {0}"), FText::FromName(Track.Name));				
			}			
		}

		if (TracksToBeRemoved.Num() || TracksUpdated.Num())
		{
			CONDITIONAL_BRACKET(LOCTEXT("RemoveBoneTracksMissingFromSkeleton", "Validating Bone Animation Track Data against Skeleton"));
			for (const FName& TrackName : TracksToBeRemoved)
			{
				RemoveBoneTrack(TrackName);
			}

			for (const FName& TrackName : TracksUpdated)
			{
				FAnimationTrackChangedPayload Payload;
				Payload.Name = TrackName;
				Model->Notify(EAnimDataModelNotifyType::TrackChanged, Payload);
			}
		}

		return TracksToBeRemoved.Num() > 0 || TracksUpdated.Num() > 0;
	}
	else
	{
		Report(ELogVerbosity::Error, LOCTEXT("InvalidSkeletonError", "Invalid USkeleton supplied"));
	}

	return false;
}

void UAnimDataController::UpdateAttributesFromSkeleton(const USkeleton* Skeleton, bool bShouldTransact)
{
	ValidateModel();
	
	if (Skeleton)
	{
		// Verifying that bone (names) for attribute data exist on new skeleton
		TArray<FAnimationAttributeIdentifier> ToRemoveIdentifiers;
		TArray<TPair<FAnimationAttributeIdentifier, int32>> ToDuplicateIdentifiers;
			    
		for (const FAnimatedBoneAttribute& Attribute : Model->AnimatedBoneAttributes)
		{
			const int32 NewBoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(Attribute.Identifier.GetBoneName());

			if (NewBoneIndex == INDEX_NONE)
			{
				ToRemoveIdentifiers.Add(Attribute.Identifier);
			}
			else if(NewBoneIndex != Attribute.Identifier.GetBoneIndex())
			{						
				ToDuplicateIdentifiers.Add(TPair<FAnimationAttributeIdentifier, int32>(Attribute.Identifier, NewBoneIndex));
			}
		}

		if (ToRemoveIdentifiers.Num() || ToDuplicateIdentifiers.Num())
		{
			CONDITIONAL_BRACKET(LOCTEXT("VerifyAttributeBoneNames", "Remapping Animation Attribute Data"));
			for (const FAnimationAttributeIdentifier& Identifier : ToRemoveIdentifiers)
			{
				RemoveAttribute(Identifier);
			}
			
			for (const TPair<FAnimationAttributeIdentifier, int32>& Pair : ToDuplicateIdentifiers)
			{
				FAnimationAttributeIdentifier NewIdentifier = Pair.Key;
				NewIdentifier.BoneIndex = Pair.Value;

				DuplicateAttribute(Pair.Key, NewIdentifier);
				RemoveAttribute(Pair.Key);
			}	
		}
	}
	else
	{
		Report(ELogVerbosity::Error, LOCTEXT("InvalidSkeletonError", "Invalid USkeleton supplied"));
	}
}

void UAnimDataController::ResetModel(bool bShouldTransact /*= true*/)
{
	ValidateModel();

	CONDITIONAL_BRACKET(LOCTEXT("ResetModel", "Clearing Animation Data"));

	RemoveAllBoneTracks(bShouldTransact);

	RemoveAllCurvesOfType(ERawCurveTrackTypes::RCT_Float, bShouldTransact);
	RemoveAllCurvesOfType(ERawCurveTrackTypes::RCT_Transform, bShouldTransact);

	SetPlayLength(MINIMUM_ANIMATION_LENGTH, bShouldTransact);
	SetFrameRate(FFrameRate(30,1), bShouldTransact);

	Model->Notify(EAnimDataModelNotifyType::Reset);
}

bool UAnimDataController::AddCurve(const FAnimationCurveIdentifier& CurveId, int32 CurveFlags /*= EAnimAssetCurveFlags::AACF_Editable*/, bool bShouldTransact /*= true*/)
{
	ValidateModel();
	if (CurveId.InternalName.IsValid())
	{		
		if (IsSupportedCurveType(CurveId.CurveType))
		{
			if (!Model->FindCurve(CurveId))
			{
				CONDITIONAL_TRANSACTION(LOCTEXT("AddRawCurve", "Adding Animation Curve"));

				FCurveAddedPayload Payload;
				Payload.Identifier = CurveId;
				
				auto AddNewCurve = [CurveName = CurveId.InternalName, CurveFlags](auto& CurveTypeArray)
				{
					CurveTypeArray.Add({ CurveName, CurveFlags});
				};
				
				switch (CurveId.CurveType)
				{
				case ERawCurveTrackTypes::RCT_Transform:
					AddNewCurve(Model->CurveData.TransformCurves);
					break;
				case ERawCurveTrackTypes::RCT_Float:
					AddNewCurve(Model->CurveData.FloatCurves);
					break;
				}

				CONDITIONAL_ACTION(UE::Anim::FRemoveCurveAction, CurveId);
				Model->Notify(EAnimDataModelNotifyType::CurveAdded, Payload);

				return true;
			}
			else
			{
				const FString CurveTypeAsString = GetCurveTypeValueName(CurveId.CurveType);
				ReportWarningf(LOCTEXT("ExistingCurveNameWarning", "Curve with name {0} and type {1} ({2}) already exists"), FText::FromName(CurveId.InternalName.DisplayName), FText::FromString(CurveTypeAsString), FText::AsNumber((int32)CurveId.CurveType));
			}			
		}
		else 
		{
			const FString CurveTypeAsString = GetCurveTypeValueName(CurveId.CurveType);
			ReportWarningf(LOCTEXT("InvalidCurveTypeWarning", "Invalid curve type provided: {0} ({1})"), FText::FromString(CurveTypeAsString), FText::AsNumber((int32)CurveId.CurveType));
		}		
	}
	else
	{
		const FString CurveTypeAsString = GetCurveTypeValueName(CurveId.CurveType);
		ReportWarningf(LOCTEXT("InvalidCurveIdentifierWarning", "Invalid curve identifier provided: name: {0}, UID: {1} type: {2}"), FText::FromName(CurveId.InternalName.DisplayName), FText::AsNumber(CurveId.InternalName.UID), FText::FromString(CurveTypeAsString));
	}

	return false;
}

bool UAnimDataController::DuplicateCurve(const FAnimationCurveIdentifier& CopyCurveId, const FAnimationCurveIdentifier& NewCurveId, bool bShouldTransact /*= true*/)
{
	ValidateModel();

	ERawCurveTrackTypes SupportedCurveType = CopyCurveId.CurveType;

	if (CopyCurveId.InternalName.IsValid() && NewCurveId.InternalName.IsValid())
	{
		if (IsSupportedCurveType(SupportedCurveType))
		{
			if (CopyCurveId.CurveType == NewCurveId.CurveType)
			{
				if (Model->FindCurve(CopyCurveId))
				{
					if (!Model->FindCurve(NewCurveId))
					{
						CONDITIONAL_TRANSACTION(LOCTEXT("CopyRawCurve", "Duplicating Animation Curve"));

						auto DuplicateCurve = [NewCurveName = NewCurveId.InternalName](auto& CurveDataArray, const auto& SourceCurve)
						{
							auto& DuplicatedCurve = CurveDataArray.Add_GetRef( { NewCurveName, SourceCurve.GetCurveTypeFlags() });
							DuplicatedCurve.CopyCurve(SourceCurve);
						};
						
						switch (SupportedCurveType)
						{
						case ERawCurveTrackTypes::RCT_Transform:
							DuplicateCurve(Model->CurveData.TransformCurves, Model->GetTransformCurve(CopyCurveId));
							break;
						case ERawCurveTrackTypes::RCT_Float:
							DuplicateCurve(Model->CurveData.FloatCurves, Model->GetFloatCurve(CopyCurveId));
							break;
						}

						FCurveAddedPayload Payload;
						Payload.Identifier = NewCurveId;
						Model->Notify(EAnimDataModelNotifyType::CurveAdded, Payload);

						CONDITIONAL_ACTION(UE::Anim::FRemoveCurveAction, NewCurveId);

						return true;
					}
					else
					{
						const FString CurveTypeAsString = GetCurveTypeValueName(NewCurveId.CurveType);
						ReportWarningf(LOCTEXT("ExistingCurveNameWarning", "Curve with name {0} and type {1} ({2}) already exists"), FText::FromName(NewCurveId.InternalName.DisplayName), FText::FromString(CurveTypeAsString), FText::AsNumber((int32)NewCurveId.CurveType));
					}
				}
				else
				{
					const FString CurveTypeAsString = GetCurveTypeValueName(CopyCurveId.CurveType);
					ReportWarningf(LOCTEXT("CurveNameToDuplicateNotFoundWarning", "Could not find curve with name {0} and type {1} ({2}) for duplication"), FText::FromName(NewCurveId.InternalName.DisplayName), FText::FromString(CurveTypeAsString), FText::AsNumber((int32)NewCurveId.CurveType));
				}
			}
		}
		else
		{
			const FString CurveTypeAsString = GetCurveTypeValueName(SupportedCurveType);
			ReportWarningf(LOCTEXT("InvalidCurveTypeWarning", "Invalid curve type provided: {0} ({1})"), FText::FromString(CurveTypeAsString), FText::AsNumber((int32)SupportedCurveType));
		}
	}

	return false;
}


bool UAnimDataController::RemoveCurve(const FAnimationCurveIdentifier& CurveId, bool bShouldTransact /*= true*/)
{
	ValidateModel();
	ERawCurveTrackTypes SupportedCurveType = CurveId.CurveType;

	if (CurveId.InternalName.IsValid())
	{
		if (IsSupportedCurveType(CurveId.CurveType))
		{
			const FAnimCurveBase* Curve = Model->FindCurve(CurveId);
			if (Curve)
			{
				CONDITIONAL_TRANSACTION(LOCTEXT("RemoveCurve", "Removing Animation Curve"));

				switch (SupportedCurveType)
				{
					case ERawCurveTrackTypes::RCT_Transform:
					{
						const FTransformCurve& TransformCurve = Model->GetTransformCurve(CurveId);
						CONDITIONAL_ACTION(UE::Anim::FAddTransformCurveAction, CurveId, TransformCurve.GetCurveTypeFlags(), TransformCurve);
						Model->CurveData.TransformCurves.RemoveAll([Name = TransformCurve.Name](const FTransformCurve& ToRemoveCurve) { return ToRemoveCurve.Name == Name; });
						break;
					}
					case ERawCurveTrackTypes::RCT_Float:
					{
						const FFloatCurve& FloatCurve = Model->GetFloatCurve(CurveId);
						CONDITIONAL_ACTION(UE::Anim::FAddFloatCurveAction, CurveId, FloatCurve.GetCurveTypeFlags(), FloatCurve.FloatCurve.GetConstRefOfKeys(), FloatCurve.Color);
						Model->CurveData.FloatCurves.RemoveAll([Name = FloatCurve.Name](const FFloatCurve& ToRemoveCurve) { return ToRemoveCurve.Name == Name; });
						break;
					}
				}

				FCurveRemovedPayload Payload;
				Payload.Identifier = CurveId;
				Model->Notify(EAnimDataModelNotifyType::CurveRemoved, Payload);

				return true;
			}
			else
			{
				const FString CurveTypeAsString = GetCurveTypeValueName(SupportedCurveType);
				ReportWarningf(LOCTEXT("UnableToFindCurveForRemovalWarning", "Unable to find curve: {0} of type {1}"), FText::FromName(CurveId.InternalName.DisplayName), FText::FromString(CurveTypeAsString));
			}
		}
		else
		{
			const FString CurveTypeAsString = GetCurveTypeValueName(CurveId.CurveType);
			ReportWarningf(LOCTEXT("InvalidCurveTypeWarning", "Invalid curve type provided: {0} ({1})"), FText::FromString(CurveTypeAsString), FText::AsNumber((int32)CurveId.CurveType));
		}
	}

	return false;
}

void UAnimDataController::RemoveAllCurvesOfType(ERawCurveTrackTypes SupportedCurveType /*= ERawCurveTrackTypes::RCT_Float*/, bool bShouldTransact /*= true*/)
{
	ValidateModel();

	CONDITIONAL_BRACKET(LOCTEXT("DeleteAllRawCurve", "Deleting All Animation Curve"));
	switch (SupportedCurveType)
	{
	case ERawCurveTrackTypes::RCT_Transform:
	{
		TArray<FTransformCurve> TransformCurves = Model->CurveData.TransformCurves;
		for (const FTransformCurve& Curve : TransformCurves)
		{
			RemoveCurve(FAnimationCurveIdentifier(Curve.Name, ERawCurveTrackTypes::RCT_Transform), bShouldTransact);
		}
		break;
	}
	case ERawCurveTrackTypes::RCT_Float:
	{
		TArray<FFloatCurve> FloatCurves = Model->CurveData.FloatCurves;
		for (const FFloatCurve& Curve : FloatCurves)
		{
			RemoveCurve(FAnimationCurveIdentifier(Curve.Name, ERawCurveTrackTypes::RCT_Float), bShouldTransact);
		}
		break;
	}
	case ERawCurveTrackTypes::RCT_Vector:
	default:
		const FString CurveTypeAsString = GetCurveTypeValueName(SupportedCurveType);
		ReportWarningf(LOCTEXT("InvalidCurveTypeWarning", "Invalid curve type provided: {0} ({1})"), FText::FromString(CurveTypeAsString), FText::AsNumber((int32)SupportedCurveType));
	}

}

bool UAnimDataController::SetCurveFlag(const FAnimationCurveIdentifier& CurveId, EAnimAssetCurveFlags Flag, bool bState /*= true*/, bool bShouldTransact /*= true*/)
{
	ValidateModel();

	ERawCurveTrackTypes SupportedCurveType = CurveId.CurveType;

	FAnimCurveBase* Curve = nullptr;

	if (SupportedCurveType == ERawCurveTrackTypes::RCT_Float)
	{
		Curve = Model->FindMutableFloatCurveById(CurveId);
	}
	else if (SupportedCurveType == ERawCurveTrackTypes::RCT_Transform)
	{
		Curve = Model->FindMutableTransformCurveById(CurveId);
	}
	
	if (Curve)
	{
		CONDITIONAL_TRANSACTION(LOCTEXT("SetCurveFlag", "Setting Raw Curve Flag"));

		const int32 CurrentFlags = Curve->GetCurveTypeFlags();

		CONDITIONAL_ACTION(UE::Anim::FSetCurveFlagsAction, CurveId, CurrentFlags, SupportedCurveType);

		FCurveFlagsChangedPayload Payload;
		Payload.Identifier = CurveId;
		Payload.OldFlags = Curve->GetCurveTypeFlags();

		Curve->SetCurveTypeFlag(Flag, bState);

		Model->Notify(EAnimDataModelNotifyType::CurveFlagsChanged, Payload);

		return true;
	}
	else
	{
		const FString CurveTypeAsString = GetCurveTypeValueName(SupportedCurveType);
		ReportWarningf(LOCTEXT("UnableToFindCurveWarning", "Unable to find curve: {0} of type {1}"), FText::FromName(CurveId.InternalName.DisplayName), FText::FromString(CurveTypeAsString));
	}

	return false;
}

bool UAnimDataController::SetCurveFlags(const FAnimationCurveIdentifier& CurveId, int32 Flags, bool bShouldTransact /*= true*/)
{
	ValidateModel();

	FAnimCurveBase* Curve = nullptr;

	ERawCurveTrackTypes SupportedCurveType = CurveId.CurveType;

	if (SupportedCurveType == ERawCurveTrackTypes::RCT_Float)
	{
		Curve = Model->FindMutableFloatCurveById(CurveId);
	}
	else if (SupportedCurveType == ERawCurveTrackTypes::RCT_Transform)
	{
		Curve = Model->FindMutableTransformCurveById(CurveId);
	}

	if (Curve)
	{
		CONDITIONAL_TRANSACTION(LOCTEXT("SetRawCurveFlag", "Setting Raw Curve Flags"));

		const int32 CurrentFlags = Curve->GetCurveTypeFlags();

		CONDITIONAL_ACTION(UE::Anim::FSetCurveFlagsAction, CurveId, CurrentFlags, SupportedCurveType);

		FCurveFlagsChangedPayload Payload;
		Payload.Identifier = CurveId;
		Payload.OldFlags = Curve->GetCurveTypeFlags();

		Curve->SetCurveTypeFlags(Flags);

		Model->Notify(EAnimDataModelNotifyType::CurveFlagsChanged, Payload);

		return true;
	}
	else
	{
		const FString CurveTypeAsString = GetCurveTypeValueName(SupportedCurveType);
		ReportWarningf(LOCTEXT("UnableToFindCurveForRemovalWarning", "Unable to find curve: {0} of type {1}"), FText::FromName(CurveId.InternalName.DisplayName), FText::FromString(CurveTypeAsString));
	}

	return false;
}

bool UAnimDataController::SetTransformCurveKeys(const FAnimationCurveIdentifier& CurveId, const TArray<FTransform>& TransformValues, const TArray<float>& TimeKeys, bool bShouldTransact /*= true*/)
{
	ValidateModel();

	if (TransformValues.Num() == TimeKeys.Num())
	{
		FTransformCurve* Curve = Model->FindMutableTransformCurveById(CurveId);

		if (Curve)
		{
			CONDITIONAL_BRACKET(LOCTEXT("SetTransformCurveKeys_Bracket", "Setting Transform Curve Keys"));
			
			struct FKeys
			{
				FKeys(int32 NumKeys)
				{
					for (int32 ChannelIndex = 0; ChannelIndex < 3; ++ChannelIndex)
					{
						ChannelKeys[ChannelIndex].SetNum(NumKeys);
					}
				}

				TArray<FRichCurveKey> ChannelKeys[3];
			};

			FKeys TranslationKeys(TransformValues.Num());
			FKeys RotationKeys(TransformValues.Num());
			FKeys ScaleKeys(TransformValues.Num());

			FKeys* SubCurveKeys[3] = { &TranslationKeys, &RotationKeys, &ScaleKeys };

			// Generate the curve keys
			for (int32 KeyIndex = 0; KeyIndex < TransformValues.Num(); ++KeyIndex)
			{
				const FTransform& Value = TransformValues[KeyIndex];
				const float& Time = TimeKeys[KeyIndex];

				const FVector Translation = Value.GetLocation();
				const FVector Rotation = Value.GetRotation().Euler();
				const FVector Scale = Value.GetScale3D();

				auto SetKey = [Time, KeyIndex](FKeys& Key, const FVector& Vector)
				{
					for (int32 ChannelIndex = 0; ChannelIndex < 3; ++ChannelIndex)
					{
						Key.ChannelKeys[ChannelIndex][KeyIndex] = FRichCurveKey(Time, Vector[ChannelIndex]);
					}
				};

				SetKey(TranslationKeys, Translation);
				SetKey(RotationKeys, Rotation);
				SetKey(ScaleKeys, Scale);
			}
			
			for (int32 SubCurveIndex = 0; SubCurveIndex < 3; ++SubCurveIndex)
			{
				const ETransformCurveChannel Channel = (ETransformCurveChannel)SubCurveIndex;
				FKeys* CurveKeys = SubCurveKeys[SubCurveIndex];
				for (int32 ChannelIndex = 0; ChannelIndex < 3; ++ChannelIndex)
				{
					const EVectorCurveChannel Axis = (EVectorCurveChannel)ChannelIndex;
					FAnimationCurveIdentifier TargetCurveIdentifier = CurveId;
					UAnimationCurveIdentifierExtensions::GetTransformChildCurveIdentifier(TargetCurveIdentifier, Channel, Axis);
					SetCurveKeys(TargetCurveIdentifier, CurveKeys->ChannelKeys[ChannelIndex], bShouldTransact);
				}
			}

			return true;
		}
		else
		{
			ReportWarningf(LOCTEXT("UnableToFindTransformCurveWarning", "Unable to find transform curve: {0}"), FText::FromName(CurveId.InternalName.DisplayName));
		}
	}
	else
	{
		// time value mismatch
		ReportWarningf(LOCTEXT("InvalidNumberOfTimeAndKeyEntriesWarning", "Number of times and key entries do not match: number of time values {0}, number of key values {1}"), FText::AsNumber(TimeKeys.Num()), FText::AsNumber(TransformValues.Num()));
	}

	return false;	
}


bool UAnimDataController::SetTransformCurveKey(const FAnimationCurveIdentifier& CurveId, float Time, const FTransform& Value, bool bShouldTransact /*= true*/)
{
	ValidateModel();

	FTransformCurve* Curve = Model->FindMutableTransformCurveById(CurveId);

	if (Curve)
	{
		CONDITIONAL_BRACKET(LOCTEXT("AddTransformCurveKey_Bracket", "Setting Transform Curve Key"));
		struct FKeys
		{
			FRichCurveKey ChannelKeys[3];
		};

		FKeys VectorKeys[3];
		
		// Generate the rich curve keys		
		const FVector Translation = Value.GetLocation();
		const FVector Rotation = Value.GetRotation().Euler();
		const FVector Scale = Value.GetScale3D();

		auto SetKey = [Time](FKeys& Key, const FVector& Vector)
		{
			for (int32 ChannelIndex = 0; ChannelIndex < 3; ++ChannelIndex)
			{
				Key.ChannelKeys[ChannelIndex] = FRichCurveKey(Time, Vector[ChannelIndex]);
			}
		};

		SetKey(VectorKeys[0], Translation);
		SetKey(VectorKeys[1], Rotation);
		SetKey(VectorKeys[2], Scale);
		
		for (int32 SubCurveIndex = 0; SubCurveIndex < 3; ++SubCurveIndex)
		{
			const ETransformCurveChannel Channel = (ETransformCurveChannel)SubCurveIndex;
			const FKeys& VectorCurveKeys = VectorKeys[SubCurveIndex];
			for (int32 ChannelIndex = 0; ChannelIndex < 3; ++ChannelIndex)
			{
				const EVectorCurveChannel Axis = (EVectorCurveChannel)ChannelIndex;
				FAnimationCurveIdentifier TargetCurveIdentifier = CurveId;
				UAnimationCurveIdentifierExtensions::GetTransformChildCurveIdentifier(TargetCurveIdentifier, Channel, Axis);
				SetCurveKey(TargetCurveIdentifier, VectorCurveKeys.ChannelKeys[ChannelIndex], bShouldTransact);
			}
		}

		return true;
	}
	else
	{
		ReportWarningf(LOCTEXT("UnableToFindTransformCurveWarning", "Unable to find transform curve: {0}"), FText::FromName(CurveId.InternalName.DisplayName));
	}

	return false;
}


bool UAnimDataController::RemoveTransformCurveKey(const FAnimationCurveIdentifier& CurveId, float Time, bool bShouldTransact /*= true*/)
{
	ValidateModel();

	FTransformCurve* TransformCurve = Model->FindMutableTransformCurveById(CurveId);
	if (TransformCurve)
	{
		const FString BaseCurveName = CurveId.InternalName.DisplayName.ToString();
		const TArray<FString> SubCurveNames = { TEXT( "Translation"), TEXT( "Rotation"), TEXT( "Scale") };
		const TArray<FString> ChannelCurveNames = { TEXT("X"), TEXT("Y"), TEXT("Z") };

		CONDITIONAL_BRACKET(LOCTEXT("RemoveTransformCurveKey_Bracket", "Deleting Animation Transform Curve Key"));
		
		for (int32 SubCurveIndex = 0; SubCurveIndex < 3; ++SubCurveIndex)
		{
			const ETransformCurveChannel Channel = (ETransformCurveChannel)SubCurveIndex;
			for (int32 ChannelIndex = 0; ChannelIndex < 3; ++ChannelIndex)
			{
				const EVectorCurveChannel Axis = (EVectorCurveChannel)ChannelIndex;
				FAnimationCurveIdentifier TargetCurveIdentifier = CurveId;
				UAnimationCurveIdentifierExtensions::GetTransformChildCurveIdentifier(TargetCurveIdentifier, Channel, Axis);
				RemoveCurveKey(TargetCurveIdentifier, Time, bShouldTransact);
			}
		}


		return true;

	}
	else
	{
		ReportWarningf(LOCTEXT("UnableToFindTransformCurveWarning", "Unable to find transform curve: {0}"), FText::FromName(CurveId.InternalName.DisplayName));
	}

	return false;
}

bool UAnimDataController::RenameCurve(const FAnimationCurveIdentifier& CurveToRenameId, const FAnimationCurveIdentifier& NewCurveId, bool bShouldTransact /*= true*/)
{
	ValidateModel();

	if (NewCurveId.IsValid())
	{
		if (CurveToRenameId != NewCurveId)
		{
			if (CurveToRenameId.CurveType == NewCurveId.CurveType)
			{
				FAnimCurveBase* Curve = Model->FindMutableCurveById(CurveToRenameId);
				if (Curve)
				{
					CONDITIONAL_TRANSACTION(LOCTEXT("RenameCurve", "Renaming Curve"));

					FCurveRenamedPayload Payload;
					Payload.Identifier = FAnimationCurveIdentifier(Curve->Name, CurveToRenameId.CurveType);

					Curve->Name = NewCurveId.InternalName;
					Payload.NewIdentifier = NewCurveId;

					CONDITIONAL_ACTION(UE::Anim::FRenameCurveAction, NewCurveId, CurveToRenameId);

					Model->Notify(EAnimDataModelNotifyType::CurveRenamed, Payload);

					return true;
				}
				else
				{
					const FString CurveTypeAsString = GetCurveTypeValueName(CurveToRenameId.CurveType);
					ReportWarningf(LOCTEXT("UnableToFindCurveWarning", "Unable to find curve: {0} of type {1}"), FText::FromName(CurveToRenameId.InternalName.DisplayName), FText::FromString(CurveTypeAsString));
				}
			}
			else
			{
				const FString CurrentCurveTypeAsString = GetCurveTypeValueName(CurveToRenameId.CurveType);
				const FString NewCurveTypeAsString = GetCurveTypeValueName(NewCurveId.CurveType);
				ReportWarningf(LOCTEXT("MismatchOfCurveTypesWarning", "Different curve types provided between current and new curve names: {0} ({1}) and {2} ({3})"), FText::FromName(CurveToRenameId.InternalName.DisplayName), FText::FromString(CurrentCurveTypeAsString),
					FText::FromName(NewCurveId.InternalName.DisplayName), FText::FromString(NewCurveTypeAsString));
			}
		}
		else
		{
			ReportWarningf(LOCTEXT("MatchingCurveNamesWarning", "Provided curve names are the same: {0}"), FText::FromName(CurveToRenameId.InternalName.DisplayName));
		}
		
	}
	else
	{
		ReportWarningf(LOCTEXT("InvalidCurveIdentiferProvidedWarning", "Invalid new curve identifier provided: {2} ({3})"), FText::FromName(NewCurveId.InternalName.DisplayName), FText::AsNumber(NewCurveId.InternalName.UID));
	}

	return false;
}

bool UAnimDataController::SetCurveColor(const FAnimationCurveIdentifier& CurveId, FLinearColor Color, bool bShouldTransact)
{
	ValidateModel();

	if (CurveId.IsValid())
	{
		if (CurveId.CurveType == ERawCurveTrackTypes::RCT_Float)
		{
			FFloatCurve* Curve = Model->FindMutableFloatCurveById(CurveId);
			if (Curve)
			{
				CONDITIONAL_TRANSACTION(LOCTEXT("ChangingCurveColor", "Changing Curve Color"));

				CONDITIONAL_ACTION(UE::Anim::FSetCurveColorAction, CurveId, Curve->Color);

				Curve->Color = Color;

				FCurveChangedPayload Payload;
				Payload.Identifier = CurveId;
				Model->Notify(EAnimDataModelNotifyType::CurveColorChanged, Payload);

				return true;				
			}
			else
			{
				const FString CurveTypeAsString = GetCurveTypeValueName(CurveId.CurveType);
				ReportWarningf(LOCTEXT("UnableToFindCurveWarning", "Unable to find curve: {0} of type {1}"), FText::FromName(CurveId.InternalName.DisplayName), FText::FromString(CurveTypeAsString));
			}
		}
		else
		{
			Report(ELogVerbosity::Warning, LOCTEXT("NonSupportedCurveColorSetWarning", "Changing curve color is currently only supported for float curves"));
		}
	}
	else
	{
		ReportWarningf(LOCTEXT("InvalidCurveIdentifier", "Invalid Curve Identifier : {0} ({1})"), FText::FromName(CurveId.InternalName.DisplayName), FText::AsNumber(CurveId.InternalName.UID));
	}	

	return false;
}

bool UAnimDataController::ScaleCurve(const FAnimationCurveIdentifier& CurveId, float Origin, float Factor, bool bShouldTransact /*= true*/)
{
	ValidateModel();

	ERawCurveTrackTypes SupportedCurveType = CurveId.CurveType;
	if (SupportedCurveType == ERawCurveTrackTypes::RCT_Float)
	{
		FFloatCurve* Curve = Model->FindMutableFloatCurveById(CurveId);
		if (Curve)
		{
			CONDITIONAL_TRANSACTION(LOCTEXT("ScalingCurve", "Scaling Curve"));

			Curve->FloatCurve.ScaleCurve(Origin, Factor);

			FCurveScaledPayload Payload;
			Payload.Identifier = CurveId;
			Payload.Factor = Factor;
			Payload.Origin = Origin;
			
			CONDITIONAL_ACTION(UE::Anim::FScaleCurveAction, CurveId, Origin, 1.0f / Factor, SupportedCurveType);

			Model->Notify(EAnimDataModelNotifyType::CurveScaled, Payload);

			return true;
		}
		else
		{
			ReportWarningf(LOCTEXT("UnableToFindFloatCurveWarning", "Unable to find float curve: {0}"), FText::FromName(CurveId.InternalName.DisplayName));
		}
	}
	else
	{
		Report(ELogVerbosity::Warning, LOCTEXT("NonSupportedCurveScalingWarning", "Scaling curves is currently only supported for float curves"));
	}

	return false;
}

bool UAnimDataController::SetCurveKey(const FAnimationCurveIdentifier& CurveId, const FRichCurveKey& Key, bool bShouldTransact)
{
	ValidateModel();

	FRichCurve* RichCurve = Model->GetMutableRichCurve(CurveId);
	ERawCurveTrackTypes SupportedCurveType = CurveId.CurveType;
	if (RichCurve)
	{
		FCurveChangedPayload Payload;
		Payload.Identifier = CurveId;

		// Set or add rich curve value
		const FKeyHandle Handle = RichCurve->FindKey(Key.Time, 0.f);
		if (Handle != FKeyHandle::Invalid())
		{
			CONDITIONAL_TRANSACTION(LOCTEXT("SetNamedCurveKey", "Setting Curve Key"));
			// Cache old value for action
			const FRichCurveKey CurrentKey = RichCurve->GetKey(Handle);
			CONDITIONAL_ACTION(UE::Anim::FSetRichCurveKeyAction, CurveId, CurrentKey);

			// Set the new value
			RichCurve->SetKeyValue(Handle, Key.Value);

			Model->Notify(EAnimDataModelNotifyType::CurveChanged, Payload);
		}
		else
		{
			CONDITIONAL_TRANSACTION(LOCTEXT("AddNamedCurveKey", "Adding Curve Key"));
			CONDITIONAL_ACTION(UE::Anim::FRemoveRichCurveKeyAction, CurveId, Key.Time);

			// Add the new key
			RichCurve->AddKey(Key.Time, Key.Value);

			Model->Notify(EAnimDataModelNotifyType::CurveChanged, Payload);
		}

		return true;
	}

	return false;
}

bool UAnimDataController::RemoveCurveKey(const FAnimationCurveIdentifier& CurveId, float Time, bool bShouldTransact)
{
	ValidateModel();

	FRichCurve* RichCurve = Model->GetMutableRichCurve(CurveId);
	ERawCurveTrackTypes SupportedCurveType = CurveId.CurveType;
	if (RichCurve)
	{
		FCurveChangedPayload Payload;
		Payload.Identifier = CurveId;

		// Remove key at time value		
		const FKeyHandle Handle = RichCurve->FindKey(Time, 0.f);
		if (Handle != FKeyHandle::Invalid())
		{
			CONDITIONAL_TRANSACTION(LOCTEXT("RemoveNamedCurveKey", "Removing Curve Key"));

			// Cached current value for action
			const FRichCurveKey CurrentKey = RichCurve->GetKey(Handle);
			CONDITIONAL_ACTION(UE::Anim::FAddRichCurveKeyAction, CurveId, CurrentKey);

			RichCurve->DeleteKey(Handle);

			Model->Notify(EAnimDataModelNotifyType::CurveChanged, Payload);

			return true;
		}
		else
		{
			ReportErrorf(LOCTEXT("RichCurveKeyNotFoundError", "Unable to find rich curve key: curve name {0}, time {1}"), FText::FromName(CurveId.InternalName.DisplayName), FText::AsNumber(Time));
		}
	}

	return false;
}


bool UAnimDataController::SetCurveKeys(const FAnimationCurveIdentifier& CurveId, const TArray<FRichCurveKey>& CurveKeys, bool bShouldTransact)
{
	ValidateModel();

	FRichCurve* RichCurve = Model->GetMutableRichCurve(CurveId);
	ERawCurveTrackTypes SupportedCurveType = CurveId.CurveType;
	if (RichCurve)
	{
		CONDITIONAL_TRANSACTION(LOCTEXT("SettingNamedCurveKeys", "Setting Curve Keys"));
		CONDITIONAL_ACTION(UE::Anim::FSetRichCurveKeysAction, CurveId, RichCurve->GetConstRefOfKeys());

		// Set rich curve values
		RichCurve->SetKeys(CurveKeys);

		FCurveChangedPayload Payload;
		Payload.Identifier = CurveId;
		Model->Notify(EAnimDataModelNotifyType::CurveChanged, Payload);

		return true;
	}

	return false;
}


bool UAnimDataController::SetCurveAttributes(const FAnimationCurveIdentifier& CurveId, const FCurveAttributes& Attributes, bool bShouldTransact)
{
	ValidateModel();

	FRichCurve* RichCurve = Model->GetMutableRichCurve(CurveId);
	ERawCurveTrackTypes SupportedCurveType = CurveId.CurveType;
	if (RichCurve)
	{
		CONDITIONAL_TRANSACTION(LOCTEXT("SettingNamedCurveAttributes", "Setting Curve Attributes"));

		FCurveAttributes CurrentAttributes;
		CurrentAttributes.SetPreExtrapolation(RichCurve->PreInfinityExtrap);
		CurrentAttributes.SetPostExtrapolation(RichCurve->PostInfinityExtrap);		
		CONDITIONAL_ACTION(UE::Anim::FSetRichCurveAttributesAction, CurveId, CurrentAttributes);

		if(Attributes.HasPreExtrapolation())
		{
			RichCurve->PreInfinityExtrap = Attributes.GetPreExtrapolation();
		}

		if(Attributes.HasPostExtrapolation())
		{
			RichCurve->PostInfinityExtrap = Attributes.GetPostExtrapolation();
		}		

		FCurveChangedPayload Payload;
		Payload.Identifier = CurveId;
		Model->Notify(EAnimDataModelNotifyType::CurveChanged, Payload);

		return true;
	}

	return false;
}

void UAnimDataController::NotifyPopulated()
{
	ValidateModel();
	Model->Notify(EAnimDataModelNotifyType::Populated);
}

void UAnimDataController::NotifyBracketOpen()
{
	ValidateModel();
	Model->Notify(EAnimDataModelNotifyType::BracketOpened);
}

void UAnimDataController::NotifyBracketClosed()
{
	ValidateModel();
	Model->Notify(EAnimDataModelNotifyType::BracketClosed);
}

const bool UAnimDataController::IsSupportedCurveType(ERawCurveTrackTypes CurveType) const
{
	const TArray<ERawCurveTrackTypes> SupportedTypes = { ERawCurveTrackTypes::RCT_Float, ERawCurveTrackTypes::RCT_Transform };
	return SupportedTypes.Contains(CurveType);
}

void UAnimDataController::ValidateModel() const
{
	checkf(Model != nullptr, TEXT("Invalid Model"));
}

void UAnimDataController::ResizePlayLength(float Length, float T0, float T1, bool bShouldTransact)
{
	const TRange<float> PlayRange(TRange<float>::BoundsType::Inclusive(0.f), TRange<float>::BoundsType::Inclusive(Model->PlayLength));
	if (!FMath::IsNearlyZero(Length) && Length > 0.f)
	{
		if (Length != Model->PlayLength)
		{
			// Ensure that T0 is within the curent play range
			if (PlayRange.Contains(T0))
			{
				// Ensure that the start and end length of either removal or insertion are valid
				if (T0 < T1)
				{
					CONDITIONAL_TRANSACTION(LOCTEXT("ResizePlayLength", "Resizing Play Length"));

					FSequenceLengthChangedPayload Payload;
					Payload.T0 = T0;
					Payload.T1 = T1;
					Payload.PreviousLength = Model->PlayLength;

					CONDITIONAL_ACTION(UE::Anim::FResizePlayLengthAction, Model.Get(), T0, T1);

					Model->PlayLength = Length;

					Model->NumberOfFrames = Model->FrameRate.AsFrameTime(Model->PlayLength).RoundToFrame().Value;
					Model->NumberOfKeys = Model->NumberOfFrames + 1;
	
					Model->Notify<FSequenceLengthChangedPayload>(EAnimDataModelNotifyType::SequenceLengthChanged, Payload);
				}
				else
				{
					ReportErrorf(LOCTEXT("InvalidEndTimeError", "Invalid T1, smaller that T0 value: T0 {0}, T1 {1}"), FText::AsNumber(T0), FText::AsNumber(T1));
				}
			}
			else
			{
				ReportErrorf(LOCTEXT("InvalidStartTimeError", "Invalid T0, not within existing play range: T0 {0}, Play Length {1}"), FText::AsNumber(T0), FText::AsNumber(Model->PlayLength));
			}
		}
		else
		{
			ReportWarningf(LOCTEXT("SamePlayLengthWarning", "New play length is same as existing one: {0} seconds"), FText::AsNumber(Length));
		}
	}
	else
	{
		ReportErrorf(LOCTEXT("InvalidPlayLengthError", "Invalid play length value provided: {0} seconds"), FText::AsNumber(Length));
	}
}


void UAnimDataController::Report(ELogVerbosity::Type InVerbosity, const FText& InMessage) const
{
	FString Message = InMessage.ToString();
	if (Model != nullptr)
	{
		if (UPackage* Package = Cast<UPackage>(Model->GetOutermost()))
		{
			Message = FString::Printf(TEXT("%s : %s"), *Package->GetPathName(), *Message);
		}
	}

	FScriptExceptionHandler::Get().HandleException(InVerbosity, *Message, *FString());
}

FString UAnimDataController::GetCurveTypeValueName(ERawCurveTrackTypes InType) const
{
	FString ValueString;

	const UEnum* Enum = FindObject<UEnum>(nullptr, TEXT("/Script/Engine.ERawCurveTrackTypes"));
	if (Enum)
	{
		ValueString = Enum->GetNameStringByValue((int64)InType);
	}

	return ValueString;
}

bool UAnimDataController::CheckOuterClass(UClass* InClass) const
{
	ValidateModel();
	
	const UObject* ModelOuter = Model->GetOuter();
	if (ModelOuter)
	{
		const UClass* OuterClass = ModelOuter->GetClass();
		if (OuterClass)
		{
			if (OuterClass == InClass || OuterClass->IsChildOf(InClass))
			{
				return true;
			}
			else
			{
				ReportErrorf(LOCTEXT("NoValidOuterClassError", "Incorrect outer object class found for Animation Data Model {0}, expected {1} actual {2}"), FText::FromString(Model->GetName()), FText::FromString(InClass->GetName()), FText::FromString(OuterClass->GetName()));
			}
		}
	}
	else
	{
		ReportErrorf(LOCTEXT("NoValidOuterObjectFoundError", "No valid outer object found for Animation Data Model {0}"), FText::FromString(Model->GetName()));
	}

	return false;
}

int32 UAnimDataController::AddBoneTrack(FName BoneName, bool bShouldTransact /*= true*/)
{
	if (!CheckOuterClass(UAnimSequence::StaticClass()))
	{
		return INDEX_NONE;
	}

	CONDITIONAL_TRANSACTION(LOCTEXT("AddBoneTrack", "Adding Animation Data Track"));
	return InsertBoneTrack(BoneName, INDEX_NONE, bShouldTransact);
}

int32 UAnimDataController::InsertBoneTrack(FName BoneName, int32 DesiredIndex, bool bShouldTransact /*= true*/)
{
	if (!CheckOuterClass(UAnimSequence::StaticClass()))
	{
		return INDEX_NONE;
	}
	
	const int32 TrackIndex = Model->GetBoneTrackIndexByName(BoneName);

	if (TrackIndex == INDEX_NONE)
	{
		if (Model->GetNumBoneTracks() >= MAX_ANIMATION_TRACKS)
		{
			ReportWarningf(LOCTEXT("MaxNumberOfTracksReachedWarning", "Cannot add track with name {0}. An animation sequence cannot contain more than 65535 tracks"), FText::FromName(BoneName));
		}
		else
		{
			CONDITIONAL_TRANSACTION(LOCTEXT("InsertBoneTrack", "Inserting Animation Data Track"));

			// Determine correct index to do insertion at
			const int32 InsertIndex = Model->BoneAnimationTracks.IsValidIndex(DesiredIndex) ? DesiredIndex : Model->BoneAnimationTracks.Num();

			FBoneAnimationTrack& NewTrack = Model->BoneAnimationTracks.InsertDefaulted_GetRef(InsertIndex);
			NewTrack.Name = BoneName;

			if (const UAnimSequence* AnimationSequence = Model->GetAnimationSequence())
			{
				if (const USkeleton* Skeleton = AnimationSequence->GetSkeleton())
				{
					const int32 BoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(BoneName);

					if (BoneIndex == INDEX_NONE)
					{
						ReportWarningf(LOCTEXT("UnableToFindBoneIndexWarning", "Unable to retrieve bone index for track: {0}"), FText::FromName(BoneName));
					}

					NewTrack.BoneTreeIndex = BoneIndex;
				}
				else
				{
					Report(ELogVerbosity::Error, LOCTEXT("UnableToGetOuterSkeletonError", "Unable to retrieve Skeleton for outer Animation Sequence"));
				}
			}
			else
			{
				Report(ELogVerbosity::Error, LOCTEXT("UnableToGetOuterAnimSequenceError", "Unable to retrieve outer Animation Sequence"));
			}

				FAnimationTrackAddedPayload Payload;
				Payload.Name = BoneName;
				Payload.TrackIndex = InsertIndex;

			Model->Notify<FAnimationTrackAddedPayload>(EAnimDataModelNotifyType::TrackAdded, Payload);
			CONDITIONAL_ACTION(UE::Anim::FRemoveTrackAction, NewTrack, InsertIndex);

				return InsertIndex;
			}
		}
	else
	{
		ReportWarningf(LOCTEXT("TrackNameAlreadyExistsWarning", "Track with name {0} already exists"), FText::FromName(BoneName));
	}
	
	return TrackIndex;
}

bool UAnimDataController::RemoveBoneTrack(FName BoneName, bool bShouldTransact /*= true*/)
{
	if (!CheckOuterClass(UAnimSequence::StaticClass()))
	{
		return false;
	}

	const FBoneAnimationTrack* ExistingTrackPtr = Model->FindBoneTrackByName(BoneName);

	if (ExistingTrackPtr != nullptr)
	{
		CONDITIONAL_TRANSACTION(LOCTEXT("RemoveBoneTrack", "Removing Animation Data Track"));
		const int32 TrackIndex = Model->BoneAnimationTracks.IndexOfByPredicate([ExistingTrackPtr](const FBoneAnimationTrack& Track)
		{
			return Track.Name == ExistingTrackPtr->Name;
		});

		ensure(TrackIndex != INDEX_NONE);

		CONDITIONAL_ACTION(UE::Anim::FAddTrackAction, *ExistingTrackPtr, TrackIndex);
		Model->BoneAnimationTracks.RemoveAt(TrackIndex);

		FAnimationTrackRemovedPayload Payload;
		Payload.Name = BoneName;

		Model->Notify(EAnimDataModelNotifyType::TrackRemoved, Payload);

		return true;
	}
	else
	{
		ReportWarningf(LOCTEXT("UnableToFindTrackWarning", "Could not find track with name {0}"), FText::FromName(BoneName));
	}

	return false;
}

void UAnimDataController::RemoveAllBoneTracks(bool bShouldTransact /*= true*/)
{
	if (!CheckOuterClass(UAnimSequence::StaticClass()))
	{
		return;
	}
	
	TArray<FName> TrackNames;
	Model->GetBoneTrackNames(TrackNames);

	if (TrackNames.Num())
	{
		CONDITIONAL_BRACKET(LOCTEXT("RemoveAllBoneTracks", "Removing all Animation Data Tracks"));
		for (const FName& TrackName : TrackNames)
		{
			RemoveBoneTrack(TrackName, bShouldTransact);
		}
	}	
}

bool UAnimDataController::SetBoneTrackKeys(FName BoneName, const TArray<FVector>& PositionalKeys, const TArray<FQuat>& RotationalKeys, const TArray<FVector>& ScalingKeys, bool bShouldTransact /*= true*/)
{
	if (!CheckOuterClass(UAnimSequence::StaticClass()))
	{
		return false;
	}

	CONDITIONAL_TRANSACTION(LOCTEXT("SetTrackKeysTransaction", "Setting Animation Data Track keys"));

	// Validate key format
	const int32 MaxNumKeys = FMath::Max(FMath::Max(PositionalKeys.Num(), RotationalKeys.Num()), ScalingKeys.Num());

	if (MaxNumKeys > 0)
	{
		const bool bValidPosKeys = PositionalKeys.Num() == MaxNumKeys;
		const bool bValidRotKeys = RotationalKeys.Num() == MaxNumKeys;
		const bool bValidScaleKeys = ScalingKeys.Num() == MaxNumKeys;

		if (bValidPosKeys && bValidRotKeys && bValidScaleKeys)
		{
			if (FBoneAnimationTrack* TrackPtr = Model->FindMutableBoneTrackByName(BoneName))
			{
				CONDITIONAL_ACTION(UE::Anim::FSetTrackKeysAction, *TrackPtr);


				TrackPtr->InternalTrackData.PosKeys.SetNum(MaxNumKeys);
				TrackPtr->InternalTrackData.ScaleKeys.SetNum(MaxNumKeys);
				TrackPtr->InternalTrackData.RotKeys.SetNum(MaxNumKeys);
				for(int KeyIndex = 0; KeyIndex<MaxNumKeys; KeyIndex++)
				{
					TrackPtr->InternalTrackData.PosKeys[KeyIndex] = FVector3f(PositionalKeys[KeyIndex]);
					TrackPtr->InternalTrackData.ScaleKeys[KeyIndex] = FVector3f(ScalingKeys[KeyIndex]);
					TrackPtr->InternalTrackData.RotKeys[KeyIndex] = FQuat4f(RotationalKeys[KeyIndex]);
				}

				FAnimationTrackChangedPayload Payload;
				Payload.Name = BoneName;

				Model->Notify(EAnimDataModelNotifyType::TrackChanged, Payload);

				return true;
			}
			else
			{
				ReportWarningf(LOCTEXT("InvalidTrackNameWarning", "Track with name {0} does not exist"), FText::FromName(BoneName));
			}
		}
		else
		{
			ReportErrorf(LOCTEXT("InvalidTrackKeyDataError", "Invalid track key data, expected uniform data: number of positional keys {0}, number of rotational keys {1}, number of scaling keys {2}"), FText::AsNumber(PositionalKeys.Num()), FText::AsNumber(RotationalKeys.Num()), FText::AsNumber(ScalingKeys.Num()));
		}
	}
	else
	{
		ReportErrorf(LOCTEXT("MissingTrackKeyDataError", "Missing track key data, expected uniform data: number of positional keys {0}, number of rotational keys {1}, number of scaling keys {2}"), FText::AsNumber(PositionalKeys.Num()), FText::AsNumber(RotationalKeys.Num()), FText::AsNumber(ScalingKeys.Num()));
	}

	return false;
}

bool UAnimDataController::SetBoneTrackKeys(FName BoneName, const TArray<FVector3f>& PositionalKeys, const TArray<FQuat4f>& RotationalKeys, const TArray<FVector3f>& ScalingKeys, bool bShouldTransact /*= true*/)
{
	if (!CheckOuterClass(UAnimSequence::StaticClass()))
	{
		return false;
	}

	CONDITIONAL_TRANSACTION(LOCTEXT("SetTrackKeysTransaction", "Setting Animation Data Track keys"));

	// Validate key format
	const int32 MaxNumKeys = FMath::Max(FMath::Max(PositionalKeys.Num(), RotationalKeys.Num()), ScalingKeys.Num());

	if (MaxNumKeys > 0)
	{
		const bool bValidPosKeys = PositionalKeys.Num() == MaxNumKeys;
		const bool bValidRotKeys = RotationalKeys.Num() == MaxNumKeys;
		const bool bValidScaleKeys = ScalingKeys.Num() == MaxNumKeys;

		if (bValidPosKeys && bValidRotKeys && bValidScaleKeys)
		{
			if (FBoneAnimationTrack* TrackPtr = Model->FindMutableBoneTrackByName(BoneName))
			{
				CONDITIONAL_ACTION(UE::Anim::FSetTrackKeysAction, *TrackPtr);

				TrackPtr->InternalTrackData.PosKeys = PositionalKeys;
				TrackPtr->InternalTrackData.RotKeys = RotationalKeys;
				TrackPtr->InternalTrackData.ScaleKeys = ScalingKeys;

				FAnimationTrackChangedPayload Payload;
				Payload.Name = BoneName;

				Model->Notify(EAnimDataModelNotifyType::TrackChanged, Payload);

				return true;
			}
			else
			{
				ReportWarningf(LOCTEXT("InvalidTrackNameWarning", "Track with name {0} does not exist"), FText::FromName(BoneName));
			}
		}
		else
		{
			ReportErrorf(LOCTEXT("InvalidTrackKeyDataError", "Invalid track key data, expected uniform data: number of positional keys {0}, number of rotational keys {1}, number of scaling keys {2}"), FText::AsNumber(PositionalKeys.Num()), FText::AsNumber(RotationalKeys.Num()), FText::AsNumber(ScalingKeys.Num()));
		}
	}
	else
	{
		ReportErrorf(LOCTEXT("MissingTrackKeyDataError", "Missing track key data, expected uniform data: number of positional keys {0}, number of rotational keys {1}, number of scaling keys {2}"), FText::AsNumber(PositionalKeys.Num()), FText::AsNumber(RotationalKeys.Num()), FText::AsNumber(ScalingKeys.Num()));
	}

	return false;
}

int32 DiscreteInclusiveLower(const TRange<int32>& InRange)
{
	check(!InRange.GetLowerBound().IsOpen());

	// Add one for exclusive lower bounds since they start on the next subsequent frame
	static const int32 Offsets[]   = { 0, 1 };
	const int32        OffsetIndex = (int32)InRange.GetLowerBound().IsExclusive();

	return InRange.GetLowerBound().GetValue() + Offsets[OffsetIndex];
}

int32 DiscreteExclusiveUpper(const TRange<int32>& InRange)
{
	check(!InRange.GetUpperBound().IsOpen());

	// Add one for inclusive upper bounds since they finish on the next subsequent frame
	static const int32 Offsets[]   = { 0, 1 };
	const int32        OffsetIndex = (int32)InRange.GetUpperBound().IsInclusive();

	return InRange.GetUpperBound().GetValue() + Offsets[OffsetIndex];
}

bool UAnimDataController::UpdateBoneTrackKeys(FName BoneName, const FInt32Range& KeyRangeToSet, const TArray<FVector3f>& PositionalKeys, const TArray<FQuat4f>& RotationalKeys, const TArray<FVector3f>& ScalingKeys, bool bShouldTransact)
{	
	if (!CheckOuterClass(UAnimSequence::StaticClass()))
	{
		return false;
	}

	CONDITIONAL_TRANSACTION(LOCTEXT("SetTrackKeysRangeTransaction", "Setting Animation Data Track keys"));

	// Validate key format
	const int32 MaxNumKeys = FMath::Max(FMath::Max(PositionalKeys.Num(), RotationalKeys.Num()), ScalingKeys.Num());
	const int32 RangeMin = DiscreteInclusiveLower(KeyRangeToSet);
	const int32 RangeMax = DiscreteExclusiveUpper(KeyRangeToSet);
	if (MaxNumKeys > 0)
	{
		const bool bValidPosKeys = PositionalKeys.Num() == MaxNumKeys;
		const bool bValidRotKeys = RotationalKeys.Num() == MaxNumKeys;
		const bool bValidScaleKeys = ScalingKeys.Num() == MaxNumKeys;

		if (bValidPosKeys && bValidRotKeys && bValidScaleKeys)
		{
			const int32 NumKeysToSet = RangeMax - RangeMin;
			if (NumKeysToSet == MaxNumKeys)
			{
				if (FBoneAnimationTrack* TrackPtr = Model->FindMutableBoneTrackByName(BoneName))
				{
					const FInt32Range TrackKeyRange(0, TrackPtr->InternalTrackData.PosKeys.Num());
					if(TrackKeyRange.Contains(KeyRangeToSet))
					{
						FRawAnimSequenceTrack& InternalTrackData = TrackPtr->InternalTrackData;
						TArray<FVector3f>& TrackPosKeys = InternalTrackData.PosKeys;
						TArray<FQuat4f>& TrackRotKeys = InternalTrackData.RotKeys;
						TArray<FVector3f>& TrackScaleKeys = InternalTrackData.ScaleKeys;

						CONDITIONAL_ACTION(UE::Anim::FSetTrackKeysAction, *TrackPtr);

						int32 KeyIndex = 0;
						for (int32 FrameIndex = RangeMin; FrameIndex < RangeMax; ++FrameIndex, ++KeyIndex)
						{
							TrackPosKeys[FrameIndex] = PositionalKeys[KeyIndex];
							TrackRotKeys[FrameIndex] = RotationalKeys[KeyIndex];
							TrackScaleKeys[FrameIndex] = ScalingKeys[KeyIndex];
						}

						FAnimationTrackChangedPayload Payload;
						Payload.Name = BoneName;

						Model->Notify(EAnimDataModelNotifyType::TrackChanged, Payload);

						return true;
					}
					else
					{
						ReportWarningf(LOCTEXT("InvalidTrackSetFrameRange", "Range of to-be-set bone track (with name {0}) keys does not overlap existing range"), FText::FromName(BoneName));
					}
				}
				else
				{
					ReportWarningf(LOCTEXT("InvalidTrackNameWarning", "Track with name {0} does not exist"), FText::FromName(BoneName));
				}
			}
			else
			{
				ReportErrorf(LOCTEXT("InvalidKeyIndexRangeError",
								 "Invalid key index range bound [{0}, {1}], expected to equal the size of the positional, rotational and scaling keys {2}"),
								 FText::AsNumber(RangeMin),
								 FText::AsNumber(RangeMax),
								 FText::AsNumber(MaxNumKeys));
			}	
		}
		else
		{
			ReportErrorf(LOCTEXT("InvalidTrackKeyDataError", "Invalid track key data, expected uniform data: number of positional keys {0}, number of rotational keys {1}, number of scaling keys {2}"), FText::AsNumber(PositionalKeys.Num()), FText::AsNumber(RotationalKeys.Num()), FText::AsNumber(ScalingKeys.Num()));
		}
	}
	else
	{
		ReportErrorf(LOCTEXT("MissingTrackKeyDataError", "Missing track key data, expected uniform data: number of positional keys {0}, number of rotational keys {1}, number of scaling keys {2}"), FText::AsNumber(PositionalKeys.Num()), FText::AsNumber(RotationalKeys.Num()), FText::AsNumber(ScalingKeys.Num()));
	}

	return false;
}

bool UAnimDataController::UpdateBoneTrackKeys(FName BoneName, const FInt32Range& KeyRangeToSet, const TArray<FVector>& PositionalKeys, const TArray<FQuat>& RotationalKeys, const TArray<FVector>& ScalingKeys, bool bShouldTransact)
{
	if (!CheckOuterClass(UAnimSequence::StaticClass()))
	{
		return false;
	}

	CONDITIONAL_TRANSACTION(LOCTEXT("SetTrackKeysTransaction", "Setting Animation Data Track keys"));

	// Validate key format
	const int32 MaxNumKeys = FMath::Max(FMath::Max(PositionalKeys.Num(), RotationalKeys.Num()), ScalingKeys.Num());
	const int32 RangeMin = DiscreteInclusiveLower(KeyRangeToSet);
	const int32 RangeMax = DiscreteExclusiveUpper(KeyRangeToSet);
	if (MaxNumKeys > 0)
	{
		const bool bValidPosKeys = PositionalKeys.Num() == MaxNumKeys;
		const bool bValidRotKeys = RotationalKeys.Num() == MaxNumKeys;
		const bool bValidScaleKeys = ScalingKeys.Num() == MaxNumKeys;

		if (bValidPosKeys && bValidRotKeys && bValidScaleKeys)
		{
			const int32 NumKeysToSet = RangeMax - RangeMin;
			if (NumKeysToSet == MaxNumKeys)
			{
				if (FBoneAnimationTrack* TrackPtr = Model->FindMutableBoneTrackByName(BoneName))
				{
					const FInt32Range TrackKeyRange(0, TrackPtr->InternalTrackData.PosKeys.Num());
					if(TrackKeyRange.Contains(KeyRangeToSet))
					{
						FRawAnimSequenceTrack& InternalTrackData = TrackPtr->InternalTrackData;
						TArray<FVector3f>& TrackPosKeys = InternalTrackData.PosKeys;
						TArray<FQuat4f>& TrackRotKeys = InternalTrackData.RotKeys;
						TArray<FVector3f>& TrackScaleKeys = InternalTrackData.ScaleKeys;

						CONDITIONAL_ACTION(UE::Anim::FSetTrackKeysAction, *TrackPtr);

						int32 KeyIndex = 0;
						for (int32 FrameIndex = RangeMin; FrameIndex < RangeMax; ++FrameIndex, ++KeyIndex)
						{
							TrackPosKeys[FrameIndex] = FVector3f(PositionalKeys[KeyIndex]);
							TrackRotKeys[FrameIndex] = FQuat4f(RotationalKeys[KeyIndex]);
							TrackScaleKeys[FrameIndex] = FVector3f(ScalingKeys[KeyIndex]);
						}

						FAnimationTrackChangedPayload Payload;
						Payload.Name = BoneName;

						Model->Notify(EAnimDataModelNotifyType::TrackChanged, Payload);

						return true;
					}
					else
					{
						ReportWarningf(LOCTEXT("InvalidTrackSetFrameRange", "Range of to-be-set bone track (with name {0}) keys does not overlap existing range"), FText::FromName(BoneName));
					}
				}
				else
				{
					ReportWarningf(LOCTEXT("InvalidTrackNameWarning", "Track with name {0} does not exist"), FText::FromName(BoneName));
				}
			}
			else
			{
				ReportErrorf(LOCTEXT("InvalidKeyIndexRangeError",
								 "Invalid key index range bound [{0}, {1}], expected to equal the size of the positional, rotational and scaling keys {2}"),
								 FText::AsNumber(KeyRangeToSet.GetLowerBoundValue()),
								 FText::AsNumber(KeyRangeToSet.GetUpperBoundValue()),
								 FText::AsNumber(MaxNumKeys));
			}	
		}
		else
		{
			ReportErrorf(LOCTEXT("InvalidTrackKeyDataError", "Invalid track key data, expected uniform data: number of positional keys {0}, number of rotational keys {1}, number of scaling keys {2}"), FText::AsNumber(PositionalKeys.Num()), FText::AsNumber(RotationalKeys.Num()), FText::AsNumber(ScalingKeys.Num()));
		}
	}
	else
	{
		ReportErrorf(LOCTEXT("MissingTrackKeyDataError", "Missing track key data, expected uniform data: number of positional keys {0}, number of rotational keys {1}, number of scaling keys {2}"), FText::AsNumber(PositionalKeys.Num()), FText::AsNumber(RotationalKeys.Num()), FText::AsNumber(ScalingKeys.Num()));
	}

	return false;
}

void UAnimDataController::ResizeCurves(float NewLength, bool bInserted, float T0, float T1, bool bShouldTransact /*= true*/)
{
	CONDITIONAL_BRACKET(LOCTEXT("ResizeCurves", "Resizing all Curves"));

	for (FFloatCurve& Curve : Model->CurveData.FloatCurves)
	{
		FFloatCurve ResizedCurve = Curve;
		ResizedCurve.Resize(NewLength, bInserted, T0, T1);
		SetCurveKeys(FAnimationCurveIdentifier(Curve.Name, ERawCurveTrackTypes::RCT_Float), ResizedCurve.FloatCurve.GetConstRefOfKeys(), bShouldTransact);
	}

	for (FTransformCurve& Curve : Model->CurveData.TransformCurves)
	{
		FTransformCurve ResizedCurve = Curve;
		for (int32 SubCurveIndex = 0; SubCurveIndex < 3; ++SubCurveIndex)
		{
			const ETransformCurveChannel Channel = (ETransformCurveChannel)SubCurveIndex;
			FVectorCurve& SubCurve = *ResizedCurve.GetVectorCurveByIndex(SubCurveIndex);
			for (int32 ChannelIndex = 0; ChannelIndex < 3; ++ChannelIndex)
			{
				const EVectorCurveChannel Axis = (EVectorCurveChannel)ChannelIndex;
				FAnimationCurveIdentifier TargetCurveIdentifier = FAnimationCurveIdentifier(Curve.Name, ERawCurveTrackTypes::RCT_Transform);
				UAnimationCurveIdentifierExtensions::GetTransformChildCurveIdentifier(TargetCurveIdentifier, Channel, Axis);
				
				FRichCurve& ChannelCurve = SubCurve.FloatCurves[ChannelIndex];
				ChannelCurve.ReadjustTimeRange(0, NewLength, bInserted, T0, T1);
				SetCurveKeys(TargetCurveIdentifier, ChannelCurve.GetConstRefOfKeys(), bShouldTransact);
			}
		}
	}
}

void UAnimDataController::ResizeAttributes(float NewLength, bool bInserted, float T0, float T1, bool bShouldTransact)
{
	CONDITIONAL_BRACKET(LOCTEXT("ResizeAttributes", "Resizing all Attributes"));

	for (FAnimatedBoneAttribute& Attribute : Model->AnimatedBoneAttributes)
	{
		FAttributeCurve ResizedCurve = Attribute.Curve;
		ResizedCurve.ReadjustTimeRange(0, NewLength, bInserted, T0, T1);

		// Generate arrays necessary for API
		TArray<float> Times;
		TArray<const void*> Values;
		const TArray<FAttributeKey>& Keys = ResizedCurve.GetConstRefOfKeys();
		for (int32 KeyIndex = 0; KeyIndex < ResizedCurve.GetNumKeys(); ++KeyIndex)
		{
			Times.Add(Keys[KeyIndex].Time);
			Values.Add(Keys[KeyIndex].GetValuePtr<void>());
		}
		
		SetAttributeKeys(Attribute.Identifier, Times, Values, Attribute.Curve.GetScriptStruct(), bShouldTransact);
	}
}

bool UAnimDataController::AddAttribute(const FAnimationAttributeIdentifier& AttributeIdentifier, bool bShouldTransact /*= true*/)
{
	if (AttributeIdentifier.IsValid())
	{
		const bool bAttributeAlreadyExists = Model->AnimatedBoneAttributes.ContainsByPredicate([AttributeIdentifier](const FAnimatedBoneAttribute& Attribute) -> bool
		{
			return Attribute.Identifier == AttributeIdentifier;
		});

		if (!bAttributeAlreadyExists)
		{
			CONDITIONAL_TRANSACTION(LOCTEXT("AddAttribute", "Adding Animated Bone Attribute"));

			FAnimatedBoneAttribute& Attribute = Model->AnimatedBoneAttributes.AddDefaulted_GetRef();
			Attribute.Identifier = AttributeIdentifier;

			Attribute.Curve.SetScriptStruct(AttributeIdentifier.GetType());
		
			CONDITIONAL_ACTION(UE::Anim::FRemoveAtributeAction, AttributeIdentifier);

			FAttributeAddedPayload Payload;
			Payload.Identifier = AttributeIdentifier;
			Model->Notify(EAnimDataModelNotifyType::AttributeAdded, Payload);

			return true;
		}
		else
		{
			ReportErrorf(LOCTEXT("AttributeAlreadyExists", "Attribute identifier provided already exists: {0} {1} ({2}) {3}"),
			FText::FromName(AttributeIdentifier.GetName()), FText::FromName(AttributeIdentifier.GetBoneName()), FText::AsNumber(AttributeIdentifier.GetBoneIndex()), FText::FromName(AttributeIdentifier.GetType()->GetFName()));
		}
	}
	else
	{
		Report(ELogVerbosity::Error, LOCTEXT("InvalidAttributeIdentifier", "Invalid attribute identifier provided"));
	}

	return false;
}

bool UAnimDataController::RemoveAttribute(const FAnimationAttributeIdentifier& AttributeIdentifier, bool bShouldTransact /*= true*/)
{
	if (AttributeIdentifier.IsValid())
	{
		const int32 AttributeIndex = Model->AnimatedBoneAttributes.IndexOfByPredicate([AttributeIdentifier](const FAnimatedBoneAttribute& Attribute) -> bool
		{
			return Attribute.Identifier == AttributeIdentifier;
		});

		if (AttributeIndex != INDEX_NONE)
		{
			CONDITIONAL_TRANSACTION(LOCTEXT("RemoveAttribute", "Removing Animated Bone Attribute"));

			CONDITIONAL_ACTION(UE::Anim::FAddAtributeAction, Model->AnimatedBoneAttributes[AttributeIndex]);

			Model->AnimatedBoneAttributes.RemoveAtSwap(AttributeIndex);
			
			FAttributeRemovedPayload Payload;
			Payload.Identifier = AttributeIdentifier;
			Model->Notify(EAnimDataModelNotifyType::AttributeRemoved, Payload);

			return true;
		}
		else
		{
			ReportErrorf(LOCTEXT("AttributeNotFound", "Attribute identifier provided was not found: {0} {1} ({2}) {3}"),
				FText::FromName(AttributeIdentifier.GetName()), FText::FromName(AttributeIdentifier.GetBoneName()), FText::AsNumber(AttributeIdentifier.GetBoneIndex()), FText::FromName(AttributeIdentifier.GetType()->GetFName()));
		}
	}
	else
	{
		Report(ELogVerbosity::Error, LOCTEXT("InvalidAttributeIdentifier", "Invalid attribute identifier provided"));
	}

	return false;
}

int32 UAnimDataController::RemoveAllAttributesForBone(const FName& BoneName, bool bShouldTransact)
{
	int32 NumRemovedAttributes = 0;

	// Generate list of attribute identifiers, matching the bone name, for removal
	TArray<FAnimationAttributeIdentifier> Identifiers;
	Algo::TransformIf(Model->AnimatedBoneAttributes, Identifiers,
		[BoneName](const FAnimatedBoneAttribute& Attribute) -> bool
		{
			return Attribute.Identifier.GetBoneName() == BoneName;
		},
		[](const FAnimatedBoneAttribute& Attribute)
		{
			return Attribute.Identifier;
		}
	);

	if (Identifiers.Num())
	{
		CONDITIONAL_BRACKET(LOCTEXT("RemoveAllAttributesForBone", "Removing all Attributes for Bone"));
		for (const FAnimationAttributeIdentifier& Identifier : Identifiers)
		{
			NumRemovedAttributes += RemoveAttribute(Identifier, bShouldTransact) ? 1 : 0;
		}
	}	

	return NumRemovedAttributes;
}


int32 UAnimDataController::RemoveAllAttributes(bool bShouldTransact)
{
	int32 NumRemovedAttributes = 0;

	TArray<FAnimationAttributeIdentifier> Identifiers;
	Algo::Transform(Model->AnimatedBoneAttributes, Identifiers, [](const FAnimatedBoneAttribute& Attribute)
	{
		return Attribute.Identifier;
	});

	if (Identifiers.Num())
	{
		CONDITIONAL_BRACKET(LOCTEXT("RemoveAllAttributes", "Removing all Attributes"));
		for (const FAnimationAttributeIdentifier& Identifier : Identifiers)
		{
			NumRemovedAttributes += RemoveAttribute(Identifier, bShouldTransact) ? 1: 0;
		}
	}

	return NumRemovedAttributes;
}

bool UAnimDataController::SetAttributeKey_Internal(const FAnimationAttributeIdentifier& AttributeIdentifier, float Time, const void* KeyValue, const UScriptStruct* TypeStruct, bool bShouldTransact /*= true*/)
{
	if (AttributeIdentifier.IsValid())
	{
		if (KeyValue)
		{
			FAnimatedBoneAttribute* AttributePtr = Model->AnimatedBoneAttributes.FindByPredicate([AttributeIdentifier](FAnimatedBoneAttribute& Attribute)
			{
				return Attribute.Identifier == AttributeIdentifier;
			});

			if (AttributePtr)
			{
				if (TypeStruct == AttributePtr->Identifier.GetType())
				{
					CONDITIONAL_TRANSACTION(LOCTEXT("SettingAttributeKey", "Setting Animated Bone Attribute key"));

					FAttributeCurve& Curve = AttributePtr->Curve;
					FKeyHandle KeyHandle = Curve.FindKey(Time);
					// In case the key does not yet exist one will be added, and thus the undo is a remove
					if (KeyHandle == FKeyHandle::Invalid())
					{
						CONDITIONAL_ACTION(UE::Anim::FRemoveAtributeKeyAction, AttributeIdentifier, Time);
						Curve.UpdateOrAddKey(Time, KeyValue);
					}
					// In case the key does exist it will be updated , and thus the undo is a revert to the current value
					else
					{
						CONDITIONAL_ACTION(UE::Anim::FSetAtributeKeyAction, AttributeIdentifier, Curve.GetKey(KeyHandle));
						Curve.UpdateOrAddKey(Time, KeyValue);
					}

					FAttributeChangedPayload Payload;
					Payload.Identifier = AttributeIdentifier;
					Model->Notify(EAnimDataModelNotifyType::AttributeChanged, Payload);

					return true;
				}
				else
				{
					ReportErrorf(LOCTEXT("AttributeTypeDoesNotMatchKeyType", "Key type does not match attribute: {0} {1}"),
						FText::FromName(AttributePtr->Identifier.GetType()->GetFName()), FText::FromName(TypeStruct->GetFName()));
				}
			}
			else
			{
				ReportErrorf(LOCTEXT("AttributeNotFound", "Attribute identifier provided was not found: {0} {1} ({2}) {3}"),
					FText::FromName(AttributeIdentifier.GetName()), FText::FromName(AttributeIdentifier.GetBoneName()), FText::AsNumber(AttributeIdentifier.GetBoneIndex()), FText::FromName(AttributeIdentifier.GetType()->GetFName()));
			}
		}
		else
		{
			Report(ELogVerbosity::Error, LOCTEXT("InvalidAttributeKey", "Invalid attribute key value provided"));
		}
	}
	else
	{
		Report(ELogVerbosity::Error, LOCTEXT("InvalidAttributeIdentifier", "Invalid attribute identifier provided"));
	}

	return false;
}

bool UAnimDataController::SetAttributeKeys_Internal(const FAnimationAttributeIdentifier& AttributeIdentifier, TArrayView<const float> Times, TArrayView<const void*> KeyValues, const UScriptStruct* TypeStruct, bool bShouldTransact)
{
	if (AttributeIdentifier.IsValid())
	{
		if (Times.Num() == KeyValues.Num())
		{
			FAnimatedBoneAttribute* AttributePtr = Model->AnimatedBoneAttributes.FindByPredicate([AttributeIdentifier](FAnimatedBoneAttribute& Attribute)
			{
				return Attribute.Identifier == AttributeIdentifier;
			});

			if (AttributePtr)
			{
				if (TypeStruct == AttributePtr->Identifier.GetType())
				{
					CONDITIONAL_TRANSACTION(LOCTEXT("SettingAttributeKeys", "Setting Animated Bone Attribute keys"));

					FAnimatedBoneAttribute& Attribute = *AttributePtr;

					CONDITIONAL_ACTION(UE::Anim::FSetAtributeKeysAction, Attribute);
			
					Attribute.Curve.SetKeys(Times, KeyValues);

					FAttributeChangedPayload Payload;
					Payload.Identifier = AttributeIdentifier;
					Model->Notify(EAnimDataModelNotifyType::AttributeChanged, Payload);

					return true;
				}
				else
				{
					ReportErrorf(LOCTEXT("AttributeTypeDoesNotMatchKeyType", "Key type does not match attribute: {0} {1}"),
						FText::FromName(AttributePtr->Identifier.GetType()->GetFName()), FText::FromName(TypeStruct->GetFName()));
				}
			}
			else
			{
				ReportErrorf(LOCTEXT("AttributeNotFound", "Attribute identifier provided was not found: {0} {1} ({2}) {3}"),
					FText::FromName(AttributeIdentifier.GetName()), FText::FromName(AttributeIdentifier.GetBoneName()), FText::AsNumber(AttributeIdentifier.GetBoneIndex()), FText::FromName(AttributeIdentifier.GetType()->GetFName()));
			}
		}
		else
		{
			ReportErrorf(LOCTEXT("AttributeKeysMismatch", "Non matching number of key time/values: time entries {0} key entries {1}"),
				FText::AsNumber(Times.Num()), FText::AsNumber(KeyValues.Num()));
		}
	}
	else
	{
		Report(ELogVerbosity::Error, LOCTEXT("InvalidAttributeIdentifier", "Invalid attribute identifier provided"));
	}

	return false;
}


bool UAnimDataController::RemoveAttributeKey(const FAnimationAttributeIdentifier& AttributeIdentifier, float Time, bool bShouldTransact /*= true*/)
{
	if (AttributeIdentifier.IsValid())
	{
		FAnimatedBoneAttribute* AttributePtr = Model->AnimatedBoneAttributes.FindByPredicate([AttributeIdentifier](const FAnimatedBoneAttribute& Attribute)
		{
			return Attribute.Identifier == AttributeIdentifier;
		});

		if (AttributePtr)
		{
			FAttributeCurve& Curve = AttributePtr->Curve;
			FKeyHandle KeyHandle = Curve.FindKey(Time);

			if (KeyHandle != FKeyHandle::Invalid())
			{
				CONDITIONAL_TRANSACTION(LOCTEXT("RemovingAttributeKey", "Removing Animated Bone Attribute key"));

				CONDITIONAL_ACTION(UE::Anim::FAddAtributeKeyAction, AttributeIdentifier, Curve.GetKey(KeyHandle));

				Curve.DeleteKey(KeyHandle);

				FAttributeAddedPayload Payload;
				Payload.Identifier = AttributeIdentifier;
				Model->Notify(EAnimDataModelNotifyType::AttributeChanged, Payload);

				return true;
			}
			else
			{
				Report(ELogVerbosity::Warning, LOCTEXT("AttributeKeyNotFound", "Attribute does not contain key for provided time"));
			}	
		}
		else
		{
			ReportErrorf(LOCTEXT("AttributeNotFound", "Attribute identifier provided was not found: {0} {1} ({2}) {3}"),
				FText::FromName(AttributeIdentifier.GetName()), FText::FromName(AttributeIdentifier.GetBoneName()), FText::AsNumber(AttributeIdentifier.GetBoneIndex()), FText::FromName(AttributeIdentifier.GetType()->GetFName()));
		}
	}
	else
	{
		Report(ELogVerbosity::Error, LOCTEXT("InvalidAttributeIdentifier", "Invalid attribute identifier provided"));
	}

	return false;
}

bool UAnimDataController::DuplicateAttribute(const FAnimationAttributeIdentifier& AttributeIdentifier, const FAnimationAttributeIdentifier& NewAttributeIdentifier, bool bShouldTransact)
{
	ValidateModel();

	if (AttributeIdentifier.IsValid() && NewAttributeIdentifier.IsValid())
	{
		if (AttributeIdentifier.GetType() == NewAttributeIdentifier.GetType())
		{
			const FAnimatedBoneAttribute* ExistingAttributePtr = Model->FindAttribute(NewAttributeIdentifier);
			if (ExistingAttributePtr == nullptr)
			{
				if(const FAnimatedBoneAttribute* AttributePtr = Model->FindAttribute(AttributeIdentifier))
				{
					CONDITIONAL_TRANSACTION(LOCTEXT("DuplicateAttribute", "Duplicating Animation Attribute"));

					FAnimatedBoneAttribute& DuplicateAttribute = Model->AnimatedBoneAttributes.AddDefaulted_GetRef();
					DuplicateAttribute.Identifier = NewAttributeIdentifier;
					DuplicateAttribute.Curve = AttributePtr->Curve;

					FAttributeAddedPayload Payload;
					Payload.Identifier = NewAttributeIdentifier;
					Model->Notify(EAnimDataModelNotifyType::AttributeAdded, Payload);

					CONDITIONAL_ACTION(UE::Anim::FRemoveAtributeAction, NewAttributeIdentifier);
					
					return true;
				}
				else
				{
					ReportWarningf(LOCTEXT("AttributeNameToDuplicateNotFoundWarning", "Could not find attribute with name {0} and type {1} for duplication"), FText::FromName(AttributeIdentifier.GetName()), FText::FromString(AttributeIdentifier.GetType()->GetName()));
				}
			}
			else
			{
				ReportWarningf(LOCTEXT("ExistingAttributeWarning", "Attribute with name {0} already exists"), FText::FromName(AttributeIdentifier.GetName()));
			}
		}
		else
		{
			ReportWarningf(LOCTEXT("InvalidAttributeTypeWarning", "Attribute types do not match: {0} ({1})"), FText::FromString(AttributeIdentifier.GetType()->GetName()), FText::FromString(AttributeIdentifier.GetType()->GetName()));
		}
	}
	else
	{
		ReportWarningf(LOCTEXT("InvalidAttributeIdentifierWarning", "Invalid attribute identifier(s) provided: {0} and/or {1}"), FText::FromName(AttributeIdentifier.GetName()), FText::FromName(AttributeIdentifier.GetName()));
	}

	return false;
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE // "AnimDataController"


