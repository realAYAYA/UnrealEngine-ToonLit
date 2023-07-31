// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "IWaveformTransformation.h"
#include "Sound/SoundWave.h"
#include "Templates/Function.h"
#include "UObject/UnrealType.h"

class FCursorReply;
class FPaintArgs;
class FReply;
class FSlateRect;
class FSlateWindowElementList;
class FWidgetStyle;
class SWidget;
struct FGeometry;
struct FPointerEvent;

struct WAVEFORMEDITORWIDGETS_API FWaveformTransformationRenderInfo
{
	float SampleRate = 0.f;
	int32 NumChannels = 0;
	uint32 StartFrameOffset = 0;	//the first frame this transformation can act upon
	uint32 NumAvilableSamples = 0;	//the number of samples available to the transformation
};

class WAVEFORMEDITORWIDGETS_API IWaveformTransformationRenderer
{
public:
	virtual ~IWaveformTransformationRenderer() {};

	/* SWidget interceptor methods */
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const = 0;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) = 0;
	virtual FReply OnMouseButtonDown(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) = 0;
	virtual FReply OnMouseButtonUp(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) = 0;
	virtual FReply OnMouseButtonDoubleClick(SWidget& OwnerWidget, const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) = 0;
	virtual FReply OnMouseMove(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) = 0;
	virtual FReply OnMouseWheel(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) = 0;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) = 0;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) = 0;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const = 0;

	/* Gets info about the transformation being rendered */
	void SetTransformationWaveInfo(FWaveformTransformationRenderInfo InWaveInfo)
	{
		TransformationWaveInfo = InWaveInfo;
	}

	/* Sets a callback to notify when a property has been changed by the renderer */
	void SetTransformationNotifier(const TFunction<void(FPropertyChangedEvent&, FEditPropertyChain*)>& InTransformationChangeNotifier)
	{
		TransformationChangeNotifier = InTransformationChangeNotifier;
	}

protected:

	/* Notifies that a property has been changed through TransformationChangeNotifier  */
	void NotifyTransformationPropertyChanged(const TObjectPtr<UWaveformTransformationBase> EditedTransformation, const FName& PropertyName, const EPropertyChangeType::Type InChangeType)
	{
		FProperty* Property = EditedTransformation->GetClass()->FindPropertyByName(PropertyName);
		check(Property);

		USoundWave* ParentSoundWave = EditedTransformation->GetTypedOuter<USoundWave>();
		check(ParentSoundWave)

		FProperty* TransformationsProperty = ParentSoundWave->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(USoundWave, Transformations));
		check(TransformationsProperty)

		FEditPropertyChain EditChain;
		EditChain.AddHead(TransformationsProperty);
		EditChain.AddTail(Property);
		EditChain.SetActivePropertyNode(Property);

		FPropertyChangedEvent PropertyEvent(Property, InChangeType);
		FPropertyChangedEvent TransformationsPropertyEvent(TransformationsProperty, InChangeType);

		EditedTransformation->PostEditChangeProperty(PropertyEvent);
		ParentSoundWave->PostEditChangeProperty(TransformationsPropertyEvent);

		TransformationChangeNotifier(PropertyEvent, &EditChain);
	}

	FWaveformTransformationRenderInfo TransformationWaveInfo;

private:
	TFunction<void(FPropertyChangedEvent&, FEditPropertyChain*)> TransformationChangeNotifier;

};