// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Delegates/IDelegateInstance.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/DataTable.h"
#include "HAL/Platform.h"
#include "HAL/UnrealMemory.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "Math/Vector2D.h"
#include "MuCO/CustomizableObjectUIData.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "UObject/Class.h"
#include "UObject/Field.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectPtr.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

#include "CustomizableObjectNodeTable.generated.h"

class UCustomizableObjectLayout;
class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UObject;
class USkeletalMesh;
class UTexture2D;
struct FGuid;


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


/** Additional data for a Mesh pins. */
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

	/** Material Index (Surface Index) of the mesh related to this Mesh pin */
	UPROPERTY()
	int32 Material = 0;

	/** Layouts related to this Mesh pin */
	UPROPERTY()
	TArray< TObjectPtr<UCustomizableObjectLayout> > Layouts;
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
	
	// UCustomizableObjectNode interface
	virtual void PostBackwardsCompatibleFixup() override;
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	bool IsNodeOutDatedAndNeedsRefresh() override;
	FString GetRefreshMessage() const override;
	virtual bool ProvidesCustomPinRelevancyTest() const override;
	virtual bool IsPinRelevant(const UEdGraphPin* Pin) const override;

	/*** Allows to perform work when remapping the pin data. */
	virtual void RemapPinsData(const TMap<UEdGraphPin*, UEdGraphPin*>& PinsToRemap) override;
	
	// Own interface
	/** Given an Image pin, return the PinMode. */
	bool ForceImageMutableMode(const UEdGraphPin* Pin, FGuid ParameterId) const;
	
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
	void GetPinLODAndMaterial(const UEdGraphPin* Pin, int32& LOD, int32& Material) const;

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

	// Return the name of the enabled rows in the data table.
	// Returns the name if the row has a bool column set as false (true == disabled)
	TArray<FName> GetRowNames() const;

private:

	/** Number of properties to know when the node needs an update */
	UPROPERTY()
	int32 NumProperties;
	
	FDelegateHandle OnTableChangedDelegateHandle;
	
	// Generates a mesh pin for each LOD and Material Surface of the reference SkeletalMesh
	void GenerateMeshPins(UObject* Mesh, FString Name);

	// Checks if a pin already exists and if it has the same type as before the node refresh
	bool CheckPinUpdated(const FString& PinName, const FName& PinType) const;
};
