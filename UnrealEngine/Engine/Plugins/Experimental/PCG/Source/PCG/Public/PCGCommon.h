// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/IConsoleManager.h"

#include "PCGCommon.generated.h"

using FPCGTaskId = uint64;
static const FPCGTaskId InvalidPCGTaskId = (uint64)-1;

UENUM(meta = (Bitflags))
enum class EPCGChangeType : uint8
{
	None = 0,
	Cosmetic = 1 << 0,
	Settings = 1 << 1,
	Input = 1 << 2,
	Edge = 1 << 3,
	Node = 1 << 4,
	Structural = 1 << 5
};
ENUM_CLASS_FLAGS(EPCGChangeType);

// Bitmask containing the various data types supported in PCG. Note that this enum cannot be a blueprint type because
// enums have to be uint8 for blueprint, and we already use more than 8 bits in the bitmask.
// This is why we have a parallel enum just below that must match on a name basis 1:1 to allow the make/break functions to work properly
// in blueprint.
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
	Any
};

namespace PCGPinConstants
{
	const FName DefaultInputLabel = TEXT("In");
	const FName DefaultOutputLabel = TEXT("Out");
	const FName DefaultParamsLabel = TEXT("Overrides");
	const FName DefaultDependencyOnlyLabel = TEXT("Dependency Only");

namespace Private
{
	const FName OldDefaultParamsLabel = TEXT("Params");
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
}

/** Describes one or more target execution grids. */
UENUM(meta = (Bitflags))
enum class EPCGHiGenGrid : uint32
{
	Uninitialized = 0 UMETA(Hidden),

	// NOTE: When adding new grids, increment PCGHiGenGrid::NumGridValues below.
	Grid32 = 32 UMETA(DisplayName = "3200"),
	Grid64 = 64 UMETA(DisplayName = "6400"),
	Grid128 = 128 UMETA(DisplayName = "12800"),
	Grid256 = 256 UMETA(DisplayName = "25600"),
	Grid512 = 512 UMETA(DisplayName = "51200"),
	Grid1024 = 1024 UMETA(DisplayName = "102400"),
	Grid2048 = 2048 UMETA(DisplayName = "204800"),
	
	GridMin = Grid32 UMETA(Hidden),
	GridMax = Grid2048 UMETA(Hidden),

	// Should execute once rather than executing on any grid
	Unbounded = 2 * GridMax,

	// Flag for grid not known yet. Represents the default grid size (which can be changed at runtime on PCGWorldActor)
	GenerationDefault = 2 * Unbounded UMETA(Hidden),
};
ENUM_CLASS_FLAGS(EPCGHiGenGrid);

namespace PCGHiGenGrid
{
	// Number of unique values of EPCGHiGenGrid, const so it can be used for the inline allocator below.
	constexpr uint32 NumGridValues = 10;

	// Alias for array which is allocated on the stack (we have a strong idea of the max required elements).
	using FSizeArray = TArray<uint32, TInlineAllocator<PCGHiGenGrid::NumGridValues>>;

	// Alias for grid size to guid map allocated on the stack, which is unlikely to have a large number of elements.
	using FSizeToGuidMap = TMap<uint32, FGuid, TInlineSetAllocator<32>>;

	PCG_API bool IsValidGridSize(uint32 InGridSize);
	PCG_API bool IsValidGrid(EPCGHiGenGrid InGrid);
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

struct FInstancedPropertyBag;

namespace PCGDelegates
{
#if WITH_EDITOR
	/** Callback to hook in the UI to detect property bag changes, so the UI is reset and does not try to read in garbage memory. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnInstanceLayoutChanged, const FInstancedPropertyBag& /*Instance*/);
	extern PCG_API FOnInstanceLayoutChanged OnInstancedPropertyBagLayoutChanged;
#endif
}
