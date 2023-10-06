// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "XRScribeFileFormat.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/StaticArray.h"
#include "Logging/LogMacros.h"
#include "Math/Transform.h"
#include "OpenXRCore.h"

namespace UE::XRScribe
{

// The PoseManager has two primary jobs:
// * ingest data from capture
// * generate estimated poses based on emulator frametimes
//
// When using the PoseManager, we'll interact with it similarly to xrLocateSpace: give it time and spaces,
// and return locations.
//
// Internally, we'll plan to have all managed poses in tracker-space, and convert to the different spaces
// as needed. This does mean we'll need to know how to convert between different spaces, even if the
// original capture might not tell us how to

class FOpenXRPoseManager
{

public:

	// Number of poses the manager will keep available relative to 'current' pose, in order for multi-frame pose queries
	static const int32 PoseHistorySize = 5;

	/**
	 * Register information about wait frames from a capture file, in order to build a history of
	 * display times
	 *
	 * @param InWaitFrameHistory The WaitFrame packets from the capture
	 *
	 */
	void RegisterCapturedWaitFrames(const TArray<FOpenXRWaitFramePacket>& InWaitFrameHistory);

	/**
	 * Register information about reference spaces from a capture file, in order to build a history of
	 * space locations relative to those reference spaces.
	 *
	 * @param CreateReferenceSpacePackets The create infos for the captured reference spaces
	 *
	 */
	void RegisterCapturedReferenceSpaces(const TArray<FOpenXRCreateReferenceSpacePacket>& CreateReferenceSpacePackets);

	/**
	 * Register information about actions from capture file, in order to fetch action name for mapping between capture and emulation
	 *
	 * @param CreateActionPackets The create infos for the captured actions
	 *
	 */
	void RegisterCapturedActions(const TArray<FOpenXRCreateActionPacket>& CreateActionPackets);


	/**
	 * Register information about action spaces from capture file, which we need to build a history of space locations
	 * for XR devices
	 *
	 * @param CreateActionSpacePackets The create infos for the captured action spaces
	 *
	 */
	void RegisterCapturedActionSpaces(const TArray<FOpenXRCreateActionSpacePacket>& CreateActionSpacePackets);

	/**
	 * Register history of LocateSpace calls
	 *
	 * @param SpaceHistories List of LocateSpace histories, with a unified Space per history (but possibly varying BaseSpace) 
	 *
	 */
	void RegisterCapturedSpaceHistories(const TMap<XrSpace, TArray<FOpenXRLocateSpacePacket>>& SpaceHistories);


	/**
	 * Register history of GetActionState + SyncActions calls
	 *
	 * @param SyncActionsPackets List of SyncActions calls
	 * @param BooleanActionStates Per-action history of boolean action states
	 * @param FloatActionStates Per-action history of float action states
	 * @param VectorActionStates Per-action history of Vector2f action states
	 * @param PoseActionStates Per-action history of Pose action states (indicating whether pose is valid or not, not actual pose data)
	 *
	 */
	void RegisterCapturedActionStates(const TArray<FOpenXRSyncActionsPacket>& SyncActionsPackets, 
		const TMap<XrAction, TArray<FOpenXRGetActionStateBooleanPacket>>& BooleanActionStates,
		const TMap<XrAction, TArray<FOpenXRGetActionStateFloatPacket>>& FloatActionStates,
		const TMap<XrAction, TArray<FOpenXRGetActionStateVector2fPacket>>& VectorActionStates,
		const TMap<XrAction, TArray<FOpenXRGetActionStatePosePacket>>& PoseActionStates);

	/**
	 * Register emulated reference space. Pose manager needs this to match the new emulated reference space against
	 * a captured reference space of similar attributes.
	 *
	 * @param CreateInfo Create info for emulated reference space, in order to match against captured history of similarly configured space
	 * @param SpaceHandle Emulation generated handle for new space
	 *
	 */
	void RegisterEmulatedReferenceSpace(const XrReferenceSpaceCreateInfo& CreateInfo, XrSpace SpaceHandle);

	/**
	 * Register emulated action space. Pose manager needs this to match emulated action space against
	 * a captured action space with matching action name.
	 *
	 * @param ActionName Name from action create info
	 * @param SpaceCreateInfo Create info for emulated action space, in order to match against captured history of similarly configured space
	 * @param SpaceHandle Emulation generated handle for new space
	 *
	 */
	void RegisterEmulatedActionSpace(TStaticArray<ANSICHAR, XR_MAX_ACTION_NAME_SIZE>& ActionName, const XrActionSpaceCreateInfo& SpaceCreateInfo, XrSpace SpaceHandle);

