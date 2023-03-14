// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once
//#include "UObject/Interface.h"

#include "UObject/Object.h"
#include "Templates/SharedPointer.h"
#include "Internationalization/Text.h"
#include "Engine/LocalPlayer.h"
#include "Interactions/SocialInteractionHandle.h"

#include "Internationalization/Internationalization.h"
#include "Internationalization/CulturePointer.h"

class FSocialUserList;
class FChatSlashCommand;
class FInteractionCommandWrapper;
class ULocalPlayer;
class USocialManager;
class USocialChatManager;
class USocialUser;
class USocialToolkit;



///////////////////////////////////////////////////////////////////////////////
struct FAutoCompleteStruct
{
	FAutoCompleteStruct(const FString& InFullString, const TWeakPtr<const FChatSlashCommand>& InCmd, TWeakObjectPtr<USocialUser> InOptionalTargetUser);

	//Cacheing data with strings is tricky because they will become invalid when the user changes their localization
	//but a user can't change this while typing a command, so it is okay in this case.
	FString FullString;
	TWeakPtr<const FChatSlashCommand> SlashCommand;
	TWeakObjectPtr<USocialUser> OptionalTargetUser;
	TArray<FString> Tokens;
};

///////////////////////////////////////////////////////////////////////////////
//Slash Command Component
class PARTY_API FRegisteredSlashCommands : public TSharedFromThis<FRegisteredSlashCommands>
{
public:
	static void TokenizeMessage(const FString& InChatText, TArray<FString>& Tokens);
	static bool TokensExactMatch(TArray<FString>& TokensLHS, TArray<FString>& TokensRHS);
	static bool CmdMatchesFirstToken(const FString& CmdString, const TArray<FString>& Tokens);

	FRegisteredSlashCommands() = default;
	void Init(USocialToolkit& Toolkit);

	/** main entry point for class encapsulated behavior; returns true if command executed */
	bool NotifyUserTextChanged(const FText& InText);

	bool TryExecuteCommandByMatchingText(const FString& UserTypedText);
	bool HasAutoCompleteSuggestions(){ return AutoCompleteData.Num() != 0; }
	const TArray<TSharedPtr<FAutoCompleteStruct>>& GetAutoCompleteStrings() const { return AutoCompleteData; }
	void RegisterCommand(const TSharedPtr<FChatSlashCommand>& NewSlashCommand);
	bool IsEnabled();

private: 
	void PrepareInteractionAutocompleteStrings(const TArray<FString>& StringTokens);
	void HandleCultureChanged() const;

	uint32 LastQueryTextLen = 0;
	bool bValidUsersCached = false;
	bool SpaceWasJustTyped(const FString& NewUserText);

private: 
	TArray<TSharedPtr<FChatSlashCommand>> RegisteredCustomSlashCommands;
	TArray<TSharedPtr<FInteractionCommandWrapper>> RegisteredInteractionSlashCommands;

	//once set, this should always be valid since lifetime of SocialManager is tied to game instance
	TWeakObjectPtr<USocialToolkit> MyToolkit;
	mutable TArray<TSharedPtr<FAutoCompleteStruct>> AutoCompleteData;

	FRegisteredSlashCommands(const FRegisteredSlashCommands& copy) = delete;
	FRegisteredSlashCommands(FRegisteredSlashCommands&& move) = delete;
	FRegisteredSlashCommands& operator=(const FRegisteredSlashCommands& copy) = delete;
	FRegisteredSlashCommands& operator=(FRegisteredSlashCommands&& move) = delete;
};
///////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////////
class PARTY_API FChatSlashCommand : public TSharedFromThis<FChatSlashCommand>
{
public:
	/** @param InCommandName Command name including / prefix. eg "/party".	*/
	explicit FChatSlashCommand(const FText& InCommandName);
	virtual ~FChatSlashCommand() = default;

	virtual void Init(USocialToolkit& InToolkit);
	virtual bool IsEnabled() const = 0;
	virtual void ExecuteSlashCommand(USocialUser* OptionalTargetUser) const = 0;

	virtual void GetAutoCompleteStrings(TArray< TSharedPtr<FAutoCompleteStruct> >& OutStringContainer, const TArray<FString>& UserTextTokens) const;
	virtual bool CanExecuteSpacebarFromPartialTokens(const TArray<FString>& UserTextTokens) const { return false;  }
	virtual bool HasSpacebarExecuteFunctionality() const { return false; }
	virtual bool RequiresUserForExecution() const {return false;}
	virtual void RecacheForLocalization() const;

	const FString& GetCommandNameString() const { return CommandNameString; }

protected:
	USocialToolkit* GetToolkit() const { return MyToolkit.Get(); }
	mutable FString CommandNameString;

private:
	const FText CommandNameTextSrc;
	TWeakObjectPtr<USocialToolkit> MyToolkit;

	FChatSlashCommand(const FChatSlashCommand& copy) = delete;
	FChatSlashCommand(FChatSlashCommand&& move) = delete;
	FChatSlashCommand& operator=(const FChatSlashCommand& copy) = delete;
	FChatSlashCommand& operator=(FChatSlashCommand&& move) = delete;
};

//////////////////////////////////////////////////////////////////////////////////

class PARTY_API FInteractionCommandWrapper: public FChatSlashCommand
{
public:
	/** Interaction tokens will have / prefix appended.*/
	FInteractionCommandWrapper(FSocialInteractionHandle Interaction);

	virtual void ExecuteSlashCommand(USocialUser* OptionalTargetUser) const override; 
	virtual bool IsEnabled() const override	{ return true; 	}
	virtual bool HasSpacebarExecuteFunctionality() const { return true; }
	virtual bool RequiresUserForExecution() const override{ return true; }
	virtual bool CanExecuteSpacebarFromPartialTokens(const TArray<FString>& UserTextTokens) const{return false;	}
	
	void ResetUserCache(); 
	FORCEINLINE void TryCacheValidAutoCompleteUser(USocialUser& User, const TArray<FString>& StringTokens);
	virtual void GetAutoCompleteStrings(TArray< TSharedPtr<FAutoCompleteStruct> >& OutStringContainer, const TArray<FString>& UserTextTokens) const override;
	virtual void RecacheForLocalization() const override;

private:
	FSocialInteractionHandle WrappedInteraction;
	mutable FString CachedCommandToken;
	TArray<TWeakObjectPtr<USocialUser>> CachedValidUsers;

	/* 
	* NOTE: we cannot simply cache the FText long term because localization changes while running will invalidate cache.
	* So, there exists the following function to re-query the localization
	*/
	void CacheStringDataForLocalization() const;
};