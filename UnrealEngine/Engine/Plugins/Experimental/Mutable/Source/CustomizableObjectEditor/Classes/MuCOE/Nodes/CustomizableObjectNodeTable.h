// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataTable.h"
#include "Engine/Texture2DArray.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "MuCOE/RemapPins/CustomizableObjectNodeRemapPinsByName.h"

#include "CustomizableObjectNodeTable.generated.h"

namespace ENodeTitleType { enum Type : int; }

class UCustomizableObjectLayout;
class UCustomizableObjectNodeRemapPins;
class UCustomizableObjectNodeRemapPinsByName;
class UEdGraphPin;
class UObject;
class USkeletalMesh;
class UTexture2D;
class UTexture2DArray;
struct FGuid;


/** Enum class for the different types of image pins */
UENUM()
enum class ETableTextureType : uint8
{
	PASSTHROUGH_TEXTURE = 0 UMETA(DisplayName = "Passthrough"),
	MUTABLE_TEXTURE = 1 UMETA(DisplayName = "Mutable")
};


/** Enum class for the different types of pin meshes */
enum class ETableMeshPinType : uint8
{
	NONE = 0,
	SKELETAL_MESH = 1,
	STATIC_MESH = 2
};


/** Base class for all Table Pins. */
UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeTableObjectPinData : public UCustomizableObjectNodePinData
{
	GENERATED_BODY()

public:

	/** Name of the data table column related to the pin */
	UPROPERTY()
	FString ColumnName = "";

};


/** Additional data for Image pins. */
UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeTableImagePinData : public UCustomizableObjectNodeTableObjectPinData
{
	GENERATED_BODY()

public:

	bool IsDefaultImageMode() { return bIsDefault; }
	void SetDefaultImageMode(bool bValue) { bIsDefault = bValue; }

	bool IsArrayTexture() { return bIsArrayTexture; }
	void SetIsArrayTexture(bool bValue) { bIsArrayTexture = bValue; }

	// Pin Type
	UPROPERTY()
	ETableTextureType ImageMode = ETableTextureType::MUTABLE_TEXTURE;

private:

	UPROPERTY()
	bool bIsDefault = true;

	UPROPERTY()
	bool bIsArrayTexture = false;

};


/** Additional data for Mesh pins. */
UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeTableMeshPinData : public UCustomizableObjectNodeTableObjectPinData
{
	GENERATED_BODY()

public:
	
	/** Anim Instance Column name related to this Mesh pin */
	UPROPERTY()
	FString AnimInstanceColumnName = "";

	/** Anim Slot Column name related to this Mesh pin */
	UPROPERTY()
	FString AnimSlotColumnName = "";

	/** Anim Tag Column name related to this Mesh pin */
	UPROPERTY()
	FString AnimTagColumnName = "";

	UPROPERTY()
	FString MutableColumnName = "";

	/** LOD of the mesh related to this Mesh pin */
	UPROPERTY()
	int32 LOD = 0;

	/** Section Index (Surface Index) of the mesh related to this Mesh pin */
	UPROPERTY()
	int32 Material = 0;

	/** Layouts related to this Mesh pin */
	UPROPERTY()
	TArray< TObjectPtr<UCustomizableObjectLayout> > Layouts;
};


UCLASS()
class UCustomizableObjectNodeTableRemapPins : public UCustomizableObjectNodeRemapPinsByName
{
	GENERATED_BODY()
public:

	// Specific method to decide when two pins are equal
	virtual bool Equal(const UCustomizableObjectNode& Node, const UEdGraphPin& OldPin, const UEdGraphPin& NewPin) const override;

