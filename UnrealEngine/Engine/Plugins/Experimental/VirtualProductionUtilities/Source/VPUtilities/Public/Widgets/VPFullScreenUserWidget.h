// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/WidgetComponent.h"
#include "Fullscreen/VPFullScreenUserWidget_PostProcess.h"
#include "Fullscreen/VPFullScreenUserWidget_PostProcessWithSVE.h"
#include "VPFullScreenUserWidget.generated.h"

class FSceneViewport;
class FWidgetRenderer;
class FVPWidgetPostProcessHitTester;
class SConstraintCanvas;
class SVirtualWindow;
class SViewport;
class SWidget;
class ULevel;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPostProcessComponent;
class UTextureRenderTarget2D;
class UWorld;

#if WITH_EDITOR
class SLevelViewport;
#endif

UENUM(BlueprintType)
enum class EVPWidgetDisplayType : uint8
{
	/** Do not display. */
	Inactive,
	/** Display on a game viewport. */
	Viewport,
	/** Display by adding post process material via post process volume settings. Widget added only over area rendered by anamorphic squeeze. */
	PostProcessWithBlendMaterial,
	/** Render to a texture and send to composure. */
	Composure,
	/** Display by adding post process material via SceneViewExtensions. Widget added over entire viewport ignoring anamorphic squeeze.  */
	PostProcessSceneViewExtension,
};


USTRUCT()
struct FVPFullScreenUserWidget_Viewport
{
	GENERATED_BODY()

	bool Display(UWorld* World, UUserWidget* Widget, TAttribute<float> InDPIScale);
	void Hide(UWorld* World);

#if WITH_EDITOR
	/**
	 * The viewport to use for displaying.
	 * Defaults to GetFirstActiveLevelViewport().
	 */
	TWeakPtr<FSceneViewport> EditorTargetViewport;
#endif

private:

	/** Constraint widget that contains the widget we want to display. */
	TWeakPtr<SConstraintCanvas> FullScreenCanvasWidget;

#if WITH_EDITOR
	/** Level viewport the widget was added to. */
	TWeakPtr<SLevelViewport> OverlayWidgetLevelViewport;
#endif
};

/**
 * Will set the Widgets on a viewport either by Widgets are first rendered to a render target, then that render target is displayed in the world.
 */
UCLASS(meta=(ShowOnlyInnerProperties))
class VPUTILITIES_API UVPFullScreenUserWidget : public UObject
{
	GENERATED_BODY()
public:
	
	UVPFullScreenUserWidget(const FObjectInitializer& ObjectInitializer);

	//~ Begin UObject interface
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface

	virtual bool Display(UWorld* World);
	virtual void Hide();
	virtual void Tick(float DeltaTime);

	/** Sets the way the widget is supposed to be displayed */
	void SetDisplayTypes(EVPWidgetDisplayType InEditorDisplayType, EVPWidgetDisplayType InGameDisplayType, EVPWidgetDisplayType InPIEDisplayType);
	
	/**
	 * If using EVPWidgetDisplayType::PostProcessWithBlendMaterial, you can specify a custom post process settings that should be modified.
	 * By default, a new post process component is added to AWorldSettings.
	 *
	 * @param InCustomPostProcessSettingsSource An object containing a FPostProcessSettings UPROPERTY()
	 */
	void SetCustomPostProcessSettingsSource(TWeakObjectPtr<UObject> InCustomPostProcessSettingsSource);
	
	bool ShouldDisplay(UWorld* World) const;
	EVPWidgetDisplayType GetDisplayType(UWorld* World) const;
	bool IsDisplayed() const;
	
	/** Get a pointer to the inner widget. Note: This should not be stored! */
	UUserWidget* GetWidget() const { return Widget; };

	/** Gets the widget class being displayed */
	TSubclassOf<UUserWidget> GetWidgetClass() const { return WidgetClass; }
	/** Sets the widget class to be displayed. This must be called while not widget is being displayed - the class will not be updated while IsDisplayed().  */
	void SetWidgetClass(TSubclassOf<UUserWidget> InWidgetClass);

