// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMXAttribute.h"
#include "DMXProtocolTypes.h"
#include "Library/DMXEntity.h"
#include "Library/DMXEntityReference.h"
#include "Modulators/DMXModulator.h"

#include "DMXEntityFixtureType.generated.h"

class UDMXImport;
class UDMXImportGDTF;


UENUM(BlueprintType)
enum class EDMXPixelMappingDistribution : uint8
{
	TopLeftToRight,
	TopLeftToBottom,
	TopLeftToClockwise,
	TopLeftToAntiClockwise,

	TopRightToLeft,
	BottomLeftToTop,
	TopRightToAntiClockwise,
	BottomLeftToClockwise,

	BottomLeftToRight,
	TopRightToBottom,
	BottomLeftAntiClockwise,
	TopRightToClockwise,

	BottomRightToLeft,
	BottomRightToTop,
	BottomRightToClockwise,
	BottomRightToAntiClockwise
};

USTRUCT(BlueprintType)
struct DMXRUNTIME_API FDMXFixtureFunction
{
	GENERATED_BODY()

	/** Constructor */
	FDMXFixtureFunction()
		: Attribute()
		, FunctionName()
		, Description()
		, DefaultValue(0)
		, Channel(1)
		, ChannelOffset_DEPRECATED(0)
		, DataType(EDMXFixtureSignalFormat::E8Bit)
		, bUseLSBMode(false)
	{}

	/** Returns the number of channels the function spans, according to its data type */
	FORCEINLINE uint8 GetNumChannels() const { return static_cast<uint8>(DataType) + 1; }

	/** Returns the last channel of the Function */
	int32 GetLastChannel() const;

