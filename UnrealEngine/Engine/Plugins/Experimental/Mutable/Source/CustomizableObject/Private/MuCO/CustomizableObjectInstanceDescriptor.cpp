// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/CustomizableObjectInstanceDescriptor.h"

#include "HAL/PlatformCrt.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Math/Vector.h"
#include "Misc/AssertionMacros.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuR/Model.h"
#include "MuR/Parameters.h"
#include "MuR/RefCounted.h"
#include "Serialization/Archive.h"
#include "Serialization/StructuredArchiveAdapters.h"
#include "Templates/TypeHash.h"
#include "Trace/Detail/Channel.h"


const FString EmptyString;


FString GetAvailableOptionsString(const UCustomizableObject& CustomizableObject, const int32 ParameterIndexInObject)
{
	FString OptionsString;
	const int32 NumOptions = CustomizableObject.GetIntParameterNumOptions(ParameterIndexInObject);

	for (int32 k = 0; k < NumOptions; ++k)
	{
		OptionsString += CustomizableObject.GetIntParameterAvailableOption(ParameterIndexInObject, k);

		if (k < NumOptions - 1)
		{
			OptionsString += FString(", ");
		}
	}

	return OptionsString;
}


FCustomizableObjectInstanceDescriptor::FCustomizableObjectInstanceDescriptor(UCustomizableObject &Object)
{
	CustomizableObject = &Object;	
	Init();
}


void FCustomizableObjectInstanceDescriptor::SaveDescriptor(FArchive& Ar)
{
	check(CustomizableObject);

	// This is a non-portable but very compact descriptor if bUseCompactDescriptor is true. It assumes the compiled objects are the same
	// on both ends of the serialisation. That's why we iterate the parameters in the actual compiled
	// model, instead of the arrays in this class.

	bool bUseCompactDescriptor = UCustomizableObjectSystem::GetInstance()->IsCompactSerializationEnabled();

	Ar << bUseCompactDescriptor;

	// Not sure if this is needed, but it is small.
	Ar << State;

	int32 ModelParameterCount = CustomizableObject->GetParameterCount();

	if (!bUseCompactDescriptor)
	{
		Ar << ModelParameterCount;
	}

	for (int32 ModelParameterIndex = 0; ModelParameterIndex < ModelParameterCount; ++ModelParameterIndex)
	{
		const FString & Name = CustomizableObject->GetParameterName(ModelParameterIndex);
		EMutableParameterType Type = CustomizableObject->GetParameterType(ModelParameterIndex);

		if (!bUseCompactDescriptor)
		{
			check(Ar.IsSaving());
			Ar << const_cast<FString &>(Name);
		}

		switch (Type)
		{
		case EMutableParameterType::Bool:
		{
			bool Value = false;
			for (const FCustomizableObjectBoolParameterValue& P: BoolParameters)
			{
				if (P.ParameterName == Name)
				{
					Value = P.ParameterValue;
					break;
				}
			}
			Ar << Value;
			break;
		}

		case EMutableParameterType::Float:
		{
			float Value = 0.0f;
			TArray<float> RangeValues;
			for (const FCustomizableObjectFloatParameterValue& P : FloatParameters)
			{
				if (P.ParameterName == Name)
				{
					Value = P.ParameterValue;
					RangeValues = P.ParameterRangeValues;
					break;
				}
			}
			Ar << Value;
			Ar << RangeValues;
			break;
		}

		case EMutableParameterType::Int:
		{
			int32 Value = 0;
			FString ValueName;

			TArray<int32> Values;
			TArray<FString> ValueNames;

			bool bIsParamMultidimensional = false;

			int32 IntParameterIndex = FindIntParameterNameIndex(Name);
			if (IntParameterIndex >= 0)
				{					
				const FCustomizableObjectIntParameterValue & P = IntParameters[IntParameterIndex];
					Value = CustomizableObject->FindIntParameterValue(ModelParameterIndex,P.ParameterValueName);

				int32 ParameterIndexInObject = CustomizableObject->FindParameter(IntParameters[IntParameterIndex].ParameterName);
				bIsParamMultidimensional = IsParamMultidimensional(ParameterIndexInObject);

				if (bIsParamMultidimensional)
				{
					for (int i = 0; i < P.ParameterRangeValueNames.Num(); ++i)
					{
						ValueNames.Add(P.ParameterRangeValueNames[i]);
						Values.Add(CustomizableObject->FindIntParameterValue(ModelParameterIndex, P.ParameterRangeValueNames[i]));
					}
				}

				if (!bUseCompactDescriptor)
				{
					ValueName = P.ParameterValueName;
				}
			}

			if (bUseCompactDescriptor)
			{
				Ar << Value;

				if (bIsParamMultidimensional)
				{
					Ar << Values;
				}
			}
			else
			{
				Ar << ValueName;

				if (bIsParamMultidimensional)
				{
					Ar << ValueNames;
				}
			}

			break;
		}

		case EMutableParameterType::Color:
		{
			FLinearColor Value(FLinearColor::Black);
			for (const FCustomizableObjectVectorParameterValue& P : VectorParameters)
			{
				if (P.ParameterName == Name)
				{
					Value = P.ParameterValue;
					break;
				}
			}
			Ar << Value;

			break;
		}

		case EMutableParameterType::Texture:
		{
			uint64 Value = 0;
			for (const FCustomizableObjectTextureParameterValue& P : TextureParameters)
			{
				if (P.ParameterName == Name)
				{
					Value = P.ParameterValue;
					break;
				}
			}
			Ar << Value;

			break;
		}

		case EMutableParameterType::Projector:
		{
			FCustomizableObjectProjector Value;
			TArray<FCustomizableObjectProjector> RangeValues;

			for (const FCustomizableObjectProjectorParameterValue& P : ProjectorParameters)
			{
				if (P.ParameterName == Name)
				{
					Value = P.Value;
					RangeValues = P.RangeValues;
					break;
				}
			}
			Ar << Value;
			Ar << RangeValues;

			break;
		}

		default:
			// Parameter type replication not implemented.
			check(false);
		}
	}
}


void FCustomizableObjectInstanceDescriptor::LoadDescriptor(FArchive& Ar)
{
	check(CustomizableObject);

	// This is a non-portable but very compact descriptor if bUseCompactDescriptor is true. It assumes the compiled objects are the same
	// on both ends of the serialisation. That's why we iterate the parameters in the actual compiled
	// model, instead of the arrays in this class.

	bool bUseCompactDescriptor = UCustomizableObjectSystem::GetInstance()->IsCompactSerializationEnabled();

	Ar << bUseCompactDescriptor;

	// Not sure if this is needed, but it is small.
	Ar << State;
	
	int32 ModelParameterCount = CustomizableObject->GetParameterCount();

	if (!bUseCompactDescriptor)
	{
		Ar << ModelParameterCount;
	}

	for (int32 i = 0; i < ModelParameterCount; ++i)
	{
		FString Name;
		EMutableParameterType Type;
		int32 ModelParameterIndex = -1;

		if (bUseCompactDescriptor)
		{
			ModelParameterIndex = i;
			Name = CustomizableObject->GetParameterName(ModelParameterIndex);
			Type = CustomizableObject->GetParameterType(ModelParameterIndex);
		}
		else
		{
			Ar << Name;
			Type = CustomizableObject->GetParameterTypeByName(Name);
		}

		switch (Type)
		{
		case EMutableParameterType::Bool:
		{
			bool Value = false;
			Ar << Value;
			for (FCustomizableObjectBoolParameterValue& P : BoolParameters)
			{
				if (P.ParameterName == Name)
				{
					P.ParameterValue = Value;
					break;
				}
			}
			break;
		}

		case EMutableParameterType::Float:
		{
			float Value = 0.0f;
			TArray<float> RangeValues;
			Ar << Value;
			Ar << RangeValues;
			for (FCustomizableObjectFloatParameterValue& P : FloatParameters)
			{
				if (P.ParameterName == Name)
				{
					P.ParameterValue = Value;
					P.ParameterRangeValues = RangeValues;
					break;
				}
			}
			break;
		}

		case EMutableParameterType::Int:
		{
			int32 Value = 0;
			FString ValueName;

			TArray<int32> Values;
			TArray<FString> ValueNames;

			const int32 IntParameterIndex = FindIntParameterNameIndex(Name);
			const int32 ParameterIndexInObject = CustomizableObject->FindParameter(IntParameters[IntParameterIndex].ParameterName);
			const bool bIsParamMultidimensional = IsParamMultidimensional(ParameterIndexInObject);

			if (bUseCompactDescriptor)
			{
				Ar << Value;

				if (bIsParamMultidimensional)
				{
					Ar << Values;
				}
			}
			else
			{
				Ar << ValueName;

				if (bIsParamMultidimensional)
				{
					Ar << ValueNames;
				}
			}

			for (FCustomizableObjectIntParameterValue& P : IntParameters)
			{
				if (P.ParameterName == Name)
				{
					if (bUseCompactDescriptor)
					{
						P.ParameterValueName = CustomizableObject->FindIntParameterValueName(ModelParameterIndex, Value);
						//check((P.ParameterValueName.IsEmpty() && ValueName.Equals(FString("None"))) || P.ParameterValueName.Equals(ValueName));
						P.ParameterRangeValueNames.SetNum(Values.Num());

						for (int ParamIndex = 0; ParamIndex < Values.Num(); ++ParamIndex)
						{
							P.ParameterRangeValueNames[ParamIndex] = CustomizableObject->FindIntParameterValueName(ModelParameterIndex, Values[ParamIndex]);
						}
					}
					else
					{
						P.ParameterValueName = ValueName;
						P.ParameterRangeValueNames = ValueNames;
					}

					break;
				}
			}

			break;
		}

		case EMutableParameterType::Color:
		{
			FLinearColor Value(FLinearColor::Black);
			Ar << Value;
			for (FCustomizableObjectVectorParameterValue& P : VectorParameters)
			{
				if (P.ParameterName == Name)
				{
					P.ParameterValue = Value;
					break;
				}
			}

			break;
		}

		case EMutableParameterType::Texture:
		{
			uint64 Value = 0;
			Ar << Value;
			for (FCustomizableObjectTextureParameterValue& P : TextureParameters)
			{
				if (P.ParameterName == Name)
				{
					P.ParameterValue = Value;
					break;
				}
			}

			break;
		}

		case EMutableParameterType::Projector:
		{
			FCustomizableObjectProjector Value;
			TArray<FCustomizableObjectProjector> RangeValues;
			Ar << Value;
			Ar << RangeValues;

			for (FCustomizableObjectProjectorParameterValue& P : ProjectorParameters)
			{
				if (P.ParameterName == Name)
				{
					P.Value = Value;
					P.RangeValues = RangeValues;
					break;
				}
			}

			break;
		}

		default:
			// Parameter type replication not implemented.
			check(false);
		}
	}

	CreateParametersLookupTable();
}


