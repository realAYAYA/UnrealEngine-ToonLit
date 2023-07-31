// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	FeedbackContextEditor.h: Feedback context tailored to UnrealEd

=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Misc/MessageDialog.h"
#include "Misc/FeedbackContext.h"
#include "Widgets/SWindow.h"

class FContextSupplier;
class SBuildProgressWidget;

/**
 * A FFeedbackContext implementation for use in UnrealEd.
 */
class UNREALED_API FFeedbackContextEditor : public FFeedbackContext
{
	/** Slate slow task widget */
	TWeakPtr<class SWindow> SlowTaskWindow;

	/** Special Windows/Widget popup for building */
	TWeakPtr<class SWindow> BuildProgressWindow;
	TSharedPtr<class SBuildProgressWidget> BuildProgressWidget;

	bool HasTaskBeenCancelled;

	double SlowTaskStartTime = 0;

public:

	FFeedbackContextEditor();

	virtual void Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category ) override;

	virtual void StartSlowTask( const FText& Task, bool bShowCancelButton=false ) override;
	virtual void FinalizeSlowTask( ) override;
	virtual void ProgressReported( const float TotalProgressInterp, FText DisplayMessage ) override;
	virtual bool IsPlayingInEditor() const override;

	void SetContext( FContextSupplier* InSupplier ) override {}

	/**
	 * Whether or not the user has canceled out of the progress dialog 
	 * (i.e. the ongoing slow task or the last one that ran).
	 * The user cancel flag is reset when starting a new root slow task.
	 */
	virtual bool ReceivedUserCancel() override;

	void OnUserCancel();

	virtual bool YesNof( const FText& Question ) override
	{
		return EAppReturnType::Yes == FMessageDialog::Open( EAppMsgType::YesNo, Question );
	}

	/** 
	 * Show the Build Progress Window 
	 * @return Handle to the Build Progress Widget created
	 */
	TWeakPtr<class SBuildProgressWidget> ShowBuildProgressWindow() override;
	
	/** Close the Build Progress Window */
	void CloseBuildProgressWindow() override;
};
