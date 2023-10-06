// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InteractiveTool.h"
#include "InteractiveGizmo.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ScriptableInteractiveTool.generated.h"

class UWorld;
class UCombinedTransformGizmo;
class UTransformProxy;
class UScriptableInteractiveTool;
class UBaseScriptableToolBuilder;
class FToolDataVisualizer;
class FCanvas;
class FSceneView;

UENUM(BlueprintType)
enum class EToolsFrameworkOutcomePins : uint8
{
	Success,
	Failure
};


UENUM(BlueprintType)
enum class EScriptableToolShutdownType : uint8
{
	Complete = 0,
	AcceptCancel = 1
};

USTRUCT(BlueprintType)
struct FScriptableToolModifierStates
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Modifiers)
	bool bShiftDown = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Modifiers)
	bool bCtrlDown = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Modifiers)
	bool bAltDown = false;
};



UENUM(BlueprintType)
enum class EScriptableToolGizmoMode : uint8
{
	TranslationOnly = 0,
	RotationOnly = 1,
	ScaleOnly = 2,
	Combined = 3,
	FromViewportSettings = 4
};

UENUM(BlueprintType)
enum class EScriptableToolGizmoCoordinateSystem : uint8
{
	World = 0,
	Local = 1,
	FromViewportSettings = 2
};

UENUM(BlueprintType)
enum class EScriptableToolGizmoStateChangeType : uint8
{
	BeginTransform = 0,
	EndTransform = 1,
	UndoRedo = 2
};

UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EScriptableToolGizmoTranslation : uint8
{
	None = 0,

	TranslateAxisX = 1 << 1,
	TranslateAxisY = 1 << 2,
	TranslateAxisZ = 1 << 3,
	TranslatePlaneXY = 1 << 4,
	TranslatePlaneXZ = 1 << 5,
	TranslatePlaneYZ = 1 << 6,

	All = 0xFF UMETA(Hidden)
};
ENUM_CLASS_FLAGS(EScriptableToolGizmoTranslation);


UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EScriptableToolGizmoRotation : uint8
{
	None = 0,

	RotateAxisX = 1 << 1,
	RotateAxisY = 1 << 2,
	RotateAxisZ = 1 << 3,

	All = 0xFF UMETA(Hidden)
};
ENUM_CLASS_FLAGS(EScriptableToolGizmoRotation);


UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EScriptableToolGizmoScale : uint8
{
	None = 0,

	ScaleAxisX = 1 << 1,
	ScaleAxisY = 1 << 2,
	ScaleAxisZ = 1 << 3,
	ScalePlaneXY = 1 << 4,
	ScalePlaneXZ = 1 << 5,
	ScalePlaneYZ = 1 << 6,
	ScaleUniform = 1 << 7,

	All = 0xFF UMETA(Hidden)
};
ENUM_CLASS_FLAGS(EScriptableToolGizmoScale);


/**
 * FScriptableToolGizmoOptions is a configuration struct passed to the CreateTRSGizmo function
 * of UScriptableInteractiveTool. 
 */
USTRUCT(BlueprintType)
struct FScriptableToolGizmoOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Gizmo)
	EScriptableToolGizmoMode GizmoMode = EScriptableToolGizmoMode::Combined;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Gizmo, meta = (DisplayName = "Translation Elements", Bitmask, BitmaskEnum = "/Script/ScriptableToolsFramework.EScriptableToolGizmoTranslation",
	ValidEnumValues = "TranslateAxisX, TranslateAxisY, TranslateAxisZ, TranslatePlaneXY, TranslatePlaneXZ, TranslatePlaneYZ"))
	int32 TranslationParts = static_cast<int32>(EScriptableToolGizmoTranslation::All);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Gizmo, meta = (DisplayName = "Rotation Elements", Bitmask, BitmaskEnum = "/Script/ScriptableToolsFramework.EScriptableToolGizmoRotation",
	ValidEnumValues = "RotateAxisX, RotateAxisY, RotateAxisZ"))
	int32 RotationParts = static_cast<int32>(EScriptableToolGizmoRotation::All);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Gizmo, meta = (DisplayName = "Scale Elements", Bitmask, BitmaskEnum = "/Script/ScriptableToolsFramework.EScriptableToolGizmoScale",
	ValidEnumValues = "ScaleAxisX, ScaleAxisY, ScaleAxisZ, ScalePlaneXY, ScalePlaneXZ, ScalePlaneYZ"))
	int32 ScaleParts = static_cast<int32>(EScriptableToolGizmoScale::All);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Gizmo)
	EScriptableToolGizmoCoordinateSystem CoordSystem = EScriptableToolGizmoCoordinateSystem::Local;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Gizmo)
	bool bSnapTranslation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Gizmo)
	bool bSnapRotation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Gizmo)
	bool bRepositionable = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Gizmo)
	bool bAllowNegativeScaling = true;
};