UCustomizableObject* FCustomizableObjectInstanceDescriptor::GetCustomizableObject() const
{
	return CustomizableObject;
}


bool FCustomizableObjectInstanceDescriptor::GetBuildParameterDecorations() const
{
	return bBuildParameterDecorations;
}


void FCustomizableObjectInstanceDescriptor::SetBuildParameterDecorations(const bool Value)
{
	bBuildParameterDecorations = Value;
}


void FCustomizableObjectInstanceDescriptor::SetCustomizableObject(UCustomizableObject* InCustomizableObject)
{
	CustomizableObject = InCustomizableObject;
	Init();
}


mu::ParametersPtr FCustomizableObjectInstanceDescriptor::ReloadParametersFromObject()
{
	if (!CustomizableObject)
	{
		return nullptr;
	}

	if (!CustomizableObject->IsCompiled())
	{	
		return nullptr;
	}
	
	int32 OldState = GetState();
	TArray<FCustomizableObjectBoolParameterValue> OldBoolParameters = GetBoolParameters();
	TArray<FCustomizableObjectIntParameterValue> OldIntParameters = GetIntParameters();
	TArray<FCustomizableObjectFloatParameterValue> OldFloatParameters = GetFloatParameters();
	TArray<FCustomizableObjectTextureParameterValue> OldTextureParameters = GetTextureParameters();
	TArray<FCustomizableObjectVectorParameterValue> OldVectorParameters = GetVectorParameters();
	TArray<FCustomizableObjectProjectorParameterValue> OldProjectorParameters = GetProjectorParameters();

	mu::ParametersPtr MutableParameters;
	SetState(FMath::Clamp(OldState, 0, CustomizableObject->GetStateCount() - 1));
	GetBoolParameters().Reset();
	GetIntParameters().Reset();
	GetFloatParameters().Reset();
	GetTextureParameters().Reset();
	GetVectorParameters().Reset();
	GetProjectorParameters().Reset();
	
	if (!CustomizableObject->GetPrivate()->GetModel())
	{
		UE_LOG(LogMutable, Warning, TEXT("[ReloadParametersFromObject] No model in object [%s], generated empty parameters for [%s] "), *CustomizableObject->GetName());
		return nullptr;
	}

	TArray<bool> OldIntParametersUsed;
	OldIntParametersUsed.SetNumUninitialized(OldIntParameters.Num());
	FMemory::Memzero(OldIntParametersUsed.GetData(), OldIntParametersUsed.GetAllocatedSize());

	MutableParameters = CustomizableObject->GetPrivate()->GetModel()->NewParameters();
	int32 paramCount = MutableParameters->GetCount();

	for (int32 paramIndex = 0; paramIndex<paramCount; ++paramIndex)
	{
		FString Name = MutableParameters->GetName(paramIndex);
		FString Uid = MutableParameters->GetUid(paramIndex);
		mu::PARAMETER_TYPE mutableType = MutableParameters->GetType(paramIndex);

		switch (mutableType)
		{
		case mu::PARAMETER_TYPE::T_BOOL:
		{
			FCustomizableObjectBoolParameterValue param;
			param.ParameterName = Name;
			param.Uid = Uid;

			bool found = false;
			for (int32 i = 0; i < OldBoolParameters.Num(); ++i)
			{
				if (OldBoolParameters[i].ParameterName == Name)
				{
					found = true;
					param.ParameterValue = OldBoolParameters[i].ParameterValue;
					MutableParameters->SetBoolValue(paramIndex, param.ParameterValue);
					break;
				}
			}

			if (!found)
			{
				for (int32 i = 0; i < OldBoolParameters.Num(); ++i)
				{
					if (!Uid.IsEmpty() && OldBoolParameters[i].Uid == Uid)
					{
						found = true;
						param.ParameterValue = OldBoolParameters[i].ParameterValue;
						MutableParameters->SetBoolValue(paramIndex, param.ParameterValue);
						break;
					}
				}
			}

			if (!found)
			{
				param.ParameterValue = MutableParameters->GetBoolValue(paramIndex);
			}

			GetBoolParameters().Add(param);
			break;
		}

		case mu::PARAMETER_TYPE::T_INT:
		{
			FString ParameterValueName;
			TArray<FString> ParameterRangeValueNames;

			bool found = false;
			for (int32 i = 0; i < OldIntParameters.Num(); ++i)
			{
				if (OldIntParametersUsed[i] == false && OldIntParameters[i].ParameterName.Equals(Name, ESearchCase::CaseSensitive))
				{
					found = true;
					OldIntParametersUsed[i] = true;

					mu::RangeIndexPtr RangeIdxPtr = MutableParameters->NewRangeIndex(paramIndex);

					if (!RangeIdxPtr)
					{
					ParameterValueName = OldIntParameters[i].ParameterValueName;
					int32 Value = CustomizableObject->FindIntParameterValue(paramIndex, ParameterValueName);
					MutableParameters->SetIntValue(paramIndex, Value);
					}
					else
					{
						for (int RangeIndex = 0; RangeIndex < OldIntParameters[i].ParameterRangeValueNames.Num(); ++RangeIndex)
						{
							FString AuxParameterValueName = OldIntParameters[i].ParameterRangeValueNames[RangeIndex];
							ParameterRangeValueNames.Add(AuxParameterValueName);
							int32 Value = CustomizableObject->FindIntParameterValue(paramIndex, AuxParameterValueName);
							RangeIdxPtr->SetPosition(0, RangeIndex);
							MutableParameters->SetIntValue(paramIndex, Value, RangeIdxPtr);
						}
					}

					break;
				}
			}

			if (!found)
			{
				for (int32 i = 0; i < OldIntParameters.Num(); ++i)
				{
					if (!Uid.IsEmpty() && OldIntParametersUsed[i] == false && OldIntParameters[i].Uid == Uid)
					{
						found = true;
						OldIntParametersUsed[i] = true;

						mu::RangeIndexPtr RangeIdxPtr = MutableParameters->NewRangeIndex(paramIndex);

						if (!RangeIdxPtr)
						{
						ParameterValueName = OldIntParameters[i].ParameterValueName;
						int32 Value = CustomizableObject->FindIntParameterValue(paramIndex, ParameterValueName);
						MutableParameters->SetIntValue(paramIndex, Value);
						}
						else
						{
							for (int RangeIndex = 0; RangeIndex < OldIntParameters[i].ParameterRangeValueNames.Num(); ++RangeIndex)
							{
								FString AuxParameterValueName = OldIntParameters[i].ParameterRangeValueNames[RangeIndex];
								ParameterRangeValueNames.Add(AuxParameterValueName);
								int32 Value = CustomizableObject->FindIntParameterValue(paramIndex, AuxParameterValueName);
								RangeIdxPtr->SetPosition(0, RangeIndex);
								MutableParameters->SetIntValue(paramIndex, Value, RangeIdxPtr);
							}
						}

						break;
					}
				}
			}

			if (!found)
			{
				mu::RangeIndexPtr RangeIdxPtr = MutableParameters->NewRangeIndex(paramIndex);

				if (!RangeIdxPtr.get())
				{
				int32 Value = MutableParameters->GetIntValue(paramIndex);
				ParameterValueName = CustomizableObject->FindIntParameterValueName(paramIndex, Value);
				}
				else
				{
					int32 ValueCount = MutableParameters->GetValueCount(paramIndex);

					for (int32 ValueIterator = 0; ValueIterator < ValueCount; ++ValueIterator)
					{
						mu::RangeIndexPtr RangeValueIdxPtr = MutableParameters->GetValueIndex(paramIndex, ValueIterator);
						int32 ValueIndex = RangeValueIdxPtr->GetPosition(0);

						//if (!param.RangeValues.IsValidIndex(ValueIndex))
						//{
						//	param.RangeValues.AddDefaulted(ValueIndex + 1 - param.RangeValues.Num());
						//}

						int32 Value = MutableParameters->GetIntValue(paramIndex, RangeValueIdxPtr);
						FString AuxParameterValueName = CustomizableObject->FindIntParameterValueName(paramIndex, Value);
						//param.RangeValues[ValueIndex] = AuxParameterValueName;

						if (!ParameterRangeValueNames.IsValidIndex(ValueIndex))
						{
							ParameterRangeValueNames.AddDefaulted(ValueIndex + 1 - ParameterRangeValueNames.Num());
						}

						ParameterRangeValueNames[ValueIndex] = AuxParameterValueName;
					}
				}
			}

			GetIntParameters().Emplace(Name, ParameterValueName, Uid, ParameterRangeValueNames);

			break;
		}

		case mu::PARAMETER_TYPE::T_FLOAT:
		{
			FCustomizableObjectFloatParameterValue param;
			param.ParameterName = Name;
			param.Uid = Uid;

			bool found = false;
			for (int32 i = 0; i < OldFloatParameters.Num(); ++i)
			{
				if (OldFloatParameters[i].ParameterName == Name || (!Uid.IsEmpty() && OldFloatParameters[i].Uid == Uid))
				{
					found = true;
					param.ParameterValue = OldFloatParameters[i].ParameterValue;
					param.ParameterRangeValues = OldFloatParameters[i].ParameterRangeValues;

					mu::RangeIndexPtr RangeIdxPtr = MutableParameters->NewRangeIndex(paramIndex);

					if (!RangeIdxPtr)
					{
						MutableParameters->SetFloatValue(paramIndex, param.ParameterValue);
					}
					else
					{
						for (int RangeIndex = 0; RangeIndex < OldFloatParameters[i].ParameterRangeValues.Num(); ++RangeIndex)
						{
							RangeIdxPtr->SetPosition(0, RangeIndex);
							MutableParameters->SetFloatValue(paramIndex, param.ParameterRangeValues[RangeIndex], RangeIdxPtr);
						}
					}
				}
			}

			if (!found)
			{
				mu::RangeIndexPtr RangeIdxPtr = MutableParameters->NewRangeIndex(paramIndex);

				if (!RangeIdxPtr.get())
				{
				param.ParameterValue = MutableParameters->GetFloatValue(paramIndex);
			}
				else
				{
				param.ParameterValue = MutableParameters->GetFloatValue(paramIndex);

					int32 ValueCount = MutableParameters->GetValueCount(paramIndex);

					for (int32 ValueIterator = 0; ValueIterator < ValueCount; ++ValueIterator)
					{
						mu::RangeIndexPtr RangeValueIdxPtr = MutableParameters->GetValueIndex(paramIndex, ValueIterator);
						int32 ValueIndex = RangeValueIdxPtr->GetPosition(0);

						float Value = MutableParameters->GetFloatValue(paramIndex, RangeValueIdxPtr);

						if (!param.ParameterRangeValues.IsValidIndex(ValueIndex))
						{
							param.ParameterRangeValues.AddDefaulted(ValueIndex + 1 - param.ParameterRangeValues.Num());
						}

						param.ParameterRangeValues[ValueIndex] = Value;
					}
				}
			}

			GetFloatParameters().Add(param);
			break;
		}

		case mu::PARAMETER_TYPE::T_COLOUR:
		{
			FCustomizableObjectVectorParameterValue param;
			param.ParameterName = Name;
			param.Uid = Uid;

			bool found = false;
			for (int32 i = 0; i < OldVectorParameters.Num(); ++i)
			{
				if (OldVectorParameters[i].ParameterName == Name || (!Uid.IsEmpty() && OldVectorParameters[i].Uid == Uid))
				{
					found = true;
					param.ParameterValue = OldVectorParameters[i].ParameterValue;
					MutableParameters->SetColourValue(paramIndex, param.ParameterValue.R, param.ParameterValue.G, param.ParameterValue.B);
				}
			}

			if (!found)
			{
				MutableParameters->GetColourValue(paramIndex, &param.ParameterValue.R, &param.ParameterValue.G, &param.ParameterValue.B);
				param.ParameterValue.A = 1.0f;
			}

			GetVectorParameters().Add(param);
			break;
		}

		case mu::PARAMETER_TYPE::T_PROJECTOR:
		{
			FCustomizableObjectProjectorParameterValue param;
			param.ParameterName = Name;
			param.Uid = Uid;

			bool found = false;
			for (int32 i = 0; i < OldProjectorParameters.Num(); ++i)
			{
				if (OldProjectorParameters[i].ParameterName == Name || (!Uid.IsEmpty() && OldProjectorParameters[i].Uid == Uid))
				{
					found = true;

					param.Value = OldProjectorParameters[i].Value;
					param.RangeValues = OldProjectorParameters[i].RangeValues;

					mu::RangeIndexPtr RangeIdxPtr = MutableParameters->NewRangeIndex(paramIndex);

					TArray<FCustomizableObjectProjector> AuxValuesArray;
					AuxValuesArray.Add(param.Value);
					TArray<FCustomizableObjectProjector>& Values = RangeIdxPtr.get() ? param.RangeValues : AuxValuesArray;
					int32 ArrayIndex = 0;

					for (FCustomizableObjectProjector& Value : Values)
					{
						if (RangeIdxPtr.get())
						{
							check(RangeIdxPtr->GetRangeCount() == 1);
							RangeIdxPtr->SetPosition(0, ArrayIndex);
						}

						switch (Value.ProjectionType)
						{
						case ECustomizableObjectProjectorType::Planar:
						case ECustomizableObjectProjectorType::Wrapping:
						{
							MutableParameters->SetProjectorValue(paramIndex,
								Value.Position[0], Value.Position[1], Value.Position[2],
								Value.Direction[0], Value.Direction[1], Value.Direction[2],
								Value.Up[0], Value.Up[1], Value.Up[2],
								Value.Scale[0], Value.Scale[1], Value.Scale[2],
								Value.Angle,
								RangeIdxPtr);
							break;
						}

						case ECustomizableObjectProjectorType::Cylindrical:
						{
							// Apply strange swizzle for scales
							// TODO: try to avoid this
								float Radius = FMath::Abs(Value.Scale[0] / 2.0f);
								float Height = Value.Scale[2];
							// TODO: try to avoid this
							MutableParameters->SetProjectorValue(paramIndex,
									Value.Position[0], Value.Position[1], Value.Position[2],
									-Value.Direction[0], -Value.Direction[1], -Value.Direction[2],
									-Value.Up[0], -Value.Up[1], -Value.Up[2],
									-Height, Radius, Radius,
									Value.Angle,
									RangeIdxPtr);
							break;
						}

						default:
							// Not implemented.
							check(false);
						}
						
						ArrayIndex++;
					}
				}
			}

			if (!found)
			{
				mu::RangeIndexPtr RangeIdxPtr = MutableParameters->NewRangeIndex(paramIndex);

				if (!RangeIdxPtr.get())
				{
				mu::PROJECTOR_TYPE type;
				MutableParameters->GetProjectorValue(paramIndex,
					&type,
					&param.Value.Position[0], &param.Value.Position[1], &param.Value.Position[2],
					&param.Value.Direction[0], &param.Value.Direction[1], &param.Value.Direction[2],
					&param.Value.Up[0], &param.Value.Up[1], &param.Value.Up[2],
					&param.Value.Scale[0], &param.Value.Scale[1], &param.Value.Scale[2],
					&param.Value.Angle );
				switch (type)
				{
				case mu::PROJECTOR_TYPE::PLANAR:
					param.Value.ProjectionType = ECustomizableObjectProjectorType::Planar;
					break;

				case mu::PROJECTOR_TYPE::CYLINDRICAL:
					// Unapply strange swizzle for scales.
					// TODO: try to avoid this
					param.Value.ProjectionType = ECustomizableObjectProjectorType::Cylindrical;
					param.Value.Direction = -param.Value.Direction;
					param.Value.Up = -param.Value.Up;
					param.Value.Scale[2] = -param.Value.Scale[0];
					param.Value.Scale[0] = param.Value.Scale[1] = param.Value.Scale[1] * 2.0f;
					break;

				case mu::PROJECTOR_TYPE::WRAPPING:
					param.Value.ProjectionType = ECustomizableObjectProjectorType::Wrapping;
					break;
				default:
					// not implemented
					check(false);
				}
			}
				else
				{
					int32 ValueCount = MutableParameters->GetValueCount(paramIndex);

					for (int32 ValueIterator = 0; ValueIterator < ValueCount; ++ValueIterator)
					{
						mu::RangeIndexPtr RangeValueIdxPtr = MutableParameters->GetValueIndex(paramIndex, ValueIterator);
						int32 ValueIndex = RangeValueIdxPtr->GetPosition(0);

						if (!param.RangeValues.IsValidIndex(ValueIndex))
						{
							param.RangeValues.AddDefaulted(ValueIndex + 1 - param.RangeValues.Num());
						}

						mu::PROJECTOR_TYPE type;
						MutableParameters->GetProjectorValue(paramIndex,
						&type,
						&param.RangeValues[ValueIndex].Position[0], &param.RangeValues[ValueIndex].Position[1], &param.RangeValues[ValueIndex].Position[2],
						&param.RangeValues[ValueIndex].Direction[0], &param.RangeValues[ValueIndex].Direction[1], &param.RangeValues[ValueIndex].Direction[2],
						&param.RangeValues[ValueIndex].Up[0], &param.RangeValues[ValueIndex].Up[1], &param.RangeValues[ValueIndex].Up[2],
						&param.RangeValues[ValueIndex].Scale[0], &param.RangeValues[ValueIndex].Scale[1], &param.RangeValues[ValueIndex].Scale[2],
						&param.RangeValues[ValueIndex].Angle,
						RangeValueIdxPtr);

						switch (type)
						{
						case mu::PROJECTOR_TYPE::PLANAR:
							param.RangeValues[ValueIndex].ProjectionType = ECustomizableObjectProjectorType::Planar;
							break;

						case mu::PROJECTOR_TYPE::CYLINDRICAL:
							// Unapply strange swizzle for scales.
							// TODO: try to avoid this
							param.RangeValues[ValueIndex].ProjectionType = ECustomizableObjectProjectorType::Cylindrical;
							param.RangeValues[ValueIndex].Direction = -param.RangeValues[ValueIndex].Direction;
							param.RangeValues[ValueIndex].Up = -param.RangeValues[ValueIndex].Up;
							param.RangeValues[ValueIndex].Scale[2] = -param.RangeValues[ValueIndex].Scale[0];
							param.RangeValues[ValueIndex].Scale[0] = param.RangeValues[ValueIndex].Scale[1] = param.RangeValues[ValueIndex].Scale[1] * 2.0f;
							break;

						case mu::PROJECTOR_TYPE::WRAPPING:
							param.RangeValues[ValueIndex].ProjectionType = ECustomizableObjectProjectorType::Wrapping;
							break;
						default:
							// not implemented
							check(false);
						}
					}
				}
			}

			GetProjectorParameters().Add(param);
			break;
		}

		case mu::PARAMETER_TYPE::T_IMAGE:
		{
			FCustomizableObjectTextureParameterValue param;
			param.ParameterName = Name;
			param.Uid = Uid;

			bool found = false;
			for (int32 i = 0; i < OldTextureParameters.Num(); ++i)
			{
				if (OldTextureParameters[i].ParameterName == Name || (!Uid.IsEmpty() && OldTextureParameters[i].Uid == Uid))
				{
					found = true;
					param.ParameterValue = OldTextureParameters[i].ParameterValue;
					MutableParameters->SetImageValue(paramIndex, param.ParameterValue);
				}
			}

			if (!found)
			{
				param.ParameterValue = MutableParameters->GetImageValue(paramIndex);
			}

			GetTextureParameters().Add(param);
			break;
		}

		default:
			// TODO
			break;

		}
	}
	
	CreateParametersLookupTable();

	return MutableParameters;
}


