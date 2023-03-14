// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "ControlRigDefines.h" 
#include "RigUnit_DynamicHierarchy.generated.h"

USTRUCT(meta = (Abstract, NodeColor="0.262745, 0.8, 0, 0.229412", Category = "DynamicHierarchy"))
struct CONTROLRIG_API FRigUnit_DynamicHierarchyBase : public FRigUnit
{
	GENERATED_BODY()

	static bool IsValidToRunInContext(
		const FRigUnitContext& InContext,
		const FControlRigExecuteContext& InExecuteContext,
		bool bAllowOnlyConstructionEvent,
		FString* OutErrorMessage = nullptr);
};

USTRUCT(meta = (Abstract, NodeColor="0.262745, 0.8, 0, 0.229412", Category = "DynamicHierarchy"))
struct CONTROLRIG_API FRigUnit_DynamicHierarchyBaseMutable : public FRigUnitMutable
{
	GENERATED_BODY()
};

/**
* Adds a new parent to an element. The weight for the new parent will be 0.0.
* You can use the SetParentWeights node to change the parent weights later.
*/
USTRUCT(meta=(DisplayName="Add Parent", Keywords="Children,Parent,Constraint,Space", Varying))
struct CONTROLRIG_API FRigUnit_AddParent : public FRigUnit_DynamicHierarchyBaseMutable
{
	GENERATED_BODY()

	FRigUnit_AddParent()
	{
		Child = Parent = FRigElementKey(NAME_None, ERigElementType::Control);
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/*
	 * The child to be parented under the new parent
	 */
	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey Child;

	/*
	 * The new parent to be added to the child
	 */
	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey Parent;
};

/**
 * Changes the default parent for an item - this removes all other current parents.
 */
USTRUCT(meta=(DisplayName="Set Default Parent", Keywords="Children,Parent,Constraint,Space,SetParent,AddParent", Varying))
struct CONTROLRIG_API FRigUnit_SetDefaultParent : public FRigUnit_DynamicHierarchyBaseMutable
{
	GENERATED_BODY()

	FRigUnit_SetDefaultParent()
	{
		Child = Parent = FRigElementKey(NAME_None, ERigElementType::Control);
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/*
	 * The child to be parented under the new default parent
	 */
	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey Child;

	/*
	 * The default parent to be used for the child
	 */
	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey Parent;
};

UENUM()
enum class ERigSwitchParentMode : uint8
{
	/** Switches the element to be parented to the world */
	World,

	/** Switches back to the original / default parent */
	DefaultParent,

	/** Switches the child to the provided parent item */
	ParentItem
};

/**
* Switches an element to a new parent.
*/
USTRUCT(meta=(DisplayName="Switch Parent", Keywords="Children,Parent,Constraint,Space,Switch", Varying))
struct CONTROLRIG_API FRigUnit_SwitchParent : public FRigUnit_DynamicHierarchyBaseMutable
{
	GENERATED_BODY()

	FRigUnit_SwitchParent()
	{
		Mode = ERigSwitchParentMode::ParentItem;
		Child = Parent = FRigElementKey(NAME_None, ERigElementType::Control);
		bMaintainGlobal = true;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/* Depending on this the child will switch to the world,
	 * back to its default or to the item provided by the Parent pin
	 */
	UPROPERTY(meta = (Input))
	ERigSwitchParentMode Mode;

	/* The child to switch to a new parent */
	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey Child;

	/* The optional parent to switch to. This is only used if the mode is set to 'Parent Item' */
	UPROPERTY(meta = (Input, ExpandByDefault, EditCondition="Mode==ParentItem"))
	FRigElementKey Parent;

	/* If set to true the item will maintain its global transform,
	 * otherwise it will maintain local
	 */
	UPROPERTY(meta = (Input))
	bool bMaintainGlobal;
};

/**
* Returns the item's parents' weights
*/
USTRUCT(meta=(DisplayName="Get Parent Weights", Keywords="Chain,Parents,Hierarchy", Varying, Deprecated = "5.0"))
struct CONTROLRIG_API FRigUnit_HierarchyGetParentWeights : public FRigUnit_DynamicHierarchyBase
{
	GENERATED_BODY()