/**
 * UScriptableTool_RenderAPI is helper Object that is created internally by a UScriptableInteractiveTool
 * to allow Blueprints to access basic 3D rendering functionality, in the context of a specific Tool.
 * The OnScriptRender event is called with an instance of this type.
 */
UCLASS(Transient)
class SCRIPTABLETOOLSFRAMEWORK_API UScriptableTool_RenderAPI : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "ScriptableTool|Render")
	UPARAM(DisplayName="Render Object") UScriptableTool_RenderAPI* 
	DrawLine(FVector Start, FVector End, FLinearColor Color, float Thickness = 1.0, float DepthBias = 0.0f, bool bDepthTested = true);

	UFUNCTION(BlueprintCallable, Category = "ScriptableTool|Render")
	UPARAM(DisplayName="Render Object") UScriptableTool_RenderAPI* 
	DrawRectWidthHeightXY(FTransform Transform, double Width, double Height, FLinearColor Color, float LineThickness = 1.0, float DepthBias = 0.0f, bool bDepthTested = true, bool bCentered = true);


	// DrawPoint, DrawCircle, DrawViewFacingCircle
	// DrawWireCylinder, DrawWireBox, DrawSquare
	// PushTransform, PopTransform, SetTransform, PopAllTransforms

protected:
	friend class UScriptableInteractiveTool;

	FToolDataVisualizer* ActiveVisualizer = nullptr;
};

/**
 * UScriptableTool_HUDAPI is helper Object that is created internally by a UScriptableInteractiveTool
 * to allow Blueprints to access basic 2D rendering functionality, in the context of a specific Tool.
 * The OnScriptDrawHUD event is called with an instance of this type.
 */
UCLASS(Transient)
class SCRIPTABLETOOLSFRAMEWORK_API UScriptableTool_HUDAPI : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "ScriptableTool|DrawHUD", Meta = (Color = "(R=1.0,G=1.0,B=1.0,A=1.0)"))
	UPARAM(DisplayName="HUD Object") UScriptableTool_HUDAPI* 
	DrawTextAtLocation(FVector Location, FString String, FLinearColor Color, bool bCentered, float ShiftRowsY);

	UFUNCTION(BlueprintCallable, Category = "ScriptableTool|DrawHUD", Meta = (Color = "(R=1.0,G=1.0,B=1.0,A=1.0)"))
	UPARAM(DisplayName="HUD Object") UScriptableTool_HUDAPI* 
	DrawTextArrayAtLocation(FVector Location, TArray<FString> Strings, FLinearColor Color, bool bCentered, float ShiftRowsY);

	UFUNCTION(BlueprintCallable, Category = "ScriptableTool|DrawHUD")
	UPARAM(DisplayName="HUD Object") UScriptableTool_HUDAPI* 
	GetCanvasLocation(FVector Location, FVector2D& CanvasLocation );


	// DrawItem to draw line/etc (FCanvasLineItem, FCanvasBoxItem, FCanvasTriangleItem, FCanvasNGonItem)
	// DrawTile
	// DrawMaterial? via FCanvasTileItem

protected:
	friend class UScriptableInteractiveTool;
	FCanvas* ActiveCanvas = nullptr;
	const FSceneView* ActiveSceneView = nullptr;
};



