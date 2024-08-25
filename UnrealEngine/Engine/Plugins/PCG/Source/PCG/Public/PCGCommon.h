// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/IConsoleManager.h"

#include "PCGCommon.generated.h"

using FPCGTaskId = uint64;
static const FPCGTaskId InvalidPCGTaskId = (uint64)-1;

using FPCGPinId = uint64;

namespace PCGPinIdHelpers
{
	/** Pin active bitmask stored in uint64, so 64 flags available. */
	constexpr int PinActiveBitmaskSize = 64;

	/** There are 64 pin flags available, however we use flag 63 as a special pin-less ID for task dependencies that don't have associated pins. */
	constexpr int MaxOutputPins = PinActiveBitmaskSize - 1;

	/** Convert node ID and pin index to a unique pin ID. */
	FPCGPinId NodeIdAndPinIndexToPinId(FPCGTaskId NodeId, uint64 PinIndex);

	/** Create a pin ID from a node ID alone. Used for task inputs that don't have associated pins. */
	FPCGPinId NodeIdToPinId(FPCGTaskId NodeId);

	/** Adjust the pin ID to incorporate the given node ID offset. */
	FPCGPinId OffsetNodeIdInPinId(FPCGPinId PinId, uint64 NodeIDOffset);

	/** Extract node ID from the given pin ID. */
	FPCGTaskId GetNodeIdFromPinId(FPCGPinId PinId);

	/** Extract pin index from unique pin ID. */
	uint64 GetPinIndexFromPinId(FPCGPinId PinId);
}

UENUM(meta = (Bitflags))
enum class EPCGChangeType : uint8
{
	None = 0,
	Cosmetic = 1 << 0,
	Settings = 1 << 1,
	Input = 1 << 2,
	Edge = 1 << 3,
	Node = 1 << 4,
	Structural = 1 << 5,
	/** Anything related to generation grids - changing grid size or adding/removing grid size nodes. */
	GenerationGrid = 1 << 6,
};
ENUM_CLASS_FLAGS(EPCGChangeType);

// Bitmask containing the various data types supported in PCG. Note that this enum cannot be a blueprint type because
// enums have to be uint8 for blueprint, and we already use more than 8 bits in the bitmask.
// This is why we have a parallel enum just below that must match on a name basis 1:1 to allow the make/break functions to work properly
// in blueprint.
// WARNING: Please be mindful that combination of flags that are not explicitly defined there won't be serialized correctly, inducing data loss.
UENUM(meta = (Bitflags))
enum class EPCGDataType : uint32
{
	None = 0 UMETA(Hidden),
	Point = 1 << 1,

	Spline = 1 << 2,
	LandscapeSpline = 1 << 3,
	PolyLine = Spline | LandscapeSpline UMETA(DisplayName = "Curve"),

	Landscape = 1 << 4,
	Texture = 1 << 5,
	RenderTarget = 1 << 6,
	BaseTexture = Texture | RenderTarget UMETA(Hidden),
	Surface = Landscape | BaseTexture,

	Volume = 1 << 7,
	Primitive = 1 << 8,

	/** Simple concrete data. */
	Concrete = Point | PolyLine | Surface | Volume | Primitive,

	/** Boolean operations like union, difference, intersection. */
	Composite = 1 << 9 UMETA(Hidden),

	/** Combinations of concrete data and/or boolean operations. */
	Spatial = Composite | Concrete,

	Param = 1 << 27 UMETA(DisplayName = "Attribute Set"),

	// Combination of Param and Point, necessary for named-based serialization of enums.
	PointOrParam = Point | Param,

	Settings = 1 << 28 UMETA(Hidden),
	Other = 1 << 29,
	Any = (1 << 30) - 1
};
ENUM_CLASS_FLAGS(EPCGDataType);

// As discussed just before, a parallel version for "exclusive" (as in only type) of the EPCGDataType enum. Needed for blueprint compatibility.
UENUM(BlueprintType, meta=(DisplayName="PCG Data Type"))
enum class EPCGExclusiveDataType : uint8
{
	None = 0 UMETA(Hidden),
	Point,
	Spline,
	LandscapeSpline,
	PolyLine UMETA(DisplayName = "Curve"),
	Landscape,
	Texture,
	RenderTarget,
	BaseTexture UMETA(Hidden),
	Surface,
	Volume,
	Primitive,
	Concrete,
	Composite UMETA(Hidden),
	Spatial,
	Param UMETA(DisplayName = "Attribute Set"),
	Settings UMETA(Hidden),
	Other,
	Any,
	PointOrParam
};

namespace PCGPinConstants
{
	const FName DefaultInputLabel = TEXT("In");
	const FName DefaultOutputLabel = TEXT("Out");
	const FName DefaultParamsLabel = TEXT("Overrides");
	const FName DefaultDependencyOnlyLabel = TEXT("Dependency Only");