TArray<FCustomizableObjectBoolParameterValue>& FCustomizableObjectInstanceDescriptor::GetBoolParameters()
{
	return BoolParameters;
}


const TArray<FCustomizableObjectBoolParameterValue>& FCustomizableObjectInstanceDescriptor::GetBoolParameters() const
{
	return const_cast<FCustomizableObjectInstanceDescriptor*>(this)->GetBoolParameters();
}


TArray<FCustomizableObjectIntParameterValue>& FCustomizableObjectInstanceDescriptor::GetIntParameters()
{
	return IntParameters;
}


const TArray<FCustomizableObjectIntParameterValue>& FCustomizableObjectInstanceDescriptor::GetIntParameters() const
{
	return const_cast<FCustomizableObjectInstanceDescriptor*>(this)->GetIntParameters();	
}


TArray<FCustomizableObjectFloatParameterValue>& FCustomizableObjectInstanceDescriptor::GetFloatParameters()
{
	return FloatParameters;	
}


const TArray<FCustomizableObjectFloatParameterValue>& FCustomizableObjectInstanceDescriptor::GetFloatParameters() const
{
	return const_cast<FCustomizableObjectInstanceDescriptor*>(this)->GetFloatParameters();	
}


TArray<FCustomizableObjectTextureParameterValue>& FCustomizableObjectInstanceDescriptor::GetTextureParameters()
{
	return TextureParameters;	
}


