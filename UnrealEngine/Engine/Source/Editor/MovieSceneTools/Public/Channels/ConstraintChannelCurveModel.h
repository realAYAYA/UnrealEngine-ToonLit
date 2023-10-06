// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Channels/MovieSceneChannelHandle.h"
#include "ConstraintChannel.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "CoreTypes.h"
#include "CurveEditorTypes.h"
#include "Curves/KeyHandle.h"
#include "Delegates/IDelegateInstance.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "KeyBarCurveModel.h"
#include "Misc/Optional.h"
#include "Templates/SharedPointer.h"
#include "Templates/Tuple.h"
#include "UObject/UnrealType.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class FCurveEditor;
class FMenuBuilder;
class ISequencer;
class UMovieSceneSection;
class UObject;
class UTickableConstraint;
struct FCurveAttributes;
struct FCurveEditorScreenSpace;
struct FKeyAttributes;
struct FKeyDrawInfo;
struct FKeyPosition;
struct FMovieSceneConstraintChannel;

class FConstraintChannelCurveModel : public FKeyBarCurveModel
{
public:
	static ECurveEditorViewID ViewID;

	FConstraintChannelCurveModel(TMovieSceneChannelHandle<FMovieSceneConstraintChannel> InChannel, UMovieSceneSection* InOwningSection, TWeakPtr<ISequencer> InWeakSequencer);
	~FConstraintChannelCurveModel();
	//FCurveModel
	virtual const void* GetCurve() const override;

	virtual void Modify() override;

	virtual void DrawCurve(const FCurveEditor& CurveEditor, const FCurveEditorScreenSpace& ScreenSpace, TArray<TTuple<double, double>>& InterpolatingPoints) const override;
	virtual void GetKeys(const FCurveEditor& CurveEditor, double MinTime, double MaxTime, double MinValue, double MaxValue, TArray<FKeyHandle>& OutKeyHandles) const override;
	virtual void GetKeyDrawInfo(ECurvePointType PointType, const FKeyHandle InKeyHandle, FKeyDrawInfo& OutDrawInfo) const override;

	virtual void GetKeyPositions(TArrayView<const FKeyHandle> InKeys, TArrayView<FKeyPosition> OutKeyPositions) const override;
	virtual void SetKeyPositions(TArrayView<const FKeyHandle> InKeys, TArrayView<const FKeyPosition> InKeyPositions, EPropertyChangeType::Type ChangeType) override;

	virtual void GetKeyAttributes(TArrayView<const FKeyHandle> InKeys, TArrayView<FKeyAttributes> OutAttributes) const override;
	virtual void SetKeyAttributes(TArrayView<const FKeyHandle> InKeys, TArrayView<const FKeyAttributes> InAttributes, EPropertyChangeType::Type ChangeType = EPropertyChangeType::Unspecified) override;

	virtual void GetCurveAttributes(FCurveAttributes& OutCurveAttributes) const override;
	virtual void SetCurveAttributes(const FCurveAttributes& InCurveAttributes) override;
	virtual void GetTimeRange(double& MinTime, double& MaxTime) const override;
	virtual void GetValueRange(double& MinValue, double& MaxValue) const override;
	virtual int32 GetNumKeys() const override;
	virtual void GetNeighboringKeys(const FKeyHandle InKeyHandle, TOptional<FKeyHandle>& OutPreviousKeyHandle, TOptional<FKeyHandle>& OutNextKeyHandle) const override {}
	virtual bool Evaluate(double ProspectiveTime, double& OutValue) const override;
	virtual void AddKeys(TArrayView<const FKeyPosition> InKeyPositions, TArrayView<const FKeyAttributes> InAttributes, TArrayView<TOptional<FKeyHandle>>* OutKeyHandles) override;
	virtual void RemoveKeys(TArrayView<const FKeyHandle> InKeys) override;

	virtual void CreateKeyProxies(TArrayView<const FKeyHandle> InKeyHandles, TArrayView<UObject*> OutObjects) override;
	
	virtual UObject* GetOwningObject() const override;
	virtual bool HasChangedAndResetTest() override;
	
	//FKeyBarCurveModel
	virtual void BuildContextMenu(const FCurveEditor& CurveEditor, FMenuBuilder& MenuBuilder, TOptional<FCurvePointHandle> ClickedPoint);
	virtual TArray<FKeyBarCurveModel::FBarRange> FindRanges();


private:
	//Fixup the curve and get the right proxy later when we access it due to the fact the constraints channels may not be setup correctly
	//These functions all need to be const since they can be called by const functions so we need to specify the things they change as mutable
	void FixupCurve() const;
	void NeedNewProxy() const;
	FMovieSceneConstraintChannel* GetChannel() const;

private:
	mutable TMovieSceneChannelHandle<FMovieSceneConstraintChannel> ChannelHandle;
	TWeakObjectPtr<UMovieSceneSection> WeakSection;
	FGuid LastSignature;
	TWeakPtr<ISequencer> WeakSequencer;
	TWeakObjectPtr<UTickableConstraint> Constraint;

	mutable FDelegateHandle OnDestroyHandle;
	mutable bool bNeedNewProxy = false; 
};