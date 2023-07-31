// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/Map.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Templates/SharedPointer.h"
#include "Templates/TypeHash.h"

struct FGeometry;

namespace UE
{
namespace Sequencer
{

class FChannelModel;
class FSectionModel;
class FViewModel;

/** A layout element specifying the geometry required to render a key area */
struct FSectionLayoutElement
{
	/** Whether this layout element pertains to one or multiple key areas */
	enum EType { Single, Group };

	/** Construct this element from a grouped key area */
	static FSectionLayoutElement FromGroup(const TSharedPtr<FViewModel>& InGroup, const TSharedPtr<FViewModel>& InChannelRoot, float InOffset, float InHeight);

	/** Construct this element from a single Key area node */
	static FSectionLayoutElement FromChannel(const TSharedPtr<FChannelModel>& InChannel, float InOffset, float InHeight);

	/** Construct this element from a single Key area node */
	static FSectionLayoutElement EmptySpace(const TSharedPtr<FViewModel>& InNode, float InOffset, float InHeight);

	/** Retrieve the type of this layout element */
	EType GetType() const;

	/** Retrieve vertical offset from the top of this element's parent */
	float GetOffset() const;

	/** Retrieve the desired height of this element based on the specified parent geometry */
	float GetHeight() const;

	/** Access all the key areas that this layout element represents */
	TArrayView<const TWeakPtr<FChannelModel>> GetChannels() const;

	/** Access the display node that this layout element was generated for */
	TSharedPtr<FViewModel> GetModel() const;

	/** Computes the geometry for this layout as a child of the specified section area geometry */
	FGeometry ComputeGeometry(const FGeometry& SectionAreaGeometry) const;

private:

	/** Pointer to the key area that we were generated from */
	TArray<TWeakPtr<FChannelModel>, TInlineAllocator<1>> WeakChannels;

	/** The specific node that this key area relates to */
	TWeakPtr<FViewModel> DataModel;

	/** The type of this layout element */
	EType Type;

	/** The vertical offset from the top of the element's parent */
	float LocalOffset;

	/** Explicit height of the layout element */
	float Height;
};

/** Class used for generating, and caching the layout geometry for a given display node's key areas */
class FSectionLayout
{
public:

	/** Constructor that takes a display node, and the index of the section to layout */
	FSectionLayout(TSharedPtr<UE::Sequencer::FSectionModel> SectionModel);

	/** Get all layout elements that we generated */
	const TArray<FSectionLayoutElement>& GetElements() const;
	
	/** Get the desired total height of this layout */
	float GetTotalHeight() const;

private:
	/** Array of layout elements that we generated */
	TArray<FSectionLayoutElement> Elements;

	float Height;
};

/** Key funcs for using a section layout element as a key. Intentionally not supported implicitly due to performance reasons. */
struct FSectionLayoutElementKeyFuncs
{
	template<typename T>
	static const FSectionLayoutElement& GetSetKey(const TPair<FSectionLayoutElement, T>& Element)
	{
		return Element.Key;
	}

	static bool Matches(const FSectionLayoutElement& A, const FSectionLayoutElement& B);
	static uint32 GetKeyHash(const FSectionLayoutElement& Key);
};

} // namespace Sequencer
} // namespace UE