const TArray<FCustomizableObjectTextureParameterValue>& FCustomizableObjectInstanceDescriptor::GetTextureParameters() const
{
	return const_cast<FCustomizableObjectInstanceDescriptor*>(this)->GetTextureParameters();	
}


TArray<FCustomizableObjectVectorParameterValue>& FCustomizableObjectInstanceDescriptor::GetVectorParameters()
{
	return VectorParameters;	
}


const TArray<FCustomizableObjectVectorParameterValue>& FCustomizableObjectInstanceDescriptor::GetVectorParameters() const
{
	return const_cast<FCustomizableObjectInstanceDescriptor*>(this)->GetVectorParameters();
}


TArray<FCustomizableObjectProjectorParameterValue>& FCustomizableObjectInstanceDescriptor::GetProjectorParameters()
{
	return ProjectorParameters;	
}


const TArray<FCustomizableObjectProjectorParameterValue>& FCustomizableObjectInstanceDescriptor::GetProjectorParameters() const
{
	return const_cast<FCustomizableObjectInstanceDescriptor*>(this)->GetProjectorParameters();	
}


bool FCustomizableObjectInstanceDescriptor::HasAnyParameters() const
{
	return !BoolParameters.IsEmpty() ||
		!IntParameters.IsEmpty() ||
		!FloatParameters.IsEmpty() || 
		!TextureParameters.IsEmpty() ||
		!ProjectorParameters.IsEmpty() ||
		!VectorParameters.IsEmpty();
}


const FString& FCustomizableObjectInstanceDescriptor::GetIntParameterSelectedOption(const FString& ParamName, const int32 RangeIndex) const
{
	check(CustomizableObject);
	
	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(ParamName);

	const int32 IntParamIndex = FindIntParameterNameIndex(ParamName);

	if (ParameterIndexInObject >= 0 && IntParamIndex >= 0)
	{
		if (RangeIndex == -1)
		{
			check(!IsParamMultidimensional(ParameterIndexInObject)); // This param is multidimensional, it must have a RangeIndex of 0 or more
			return IntParameters[IntParamIndex].ParameterValueName;
		}
		else
		{
			check(IsParamMultidimensional(ParameterIndexInObject)); // This param is not multidimensional, it must have a RangeIndex of -1

			if (IntParameters[IntParamIndex].ParameterRangeValueNames.IsValidIndex(RangeIndex))
			{
				return IntParameters[IntParamIndex].ParameterRangeValueNames[RangeIndex];
			}
		}
	}

	return EmptyString;
}


void FCustomizableObjectInstanceDescriptor::SetIntParameterSelectedOption(const int32 IntParamIndex, const FString& SelectedOption, const int32 RangeIndex)
{
	check(CustomizableObject);

	check(IntParameters.IsValidIndex(IntParamIndex));

	if (IntParameters.IsValidIndex(IntParamIndex))
	{
		const int32 ParameterIndexInObject = CustomizableObject->FindParameter(IntParameters[IntParamIndex].ParameterName);
		if (ParameterIndexInObject >= 0)
		{
			const bool bValid = SelectedOption == TEXT("None") || CustomizableObject->FindIntParameterValue(ParameterIndexInObject, SelectedOption) >= 0;
			if (!bValid)
			{
				const FString Message = FString::Printf(
					TEXT("LogMutable: Tried to set the invalid value [%s] to parameter [%d, %s]! Value index=[%d]. Correct values=[%s]."), 
					*SelectedOption, ParameterIndexInObject,
					*IntParameters[IntParamIndex].ParameterName, 
					CustomizableObject->FindIntParameterValue(ParameterIndexInObject, SelectedOption), 
					*GetAvailableOptionsString(*CustomizableObject, ParameterIndexInObject)
				);
				UE_LOG(LogMutable, Error, TEXT("%s"), *Message);
			}
			
			if (RangeIndex == -1)
			{
				check(!IsParamMultidimensional(ParameterIndexInObject)); // This param is multidimensional, it must have a RangeIndex of 0 or more
				IntParameters[IntParamIndex].ParameterValueName = SelectedOption;
			}
			else
			{
				check(IsParamMultidimensional(ParameterIndexInObject)); // This param is not multidimensional, it must have a RangeIndex of -1

				if (!IntParameters[IntParamIndex].ParameterRangeValueNames.IsValidIndex(RangeIndex))
				{
					const int32 InsertionIndex = IntParameters[IntParamIndex].ParameterRangeValueNames.Num();
					const int32 NumInsertedElements = RangeIndex + 1 - IntParameters[IntParamIndex].ParameterRangeValueNames.Num();
					IntParameters[IntParamIndex].ParameterRangeValueNames.InsertDefaulted(InsertionIndex, NumInsertedElements);
				}

				check(IntParameters[IntParamIndex].ParameterRangeValueNames.IsValidIndex(RangeIndex));
				IntParameters[IntParamIndex].ParameterRangeValueNames[RangeIndex] = SelectedOption;
			}
		}
	}
}


void FCustomizableObjectInstanceDescriptor::SetIntParameterSelectedOption(const FString& ParamName, const FString& SelectedOptionName, const int32 RangeIndex)
{
	const int32 IntParamIndex = FindIntParameterNameIndex(ParamName);
	SetIntParameterSelectedOption(IntParamIndex, SelectedOptionName, RangeIndex);
}


