// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "ISourceControlChangelistState.h"
#include "ISourceControlOperation.h"
#include "ISourceControlProvider.h"
#include "ISourceControlState.h"
#include "Internationalization/Text.h"
#include "Misc/DateTime.h"
#include "Misc/EnumClassFlags.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "UncontrolledChangelist.h"

class FJsonObject;

class UNCONTROLLEDCHANGELISTS_API FUncontrolledChangelistState : public TSharedFromThis<FUncontrolledChangelistState, ESPMode::ThreadSafe>
{
public:
	static constexpr const TCHAR* FILES_NAME = TEXT("files");
	static constexpr const TCHAR* NAME_NAME = TEXT("name");
	static constexpr const TCHAR* DESCRIPTION_NAME = TEXT("description");
	static const FText DEFAULT_UNCONTROLLED_CHANGELIST_DESCRIPTION;

	enum class ECheckFlags
	{
		/** No Check */
		None			= 0,

		/** File has been modified */
		Modified		= 1,

		/** File is not checked out */
		NotCheckedOut	= 1 << 1,

		/** All the above checks */
		All = Modified | NotCheckedOut,
	};

public:
	// An FUncontrolledChangelistState should always reference a given Changelist (with a valid GUID).
	FUncontrolledChangelistState() = delete;

	FUncontrolledChangelistState(const FUncontrolledChangelist& InUncontrolledChangelist);
	
	FUncontrolledChangelistState(const FUncontrolledChangelist& InUncontrolledChangelist, const FText& InDescription);

	// Uncontrolled Changelist states are unique and non-copyable, should always be used by reference to preserve cache coherence.
	FUncontrolledChangelistState(const FUncontrolledChangelistState& InUncontrolledChangelistState) = delete;
	FUncontrolledChangelistState& operator=(const FUncontrolledChangelistState& InUncontrolledChangelistState) = delete;

	/**
	 * Get the name of the icon graphic we should use to display the state in a UI.
	 * @returns the name of the icon to display
	 */
	 FName GetIconName() const;

	/**
	 * Get the name of the small icon graphic we should use to display the state in a UI.
	 * @returns the name of the icon to display
	 */
	FName GetSmallIconName() const;

	/**
	 * Get a text representation of the state
	 * @returns	the text to display for this state
	 */
	const FText& GetDisplayText() const;

	/**
	 * Get a text representation of the state
	 * @returns	the text to display for this state
	 */
	const FText& GetDescriptionText() const;

	/**
	 * Get a tooltip to describe this state
	 * @returns	the text to display for this states tooltip
	 */
	FText GetDisplayTooltip() const;

	/**
	 * Get the timestamp of the last update that was made to this state.
	 * @returns	the timestamp of the last update
	 */
	const FDateTime& GetTimeStamp() const;

	const TSet<FSourceControlStateRef>& GetFilesStates() const;
	
	const TSet<FString>& GetOfflineFiles() const;

	const TSet<FString>& GetDeletedOfflineFiles() const;

	/**
	 * Get the number of files in the CL. (Includes file states and offline files) 
	 */
	int32 GetFileCount() const;

	/**
	 * Get the filenames in the CL. (Includes file states and offline files)
	 */
	TArray<FString> GetFilenames() const;

	/**
	 * Check whether a file exists in the file states or offline files
	 */
	bool ContainsFilename(const FString& PackageFilename) const;

	/**
	 * Serialize the state of the Uncontrolled Changelist to a Json Object.
	 * @param 	OutJsonObject 	The Json object used to serialize.
	 */
	void Serialize(TSharedRef<class FJsonObject> OutJsonObject) const;

	/**
	 * Deserialize the state of the Uncontrolled Changelist from a Json Object.
	 * @param 	InJsonValue 	The Json Object to read from.
	 * @return 	True if Deserialization succeeded.
	 */
	bool Deserialize(const TSharedRef<FJsonObject> InJsonValue);

	/**
	 * Adds files to this Uncontrolled Changelist State.
	 * @param 	InFilenames		The files to be added.
	 * @param 	InCheckFlags 	Tells which checks have to pass to add a file.
	 * @return 	True if a change has been performed in the Uncontrolled Changelist State.
	 */
	bool AddFiles(const TArray<FString>& InFilenames, const ECheckFlags InCheckFlags);

	/**
	 * Removes files from this Uncontrolled Changelist State if present.
	 * @param 	InFileStates 	The files to be removed.
	 * @return 	True if a change has been performed in the Uncontrolled Changelist State.
	 */
	bool RemoveFiles(const TArray<FSourceControlStateRef>& InFileStates);

	/**
	 * Updates the status of all files contained in this changelist.
	 * @return 	True if the state has been modified.
	 */
	bool UpdateStatus();

	/**
	 * Removes files present both in the Uncontrolled Changelist and the provided set.
	 * @param 	InOutAddedAssets 	A Set representing Added Assets to check against.
	 */
	void RemoveDuplicates(TSet<FString>& InOutAddedAssets);

	/**
	 * Sets a new description for this Uncontrolled Changelist
	 * @param	InDescription	The new description to set.
	 */
	void SetDescription(const FText& InDescription);

	/** Returns true if the Uncontrolled Changelists contains either Files or OfflineFiles.	*/
	bool ContainsFiles() const;

public:
	FUncontrolledChangelist Changelist;
	FText Description;
	TSet<FSourceControlStateRef> Files;
	TSet<FString> OfflineFiles;
	TSet<FString> DeletedOfflineFiles;

	/** The timestamp of the last update */
	FDateTime TimeStamp;
};

ENUM_CLASS_FLAGS(FUncontrolledChangelistState::ECheckFlags);
typedef TSharedPtr<FUncontrolledChangelistState, ESPMode::ThreadSafe> FUncontrolledChangelistStatePtr;
typedef TSharedRef<FUncontrolledChangelistState, ESPMode::ThreadSafe> FUncontrolledChangelistStateRef;
