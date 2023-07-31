// Copyright Epic Games, Inc. All Rights Reserved.

#include "SectionLayout.h"

#include "Algo/Sort.h"
#include "Layout/Geometry.h"
#include "MVVM/Extensions/IGeometryExtension.h"
#include "MVVM/Extensions/IOutlinerExtension.h"
#include "MVVM/Extensions/LinkedOutlinerExtension.h"
#include "MVVM/ViewModelPtr.h"
#include "MVVM/ViewModels/ChannelModel.h"
#include "MVVM/ViewModels/SectionModel.h"
#include "MVVM/ViewModels/TrackModel.h"
#include "MVVM/ViewModels/ViewModel.h"
#include "MVVM/ViewModels/ViewModelIterators.h"
#include "Math/Vector2D.h"
#include "Misc/AssertionMacros.h"
#include "SequencerCoreFwd.h"
#include "Templates/UnrealTemplate.h"

namespace UE
{
namespace Sequencer
{

bool FSectionLayoutElementKeyFuncs::Matches(const FSectionLayoutElement& A, const FSectionLayoutElement& B)
{
	if (A.GetModel() != B.GetModel())
	{
		return false;
	}
	TArrayView<const TWeakPtr<FChannelModel>> ChannelsA = A.GetChannels(), ChannelsB = B.GetChannels();
	if (ChannelsA.Num() != ChannelsB.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < ChannelsA.Num(); ++Index)
	{
		if (ChannelsA[Index] != ChannelsB[Index])
		{
			return false;
		}
	}
	return true;
}

uint32 FSectionLayoutElementKeyFuncs::GetKeyHash(const FSectionLayoutElement& Key)
{
	uint32 Hash = GetTypeHash(Key.GetModel()) ;
	for (TWeakPtr<FChannelModel> Channel : Key.GetChannels())
	{
		Hash = HashCombine(GetTypeHash(Channel), Hash);
	}
	return Hash;
}

FSectionLayoutElement FSectionLayoutElement::FromGroup(const TSharedPtr<FViewModel>& InGroup, const TSharedPtr<FViewModel>& InChannelRoot, float InOffset, float InHeight)
{
	FSectionLayoutElement Tmp;
	Tmp.Type = Group;

	for (TSharedPtr<FLinkedOutlinerExtension> OutlinerItemExtension: InChannelRoot->GetDescendantsOfType<FLinkedOutlinerExtension>(true /*bIncludeThis*/))
	{
		if (TViewModelPtr<IOutlinerExtension> Outliner = OutlinerItemExtension->GetLinkedOutlinerItem())
		{
			if (Outliner->IsFilteredOut() == false)
			{
				TSharedPtr<FViewModel> Model = OutlinerItemExtension->GetLinkedOutlinerItem().AsModel();
				for (TSharedPtr<FChannelGroupModel> ChannelGroup : Model->GetDescendantsOfType<FChannelGroupModel>())
				{
					TArrayView<const TWeakViewModelPtr<FChannelModel>> Channels  =  ChannelGroup->GetChannels();
					for (const TWeakViewModelPtr < FChannelModel>& Channel : Channels)
					{
						Tmp.WeakChannels.Add(Channel);
					}
				}
			}
		}
	}
	Tmp.LocalOffset = InOffset;
	Tmp.Height = InHeight;
	Tmp.DataModel = InGroup;
	return Tmp;
}

FSectionLayoutElement FSectionLayoutElement::FromChannel(const TSharedPtr<FChannelModel>& InChannel, float InOffset, float InHeight)
{
	FSectionLayoutElement Tmp;
	Tmp.Type = Single;
	Tmp.WeakChannels.Add(InChannel);
	Tmp.LocalOffset = InOffset;
	Tmp.Height = InHeight;
	Tmp.DataModel = InChannel;
	return Tmp;
}

FSectionLayoutElement FSectionLayoutElement::EmptySpace(const TSharedPtr<FViewModel>& InModel, float InOffset, float InHeight)
{
	FSectionLayoutElement Tmp;
	Tmp.Type = Single;
	Tmp.LocalOffset = InOffset;
	Tmp.Height = InHeight;
	Tmp.DataModel = InModel;
	return Tmp;
}

FSectionLayoutElement::EType FSectionLayoutElement::GetType() const
{
	return Type;
}

float FSectionLayoutElement::GetOffset() const
{
	return LocalOffset;
}

float FSectionLayoutElement::GetHeight() const
{
	return Height;
}

TArrayView<const TWeakPtr<FChannelModel>> FSectionLayoutElement::GetChannels() const
{
	return WeakChannels;
}

TSharedPtr<FViewModel> FSectionLayoutElement::GetModel() const
{
	return DataModel.Pin();
}

FGeometry FSectionLayoutElement::ComputeGeometry(const FGeometry& SectionAreaGeometry) const
{
	return SectionAreaGeometry.MakeChild(
		FVector2D(0, LocalOffset),
		FVector2D(SectionAreaGeometry.GetLocalSize().X, Height)
	);
}

FSectionLayout::FSectionLayout(TSharedPtr<UE::Sequencer::FSectionModel> SectionModel)
	: Height(0.f)
{
	using namespace UE::Sequencer;

	float VerticalOffset = 0.f;
	bool bLayoutChildren = true;

	// First layout the parent
	if (TSharedPtr<FTrackModel> ParentTrackModel = StaticCastSharedPtr<FTrackModel>(SectionModel->GetParent()))
	{
		const bool bIsExpanded = ParentTrackModel->IsExpanded();
		const FOutlinerSizing ParentSizing = ParentTrackModel->GetOutlinerSizing();
		VerticalOffset += ParentSizing.PaddingTop;
		const float ParentModelHeight = ParentSizing.Height;

		if (!bIsExpanded)
		{
			Elements.Add(FSectionLayoutElement::FromGroup(ParentTrackModel, SectionModel, VerticalOffset, ParentModelHeight));
		}
		else
		{
			Elements.Add(FSectionLayoutElement::EmptySpace(ParentTrackModel, VerticalOffset, ParentModelHeight));
		}

		VerticalOffset += ParentModelHeight + ParentSizing.PaddingBottom;
		Height = VerticalOffset;

		// Don't layout children if the parent is collapsed or filtered out.
		bLayoutChildren = bIsExpanded && !ParentTrackModel->IsFilteredOut();
	}

	if (!bLayoutChildren)
	{
		// No need to sort out layout elements since there's only one.
		return;
	}

	// Then layout the children
	for (FParentFirstChildIterator ChildIt = SectionModel->GetDescendants(); ChildIt; ++ChildIt)
	{
		TSharedPtr<FViewModel> Model = *ChildIt;

		bLayoutChildren = true;
		FOutlinerSizing ChildSizing;

		if (FLinkedOutlinerExtension* LinkedOutlinerExtension = Model->CastThis<FLinkedOutlinerExtension>())
		{
			TSharedPtr<IOutlinerExtension> LinkedOutlinerItem = LinkedOutlinerExtension->GetLinkedOutlinerItem();
			const bool bIsFilteredOut = !LinkedOutlinerItem || LinkedOutlinerItem->IsFilteredOut();
			if (bIsFilteredOut)
			{
				continue;
			}

			bLayoutChildren = LinkedOutlinerItem->IsExpanded();
			ChildSizing = LinkedOutlinerItem->GetOutlinerSizing();
		}
		else if (IGeometryExtension* GeometryExtension = Model->CastThis<IGeometryExtension>())
		{
			FVirtualGeometry Geometry = GeometryExtension->GetVirtualGeometry();
			ChildSizing = FOutlinerSizing(Geometry.GetHeight());
		}
		else
		{
			ensure(false);
		}

		VerticalOffset += ChildSizing.PaddingTop;
		const float ChildModelHeight = ChildSizing.Height;

		if (TSharedPtr<FChannelModel> Channel = Model->CastThisShared<FChannelModel>())
		{
			Elements.Add(FSectionLayoutElement::FromChannel(Channel, VerticalOffset, ChildModelHeight));
		}
		else if (Model->HasChildren() && !bLayoutChildren)
		{
			Elements.Add(FSectionLayoutElement::FromGroup(Model, Model, VerticalOffset, ChildModelHeight));
		}
		else
		{
			Elements.Add(FSectionLayoutElement::EmptySpace(Model, VerticalOffset, ChildModelHeight));
		}

		VerticalOffset += ChildModelHeight + ChildSizing.PaddingBottom;
		Height = VerticalOffset;

		if (!bLayoutChildren)
		{
			ChildIt.IgnoreCurrentChildren();
		}
	}

	Algo::SortBy(Elements, &FSectionLayoutElement::GetOffset);
}

const TArray<FSectionLayoutElement>& FSectionLayout::GetElements() const
{
	return Elements;
}

float FSectionLayout::GetTotalHeight() const
{
	return Height;
}

} // namespace Sequencer
} //namespace UE