float FCustomizableObjectInstanceDescriptor::GetFloatParameterSelectedOption(const FString& FloatParamName, const int32 RangeIndex) const
{
	check(CustomizableObject);

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(FloatParamName);

	const int32 FloatParamIndex = FindFloatParameterNameIndex(FloatParamName);

	if (ParameterIndexInObject >= 0 && FloatParamIndex >= 0)
	{
		if (RangeIndex == -1)
		{
			check(!IsParamMultidimensional(ParameterIndexInObject)); // This param is multidimensional, it must have a RangeIndex of 0 or more
			return FloatParameters[FloatParamIndex].ParameterValue;
		}
		else
		{
			check(IsParamMultidimensional(ParameterIndexInObject)); // This param is not multidimensional, it must have a RangeIndex of -1

			if (FloatParameters[FloatParamIndex].ParameterRangeValues.IsValidIndex(RangeIndex))
			{
				return FloatParameters[FloatParamIndex].ParameterRangeValues[RangeIndex];
			}
		}
	}

	return -1.0f; 
}


void FCustomizableObjectInstanceDescriptor::SetFloatParameterSelectedOption(const FString& FloatParamName, const float FloatValue, const int32 RangeIndex)
{
	check(CustomizableObject);

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(FloatParamName);

	const int32 FloatParamIndex = FindFloatParameterNameIndex(FloatParamName);

	if (ParameterIndexInObject >= 0 && FloatParamIndex >= 0)
	{
		if (RangeIndex == -1)
		{
			check(!IsParamMultidimensional(ParameterIndexInObject)); // This param is multidimensional, it must have a RangeIndex of 0 or more
			FloatParameters[FloatParamIndex].ParameterValue = FloatValue;
		}
		else
		{
			check(IsParamMultidimensional(ParameterIndexInObject)); // This param is not multidimensional, it must have a RangeIndex of -1

			if (!FloatParameters[FloatParamIndex].ParameterRangeValues.IsValidIndex(RangeIndex))
			{
				const int32 InsertionIndex = FloatParameters[FloatParamIndex].ParameterRangeValues.Num();
				const int32 NumInsertedElements = RangeIndex + 1 - FloatParameters[FloatParamIndex].ParameterRangeValues.Num();
				FloatParameters[FloatParamIndex].ParameterRangeValues.InsertDefaulted(InsertionIndex, NumInsertedElements);
			}

			check(FloatParameters[FloatParamIndex].ParameterRangeValues.IsValidIndex(RangeIndex));
			FloatParameters[FloatParamIndex].ParameterRangeValues[RangeIndex] = FloatValue;
		}
	}
}


FLinearColor FCustomizableObjectInstanceDescriptor::GetColorParameterSelectedOption(const FString& ColorParamName) const
{
	check(CustomizableObject);

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(ColorParamName);

	const int32 ColorParamIndex = FindVectorParameterNameIndex(ColorParamName);

	if (ParameterIndexInObject >= 0 && ColorParamIndex >= 0)
	{
		return VectorParameters[ColorParamIndex].ParameterValue;
	}

	return FLinearColor::Black;
}


void FCustomizableObjectInstanceDescriptor::SetColorParameterSelectedOption(const FString& ColorParamName, const FLinearColor& ColorValue)
{
	SetVectorParameterSelectedOption(ColorParamName, ColorValue);
}


bool FCustomizableObjectInstanceDescriptor::GetBoolParameterSelectedOption(const FString& BoolParamName) const
{
	check(CustomizableObject);

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(BoolParamName);

	const int32 BoolParamIndex = FindBoolParameterNameIndex(BoolParamName);

	if (ParameterIndexInObject >= 0 && BoolParamIndex >= 0)
	{
		return BoolParameters[BoolParamIndex].ParameterValue;
	}

	return false;
}


void FCustomizableObjectInstanceDescriptor::SetBoolParameterSelectedOption(const FString& BoolParamName, const bool BoolValue)
{
	check(CustomizableObject);

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(BoolParamName);

	const int32 BoolParamIndex = FindBoolParameterNameIndex(BoolParamName);

	if (ParameterIndexInObject >= 0 && BoolParamIndex >= 0)
	{
		BoolParameters[BoolParamIndex].ParameterValue = BoolValue;
	}
}


void FCustomizableObjectInstanceDescriptor::SetVectorParameterSelectedOption(const FString& VectorParamName, const FLinearColor& VectorValue)
{
	check(CustomizableObject);

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(VectorParamName);

	const int32 VectorParamIndex = FindVectorParameterNameIndex(VectorParamName);

	if (ParameterIndexInObject >= 0	&& VectorParamIndex >= 0)
	{
		VectorParameters[VectorParamIndex].ParameterValue = VectorValue;
	}
}


void FCustomizableObjectInstanceDescriptor::SetProjectorValue(const FString& ProjectorParamName,
	const FVector& Pos, const FVector& Direction, const FVector& Up, const FVector& Scale,
	const float Angle,
	const int32 RangeIndex)
{
	check(CustomizableObject);

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(ProjectorParamName);

	const int32 ProjectorParamIndex = FindProjectorParameterNameIndex(ProjectorParamName);

	if (ParameterIndexInObject >= 0 && ProjectorParamIndex >= 0)
	{
		FCustomizableObjectProjector ProjectorData;
		ProjectorData.Position = static_cast<FVector3f>(Pos);
		ProjectorData.Direction = static_cast<FVector3f>(Direction);
		ProjectorData.Up = static_cast<FVector3f>(Up);
		ProjectorData.Scale = static_cast<FVector3f>(Scale);
		ProjectorData.Angle = Angle;
		ProjectorData.ProjectionType = ProjectorParameters[ProjectorParamIndex].Value.ProjectionType;

		if (RangeIndex == -1)
		{
			check(!IsParamMultidimensional(ParameterIndexInObject)); // This param is multidimensional, it must have a RangeIndex of 0 or more
			ProjectorParameters[ProjectorParamIndex].Value = ProjectorData;
		}
		else
		{
			check(IsParamMultidimensional(ParameterIndexInObject)); // This param is not multidimensional, it must have a RangeIndex of -1

			if (!ProjectorParameters[ProjectorParamIndex].RangeValues.IsValidIndex(RangeIndex))
			{
				const int32 InsertionIndex = ProjectorParameters[ProjectorParamIndex].RangeValues.Num();
				const int32 NumInsertedElements = RangeIndex + 1 - ProjectorParameters[ProjectorParamIndex].RangeValues.Num();
				ProjectorParameters[ProjectorParamIndex].RangeValues.InsertDefaulted(InsertionIndex, NumInsertedElements);
			}

			check(ProjectorParameters[ProjectorParamIndex].RangeValues.IsValidIndex(RangeIndex));
			ProjectorParameters[ProjectorParamIndex].RangeValues[RangeIndex] = ProjectorData;
		}
	}
}


void FCustomizableObjectInstanceDescriptor::SetProjectorPosition(const FString& ProjectorParamName, const FVector3f& Pos, const int32 RangeIndex)
{
	check(CustomizableObject);

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(ProjectorParamName);

	const int32 ProjectorParamIndex = FindProjectorParameterNameIndex(ProjectorParamName);

	if (ParameterIndexInObject >= 0	&& ProjectorParamIndex >= 0)
	{
		FCustomizableObjectProjector ProjectorData = ProjectorParameters[ProjectorParamIndex].Value;
		ProjectorData.Position = static_cast<FVector3f>(Pos);

		if (RangeIndex == -1)
		{
			check(!IsParamMultidimensional(ParameterIndexInObject)); // This param is multidimensional, it must have a RangeIndex of 0 or more
			ProjectorParameters[ProjectorParamIndex].Value = ProjectorData;
		}
		else
		{
			check(IsParamMultidimensional(ParameterIndexInObject)); // This param is not multidimensional, it must have a RangeIndex of -1

			if (!ProjectorParameters[ProjectorParamIndex].RangeValues.IsValidIndex(RangeIndex))
			{
				const int32 InsertionIndex = ProjectorParameters[ProjectorParamIndex].RangeValues.Num();
				const int32 NumInsertedElements = RangeIndex + 1 - ProjectorParameters[ProjectorParamIndex].RangeValues.Num();
				ProjectorParameters[ProjectorParamIndex].RangeValues.InsertDefaulted(InsertionIndex, NumInsertedElements);
			}

			check(ProjectorParameters[ProjectorParamIndex].RangeValues.IsValidIndex(RangeIndex));
			ProjectorParameters[ProjectorParamIndex].RangeValues[RangeIndex] = ProjectorData;
		}
	}
}


