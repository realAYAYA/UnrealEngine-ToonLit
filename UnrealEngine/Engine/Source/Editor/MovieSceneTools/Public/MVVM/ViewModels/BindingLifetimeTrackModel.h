// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVM/Extensions/ISortableExtension.h"
#include "MVVM/ICastable.h"
#include "MVVM/ViewModels/TrackModel.h"
#include "Templates/SharedPointer.h"
#include "MVVM/Extensions/IBindingLifetimeExtension.h"

class UMovieSceneBindingLifetimeTrack;
class UMovieSceneTrack;
namespace UE::Sequencer { template <typename T> struct TAutoRegisterViewModelTypeID; }

namespace UE
{
	namespace Sequencer
	{

		class MOVIESCENETOOLS_API FBindingLifetimeTrackModel
			: public FTrackModel
			, public IBindingLifetimeExtension
		{
		public:

			UE_SEQUENCER_DECLARE_CASTABLE(FBindingLifetimeTrackModel, FTrackModel, IBindingLifetimeExtension);

			static TSharedPtr<FTrackModel> CreateTrackModel(UMovieSceneTrack* Track);

			explicit FBindingLifetimeTrackModel(UMovieSceneBindingLifetimeTrack* Track);

			FSortingKey GetSortingKey() const override;

			void OnDeferredModifyFlush() override;

			// IBindingLifetimeExtension
			const TArray<FFrameNumberRange>& GetInverseLifetimeRange() const override { return InverseLifetimeRange; }
		
		protected:
			virtual void OnConstruct() override;

		private:
			void RecalculateInverseLifetimeRange();

			// The inverse of the range created by all of our binding lifetime sections
			// In other words, the ranges where the object binding should be deactivated.
			TArray<FFrameNumberRange> InverseLifetimeRange;

		};

	} // namespace Sequencer
} // namespace UE

