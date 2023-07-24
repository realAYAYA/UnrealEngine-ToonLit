// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/StringFwd.h"
#include "Containers/UnrealString.h"
#include "CoreGlobals.h"
#include "CoreTypes.h"
#include "HAL/CriticalSection.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Text.h"
#include "Misc/OutputDevice.h"
#include "Misc/ScopeLock.h"
#include "Misc/SlowTask.h"
#include "Misc/SlowTaskStack.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#include "Templates/UnrealTemplate.h"

class FContextSupplier;
class SBuildProgressWidget;
struct FSlowTask;

/** A context for displaying modal warning messages. */
class CORE_API FFeedbackContext
	: public FOutputDevice
{
public:
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override;
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category, double Time) override;
	virtual void SerializeRecord(const UE::FLogRecord& Record) override;

	/** Ask the user a binary question, returning their answer */
	virtual bool YesNof( const FText& Question );
	
	/**
	 * Whether or not the user has canceled out of the progress dialog
	 * (i.e. the ongoing slow task or the last one that ran).
	 * The user cancel flag is reset when starting a new root slow task.
	 */
	virtual bool ReceivedUserCancel() { return false; }
	
	/** Public const access to the current state of the scope stack */
	FORCEINLINE const FSlowTaskStack& GetScopeStack() const
	{
		return ScopeStack;
	}

	/**** Legacy API - not deprecated as it's still in heavy use, but superceded by FScopedSlowTask ****/
	void BeginSlowTask( const FText& Task, bool ShowProgressDialog, bool bShowCancelButton=false );
	void UpdateProgress( int32 Numerator, int32 Denominator );
	void StatusUpdate( int32 Numerator, int32 Denominator, const FText& StatusText );
	void StatusForceUpdate( int32 Numerator, int32 Denominator, const FText& StatusText );
	void EndSlowTask();
	/**** end legacy API ****/

protected:

	/**
	 * Called to create a slow task
	 */
	virtual void StartSlowTask( const FText& Task, bool bShowCancelButton=false )
	{
		GIsSlowTask = true;
	}

	/**
	 * Called to destroy a slow task
	 */
	virtual void FinalizeSlowTask( )
	{
		GIsSlowTask = false;
	}

	/**
	 * Called when some progress has occurred
	 * @param	TotalProgressInterp		[0..1] Value indicating the total progress of the slow task
	 * @param	DisplayMessage			The message to display on the slow task
	 */
	virtual void ProgressReported( const float TotalProgressInterp, FText DisplayMessage ) {}

	/** Called to check whether we are playing in editor when starting a slow task */
	virtual bool IsPlayingInEditor() const;

	void FormatLine(FStringBuilderBase& Out, const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category, double Time, ELogVerbosity::Type* OutVerbosity = nullptr) const;
	void FormatRecordLine(FStringBuilderBase& Out, const UE::FLogRecord& Record, ELogVerbosity::Type* OutVerbosity = nullptr) const;

public:
	virtual FContextSupplier* GetContext() const { return nullptr; }
	virtual void SetContext(FContextSupplier* InContext) {}

	/** Shows/Closes Special Build Progress dialogs */
	virtual TWeakPtr<class SBuildProgressWidget> ShowBuildProgressWindow() {return TWeakPtr<class SBuildProgressWidget>();}
	virtual void CloseBuildProgressWindow() {}

	bool	TreatWarningsAsErrors = false;

	FFeedbackContext();
	virtual ~FFeedbackContext();

	/** Gets warnings history */
	void GetWarnings(TArray<FString>& OutWarnings) const
	{
		FScopeLock WarningsAndErrorsLock(&WarningsAndErrorsCritical);
		OutWarnings = Warnings;
	}
	int32 GetNumWarnings() const
	{
		return Warnings.Num();
	}

	/** Gets errors history */
	void GetErrors(TArray<FString>& OutErrors) const
	{
		FScopeLock WarningsAndErrorsLock(&WarningsAndErrorsCritical);
		OutErrors = Errors;
	}
	int32 GetNumErrors() const
	{
		return Errors.Num();
	}

	/** Gets all errors and warnings and clears the history */
	void GetErrorsAndWarningsAndEmpty(TArray<FString>& OutWarningsAndErrors)
	{
		FScopeLock WarningsAndErrorsLock(&WarningsAndErrorsCritical);
		OutWarningsAndErrors = MoveTemp(Errors);
		OutWarningsAndErrors += MoveTemp(Warnings);
	}
	/** Clears all history */
	void ClearWarningsAndErrors()
	{
		FScopeLock WarningsAndErrorsLock(&WarningsAndErrorsCritical);
		Errors.Empty();
		Warnings.Empty();
	}

private:
	void AddToHistory(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category, double Time);
	void AddRecordToHistory(const UE::FLogRecord& Record);

	FFeedbackContext(const FFeedbackContext&);
	FFeedbackContext& operator=(const FFeedbackContext&);

	/** Warnings history */
	TArray<FString> Warnings;
	/** Errors history */
	TArray<FString> Errors;
	/** Guard for the errors and warnings history */
	mutable FCriticalSection WarningsAndErrorsCritical;

protected:
	
	friend FSlowTask;

	/** Stack of pointers to feedback scopes that are currently open */
	FSlowTaskStack ScopeStack;

	/**
	 * Points to the ScopeStack above when initialized - this is because Slate wants a TSharedPtr,
	 * but we don't want to allocate
	 */
	mutable TSharedPtr<FSlowTaskStack> ScopeStackSharedPtr;

	const TSharedPtr<FSlowTaskStack>& GetScopeStackSharedPtr() const
	{
		if (!ScopeStackSharedPtr)
		{
			ScopeStackSharedPtr = MakeShareable(const_cast<FSlowTaskStack*>(&ScopeStack), [](FSlowTaskStack*){});
		}
		return ScopeStackSharedPtr;
	}

	TArray<TUniquePtr<FSlowTask>> LegacyAPIScopes;

	/** Ask that the UI be updated as a result of the scope stack changing */
	void RequestUpdateUI(bool bForceUpdate = false);

	/** Update the UI as a result of the scope stack changing */
	void UpdateUI();

	/**
	 * Adds a new warning message to warnings history.
	 * @param InWarning Warning message
	 */
	void AddWarning(const FString& InWarning)
	{
		FScopeLock WarningsAndErrorsLock(&WarningsAndErrorsCritical);
		Warnings.Add(InWarning);
	}
	void AddWarning(FString&& InWarning)
	{
		FScopeLock WarningsAndErrorsLock(&WarningsAndErrorsCritical);
		Warnings.Add(MoveTemp(InWarning));
	}

	/**
	* Adds a new error message to errors history.
	* @param InWarning Error message
	*/
	void AddError(const FString& InError)
	{
		FScopeLock WarningsAndErrorsLock(&WarningsAndErrorsCritical);
		Errors.Add(InError);
	}
	void AddError(FString&& InError)
	{
		FScopeLock WarningsAndErrorsLock(&WarningsAndErrorsCritical);
		Errors.Add(MoveTemp(InError));
	}
};
