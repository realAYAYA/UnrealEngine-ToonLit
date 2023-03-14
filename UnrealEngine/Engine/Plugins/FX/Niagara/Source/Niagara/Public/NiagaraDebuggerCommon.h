// Copyright Epic Games, Inc. All Rights Reserved.

/*==============================================================================
All common code shared between the editor side debugger and debugger clients running in game.
==============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Misc/NotifyHook.h"
#include "NiagaraTypes.h"
#include "NiagaraCommon.h"
#include "NiagaraSimCache.h"
#include "NiagaraDebuggerCommon.generated.h"

//////////////////////////////////////////////////////////////////////////
// Niagara Outliner.

USTRUCT()
struct FNiagaraOutlinerTimingData
{
	GENERATED_BODY()

	/** Game thread time, including concurrent tasks*/
	UPROPERTY(VisibleAnywhere, Category="Time")
	float GameThread = 0.0f;

	/** Render thread time. */
	UPROPERTY(VisibleAnywhere, Category="Time")
	float RenderThread = 0.0f;
};

USTRUCT()
struct FNiagaraOutlinerEmitterInstanceData
{
	GENERATED_BODY()

	//Name of this emitter.
	UPROPERTY(VisibleAnywhere, Category = "Emitter")
	FString EmitterName; //TODO: Move to shared asset representation.

	UPROPERTY(VisibleAnywhere, Category = "Emitter")
	ENiagaraSimTarget SimTarget = ENiagaraSimTarget::CPUSim; //TODO: Move to shared asset representation.

	UPROPERTY(VisibleAnywhere, Category = "Emitter")
	ENiagaraExecutionState ExecState = ENiagaraExecutionState::Num;
	
	UPROPERTY(VisibleAnywhere, Category = "Emitter")
	int32 NumParticles = 0;

	UPROPERTY(VisibleAnywhere, Category = "Emitter")
	uint32 bRequiresPersistentIDs : 1;

	FNiagaraOutlinerEmitterInstanceData()
		: bRequiresPersistentIDs(false)
	{}

	//Mem Usage?
	//Scalability info?
};

/** Outliner information on a specific system instance. */
USTRUCT()
struct FNiagaraOutlinerSystemInstanceData
{
	GENERATED_BODY()

	/** Name of the component object for this instance, if there is one. */
	UPROPERTY(VisibleAnywhere, Category = "System")
	FString ComponentName;

	UPROPERTY(VisibleAnywhere, Category = "System")
	FVector3f LWCTile = FVector3f::Zero();

	UPROPERTY(VisibleAnywhere, Category = "System")
	TArray<FNiagaraOutlinerEmitterInstanceData> Emitters;
	
	UPROPERTY(VisibleAnywhere, Category = "State")
	ENiagaraExecutionState ActualExecutionState = ENiagaraExecutionState::Num;

	UPROPERTY(VisibleAnywhere, Category = "State")
	ENiagaraExecutionState RequestedExecutionState = ENiagaraExecutionState::Num;
	
	UPROPERTY(VisibleAnywhere, Category = "State")
	FNiagaraScalabilityState ScalabilityState;
	
	UPROPERTY(VisibleAnywhere, Category = "State")
	uint32 bPendingKill : 1;

	UPROPERTY(VisibleAnywhere, Category = "State")
	uint32 bUsingCullProxy : 1;
	
	UPROPERTY(VisibleAnywhere, Category = "State")
	ENCPoolMethod PoolMethod = ENCPoolMethod::None;

	UPROPERTY(VisibleAnywhere, Category = "Performance")
	FNiagaraOutlinerTimingData AverageTime;

	UPROPERTY(VisibleAnywhere, Category = "Performance")
	FNiagaraOutlinerTimingData MaxTime;

	UPROPERTY(VisibleAnywhere, Category = "Ticking")
	TEnumAsByte<ETickingGroup> TickGroup;

	UPROPERTY(VisibleAnywhere, Category = "Ticking")
	TEnumAsByte<ENiagaraGpuComputeTickStage::Type> GpuTickStage;

	UPROPERTY(VisibleAnywhere, Category = "Gpu")
	uint32 bIsSolo : 1;