/**
 * UScriptableInteractiveToolPropertySet is a Blueprintable extension of UInteractiveToolPropertySet.
 * This is a helper type meant to be used with UScriptableInteractiveTool. The intention is that
 * the "Settings" of a particular Tool are stored in UScriptableInteractiveToolPropertySet ("Property Set") instances.
 * The function UScriptableInteractiveTool::AddPropertySetOfType can be used to create and attach
 * a new Property Set. Then these settings can be automatically exposed by, for example, the Editor Mode UI, 
 * similar to (eg) Modeling Mode in the UE Editor.
 * 
 * In addition, UScriptableInteractiveTool has a set of functions like WatchFloatProperty, WatchBoolProperty, etc,
 * that can be used to detect and respond to changes in the Property Set. This works with both changes caused
 * by Editor UI (eg a Details Panel) as well as done directly in Blueprints or even C++ code. 
  * 
 * Note, however, that this Property Set mechanism completely optional, a Tool builder is free to use any
 * method whatsoever to store/modify Tool settings.
 */
UCLASS(Transient, Blueprintable)
class SCRIPTABLETOOLSFRAMEWORK_API UScriptableInteractiveToolPropertySet : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

	/** Access the Tool that owns this PropertySet */
	UFUNCTION(BlueprintCallable, Category = "ScriptableTool", Meta = (ExpandEnumAsExecs = "Outcome"))
	UScriptableInteractiveTool* GetOwningTool(
		EToolsFrameworkOutcomePins& Outcome);

protected:
	TWeakObjectPtr<UScriptableInteractiveTool> ParentTool;
	friend class UScriptableInteractiveTool;
};



// these are delegates for the various property-watchers below
DECLARE_DYNAMIC_DELEGATE_TwoParams(FToolPropertyModifiedDelegate, UScriptableInteractiveToolPropertySet*, PropertySet, FString, PropertyName);
DECLARE_DYNAMIC_DELEGATE_ThreeParams(FToolFloatPropertyModifiedDelegate, UScriptableInteractiveToolPropertySet*, PropertySet, FString, PropertyName, double, NewValue);
DECLARE_DYNAMIC_DELEGATE_ThreeParams(FToolIntPropertyModifiedDelegate, UScriptableInteractiveToolPropertySet*, PropertySet, FString, PropertyName, int, NewValue);
DECLARE_DYNAMIC_DELEGATE_ThreeParams(FToolBoolPropertyModifiedDelegate, UScriptableInteractiveToolPropertySet*, PropertySet, FString, PropertyName, bool, bNewValue);
DECLARE_DYNAMIC_DELEGATE_ThreeParams(FToolEnumPropertyModifiedDelegate, UScriptableInteractiveToolPropertySet*, PropertySet, FString, PropertyName, uint8, NewValue);
DECLARE_DYNAMIC_DELEGATE_ThreeParams(FToolStringPropertyModifiedDelegate, UScriptableInteractiveToolPropertySet*, PropertySet, FString, PropertyName, FString, NewValue);
DECLARE_DYNAMIC_DELEGATE_ThreeParams(FToolFNamePropertyModifiedDelegate, UScriptableInteractiveToolPropertySet*, PropertySet, FString, PropertyName, FName, NewValue);
DECLARE_DYNAMIC_DELEGATE_ThreeParams(FToolObjectPropertyModifiedDelegate, UScriptableInteractiveToolPropertySet*, PropertySet, FString, PropertyName, UObject*, NewValue);


/**
 * UScriptableInteractiveTool is an extension of UInteractiveTool that allows the Tool functionality to be 
 * defined via Blueprints. 
 */
UCLASS(Transient, Blueprintable)
class SCRIPTABLETOOLSFRAMEWORK_API UScriptableInteractiveTool : public UInteractiveTool
{
	GENERATED_BODY()
public:
	/** Name of this Tool, will be used in (eg) Toolbars */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Scriptable Tool Settings")
	FText ToolName;

	/** Long Name of this Tool, will be used in (eg) longer labels like the Accept/Cancel toolbar */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Scriptable Tool Settings", meta=(DisplayName="Long Name"))
	FText ToolLongName;

	/** Category of this Tool, will be used in (eg) Tool Palette Section headers */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Scriptable Tool Settings", meta=(DisplayName="Category"))
	FText ToolCategory;

	/** Tooltip used for this Tool in (eg) icons/etc */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Scriptable Tool Settings", meta=(DisplayName="Tooltip"))
	FText ToolTooltip;