	FRigUnit_HierarchyGetParentWeights()
	{
		Child = FRigElementKey(NAME_None, ERigElementType::Control);
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/*
	 * The child to retrieve the weights for
	 */
	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey Child;

	/*
	 * The weight of each parent
	 */
	UPROPERTY(meta = (Output))
	TArray<FRigElementWeight> Weights;

	/*
	 * The key for each parent
	 */
	UPROPERTY(meta = (Output))
	FRigElementKeyCollection Parents;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
* Returns the item's parents' weights
*/
USTRUCT(meta=(DisplayName="Get Parent Weights", Keywords="Chain,Parents,Hierarchy", Varying))
struct CONTROLRIG_API FRigUnit_HierarchyGetParentWeightsArray : public FRigUnit_DynamicHierarchyBase
{
	GENERATED_BODY()

	FRigUnit_HierarchyGetParentWeightsArray()
	{
		Child = FRigElementKey(NAME_None, ERigElementType::Control);
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/*
	 * The child to retrieve the weights for
	 */
	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey Child;

	/*
	 * The weight of each parent
	 */
	UPROPERTY(meta = (Output))
	TArray<FRigElementWeight> Weights;

	/*
	 * The key for each parent
	 */
	UPROPERTY(meta = (Output))
	TArray<FRigElementKey> Parents;
};

/**
* Sets the item's parents' weights
*/
USTRUCT(meta=(DisplayName="Set Parent Weights", Keywords="Chain,Parents,Hierarchy", Varying))
struct CONTROLRIG_API FRigUnit_HierarchySetParentWeights : public FRigUnit_DynamicHierarchyBaseMutable
{
	GENERATED_BODY()

	FRigUnit_HierarchySetParentWeights()
	{
		Child = FRigElementKey(NAME_None, ERigElementType::Control);
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/*
	 * The child to set the parents' weights for
	 */
	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey Child;

	/*
	 * The weights to set for the child's parents.
	 * The number of weights needs to match the current number of parents.
	 */
	UPROPERTY(meta = (Input))
	TArray<FRigElementWeight> Weights;
};

/**
 * Removes all elements from the hierarchy
 * Note: This node only runs as part of the construction event.
 */
USTRUCT(meta=(DisplayName="Reset Hierarchy", Keywords="Delete,Erase,Clear,Empty,RemoveAll", Varying))
struct FRigUnit_HierarchyReset : public FRigUnit_DynamicHierarchyBaseMutable
{
	GENERATED_BODY()

	FRigUnit_HierarchyReset()
	{
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Imports all bones (and curves) from the currently assigned skeleton.
 * Note: This node only runs as part of the construction event.
 */
USTRUCT(meta=(DisplayName="Import Skeleton", Keywords="Skeleton,Mesh,AddBone,AddCurve,Spawn", Varying))
struct FRigUnit_HierarchyImportFromSkeleton : public FRigUnit_DynamicHierarchyBaseMutable
{
	GENERATED_BODY()

	FRigUnit_HierarchyImportFromSkeleton()
	{
		NameSpace = NAME_None;
		bIncludeCurves = false;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FName NameSpace;

	UPROPERTY(meta = (Input))
	bool bIncludeCurves;

	UPROPERTY(meta = (Output))
	TArray<FRigElementKey> Items;
};

/**
 * Removes an element from the hierarchy
 * Note: This node only runs as part of the construction event.
 */
USTRUCT(meta=(DisplayName="Remove Item", Keywords="Delete,Erase,Joint,Skeleton", Varying))
struct CONTROLRIG_API FRigUnit_HierarchyRemoveElement : public FRigUnit_DynamicHierarchyBaseMutable
{
	GENERATED_BODY()

	FRigUnit_HierarchyRemoveElement()
	{
		Item = FRigElementKey(NAME_None, ERigElementType::Bone);
		bSuccess = false;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/*
	 * The item to remove
	 */
	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey Item;

	/*
	 * True if the element has been removed successfuly 
	 */
	UPROPERTY(meta = (Output, ExpandByDefault))
	bool bSuccess;
};

USTRUCT(meta=(Abstract))
struct CONTROLRIG_API FRigUnit_HierarchyAddElement : public FRigUnit_DynamicHierarchyBaseMutable
{
	GENERATED_BODY()

	FRigUnit_HierarchyAddElement()
	{
		Parent = Item = FRigElementKey(NAME_None, ERigElementType::Bone);
		Name = NAME_None;
	}

	/*
	 * The parent of the new element to add
	 */
	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey Parent;

	/*
	 * The name of the new element to add
	 */
	UPROPERTY(meta = (Input))
	FName Name;

	/*
	 * The resulting item
	 */
	UPROPERTY(meta = (Output))
	FRigElementKey Item;
};

/**
 * Adds a new bone to the hierarchy
 * Note: This node only runs as part of the construction event.
 */
USTRUCT(meta=(DisplayName="Spawn Bone", Keywords="Construction,Create,New,Joint,Skeleton", Varying))
struct CONTROLRIG_API FRigUnit_HierarchyAddBone : public FRigUnit_HierarchyAddElement
{
	GENERATED_BODY()

	FRigUnit_HierarchyAddBone()
	{
		Name = TEXT("NewBone");
		Transform = FTransform::Identity;
		Space = EBoneGetterSetterMode::LocalSpace;
	}

	/*
	 * The initial transform of the new element
	 */
	UPROPERTY(meta = (Input))
	FTransform Transform;

	/**
	 * Defines if the transform should be interpreted in local or global space
	 */ 
	UPROPERTY(meta = (Input))
	EBoneGetterSetterMode Space;

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Adds a new null to the hierarchy
 * Note: This node only runs as part of the construction event.
 */
USTRUCT(meta=(DisplayName="Spawn Null", Keywords="Construction,Create,New,Locator,Group", Varying))
struct CONTROLRIG_API FRigUnit_HierarchyAddNull : public FRigUnit_HierarchyAddElement
{
	GENERATED_BODY()

	FRigUnit_HierarchyAddNull()
	{
		Name = TEXT("NewNull");
		Transform = FTransform::Identity;
		Space = EBoneGetterSetterMode::LocalSpace;
	}

	/*
	 * The initial transform of the new element
	 */
	UPROPERTY(meta = (Input))
	FTransform Transform;

	/**
	 * Defines if the transform should be interpreted in local or global space
	 */ 
	UPROPERTY(meta = (Input))
	EBoneGetterSetterMode Space;

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigUnit_HierarchyAddControl_Settings
{
	GENERATED_BODY()

	FRigUnit_HierarchyAddControl_Settings()
		: DisplayName(NAME_None)
	{}
	virtual ~FRigUnit_HierarchyAddControl_Settings(){}

	void Configure(FRigControlSettings& OutSettings) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FName DisplayName;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigUnit_HierarchyAddControl_ShapeSettings
{
	GENERATED_BODY()

	FRigUnit_HierarchyAddControl_ShapeSettings()
		: bVisible(true)
		, Name(TEXT("Default"))
		, Color(FLinearColor::Red)
		, Transform(FTransform::Identity)
	{}

	void Configure(FRigControlSettings& OutSettings) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bVisible;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta=(CustomWidget = "ShapeName"))
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FLinearColor Color;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FTransform Transform;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigUnit_HierarchyAddControl_ProxySettings
{
	GENERATED_BODY()

	FRigUnit_HierarchyAddControl_ProxySettings()
		: bIsProxy(false)
		, ShapeVisibility(ERigControlVisibility::BasedOnSelection)
	{}

	void Configure(FRigControlSettings& OutSettings) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bIsProxy;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	TArray<FRigElementKey> DrivenControls;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	ERigControlVisibility ShapeVisibility;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigUnit_HierarchyAddControlFloat_LimitSettings
{
	GENERATED_BODY();
	
	FRigUnit_HierarchyAddControlFloat_LimitSettings()
		: Limit(true, true)
		, MinValue(0.f)
		, MaxValue(100.f)
		, bDrawLimits(true)
	{}

	void Configure(FRigControlSettings& OutSettings) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FRigControlLimitEnabled Limit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float MinValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float MaxValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bDrawLimits;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigUnit_HierarchyAddControlFloat_Settings : public FRigUnit_HierarchyAddControl_Settings
{
	GENERATED_BODY()
	
	FRigUnit_HierarchyAddControlFloat_Settings()
		: FRigUnit_HierarchyAddControl_Settings()
		, PrimaryAxis(ERigControlAxis::X)
	{}
	virtual ~FRigUnit_HierarchyAddControlFloat_Settings() override {}

	void Configure(FRigControlSettings& OutSettings) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	ERigControlAxis PrimaryAxis;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FRigUnit_HierarchyAddControlFloat_LimitSettings Limits;;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FRigUnit_HierarchyAddControl_ShapeSettings Shape;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FRigUnit_HierarchyAddControl_ProxySettings Proxy;
};

/**
 * Adds a new control to the hierarchy
 * Note: This node only runs as part of the construction event.
 */
USTRUCT(meta=(DisplayName="Spawn Float Control", TemplateName="SpawnControl", Keywords="Construction,Create,New,AddControl,NewControl,CreateControl", Varying))
struct CONTROLRIG_API FRigUnit_HierarchyAddControlFloat : public FRigUnit_HierarchyAddElement
{
	GENERATED_BODY()

	FRigUnit_HierarchyAddControlFloat()
	{
		Name = TEXT("NewControl");
		OffsetTransform = FTransform::Identity;
		InitialValue = 0.f;
	}

	/*
	 * The offset transform of the new control
	 */
	UPROPERTY(meta = (Input))
	FTransform OffsetTransform;

	/*
	 * The initial value of the new control
	 */
	UPROPERTY(meta = (Input))
	float InitialValue;
	
	/*
	 * The settings for the control
	 */
	UPROPERTY(meta = (Input))
	FRigUnit_HierarchyAddControlFloat_Settings Settings;

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigUnit_HierarchyAddControlInteger_LimitSettings
{
	GENERATED_BODY();
	
	FRigUnit_HierarchyAddControlInteger_LimitSettings()
		: Limit(true, true)
		, MinValue(0)
		, MaxValue(100)
		, bDrawLimits(true)
	{}

	void Configure(FRigControlSettings& OutSettings) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FRigControlLimitEnabled Limit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	int32 MinValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	int32 MaxValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bDrawLimits;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigUnit_HierarchyAddControlInteger_Settings : public FRigUnit_HierarchyAddControl_Settings
{
	GENERATED_BODY()
	
	FRigUnit_HierarchyAddControlInteger_Settings()
		: FRigUnit_HierarchyAddControl_Settings()
		, PrimaryAxis(ERigControlAxis::X)
	{}
	virtual ~FRigUnit_HierarchyAddControlInteger_Settings() override {}

	void Configure(FRigControlSettings& OutSettings) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	ERigControlAxis PrimaryAxis;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FRigUnit_HierarchyAddControlInteger_LimitSettings Limits;;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FRigUnit_HierarchyAddControl_ShapeSettings Shape;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FRigUnit_HierarchyAddControl_ProxySettings Proxy;
};

/**
 * Adds a new control to the hierarchy
 * Note: This node only runs as part of the construction event.
 */
USTRUCT(meta=(DisplayName="Spawn Integer Control", TemplateName="SpawnControl", Keywords="Construction,Create,New,AddControl,NewControl,CreateControl", Varying))
struct CONTROLRIG_API FRigUnit_HierarchyAddControlInteger : public FRigUnit_HierarchyAddElement
{
	GENERATED_BODY()

	FRigUnit_HierarchyAddControlInteger()
	{
		Name = TEXT("NewControl");
		OffsetTransform = FTransform::Identity;
		InitialValue = 0;
	}

	/*
	 * The offset transform of the new control
	 */
	UPROPERTY(meta = (Input))
	FTransform OffsetTransform;

	/*
	 * The initial value of the new control
	 */
	UPROPERTY(meta = (Input))
	int32 InitialValue;
	
	/*
	 * The settings for the control
	 */
	UPROPERTY(meta = (Input))
	FRigUnit_HierarchyAddControlInteger_Settings Settings;

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigUnit_HierarchyAddControlVector2D_LimitSettings
{
	GENERATED_BODY();
	
	FRigUnit_HierarchyAddControlVector2D_LimitSettings()
		: LimitX(true, true)
		, LimitY(true, true)
		, MinValue(FVector2D::ZeroVector)
		, MaxValue(FVector2D(1.f, 1.f))
		, bDrawLimits(true)
	{}

	void Configure(FRigControlSettings& OutSettings) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FRigControlLimitEnabled LimitX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FRigControlLimitEnabled LimitY;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FVector2D MinValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FVector2D MaxValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bDrawLimits;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigUnit_HierarchyAddControlVector2D_Settings : public FRigUnit_HierarchyAddControl_Settings
{
	GENERATED_BODY()
	
	FRigUnit_HierarchyAddControlVector2D_Settings()
		: FRigUnit_HierarchyAddControl_Settings()
		, PrimaryAxis(ERigControlAxis::X)
	{}
	virtual ~FRigUnit_HierarchyAddControlVector2D_Settings() override {}

	void Configure(FRigControlSettings& OutSettings) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	ERigControlAxis PrimaryAxis;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FRigUnit_HierarchyAddControlVector2D_LimitSettings Limits;;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FRigUnit_HierarchyAddControl_ShapeSettings Shape;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FRigUnit_HierarchyAddControl_ProxySettings Proxy;
};

/**
 * Adds a new control to the hierarchy
 * Note: This node only runs as part of the construction event.
 */
USTRUCT(meta=(DisplayName="Spawn Vector2D Control", TemplateName="SpawnControl", Keywords="Construction,Create,New,AddControl,NewControl,CreateControl", Varying))
struct CONTROLRIG_API FRigUnit_HierarchyAddControlVector2D : public FRigUnit_HierarchyAddElement
{
	GENERATED_BODY()

	FRigUnit_HierarchyAddControlVector2D()
	{
		Name = TEXT("NewControl");
		OffsetTransform = FTransform::Identity;
		InitialValue = FVector2D::ZeroVector;
	}

	/*
	 * The offset transform of the new control
	 */
	UPROPERTY(meta = (Input))
	FTransform OffsetTransform;

	/*
	 * The initial value of the new control
	 */
	UPROPERTY(meta = (Input))
	FVector2D InitialValue;
	
	/*
	 * The settings for the control
	 */
	UPROPERTY(meta = (Input))
	FRigUnit_HierarchyAddControlVector2D_Settings Settings;

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigUnit_HierarchyAddControlVector_LimitSettings
{
	GENERATED_BODY();
	
	FRigUnit_HierarchyAddControlVector_LimitSettings()
		: LimitX(false, false)
		, LimitY(false, false)
		, LimitZ(false, false)
		, MinValue(FVector::ZeroVector)
		, MaxValue(FVector::OneVector)
		, bDrawLimits(true)
	{}

	void Configure(FRigControlSettings& OutSettings) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FRigControlLimitEnabled LimitX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FRigControlLimitEnabled LimitY;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FRigControlLimitEnabled LimitZ;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FVector MinValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FVector MaxValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bDrawLimits;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigUnit_HierarchyAddControlVector_Settings : public FRigUnit_HierarchyAddControl_Settings
{
	GENERATED_BODY()
	
	FRigUnit_HierarchyAddControlVector_Settings()
		: FRigUnit_HierarchyAddControl_Settings()
		, bIsPosition(true)
	{}
	virtual ~FRigUnit_HierarchyAddControlVector_Settings() override {}

	void Configure(FRigControlSettings& OutSettings) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bIsPosition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FRigUnit_HierarchyAddControlVector_LimitSettings Limits;;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FRigUnit_HierarchyAddControl_ShapeSettings Shape;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FRigUnit_HierarchyAddControl_ProxySettings Proxy;
};

/**
 * Adds a new control to the hierarchy
 * Note: This node only runs as part of the construction event.
 */
USTRUCT(meta=(DisplayName="Spawn Vector Control", TemplateName="SpawnControl", Keywords="Construction,Create,New,AddControl,NewControl,CreateControl", Varying))
struct CONTROLRIG_API FRigUnit_HierarchyAddControlVector : public FRigUnit_HierarchyAddElement
{
	GENERATED_BODY()

	FRigUnit_HierarchyAddControlVector()
	{
		Name = TEXT("NewControl");
		OffsetTransform = FTransform::Identity;
		InitialValue = FVector::ZeroVector;
	}

	/*
	 * The offset transform of the new control
	 */
	UPROPERTY(meta = (Input))
	FTransform OffsetTransform;

	/*
	 * The initial value of the new control
	 */
	UPROPERTY(meta = (Input))
	FVector InitialValue;

	/*
	 * The settings for the control
	 */
	UPROPERTY(meta = (Input))
	FRigUnit_HierarchyAddControlVector_Settings Settings;

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigUnit_HierarchyAddControlRotator_LimitSettings
{
	GENERATED_BODY();
	
	FRigUnit_HierarchyAddControlRotator_LimitSettings()
		: LimitPitch(false, false)
		, LimitYaw(false, false)
		, LimitRoll(false, false)
		, MinValue(FRotator(-180.f, -180.f, -180.f))
		, MaxValue(FRotator(180.f, 180.f, 180.f))
		, bDrawLimits(true)
	{}

	void Configure(FRigControlSettings& OutSettings) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FRigControlLimitEnabled LimitPitch;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FRigControlLimitEnabled LimitYaw;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FRigControlLimitEnabled LimitRoll;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FRotator MinValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FRotator MaxValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bDrawLimits;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigUnit_HierarchyAddControlRotator_Settings : public FRigUnit_HierarchyAddControl_Settings
{
	GENERATED_BODY()
	
	FRigUnit_HierarchyAddControlRotator_Settings()
		: FRigUnit_HierarchyAddControl_Settings()
	{}
	virtual ~FRigUnit_HierarchyAddControlRotator_Settings() override {}

	void Configure(FRigControlSettings& OutSettings) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FRigUnit_HierarchyAddControlRotator_LimitSettings Limits;;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FRigUnit_HierarchyAddControl_ShapeSettings Shape;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FRigUnit_HierarchyAddControl_ProxySettings Proxy;
};

/**
 * Adds a new control to the hierarchy
 * Note: This node only runs as part of the construction event.
 */
USTRUCT(meta=(DisplayName="Spawn Rotator Control", TemplateName="SpawnControl", Keywords="Construction,Create,New,AddControl,NewControl,CreateControl,Rotation", Varying))
struct CONTROLRIG_API FRigUnit_HierarchyAddControlRotator : public FRigUnit_HierarchyAddElement
{
	GENERATED_BODY()

	FRigUnit_HierarchyAddControlRotator()
	{
		Name = TEXT("NewControl");
		OffsetTransform = FTransform::Identity;
		InitialValue = FRotator::ZeroRotator;
	}

	/*
	 * The offset transform of the new control
	 */
	UPROPERTY(meta = (Input))
	FTransform OffsetTransform;

	/*
	 * The initial value of the new control
	 */
	UPROPERTY(meta = (Input))
	FRotator InitialValue;

	/*
	 * The settings for the control
	 */
	UPROPERTY(meta = (Input))
	FRigUnit_HierarchyAddControlRotator_Settings Settings;

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigUnit_HierarchyAddControlTransform_Settings : public FRigUnit_HierarchyAddControl_Settings
{
	GENERATED_BODY()
	
	FRigUnit_HierarchyAddControlTransform_Settings()
		: FRigUnit_HierarchyAddControl_Settings()
	{}
	virtual ~FRigUnit_HierarchyAddControlTransform_Settings() override {}

	void Configure(FRigControlSettings& OutSettings) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FRigUnit_HierarchyAddControl_ShapeSettings Shape;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FRigUnit_HierarchyAddControl_ProxySettings Proxy;
};

/**
 * Adds a new control to the hierarchy
 * Note: This node only runs as part of the construction event.
 */
USTRUCT(meta=(DisplayName="Spawn Transform Control", TemplateName="SpawnControl", Keywords="Construction,Create,New,AddControl,NewControl,CreateControl", Varying))
struct CONTROLRIG_API FRigUnit_HierarchyAddControlTransform : public FRigUnit_HierarchyAddElement
{
	GENERATED_BODY()

	FRigUnit_HierarchyAddControlTransform()
	{
		Name = TEXT("NewControl");
		OffsetTransform = FTransform::Identity;
		InitialValue = FTransform::Identity;
	}

	/*
	 * The offset transform of the new control
	 */
	UPROPERTY(meta = (Input))
	FTransform OffsetTransform;

	/*
	 * The initial value of the new control
	 */
	UPROPERTY(meta = (Input))
	FTransform InitialValue;
	
	/*
	 * The settings for the control
	 */
	UPROPERTY(meta = (Input))
	FRigUnit_HierarchyAddControlTransform_Settings Settings;

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Adds a new animation channel to the hierarchy
 * Note: This node only runs as part of the construction event.
 */
USTRUCT(meta=(DisplayName="Spawn Bool Animation Channel", TemplateName="SpawnAnimationChannel", Keywords="Construction,Create,New,AddAnimationChannel,NewAnimationChannel,CreateAnimationChannel,AddChannel,NewChannel,CreateChannel,SpawnChannel", Varying))
struct CONTROLRIG_API FRigUnit_HierarchyAddAnimationChannelBool : public FRigUnit_HierarchyAddElement
{
	GENERATED_BODY()

	FRigUnit_HierarchyAddAnimationChannelBool()
	{
		Name = TEXT("NewChannel");
		InitialValue = false;
		MinimumValue = false;
		MaximumValue = true;
	}

	/*
	 * The initial value of the new animation channel
	 */
	UPROPERTY(meta = (Input))
	bool InitialValue;

	/*
	 * The initial value of the new animation channel
	 */
	UPROPERTY(meta = (Input))
	bool MinimumValue;

	/*
	 * The maximum value for the animation channel
	 */
	UPROPERTY(meta = (Input))
	bool MaximumValue;

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Adds a new animation channel to the hierarchy
 * Note: This node only runs as part of the construction event.
 */
USTRUCT(meta=(DisplayName="Spawn Float Animation Channel", TemplateName="SpawnAnimationChannel", Keywords="Construction,Create,New,AddAnimationChannel,NewAnimationChannel,CreateAnimationChannel,AddChannel,NewChannel,CreateChannel,SpawnChannel", Varying))
struct CONTROLRIG_API FRigUnit_HierarchyAddAnimationChannelFloat : public FRigUnit_HierarchyAddElement
{
	GENERATED_BODY()

	FRigUnit_HierarchyAddAnimationChannelFloat()
	{
		Name = TEXT("NewChannel");
		InitialValue = 0.f;
		MinimumValue = 0.f;
		MaximumValue = 1.f;
	}

	/*
	 * The initial value of the new animation channel
	 */
	UPROPERTY(meta = (Input))
	float InitialValue;

	/*
	 * The initial value of the new animation channel
	 */
	UPROPERTY(meta = (Input))
	float MinimumValue;

	/*
	 * The maximum value for the animation channel
	 */
	UPROPERTY(meta = (Input))
	float MaximumValue;

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Adds a new animation channel to the hierarchy
 * Note: This node only runs as part of the construction event.
 */
USTRUCT(meta=(DisplayName="Spawn Integer Animation Channel", TemplateName="SpawnAnimationChannel", Keywords="Construction,Create,New,AddAnimationChannel,NewAnimationChannel,CreateAnimationChannel,AddChannel,NewChannel,CreateChannel,SpawnChannel", Varying))
struct CONTROLRIG_API FRigUnit_HierarchyAddAnimationChannelInteger : public FRigUnit_HierarchyAddElement
{
	GENERATED_BODY()

	FRigUnit_HierarchyAddAnimationChannelInteger()
	{
		Name = TEXT("NewChannel");
		InitialValue = 0;
		MinimumValue = 0;
		MaximumValue = 100;
	}

	/*
	 * The initial value of the new animation channel
	 */
	UPROPERTY(meta = (Input))
	int32 InitialValue;

	/*
	 * The initial value of the new animation channel
	 */
	UPROPERTY(meta = (Input))
	int32 MinimumValue;

	/*
	 * The maximum value for the animation channel
	 */
	UPROPERTY(meta = (Input))
	int32 MaximumValue;

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Adds a new animation channel to the hierarchy
 * Note: This node only runs as part of the construction event.
 */
USTRUCT(meta=(DisplayName="Spawn Vector2D Animation Channel", TemplateName="SpawnAnimationChannel", Keywords="Construction,Create,New,AddAnimationChannel,NewAnimationChannel,CreateAnimationChannel,AddChannel,NewChannel,CreateChannel,SpawnChannel", Varying))
struct CONTROLRIG_API FRigUnit_HierarchyAddAnimationChannelVector2D : public FRigUnit_HierarchyAddElement
{
	GENERATED_BODY()

	FRigUnit_HierarchyAddAnimationChannelVector2D()
	{
		Name = TEXT("NewChannel");
		InitialValue = FVector2D::ZeroVector;
		MinimumValue = FVector2D::ZeroVector;
		MaximumValue = FVector2D(1.f, 1.f);
	}

	/*
	 * The initial value of the new animation channel
	 */
	UPROPERTY(meta = (Input))
	FVector2D InitialValue;

	/*
	 * The initial value of the new animation channel
	 */
	UPROPERTY(meta = (Input))
	FVector2D MinimumValue;

	/*
	 * The maximum value for the animation channel
	 */
	UPROPERTY(meta = (Input))
	FVector2D MaximumValue;

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Adds a new animation channel to the hierarchy
 * Note: This node only runs as part of the construction event.
 */
USTRUCT(meta=(DisplayName="Spawn Vector Animation Channel", TemplateName="SpawnAnimationChannel", Keywords="Construction,Create,New,AddAnimationChannel,NewAnimationChannel,CreateAnimationChannel,AddChannel,NewChannel,CreateChannel,SpawnChannel", Varying))
struct CONTROLRIG_API FRigUnit_HierarchyAddAnimationChannelVector : public FRigUnit_HierarchyAddElement
{
	GENERATED_BODY()

	FRigUnit_HierarchyAddAnimationChannelVector()
	{
		Name = TEXT("NewChannel");
		InitialValue = FVector::ZeroVector;
		MinimumValue = FVector::ZeroVector;
		MaximumValue = FVector(1.f, 1.f, 1.f);
	}

	/*
	 * The initial value of the new animation channel
	 */
	UPROPERTY(meta = (Input))
	FVector InitialValue;

	/*
	 * The initial value of the new animation channel
	 */
	UPROPERTY(meta = (Input))
	FVector MinimumValue;

	/*
	 * The maximum value for the animation channel
	 */
	UPROPERTY(meta = (Input))
	FVector MaximumValue;

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Adds a new animation channel to the hierarchy
 * Note: This node only runs as part of the construction event.
 */
USTRUCT(meta=(DisplayName="Spawn Rotator Animation Channel", TemplateName="SpawnAnimationChannel", Keywords="Construction,Create,New,AddAnimationChannel,NewAnimationChannel,CreateAnimationChannel,AddChannel,NewChannel,CreateChannel,SpawnChannel", Varying))
struct CONTROLRIG_API FRigUnit_HierarchyAddAnimationChannelRotator : public FRigUnit_HierarchyAddElement
{
	GENERATED_BODY()

	FRigUnit_HierarchyAddAnimationChannelRotator()
	{
		Name = TEXT("NewChannel");
		InitialValue = FRotator::ZeroRotator;
		MinimumValue = FRotator(-180.f, -180.f, -180.f);
		MaximumValue = FRotator(180.f, 180.f, 180.f);
	}

	/*
	 * The initial value of the new animation channel
	 */
	UPROPERTY(meta = (Input))
	FRotator InitialValue;

	/*
	 * The initial value of the new animation channel
	 */
	UPROPERTY(meta = (Input))
	FRotator MinimumValue;

	/*
	 * The maximum value for the animation channel
	 */
	UPROPERTY(meta = (Input))
	FRotator MaximumValue;

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};