void FCustomizableObjectInstanceDescriptor::GetProjectorValue(const FString& ProjectorParamName,
	FVector& OutPos, FVector& OutDirection, FVector& OutUp, FVector& OutScale,
	float& OutAngle, ECustomizableObjectProjectorType& OutType,
	const int32 RangeIndex) const
{
	check(CustomizableObject);

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(ProjectorParamName);

	const int32 ProjectorParamIndex = FindProjectorParameterNameIndex(ProjectorParamName);

	if (ParameterIndexInObject >= 0 && ProjectorParamIndex >= 0)
	{
		const FCustomizableObjectProjector* Projector;

		if (RangeIndex == -1)
		{
			check(!IsParamMultidimensional(ParameterIndexInObject)); // This param is multidimensional, it must have a RangeIndex of 0 or more

			Projector = &ProjectorParameters[ProjectorParamIndex].Value;
		}
		else
		{
			check(IsParamMultidimensional(ParameterIndexInObject)); // This param is not multidimensional, it must have a RangeIndex of -1
			check(ProjectorParameters[ProjectorParamIndex].RangeValues.IsValidIndex(RangeIndex));

			Projector = &ProjectorParameters[ProjectorParamIndex].RangeValues[RangeIndex];
		}

		if (Projector)
		{
			OutPos = static_cast<FVector>(Projector->Position);
			OutDirection = static_cast<FVector>(Projector->Direction);
			OutUp = static_cast<FVector>(Projector->Up);
			OutScale = static_cast<FVector>(Projector->Scale);
			OutAngle = Projector->Angle;
			OutType = Projector->ProjectionType;
		}
	}
}


void FCustomizableObjectInstanceDescriptor::GetProjectorValueF(const FString& ProjectorParamName,
	FVector3f& OutPos, FVector3f& OutDirection, FVector3f& OutUp, FVector3f& OutScale,
	float& OutAngle, ECustomizableObjectProjectorType& OutType,
	const int32 RangeIndex) const
{
	check(CustomizableObject);

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(ProjectorParamName);

	const int32 ProjectorParamIndex = FindProjectorParameterNameIndex(ProjectorParamName);

	if (ParameterIndexInObject >= 0 && ProjectorParamIndex >= 0)
	{
		const FCustomizableObjectProjector* Projector;

		if (RangeIndex == -1)
		{
			check(!IsParamMultidimensional(ParameterIndexInObject)); // This param is multidimensional, it must have a RangeIndex of 0 or more

			Projector = &ProjectorParameters[ProjectorParamIndex].Value;
		}
		else
		{
			check(IsParamMultidimensional(ParameterIndexInObject)); // This param is not multidimensional, it must have a RangeIndex of -1
			check(ProjectorParameters[ProjectorParamIndex].RangeValues.IsValidIndex(RangeIndex));

			Projector = &ProjectorParameters[ProjectorParamIndex].RangeValues[RangeIndex];
		}

		if (Projector)
		{
			OutPos = Projector->Position;
			OutDirection = Projector->Direction;
			OutUp = Projector->Up;
			OutScale = Projector->Scale;
			OutAngle = Projector->Angle;
			OutType = Projector->ProjectionType;
		}
	}
}


FVector FCustomizableObjectInstanceDescriptor::GetProjectorPosition(const FString& ParamName, const int32 RangeIndex) const
{
	check(CustomizableObject);

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(ParamName);

	const int32 ProjectorParamIndex = FindProjectorParameterNameIndex(ParamName);

	if (ParameterIndexInObject >= 0 && ProjectorParamIndex >= 0)
	{
		if (RangeIndex == -1)
		{
			return static_cast<FVector>(ProjectorParameters[ProjectorParamIndex].Value.Position);
		}
		else if (ProjectorParameters[ProjectorParamIndex].RangeValues.IsValidIndex(RangeIndex))
		{
			return static_cast<FVector>(ProjectorParameters[ProjectorParamIndex].RangeValues[RangeIndex].Position);
		}
	}

	return FVector(-0.0,-0.0,-0.0);
}


FVector FCustomizableObjectInstanceDescriptor::GetProjectorDirection(const FString& ParamName, const int32 RangeIndex) const
{
	check(CustomizableObject);

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(ParamName);

	const int32 ProjectorParamIndex = FindProjectorParameterNameIndex(ParamName);

	if (ParameterIndexInObject >= 0 && ProjectorParamIndex >= 0)
	{
		if (RangeIndex == -1)
		{
			return static_cast<FVector>(ProjectorParameters[ProjectorParamIndex].Value.Direction);
		}
		else if (ProjectorParameters[ProjectorParamIndex].RangeValues.IsValidIndex(RangeIndex))
		{
			return static_cast<FVector>(ProjectorParameters[ProjectorParamIndex].RangeValues[RangeIndex].Direction);
		}
	}

	return FVector(-0.0, -0.0, -0.0);
}


FVector FCustomizableObjectInstanceDescriptor::GetProjectorUp(const FString& ParamName, const int32 RangeIndex) const
{
	check(CustomizableObject);

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(ParamName);

	const int32 ProjectorParamIndex = FindProjectorParameterNameIndex(ParamName);

	if (ParameterIndexInObject >= 0 && ProjectorParamIndex >= 0)
	{
		if (RangeIndex == -1)
		{
			return static_cast<FVector>(ProjectorParameters[ProjectorParamIndex].Value.Up);
		}
		else if (ProjectorParameters[ProjectorParamIndex].RangeValues.IsValidIndex(RangeIndex))
		{
			return static_cast<FVector>(ProjectorParameters[ProjectorParamIndex].RangeValues[RangeIndex].Up);
		}
	}

	return FVector(-0.0, -0.0, -0.0);	
}


FVector FCustomizableObjectInstanceDescriptor::GetProjectorScale(const FString& ParamName, const int32 RangeIndex) const
{
	check(CustomizableObject);

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(ParamName);

	const int32 ProjectorParamIndex = FindProjectorParameterNameIndex(ParamName);

	if ((ParameterIndexInObject >= 0) && (ProjectorParamIndex >= 0))
	{
		if (RangeIndex == -1)
		{
			return static_cast<FVector>(ProjectorParameters[ProjectorParamIndex].Value.Scale);
		}
		else if (ProjectorParameters[ProjectorParamIndex].RangeValues.IsValidIndex(RangeIndex))
		{
			return static_cast<FVector>(ProjectorParameters[ProjectorParamIndex].RangeValues[RangeIndex].Scale);
		}
	}

	return FVector(-0.0, -0.0, -0.0);
}


float FCustomizableObjectInstanceDescriptor::GetProjectorAngle(const FString& ParamName, const int32 RangeIndex) const
{
	check(CustomizableObject);

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(ParamName);

	const int32 ProjectorParamIndex = FindProjectorParameterNameIndex(ParamName);

	if (ParameterIndexInObject >= 0 && ProjectorParamIndex >= 0)
	{
		if (RangeIndex == -1)
		{
			return ProjectorParameters[ProjectorParamIndex].Value.Angle;
		}
		else if (ProjectorParameters[ProjectorParamIndex].RangeValues.IsValidIndex(RangeIndex))
		{
			return ProjectorParameters[ProjectorParamIndex].RangeValues[RangeIndex].Angle;
		}
	}

	return 0.0;
}


ECustomizableObjectProjectorType FCustomizableObjectInstanceDescriptor::GetProjectorParameterType(const FString& ParamName, const int32 RangeIndex) const
{
	check(CustomizableObject);

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(ParamName);

	const int32 ProjectorParamIndex = FindProjectorParameterNameIndex(ParamName);

	if (ParameterIndexInObject >= 0 && ProjectorParamIndex >= 0)
	{
		if (RangeIndex == -1)
		{
			return ProjectorParameters[ProjectorParamIndex].Value.ProjectionType;
		}
		else if (ProjectorParameters[ProjectorParamIndex].RangeValues.IsValidIndex(RangeIndex))
		{
			return ProjectorParameters[ProjectorParamIndex].RangeValues[RangeIndex].ProjectionType;
		}
	}

	return ECustomizableObjectProjectorType::Planar;
}


FCustomizableObjectProjector FCustomizableObjectInstanceDescriptor::GetProjector(const FString& ParamName, const int32 RangeIndex) const
{
	const int32 Index = FindProjectorParameterNameIndex(ParamName);

	if (Index != -1)
	{
		if (RangeIndex == -1)
		{
			return ProjectorParameters[Index].Value;
		}
		else if (ProjectorParameters[Index].RangeValues.IsValidIndex(RangeIndex))
		{
			return ProjectorParameters[Index].RangeValues[RangeIndex];
		}
	}

	return FCustomizableObjectProjector();
}


int32 FCustomizableObjectInstanceDescriptor::FindIntParameterNameIndex(const FString& ParamName) const
{
	if (const int32* Found = IntParametersLookupTable.Find(ParamName))
	{
		return *Found;
	}
	else
	{
		return INDEX_NONE;
	}
}


int32 FCustomizableObjectInstanceDescriptor::FindFloatParameterNameIndex(const FString& ParamName) const
{
	for (int32 i = 0; i < FloatParameters.Num(); ++i)
	{
		if (FloatParameters[i].ParameterName == ParamName)
		{
			return i;
		}
	}

	return -1;
}


int32 FCustomizableObjectInstanceDescriptor::FindBoolParameterNameIndex(const FString& ParamName) const
{
	for (int32 i = 0; i < BoolParameters.Num(); ++i)
	{
		if (BoolParameters[i].ParameterName == ParamName)
		{
			return i;
		}
	}

	return -1;
}



int32 FCustomizableObjectInstanceDescriptor::FindVectorParameterNameIndex(const FString& ParamName) const
{
	for (int32 i = 0; i < VectorParameters.Num(); ++i)
	{
		if (VectorParameters[i].ParameterName == ParamName)
		{
			return i;
		}
	}

	return -1;
}