	/**
	 * Relative Path to a custom Icon Image for this Tool. The Image file format must be png or svg. 
	 *
	 * The Image file must reside in the same Content folder hierarchy as contains the Tool Class (ie Blueprint asset), 
	 * i.e. either the Project Content folder or a Plugin Content folder.
	 * 
	 * So for example if the Tool BP is in a plugin named MyCustomTools, the icon must be in 
	 *      MyProject/Plugins/MyCustomTools/Content/<SubFolders>/MyToolIcon.png,
	 * and the relative path to use here would be <SubFolders>/MyToolIcon.png
	 */ 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Scriptable Tool Settings", meta = (DisplayName = "Custom Icon Path"))
	FString CustomIconPath;

	/** A generic flag to indicate whether this Tool should be shown in the UE Editor. This may be interpreted in different ways */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Scriptable Tool Settings", meta=(DisplayName="Visible in Editor"))
	bool bShowToolInEditor = true;

	/** Specifies how the user exits this Tool, either Accept/Cancel-style or Complete-style */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Scriptable Tool Settings", meta=(DisplayName="Shutdown Type"))
	EScriptableToolShutdownType ToolShutdownType = EScriptableToolShutdownType::Complete;

	/**
	 * Implement OnScriptSetup to do initial setup/configuration of the Tool, such as adding
	 * Property Sets, creating Gizmos, etc
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ScriptableTool|Events")
	void OnScriptSetup();

	virtual void Setup() override;


	/**
	 * OnScriptTick is called every Editor Tick, ie basically every frame.
	 * Implement per-frame processing here.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ScriptableTool|Events")
	void OnScriptTick(float DeltaTime);

	virtual void OnTick(float DeltaTime) override;


	/**
	 * CanAccept function. Implement this function for AcceptCancel Tools, and return
	 * true when it is valid for the Tool Accept button to be active. Defaults to always true.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "ScriptableTool|Events")
	bool OnScriptCanAccept() const;

	bool OnScriptCanAccept_Implementation() const;

	virtual bool HasAccept() const override;
	virtual bool HasCancel() const override;
	virtual bool CanAccept() const override;



	/**
	 * OnScriptShutdown is called when the Tool is shutting down. The ShutdownType defines
	 * what the Tool should do. For Complete-style Tools, there is no difference, but for
	 * Accept/Cancel-style Tools, on Accept the Tool should "commit" whatever it is previewing/etc,
	 * and on Cancel it should roll back / do-nothing. Which of these occurs is based on the
	 * ToolShutdownType property.
	 * 
	 * Tool Shutdown cannot be interrupted/aborted
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ScriptableTool|Events")
	void OnScriptShutdown(EToolShutdownType ShutdownType);

	virtual void Shutdown(EToolShutdownType ShutdownType) override;

	/**
	 * Request that the active Tool be shut down. The Tool can post this to shut itself down (eg in a Tool where
	 * the interaction paradigm is "click to do something and exit"). 
	 * @param bAccept if this is an Accept/Cancel Tool, Accept it, otherwise Cancel. Ignored for Complete-type Tools.
	 */
	UFUNCTION(BlueprintCallable, Category = "ScriptableTool", Meta = (Identifier="Settings"))
	void RequestToolShutdown(
		bool bAccept,
		bool bShowUserPopupMessage,
		FText UserMessage);



protected:
	virtual void PostInitProperties() override;


	// return instance of custom tool builder. Should only be called on CDO.
	virtual UBaseScriptableToolBuilder* GetNewCustomToolBuilderInstance(UObject* Outer) { return nullptr; }
	friend class UScriptableToolSet;


protected:
	UPROPERTY(Transient, DuplicateTransient, NonTransactional, SkipSerialization)
	TWeakObjectPtr<UWorld> TargetWorld;

	virtual UWorld* GetWorld() const override;		// must implement this for some reason, otherwise many BP functions will not show up

public:
	virtual void SetTargetWorld(UWorld* World);		// needs to be accessible to ToolBuilder

	/**
	 * Access the World this Tool is currently operating on.
	 */
	UFUNCTION(BlueprintCallable, Category = "ScriptableTool")
	UWorld* GetToolWorld();


	//
	// Render and DrawHUD support
	//
public:
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

