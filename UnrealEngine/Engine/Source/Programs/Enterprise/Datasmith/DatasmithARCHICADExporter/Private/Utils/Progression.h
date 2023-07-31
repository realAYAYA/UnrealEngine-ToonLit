// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AddonTools.h"

#include "Condition.hpp"
#include "Lock.hpp"
#include "../ResourcesIDs.h"

BEGIN_NAMESPACE_UE_AC

enum EPhaseStrId
{
	kCommonPhaseInvalid = 0,
	kCommonProjectInfos,
	kCommonCollectElements,
	kCommonConvertElements,
	kCommonCollectMetaDatas,
	kCommonSetUpMaterials,
	kCommonSetUpLights,
	kCommonSetUpCameras,
	kSyncWaitPreviousSync,

	kDebugSaveScene,

	kSyncSnapshot,
	kExportSaving,

	kSyncTitle,
	kExportTitle,

	kNbPhases = kCommonSetUpCameras - kCommonPhaseInvalid + 1
};

// Class for progression window, make difficult to forget closing it.
class FProgression
{
  public:
	enum EMode
	{
		kNoCancel = 0,
		kThrowOnCancel,
		kSetFlags
	};

	class AutoCancelMode
	{
	  public:
		AutoCancelMode(FProgression* IOProgression, EMode InMode)
			: Progression(IOProgression)
			, PreviousMode(IOProgression ? IOProgression->SetCancelMode(InMode) : kNoCancel)
		{
		}

		~AutoCancelMode()
		{
			if (Progression)
			{
				Progression->SetCancelMode(PreviousMode);
			}
		}

		FProgression* Progression;
		EMode		  PreviousMode;
	};

	// Constructor (SetUp progression window)
	FProgression(int InResID, EPhaseStrId InTitle, short InNbPhases, EMode InCancelMode,
				 volatile bool* OutUserCancelled);

	// Destructor (Automatically close the progression window)
	~FProgression();

	// Set new cancel mode
	EMode SetCancelMode(EMode InNewCancelMode);

	// Start the next phase
	void NewPhase(EPhaseStrId InPhaseId, int InMaxValue = 0);

	void NewPhaseX(EPhaseStrId InPhaseId, int InMaxValue = 0);

	// Advance progression bar for the current value
	void NewCurrentValue(int InCurrentValue = -1);

	void NewCurrentValueX(int InCurrentValue = -1);

	// Call from the main thread (Usualy when Join wait)
	void Update();

	// Test if user cancelled the operation
	void CheckForCancel();

	// Return true if user cancelled the current phase
	bool IsUserCancelled() const { return bUserCancelled ? *bUserCancelled : false; }

	// Throw an exception if user cancelled current operation
	void ThrowIfUserCancelled() const;

  private:
	// Illegal to copy an FProgression
	FProgression& operator=(const FProgression&) { return *this; }

	// Return the string specifien by the index
	GS::UniString GetText(int InIndex);

	// Return time value in sec with hi resolution
	double GetHiResTime();

	// The string ressource id
	int ResID;

	// The current phase counter
	EPhaseStrId CurrentPhase;

	double Ratio100 = 100.0;

	// The current item processed
	int CurrentValue;

	// The last item update time (Used to not refresh screen too often)
	double LastUpdate;

	// The inverse of the frequecy, used to compute
	double InvFreq;

	// True if we initalize without error
	bool bProgressionShown;

	// The error code value if we got one
	GSErrCode ErrorCode;

	// The task when we got an error message
	const char* ErrorMgs;

	// The cancel mode
	EMode CancelMode;

	// When user cancel, we set this flag to true
	volatile bool* bUserCancelled;

	EPhaseStrId NextPhase;
	int			NextPhaseMaxValue;
	int			NextCurrentValue;

	// Control access on this object (for queue operations)
	GS::Lock AccessControl;

	// Condition variable
	GS::Condition mCV;
};

END_NAMESPACE_UE_AC