	const FName DefaultInFilterLabel = TEXT("InsideFilter");
	const FName DefaultOutFilterLabel = TEXT("OutsideFilter");

namespace Private
{
	const FName OldDefaultParamsLabel = TEXT("Params");
}

namespace Icons
{
	const FName LoopPinIcon = TEXT("GraphEditor.Macro.Loop_16x");
	const FName FeedbackPinIcon = TEXT("GraphEditor.GetSequenceBinding");
}
}

namespace PCGNodeConstants
{
namespace Icons
{
	const FName CompactNodeConvert = TEXT("PCG.Node.Compact.Convert");
	const FName CompactNodeFilter = TEXT("PCG.Node.Compact.Filter");
}
}

// Metadata used by PCG
namespace PCGObjectMetadata
{
	const FName Overridable = TEXT("PCG_Overridable");
	const FName NotOverridable = TEXT("PCG_NotOverridable");
	const FName OverrideAliases = TEXT("PCG_OverrideAliases");
	const FName DiscardPropertySelection = TEXT("PCG_DiscardPropertySelection");
	const FName DiscardExtraSelection = TEXT("PCG_DiscardExtraSelection");

	// Metadata usable in UPROPERTY for customizing the behavior when displaying the property in a property panel or graph node
	enum
	{
		/// [PropertyMetadata] Indicates that the property is overridable by params.
		PCG_Overridable,

		/// [PropertyMetadata] Indicates that the property is not-overridable by params. Used in structs to hide some parameters
		PCG_NotOverridable,

		/// [PropertyMetadata] Extra names to match for a given property.
		PCG_OverrideAliases,

		/// [PropertyMetadata] For FPCGAttributePropertySelector, won't display the point property items in the dropdown
		PCG_DiscardPropertySelection,

		/// [PropertyMetadata] For FPCGAttributePropertySelector, won't display the extra property items in the dropdown
		PCG_DiscardExtraSelection,
	};
}

namespace PCGFeatureSwitches
{
	extern PCG_API TAutoConsoleVariable<bool> CVarCheckSamplerMemory;
	extern PCG_API TAutoConsoleVariable<float> CVarSamplerMemoryThreshold;
}

/** Describes space referential for operations that create data */
UENUM(BlueprintType)
enum class EPCGCoordinateSpace : uint8
{
	World UMETA(DisplayName = "Global"),
	OriginalComponent,
	LocalComponent
};

/** Describes one or more target execution grids. */
UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EPCGHiGenGrid : uint32
{
	Uninitialized = 0 UMETA(Hidden),

	// NOTE: When adding new grids, increment PCGHiGenGrid::NumGridValues below
	Grid4 = 4 UMETA(DisplayName = "400"),
	Grid8 = 8 UMETA(DisplayName = "800"),
	Grid16 = 16 UMETA(DisplayName = "1600"),
	Grid32 = 32 UMETA(DisplayName = "3200"),
	Grid64 = 64 UMETA(DisplayName = "6400"),
	Grid128 = 128 UMETA(DisplayName = "12800"),
	Grid256 = 256 UMETA(DisplayName = "25600"),
	Grid512 = 512 UMETA(DisplayName = "51200"),
	Grid1024 = 1024 UMETA(DisplayName = "102400"),
	Grid2048 = 2048 UMETA(DisplayName = "204800"),
	
	GridMin = Grid4 UMETA(Hidden),
	GridMax = Grid2048 UMETA(Hidden),

	// Should execute once rather than executing on any grid
	Unbounded = 1u << 31,
};
ENUM_CLASS_FLAGS(EPCGHiGenGrid);

namespace PCGHiGenGrid
{
	// Number of unique values of EPCGHiGenGrid, const so it can be used for the inline allocator below.
	constexpr uint32 NumGridValues = 13;

	// Alias for array which is allocated on the stack (we have a strong idea of the max required elements).
	using FSizeArray = TArray<uint32, TInlineAllocator<PCGHiGenGrid::NumGridValues>>;

	// Alias for grid size to guid map allocated on the stack, which is unlikely to have a large number of elements.
	using FSizeToGuidMap = TMap<uint32, FGuid, TInlineSetAllocator<32>>;

	PCG_API bool IsValidGridSize(uint32 InGridSize);
	PCG_API bool IsValidGrid(EPCGHiGenGrid InGrid);
	PCG_API bool IsValidGridOrUninitialized(EPCGHiGenGrid InGrid);
	PCG_API uint32 GridToGridSize(EPCGHiGenGrid InGrid);
	PCG_API EPCGHiGenGrid GridSizeToGrid(uint32 InGridSize);

	PCG_API uint32 UninitializedGridSize();
	PCG_API uint32 UnboundedGridSize();
}

UENUM()
enum class EPCGAttachOptions : uint32
{
	NotAttached UMETA(Tooltip="Actor will not be attached to the target actor nor placed in an actor folder"),
	Attached UMETA(Tooltip="Actor will be attached to the target actor in the given node"),
	InFolder UMETA(Tooltip="Actor will be placed in an actor folder containing the name of the target actor.")
};