	// Method to use in the RemapPins step of the node reconstruction process
	virtual void RemapPins(const UCustomizableObjectNode& Node, const TArray<UEdGraphPin*>& OldPins, const TArray<UEdGraphPin*>& NewPins, TMap<UEdGraphPin*, UEdGraphPin*>& PinsToRemap, TArray<UEdGraphPin*>& PinsToOrphan) override;
};


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeTable : public UCustomizableObjectNode
{
public:
	
	GENERATED_BODY()

	// Name of the property parameter
	UPROPERTY(EditAnywhere, Category = TableProperties)
	FString ParameterName = "Default Name";

	/** If true, adds a "None" parameter option */
	UPROPERTY(EditAnywhere, Category = TableProperties)
	bool bAddNoneOption;

	// Pointer to the Data Table Asset represented in this node
	UPROPERTY(EditAnywhere, Category = TableProperties, meta = (DontUpdateWhileEditing))
	TObjectPtr<UDataTable> Table = nullptr;
	
	/** If there is a bool column in the table, checked rows will not be compiled */
	UPROPERTY(EditAnywhere, Category = TableProperties)
	bool bDisableCheckedRows = true;

	/** Decides the default type of the texture pins (passtrhough or mutable)
	*   Right click on a non-linked image pin to customize its image mode
	*/
	UPROPERTY(EditAnywhere, Category = TableProperties)
	ETableTextureType DefaultImageMode = ETableTextureType::MUTABLE_TEXTURE;

	UPROPERTY(EditAnywhere, Category = UI, meta = (DisplayName = "Parameter UI Metadata"))
	FMutableParamUIMetadata ParamUIMetadata;

	// UObject interface
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	
	// EdGraphNode interface
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FLinearColor GetNodeTitleColor() const override;
	FText GetTooltipText() const override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
	
	// UCustomizableObjectNode interface
	virtual void PostBackwardsCompatibleFixup() override;
	virtual void BackwardsCompatibleFixup() override;
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	bool IsNodeOutDatedAndNeedsRefresh() override;
	FString GetRefreshMessage() const override;
	virtual bool ProvidesCustomPinRelevancyTest() const override;
	virtual bool IsPinRelevant(const UEdGraphPin* Pin) const override;
	UCustomizableObjectNodeTableRemapPins* CreateRemapPinsDefault() const;

	/*** Allows to perform work when remapping the pin data. */
	virtual void RemapPinsData(const TMap<UEdGraphPin*, UEdGraphPin*>& PinsToRemap) override;
	
	// Own interface	
	// Returns the reference Texture parameter from a Material
	UTexture2D* FindReferenceTextureParameter(const UEdGraphPin* Pin, FString ParameterImageName) const;

	// Methods to get the UVs of the skeletal mesh 
	void GetUVChannel(const UCustomizableObjectLayout* CurrentLayout, TArray<FVector2f>& OutSegments) const;
	void GetUVChannelForPin(const UEdGraphPin* Pin, TArray<FVector2f>& OutSegments, int32 UVChannel = 0) const;

	// Methods to provide the Layouts to the Layout block editors
	void SetLayoutInLayoutEditor(UCustomizableObjectLayout* CurrentLayout);
	TArray<UCustomizableObjectLayout*> GetLayouts(const UEdGraphPin* Pin) const;

	// Returns the name of the table column related to a pin
	FString GetColumnNameByPin(const UEdGraphPin* Pin) const;

	// Returns the name of a Mesh Mutable Column from its LOD and pin (Only for AutomaticFromMesh LOD Strategy)
	FString GetMutableColumnName(const UEdGraphPin* Pin, const int32& LOD) const;

	// Returns the LOD of the mesh associated to the input pin
	void GetPinLODAndSection(const UEdGraphPin* Pin, int32& LODIndex, int32& SectionIndex) const;

	// Get the anim blueprint and anim slot columns related to a mesh
	void GetAnimationColumns(const FString& ColumnName, FString& AnimBPColumnName, FString& AnimSlotColumnName, FString& AnimTagColumnName) const;

	/** Callback called when the Table property or its contents has changed. */
	void OnTableChanged();
	
	template<class T> 
	T* GetColumnDefaultAssetByType(FString ColumnName) const
	{
		T* ObjectToReturn = nullptr;

		if (Table)
		{
			// Getting Struct Pointer
			const UScriptStruct* TableStruct = Table->GetRowStruct();

			if (TableStruct)
			{
				// Getting Default Struct Values
				uint8* DefaultRowData = (uint8*)FMemory::Malloc(TableStruct->GetStructureSize());
				TableStruct->InitializeStruct(DefaultRowData);

				if (DefaultRowData)
				{
					FProperty* Property = Table->FindTableProperty(FName(*ColumnName));

					if (Property)
					{
						if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(Property))
						{
							UObject* Object = nullptr;
							
							// Getting default UObject
							uint8* CellData = SoftObjectProperty->ContainerPtrToValuePtr<uint8>(DefaultRowData);

							if (CellData)
							{
								Object = SoftObjectProperty->GetPropertyValue(CellData).LoadSynchronous();
							}

							if (Object)
							{
								if (Object->IsA(T::StaticClass()))
								{
									ObjectToReturn = Cast <T>(Object);
								}
							}
						}
					}

					// Cleaning Default Structure Pointer
					TableStruct->DestroyStruct(DefaultRowData);
					FMemory::Free(DefaultRowData);
				}
			}
		}

		return ObjectToReturn;
	}

	template<class T>
	T* GetColumnDefaultAssetByType(const UEdGraphPin* Pin) const
	{
		if (Pin)
		{
			FString ColumnName = GetColumnNameByPin(Pin);

			if (ColumnName != FString())
			{
				return GetColumnDefaultAssetByType<T>(ColumnName);
			}
		}

		return nullptr;
	}


	// We should do this in a template!
	USkeletalMesh* GetSkeletalMeshAt(const UEdGraphPin* Pin, const FName& RowName) const;
	TSoftClassPtr<UAnimInstance> GetAnimInstanceAt(const UEdGraphPin* Pin, const FName& RowName) const;


	// Return the name of the enabled rows in the data table.
	// Returns the name if the row has a bool column set as false (true == disabled)
	TArray<FName> GetRowNames() const;

	// Changes the image mode of a pin
	// bSetDefault param: if true sets the pin to be equal to the default mode (same as node)
	void ChangeImagePinMode(UEdGraphPin* Pin, bool bSetDefault = false);

	// Returns true if the pin is in the default mode (same as node)
	bool IsImagePinDefault(UEdGraphPin* Pin);

	// Returns true if the pin is a texture array pin
	bool IsImageArrayPin(UEdGraphPin* Pin);

	// Returns the image mode of the column
	ETableTextureType GetColumnImageMode(const FString& ColumnName) const;

	// Returns the mesh type of the Pin
	ETableMeshPinType GetPinMeshType(const UEdGraphPin* Pin) const;

	// Functions to generate the names of a mutable table's column
	FString GenerateSkeletalMeshMutableColumName(const FString& PinName, int32 LODIndex, int32 MaterialIndex) const;
	FString GenerateStaticMeshMutableColumName(const FString& PinName, int32 MaterialIndex) const;

private:

	/** Number of properties to know when the node needs an update */
	UPROPERTY()
	int32 NumProperties;
	
	FDelegateHandle OnTableChangedDelegateHandle;
	
	// Generates a mesh pin for each LOD and Material Surface of the reference SkeletalMesh
	void GenerateMeshPins(UObject* Mesh, const FString& Name);

	// Checks if a pin already exists and if it has the same type as before the node refresh
	bool CheckPinUpdated(const FString& PinName, const FName& PinType) const;


};