	/**
	 * The Attribute name to map this Function to.
	 * This is used to easily find the Function in Blueprints, using an Attribute
	 * list instead of typing the Function name directly.
	 * The list of Attributes can be edited on
	 * Project Settings->Plugins->DMX Protocol->Fixture Settings->Fixture Function Attributes
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Attribute Mapping", DisplayPriority = "11"), Category = "Function Settings")
	FDMXAttributeName Attribute;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayPriority = "10"), Category = "Function Settings")
	FString FunctionName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayPriority = "20"), Category = "Function Settings")
	FString Description;

	/** The Default Value of the function, imported from GDTF. The plugin doesn't make use of this value, but it can be used in blueprints */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayPriority = "30"), Category = "Function Settings")
	int64 DefaultValue;

	/** This function's starting channel (use editor above to make changes) */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, meta = (DisplayName = "Channel Assignment", DisplayPriority = "2"), Category = "Function Settings")
	int32 Channel;

	/** DEPRECATED 5.0. Instead the 'Channel' property is EditAnywhere so any function can be assigned freely */
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Deprecated since the Channel property can be set in the DMX Library Editor."))
	int32 ChannelOffset_DEPRECATED;

	/** This function's data type. Defines the used number of channels (bytes) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayPriority = "5"), Category = "Function Settings")
	EDMXFixtureSignalFormat DataType;

	/**
	 * Least Significant Byte mode makes the individual bytes (channels) of the function be
	 * interpreted with the first bytes being the lowest part of the number (endianness).
	 * 
	 * E.g., given a 16 bit function with two channel values set to [0, 1],
	 * they would be interpreted as the binary number 0x01 0x00, which means 256.
	 * The first byte (0) became the lowest part in binary form and the following byte (1), the highest.
	 * 
	 * Most Fixtures use MSB (Most Significant Byte) mode, which interprets bytes as highest first.
	 * In MSB mode, the example above would be interpreted in binary as 0x00 0x01, which means 1.
	 * The first byte (0) became the highest part in binary form and the following byte (1), the lowest.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Use LSB Mode", DisplayPriority = "29"), Category = "Function Settings")
	bool bUseLSBMode = false;
};

USTRUCT(BlueprintType)
struct DMXRUNTIME_API FDMXFixtureCellAttribute
{
	GENERATED_BODY()

	/** Constructor */
	FDMXFixtureCellAttribute()
		: Attribute()
		, Description()
		, DefaultValue(0)
		, DataType(EDMXFixtureSignalFormat::E8Bit)
		, bUseLSBMode(false)
	{}

	/** Returns the number of channels of the attribute */
	uint8 GetNumChannels() const { return static_cast<uint8>(DataType) + 1; }

	/**
	 * The Attribute name to map this Function to.
	 * This is used to easily find the Function in Blueprints, using an Attribute
	 * list instead of typing the Function name directly.
	 * The list of Attributes can be edited on
	 * Project Settings->Plugins->DMX Protocol->Fixture Settings->Fixture Function Attributes
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Attribute Mapping", DisplayPriority = "11"), Category = "DMX")
	FDMXAttributeName Attribute;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayPriority = "20", DisplayName = "Description"), Category = "DMX")
	FString Description;

	/** Initial value for this function when no value is set */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayPriority = "30", DisplayName = "Default Value"), Category = "DMX")
	int64 DefaultValue;

	/** This function's data type. Defines the used number of channels (bytes) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayPriority = "5", DisplayName = "Data Type"), Category = "DMX")
	EDMXFixtureSignalFormat DataType;

	/**
	 * The Endianess of the Attribute:
	 * Least Significant Byte mode makes the individual bytes (channels) of the function be
	 * interpreted with the first bytes being the lowest part of the number.
	 *
	 * E.g., given a 16 bit function with two channel values set to [0, 1],
	 * they would be interpreted as the binary number 00000001 00000000, which means 256.
	 * The first byte (0) became the lowest part in binary form and the following byte (1), the highest.
	 *
	 * Most Fixtures use MSB (Most Significant Byte) mode, which interprets bytes as highest first.
	 * In MSB mode, the example above would be interpreted in binary as 00000000 00000001, which means 1.
	 * The first byte (0) became the highest part in binary form and the following byte (1), the lowest.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Use LSB Mode", DisplayPriority = "29"), Category = "DMX")
	bool bUseLSBMode;
};

USTRUCT(BlueprintType)
struct DMXRUNTIME_API FDMXFixtureMatrix
{
	GENERATED_BODY()

	/** Constructor */
	FDMXFixtureMatrix();

	/** Returns the number of channels of the Matrix */
	int32 GetNumChannels() const;

	/** Returns the last channel of the Matrix */
	int32 GetLastChannel() const;

	UE_DEPRECATED(5.0, "Deprecated for more consistent naming. Instead use FDMXFixtureMatrix::GetLastChannel")
	int32 GetFixtureMatrixLastChannel() const;

	UE_DEPRECATED(5.0, "Deprecated to reduce redundant code. Instead use UDMXFixturePatch::GetMatrixCellChannelsRelative")
	bool GetChannelsFromCell(FIntPoint CellCoordinate, FDMXAttributeName Attribute, TArray<int32>& Channels) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayPriority = "60", DisplayName = "Cell Attributes"), Category = "Mode Settings")
	TArray<FDMXFixtureCellAttribute> CellAttributes;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, meta = (DisplayPriority = "20", DisplayName = "First Cell Channel", ClampMin = "1", ClampMax = "512"), Category = "Mode Settings")
	int32 FirstCellChannel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayPriority = "30", DisplayName = "X Cells", ClampMin = "1", ClampMax = "512", UIMin = "1", UIMax = "512"), Category = "Mode Settings")
	int32 XCells = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayPriority = "40", DisplayName = "Y Cells", ClampMin = "1", ClampMax = "512", UIMin = "1", UIMax = "512"), Category = "Mode Settings")
	int32 YCells = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayPriority = "50", DisplayName = "PixelMapping Distribution"), Category = "Mode Settings")
	EDMXPixelMappingDistribution PixelMappingDistribution = EDMXPixelMappingDistribution::TopLeftToRight;
};

USTRUCT(BlueprintType)
struct DMXRUNTIME_API FDMXCell
{
	GENERATED_BODY()

