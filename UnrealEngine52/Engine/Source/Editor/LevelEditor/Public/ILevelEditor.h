// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Editor/UnrealEdTypes.h"
#include "Elements/Framework/TypedElementHandle.h"
#include "Elements/Framework/EngineElementsLibrary.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Commands/UICommandList.h"
#include "AssetThumbnail.h"
#include "Toolkits/IToolkitHost.h"

class FDetailsViewObjectFilter;
class IDetailRootObjectCustomization;
class ISceneOutliner;
class IAssetViewport;
class SLevelViewport;
class UTypedElementSelectionSet;

/**
 * Public interface to SLevelEditor
 */
class ILevelEditor : public SCompoundWidget, public IToolkitHost
{

public:

	/** Get the element selection set used by this level editor */
	virtual const UTypedElementSelectionSet* GetElementSelectionSet() const = 0;
	virtual UTypedElementSelectionSet* GetMutableElementSelectionSet() = 0;

	/** Summons a context menu for this level editor at the mouse cursor's location */
	virtual void SummonLevelViewportContextMenu(const FTypedElementHandle& HitProxyElement = FTypedElementHandle()) = 0;

	UE_DEPRECATED(5.0, "Use the SummonLevelViewportContextMenu overload which takes a FTypedElementHandle.")
	void SummonLevelViewportContextMenu(AActor* HitProxyActor)
	{
		FTypedElementHandle HitProxyElement;
		if (HitProxyActor)
		{
			HitProxyElement = UEngineElementsLibrary::AcquireEditorActorElementHandle(HitProxyActor);
		}
		SummonLevelViewportContextMenu(HitProxyElement);
	}

	/** Gets the title for the context menu for this level editor */
	virtual FText GetLevelViewportContextMenuTitle() const = 0;

	/** Summons a context menu for view options */
	virtual void SummonLevelViewportViewOptionMenu(ELevelViewportType ViewOption) = 0;

	/** Returns a list of all of the toolkits that are currently hosted by this toolkit host */
	virtual const TArray< TSharedPtr< IToolkit > >& GetHostedToolkits() const = 0;

	/** Gets an array of all viewports in this level editor */
	virtual TArray< TSharedPtr< SLevelViewport > > GetViewports() const = 0;
	
	/** Gets the active level viewport for this level editor */
	virtual TSharedPtr<SLevelViewport> GetActiveViewportInterface() = 0;

	/** Get the thumbnail pool used by this level editor */
	UE_DEPRECATED(5.0, "GetThumbnailPool has been replaced by UThumbnailManager::Get().GetSharedThumbnailPool().")
	virtual TSharedPtr< class FAssetThumbnailPool > GetThumbnailPool() const = 0;

	/** Access the level editor's action command list */
	virtual const TSharedPtr< FUICommandList >& GetLevelEditorActions() const = 0;

	/** Called to process a key down event in a viewport when in immersive mode */
	virtual FReply OnKeyDownInViewport( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) = 0;

	/** Append commands to the command list for the level editor */
	virtual void AppendCommands( const TSharedRef<FUICommandList>& InCommandsToAppend ) = 0;

	/** After spawning a new level viewport outside of the editor's tab system, this function must be called so that
	    the editor can keep track of that viewport */
	virtual void AddStandaloneLevelViewport( const TSharedRef<SLevelViewport>& LevelViewport ) = 0;

	/** Spawns an Actor Details widget */
	virtual TSharedRef<SWidget> CreateActorDetails( const FName TabIdentifier ) = 0;

	/** Set the filter that should be used to determine the set of objects that should be shown in a details panel when an actor in the level editor is selected */
	virtual void SetActorDetailsRootCustomization(TSharedPtr<FDetailsViewObjectFilter> ActorDetailsObjectFilter, TSharedPtr<IDetailRootObjectCustomization> ActorDetailsRootCustomization) = 0;

	/** Sets the UI customization of the SCSEditor inside the level editor details panel. */
	virtual void SetActorDetailsSCSEditorUICustomization(TSharedPtr<class ISCSEditorUICustomization> ActorDetailsSCSEditorUICustomization) = 0;

	/** Return the most recently interacted with Outliner */
	UE_DEPRECATED(5.1, "The Level Editor has multiple outliners, use GetAllSceneOutliners() or GetMostRecentlyUsedSceneOutliner() instead to avoid ambiguity")
	virtual TSharedPtr<ISceneOutliner> GetSceneOutliner() const = 0;

	/** Get an array containing weak pointers to all 4 Scene Outliners which could be potentially active */
	virtual TArray<TWeakPtr<ISceneOutliner>> GetAllSceneOutliners() const = 0;

	/** Set the outliner with the given name as the most recently interacted with */
	virtual void SetMostRecentlyUsedSceneOutliner(FName OutlinerIdentifier) = 0;

	/** Return the most recently interacted with Outliner */
	virtual TSharedPtr<ISceneOutliner> GetMostRecentlyUsedSceneOutliner() = 0;
};


