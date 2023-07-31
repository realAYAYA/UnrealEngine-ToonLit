// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/SharedPointer.h"
#include "Misc/FrameTime.h"
#include "Math/Vector2D.h"
#include "MVVM/ViewModelTypeID.h"

struct FGuid;
struct FPointerEvent;

namespace UE
{
namespace Sequencer
{

class IStretchOperation;
class FViewModel;
class FVirtualTrackArea;

enum class EStretchConstraint
{
	AnchorToStart, AnchorToEnd,
};

enum class EStretchResult
{
	Success, Failure
};

struct SEQUENCERCORE_API FStretchParameters
{
	double Anchor = 0.0;
	double Handle = 0.0;

	bool IsValid() const
	{
		return Anchor != Handle;
	}
};

struct SEQUENCERCORE_API FStretchScreenParameters
{
	FFrameTime DragStartPosition;
	FFrameTime CurrentDragPosition;

	FVector2D LocalMousePos;

	const FPointerEvent* MouseEvent = nullptr;
	const FVirtualTrackArea* VirtualTrackArea = nullptr;
};

class SEQUENCERCORE_API IStretchableExtension
{
public:

	UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(IStretchableExtension)

	virtual ~IStretchableExtension(){}

	virtual void OnInitiateStretch(IStretchOperation& StretchOperation, EStretchConstraint Constraint, FStretchParameters* InOutGlobalParameters) {}

	virtual EStretchResult OnBeginStretch(const IStretchOperation& StretchOperation, const FStretchScreenParameters& ScreenParameters, FStretchParameters* InOutParameters) { return EStretchResult::Failure; }
	virtual void OnStretch(const IStretchOperation& StretchOperation, const FStretchScreenParameters& ScreenParameters, FStretchParameters* InOutParameters) {}
	virtual void OnEndStretch(const IStretchOperation& StretchOperation, const FStretchScreenParameters& ScreenParametersm, FStretchParameters* InOutParameters) {}
};

class SEQUENCERCORE_API IStretchOperation
{
public:
	virtual ~IStretchOperation() {}

	virtual void DoNotSnapTo(TSharedPtr<FViewModel> Model) = 0;
	virtual bool InitiateStretch(TSharedPtr<FViewModel> Controller, TSharedPtr<IStretchableExtension> Target, int32 Priority, const FStretchParameters& InParams) = 0;
};

} // namespace Sequencer
} // namespace UE