	UPROPERTY(VisibleAnywhere, Category = "Gpu")
	uint32 bRequiresDistanceFieldData : 1;

	UPROPERTY(VisibleAnywhere, Category = "Gpu")
	uint32 bRequiresDepthBuffer : 1;

	UPROPERTY(VisibleAnywhere, Category = "Gpu")
	uint32 bRequiresEarlyViewData : 1;

	UPROPERTY(VisibleAnywhere, Category = "Gpu")
	uint32 bRequiresViewUniformBuffer : 1;

	UPROPERTY(VisibleAnywhere, Category = "Gpu")
	uint32 bRequiresRayTracingScene : 1;

	FNiagaraOutlinerSystemInstanceData()
		: bPendingKill(false)
		, bUsingCullProxy(false)
		, TickGroup(0)
		, GpuTickStage(ENiagaraGpuComputeTickStage::First)
		, bIsSolo(false)
		, bRequiresDistanceFieldData(false)
		, bRequiresDepthBuffer(false)
		, bRequiresEarlyViewData(false)
		, bRequiresViewUniformBuffer(false)
		, bRequiresRayTracingScene(false)
	{}
};

/** Wrapper for array of system instance outliner data so that it can be placed in a map. */
USTRUCT()
struct FNiagaraOutlinerSystemData
{
	GENERATED_BODY()

	//TODO: Cache off any shared representation of the system and emitters here for the instances to reference. 

	/** Map of System Instance data indexed by the UNiagaraSystem name. */
	UPROPERTY(VisibleAnywhere, Category = "System")
	TArray<FNiagaraOutlinerSystemInstanceData> SystemInstances;

	
	
	UPROPERTY(VisibleAnywhere, Category = "Performance")
	FNiagaraOutlinerTimingData AveragePerFrameTime;

	UPROPERTY(VisibleAnywhere, Category = "Performance")
	FNiagaraOutlinerTimingData MaxPerFrameTime;
	
	UPROPERTY(VisibleAnywhere, Category = "Performance")
	FNiagaraOutlinerTimingData AveragePerInstanceTime;

	UPROPERTY(VisibleAnywhere, Category = "Performance")
	FNiagaraOutlinerTimingData MaxPerInstanceTime;
};

/** All information about a specific world for the Niagara Outliner. */
USTRUCT()
struct FNiagaraOutlinerWorldData
{
	GENERATED_BODY()

	/** Map of System Instance data indexed by the UNiagaraSystem name. */
	UPROPERTY(VisibleAnywhere, Category = "World")
	TMap<FString, FNiagaraOutlinerSystemData> Systems;

	UPROPERTY(VisibleAnywhere, Category = "State")
	bool bHasBegunPlay = false;

	UPROPERTY(VisibleAnywhere, Category = "State")
	uint8 WorldType = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, Category = "State")
	uint8 NetMode = INDEX_NONE;
	

	UPROPERTY(VisibleAnywhere, Category = "Performance")
	FNiagaraOutlinerTimingData AveragePerFrameTime;

	UPROPERTY(VisibleAnywhere, Category = "Performance")
	FNiagaraOutlinerTimingData MaxPerFrameTime;

	//Mem Usage?
};

USTRUCT()
struct FNiagaraOutlinerData
{
	GENERATED_BODY()

	/** Map all world data indexed by the world name. */
	UPROPERTY(VisibleAnywhere, Category = "Outliner")
	TMap<FString, FNiagaraOutlinerWorldData> WorldData;
};

// Outliner END
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// Messages passed between the editor side debugger and the client. 

/** 
Messaged broadcast from debugger to request a connection to a particular session. 
If any matching client is found and it accepts, it will return a FNiagaraDebuggerAcceptConnection message to the sender. 
*/
USTRUCT()
struct NIAGARA_API FNiagaraDebuggerRequestConnection
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Message")
	FGuid SessionId;

	UPROPERTY(EditAnywhere, Category = "Message")
	FGuid InstanceId;

	FNiagaraDebuggerRequestConnection() { }

	FNiagaraDebuggerRequestConnection(const FGuid& InSessionId, const FGuid& InInstanceId)
		: SessionId(InSessionId)
		, InstanceId(InInstanceId)
	{}
};

