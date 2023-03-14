// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaAssetsPrivate.h"

#include "CoreMinimal.h"
#include "IMediaAssetsModule.h"
#include "Misc/CoreMisc.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"

#include "MediaPlayer.h"


DEFINE_LOG_CATEGORY(LogMediaAssets);


/**
 * Implements the MediaAssets module.
 */
class FMediaAssetsModule
	: public FSelfRegisteringExec
	, public IMediaAssetsModule
{
public:

	//~ FSelfRegisteringExec interface

	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
	{
		if (FParse::Command(&Cmd, TEXT("MEDIA")))
		{
			FString MovieCmd = FParse::Token(Cmd, 0);

			if (MovieCmd.Contains(TEXT("PLAY")))
			{
				for (TObjectIterator<UMediaPlayer> It; It; ++It)
				{
					UMediaPlayer* MediaPlayer = *It;
					MediaPlayer->Play();
				}
			}
			else if (MovieCmd.Contains(TEXT("PAUSE")))
			{
				for (TObjectIterator<UMediaPlayer> It; It; ++It)
				{
					UMediaPlayer* MediaPlayer = *It;
					MediaPlayer->Pause();
				}
			}

			return true;
		}

		return false;
	}

public:
	//~ IMediaAssetsModule interface
	virtual int32 RegisterGetPlayerFromObject(const FOnGetPlayerFromObject& Delegate) override
	{
		int32 Index = GetPlayerFromObjectDelegates.Num();
		GetPlayerFromObjectDelegates.Add(Delegate);
		return Index;
	}

	virtual void UnregisterGetPlayerFromObject(int32 DelegateID) override
	{
		GetPlayerFromObjectDelegates[DelegateID].Unbind();
	}

	virtual UMediaPlayer* GetPlayerFromObject(UObject* Object, UObject*& PlayerProxy) override
	{
		UMediaPlayer* Player = nullptr;
		PlayerProxy = nullptr;

		// Go through all our delegates.
		for (FOnGetPlayerFromObject& Delegate : GetPlayerFromObjectDelegates)
		{
			if (Delegate.IsBound())
			{
				Player = Delegate.Execute(Object, PlayerProxy);
				if (Player != nullptr)
				{
					break;
				}
			}
		}

		return Player;
	}
	
	//~ IModuleInterface interface

	virtual void StartupModule() override { }
	virtual void ShutdownModule() override { }

	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}

private:
	/** All the functions that can get a player from an object. */
	TArray<FOnGetPlayerFromObject> GetPlayerFromObjectDelegates;

};


IMPLEMENT_MODULE(FMediaAssetsModule, MediaAssets);