	/**
	 * OnScriptRender is called every frame. Use the RenderAPI object to draw
	 * various simple geometric elements like lines and points. This drawing
	 * is not very efficient but is useful for basic Tool visualization/feedback.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ScriptableTool|Events")
	void OnScriptRender( UPARAM(DisplayName="Render Object") UScriptableTool_RenderAPI* RenderAPI);

	virtual void DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI) override;

	/**
	 * OnScriptDrawHUD is called every frame. Use the DrawHUDAPI to draw various
	 * simple HUD elements like text. This drawing is not very efficient but
	 * is useful for basic Tool visualization/feedback.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ScriptableTool|Events")
	void OnScriptDrawHUD( UPARAM(DisplayName="HUD Object") UScriptableTool_HUDAPI* DrawHUDAPI);


protected:
	UPROPERTY(Transient, DuplicateTransient, NonTransactional, SkipSerialization)
	TObjectPtr<UScriptableTool_RenderAPI> RenderHelper;

	UPROPERTY(Transient, DuplicateTransient, NonTransactional, SkipSerialization)
	TObjectPtr<UScriptableTool_HUDAPI> DrawHUDHelper;


	//
	// Property Set support
	//
public:

	/**
	 * Create a new Tool Property Set (ie BP subclass of UScriptableInteractiveToolPropertySet) with the given string Identifier
	 * and attach it to the Tool. The public variables of this Property Set object will appear in (eg) Mode Details Panels, etc.
	 * Multiple Property Sets of the same Type can be attached to a Tool, but each must have a unique Identifier.
	 */
	UFUNCTION(BlueprintCallable, Category = "ScriptableTool|PropertySets", Meta = (ExpandEnumAsExecs = "Outcome", Identifier="Settings"))
	UPARAM(DisplayName="New Property Set") UScriptableInteractiveToolPropertySet* 
	AddPropertySetOfType(
		TSubclassOf<UScriptableInteractiveToolPropertySet> PropertySetType,
		FString Identifier,
		EToolsFrameworkOutcomePins& Outcome);

	/**
	 * Remove a Property Set from the current Tool, found via it's unique Identifier.
	 * Unless the Property Set absolutely needs to stop existing, it is likely preferable to use SetPropertySetVisibleByName() to simply hide it.
	 */
	UFUNCTION(BlueprintCallable, Category = "ScriptableTool|PropertySets", Meta = (ExpandEnumAsExecs = "Outcome", Identifier="Settings"))
	void RemovePropertySetByName(
		FString Identifier,
		EToolsFrameworkOutcomePins& Outcome);

	/**
	 * Set the visibility of a Property Set that is paired with the given unique Identifier.
	 */
	UFUNCTION(BlueprintCallable, Category = "ScriptableTool|PropertySets", Meta = (Identifier="Settings"))
	void SetPropertySetVisibleByName(
		FString Identifier,
		bool bVisible);

	/**
	 * Force the Property Set associated with the given Identifier to be updated. 
	 * This may be necessary if you change the Property Set values directly via Blueprints, and want to
	 * see your changes reflected in (eg) a Details Panel showing the Tool Property values. 
	 * Currently these changes cannot be detected automatically, necessitating this function.
	 */
	UFUNCTION(BlueprintCallable, Category = "ScriptableTool|PropertySets", Meta = (Identifier="Settings"))
	void ForcePropertySetUpdateByName(
		FString Identifier);

	/**
	 * Restore the values of the specified PropertySet, optionally with a specific SaveKey string.
	 * This will have no effect unless SavePropertySetSettings() has been called on a compatible Property Set
	 * in the same Engine session (ie in a previous invocation of the Tool, or another Tool that uses the same Property Set)
	 */
	UFUNCTION(BlueprintCallable, Category = "ScriptableTool|PropertySets", Meta = (Identifier="Settings"))
	UPARAM(DisplayName="Property Set") UScriptableInteractiveToolPropertySet* RestorePropertySetSettings(
		UScriptableInteractiveToolPropertySet* PropertySet,
		FString SaveKey);

	/**
	 * Save the values of the specified PropertySet, optionally with a specific SaveKey string.
	 * These saved values can be restored in future Tool invocations based on the SaveKey.
	 */
	UFUNCTION(BlueprintCallable, Category = "ScriptableTool|PropertySets", Meta = (Identifier="Settings"))
	UPARAM(DisplayName="Property Set") UScriptableInteractiveToolPropertySet* SavePropertySetSettings(
		UScriptableInteractiveToolPropertySet* PropertySet,
		FString SaveKey);