/** Response message from the a debugger client accepting a connection requested by a FNiagaraDebuggerRequestConnection message. */
USTRUCT()
struct NIAGARA_API FNiagaraDebuggerAcceptConnection
{
	GENERATED_BODY()
		
	UPROPERTY(EditAnywhere, Category = "Message")
	FGuid SessionId;

	UPROPERTY(EditAnywhere, Category = "Message")
	FGuid InstanceId;

	FNiagaraDebuggerAcceptConnection() { }
	
	FNiagaraDebuggerAcceptConnection(const FGuid& InSessionId, const FGuid& InInstanceId)
		: SessionId(InSessionId)
		, InstanceId(InInstanceId)
	{}
};

/** Empty message informing a debugger client that the debugger is closing the connection. */
USTRUCT()
struct NIAGARA_API FNiagaraDebuggerConnectionClosed
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Message")
	FGuid SessionId;

	UPROPERTY(EditAnywhere, Category = "Message")
	FGuid InstanceId;

	FNiagaraDebuggerConnectionClosed() { }
	
	FNiagaraDebuggerConnectionClosed(const FGuid& InSessionId, const FGuid& InInstanceId)
		: SessionId(InSessionId)
		, InstanceId(InInstanceId)
	{}
};

/** Command that will execute a console command on the debugger client. */
USTRUCT() 
struct NIAGARA_API FNiagaraDebuggerExecuteConsoleCommand
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Message")
	FString Command;

	UPROPERTY()
	bool bRequiresWorld = false;

	FNiagaraDebuggerExecuteConsoleCommand() { }
	
	FNiagaraDebuggerExecuteConsoleCommand(FString InCommand, bool bInRequiresWorld)
		: Command(InCommand)
		, bRequiresWorld(bInRequiresWorld)
	{}
};

/** Message containing updated outliner information sent from the client to the debugger. */
USTRUCT()
struct NIAGARA_API FNiagaraDebuggerOutlinerUpdate
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Message")
	FNiagaraOutlinerData OutlinerData;

	FNiagaraDebuggerOutlinerUpdate() { }
};

// End of messages.
//////////////////////////////////////////////////////////////////////////

UENUM()
enum class ENiagaraDebugPlaybackMode : uint8
{
	Play = 0,
	Loop,
	Paused,
	Step,
};

UENUM()
enum class ENiagaraDebugHudHAlign : uint8
{
	Left,
	Center,
	Right
};

UENUM()
enum class ENiagaraDebugHudVAlign : uint8
{
	Top,
	Center,
	Bottom
};

UENUM()
enum class ENiagaraDebugHudFont
{
	Small = 0,
	Normal,
};

UENUM()
enum class ENiagaraDebugHudVerbosity
{
	None,
	Basic,
	Verbose,
};

USTRUCT()
struct FNiagaraDebugHudTextOptions
{
	GENERATED_BODY()
		
	UPROPERTY(Config, EditAnywhere, Category = "Text Options")
	ENiagaraDebugHudFont Font = ENiagaraDebugHudFont::Small;

	UPROPERTY(EditAnywhere, Category = "Text Options")
	ENiagaraDebugHudHAlign	HorizontalAlignment = ENiagaraDebugHudHAlign::Left;

	UPROPERTY(EditAnywhere, Category = "Text Options")
	ENiagaraDebugHudVAlign	VerticalAlignment = ENiagaraDebugHudVAlign::Top;

	UPROPERTY(EditAnywhere, Category = "Text Options")
	FVector2D ScreenOffset = FVector2D::ZeroVector;
};

USTRUCT()
struct NIAGARA_API FNiagaraDebugHUDVariable
{
	GENERATED_BODY()
		
	UPROPERTY(EditAnywhere, Category = "Attribute")
	bool bEnabled = true;

	/** Name of attributes to match, uses wildcard matching. */
	UPROPERTY(EditAnywhere, Category = "Attribute")
	FString Name;