UENUM()
enum class EPCGEditorDirtyMode : uint8
{
	Normal UMETA(Tooltip="Normal editing mode where generation changes (generation, cleanup) dirty the component and its resources."),
	Preview UMETA(Tooltip="Editing mode where generation changes (generation, cleanup, resources) on the component will not trigger any dirty state, but will also not save any of the generated resources. Also represents the state after loading from the Load as Preview edit mode, where this will hold the last saved generation until a new generation is triggered."),
	LoadAsPreview UMETA(Tooltip="Acts as the normal editing mode until the next load of the component, at which state it acts as-if-transient, namely that any further generation changes will not dirty the component.")
};

USTRUCT(BlueprintType)
struct PCG_API FPCGRuntimeGenerationRadii
{
	GENERATED_BODY()

public:
	/** Get the runtime generation radius for the given grid size. */
	double GetGenerationRadiusFromGrid(EPCGHiGenGrid Grid) const;

	/** Compute the runtime cleanup radius for the given grid size. */
	double GetCleanupRadiusFromGrid(EPCGHiGenGrid Grid) const;

	static constexpr double DefaultGenerationRadiusMultiplier = 2.0;
	static constexpr double DefaultCleanupRadiusMultiplier = 1.1;

	/** The distance (in centimeters) at which the component will be considered for generation by the RuntimeGenerationScheduler. For partitioned components, this also acts as the unbounded generation radius. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Runtime Generation")
	double GenerationRadius = PCGHiGenGrid::UnboundedGridSize() * DefaultGenerationRadiusMultiplier;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Runtime Generation")
	double GenerationRadius400 = PCGHiGenGrid::GridToGridSize(EPCGHiGenGrid::Grid4) * DefaultGenerationRadiusMultiplier;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Runtime Generation")
	double GenerationRadius800 = PCGHiGenGrid::GridToGridSize(EPCGHiGenGrid::Grid8) * DefaultGenerationRadiusMultiplier;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Runtime Generation")
	double GenerationRadius1600 = PCGHiGenGrid::GridToGridSize(EPCGHiGenGrid::Grid16) * DefaultGenerationRadiusMultiplier;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Runtime Generation")
	double GenerationRadius3200 = PCGHiGenGrid::GridToGridSize(EPCGHiGenGrid::Grid32) * DefaultGenerationRadiusMultiplier;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Runtime Generation")
	double GenerationRadius6400 = PCGHiGenGrid::GridToGridSize(EPCGHiGenGrid::Grid64) * DefaultGenerationRadiusMultiplier;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Runtime Generation")
	double GenerationRadius12800 = PCGHiGenGrid::GridToGridSize(EPCGHiGenGrid::Grid128) * DefaultGenerationRadiusMultiplier;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Runtime Generation")
	double GenerationRadius25600 = PCGHiGenGrid::GridToGridSize(EPCGHiGenGrid::Grid256) * DefaultGenerationRadiusMultiplier;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Runtime Generation")
	double GenerationRadius51200 = PCGHiGenGrid::GridToGridSize(EPCGHiGenGrid::Grid512) * DefaultGenerationRadiusMultiplier;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Runtime Generation")
	double GenerationRadius102400 = PCGHiGenGrid::GridToGridSize(EPCGHiGenGrid::Grid1024) * DefaultGenerationRadiusMultiplier;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Runtime Generation")
	double GenerationRadius204800 = PCGHiGenGrid::GridToGridSize(EPCGHiGenGrid::Grid2048) * DefaultGenerationRadiusMultiplier;

	/** Multiplier on the GenerationRadius to control the distance at which runtime generated components will be cleaned up. Applied per grid size. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Runtime Generation", meta = (UIMin = "1.0", ClampMin = "1.0"))
	double CleanupRadiusMultiplier = DefaultCleanupRadiusMultiplier;
};

struct FInstancedPropertyBag;

namespace PCGDelegates
{
#if WITH_EDITOR
	/** Callback to hook in the UI to detect property bag changes, so the UI is reset and does not try to read in garbage memory. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnInstanceLayoutChanged, const FInstancedPropertyBag& /*Instance*/);
	extern PCG_API FOnInstanceLayoutChanged OnInstancedPropertyBagLayoutChanged;
#endif
}

UENUM()
enum class EPCGNodeTitleType : uint8
{
	/** The full title, may be multiple lines. */
	FullTitle,
	/** More concise, single line title. */
	ListView,
};

namespace PCGQualityHelpers
{
	constexpr int32 NumPins = 6;
	const FName PinLabelDefault = TEXT("Default");
	const FName PinLabelLow = TEXT("Low");
	const FName PinLabelMedium = TEXT("Medium");
	const FName PinLabelHigh = TEXT("High");
	const FName PinLabelEpic = TEXT("Epic");
	const FName PinLabelCinematic = TEXT("Cinematic");

	/** Get the pin label associated with the current 'pcg.Quality' value. If the quality level is invalid, it will return the default pin label. */
	PCG_API FName GetQualityPinLabel();
}
