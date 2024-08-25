// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeMaterialBase.h"
#include "MuCOE/RemapPins/CustomizableObjectNodeRemapPinsByName.h"

#include "CustomizableObjectNodeMaterial.generated.h"

enum class EMaterialParameterType : uint8;
namespace ENodeTitleType { enum Type : int; }

class FArchive;
class SGraphNode;
class SWidget;
class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UMaterial;
class UMaterialInterface;
class UObject;
class UTexture2D;
struct FCustomizableObjectNodeMaterialImage;
struct FCustomizableObjectNodeMaterialScalar;
struct FCustomizableObjectNodeMaterialVector;
struct FEdGraphPinReference;
struct FFrame;
struct FPropertyChangedEvent;


DECLARE_MULTICAST_DELEGATE(FPostImagePinModeChangedDelegate)


/** Custom remap pins by name action.
 *
 * Remap pins by Texture Parameter Id. */
UCLASS()
class UCustomizableObjectNodeMaterialRemapPinsByName : public UCustomizableObjectNodeRemapPinsByName
{
	GENERATED_BODY()
public:
	virtual bool Equal(const UCustomizableObjectNode& Node, const UEdGraphPin& OldPin, const UEdGraphPin& NewPin) const override;

	virtual void RemapPins(const UCustomizableObjectNode& Node, const TArray<UEdGraphPin*>& OldPins, const TArray<UEdGraphPin*>& NewPins, TMap<UEdGraphPin*, UEdGraphPin*>& PinsToRemap, TArray<UEdGraphPin*>& PinsToOrphan) override;

	bool HasSavedPinData(const UCustomizableObjectNode& Node, const UEdGraphPin &Pin) const;
};


/** Base class for all Material Parameters. */
UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeMaterialPinDataParameter : public UCustomizableObjectNodePinData
{
	GENERATED_BODY()

public:
	/** Texture Parameter Id. */
	UPROPERTY()
	FGuid ParameterId;

	/** Returns true if all properties are in its default state. */
	virtual bool IsDefault() const;
};


/** Node pin mode. All pins set to EPinMode::Default will use this this mode. */
UENUM()
enum class ENodePinMode
{
	Mutable UMETA(ToolTip = "All Material Texture Parameters go through Mutable."),
	Passthrough UMETA(ToolTip = "All Material Texture Parameters are not modified by Mutable.")
};


/** Image pin, pin mode. */
UENUM()
enum class EPinMode
{
	/** Use the defined node pin mode. Does not override it. */
	Default,
	/** Override the node pin mode. Set it to Mutable mode. */
	Mutable,
	/** Override the node pin mode. Set it to Pass-through mode. */
	Passthrough
};


/** Image Pin, UV Layout Mode. */
UENUM()
enum class EUVLayoutMode
{
	/** Does not override the UV Index specified in the Material. */
	FromMaterial,
	/* Texture should not be transformed by any layout. Theses textures will not be reduced automatically for LODs. */
	Ignore,
	/** User specified UV Index. */
	Index
};


/** Enum to FText. */
FText EPinModeToText(EPinMode PinMode);

UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeMaterial : public UCustomizableObjectNodeMaterialBase
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=CustomizableObject)
	TObjectPtr<UMaterialInterface> Material = nullptr;

	UPROPERTY(EditAnywhere, Category=CustomizableObject, Meta = (ToolTip = "Set all Mateiral Texture Parameters to the specified mode. Each Texture Parameter Pin can override this mode."))
	ENodePinMode TextureParametersMode = ENodePinMode::Passthrough;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CustomizableObject)
	TArray<FString> Tags;

	/** Selects which Mesh component of the Instance this material belongs to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CustomizableObject, meta = (ClampMin = "0"))
	int32 MeshComponentIndex = 0;

	/** Materials will be reused between LODs when possible unless explicitly disabled. */
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	bool bReuseMaterialBetweenLODs = true;
	
	// UObject interface.
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	// EdGraphNode interface
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FLinearColor GetNodeTitleColor() const override;
	FText GetTooltipText() const override;
	TSharedPtr<SGraphNode> CreateVisualWidget() override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual void PostPasteNode() override;
	virtual bool CanConnect(const UEdGraphPin* InOwnedInputPin, const UEdGraphPin* InOutputPin, bool& bOutIsOtherNodeBlocklisted, bool& bOutArePinsCompatible) const override;

	// UCustomizableObjectNode interface
	virtual void BackwardsCompatibleFixup() override;
	virtual void PostBackwardsCompatibleFixup() override;
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	bool CanPinBeHidden(const UEdGraphPin& Pin) const override;
	virtual UCustomizableObjectNodeRemapPinsByName* CreateRemapPinsDefault() const override;
	bool ProvidesCustomPinRelevancyTest() const override { return true; }
	bool IsPinRelevant(const UEdGraphPin* Pin) const override;
	virtual bool CustomRemovePin(UEdGraphPin& Pin) override;
	virtual void ReconstructNode(UCustomizableObjectNodeRemapPins* RemapPinsMode) override;

	// Own Interface
	virtual UEdGraphPin* GetMeshPin() const;
	UEdGraphPin* GetMaterialAssetPin() const;

	/** Allow only one connection to a single BaseObjec node. There can be multiple conections to NodeCopyMaterial nodes. */
	void BreakExistingConnectionsPostConnection(UEdGraphPin* InputPin, UEdGraphPin* OutputPin) override;

	bool IsNodeOutDatedAndNeedsRefresh() override;
	FString GetRefreshMessage() const override;
	virtual TSharedPtr<SWidget> CustomizePinDetails(UEdGraphPin& Pin) override;

	
	// UCustomizableObjectNodeMaterialBase interface
	TArray<class UCustomizableObjectLayout*> GetLayouts() override;
	UEdGraphPin* OutputPin() const override;
	
	/** Return true if a Material Parameter has changed on which we had a pin connected or data saved. */
	bool RealMaterialDataHasChanged() const;
	
	// --------------------
	// ALL PARAMETERS
	// --------------------

	/** Returns the number of Material Parameters. */
	int32 GetNumParameters(EMaterialParameterType Type) const;

	/** Returns the Material Parameter id.
	 *
	 * @param ParameterIndex Have to be valid. */
	FGuid GetParameterId(EMaterialParameterType Type, int32 ParameterIndex) const;
	
	/** Returns the Material Parameter name.
	 *
	 * @param ParameterIndex Have to be valid. */
	FName GetParameterName(EMaterialParameterType Type, int32 ParameterIndex) const;
	
	/** Get the Material Parameter layer index.
	 *
	 * @param ParameterIndex Have to be valid.
	 * @returns INDEX_NONE for global parameters. */
	int32 GetParameterLayerIndex(EMaterialParameterType Type, int32 ParameterIndex) const;
	
	/** Get the Material Parameter layer name.
	 *
	 * @param ParameterIndex Have to be valid. */
	FText GetParameterLayerName(EMaterialParameterType Type, int32 ParameterIndex) const;

	/** Returns true if the Material contains the given Material Parameter. */
	bool HasParameter(const FGuid& ParameterId) const;
	
	/** Get the Vector pin for the given Material Vector Parameter.
	 * Not all parameters have pin.
	 *
	 * @param ParameterIndex Has to be valid.
	 * @return Can return nullptr. */
	const UEdGraphPin* GetParameterPin(EMaterialParameterType Type, int32 ParameterIndex) const;
	
	// --------------------
	// IMAGES PARAMETERS
	// --------------------

	/** Returns true if the Material Texture Parameter goes through Mutable.
	 *
	 * @param ImageIndex Have to be valid. */
	bool IsImageMutableMode(int32 ImageIndex) const;

	/** Given an Image pin, returns true if the Material Texture Parameter goes through Mutable. */
	bool IsImageMutableMode(const UEdGraphPin& Pin) const;

	/** Update a Material Texture Parameter Mode. */
	void UpdateImagePinMode(const FGuid ParameterId);
	
	/** Update a Material Texture Parameter Mode. */
	void UpdateImagePinMode(const UEdGraphPin& Pin);

	/** Update all Material Texture Parameters Mode. */
	void UpdateAllImagesPinMode();
	
	/** Returns the reference texture assigned to a Material Texture Parameter.
	 *
	 * @param ImageIndex Have to be valid.
	 * @return nullptr if it does not have one assigned. */
	UTexture2D* GetImageReferenceTexture(int32 ImageIndex) const;

	/** Returns the Texture set in the Material Texture Parameter.
	 *
	 * @param ImageIndex Have to be valid. */
	UTexture2D* GetImageValue(int32 ImageIndex) const;
	
	/** Get the Material Texture Parameter UV Index.
	 *
	 * @param ImageIndex Have to be valid.
	 * @return Return -1 if the UV Index is set to Ignore. Return >= 0 for a valid UV Index.  */
	int32 GetImageUVLayout(int32 ImageIndex) const;

	/** Delegate called when a Texture Parameter Pin Mode changes. */
	FPostImagePinModeChangedDelegate PostImagePinModeChangedDelegate;
	
