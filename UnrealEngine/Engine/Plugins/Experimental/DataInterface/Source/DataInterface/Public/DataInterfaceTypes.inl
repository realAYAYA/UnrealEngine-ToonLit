// Copyright Epic Games, Inc. All Rights Reserved.

/** Built in types for data interfaces */

#ifndef DATA_INTERFACE_TYPE
#error DATA_INTERFACE_TYPE is not defined!
#endif

DATA_INTERFACE_TYPE(bool, Bool)
DATA_INTERFACE_TYPE(uint8, Uint8)
DATA_INTERFACE_TYPE(uint16, Uint16)
DATA_INTERFACE_TYPE(uint32, Uint32)
DATA_INTERFACE_TYPE(uint64, Uint64)
DATA_INTERFACE_TYPE(int8, Int8)
DATA_INTERFACE_TYPE(int16, Int16)
DATA_INTERFACE_TYPE(int32, Int32)
DATA_INTERFACE_TYPE(int64, Int64)
DATA_INTERFACE_TYPE(float, Float)
DATA_INTERFACE_TYPE(double, Double)
DATA_INTERFACE_TYPE(FName, Name)
DATA_INTERFACE_TYPE(FVector, Vector)
DATA_INTERFACE_TYPE(FVector4, Vector4)
DATA_INTERFACE_TYPE(FQuat, Quat)
DATA_INTERFACE_TYPE(FTransform, Transform)
DATA_INTERFACE_TYPE(TScriptInterface<IDataInterface>, ScriptInterface_DataInterface)

#undef DATA_INTERFACE_TYPE