	static FString BuildVariableString(const TArray<FNiagaraDebugHUDVariable>& Variables);
	static void InitFromString(const FString& VariablesString, TArray<FNiagaraDebugHUDVariable>& OutVariables);
};

UENUM()
enum class ENiagaraDebugHUDOverviewMode
{
	Overview,
	Scalability,	
	Performance,
	GpuComputePerformance,
}; 

UENUM()
enum class ENiagaraDebugHUDPerfGraphMode
{
	None,
	GameThread,
	RenderThread,
	GPU,
}; 

UENUM()
enum class ENiagaraDebugHUDPerfSampleMode
{
	FrameTotal,
	PerInstanceAverage,	
};

/** Settings for Niagara debug HUD. Contained in it's own struct so that we can pass it whole in a message to the debugger client. */
USTRUCT()
struct NIAGARA_API FNiagaraDebugHUDSettingsData
{
	GENERATED_BODY()

	FNiagaraDebugHUDSettingsData();

	bool IsEnabled() const
	{
	#if WITH_EDITORONLY_DATA
		return bWidgetEnabled && bHudEnabled;
	#else
		return bHudEnabled;
	#endif
	}

#if WITH_EDITORONLY_DATA
	UPROPERTY(transient)
	bool bWidgetEnabled = false;
#endif

	/** Primary control for all HUD features. */
	UPROPERTY(EditAnywhere, Category = "Debug General", meta = (DisplayName = "Debug HUD Enabled"))
	bool bHudEnabled = true;

	/** Primary control for HUD rendering. */
	UPROPERTY(EditAnywhere, Category = "Debug General", meta = (DisplayName = "Debug HUD Rendering Enabled"))
	bool bHudRenderingEnabled = true;

	/**
	When enabled all Niagara systems that pass the filter will have the simulation data buffers validation.
	i.e. we will look for NaN or other invalidate data  inside it
	Note: This will have an impact on performance.
	*/
	UPROPERTY(EditAnywhere, Category = "Debug General")
	bool bValidateSystemSimulationDataBuffers = false;

	/**
	When enabled all Niagara systems that pass the filter will have the particle data buffers validation.
	i.e. we will look for NaN or other invalidate data  inside it
	Note: This will have an impact on performance.
	*/
	UPROPERTY(EditAnywhere, Category = "Debug General")
	bool bValidateParticleDataBuffers = false;

	/** When enabled the overview display will be enabled. */
	UPROPERTY(EditAnywhere, Category = "Debug Overview", meta = (DisplayName = "Debug Overview Enabled"))
	bool bOverviewEnabled = false;

	UPROPERTY(EditAnywhere, Category = "Debug Overview", meta = (DisplayName = "Debug Overview Mode"))
	ENiagaraDebugHUDOverviewMode OverviewMode = ENiagaraDebugHUDOverviewMode::Overview;

	/** Overview display font to use. */
	UPROPERTY(EditAnywhere, Category = "Debug Overview", meta = (DisplayName = "Debug Overview Font", EditCondition = "bOverviewEnabled"))
	ENiagaraDebugHudFont OverviewFont = ENiagaraDebugHudFont::Normal;

	/** Overview display location. */
	UPROPERTY(EditAnywhere, Category = "Debug Overview", meta = (DisplayName = "Debug Overview Text Location", EditCondition = "bOverviewEnabled"))
	FVector2D OverviewLocation = FIntPoint(30.0f, 150.0f);

	/** Overview display font to use. */
	UPROPERTY(EditAnywhere, Category = "Debug Overview", meta = (EditCondition = "bOverviewEnabled && OverviewMode == ENiagaraDebugHUDOverviewMode::Overview"))
	bool bShowRegisteredComponents = false;
	
	/** When enabled the overview will only show the filter system information. */
	UPROPERTY(EditAnywhere, Category = "Debug Overview", meta = (EditCondition = "bOverviewEnabled"))
	bool bOverviewShowFilteredSystemOnly = false;

	/**
	Wildcard filter which is compared against the Components Actor name to narrow down the detailed information.
	For example, "*Water*" would match all actors that contain the string "water".
	*/
	UPROPERTY(Config, EditAnywhere, Category = "Debug Filter", meta = (EditCondition = "bActorFilterEnabled"))
	FString ActorFilter;

