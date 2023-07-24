// Copyright Epic Games, Inc. All Rights Reserved.

//=============================================================================
// LocalPlayer
//=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Engine/EngineTypes.h"
#include "Input/Reply.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Engine/GameViewportClient.h"
#endif
#include "Online/CoreOnline.h"
#include "SceneTypes.h"
#include "Engine/Player.h"
#include "GameFramework/OnlineReplStructs.h"

#include "Subsystems/LocalPlayerSubsystem.h"
#include "Subsystems/SubsystemCollection.h"

#include "LocalPlayer.generated.h"


#define INVALID_CONTROLLERID (-1)

class AActor;
class FSceneView;
class FSlateUser;
class FViewport;
class UGameInstance;
class UGameViewportClient;
class ULocalPlayer;
struct FMinimalViewInfo;
struct FSceneViewProjectionData;

/** A context object that binds to a LocalPlayer. Useful for UI or other things that need to pass around player references */
struct ENGINE_API FLocalPlayerContext
{
	FLocalPlayerContext();
	FLocalPlayerContext(const class ULocalPlayer* InLocalPlayer, UWorld* InWorld = nullptr);
	FLocalPlayerContext(const class APlayerController* InPlayerController);
	FLocalPlayerContext(const FLocalPlayerContext& InPlayerContext);

	/** Is this context initialized and still valid? */
	bool IsValid() const;

	/** Is this context initialized */
	bool IsInitialized() const;

	/** This function tests if the given Actor is connected to the Local Player in any way. 
		It tests against the APlayerController, APlayerState, and APawn. */
	bool IsFromLocalPlayer(const AActor* ActorToTest) const;

	/** Returns the world context. */
	UWorld* GetWorld() const;

	/** Returns the game instance */
	UGameInstance* GetGameInstance() const;

	/** Returns the local player. */
	class ULocalPlayer* GetLocalPlayer() const;

	/** Returns the player controller. */
	class APlayerController* GetPlayerController() const;

	/** Templated version of GetPlayerController() */
	template<class T>
	FORCEINLINE T* GetPlayerController(bool bCastChecked = false) const
	{
		if (bCastChecked)
		{ 
			return CastChecked<T>(GetPlayerController(), ECastCheckedType::NullAllowed);
		}
		else
		{
			return Cast<T>(GetPlayerController());
		}
	}

	/** Getter for the Game State Base */
	class AGameStateBase* GetGameState() const;

	/** Templated Getter for the Game State */
	template<class T>
	FORCEINLINE T* GetGameState(bool bCastChecked = false) const
	{
		if (bCastChecked)
		{
			return CastChecked<T>(GetGameState(), ECastCheckedType::NullAllowed);
		}
		else
		{
			return Cast<T>(GetGameState());
		}
	}

	/** Getter for the Player State */
	class APlayerState* GetPlayerState() const;

	/** Templated Getter for the Player State */
	template<class T>
	FORCEINLINE T* GetPlayerState(bool bCastChecked = false) const
	{
		if (bCastChecked)
		{
			return CastChecked<T>(GetPlayerState(), ECastCheckedType::NullAllowed);
		}
		else
		{
			return Cast<T>(GetPlayerState());
		}
	}

	/** Getter for this player's HUD */
	class AHUD* GetHUD() const;

	/** Templated Getter for the HUD */
	template<class T>
	FORCEINLINE T* GetHUD(bool bCastChecked = false) const
	{
		if (bCastChecked)
		{
			return CastChecked<T>(GetHUD(), ECastCheckedType::NullAllowed);
		}
		else
		{
			return Cast<T>(GetHUD());
		}
	}

	/** Getter for the base pawn of this player */
	class APawn* GetPawn() const;

	/** Templated getter for the player's pawn */
	template<class T>
	FORCEINLINE T* GetPawn(bool bCastChecked = false) const
	{
		if (bCastChecked)
		{
			return CastChecked<T>(GetPawn(), ECastCheckedType::NullAllowed);
		}
		else
		{
			return Cast<T>(GetPawn());
		}
	}

private:	

	/* Set the local player. */
	void SetLocalPlayer( const class ULocalPlayer* InLocalPlayer );

	/* Set the local player via a player controller. */
	void SetPlayerController( const class APlayerController* InPlayerController );

