// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithCloth.h"

#include "Serialization/CustomVersion.h"



enum EDatasmithClothSerializationVersion
{
    EDCSV_Base = 0,
    EDCSV_WithPatternParameters = 1,

	// -----<new versions can be added before this line>-------------------------------------------------
    _EDCSV_LastPlusOne,
    _EDCSV_Last = _EDCSV_LastPlusOne -1
};

struct FDatasmithClothSerializationVersion
{
    const static FGuid GUID;

private:
    FDatasmithClothSerializationVersion() = default;
};


const FGuid FDatasmithClothSerializationVersion::GUID(0x28B01036, 0x66B4498F, 0x99425ACA, 0xDB78A9B5);

// Register the custom version with core
FCustomVersionRegistration GRegisterDatasmithClothCustomVersion(FDatasmithClothSerializationVersion::GUID, _EDCSV_Last, TEXT("DatasmithCloth"));


FArchive& operator<<(FArchive& Ar, FParameterData& ParameterData)
{
	Ar.UsingCustomVersion(FDatasmithClothSerializationVersion::GUID);

	static_assert(TVariantSize<decltype(ParameterData.Data)>::Value == 2, "Serialization code not synced with structure");
	static_assert(decltype(ParameterData.Data)::IndexOfType<TArray<float>>() == 0, "Serialization relies on this specific order");
	static_assert(decltype(ParameterData.Data)::IndexOfType<TArray<double>>() == 1, "Serialization relies on this specific order");

	Ar << ParameterData.Target;
	Ar << ParameterData.Data;

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FDatasmithClothPattern& Pattern)
{
	Ar.UsingCustomVersion(FDatasmithClothSerializationVersion::GUID);
	int32 ClothSerialVersion = Ar.CustomVer(FDatasmithClothSerializationVersion::GUID);

	Ar << Pattern.SimPosition;
	Ar << Pattern.SimRestPosition;
	Ar << Pattern.SimTriangleIndices;

	if (ClothSerialVersion >= EDCSV_WithPatternParameters)
	{
		Ar << Pattern.Parameters;
	}

	return Ar;
}


FArchive& operator<<(FArchive& Ar, FDatasmithClothPresetProperty& Property)
{
	Ar.UsingCustomVersion(FDatasmithClothSerializationVersion::GUID);

	Ar << Property.Name;
	Ar << Property.Value;

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FDatasmithClothPresetPropertySet& PropertySet)
{
	Ar.UsingCustomVersion(FDatasmithClothSerializationVersion::GUID);

	Ar << PropertySet.SetName;
	Ar << PropertySet.Properties;

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FDatasmithCloth& Cloth)
{
	Ar.UsingCustomVersion(FDatasmithClothSerializationVersion::GUID);

	Ar << Cloth.Patterns;
	Ar << Cloth.PropertySets;

	return Ar;
}
