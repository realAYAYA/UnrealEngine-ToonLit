// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CurveDataAbstraction.h"
#include "Misc/Change.h"
#include "Misc/CoreMiscDefines.h"

#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimTypes.h" 
#include "Misc/FrameRate.h"

class UObject;
class IAnimationDataModel;
class IAnimationDataController;

#if WITH_EDITOR

namespace UE {

namespace Anim {

/**
* UAnimDataController instanced FChange-based objects used for storing mutations to an IAnimationDataModel within the Transaction Buffer.
* Each Action class represents an (invertable) operation mutating an IAnimationDataModel object utilizing a UAnimDataController. Allowing 
* for a more granular approach to undo/redo-ing changes while also allowing for script-based interoperability.
*/
class ANIMATIONDATACONTROLLER_API FAnimDataBaseAction : public FSwapChange
{
public:
	virtual TUniquePtr<FChange> Execute(UObject* Object) final;
	virtual ~FAnimDataBaseAction() {}
	virtual FString ToString() const override;

protected:
	virtual TUniquePtr<FChange> ExecuteInternal(IAnimationDataModel* Model, IAnimationDataController* Controller) = 0;
	virtual FString ToStringInternal() const = 0;
};

class ANIMATIONDATACONTROLLER_API FOpenBracketAction : public FAnimDataBaseAction
{
public:
	explicit FOpenBracketAction(const FString& InDescription) : Description(InDescription) {}
	virtual ~FOpenBracketAction() {}
protected:
	FOpenBracketAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(IAnimationDataModel* Model, IAnimationDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FString Description;
};

class ANIMATIONDATACONTROLLER_API FCloseBracketAction : public FAnimDataBaseAction
{
public:
	explicit FCloseBracketAction(const FString& InDescription) : Description(InDescription) {}
	virtual ~FCloseBracketAction() {}
protected:
	FCloseBracketAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(IAnimationDataModel* Model, IAnimationDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FString Description;
};

class ANIMATIONDATACONTROLLER_API FAddTrackAction : public FAnimDataBaseAction
{
public:
	explicit FAddTrackAction(const FName& InName, TArray<FTransform>&& InTransformData);
	virtual ~FAddTrackAction() {}
protected:
	FAddTrackAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(IAnimationDataModel* Model, IAnimationDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FName Name;
	TArray<FTransform> TransformData;
};

class ANIMATIONDATACONTROLLER_API FRemoveTrackAction : public FAnimDataBaseAction
{
public:
	explicit FRemoveTrackAction(const FName& InName);
	virtual ~FRemoveTrackAction() {}
protected:
	FRemoveTrackAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(IAnimationDataModel* Model, IAnimationDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FName Name;
};

class ANIMATIONDATACONTROLLER_API FSetTrackKeysAction : public FAnimDataBaseAction
{
public:
	explicit FSetTrackKeysAction(const FName& InName, TArray<FTransform>& InTransformData);
	virtual ~FSetTrackKeysAction() {}
protected:
	FSetTrackKeysAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(IAnimationDataModel* Model, IAnimationDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FName Name;
	TArray<FTransform> TransformData;
};

class ANIMATIONDATACONTROLLER_API FResizePlayLengthInFramesAction : public FAnimDataBaseAction
{
public:
	explicit FResizePlayLengthInFramesAction(const IAnimationDataModel* InModel, FFrameNumber F0, FFrameNumber F1);
	virtual ~FResizePlayLengthInFramesAction() override {}
protected:
	FResizePlayLengthInFramesAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(IAnimationDataModel* Model, IAnimationDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FFrameNumber Length;
	FFrameNumber Frame0;
	FFrameNumber Frame1;
};

class ANIMATIONDATACONTROLLER_API FSetFrameRateAction : public FAnimDataBaseAction
{
public:
	explicit FSetFrameRateAction(const IAnimationDataModel* InModel);
	virtual ~FSetFrameRateAction() override {}
protected:
	FSetFrameRateAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(IAnimationDataModel* Model, IAnimationDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FFrameRate FrameRate;
};

class ANIMATIONDATACONTROLLER_API FAddCurveAction : public FAnimDataBaseAction
{
public:
	explicit FAddCurveAction(const FAnimationCurveIdentifier& InCurveId, int32 InFlags) : CurveId(InCurveId), Flags(InFlags) {}

