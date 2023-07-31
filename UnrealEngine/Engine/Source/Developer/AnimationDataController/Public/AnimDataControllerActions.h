// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CurveDataAbstraction.h"
#include "Misc/Change.h"

#include "Animation/AnimData/AnimDataModel.h"
#include "Animation/AnimTypes.h" 
#include "Misc/FrameRate.h"

class UObject;
class UAnimDataModel;
class UAnimDataController;

#if WITH_EDITOR

namespace UE {

namespace Anim {

/**
* UAnimDataController instanced FChange-based objects used for storing mutations to an UAnimDataModel within the Transaction Buffer.
* Each Action class represents an (invertable) operation mutating an UAnimDataModel object utilizing a UAnimDataController. Allowing 
* for a more granular approach to undo/redo-ing changes while also allowing for script-based interoperability.
*/
class FAnimDataBaseAction : public FSwapChange
{
public:
	virtual TUniquePtr<FChange> Execute(UObject* Object) final;
	virtual ~FAnimDataBaseAction() {}
	virtual FString ToString() const override;

protected:
	virtual TUniquePtr<FChange> ExecuteInternal(UAnimDataModel* Model, UAnimDataController* Controller) = 0;
	virtual FString ToStringInternal() const = 0;
};

class FOpenBracketAction : public FAnimDataBaseAction
{
public:
	explicit FOpenBracketAction(const FString& InDescription) : Description(InDescription) {}
	virtual ~FOpenBracketAction() {}
protected:
	FOpenBracketAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(UAnimDataModel* Model, UAnimDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FString Description;
};

class FCloseBracketAction : public FAnimDataBaseAction
{
public:
	explicit FCloseBracketAction(const FString& InDescription) : Description(InDescription) {}
	virtual ~FCloseBracketAction() {}
protected:
	FCloseBracketAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(UAnimDataModel* Model, UAnimDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FString Description;
};

class FAddTrackAction : public FAnimDataBaseAction
{
public:
	explicit FAddTrackAction(const FBoneAnimationTrack& Track, int32 TrackIndex);
	virtual ~FAddTrackAction() {}
protected:
	FAddTrackAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(UAnimDataModel* Model, UAnimDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FName Name;
	int32 BoneTreeIndex;
	FRawAnimSequenceTrack Data;
	int32 TrackIndex;
};

class FRemoveTrackAction : public FAnimDataBaseAction
{
public:
	explicit FRemoveTrackAction(const FBoneAnimationTrack& Track, int32 TrackIndex);
	virtual ~FRemoveTrackAction() {}
protected:
	FRemoveTrackAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(UAnimDataModel* Model, UAnimDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FName Name;
	int32 TrackIndex;
};

class FSetTrackKeysAction : public FAnimDataBaseAction
{
public:
	explicit FSetTrackKeysAction(const FBoneAnimationTrack& Track);
	virtual ~FSetTrackKeysAction() {}
protected:
	FSetTrackKeysAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(UAnimDataModel* Model, UAnimDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FName Name;
	int32 TrackIndex;

	FRawAnimSequenceTrack TrackData;
};

class FResizePlayLengthAction : public FAnimDataBaseAction
{
public:
	explicit FResizePlayLengthAction(const UAnimDataModel* InModel, float t0, float t1);
	virtual ~FResizePlayLengthAction() {}
protected:
	FResizePlayLengthAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(UAnimDataModel* Model, UAnimDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	float Length;
	float T0;
	float T1;
};

class FSetFrameRateAction : public FAnimDataBaseAction
{
public:
	explicit FSetFrameRateAction(const UAnimDataModel* InModel);
	virtual ~FSetFrameRateAction() {}
protected:
	FSetFrameRateAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(UAnimDataModel* Model, UAnimDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FFrameRate FrameRate;
};

class FAddCurveAction : public FAnimDataBaseAction
{
public:
	explicit FAddCurveAction(const FAnimationCurveIdentifier& InCurveId, int32 InFlags) : CurveId(InCurveId), Flags(InFlags) {}