private:
	/** Last static or skeletal mesh connected. Used to remove the callback once disconnected. */
	TWeakObjectPtr<UCustomizableObjectNode> LastMeshNodeConnected;

	static const TArray<EMaterialParameterType> ParameterTypes;

	/** Relates a Parameter id (key) to a Pin (value). Only used to improve performance. */
	UPROPERTY()
	TMap<FGuid, FEdGraphPinReference> PinsParameter;

	/** Relates an Image pin (key) to its Image Pin Mode (value). 
		Represents the real mode of the pin. Required due to some node configurations can force the mode independently of what the user had previously selected */
	UPROPERTY()
	TMap<FGuid, EPinMode> PinsImagePinMode;
	
	/** Create the pin data of the given parameter type. */
	UCustomizableObjectNodeMaterialPinDataParameter* CreatePinData(EMaterialParameterType Type, int32 ParameterIndex);

	/** Allocate a pin for each parameter of the given type. */
	void AllocateDefaultParameterPins(EMaterialParameterType Type);

	/** Set the default Material from the connected static or skeletal mesh. */
	void SetDefaultMaterial();

	/** Connected NodeStaticMesh or NodeSkeletalMesh Mesh UPROPERTY changed callback function. Sets the default material. */
	UFUNCTION()
	void MeshPostEditChangeProperty(FPostEditChangePropertyDelegateParameters& Parameters);

	/** Format pin name. */
	FName GetPinName(EMaterialParameterType Type, int32 ParameterIndex) const;

	/** Returns the texture coordinate of the given Material Expression. Returns -1 if not found. */
	static int32 GetExpressionTextureCoordinate(UMaterial* Material, const FGuid &ImageId);

	/** Converts node NodePinMode (does not include Default mode) to PinMode. */
	static EPinMode NodePinModeToImagePinMode(ENodePinMode NodePinMode);

	/** Returns the Image Pin Mode the pin should be at. It does not update its mode, to update it call UpdateImagePinMode. */
	EPinMode GetImagePinMode(const UEdGraphPin& Pin) const;

	/** Get the UV Layout Index defined in the Material. */
	int32 GetImageUVLayoutFromMaterial(int32 ImageIndex) const;
	
	// Deprecated properties
	/** Set all pins to Mutable mode. Even so, each pin can override its behaviour. */
	UPROPERTY()
	bool bDefaultPinModeMutable_DEPRECATED = false;
	
	UPROPERTY()
	TArray<FCustomizableObjectNodeMaterialImage> Images_DEPRECATED;
	
	UPROPERTY()
	TArray<FCustomizableObjectNodeMaterialVector> VectorParams_DEPRECATED;

	UPROPERTY()
	TArray<FCustomizableObjectNodeMaterialScalar> ScalarParams_DEPRECATED;
};


/** Additional data for a Material Texture Parameter pin. */
UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeMaterialPinDataImage : public UCustomizableObjectNodeMaterialPinDataParameter
{
	GENERATED_BODY()

	friend void UCustomizableObjectNodeMaterial::BackwardsCompatibleFixup();
	
public:
	// UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
private:
	virtual void PostLoad() override;
	
public:
	// NodePinDataParameter interface
	/** Virtual function used to copy pin data when remapping pins. */
	virtual void Copy(const UCustomizableObjectNodePinData& Other) override;
	
	// UCustomizableObjectNodeMaterialPinParameter interface
	virtual bool IsDefault() const override;

	// Own interface
	/** Constructor parameters. Should always be called after a NewObject. */
	void Init(UCustomizableObjectNodeMaterial& InNodeMaterial);

	EPinMode GetPinMode() const;

	void SetPinMode(EPinMode InPinMode);
	
private:
	/** Image pin mode. If is not default, overrides the defined node behaviour. */
	UPROPERTY(EditAnywhere, Category = NoCategory)
	EPinMode PinMode = EPinMode::Default;

public:
	constexpr static int32 UV_LAYOUT_IGNORE = -1;

	UPROPERTY(EditAnywhere, Category = NoCategory)
	EUVLayoutMode UVLayoutMode = EUVLayoutMode::FromMaterial;
	
	/** Index of the UV channel that will be used with this image.It is necessary to apply the proper layout transformations to it. */
	UPROPERTY(EditAnywhere, Category = NoCategory, meta = (EditCondition = "UVLayoutMode == EUVLayoutMode::Index", EditConditionHides))
	int32 UVLayout = -2;

	/** Reference Texture used to decide the texture properties of the mutable-generated textures
	* connected to this material. If null, it will try to be guessed at compile time from
	* the graph. */
	UPROPERTY(EditAnywhere, Category = NoCategory) // Required to be EditAnywhere for the selector to work.
	TObjectPtr<UTexture2D> ReferenceTexture = nullptr;

private:
	UPROPERTY()
	TObjectPtr<UCustomizableObjectNodeMaterial> NodeMaterial = nullptr;
};


/** Additional data for a Material Vector Parameter pin. */
UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeMaterialPinDataVector : public UCustomizableObjectNodeMaterialPinDataParameter
{
	GENERATED_BODY()
};


/** Additional data for a Material Float Parameter pin. */
UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeMaterialPinDataScalar : public UCustomizableObjectNodeMaterialPinDataParameter
{
	GENERATED_BODY()
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "MaterialTypes.h"
#include "MuCOE/CustomizableObjectEditor_Deprecated.h"
#endif