	/** The cell index in a 1D Array (row order), starting from 0 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayPriority = "20", DisplayName = "Cell ID", ClampMin = "0"), Category = "DMX")
	int32 CellID;

	/** The cell coordinate in a 2D Array, starting from (0, 0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayPriority = "30", DisplayName = "Coordinate"), Category = "DMX")
	FIntPoint Coordinate;

	FDMXCell()
		: CellID(0)
		, Coordinate(FIntPoint (-1,-1))
	{}
};

USTRUCT(BlueprintType)
struct DMXRUNTIME_API FDMXFixtureMode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayPriority = "10"), Category = "Mode Settings")
	FString ModeName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayPriority = "20"), Category = "Mode Settings")
	TArray<FDMXFixtureFunction> Functions;

	/**
	 * When enabled, ChannelSpan is automatically set based on the created functions and their data types.
	 * If disabled, ChannelSpan can be manually set and functions and functions' channels beyond the
	 * specified span will be ignored.
	 */
	UPROPERTY(EditAnywhere, Category = "Mode Settings", meta = (DisplayPriority = "30"))
	bool bAutoChannelSpan = true;

	/** Number of channels (bytes) used by this mode's functions */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mode Settings", meta = (ClampMin = "4", ClampMax = "512", DisplayPriority = "40", EditCondition = "!bAutoChannelSpan"))
	int32 ChannelSpan = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mode Settings", meta = (DisplayPriority = "60"))
	bool bFixtureMatrixEnabled = false;

	UPROPERTY(EditAnywhere, Category = "Mode Settings", meta = (DisplayPriority = "70"))
	FDMXFixtureMatrix FixtureMatrixConfig;

#if WITH_EDITOR
	/** DEPRECATED 5.0 */
	UE_DEPRECATED(5.0, "Removed in favor of UDMXEntityFixtureType::AddFunction and UDMXEntityFixtureType::InsertFunction")
	int32 AddOrInsertFunction(int32 IndexOfFunction, FDMXFixtureFunction InFunction);
#endif
};


/** Parameters to construct a Fixture Type. */
USTRUCT(BlueprintType)
struct DMXRUNTIME_API FDMXEntityFixtureTypeConstructionParams
{
	GENERATED_BODY()

	/** The DMX Library in which the Fixture Type will be constructed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fixture Type")
	TObjectPtr<UDMXLibrary> ParentDMXLibrary = nullptr;

	/** The Category of the Fixture, useful for Filtering */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fixture Type")
	FDMXFixtureCategory DMXCategory;

	/** The Modes of the Fixture Type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fixture Type")
	TArray<FDMXFixtureMode> Modes;
};

#if WITH_EDITOR
/** Notification when data type changed */
DECLARE_MULTICAST_DELEGATE_TwoParams(FDataTypeChangeDelegate, const UDMXEntityFixtureType*, const FDMXFixtureMode&);
#endif

DECLARE_MULTICAST_DELEGATE_OneParam(FDMXOnFixtureTypeChangedDelegate, const UDMXEntityFixtureType* /** ChangedFixtureType */);


/** 
 * Class to describe a type of Fixture. Fixture Patches can be created from Fixture Types (see UDMXEntityFixturePatch::ParentFixtureTypeTemplate).
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "DMX Fixture Type"))
class DMXRUNTIME_API UDMXEntityFixtureType
	: public UDMXEntity
{
	GENERATED_BODY()

public:
	/** Creates a new Fixture Type in the DMX Library */
	UFUNCTION(BlueprintCallable, Category = "DMX")
	static UDMXEntityFixtureType* CreateFixtureTypeInLibrary(FDMXEntityFixtureTypeConstructionParams ConstructionParams, const FString& DesiredName = TEXT(""), bool bMarkDMXLibraryDirty = true);

	/** Removes a Fixture Type from a DMX Library */
	UFUNCTION(BlueprintCallable, Category = "DMX")
	static void RemoveFixtureTypeFromLibrary(FDMXEntityFixtureTypeRef FixtureTypeRef);

	//~ Begin UObject interface
	virtual void Serialize(FArchive& Ar) override;
#if WITH_EDITOR
	virtual bool Modify(bool bAlwaysMarkDirty = true) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedChainEvent) override;
	virtual void PostEditUndo() override;
#endif
	//~ End UObject interface

public:
#if WITH_EDITOR
	UFUNCTION(BlueprintCallable, Category = "Fixture Settings")
	void SetModesFromDMXImport(UDMXImport* DMXImportAsset);