	virtual ~FAddCurveAction() {}
protected:
	FAddCurveAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(UAnimDataModel* Model, UAnimDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationCurveIdentifier CurveId;
	int32 Flags;
};

class FAddFloatCurveAction : public FAnimDataBaseAction
{
public:
	explicit FAddFloatCurveAction(const FAnimationCurveIdentifier& InCurveId, int32 InFlags, const TArray<FRichCurveKey>& InKeys, const FLinearColor& InColor) : CurveId(InCurveId), Flags(InFlags), Keys(InKeys), Color(InColor) {}
	virtual ~FAddFloatCurveAction() {}
protected:
	FAddFloatCurveAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(UAnimDataModel* Model, UAnimDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationCurveIdentifier CurveId;
	int32 Flags;
	TArray<FRichCurveKey> Keys;
	FLinearColor Color;
};

class FAddTransformCurveAction : public FAnimDataBaseAction
{
public:
	explicit FAddTransformCurveAction(const FAnimationCurveIdentifier& InCurveId, int32 InFlags, const FTransformCurve& InTransformCurve);
	virtual ~FAddTransformCurveAction() {}
protected:
	FAddTransformCurveAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(UAnimDataModel* Model, UAnimDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationCurveIdentifier CurveId;
	int32 Flags;
	
	TArray<FRichCurveKey> SubCurveKeys[9];
};

class FRemoveCurveAction : public FAnimDataBaseAction
{
public:
	explicit FRemoveCurveAction(const FAnimationCurveIdentifier& InCurveId) : CurveId(InCurveId) {}
	virtual ~FRemoveCurveAction() {}
protected:
	FRemoveCurveAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(UAnimDataModel* Model, UAnimDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationCurveIdentifier CurveId;
};

class FSetCurveFlagsAction : public FAnimDataBaseAction
{
public:
	explicit FSetCurveFlagsAction(const FAnimationCurveIdentifier& InCurveId, int32 InFlags, ERawCurveTrackTypes InCurveType) : CurveId(InCurveId), Flags(InFlags), CurveType(InCurveType) {}
	virtual ~FSetCurveFlagsAction() {}
protected:
	FSetCurveFlagsAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(UAnimDataModel* Model, UAnimDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationCurveIdentifier CurveId;
	int32 Flags;
	ERawCurveTrackTypes CurveType;
};

class FRenameCurveAction : public FAnimDataBaseAction
{
public:
	explicit FRenameCurveAction(const FAnimationCurveIdentifier& InCurveId, const FAnimationCurveIdentifier& InNewCurveId) : CurveId(InCurveId), NewCurveId(InNewCurveId) {}
	virtual ~FRenameCurveAction() {}
protected:
	FRenameCurveAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(UAnimDataModel* Model, UAnimDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationCurveIdentifier CurveId;
	FAnimationCurveIdentifier NewCurveId;
};

class FScaleCurveAction : public FAnimDataBaseAction
{
public:
	explicit FScaleCurveAction(const FAnimationCurveIdentifier& InCurveId, float InOrigin, float InFactor, ERawCurveTrackTypes InCurveType);
	virtual ~FScaleCurveAction() {}
protected:
	FScaleCurveAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(UAnimDataModel* Model, UAnimDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationCurveIdentifier CurveId;
	ERawCurveTrackTypes CurveType;
	float Origin;
	float Factor;
};

class FAddRichCurveKeyAction : public FAnimDataBaseAction
{
public:
	explicit FAddRichCurveKeyAction(const FAnimationCurveIdentifier& InCurveId, const FRichCurveKey& InKey) : CurveId(InCurveId), Key(InKey) {}
	virtual ~FAddRichCurveKeyAction() {}
protected:
	FAddRichCurveKeyAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(UAnimDataModel* Model, UAnimDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationCurveIdentifier CurveId;
	ERawCurveTrackTypes CurveType;
	FRichCurveKey Key;
};

class FSetRichCurveKeyAction : public FAnimDataBaseAction
{
public:
	explicit FSetRichCurveKeyAction(const FAnimationCurveIdentifier& InCurveId, const FRichCurveKey& InKey) : CurveId(InCurveId), Key(InKey) {}
	virtual ~FSetRichCurveKeyAction() {}
protected:
	FSetRichCurveKeyAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(UAnimDataModel* Model, UAnimDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationCurveIdentifier CurveId;
	FRichCurveKey Key;
};

class FRemoveRichCurveKeyAction : public FAnimDataBaseAction
{
public:
	explicit FRemoveRichCurveKeyAction(const FAnimationCurveIdentifier& InCurveId, const float InTime) : CurveId(InCurveId), Time(InTime) {}
	virtual ~FRemoveRichCurveKeyAction() {}
protected:
	FRemoveRichCurveKeyAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(UAnimDataModel* Model, UAnimDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationCurveIdentifier CurveId;
	float Time;
};

class FSetRichCurveKeysAction : public FAnimDataBaseAction
{
public:
	explicit FSetRichCurveKeysAction(const FAnimationCurveIdentifier& InCurveId, const TArray<FRichCurveKey>& InKeys) : CurveId(InCurveId), Keys(InKeys) {}
	virtual ~FSetRichCurveKeysAction() {}
protected:
	FSetRichCurveKeysAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(UAnimDataModel* Model, UAnimDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationCurveIdentifier CurveId;
	TArray<FRichCurveKey> Keys;
};

class FSetRichCurveAttributesAction : public FAnimDataBaseAction
{
public:
	explicit FSetRichCurveAttributesAction(const FAnimationCurveIdentifier& InCurveId, const FCurveAttributes& InAttributes) : CurveId(InCurveId), Attributes(InAttributes) {}
	virtual ~FSetRichCurveAttributesAction() {}
protected:
	FSetRichCurveAttributesAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(UAnimDataModel* Model, UAnimDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationCurveIdentifier CurveId;
	FCurveAttributes Attributes;
};

class FSetCurveColorAction : public FAnimDataBaseAction
{
public:
	explicit FSetCurveColorAction(const FAnimationCurveIdentifier& InCurveId, const FLinearColor& InColor) : CurveId(InCurveId), Color(InColor) {}
	virtual ~FSetCurveColorAction() {}
protected:
	FSetCurveColorAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(UAnimDataModel* Model, UAnimDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationCurveIdentifier CurveId;
	FLinearColor Color;
};

class FAddAtributeAction : public FAnimDataBaseAction
{
public:
	explicit FAddAtributeAction(const FAnimatedBoneAttribute& InAttribute);
	virtual ~FAddAtributeAction() {}
protected:
	FAddAtributeAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(UAnimDataModel* Model, UAnimDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationAttributeIdentifier AttributeId;
	TArray<FAttributeKey> Keys;
};

class FRemoveAtributeAction : public FAnimDataBaseAction
{
public:
	explicit FRemoveAtributeAction(const FAnimationAttributeIdentifier& InAttributeId) : AttributeId(InAttributeId) {}
	virtual ~FRemoveAtributeAction() {}
protected:
	FRemoveAtributeAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(UAnimDataModel* Model, UAnimDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationAttributeIdentifier AttributeId;
};

class FAddAtributeKeyAction : public FAnimDataBaseAction
{
public:
	explicit FAddAtributeKeyAction(const FAnimationAttributeIdentifier& InAttributeId, const FAttributeKey& InKey) : AttributeId(InAttributeId), Key(InKey) {}
	virtual ~FAddAtributeKeyAction() {}
protected:
	FAddAtributeKeyAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(UAnimDataModel* Model, UAnimDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationAttributeIdentifier AttributeId;
	FAttributeKey Key;
};

class FSetAtributeKeyAction : public FAnimDataBaseAction
{
public:
	explicit FSetAtributeKeyAction(const FAnimationAttributeIdentifier& InAttributeId, const FAttributeKey& InKey) : AttributeId(InAttributeId), Key(InKey) {}
	virtual ~FSetAtributeKeyAction() {}
protected:
	FSetAtributeKeyAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(UAnimDataModel* Model, UAnimDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationAttributeIdentifier AttributeId;
	FAttributeKey Key;
};

class FRemoveAtributeKeyAction : public FAnimDataBaseAction
{
public:
	explicit FRemoveAtributeKeyAction(const FAnimationAttributeIdentifier& InAttributeId, float InTime) : AttributeId(InAttributeId), Time(InTime) {}
	virtual ~FRemoveAtributeKeyAction() {}
protected:
	FRemoveAtributeKeyAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(UAnimDataModel* Model, UAnimDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationAttributeIdentifier AttributeId;
	float Time;
};

class FSetAtributeKeysAction : public FAnimDataBaseAction
{
public:
	explicit FSetAtributeKeysAction(const FAnimatedBoneAttribute& InAttribute);
	virtual ~FSetAtributeKeysAction() {}
protected:
	FSetAtributeKeysAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(UAnimDataModel* Model, UAnimDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationAttributeIdentifier AttributeId;
	TArray<FAttributeKey> Keys;
};

} // namespace Anim

} // namespace UE

#endif // WITH_EDITOR