	//
	// Property Watcher support
	//
public:
	/**
	 * Watch a Float-valued Property for changes (double precision)
	 * @param PropertySet the Property Set which contains the desired Property to watch.
	 * @param PropertyName the string name of the Property in the given Property Set
	 * @param OnModified this delegate will be called if the Property value changes.
	 */
	UFUNCTION(BlueprintCallable, Category = "ScriptableTool|PropertySets")
	UPARAM(DisplayName="Property Set") UScriptableInteractiveToolPropertySet* WatchFloatProperty(
		UScriptableInteractiveToolPropertySet* PropertySet,
		FString PropertyName,
		const FToolFloatPropertyModifiedDelegate& OnModified );

	/**
	 * Watch an Integer-valued Property for changes
	 * @param PropertySet the Property Set which contains the desired Property to watch.
	 * @param PropertyName the string name of the Property in the given Property Set
	 * @param OnModified this delegate will be called if the Property value changes.
	 */
	UFUNCTION(BlueprintCallable, Category = "ScriptableTool|PropertySets")
	UPARAM(DisplayName="Property Set") UScriptableInteractiveToolPropertySet* WatchIntProperty(
		UScriptableInteractiveToolPropertySet* PropertySet,
		FString PropertyName,
		const FToolIntPropertyModifiedDelegate& OnModified );

	/**
	 * Watch a Bool-valued Property for changes
	 * @param PropertySet the Property Set which contains the desired Property to watch.
	 * @param PropertyName the string name of the Property in the given Property Set
	 * @param OnModified this delegate will be called if the Property value changes.
	 */
	UFUNCTION(BlueprintCallable, Category = "ScriptableTool|PropertySets")
	UPARAM(DisplayName="Property Set") UScriptableInteractiveToolPropertySet* WatchBoolProperty(
		UScriptableInteractiveToolPropertySet* PropertySet,
		FString PropertyName,
		const FToolBoolPropertyModifiedDelegate& OnModified );

	/**
	 * Watch an Enum-valued Property for changes. Note that in this case the OnModified
	 * delegate will be called with a uint8 integer, which can be cast back to the original Enum type.
	 * @param PropertySet the Property Set which contains the desired Property to watch.
	 * @param PropertyName the string name of the Property in the given Property Set
	 * @param OnModified this delegate will be called if the Property value changes.
	 */
	UFUNCTION(BlueprintCallable, Category = "ScriptableTool|PropertySets")
	UPARAM(DisplayName="Property Set") UScriptableInteractiveToolPropertySet* WatchEnumProperty(
		UScriptableInteractiveToolPropertySet* PropertySet,
		FString PropertyName,
		const FToolEnumPropertyModifiedDelegate& OnModified );

	/**
	 * Watch a String-valued Property for changes
	 * @param PropertySet the Property Set which contains the desired Property to watch.
	 * @param PropertyName the string name of the Property in the given Property Set
	 * @param OnModified this delegate will be called if the Property value changes.
	 */
	UFUNCTION(BlueprintCallable, Category = "ScriptableTool|PropertySets")
	UPARAM(DisplayName="Property Set") UScriptableInteractiveToolPropertySet* WatchStringProperty(
		UScriptableInteractiveToolPropertySet* PropertySet,
		FString PropertyName,
		const FToolStringPropertyModifiedDelegate& OnModified );

	/**
	 * Watch an (F)Name-valued Property for changes
	 * @param PropertySet the Property Set which contains the desired Property to watch.
	 * @param PropertyName the string name of the Property in the given Property Set
	 * @param OnModified this delegate will be called if the Property value changes.
	 */
	UFUNCTION(BlueprintCallable, Category = "ScriptableTool|PropertySets")
	UPARAM(DisplayName="Property Set") UScriptableInteractiveToolPropertySet* WatchNameProperty(
		UScriptableInteractiveToolPropertySet* PropertySet,
		FString PropertyName,
		const FToolFNamePropertyModifiedDelegate& OnModified );