	UPROPERTY(Config, EditAnywhere, Category = "Debug Filter", meta = (InlineEditConditionToggle))
	bool bComponentFilterEnabled = false;

	/**
	Wildcard filter for the components to show more detailed information about.
	For example, "*MyComp*" would match all components that contain MyComp.
	*/
	UPROPERTY(Config, EditAnywhere, Category = "Debug Filter", meta = (EditCondition = "bComponentFilterEnabled"))
	FString ComponentFilter;

	UPROPERTY(Config, EditAnywhere, Category = "Debug Filter", meta = (InlineEditConditionToggle))
	bool bSystemFilterEnabled = false;

	/**
	Wildcard filter for the systems to show more detailed information about.
	For example,. "NS_*" would match all systems starting with NS_.
	*/
	UPROPERTY(Config, EditAnywhere, Category = "Debug Filter", meta = (EditCondition = "bSystemFilterEnabled"))
	FString SystemFilter;

	UPROPERTY(Config, EditAnywhere, Category = "Debug Filter", meta = (InlineEditConditionToggle))
	bool bEmitterFilterEnabled = false;

	/**
	Wildcard filter used to match emitters when generating particle attribute view.
	For example,. "Fluid*" would match all emtiters starting with Fluid and only particle attributes for those would be visible.
	*/
	UPROPERTY(Config, EditAnywhere, Category = "Debug Filter", meta = (EditCondition = "bEmitterFilterEnabled"))
	FString EmitterFilter;

	UPROPERTY(Config, EditAnywhere, Category = "Debug Filter", meta = (InlineEditConditionToggle))
	bool bActorFilterEnabled = false;

	/** When enabled system debug information will be displayed in world. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug System")
	ENiagaraDebugHudVerbosity SystemDebugVerbosity = ENiagaraDebugHudVerbosity::Basic;

	/** When enabled we show information about emitter / particle counts. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug System", meta = (EditCondition = "SystemDebugVerbosity != ENiagaraDebugHudVerbosity::None"))
	ENiagaraDebugHudVerbosity SystemEmitterVerbosity = ENiagaraDebugHudVerbosity::Basic;

	/** When enabled allows data interfaces to include additional debugging information. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug System", meta = (EditCondition = "SystemDebugVerbosity != ENiagaraDebugHudVerbosity::None"))
	ENiagaraDebugHudVerbosity DataInterfaceVerbosity = ENiagaraDebugHudVerbosity::None;

	/** When enabled will show the system bounds for all filtered systems. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug System")
	bool bSystemShowBounds = false;

	/** When disabled in world rendering will show systems deactivated by scalability. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug System", meta = (EditCondition = "SystemDebugVerbosity != ENiagaraDebugHudVerbosity::None"))
	bool bSystemShowActiveOnlyInWorld = true;

	/** Should we display the system attributes. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug System", meta = (EditCondition = "SystemDebugVerbosity != ENiagaraDebugHudVerbosity::None", DisplayName="Show System Attributes"))
	bool bShowSystemVariables = true;

	/**
	List of attributes to show about the system, each entry uses wildcard matching.
	For example, "System.*" would match all system attributes.
	*/
	UPROPERTY(Config, EditAnywhere, Category = "Debug System", meta = (EditCondition = "SystemDebugVerbosity != ENiagaraDebugHudVerbosity::None && bShowSystemVariables", DisplayName="System Attributes"))
	TArray<FNiagaraDebugHUDVariable> SystemVariables;

	/** Sets display text options for system information. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug System", meta=(EditCondition="SystemDebugVerbosity != ENiagaraDebugHudVerbosity::None"))
	FNiagaraDebugHudTextOptions SystemTextOptions;

	/** When enabled will show particle attributes from the list. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug Particles", meta = (DisplayName="Show Particle Attributes"))
	bool bShowParticleVariables = true;

	/**
	When enabled GPU particle data will be copied from the GPU to the CPU.
	Warning: This has an impact on performance & memory since we copy the whole buffer.
	The displayed data is latent since we are seeing what happened a few frames ago.
	*/
	UPROPERTY(Config, EditAnywhere, Category = "Debug Particles", meta = (EditCondition = "bShowParticleVariables"))
	bool bEnableGpuParticleReadback = false;