int32 FCustomizableObjectInstanceDescriptor::FindProjectorParameterNameIndex(const FString& ParamName) const
{
	for (int32 i = 0; i < ProjectorParameters.Num(); ++i)
	{
		if (ProjectorParameters[i].ParameterName == ParamName)
		{
			return i;
		}
	}

	return -1;
}


bool FCustomizableObjectInstanceDescriptor::IsParamMultidimensional(const FString& ParamName) const
{
	check(CustomizableObject);

	const int32 ParameterIndex = CustomizableObject->FindParameter(ParamName);
	return IsParamMultidimensional(ParameterIndex);
}


bool FCustomizableObjectInstanceDescriptor::IsParamMultidimensional(const int32 ParamIndex) const
{
	check(CustomizableObject);

	const mu::ParametersPtr MutableParameters = CustomizableObject->GetPrivate()->GetModel()->NewParameters();
	check(ParamIndex < MutableParameters->GetCount());
	const mu::RangeIndexPtr RangeIdxPtr = MutableParameters->NewRangeIndex(ParamIndex);

	return RangeIdxPtr.get() != nullptr;
}


int32 FCustomizableObjectInstanceDescriptor::CurrentParamRange(const FString& ParamName) const
{
	check(CustomizableObject);

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(ParamName);

	const int32 ProjectorParamIndex = FindProjectorParameterNameIndex(ParamName);

	if (ParameterIndexInObject >= 0 && ProjectorParamIndex >= 0)
	{
		return ProjectorParameters[ProjectorParamIndex].RangeValues.Num();
	}
	else
	{
		return 0;
	}
}


int32 FCustomizableObjectInstanceDescriptor::AddValueToIntRange(const FString& ParamName)
{
	check(CustomizableObject);

	const int32 intParameterIndex = FindIntParameterNameIndex(ParamName);
	if (intParameterIndex != -1)
	{
		FCustomizableObjectIntParameterValue& intParameter = IntParameters[intParameterIndex];
		const int32 ParamIndexInObject = CustomizableObject->FindParameter(intParameter.ParameterName);
		// TODO: Define the default option in the editor instead of taking the first available, like it's currently defined for GetProjectorDefaultValue()
		const FString DefaultValue = CustomizableObject->GetIntParameterAvailableOption(ParamIndexInObject, 0);
		intParameter.ParameterRangeValueNames.Add(DefaultValue);
		return intParameter.ParameterRangeValueNames.Num() - 1;
	}
	return -1;
}


int32 FCustomizableObjectInstanceDescriptor::AddValueToFloatRange(const FString& ParamName)
{
	const int32 floatParameterIndex = FindFloatParameterNameIndex(ParamName);
	if (floatParameterIndex != -1)
	{
		FCustomizableObjectFloatParameterValue& floatParameter = FloatParameters[floatParameterIndex];
		// TODO: Define the default float in the editor instead of [0.5f], like it's currently defined for GetProjectorDefaultValue()
		floatParameter.ParameterRangeValues.Add(0.5f);
		return floatParameter.ParameterRangeValues.Num() - 1;
	}
	return -1;
}


int32 FCustomizableObjectInstanceDescriptor::AddValueToProjectorRange(const FString& ParamName)
{
	check(CustomizableObject);

	const int32 projectorParameterIndex = FindProjectorParameterNameIndex(ParamName);
	if (projectorParameterIndex != -1)
	{
		FCustomizableObjectProjectorParameterValue& ProjectorParameter = ProjectorParameters[projectorParameterIndex];
		const FCustomizableObjectProjector Projector = GetProjectorDefaultValue(CustomizableObject->FindParameter(ParamName));
		ProjectorParameter.RangeValues.Add(Projector);
		return ProjectorParameter.RangeValues.Num() - 1;
	}

	return -1;
}


int32 FCustomizableObjectInstanceDescriptor::RemoveValueFromIntRange(const FString& ParamName)
{
	const int32 IntParameterIndex = FindIntParameterNameIndex(ParamName);
	if (IntParameterIndex != -1)
	{
		FCustomizableObjectIntParameterValue& IntParameter = IntParameters[IntParameterIndex];
		if (IntParameter.ParameterRangeValueNames.Num() > 0)
		{
			IntParameter.ParameterRangeValueNames.Pop();
			return IntParameter.ParameterRangeValueNames.Num() - 1;
		}
	}
	return -1;
}


int32 FCustomizableObjectInstanceDescriptor::RemoveValueFromIntRange(const FString& ParamName, const int32 RangeIndex)
{
	const int32 IntParameterIndex = FindIntParameterNameIndex(ParamName);
	if (IntParameterIndex != -1)
	{
		FCustomizableObjectIntParameterValue& IntParameter = IntParameters[IntParameterIndex];
		if (IntParameter.ParameterRangeValueNames.Num() > 0)
		{
			IntParameter.ParameterRangeValueNames.RemoveAt(RangeIndex);
			return IntParameter.ParameterRangeValueNames.Num() - 1;
		}
	}
	return -1;
}


int32 FCustomizableObjectInstanceDescriptor::RemoveValueFromFloatRange(const FString& ParamName)
{
	const int32 FloatParameterIndex = FindFloatParameterNameIndex(ParamName);
	if (FloatParameterIndex != -1)
	{
		FCustomizableObjectFloatParameterValue& FloatParameter = FloatParameters[FloatParameterIndex];
		if (FloatParameter.ParameterRangeValues.Num() > 0)
		{
			FloatParameter.ParameterRangeValues.Pop();
			return FloatParameter.ParameterRangeValues.Num() - 1;
		}
	}
	return -1;
}


int32 FCustomizableObjectInstanceDescriptor::RemoveValueFromFloatRange(const FString& ParamName, const int32 RangeIndex)
{
	const int32 FloatParameterIndex = FindFloatParameterNameIndex(ParamName);
	if (FloatParameterIndex != -1)
	{
		FCustomizableObjectFloatParameterValue& FloatParameter = FloatParameters[FloatParameterIndex];
		if (FloatParameter.ParameterRangeValues.Num() > 0)
		{
			FloatParameter.ParameterRangeValues.RemoveAt(RangeIndex);
			return FloatParameter.ParameterRangeValues.Num() - 1;
		}
	}
	return -1;
}


int32 FCustomizableObjectInstanceDescriptor::RemoveValueFromProjectorRange(const FString& ParamName)
{
	const int32 ProjectorParameterIndex = FindProjectorParameterNameIndex(ParamName);
	if (ProjectorParameterIndex != -1)
	{
		FCustomizableObjectProjectorParameterValue& ProjectorParameter = ProjectorParameters[ProjectorParameterIndex];
		if (ProjectorParameter.RangeValues.Num() > 0)
		{
			ProjectorParameter.RangeValues.Pop();
			return ProjectorParameter.RangeValues.Num() - 1;
		}
	}
	return -1;
}


int32 FCustomizableObjectInstanceDescriptor::RemoveValueFromProjectorRange(const FString& ParamName, const int32 RangeIndex)
{
	const int32 ProjectorParameterIndex = FindProjectorParameterNameIndex(ParamName);
	if (ProjectorParameterIndex != -1)
	{
		FCustomizableObjectProjectorParameterValue& ProjectorParameter = ProjectorParameters[ProjectorParameterIndex];
		if (ProjectorParameter.RangeValues.Num() > 0)
		{
			ProjectorParameter.RangeValues.RemoveAt(RangeIndex);
			return ProjectorParameter.RangeValues.Num() - 1;
		}
	}
	return -1;
}


FCustomizableObjectProjector FCustomizableObjectInstanceDescriptor::GetProjectorDefaultValue(int32 const ParamIndex) const
{
	check(CustomizableObject);

	const mu::ParametersPtr MutableParameters = CustomizableObject->GetPrivate()->GetModel()->NewParameters();
	check(ParamIndex < MutableParameters->GetCount());

	FCustomizableObjectProjector Projector;

	mu::PROJECTOR_TYPE type;
	
	MutableParameters->GetProjectorValue(ParamIndex,
		&type,
		&Projector.Position[0], &Projector.Position[1], &Projector.Position[2],
		&Projector.Direction[0], &Projector.Direction[1], &Projector.Direction[2],
		&Projector.Up[0], &Projector.Up[1], &Projector.Up[2],
		&Projector.Scale[0], &Projector.Scale[1], &Projector.Scale[2],
		&Projector.Angle,
		nullptr);

	switch (type)
	{
	case mu::PROJECTOR_TYPE::PLANAR:
		Projector.ProjectionType = ECustomizableObjectProjectorType::Planar;
		break;

	case mu::PROJECTOR_TYPE::CYLINDRICAL:
		// Unapply strange swizzle for scales.
		// TODO: try to avoid this
		Projector.ProjectionType = ECustomizableObjectProjectorType::Cylindrical;
		Projector.Direction = -Projector.Direction;
		Projector.Up = -Projector.Up;
		Projector.Scale[2] = -Projector.Scale[0];
		Projector.Scale[0] = Projector.Scale[1] = Projector.Scale[1] * 2.0f;
		break;

	case mu::PROJECTOR_TYPE::WRAPPING:
		Projector.ProjectionType = ECustomizableObjectProjectorType::Wrapping;
		break;
	default:
		unimplemented()
	}

	return Projector;
}


int32 FCustomizableObjectInstanceDescriptor::GetState() const
{
	return State;
}


