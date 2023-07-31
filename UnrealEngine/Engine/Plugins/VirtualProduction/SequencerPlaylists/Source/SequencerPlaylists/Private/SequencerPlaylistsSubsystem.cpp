// Copyright Epic Games, Inc. All Rights Reserved.

#include "SequencerPlaylistsSubsystem.h"
#include "SequencerPlaylist.h"
#include "SequencerPlaylistPlayer.h"
#include "SequencerPlaylistsLog.h"


void USequencerPlaylistsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}


void USequencerPlaylistsSubsystem::Deinitialize()
{
	Super::Deinitialize();

	ensureMsgf(EditorPlaylists.Num() == 0, TEXT("USequencerPlaylistsSubsystem::EditorPlaylists leak"));
	ensureMsgf(EditorPlayers.Num() == 0, TEXT("USequencerPlaylistsSubsystem::EditorPlayers leak"));
	ensureMsgf(EditorPackages.Num() == 0, TEXT("USequencerPlaylistsSubsystem::EditorPackages leak"));
}


USequencerPlaylistPlayer* USequencerPlaylistsSubsystem::CreatePlayerForEditor(TSharedRef<SSequencerPlaylistPanel> Editor)
{
	FSequencerPlaylistEditorHandle EditorHandle(&Editor.Get());

	TObjectPtr<USequencerPlaylistPlayer>* ExistingPlayer = EditorPlayers.Find(EditorHandle);
	if (!ensure(ExistingPlayer == nullptr))
	{
		return *ExistingPlayer;
	}

	ensure(EditorPackages.Find(EditorHandle) == nullptr);
	ensure(EditorPlaylists.Find(EditorHandle) == nullptr);
	ensure(EditorPlayers.Find(EditorHandle) == nullptr);

	USequencerPlaylist* NewPlaylist = CreateTransientPlaylistForEditor(Editor);
	EditorPlaylists.Add(EditorHandle, NewPlaylist);
	EditorPackages.Add(EditorHandle, NewPlaylist->GetPackage());

	USequencerPlaylistPlayer* NewPlayer = NewObject<USequencerPlaylistPlayer>();
	EditorPlayers.Add(EditorHandle, NewPlayer);
	NewPlayer->SetPlaylist(NewPlaylist);

	return NewPlayer;
}


USequencerPlaylist* USequencerPlaylistsSubsystem::CreateTransientPlaylistForEditor(TSharedRef<SSequencerPlaylistPanel> Editor)
{
	FName PackageName = ::MakeUniqueObjectName(nullptr, UPackage::StaticClass(), TEXT("/Engine/Transient/SequencerPlaylist"));
	UPackage* PlaylistPackage = NewObject<UPackage>(nullptr, PackageName, RF_Transient | RF_Transactional);
	//PlaylistPackage->AddToRoot();

	USequencerPlaylist* Playlist = NewObject<USequencerPlaylist>(PlaylistPackage, TEXT("UntitledPlaylist"));
	Playlist->SetFlags(RF_Transactional);

	return Playlist;
}


void USequencerPlaylistsSubsystem::NotifyEditorClosed(SSequencerPlaylistPanel* Editor)
{
	check(Editor);

	FSequencerPlaylistEditorHandle EditorHandle(Editor);

	ensure(EditorPlaylists.Remove(EditorHandle));
	ensure(EditorPlayers.Remove(EditorHandle));
	ensure(EditorPackages.Remove(EditorHandle));
}