	/**
	When enabled the particle index will be displayed along with any attributes.
	Note: This is the index in the particle data buffer and not the persistent ID index.
	*/
	UPROPERTY(Config, EditAnywhere, Category = "Debug Particles", meta = (EditCondition = "bShowParticleVariables"))
	bool bShowParticleIndex = false;

	/**
	List of attributes to show per particle, each entry uses wildcard matching.
	For example, "*Position" would match all attributes that end in Position.
	*/
	UPROPERTY(Config, EditAnywhere, Category = "Debug Particles", meta = (EditCondition = "bShowParticleVariables", DisplayName="Particle Attributes"))
	TArray<FNiagaraDebugHUDVariable> ParticlesVariables;

	/** Sets display text options for particle information. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug Particles", meta = (EditCondition = "bShowParticleVariables"))
	FNiagaraDebugHudTextOptions ParticleTextOptions;

	/**
	When enabled particle attributes will display with the system information
	rather than in world at the particle location.
	*/
	UPROPERTY(Config, EditAnywhere, Category = "Debug Particles", meta = (EditCondition = "bShowParticleVariables", DisplayName="Show Particles Attributes With System"))
	bool bShowParticlesVariablesWithSystem = false;

	UPROPERTY(Config, EditAnywhere, Category = "Debug Particles", meta = (EditCondition = "bShowParticleVariables", DisplayName = "Show Particle Attributes Vertical"))
	bool bShowParticleVariablesVertical = false;

	UPROPERTY(Config, EditAnywhere, Category = "Debug Particles", meta = (EditCondition = "bShowParticleVariables"))
	bool bUseMaxParticlesToDisplay = true;

	/** When enabled we use the clip planes to narrow down which particles to display */
	UPROPERTY(Config, EditAnywhere, Category = "Debug Particles", meta = (EditCondition = "bShowParticleVariables"))
	bool bUseParticleDisplayClip = false;

	/** Clipping planes used to display particle attributes. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug Particles", meta = (EditCondition = "bShowParticleVariables", UIMin = "0", ClampMin = "0"))
	FVector2D ParticleDisplayClip = FVector2D(0.0f, 10000.0f);

	/** When enabled we use a radius from the display center to avoid showing too many particle attributes. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug Particles", meta = (EditCondition = "bShowParticleVariables"))
	bool bUseParticleDisplayCenterRadius = false;

	/** Radius from screen center where 0 is center to 1.0 is edge to avoid display too many particle attributes. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug Particles", meta = (EditCondition = "bShowParticleVariables", UIMin = "0", ClampMin = "0"))
	float ParticleDisplayCenterRadius = 1.0f;

	/**
	When enabled, the maximum number of particles to show information about.
	When disabled all particles will show attributes, this can result in poor performance & potential OOM on some platforms.
	*/
	UPROPERTY(Config, EditAnywhere, Category = "Debug Particles", meta = (EditCondition = "bUseMaxParticlesToDisplay && bShowParticleVariables", UIMin="1", ClampMin="1"))
	int32 MaxParticlesToDisplay = 32;

	/** How many frames to capture in between updates to the max and average perf report values. */
	UPROPERTY(Config, EditAnywhere, Category = "Perf Overview")
	int32 PerfReportFrames = 60;

	UPROPERTY(Config, EditAnywhere, Category = "Perf Overview")
	ENiagaraDebugHUDPerfSampleMode PerfSampleMode = ENiagaraDebugHUDPerfSampleMode::FrameTotal;

	/** Time range of the Y Axis of the perf graph */
	UPROPERTY(Config, EditAnywhere, Category = "Perf Overview")
	ENiagaraDebugHUDPerfGraphMode PerfGraphMode = ENiagaraDebugHUDPerfGraphMode::GameThread;