	TWeakObjectPtr<class ULocalPlayer>		LocalPlayer;

	TWeakObjectPtr<UWorld>					World;
};

/**
 *	Each player that is active on the current client/listen server has a LocalPlayer.
 *	It stays active across maps, and there may be several spawned in the case of splitscreen/coop.
 *	There will be 0 spawned on dedicated servers.
 */
UCLASS(Within=Engine, config=Engine, transient)
class ENGINE_API ULocalPlayer : public UPlayer
{
	GENERATED_UCLASS_BODY()

public:

	/** DO NOT USE. This constructor is for internal usage only for hot-reload purposes. */
	ULocalPlayer(FVTableHelper& Helper) : Super(Helper), SlateOperations(FReply::Unhandled()) {}

	/** The FUniqueNetId which this player is associated with. */
	FUniqueNetIdRepl CachedUniqueNetId;

	/** The primary viewport containing this player's view. */
	UPROPERTY()
	TObjectPtr<class UGameViewportClient> ViewportClient;

	/** The coordinates for the upper left corner of the primary viewport subregion allocated to this player. 0-1 */
	FVector2D Origin;

	/** The size of the primary viewport subregion allocated to this player. 0-1 */
	FVector2D Size;

	/** The location of the player's view the previous frame. */
	FVector LastViewLocation;

	/** How to constrain perspective viewport FOV */
	UPROPERTY(config)
	TEnumAsByte<enum EAspectRatioAxisConstraint> AspectRatioAxisConstraint;

	/** The class of PlayerController to spawn for players logging in. */
	UPROPERTY()
	TSubclassOf<class APlayerController> PendingLevelPlayerControllerClass;

	/** set when we've sent a split join request */
	UPROPERTY(VisibleAnywhere, transient, Category=LocalPlayer)
	uint32 bSentSplitJoin:1;

	DECLARE_EVENT_TwoParams(ULocalPlayer, FOnControllerIdChanged, int32 /*NewId*/, int32 /*OldId*/);
	FOnControllerIdChanged& OnControllerIdChanged() const { return OnControllerIdChangedEvent; }

	/** Event called when this local player has been assigned to a new platform-level user */
	DECLARE_EVENT_TwoParams(ULocalPlayer, FOnPlatformUserIdChanged, FPlatformUserId /*NewId*/, FPlatformUserId /*OldId*/);
	FOnPlatformUserIdChanged& OnPlatformUserIdChanged() { return OnPlatformUserIdChangedEvent; }

private:
	TArray<FSceneViewStateReference> ViewStates;

	/** The controller ID which this player accepts input from. */
	UPROPERTY()
	int32 ControllerId = INVALID_CONTROLLERID;

	mutable FOnControllerIdChanged OnControllerIdChangedEvent;

	/** The platform user this player is assigned to, could correspond to multiple input devices */
	FPlatformUserId PlatformUserId;

	/** Event called when platform user id changes */
	FOnPlatformUserIdChanged OnPlatformUserIdChangedEvent;

	FObjectSubsystemCollection<ULocalPlayerSubsystem> SubsystemCollection;

public:
	// UObject interface
	virtual void FinishDestroy() override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	// End of UObject interface