	virtual ~FAddCurveAction() {}
protected:
	FAddCurveAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(IAnimationDataModel* Model, IAnimationDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationCurveIdentifier CurveId;
	int32 Flags;
};

class ANIMATIONDATACONTROLLER_API FAddFloatCurveAction : public FAnimDataBaseAction
{
public:
	explicit FAddFloatCurveAction(const FAnimationCurveIdentifier& InCurveId, int32 InFlags, const TArray<FRichCurveKey>& InKeys, const FLinearColor& InColor) : CurveId(InCurveId), Flags(InFlags), Keys(InKeys), Color(InColor) {}
	virtual ~FAddFloatCurveAction() {}
protected:
	FAddFloatCurveAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(IAnimationDataModel* Model, IAnimationDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationCurveIdentifier CurveId;
	int32 Flags;
	TArray<FRichCurveKey> Keys;
	FLinearColor Color;
};

class ANIMATIONDATACONTROLLER_API FAddTransformCurveAction : public FAnimDataBaseAction
{
public:
	explicit FAddTransformCurveAction(const FAnimationCurveIdentifier& InCurveId, int32 InFlags, const FTransformCurve& InTransformCurve);
	virtual ~FAddTransformCurveAction() {}
protected:
	FAddTransformCurveAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(IAnimationDataModel* Model, IAnimationDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationCurveIdentifier CurveId;
	int32 Flags;
	
	TArray<FRichCurveKey> SubCurveKeys[9];
};

class ANIMATIONDATACONTROLLER_API FRemoveCurveAction : public FAnimDataBaseAction
{
public:
	explicit FRemoveCurveAction(const FAnimationCurveIdentifier& InCurveId) : CurveId(InCurveId) {}
	virtual ~FRemoveCurveAction() {}
protected:
	FRemoveCurveAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(IAnimationDataModel* Model, IAnimationDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationCurveIdentifier CurveId;
};

class ANIMATIONDATACONTROLLER_API FSetCurveFlagsAction : public FAnimDataBaseAction
{
public:
	explicit FSetCurveFlagsAction(const FAnimationCurveIdentifier& InCurveId, int32 InFlags, ERawCurveTrackTypes InCurveType) : CurveId(InCurveId), Flags(InFlags), CurveType(InCurveType) {}
	virtual ~FSetCurveFlagsAction() {}
protected:
	FSetCurveFlagsAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(IAnimationDataModel* Model, IAnimationDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationCurveIdentifier CurveId;
	int32 Flags;
	ERawCurveTrackTypes CurveType;
};

class ANIMATIONDATACONTROLLER_API FRenameCurveAction : public FAnimDataBaseAction
{
public:
	explicit FRenameCurveAction(const FAnimationCurveIdentifier& InCurveId, const FAnimationCurveIdentifier& InNewCurveId) : CurveId(InCurveId), NewCurveId(InNewCurveId) {}
	virtual ~FRenameCurveAction() {}
protected:
	FRenameCurveAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(IAnimationDataModel* Model, IAnimationDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationCurveIdentifier CurveId;
	FAnimationCurveIdentifier NewCurveId;
};

class ANIMATIONDATACONTROLLER_API FScaleCurveAction : public FAnimDataBaseAction
{
public:
	explicit FScaleCurveAction(const FAnimationCurveIdentifier& InCurveId, float InOrigin, float InFactor, ERawCurveTrackTypes InCurveType);
	virtual ~FScaleCurveAction() {}
protected:
	FScaleCurveAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(IAnimationDataModel* Model, IAnimationDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationCurveIdentifier CurveId;
	ERawCurveTrackTypes CurveType;
	float Origin;
	float Factor;
};

class ANIMATIONDATACONTROLLER_API FAddRichCurveKeyAction : public FAnimDataBaseAction
{
public:
	explicit FAddRichCurveKeyAction(const FAnimationCurveIdentifier& InCurveId, const FRichCurveKey& InKey) : CurveId(InCurveId), Key(InKey) {}
	virtual ~FAddRichCurveKeyAction() {}
protected:
	FAddRichCurveKeyAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(IAnimationDataModel* Model, IAnimationDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationCurveIdentifier CurveId;
	ERawCurveTrackTypes CurveType;
	FRichCurveKey Key;
};

class ANIMATIONDATACONTROLLER_API FSetRichCurveKeyAction : public FAnimDataBaseAction
{
public:
	explicit FSetRichCurveKeyAction(const FAnimationCurveIdentifier& InCurveId, const FRichCurveKey& InKey) : CurveId(InCurveId), Key(InKey) {}
	virtual ~FSetRichCurveKeyAction() {}
protected:
	FSetRichCurveKeyAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(IAnimationDataModel* Model, IAnimationDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationCurveIdentifier CurveId;
	FRichCurveKey Key;
};

class ANIMATIONDATACONTROLLER_API FRemoveRichCurveKeyAction : public FAnimDataBaseAction
{
public:
	explicit FRemoveRichCurveKeyAction(const FAnimationCurveIdentifier& InCurveId, const float InTime) : CurveId(InCurveId), Time(InTime) {}
	virtual ~FRemoveRichCurveKeyAction() {}
protected:
	FRemoveRichCurveKeyAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(IAnimationDataModel* Model, IAnimationDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationCurveIdentifier CurveId;
	float Time;
};

class ANIMATIONDATACONTROLLER_API FSetRichCurveKeysAction : public FAnimDataBaseAction
{
public:
	explicit FSetRichCurveKeysAction(const FAnimationCurveIdentifier& InCurveId, const TArray<FRichCurveKey>& InKeys) : CurveId(InCurveId), Keys(InKeys) {}
	virtual ~FSetRichCurveKeysAction() {}
protected:
	FSetRichCurveKeysAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(IAnimationDataModel* Model, IAnimationDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationCurveIdentifier CurveId;
	TArray<FRichCurveKey> Keys;
};

class ANIMATIONDATACONTROLLER_API FSetRichCurveAttributesAction : public FAnimDataBaseAction
{
public:
	explicit FSetRichCurveAttributesAction(const FAnimationCurveIdentifier& InCurveId, const FCurveAttributes& InAttributes) : CurveId(InCurveId), Attributes(InAttributes) {}
	virtual ~FSetRichCurveAttributesAction() {}
protected:
	FSetRichCurveAttributesAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(IAnimationDataModel* Model, IAnimationDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationCurveIdentifier CurveId;
	FCurveAttributes Attributes;
};

class ANIMATIONDATACONTROLLER_API FSetCurveColorAction : public FAnimDataBaseAction
{
public:
	explicit FSetCurveColorAction(const FAnimationCurveIdentifier& InCurveId, const FLinearColor& InColor) : CurveId(InCurveId), Color(InColor) {}
	virtual ~FSetCurveColorAction() {}
protected:
	FSetCurveColorAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(IAnimationDataModel* Model, IAnimationDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationCurveIdentifier CurveId;
	FLinearColor Color;
};

class ANIMATIONDATACONTROLLER_API FSetCurveCommentAction : public FAnimDataBaseAction
{
public:
	explicit FSetCurveCommentAction(const FAnimationCurveIdentifier& InCurveId, const FString& InComment) : CurveId(InCurveId), Comment(InComment) {}
	virtual ~FSetCurveCommentAction() {}
protected:
	FSetCurveCommentAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(IAnimationDataModel* Model, IAnimationDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationCurveIdentifier CurveId;
	FString Comment;
};

class ANIMATIONDATACONTROLLER_API FAddAtributeAction : public FAnimDataBaseAction
{
public:
	explicit FAddAtributeAction(const FAnimatedBoneAttribute& InAttribute);
	virtual ~FAddAtributeAction() {}
protected:
	FAddAtributeAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(IAnimationDataModel* Model, IAnimationDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationAttributeIdentifier AttributeId;
	TArray<FAttributeKey> Keys;
};

class ANIMATIONDATACONTROLLER_API FRemoveAtributeAction : public FAnimDataBaseAction
{
public:
	explicit FRemoveAtributeAction(const FAnimationAttributeIdentifier& InAttributeId) : AttributeId(InAttributeId) {}
	virtual ~FRemoveAtributeAction() {}
protected:
	FRemoveAtributeAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(IAnimationDataModel* Model, IAnimationDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationAttributeIdentifier AttributeId;
};

class ANIMATIONDATACONTROLLER_API FAddAtributeKeyAction : public FAnimDataBaseAction
{
public:
	explicit FAddAtributeKeyAction(const FAnimationAttributeIdentifier& InAttributeId, const FAttributeKey& InKey) : AttributeId(InAttributeId), Key(InKey) {}
	virtual ~FAddAtributeKeyAction() {}
protected:
	FAddAtributeKeyAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(IAnimationDataModel* Model, IAnimationDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationAttributeIdentifier AttributeId;
	FAttributeKey Key;
};

class ANIMATIONDATACONTROLLER_API FSetAtributeKeyAction : public FAnimDataBaseAction
{
public:
	explicit FSetAtributeKeyAction(const FAnimationAttributeIdentifier& InAttributeId, const FAttributeKey& InKey) : AttributeId(InAttributeId), Key(InKey) {}
	virtual ~FSetAtributeKeyAction() {}
protected:
	FSetAtributeKeyAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(IAnimationDataModel* Model, IAnimationDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationAttributeIdentifier AttributeId;
	FAttributeKey Key;
};

class ANIMATIONDATACONTROLLER_API FRemoveAtributeKeyAction : public FAnimDataBaseAction
{
public:
	explicit FRemoveAtributeKeyAction(const FAnimationAttributeIdentifier& InAttributeId, float InTime) : AttributeId(InAttributeId), Time(InTime) {}
	virtual ~FRemoveAtributeKeyAction() {}
protected:
	FRemoveAtributeKeyAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(IAnimationDataModel* Model, IAnimationDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationAttributeIdentifier AttributeId;
	float Time;
};

class ANIMATIONDATACONTROLLER_API FSetAtributeKeysAction : public FAnimDataBaseAction
{
public:
	explicit FSetAtributeKeysAction(const FAnimatedBoneAttribute& InAttribute);
	virtual ~FSetAtributeKeysAction() {}
protected:
	FSetAtributeKeysAction() {}
	virtual TUniquePtr<FChange> ExecuteInternal(IAnimationDataModel* Model, IAnimationDataController* Controller) override;
	virtual FString ToStringInternal() const override;

protected:
	FAnimationAttributeIdentifier AttributeId;
	TArray<FAttributeKey> Keys;
};

} // namespace Anim

} // namespace UE

#endif // WITH_EDITOR