	/** How many frames of history to display in the perf graphs. */
	UPROPERTY(Config, EditAnywhere, Category = "Perf Overview")
	int32 PerfHistoryFrames = 600;

	/** Time range of the Y Axis of the perf graph */
	UPROPERTY(Config, EditAnywhere, Category = "Perf Overview")
	float PerfGraphTimeRange = 500.0f;

	/** Pixel size of the perf graph. */
	UPROPERTY(Config, EditAnywhere, Category = "Perf Overview")
	FVector2D PerfGraphSize = FVector2D(500,500);

	UPROPERTY(Config, EditAnywhere, Category = "Perf Overview")
	FLinearColor PerfGraphAxisColor = FLinearColor::White;

	// True if perf graphs should be smoothed.
	UPROPERTY(Config, EditAnywhere, Category = "Perf Overview", meta = (InlineEditConditionToggle))
	bool bEnableSmoothing = true;

	//Number of samples to use either size of a value when smoothing perf graphs.
	UPROPERTY(Config, EditAnywhere, Category = "Perf Overview", meta = (EditCondition = "bEnableSmoothing"))
	int32 SmoothingWidth = 4;

	// Default background color used generally for panels
	UPROPERTY(EditAnywhere, Category = "Colors")
	FLinearColor DefaultBackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.5f);

	// Overview heading text color
	UPROPERTY(EditAnywhere, Category = "Colors")
	FLinearColor OverviewHeadingColor = FLinearColor::Green;

	// Overview detail text color
	UPROPERTY(EditAnywhere, Category = "Colors")
	FLinearColor OverviewDetailColor = FLinearColor::White;

	// Overview detail highlight text color
	UPROPERTY(EditAnywhere, Category = "Colors")
	FLinearColor OverviewDetailHighlightColor = FLinearColor::Yellow;

	// In world text if an error is detected
	UPROPERTY(EditAnywhere, Category = "Colors")
	FLinearColor InWorldErrorTextColor = FLinearColor(1.0f, 0.4, 0.3, 1.0f);
	// In world text color
	UPROPERTY(EditAnywhere, Category = "Colors")
	FLinearColor InWorldTextColor = FLinearColor::White;

	// Message display text color
	UPROPERTY(EditAnywhere, Category = "Colors")
	FLinearColor MessageInfoTextColor = FLinearColor::White;

	// Message display warning text color
	UPROPERTY(EditAnywhere, Category = "Colors")
	FLinearColor MessageWarningTextColor = FLinearColor(0.9f, 0.7f, 0.0, 1.0f);

	// Message display error text color
	UPROPERTY(EditAnywhere, Category = "Colors")
	FLinearColor MessageErrorTextColor = FLinearColor(1.0f, 0.4, 0.3, 1.0f);
	
	/** Opacity of the system color background tile in overview table rows. */
	UPROPERTY(Config, EditAnywhere, Category = "Colors")
	float SystemColorTableOpacity = 0.2f;

	/** Additional seed value for random system colors. Useful if current colors of systems are too similar. */
	UPROPERTY(Config, EditAnywhere, Category = "Colors")
	uint32 SystemColorSeed = 0;

	/** Minimum HSV values for the random colors generated for each System. */
	UPROPERTY(Config, EditAnywhere, Category = "Colors")
	FVector SystemColorHSVMin = FVector(0, 200, 200);

	/** Maximum HSV values for the random colors generated for each System. */
	UPROPERTY(Config, EditAnywhere, Category = "Colors")
	FVector SystemColorHSVMax = FVector(255, 255, 255);

	UPROPERTY()
	ENiagaraDebugPlaybackMode PlaybackMode = ENiagaraDebugPlaybackMode::Play;

	UPROPERTY()
	bool bPlaybackRateEnabled = false;

	UPROPERTY(Config)
	float PlaybackRate = 0.25f;

	UPROPERTY(Config)
	bool bLoopTimeEnabled = false;

	UPROPERTY(Config)
	float LoopTime = 1.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Performance")
	bool bShowGlobalBudgetInfo = false;
};

/** Message passed from debugger to client when it needs updated simple client info. */
USTRUCT()
struct NIAGARA_API FNiagaraRequestSimpleClientInfoMessage
{
	GENERATED_BODY()
};