#endif // WITH_EDITOR

	/** Returns a delegate that is and should be broadcast whenever a Fixture Type changed */
	static FDMXOnFixtureTypeChangedDelegate& GetOnFixtureTypeChanged();

	/** The GDTF file from which the Fixture Type was setup */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fixture Settings")
	TObjectPtr<UDMXImport> DMXImport;

	/** The Category of the Fixture, useful for Filtering */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fixture Settings", meta = (DisplayName = "DMX Category"))
	FDMXFixtureCategory DMXCategory;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fixture Settings")
	TArray<FDMXFixtureMode> Modes;

	/** 
	 * Modulators applied right before a patch of this type is received. 
	 * NOTE: Modulators only affect the patch's normalized values! Untouched values are still available when accesing raw values. 
	 */
	UPROPERTY(EditAnywhere, Instanced, Category = "Mode Settings", meta = (DisplayPriority = "50"))
	TArray<TObjectPtr<UDMXModulator>> InputModulators;

private:
	/** Delegate that should be broadcast whenever a fixture type changed */
	static FDMXOnFixtureTypeChangedDelegate OnFixtureTypeChangedDelegate;


	//////////////////////////////////////////////////
	// Helpers to edit the Fixture Type

	// Fixture Mode related
public:
	/**
	 * Adds a Mode to the Modes Array
	 * 
	 * @param(optional)						The Base Mode Name when generating a name
	 *
	 * @return								The Index of the newly added Mode.
	 */	
	int32 AddMode(FString BaseModeName = FString("Mode"));

	/** 
	 * Duplicates the Modes at specified Indices 
	 *
	 * @param InModeIndices					The indicies of the Modes to duplicate
	 * @param OutNewModeIndices				The indices of the newly created Modes.
	 */
	void DuplicateModes(TArray<int32> InModeIndicesToDuplicate, TArray<int32>& OutNewModeIndices);

	/** Deletes the Modes at specified Indices */
	void RemoveModes(const TArray<int32>& ModeIndicesToDelete);

	/** Sets a Mode Name for specified Mode 
	 * 
	 * @param InModeIndex					The index of the Mode in the Modes array
	 * @param InDesiredModeName				The desired Name that should be set
	 * @param OutUniqueModeName				The unique Name that was set
	*/
	void SetModeName(int32 InModeIndex, const FString& InDesiredModeName, FString& OutUniqueModeName);

	/** Enables or disables the Matrix, reorders Function channels accordingly
	 *
	 * @param ModeIndex						The index of the Mode for which the Matrix should be enabled or disabled.
	 * @param bEnableMatrix					Whether to enable or disable the Matrix
	*/
	void SetFixtureMatrixEnabled(int32 ModeIndex, bool bEnableMatrix);

	/**
	 * Updates the channel span of the Mode.
	 *
	 * @param ModeIndex						The Index of the Mode for which the Channel Span should be updated.
	 */
	void UpdateChannelSpan(int32 ModeIndex);

	/** Aligns alls channels of the functions in the Mode to be consecutive */
	void AlignFunctionChannels(int32 InModeIndex);

	// Fixture Function related