	/**
	 * Add time and frame number in order to generate interpolated poses
	 * We use the frame numbers to index into a relative history of the poses, and the time
	 * for frame matching + interpolation, because LocateSpace calls only provide a time argument
	 *
	 * @param Time Emulation generated time for new frame
	 * @param FrameNum Emulation generated frame index
	 *
	 */
	void AddEmulatedFrameTime(XrTime Time, int32 FrameNum);

	/**
	 * If we have a pose history for the action, then xrGetActionStatePose can
	 * tell the client that the action is active, and poses are ready to fetch
	 *
	 * @param ActionName Name from action create info
	 *
	 */
	bool DoesActionContainPoseHistory(TStaticArray<ANSICHAR, XR_MAX_ACTION_NAME_SIZE>& ActionName);

	/**
	 * Fetch a pose generated from the captured history, by using space handles and times generated by the emulated runtime
	 *
	 * @param LocatingSpace Generated pose is associated with this space
	 * @param BaseSpace Generated pose is relative to this space
	 * @param Time Time of requested pose, relative to the known times from the emulated runtime
	 *
	 * return Location derived from captured history if available, or default location if request is unknown or invalid
	 */
	XrSpaceLocation GetEmulatedPoseForTime(XrSpace LocatingSpace, XrSpace BaseSpace, XrTime Time);

	/**
	 * Reset internal emulation state based on session teardown.
	 *
	 */
	void OnSessionTeardown()
	{
		EmulatedBaseTime = INT64_MAX;
	}
	
	// TODO: add LocateViews to pose manager tracking

private:

	/**
	 * Helper function to fetch underlying reference space type from opaque XrSpace handle
	 */
	XrReferenceSpaceType GetCapturedReferenceSpaceType(XrSpace CapturedSpace);

	bool VerifyCapturedActionSpace(XrSpace CapturedSpace);

	/**
	 * sort and remove duplicates in history for single space
	 */
	bool FilterSpaceHistory(const TArray<FOpenXRLocateSpacePacket>& RawHistory, TArray<FOpenXRLocateSpacePacket>& FilteredHistory);

	/**
	 * Resample the filtered history in order to ease reading poses back based on time offsets during emulation
	 */
	TArray<FOpenXRLocateSpacePacket> SliceFilteredSpaceHistory(const TArray<FOpenXRLocateSpacePacket>& FilteredHistory);

	void ProcessCapturedReferenceSpaceHistory(const TArray<FOpenXRLocateSpacePacket>& SpaceHistory);
	void ProcessCapturedActionSpaceHistory(const TArray<FOpenXRLocateSpacePacket>& SpaceHistory);

	// Known spaces and relevant actions from capture
	TArray<FOpenXRCreateReferenceSpacePacket> CapturedReferenceSpaces;
	TArray<FOpenXRCreateActionSpacePacket> CapturedActionSpaces;
	TMap<XrAction, FOpenXRCreateActionPacket> CapturedActions;

	/**
	 * Pose histories for space types, which could be unified across different space handles
	 * with the same underlying reference space type.
	 */
	TMap<XrReferenceSpaceType, TArray<FOpenXRLocateSpacePacket>> ReferencePoseHistories;
	TMap<FName, TArray<FOpenXRLocateSpacePacket>> ActionPoseHistories;

	TMap<FName, TArray<FOpenXRGetActionStateBooleanPacket>> BooleanActionStateHistories;
	TMap<FName, TArray<FOpenXRGetActionStateFloatPacket>> FloatActionStateHistories;
	TMap<FName, TArray<FOpenXRGetActionStateVector2fPacket>> VectorActionStateHistories;
	TMap<FName, TArray<FOpenXRGetActionStatePosePacket>> PoseActionStateHistories;

	// Map of emulated spaces to their underlying reference space type (if applicable)
	TMap<XrSpace, XrReferenceSpaceType> EmulatedReferenceSpaceTypeMap;

	TMap< XrSpace, FName> EmulatedActionSpaceNameMap;

	struct FrameTimeHistoryEntry
	{
		XrTime Time = 0;
		int32 FrameIndex = -1;
	};

	// History of frame times, in order for pose manager to look into to estimate the relative frame index for times
	TStaticArray <FrameTimeHistoryEntry, PoseHistorySize> FrameTimeHistory;
	int32 LastInsertedFrameIndex = 0;

	int64 CapturedSliceCount = 0;
	XrTime WaitFrameStart = 0;
	XrTime WaitFrameEnd = 0;
	TArray<FOpenXRWaitFramePacket> WaitFrameHistory;

	XrTime EmulatedBaseTime = INT64_MAX;
};

} // namespace UE::XRScribe