UCLASS(config = EditorPerProjectUserSettings, defaultconfig)
class NIAGARA_API UNiagaraDebugHUDSettings : public UObject, public FNotifyHook
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE(FOnChanged);
	FOnChanged OnChangedDelegate;

	UPROPERTY(Config, EditAnywhere, Category = "Settings", meta=(ShowOnlyInnerProperties))
	FNiagaraDebugHUDSettingsData Data;

	void NotifyPropertyChanged();
	virtual void NotifyPreChange(FProperty*) {}
	virtual void NotifyPostChange(const FPropertyChangedEvent&, FProperty*) { NotifyPropertyChanged(); }
	virtual void NotifyPreChange(class FEditPropertyChain*) {}
	virtual void NotifyPostChange(const FPropertyChangedEvent&, class FEditPropertyChain*) { NotifyPropertyChanged(); }
};


USTRUCT()
struct NIAGARA_API FNiagaraOutlinerCaptureSettings
{
	GENERATED_BODY()

	/** Press to trigger a single capture of Niagara data from the connected debugger client. */
	UPROPERTY(EditAnywhere, Category = "Settings")
	bool bTriggerCapture = false;

	/** How many frames to delay capture. If gathering performance data, this is how many frames will be collected. */
	UPROPERTY(EditAnywhere, Config, Category="Settings")
	uint32 CaptureDelayFrames = 60;
	
	UPROPERTY(EditAnywhere, Config, Category = "Settings")
	bool bGatherPerfData = true;

	/** How many frames capture when capturing a sim cache. */
	UPROPERTY(EditAnywhere, Config, Category = "Settings")
	uint32 SimCacheCaptureFrames = 10;
};

/** Simple information on the connected client for use in continuous or immediate response UI elements. */
USTRUCT()
struct NIAGARA_API FNiagaraSimpleClientInfo
{
	GENERATED_BODY()

	/** List of all system names in the scene. */
	UPROPERTY(EditAnywhere, Category = "Info")
	TArray<FString> Systems;

	/** List of all actors with Niagara components. */
	UPROPERTY(EditAnywhere, Category = "Info")
	TArray<FString> Actors;
	
	/** List of all Niagara components. */
	UPROPERTY(EditAnywhere, Category = "Info")
	TArray<FString> Components;
	
	/** List of all Niagara emitters. */
	UPROPERTY(EditAnywhere, Category = "Info")
	TArray<FString> Emitters;
};

/** Message sent from the debugger to a client to request a sim cache capture for a particular component. */
USTRUCT()
struct NIAGARA_API FNiagaraSystemSimCacheCaptureRequest
{
	GENERATED_BODY()

	/** Name of the component we're going to capture. */
	UPROPERTY(EditAnywhere, Category = "Settings")
	FName ComponentName;

	/** How many frames to delay capture. */
	UPROPERTY(EditAnywhere, Config, Category="Settings")
	uint32 CaptureDelayFrames = 60;

	/** How many frames to capture. */
	UPROPERTY(EditAnywhere, Config, Category = "Settings")
	uint32 CaptureFrames = 10;
};


/** Message sent from a debugger client to a connected debugger containing the results of a sim cache capture. */
USTRUCT()
struct NIAGARA_API FNiagaraSystemSimCacheCaptureReply
{
	GENERATED_BODY()

	/** Name of the captured component. */
	UPROPERTY()
	FName ComponentName;
	
	UPROPERTY()
	TArray<uint8> SimCacheData;
};

enum class ENiagaraDebugMessageType : uint8
{
	Info,
	Warning,
	Error
};
struct FNiagaraDebugMessage
{
	ENiagaraDebugMessageType Type;
	FString Message;
	float Lifetime;
	FNiagaraDebugMessage()
		: Type(ENiagaraDebugMessageType::Error)
		, Lifetime(0.0f)
	{}
	FNiagaraDebugMessage(ENiagaraDebugMessageType InType, const FString& InMessage, float InLifetime)
		: Type(InType)
		, Message(InMessage)
		, Lifetime(InLifetime)
	{}
};