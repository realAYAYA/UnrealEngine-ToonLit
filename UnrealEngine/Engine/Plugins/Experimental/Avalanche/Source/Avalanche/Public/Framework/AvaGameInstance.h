// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaGameViewportClient.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "AvaGameInstance.generated.h"

class FRenderCommandFence;
class FSceneViewport;
class UTextureRenderTarget2D;
struct FAvaViewportQualitySettings;
struct FAvaInstanceSettings;

struct FAvaInstancePlaySettings
{
	const FAvaInstanceSettings& Settings;
	FName ChannelName;
	UTextureRenderTarget2D* RenderTarget;
	FIntPoint ViewportSize;
	const FAvaViewportQualitySettings& QualitySettings;
};

UCLASS(DisplayName = "Motion Design Game Instance")
class AVALANCHE_API UAvaGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	/**
	 * Creates a new Motion Design Game instance from the given Motion Design Template Asset.
	 * Supported asset types: UWorld (Level).
	 */
	static UAvaGameInstance* Create(UObject* InOuter);

	/** Create the game world */
	bool CreateWorld();

	bool IsWorldCreated() const { return bWorldCreated; }

	UWorld* GetPlayWorld() const { return PlayWorld.Get(); }

	bool BeginPlayWorld(const FAvaInstancePlaySettings& InWorldPlaySettings);

	/**
	 * The end play is normally requested to be done on the next Tick(). This is to
	 * avoid having the world destroyed while within the Tick() of that world.
	 * However, when shutting down, we need to force the end play to be done immediately because
	 * there will not a be a next Tick().
	 */
	void RequestEndPlayWorld(bool bForceImmediate);
	
	void RequestUnloadWorld(bool bForceImmediate);

	bool IsWorldPlaying() const { return bWorldPlaying; }

	/**
	 * Cancel any pending EndPlayWorld or UnloadWorld requests. 
	 */
	void CancelWorldRequests();

	/**
	* The output channels can be resized during playback, if this happens, the
	* scene viewport needs to be updated to reflect the change.
	*/
	void UpdateSceneViewportSize(const FIntPoint& InViewportSize);

	void UpdateRenderTarget(UTextureRenderTarget2D* InRenderTarget);

	UTextureRenderTarget2D* GetRenderTarget() const;

	/**
	 * Returns true if the render target has been rendered and is ready to be used for output.
	 */
	bool IsRenderTargetReady() const { return bIsRenderTargetReady;}

	/**
	 * Reset the flag so the OnRenderTargetReady event can be called again on the next render.
	 */
	void ResetRenderTargetReady() { bIsRenderTargetReady = false; }

	/**
	 * @brief Get the currently playing scene viewport.
	 * @remark Only valid within a BeginPlay()/EndPlay() scope. Null otherwise.
	 */
	TSharedPtr<FSceneViewport> GetSceneViewport() const { return Viewport; }

	/**
	 * @brief Get the currently playing game viewport client.
	 * @remark Only valid within a BeginPlay()/EndPlay() scope. Null otherwise.
	 */
	UAvaGameViewportClient* GetAvaGameViewportClient() const { return ViewportClient; }

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAvaGameInstanceEvent, UAvaGameInstance* /*InGameInstance*/, FName /*InChannelName*/);

	/** Event called on EndPlayWorld(). */
	static FOnAvaGameInstanceEvent& GetOnEndPlay() { return OnEndPlay; }

	/** Event called when the render target becomes ready. */
	static FOnAvaGameInstanceEvent& GetOnRenderTargetReady() { return OnRenderTargetReady; }

protected:
	void Tick(float DeltaSeconds);

	void UnloadWorld();
	void EndPlayWorld();
	void OnEndFrameTick();
	void OnEnginePreExit();

	//~ Begin UObject
	virtual void BeginDestroy() override;
	//~ End UObject

	UPROPERTY(Transient)
	TObjectPtr<UWorld> PlayWorld;

	UPROPERTY(Transient)
	TObjectPtr<UAvaGameViewportClient> ViewportClient;

	TSharedPtr<FSceneViewport> Viewport;

	/** Channel this game instance is playing on. */
	FName PlayingChannelName;

	bool bWorldCreated = false;
	bool bRequestUnloadWorld = false;
	bool bWorldPlaying = false;
	bool bRequestEndPlayWorld = false;

	/** Tick reentrancy guard. */
	bool bIsTicking = false;
	
	double LastDeltaSeconds = 0.0;

	/** Render command fence to ensure the render target has been rendered. */
	TUniquePtr<FRenderCommandFence> RenderTargetFence;
	bool bIsRenderTargetReady = false;

	/** Handle to delegate for EnginePreExit event. */
	FDelegateHandle EnginePreExitHandle;

	static FOnAvaGameInstanceEvent OnEndPlay;
	static FOnAvaGameInstanceEvent OnRenderTargetReady;
};