FString FCustomizableObjectInstanceDescriptor::GetCurrentState() const
{
	check(CustomizableObject);

	return CustomizableObject->GetStateName(GetState());
}


void FCustomizableObjectInstanceDescriptor::SetState(const int32 InState)
{
	State = InState;
}


void FCustomizableObjectInstanceDescriptor::SetCurrentState(const FString& StateName)
{
	check(CustomizableObject);

	SetState(CustomizableObject->FindState(StateName));
}


void FCustomizableObjectInstanceDescriptor::SetRandomValues()
{
	check(CustomizableObject);

	for (int32 i = 0; i < FloatParameters.Num(); ++i)
	{
		FloatParameters[i].ParameterValue = FMath::SRand();
	}

	for (int32 i = 0; i < BoolParameters.Num(); ++i)
	{
		BoolParameters[i].ParameterValue = FMath::Rand() % 2 == 0;
	}

	for (int32 i = 0; i < IntParameters.Num(); ++i)
	{
		const int32 ParameterIndexInCO = CustomizableObject->FindParameter(IntParameters[i].ParameterName);

		// TODO: Randomize multidimensional parameters
		if (ParameterIndexInCO >= 0 && !IsParamMultidimensional(ParameterIndexInCO))
		{
			const int32 NumValues = CustomizableObject->GetIntParameterNumOptions(ParameterIndexInCO);
			if (NumValues > 0)
			{
				const int32 Index = FMath::Rand() % NumValues;
				FString Option = CustomizableObject->GetIntParameterAvailableOption(ParameterIndexInCO, Index);
				SetIntParameterSelectedOption(i, Option);
			}
		}
	}
}


bool FCustomizableObjectInstanceDescriptor::CreateMultiLayerProjector(const FName& ProjectorParamName)
{
	if (const FMultilayerProjector* Result = MultilayerProjectors.Find(ProjectorParamName))
	{
		checkCode(Result->CheckDescriptorParameters(*this));
	}
	else
	{
		if (!FMultilayerProjector::AreDescriptorParametersValid(*this, ProjectorParamName))
		{
			return false;
		}
		
		const FMultilayerProjector MultilayerProjector(ProjectorParamName);
		MultilayerProjectors.Add(ProjectorParamName, MultilayerProjector);
	}

	return true;
}


void FCustomizableObjectInstanceDescriptor::RemoveMultilayerProjector(const FName& ProjectorParamName)
{
	MultilayerProjectors.Remove(ProjectorParamName);
}


int32 FCustomizableObjectInstanceDescriptor::MultilayerProjectorNumLayers(const FName& ProjectorParamName) const
{
	check(MultilayerProjectors.Contains(ProjectorParamName)); // Multilayer Projector not created.
	
	return MultilayerProjectors[ProjectorParamName].NumLayers(*this);
}


void FCustomizableObjectInstanceDescriptor::MultilayerProjectorCreateLayer(const FName& ProjectorParamName, int32 Index)
{
	check(MultilayerProjectors.Contains(ProjectorParamName)); // Multilayer Projector not created.
	
	MultilayerProjectors[ProjectorParamName].CreateLayer(*this, Index);
}


void FCustomizableObjectInstanceDescriptor::MultilayerProjectorRemoveLayerAt(const FName& ProjectorParamName, int32 Index)
{
	check(MultilayerProjectors.Contains(ProjectorParamName)); // Multilayer Projector not created.
	
	MultilayerProjectors[ProjectorParamName].RemoveLayerAt(*this, Index);
}


FMultilayerProjectorLayer FCustomizableObjectInstanceDescriptor::MultilayerProjectorGetLayer(const FName& ProjectorParamName, int32 Index) const
{
	check(MultilayerProjectors.Contains(ProjectorParamName)); // Multilayer Projector not created.
	
	return MultilayerProjectors[ProjectorParamName].GetLayer(*this, Index);
}


void FCustomizableObjectInstanceDescriptor::MultilayerProjectorUpdateLayer(const FName& ProjectorParamName, int32 Index, const FMultilayerProjectorLayer& Layer)
{
	check(MultilayerProjectors.Contains(ProjectorParamName)); // Multilayer Projector not created.
	
	MultilayerProjectors[ProjectorParamName].UpdateLayer(*this, Index, Layer);
}


TArray<FName> FCustomizableObjectInstanceDescriptor::MultilayerProjectorGetVirtualLayers(const FName& ProjectorParamName) const
{
	check(MultilayerProjectors.Contains(ProjectorParamName)); // Multilayer Projector not created.
	
	return MultilayerProjectors[ProjectorParamName].GetVirtualLayers();
}


void FCustomizableObjectInstanceDescriptor::MultilayerProjectorCreateVirtualLayer(const FName& ProjectorParamName, const FName& Id)
{
	check(MultilayerProjectors.Contains(ProjectorParamName)); // Multilayer Projector not created.
	
	MultilayerProjectors[ProjectorParamName].CreateVirtualLayer(*this, Id);
}


FMultilayerProjectorVirtualLayer FCustomizableObjectInstanceDescriptor::MultilayerProjectorFindOrCreateVirtualLayer(const FName& ProjectorParamName, const FName& Id)
{
	check(MultilayerProjectors.Contains(ProjectorParamName)); // Multilayer Projector not created.
	
	return MultilayerProjectors[ProjectorParamName].FindOrCreateVirtualLayer(*this, Id);
}


void FCustomizableObjectInstanceDescriptor::MultilayerProjectorRemoveVirtualLayer(const FName& ProjectorParamName, const FName& Id)
{
	check(MultilayerProjectors.Contains(ProjectorParamName)); // Multilayer Projector not created.
	
	MultilayerProjectors[ProjectorParamName].RemoveVirtualLayer(*this, Id);
}


FMultilayerProjectorVirtualLayer FCustomizableObjectInstanceDescriptor::MultilayerProjectorGetVirtualLayer(const FName& ProjectorParamName, const FName& Id) const
{
	check(MultilayerProjectors.Contains(ProjectorParamName)); // Multilayer Projector not created.
	
	return MultilayerProjectors[ProjectorParamName].GetVirtualLayer(*this, Id);
}


void FCustomizableObjectInstanceDescriptor::MultilayerProjectorUpdateVirtualLayer(const FName& ProjectorParamName, const FName& Id, const FMultilayerProjectorVirtualLayer& Layer)
{
	check(MultilayerProjectors.Contains(ProjectorParamName)); // Multilayer Projector not created.
	
	MultilayerProjectors[ProjectorParamName].UpdateVirtualLayer(*this, Id, Layer);
}


void FCustomizableObjectInstanceDescriptor::Init()
{
	ReloadParametersFromObject();
}


void FCustomizableObjectInstanceDescriptor::CreateParametersLookupTable()
{
	check(CustomizableObject);

	const int32 NumParameters = CustomizableObject->GetParameterCount();

	IntParametersLookupTable.Reset();

	int32 IntParameter = 0;
	for (int32 ParamIndex = 0; ParamIndex < NumParameters; ++ParamIndex)
	{
		if (CustomizableObject->GetParameterType(ParamIndex) == EMutableParameterType::Int)
		{
			IntParametersLookupTable.Add(CustomizableObject->GetParameterName(ParamIndex), IntParameter++);
		}
	}
}


uint32 GetTypeHash(const FCustomizableObjectInstanceDescriptor& Key)
{
	uint32 Hash = 0;

	if (Key.CustomizableObject) 
	{
		Hash = HashCombine(Hash, GetTypeHash(Key.CustomizableObject->GetPathName()));
		Hash = HashCombine(Hash, GetTypeHash(Key.CustomizableObject->GetCompilationGuid()));
	}

	for (const FCustomizableObjectBoolParameterValue& Value : Key.BoolParameters)
	{
		Hash = HashCombine(Hash, GetTypeHash(Value));
	}	

	for (const FCustomizableObjectIntParameterValue& Value : Key.IntParameters)
	{
		Hash = HashCombine(Hash, GetTypeHash(Value));
	}
	
	for (const FCustomizableObjectFloatParameterValue& Value : Key.FloatParameters)
	{
		Hash = HashCombine(Hash, GetTypeHash(Value));
	}

	for (const FCustomizableObjectTextureParameterValue& Value : Key.TextureParameters)
	{
		Hash = HashCombine(Hash, GetTypeHash(Value));
	}

	for (const FCustomizableObjectVectorParameterValue& Value : Key.VectorParameters)
	{
		Hash = HashCombine(Hash, GetTypeHash(Value));
	}

	for (const FCustomizableObjectProjectorParameterValue& Value : Key.ProjectorParameters)
	{
		Hash = HashCombine(Hash, GetTypeHash(Value));
	}
	
	Hash = HashCombine(Hash, GetTypeHash(Key.State));
	Hash = HashCombine(Hash, GetTypeHash(Key.bBuildParameterDecorations));
	Hash = HashCombine(Hash, GetTypeHash(Key.MinLODToLoad));
	Hash = HashCombine(Hash, GetTypeHash(Key.MaxLODToLoad));

	for (const TTuple<FName, FMultilayerProjector>& Pair : Key.MultilayerProjectors)
	{
		// Hash = HashCombine(Hash, GetTypeHash(Pair.Key)); // Already hashed by the FMultilayerProjector.
		Hash = HashCombine(Hash, GetTypeHash(Pair.Value));
	}
	
	return Hash;
}