	constexpr static bool DoesDisplayTypeUsePostProcessSettings(EVPWidgetDisplayType Type) { return Type == EVPWidgetDisplayType::Composure || Type == EVPWidgetDisplayType::PostProcessSceneViewExtension || Type == EVPWidgetDisplayType::PostProcessWithBlendMaterial; }
	/** Returns the base post process settings for the given type if DoesDisplayTypeUsePostProcessSettings(Type) returns true. */
	FVPFullScreenUserWidget_PostProcessBase* GetPostProcessDisplayTypeSettingsFor(EVPWidgetDisplayType Type);
	FVPFullScreenUserWidget_PostProcess& GetPostProcessDisplayTypeWithBlendMaterialSettings() { return PostProcessDisplayTypeWithBlendMaterial; }
	FVPFullScreenUserWidget_PostProcess& GetComposureDisplayTypeSettings() { return PostProcessDisplayTypeWithBlendMaterial; }
	FVPFullScreenUserWidget_PostProcessWithSVE& GetPostProcessDisplayTypeWithSceneViewExtensionsSettings() { return PostProcessWithSceneViewExtensions; }
	/** Returns the base post process settings for the given type if DoesDisplayTypeUsePostProcessSettings(Type) returns true. */
	const FVPFullScreenUserWidget_PostProcessBase* GetPostProcessDisplayTypeSettingsFor(EVPWidgetDisplayType Type) const;
	const FVPFullScreenUserWidget_PostProcess& GetPostProcessDisplayTypeWithBlendMaterialSettings() const { return PostProcessDisplayTypeWithBlendMaterial; }
	const FVPFullScreenUserWidget_PostProcess& GetComposureDisplayTypeSettings() const { return PostProcessDisplayTypeWithBlendMaterial; }
	const FVPFullScreenUserWidget_PostProcessWithSVE& GetPostProcessDisplayTypeWithSceneViewExtensionsSettings() const { return PostProcessWithSceneViewExtensions; }

#if WITH_EDITOR
	/**
	 * Sets the TargetViewport to use on both the Viewport and the PostProcess class.
	 * 
	 * Overrides the viewport to use for displaying.
	 * Defaults to GetFirstActiveLevelViewport().
	 */
	void SetEditorTargetViewport(TWeakPtr<FSceneViewport> InTargetViewport);
	/** Resets the TargetViewport  */
	void ResetEditorTargetViewport();
#endif

protected:
	
	/** The class of User Widget to create and display an instance of */
	UPROPERTY(EditAnywhere, Category = "User Interface")
	TSubclassOf<UUserWidget> WidgetClass;
	
	/** The display type when the world is an editor world. */
	UPROPERTY(EditAnywhere, Category = "User Interface")
	EVPWidgetDisplayType EditorDisplayType;

	/** The display type when the world is a game world. */
	UPROPERTY(EditAnywhere, Category = "User Interface")
	EVPWidgetDisplayType GameDisplayType;

	/** The display type when the world is a PIE world. */
	UPROPERTY(EditAnywhere, Category = "User Interface", meta = (DisplayName = "PIE Display Type"))
	EVPWidgetDisplayType PIEDisplayType;

	/** Behavior when the widget should be display by the slate attached to the viewport. */
	UPROPERTY(EditAnywhere, Category = "Viewport", meta = (ShowOnlyInnerProperties))
	FVPFullScreenUserWidget_Viewport ViewportDisplayType;

	/** Behavior when the widget is rendered via a post process material via post process volume settings; the widget is added only over area rendered by anamorphic squeeze. */
	UPROPERTY(EditAnywhere, Category = "Post Process (Blend Material)")
	FVPFullScreenUserWidget_PostProcess PostProcessDisplayTypeWithBlendMaterial;
	
	/** Behavior when the widget is rendered via a post process material via SceneViewExtensions; the widget is added over entire viewport, ignoring anamorphic squeeze. */
	UPROPERTY(EditAnywhere, Category = "Post Process (Scene View Extensions)")
	FVPFullScreenUserWidget_PostProcessWithSVE PostProcessWithSceneViewExtensions;
	
	bool InitWidget();
	void ReleaseWidget();

	FVector2D FindSceneViewportSize();
	float GetViewportDPIScale();

private:
	
	/** The User Widget object displayed and managed by this component */
	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<UUserWidget> Widget;

	/** The world the widget is attached to. */
	UPROPERTY(Transient, DuplicateTransient)
	TWeakObjectPtr<UWorld> World;

	/** How we currently displaying the widget. */
	UPROPERTY(Transient, DuplicateTransient)
	EVPWidgetDisplayType CurrentDisplayType;

	/** The user requested the widget to be displayed. It's possible that some setting are invalid and the widget will not be displayed. */
	bool bDisplayRequested;

#if WITH_EDITOR
	/**
	 * The viewport to use for displaying.
	 * Defaults to GetFirstActiveLevelViewport().
	 */
	TWeakPtr<FSceneViewport> EditorTargetViewport;
#endif
	
	void OnLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld);
	void OnWorldCleanup(UWorld* InWorld, bool bSessionEnded, bool bCleanupResources);
};