	/**
	 * Watch an Object-valued Property for changes (ie UObject, UClass, etc)
	 * @param PropertySet the Property Set which contains the desired Property to watch.
	 * @param PropertyName the string name of the Property in the given Property Set
	 * @param OnModified this delegate will be called if the Property value changes.
	 */
	UFUNCTION(BlueprintCallable, Category = "ScriptableTool|PropertySets")
	UPARAM(DisplayName="Property Set") UScriptableInteractiveToolPropertySet* WatchObjectProperty(
		UScriptableInteractiveToolPropertySet* PropertySet,
		FString PropertyName,
		const FToolObjectPropertyModifiedDelegate& OnModified );

	/**
	 * Watch any Property in a PropertySet for changes. This function handles most basic
	 * properties, as well as Struct and Array properties, at some cost. The OnModified
	 * delegate cannot be passed the new property value, it must be fetched from the PropertySet.
	 * 
	 * Note also that in the case of Structs and Arrays, changes are currently detected by hashes,
	 * and there is always a small chance of hash collision.
	 * 
	 * @param PropertySet the Property Set which contains the desired Property to watch.
	 * @param PropertyName the string name of the Property in the given Property Set
	 * @param OnModified this delegate will be called if the Property value changes.
	 */
	UFUNCTION(BlueprintCallable, Category = "ScriptableTool|PropertySets")
	UPARAM(DisplayName="Property Set") UScriptableInteractiveToolPropertySet* WatchProperty(
		UScriptableInteractiveToolPropertySet* PropertySet,
		FString PropertyName,
		const FToolPropertyModifiedDelegate& OnModified );

protected:
	TMap<FString, TWeakObjectPtr<UScriptableInteractiveToolPropertySet>> NamedPropertySets;

	TArray<FToolFloatPropertyModifiedDelegate> WatchFloatPropertyDelegates;
	TArray<FToolIntPropertyModifiedDelegate> WatchIntPropertyDelegates;
	TArray<FToolBoolPropertyModifiedDelegate> WatchBoolPropertyDelegates;
	TArray<FToolEnumPropertyModifiedDelegate> WatchEnumPropertyDelegates;
	TArray<FToolStringPropertyModifiedDelegate> WatchStringPropertyDelegates;
	TArray<FToolFNamePropertyModifiedDelegate> WatchFNamePropertyDelegates;
	TArray<FToolObjectPropertyModifiedDelegate> WatchObjectPropertyDelegates;


	enum class EAnyPropertyWatchTypes
	{
		Integer = 0,
		Bool = 1,
		Double = 2,
		Enum = 3,
		String = 4,
		FName = 5,
		Object = 6,

		Struct = 20,
		Array = 30,


		Unknown = 100
	};
	struct FAnyPropertyWatchInfo
	{
		EAnyPropertyWatchTypes KnownType = EAnyPropertyWatchTypes::Unknown;
		TArray<uint8> TempBuffer;
		FToolPropertyModifiedDelegate Delegate;
	};
	TArray<FAnyPropertyWatchInfo> WatchAnyPropertyInfo;

	// gizmo API
	//    - option to register delegate for specific gizmo?
	//    - some way to easily connect gizmo transform to a FTransform property?
public:

	/**
	 * Create a Translate/Rotate/Scale Gizmo with the given Options at the specified InitialTransform.
	 * The Gizmo must be given a unique Identifier, which will be used to access it in other functions.
	 * The Gizmo can be explicitly destroyed with DestroyTRSGizmo(), or it will be
	 * automatically destroyed when the Tool exits
	 */
	UFUNCTION(BlueprintCallable, Category = "ScriptableTool|Gizmos", Meta = (ExpandEnumAsExecs = "Outcome", Identifier="Gizmo1"))
	void CreateTRSGizmo(
		FString Identifier,
		FTransform InitialTransform,
		FScriptableToolGizmoOptions GizmoOptions,
		EToolsFrameworkOutcomePins& Outcome);

	/**
	 * Destroy a created Gizmo by name Identifier
	 */
	UFUNCTION(BlueprintCallable, Category = "ScriptableTool|Gizmos", Meta = (ExpandEnumAsExecs = "Outcome", Identifier="Gizmo1"))
	void DestroyTRSGizmo(
		FString Identifier,
		EToolsFrameworkOutcomePins& Outcome);

	/**
	 * Set an existing Gizmo visible/hidden based on its name Identifier
	 */
	UFUNCTION(BlueprintCallable, Category = "ScriptableTool|Gizmos", Meta = (Identifier="Gizmo1") )
	void SetGizmoVisible(
		FString Identifier,
		bool bVisible);