public:
	/** 
	 * Adds a new Function to the Mode's Functions array
	 *
	 * @param InModeIndex					The index of the Mode, that will have the Function added to its Functions array.
	 * @return								The Index of the newly added Function.
	 */
	int32 AddFunction(int32 InModeIndex);

	/** 
	 * Inserts a Function to the Mode's Function Array
	 *
	 * @param InModeIndex					The index of the Mode, that will have the Function added to its Functions array.
	 * @param InInsertAtIndex				If a valid Index, the Function will be inserted at this Index, and subsequent Function's Channels will be reodered after it.
	 * @param InOutNewFunction				The function that will be inserted.
	 * @return								The Index of the newly added Function.
	 */
	int32 InsertFunction(int32 InModeIndex, int32 InInsertAtIndex, FDMXFixtureFunction& InOutNewFunction);

	/**
	 * Adds a Function to the Mode's Function Array
	 *
	 * @param InModeIndex					The index of the Mode in which the Functions to duplicate reside.
	 * @param InFunctionIndicesToDuplicate	The indices of the Functions to duplicate.
	 * @param OutNewFunctionIndices			The Function indices where the newly added Functions reside
	 */
	void DuplicateFunctions(int32 InModeIndex, const TArray<int32>& InFunctionIndicesToDuplicate, TArray<int32>& OutNewFunctionIndices);

	/**
	 * Removes Functions from the Mode's Function Array
	 *
	 * @param InModeIndex					The index of the Mode in which the Functions to remove reside
	 * @param FunctionIndicesToDelete		The indices of the Functions to remove.
	 */
	void RemoveFunctions(int32 ModeIndex, TArray<int32> FunctionIndicesToDelete);

	/**
	 * Reorders a function to reside at the Insert At Index, subsequently reorders other affected Functions
	 *
	 * @param ModeIndex						The Index of the Mode in which the Functions reside
	 * @param FunctionIndex					The Index of the Function that is reorderd
	 * @param InsertAtIndex					The Index of the Function where the function is inserted.
	 */
	void ReorderFunction(int32 ModeIndex, int32 FunctionToReorderIndex, int32 InsertAtIndex);

	/** Sets a Mode Name for specified Mode
	 *
	 * @param InModeIndex					The index of the Mode in the Modes array
	 * @param InFunctionIndex				The index of the Function in the Mode's Function array
	 * @param DesiredFunctionName			The desired Name that should be set
	 * @param OutUniqueFunctionName			The unique Name that was set
	*/
	void SetFunctionName(int32 InModeIndex, int32 InFunctionIndex, const FString& InDesiredFunctionName, FString& OutUniqueFunctionName);

	/** Sets a Starting Channel for the Function, aligns it to other functions
	 *
	 * @param InModeIndex					The index of the Mode in the Modes array
	 * @param InFunctionIndex				The index of the Function in the Mode's Function array
	 * @param InDesiredStartingChannel		The desired Starting Channel that should be set
	 * @param OutStartingChannel			The resulting Starting Channel that was set
	*/
	void SetFunctionStartingChannel(int32 InModeIndex, int32 InFunctionIndex, int32 InDesiredStartingChannel, int32& OutStartingChannel);

	/**
	 * Clamps the Default Value of the Function by its Data Type
	 *
	 * @param ModeIndex						The Index of the Mode in which the Functions reside
	 * @param FunctionToRemoveIndex			The Index of the Function for which the Default Value is clamped
	 */
	void ClampFunctionDefautValueByDataType(int32 ModeIndex, int32 FunctionToRemoveIndex);


	// Fixture Matrix related
public:
	/** Adds a new cell attribute to the Mode */
	void AddCellAttribute(int32 ModeIndex);

	/** Removes a cell attribute to the Mode */
	void RemoveCellAttribute(int32 ModeIndex, int32 CellAttributeIndex);

	/**
	 * Reorders the Fixture Matrix to reside after a function, subsequently reorders other affected Functions
	 *
	 * @param FixtureType					The Fixture Type in which the Functions reside
	 * @param ModeIndex						The Index of the Mode in which the Functions reside
	 * @param InsertAfterFunctionIndex		The Index of the Function after which the Matrix is inserted. If an invalid index is specified, the Matrix will added after the last Function Channel.
	 */
	void ReorderMatrix(int32 ModeIndex, int32 InsertAtFunctionIndex);

	/**
	 * Updates the channel span of the Mode.
	 *
	 * @param ModeIndex						The Index of the Mode for which the YCells should be updated
	 */
	void UpdateYCellsFromXCells(int32 ModeIndex);

	/**
	 * Updates the channel span of the Mode.
	 *
	 * @param ModeIndex						The Index of the Mode for which the XCells should be updated
	 */
	void UpdateXCellsFromYCells(int32 ModeIndex);


	// Conversions. @TODO, move to FDMXConversions
public:
	//~ Conversions to/from Bytes, Int and Normalized Float values.
	static void FunctionValueToBytes(const FDMXFixtureFunction& InFunction, uint32 InValue, uint8* OutBytes);
	static void IntToBytes(EDMXFixtureSignalFormat InSignalFormat, bool bUseLSB, uint32 InValue, uint8* OutBytes);

	static uint32 BytesToFunctionValue(const FDMXFixtureFunction& InFunction, const uint8* InBytes);
	static uint32 BytesToInt(EDMXFixtureSignalFormat InSignalFormat, bool bUseLSB, const uint8* InBytes);

	static void FunctionNormalizedValueToBytes(const FDMXFixtureFunction& InFunction, float InValue, uint8* OutBytes);
	static void NormalizedValueToBytes(EDMXFixtureSignalFormat InSignalFormat, bool bUseLSB, float InValue, uint8* OutBytes);

	static float BytesToFunctionNormalizedValue(const FDMXFixtureFunction& InFunction, const uint8* InBytes);
	static float BytesToNormalizedValue(EDMXFixtureSignalFormat InSignalFormat, bool bUseLSB, const uint8* InBytes);



	//////////////////////////////////////////////////
	// Deprecated Members
	
