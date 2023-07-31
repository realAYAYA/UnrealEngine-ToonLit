// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SequencerCoreFwd.h"
#include "MVVM/ViewModelTypeID.h"
#include "Curves/KeyHandle.h"

struct FGuid;

class IKeyArea;
class UMovieSceneSection;

namespace UE
{
namespace Sequencer
{

/** Structure defining a point to snap to in the sequencer */
struct SEQUENCER_API FSnapPoint
{
	enum ESnapType { Key, SectionBounds, CustomSection, PlaybackRange, CurrentTime, InOutRange, Mark };

	FSnapPoint(ESnapType InType, FFrameNumber InTime, float InWeighting = -1.f)
		: Time(InTime)
		, Weighting(InWeighting)
		, Type(InType)
	{
		if (InWeighting == -1.f)
		{
			if (Type != UE::Sequencer::FSnapPoint::Key)
			{
				Weighting = 10.f;
			}
			else
			{
				Weighting = 1.f;
			}
		}
	}

	/** The time of the snap */
	FFrameNumber Time;

	float Weighting = 1.f;

	/** The type of snap */
	ESnapType Type;
};

class SEQUENCER_API ISnapField
{
public:
	virtual ~ISnapField() {}

	virtual void AddSnapPoint(const FSnapPoint& SnapPoint) = 0;
};

/** Interface that defines how to construct an FSequencerSnapField */
struct SEQUENCER_API ISnapCandidate
{
	virtual ~ISnapCandidate() { }

	/** Return true to include the specified key in the snap field */
	virtual bool IsKeyApplicable(FKeyHandle KeyHandle, const UE::Sequencer::FViewModelPtr& Owner) const { return true; }

	/** Return true to include the specified section's bounds in the snap field */
	virtual bool AreSectionBoundsApplicable(UMovieSceneSection* Section) const { return true; }

	/** Return true to include the specified section's custom snap points in the snap field */
	virtual bool AreSectionCustomSnapsApplicable(UMovieSceneSection* Section) const { return true; }
};

class SEQUENCER_API ISnappableExtension
{
public:

	UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(ISnappableExtension)

	virtual ~ISnappableExtension(){}

	virtual void AddToSnapField(const ISnapCandidate& Candidate, ISnapField& SnapField) const = 0;
};

} // namespace Sequencer
} // namespace UE