	// FExec interface
public:
#if UE_ALLOW_EXEC_COMMANDS
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd,FOutputDevice& Ar) override;
#endif
protected:
	virtual bool Exec_Editor(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	// End of FExec interface

public:
	/** 
	 * Exec command handlers
	 */

	bool HandleDNCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleExitCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleListMoveBodyCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleListAwakeBodiesCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleListSimBodiesCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleMoveComponentTimesCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleListSkelMeshesCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleListPawnComponentsCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleExecCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleToggleDrawEventsCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleToggleStreamingVolumesCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	
protected:
	/**
	 * Retrieve the viewpoint of this player.
	 * @param OutViewInfo - Upon return contains the view information for the player.
	 */
	virtual void GetViewPoint(FMinimalViewInfo& OutViewInfo) const;

	/** @todo document */
	void ExecMacro( const TCHAR* Filename, FOutputDevice& Ar );

	/** FReply used to defer some slate operations. */
	FReply SlateOperations;

public:

	/**
	 *  Getter for slate operations.
	 */
	FReply& GetSlateOperations() { return SlateOperations; }
	const FReply& GetSlateOperations() const { return SlateOperations; }

	/** Get the SlateUser that this LocalPlayer corresponds to */
	virtual TSharedPtr<FSlateUser> GetSlateUser();
	virtual TSharedPtr<const FSlateUser> GetSlateUser() const;

	/**
	 * Get the world the players actor belongs to
	 *
	 * @return  Returns the world of the LocalPlayer's PlayerController. NULL if the LocalPlayer does not have a PlayerController
	 */
	virtual UWorld* GetWorld() const override;

	/**
	 * Get the game instance associated with this local player
	 * 
	 * @return GameInstance related to local player
	 */
	UGameInstance* GetGameInstance() const;

	/**
	 * Returns the index of this player in the Game instances local players array
	 *
	 * @return Index in array, will be >= 0 if this is a fully registered player
	 */
	int32 GetIndexInGameInstance() const;

	/**
	 * Get a Subsystem of specified type
	 */
	ULocalPlayerSubsystem* GetSubsystemBase(TSubclassOf<ULocalPlayerSubsystem> SubsystemClass) const
	{
		return SubsystemCollection.GetSubsystem<ULocalPlayerSubsystem>(SubsystemClass);
	}

	/**
	 * Get a Subsystem of specified type
	 */
	template <typename TSubsystemClass>
	TSubsystemClass* GetSubsystem() const
	{
		return SubsystemCollection.GetSubsystem<TSubsystemClass>(TSubsystemClass::StaticClass());
	}

	/**
	 * Get a Subsystem of specified type from the provided LocalPlayer
	 * returns nullptr if the Subsystem cannot be found or the LocalPlayer is null
	 */
	template <typename TSubsystemClass>
	static FORCEINLINE TSubsystemClass* GetSubsystem(const ULocalPlayer* LocalPlayer)
	{
		if (LocalPlayer)
		{
			return LocalPlayer->GetSubsystem<TSubsystemClass>();
		}
		return nullptr;
	}

	/**
	 * Get all Subsystem of specified type, this is only necessary for interfaces that can have multiple implementations instanced at a time.
	 *
	 * Do not hold onto this Array reference unless you are sure the lifetime is less than that of ULocalPlayer
	 */
	template <typename TSubsystemClass>
	const TArray<TSubsystemClass*>& GetSubsystemArray() const
	{
		return SubsystemCollection.GetSubsystemArray<TSubsystemClass>(TSubsystemClass::StaticClass());
	}

	/**
	* Calculate the view init settings for drawing from this view actor
	*
	* @param	OutInitOptions - output view struct. Not every field is initialized, some of them are only filled in by CalcSceneView
	* @param	Viewport - current client viewport
	* @param	ViewDrawer - optional drawing in the view
	* @param	StereoViewIndex - index of the view when using stereoscopy
	* @return	true if the view options were filled in. false in various fail conditions.
	*/
	virtual bool CalcSceneViewInitOptions(
		struct FSceneViewInitOptions& OutInitOptions,
		FViewport* Viewport,
		class FViewElementDrawer* ViewDrawer = NULL,
		int32 StereoViewIndex = INDEX_NONE);

	/**
	 * Calculate the view settings for drawing from this view actor
	 *
	 * @param	View - output view struct
	 * @param	OutViewLocation - output actor location
	 * @param	OutViewRotation - output actor rotation
	 * @param	Viewport - current client viewport
	 * @param	ViewDrawer - optional drawing in the view
	 * @param	StereoViewIndex - index of the view when using stereoscopy
	 */
	virtual FSceneView* CalcSceneView(class FSceneViewFamily* ViewFamily,
		FVector& OutViewLocation,
		FRotator& OutViewRotation,
		FViewport* Viewport,
		class FViewElementDrawer* ViewDrawer = NULL,
		int32 StereoViewIndex = INDEX_NONE);

	/**
	 * Called at creation time for internal setup
	 */
	virtual void PlayerAdded(class UGameViewportClient* InViewportClient, int32 InControllerID);

	/**
	 * Called at creation time for internal setup
	 */
	virtual void PlayerAdded(class UGameViewportClient* InViewportClient, FPlatformUserId InUserId);

	/**
	 * Called to initialize the online delegates
	 */
	virtual void InitOnlineSession();

	/**
	 * Called when the player is removed from the viewport client
	 */
	virtual void PlayerRemoved();

	/**
	 * Create an actor for this player.
	 * @param URL - The URL the player joined with.
	 * @param OutError - If an error occurred, returns the error description.
	 * @param InWorld - World in which to spawn the play actor
	 * @return False if an error occurred, true if the play actor was successfully spawned.	 
	 */
	virtual bool SpawnPlayActor(const FString& URL,FString& OutError, UWorld* InWorld);
	
	/** Send a splitscreen join command to the server to allow a splitscreen player to connect to the game
	 * the client must already be connected to a server for this function to work
	 * @note this happens automatically for all viewports that exist during the initial server connect
	 * 	so it's only necessary to manually call this for viewports created after that
	 * if the join fails (because the server was full, for example) all viewports on this client will be disconnected
	 *
	 * @param	Options		array of URL options to append.
	 */
	virtual void SendSplitJoin(TArray<FString>& Options);
	
	/**
	 * Change the physical ControllerId for this player; if the specified ControllerId is already taken by another player, changes the ControllerId
	 * for the other player to the ControllerId currently in use by this player.
	 *
	 * @param	NewControllerId		the ControllerId to assign to this player.
	 */
	virtual void SetControllerId(int32 NewControllerId);

	/**
	 * Returns the physical controller ID for the player
	 */
	int32 GetControllerId() const { return ControllerId; }

	/**
	 * Changes the platform user that is assigned to this player
	 */
	virtual void SetPlatformUserId(FPlatformUserId InPlatformUserId);

	/**
	 * Returns the platform user that is assigned to this player
	 */
	FPlatformUserId GetPlatformUserId() const { return PlatformUserId; }

	/**
	 * Returns the logical local player index where 0 is the first player
	 * By default, this uses index in the game instance array
	 */
	virtual int32 GetLocalPlayerIndex() const;

	/** 
	 * Retrieves this player's name/tag from the online subsystem
	 * if this function returns a non-empty string, the returned name will replace the "Name" URL parameter
	 * passed around in the level loading and connection code, which normally comes from DefaultEngine.ini
	 * 
	 * @return Name of player if specified (by onlinesubsystem or otherwise), Empty string otherwise
	 */
	virtual FString GetNickname() const;

	/** 
	 * Retrieves any game-specific login options for this player
	 * if this function returns a non-empty string, the returned option or options be added
	 * passed in to the level loading and connection code.  Options are in URL format,
	 * key=value, with multiple options concatenated together with an & between each key/value pair
	 * 
	 * @return URL Option or options for this game, Empty string otherwise
	 */
	virtual FString GetGameLoginOptions() const { return TEXT(""); }

	// This should be deprecated when engine code has been changed to expect FPlatformUserId
	// UE_DEPRECATED(5.x, "Use GetUniqueNetIdForPlatformUser instead")
	FUniqueNetIdRepl GetUniqueNetIdFromCachedControllerId() const;

	/**
	 * Retrieves this player's unique net ID from the online subsystem using the platform user Id
	 *
	 * @return unique Id associated with this player
	 */
	virtual FUniqueNetIdRepl GetUniqueNetIdForPlatformUser() const;

	/** 
	 * Retrieves this player's unique net ID that was previously cached
	 *
	 * @return unique Id associated with this player
	 */
	FUniqueNetIdRepl GetCachedUniqueNetId() const;

	/** Sets the players current cached unique net id */
	UE_DEPRECATED(5.0, "Use SetCachedUniqueNetId with FUniqueNetIdRepl")
	void SetCachedUniqueNetId(FUniqueNetIdPtr NewUniqueNetId);
	/** Sets the players current cached unique net id */
	UE_DEPRECATED(5.0, "Use SetCachedUniqueNetId with FUniqueNetIdRepl")
	void SetCachedUniqueNetId(TYPE_OF_NULLPTR);
	/** Sets the players current cached unique net id */
	void SetCachedUniqueNetId(const FUniqueNetIdRepl& NewUniqueNetId);

	/** 
	 * Retrieves the preferred unique net id. This is for backwards compatibility for games that don't use the cached unique net id logic
	 *
	 * @return unique Id associated with this player
	 */
	virtual FUniqueNetIdRepl GetPreferredUniqueNetId() const;

	UE_DEPRECATED(5.0, "Platform User Id now has priority over ControllerId, these are not expected to be the same")
	bool IsCachedUniqueNetIdPairedWithControllerId() const;

	struct FOptionalAllottedSize
	{
		FVector2f Value;

		ENGINE_API FOptionalAllottedSize(std::nullptr_t Empty);
		ENGINE_API FOptionalAllottedSize(const FVector2d* InVector2D);
		ENGINE_API FOptionalAllottedSize(const FVector2f* InVector2D);

		ENGINE_API explicit operator bool() const;
	};

	/**
	 * This function will give you two points in Pixel Space that surround the World Space box.
	 *
	 * @param	ActorBox		The World Space Box
	 * @param	OutLowerLeft	The Lower Left corner of the Pixel Space box
	 * @param	OutUpperRight	The Upper Right corner of the pixel space box
	 * @return  False if there is no viewport, or if the box is behind the camera completely
	 */
	bool GetPixelBoundingBox(const FBox& ActorBox, FVector2D& OutLowerLeft, FVector2D& OutUpperRight, const FVector2f* OptionalAllotedSize = nullptr);
	static bool GetPixelBoundingBox(const FSceneViewProjectionData& ProjectionData, const FBox& ActorBox, FVector2D& OutLowerLeft, FVector2D& OutUpperRight, const FVector2f* OptionalAllotedSize = nullptr);

	UE_DEPRECATED(5.2, "Please use const FVector2f* directly")
	bool GetPixelBoundingBox(const FBox& ActorBox, FVector2D& OutLowerLeft, FVector2D& OutUpperRight, FOptionalAllottedSize OptionalAllotedSize);
	UE_DEPRECATED(5.2, "Please use const FVector2f* directly")
	static bool GetPixelBoundingBox(const FSceneViewProjectionData& ProjectionData, const FBox& ActorBox, FVector2D& OutLowerLeft, FVector2D& OutUpperRight, FOptionalAllottedSize OptionalAllotedSize);

	/**
	 * This function will give you a point in Pixel Space from a World Space position
	 *
	 * @param	InPoint		The point in world space
	 * @param	OutPoint	The point in pixel space
	 * @return  False if there is no viewport, or if the box is behind the camera completely
	 */
	bool GetPixelPoint(const FVector& InPoint, FVector2D& OutPoint, const FVector2f* OptionalAllotedSize = nullptr);
	static bool GetPixelPoint(const FSceneViewProjectionData& ProjectionData, const FVector& InPoint, FVector2D& OutPoint, const FVector2f* OptionalAllotedSize = nullptr);

	UE_DEPRECATED(5.2, "Please use const FVector2f* directly")
	bool GetPixelPoint(const FVector& InPoint, FVector2D& OutPoint, FOptionalAllottedSize OptionalAllotedSize);
	UE_DEPRECATED(5.2, "Please use const FVector2f* directly")
	static bool GetPixelPoint(const FSceneViewProjectionData& ProjectionData, const FVector& InPoint, FVector2D& OutPoint, FOptionalAllottedSize OptionalAllotedSize);

	/**
	 * Helper function for deriving various bits of data needed for projection
	 *
	 * @param	Viewport				The ViewClient's viewport
	 * @param	ProjectionData			The structure to be filled with projection data
     * @param	StereoViewIndex		    The index of the view when using stereoscopy
	 * @return  False if there is no viewport, or if the Actor is null
	 */
	virtual bool GetProjectionData(FViewport* Viewport, FSceneViewProjectionData& ProjectionData, int32 StereoViewIndex = INDEX_NONE) const;

	/**
	 * Determines whether this player is the first and primary player on their machine.
	 * @return	true if this player is not using splitscreen, or is the first player in the split-screen layout.
	 */
	bool IsPrimaryPlayer() const;
	 
	/**
	 * Clear cached view state.  Suitable for calling when cleaning up the world but the view state has some references objects (usually mids) owned by the world (thus preventing GC) 
	 */
	virtual void CleanupViewState();

	/** Locked view state needs access to GetViewPoint. */
	friend class FLockedViewState;
};

