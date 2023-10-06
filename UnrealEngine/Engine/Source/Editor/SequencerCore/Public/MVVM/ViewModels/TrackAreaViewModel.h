// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreTypes.h"
#include "MVVM/ICastable.h"
#include "MVVM/ViewModelTypeID.h"
#include "MVVM/ViewModels/ViewModel.h"
#include "Math/Range.h"
#include "Templates/SharedPointer.h"
#include "TimeToPixel.h"
#include "UObject/NameTypes.h"

struct FFrameNumber;
struct FFrameRate;
struct FGeometry;

namespace UE
{
namespace Sequencer
{

class FEditorViewModel;
class ISequencerEditTool;
struct ITrackAreaHotspot;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnTrackAreaHotspotChanged, TSharedPtr<ITrackAreaHotspot>);

class SEQUENCERCORE_API FTrackAreaViewModel
	: public FViewModel
{
public:

	UE_SEQUENCER_DECLARE_CASTABLE(FTrackAreaViewModel, FViewModel);

	FTrackAreaViewModel();
	virtual ~FTrackAreaViewModel();

public:

	/** Get the editor view-model provided by the creation context */
	TSharedPtr<FEditorViewModel> GetEditor() const;

	/** Create a time/pixel converter for the given geometry */
	FTimeToPixel GetTimeToPixel(float TrackAreaWidth) const;

	/** Gets the current tick resolution of the editor */
	virtual FFrameRate GetTickResolution() const;
	/** Gets the current view range of this track area */
	virtual TRange<double> GetViewRange() const;

	/** Get the current active hotspot */
	TSharedPtr<ITrackAreaHotspot> GetHotspot() const;

	/** Set the hotspot to something else */
	void SetHotspot(TSharedPtr<ITrackAreaHotspot> NewHotspot);
	void AddHotspot(TSharedPtr<ITrackAreaHotspot> NewHotspot);
	void RemoveHotspot(FViewModelTypeID Type);
	void ClearHotspots();

	/** Set whether the hotspot is locked and cannot be changed (ie when a menu is open) */
	void LockHotspot(bool bIsLocked);

	/** Get the callback that is fired when the current hotspot changes */
	FOnTrackAreaHotspotChanged& GetOnHotspotChangedDelegate();

	void AddEditTool(TSharedPtr<ISequencerEditTool> InNewTool);

	/** Access the currently active track area edit tool */
	ISequencerEditTool* GetEditTool() const { return EditTool.IsValid() ? EditTool.Get() : nullptr; }

	/** Check whether it's possible to activate the specified tool */
	bool CanActivateEditTool(FName Identifier) const;

	bool AttemptToActivateTool(FName Identifier);

	void LockEditTool();
	void UnlockEditTool();

protected:

	/** The current hotspot that can be set from anywhere to initiate drags */
	TArray<TSharedPtr<ITrackAreaHotspot>> HotspotStack;

	/** The delegate that is fired when the current hotspot changes */
	FOnTrackAreaHotspotChanged OnHotspotChangedDelegate;

	/** The currently active edit tools on this track area */
	TArray<TSharedPtr<ISequencerEditTool>> EditTools;

	/** The currently active edit tool on this track area */
	TSharedPtr<ISequencerEditTool> EditTool;

	/** When true, prevents any other hotspot being activated */
	bool bHotspotLocked;

	/** When true, prevents any other edit tool being activated by way of CanActivateEditTool always returning true */
	bool bEditToolLocked;
};

} // namespace Sequencer
} // namespace UE