	/**
	 * Update the Transform on the Gizmo specified by the name Identifier
	 * @param bUndoable if true, this transform change will be transacted into the undo/redo history, ie undoable
	 */
	UFUNCTION(BlueprintCallable, Category = "ScriptableTool|Gizmos", Meta = (Identifier="Gizmo1") )
	void SetGizmoTransform(
		FString Identifier,
		FTransform NewTransform,
		bool bUndoable );

	/**
	 * Get the current Transform on the Gizmo specified by the name Identifier
	 */
	UFUNCTION(BlueprintCallable, Category = "ScriptableTool|Gizmos", Meta = (Identifier="Gizmo1") )
	UPARAM(DisplayName="Transform") FTransform 
	GetGizmoTransform( FString Identifier);

	/**
	 * The OnGizmoTransformChanged event fires whenever the transform on any Gizmo created by CreateTRSGizmo() is modified.
	 * The GizmoIdentifier can be used to disambiguate multiple active Gizmos.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ScriptableTool|Events")
	void OnGizmoTransformChanged(const FString& GizmoIdentifier, FTransform NewTransform);

	/**
	 * The OnGizmoTransformStateChange event fires whenever the user start/ends a Gizmo transform, or when an Undo/Redo event occurs.
	 * Note that when Undo/Redo occurs, OnGizmoTransformChanged will also fire.
	 * The GizmoIdentifier can be used to disambiguate multiple active Gizmos.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ScriptableTool|Events")
	void OnGizmoTransformStateChange(const FString& GizmoIdentifier, FTransform CurrentTransform, EScriptableToolGizmoStateChangeType ChangeType);


protected:
	// trying to avoid making a UStruct for this internal stuff
	UPROPERTY(Transient, DuplicateTransient, NonTransactional, SkipSerialization)
	TMap<FString, TObjectPtr<UCombinedTransformGizmo>> Gizmos;

	virtual void OnGizmoTransformChanged_Handler(FString GizmoIdentifier, FTransform NewTransform);
	virtual void OnGizmoTransformStateChange_Handler(FString GizmoIdentifier, FTransform CurrentTransform, EScriptableToolGizmoStateChangeType ChangeType);




public:
	// message API

	/**
	 * Append a Message to the UE Editor Log. 
	 * @param bHighlighted if true, the message is emitted as a Warning, otherwise as a Log (normal)
	 */
	UFUNCTION(BlueprintCallable, Category = "ScriptableTool|Messaging")
	void AddLogMessage( FText Message, bool bHighlighted = false );

	/**
	 * Display a short Help message for the user, ie to guide them in Tool usage.
	 * In the UE5 Editor this message appears in the bottom bar of the window.
	 */
	UFUNCTION(BlueprintCallable, Category = "ScriptableTool|Messaging")
	void DisplayUserHelpMessage( FText Message );

	/**
	 * Display a Warning message to the user, ie to indicate a problem/issue occurred. 
	 */
	UFUNCTION(BlueprintCallable, Category = "ScriptableTool|Messaging")
	void DisplayUserWarningMessage( FText Message );

	/**
	 * Clear any active message shown via DisplayUserHelpMessage and/or DisplayUserWarningMessage
	 */
	UFUNCTION(BlueprintCallable, Category = "ScriptableTool|Messaging")
	void ClearUserMessages( bool bHelpMessage = true, bool bWarningMessage = true );


public:

	// TODO: hotkey API
};






UCLASS(meta = (ScriptName = "ScriptableTools_Util"))
class SCRIPTABLETOOLSFRAMEWORK_API UScriptableToolsUtilityLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintPure, Category="ScriptableTool|Util")
	static UPARAM(DisplayName = "RayHit Result") FInputRayHit 
	MakeInputRayHit_Miss();

	UFUNCTION(BlueprintPure, Category="ScriptableTool|Util")
	static UPARAM(DisplayName = "RayHit Result") FInputRayHit 
	MakeInputRayHit_MaxDepth();

	UFUNCTION(BlueprintPure, Category="ScriptableTool|Util")
	static UPARAM(DisplayName = "RayHit Result") FInputRayHit 
	MakeInputRayHit(double HitDepth, UObject* OptionalHitObject);

};