public:
	UE_DEPRECATED(5.0, "Deprecated in favor of FDMXConversions::GetSignalFormatMaxValue.")
	static uint32 GetDataTypeMaxValue(EDMXFixtureSignalFormat DataType);

	UE_DEPRECATED(5.0, "Deprecated in favor of FDMXConversions::SizeOfSignalFormat.")
	static uint8 NumChannelsToOccupy(EDMXFixtureSignalFormat DataType);

	UE_DEPRECATED(5.0, "Deprecated in favor of FDMXFixtureFunction::GetLastChannel.")
	static uint8 GetFunctionLastChannel(const FDMXFixtureFunction& Function);

	UE_DEPRECATED(5.0, "Deprecated to reduce redundant code. Instead use FDMXFixtureFunction::GetNumChannels() and FDMXFixtureMode::ChannelSpan")
	static bool IsFunctionInModeRange(const FDMXFixtureFunction& InFunction, const FDMXFixtureMode& InMode, int32 ChannelOffset = 0);

	UE_DEPRECATED(5.0, "Deprecated since the use of the function and its name were not clear, leading to hard to read, complicated code.")
	static bool IsFixtureMatrixInModeRange(const FDMXFixtureMatrix& InFixtureMatrix, const FDMXFixtureMode& InMode, int32 ChannelOffset = 0);

	UE_DEPRECATED(5.0, "Deprecated to reduce redundant code. Use FDMXConversions::ClampValueBySignalFormat locally where clamping is required.")
	static void ClampDefaultValue(FDMXFixtureFunction& InFunction);

	UE_DEPRECATED(5.0, "Deprecated in favor of FDMXConversions now holding all conversions. Use FDMXConversions::ClampValueBySignalFormat instead.")
	static uint32 ClampValueToDataType(EDMXFixtureSignalFormat DataType, uint32 InValue);

#if WITH_EDITOR
	UE_DEPRECATED(5.0, "Deprecated because of unclear use. Set via FDMXFixtureFunction::DataType directly instead.")
	static void SetFunctionSize(FDMXFixtureFunction& InFunction, uint8 Size);

	UE_DEPRECATED(5.0, "Deprecated in favor of the more generic GetOnFixtureTypeChanged which is supported in non-editor builds as well.")
	static FDataTypeChangeDelegate& GetDataTypeChangeDelegate() { return DataTypeChangeDelegate_DEPRECATED; }

	UE_DEPRECATED(4.27, "Use MakeValid instead.")
	void UpdateModeChannelProperties(FDMXFixtureMode& Mode);

	UE_DEPRECATED(5.0, "Deprecated in favor of UDMXEntityFixtureType::UpdateChannelSpan(int32 ModeIndex).")
	void UpdateChannelSpan(FDMXFixtureMode& Mode);

	UE_DEPRECATED(5.0, "Deprecated  in favor of UDMXEntityFixtureType::UpdateYCellsFromXCells(int32 ModeIndex).")
	void UpdateYCellsFromXCells(FDMXFixtureMode& Mode);

	UE_DEPRECATED(5.0, "Deprecated to in favor of UDMXEntityFixtureType::UpdateXCellsFromYCells(int32 ModeIndex).")
	void UpdateXCellsFromYCells(FDMXFixtureMode& Mode);
#endif // WITH_EDITOR

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "FixtureMatrixEnabled is deprecated. Instead now each Mode has a FixtureMatrixEnabled property."))
	bool bFixtureMatrixEnabled = false;

private:
#if WITH_EDITOR
	/** Editor only data type change delagate */
	static FDataTypeChangeDelegate DataTypeChangeDelegate_DEPRECATED;
#endif
};
