// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"

class FDMXEditor;
class SDMXFixturePatchFragment;
class SDMXPatchedUniverse;
class UDMXEntityFixturePatch;


/** 
 * A fixture patch in a grid, consists of several fragments, displayed as SDMXFixturePatchNodes (see GenerateWidgets). 
 * The patch node does not necessarily have to present the state of the object, e.g. when
 * dragged or when containing an occuppied universe/channel. 
 */
class FDMXFixturePatchNode
	: public TSharedFromThis<FDMXFixturePatchNode>
{
public:
	/** Creates a new node from Patch */
	static TSharedPtr<FDMXFixturePatchNode> Create(TWeakPtr<FDMXEditor> InDMXEditor, const TWeakObjectPtr<UDMXEntityFixturePatch>& InFixturePatch);

	/** Sets the Addresses of the Node */
	void SetAddresses(int32 UniverseID, int32 NewStartingChannel, int32 NewChannelSpan, bool bTransacted);

	/** Generates widgets. Requires Addresses to be set via SetAddresses */
	TArray<TSharedPtr<SDMXFixturePatchFragment>> GenerateWidgets(const TArray<TSharedPtr<FDMXFixturePatchNode>>& FixturePatchNodeGroup);

	/** Returns widgets */
	const TArray<TSharedPtr<SDMXFixturePatchFragment>>& GetGeneratedWidgets() { return FixturePatchFragmentWidgets; };

	/** Returns wether the patch uses specified channesl */
	bool OccupiesChannels(int32 Channel, int32 Span) const;

	/** Returns true if the node is patched in a universe */
	bool IsPatched() const;

	/** Returns true if the node is selected */
	bool IsSelected() const;

	/** Returns the fixture patches this node holds */
	TWeakObjectPtr<UDMXEntityFixturePatch> GetFixturePatch() const { return FixturePatch; }

	/** Returns the universe this node resides in */
	const TSharedPtr<SDMXPatchedUniverse>& GetUniverseWidget() const { return UniverseWidget; }

	/** Returns the Universe ID the Node currently resides in. Returns a negative Value if not assigned to a Universe. */
	int32 GetUniverseID() const;

	/** Returns the Starting Channel of the Patch */
	int32 GetStartingChannel() const;

	/** Returns the Channel Span of the Patch */
	int32 GetChannelSpan() const;

	/** Sets the ZOrder of the Node */
	void SetZOrder(int32 NewZOrder) { ZOrder = NewZOrder; }

	/** Gets the ZOrder of the Node */
	int32 GetZOrder() const { return ZOrder; }

	/** Returns true if the patch was moved on the grid and new widgets need be generated. */
	bool NeedsUpdateGrid() const;

private:
	/** Called when a patch node was selected */
	void OnSelectionChanged();

	/** Universe the patch is assigned to */
	TSharedPtr<SDMXPatchedUniverse> UniverseWidget;

	/** Cached Universe of the patch */
	int32 UniverseID = 0;

	/** Cached starting channel of the patch */	
	int32 StartingChannel = 0;

	/** Cached channel span of the patch */
	int32 ChannelSpan = 0;

	/** Last transacted Universe ID, required for propert undo/redo */
	int32 LastTransactedUniverseID;

	/** Last transacted Channel ID, required for propert undo/redo */
	int32 LastTransactedChannelID;

	/** If true the node is selected */
	bool bSelected = false;

	/** ZOrder of this node */
	int32 ZOrder = 0;

	/** Keep track of Fragment Widgets, useful to handle selection */
	TArray<TSharedPtr<SDMXFixturePatchFragment>> FixturePatchFragmentWidgets;

	/** The Fixture Patch this Node stands for */
	TWeakObjectPtr<UDMXEntityFixturePatch> FixturePatch;

	/** Weak DMXEditor refrence */
	TWeakPtr<FDMXEditor> WeakDMXEditor;
};
