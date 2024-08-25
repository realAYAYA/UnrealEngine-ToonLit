// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/CustomizableObjectInstanceDescriptor.h"

#include "MuCO/CustomizableObjectInstancePrivate.h"
#include "MuCO/CustomizableObjectSystemPrivate.h"
#include "UnrealMutableImageProvider.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/DefaultImageProvider.h"
#include "MuCO/MutableProjectorTypeUtils.h"
#include "MuR/Model.h"
#include "MuR/MutableMemory.h"
#include "MuR/Parameters.h"
#include "MuR/Ptr.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CustomizableObjectInstanceDescriptor)


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
	SetCustomizableObject(Object);
}


void FCustomizableObjectInstanceDescriptor::SaveDescriptor(FArchive& Ar, bool bUseCompactDescriptor)
{
	check(CustomizableObject);

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
				bIsParamMultidimensional = CustomizableObject->IsParameterMultidimensional(ParameterIndexInObject);

				if (bIsParamMultidimensional)
				{
					for (int32 i = 0; i < P.ParameterRangeValueNames.Num(); ++i)
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
			FName Value;
			TArray<FName> RangeValues;

			for (const FCustomizableObjectTextureParameterValue& P : TextureParameters)
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

	bool bUseCompactDescriptor;
	Ar << bUseCompactDescriptor;

	// Not sure if this is needed, but it is small.
	Ar << State;
	
	int32 ModelParameterCount = CustomizableObject->GetParameterCount();

	if (!bUseCompactDescriptor)
	{
		Ar << ModelParameterCount;
	}

	for (int32 ParameterIndex = 0; ParameterIndex < ModelParameterCount; ++ParameterIndex)
	{
		FString Name;
		EMutableParameterType Type;
		int32 ModelParameterIndex = -1;

		if (bUseCompactDescriptor)
		{
			ModelParameterIndex = ParameterIndex;
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
			const bool bIsParamMultidimensional = CustomizableObject->IsParameterMultidimensional(ParameterIndexInObject);

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

						for (int32 ParamIndex = 0; ParamIndex < Values.Num(); ++ParamIndex)
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
			FName Value;
			TArray<FName> RangeValues;
			Ar << Value;
			Ar << RangeValues;

			for (FCustomizableObjectTextureParameterValue& P : TextureParameters)
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


void FCustomizableObjectInstanceDescriptor::SetCustomizableObject(UCustomizableObject& InCustomizableObject)
{
	CustomizableObject = &InCustomizableObject;
	ReloadParameters();
}


bool FCustomizableObjectInstanceDescriptor::GetBuildParameterRelevancy() const
{
#if WITH_EDITOR
	return true;
#else
	return bBuildParameterRelevancy;
#endif
}


void FCustomizableObjectInstanceDescriptor::SetBuildParameterRelevancy(const bool Value)
{
	bBuildParameterRelevancy = Value;
}


mu::ParametersPtr FCustomizableObjectInstanceDescriptor::GetParameters() const
{
	if (!CustomizableObject)
	{
		return nullptr;
	}

	if (!CustomizableObject->IsCompiled())
	{	
		return nullptr;
	}
	
	if (!CustomizableObject->GetPrivate()->GetModel())
	{
		return nullptr;
	}

	mu::ParametersPtr MutableParameters = mu::Model::NewParameters( CustomizableObject->GetPrivate()->GetModel() );

	const int32 ParamCount = MutableParameters->GetCount();
	for (int32 ParamIndex = 0; ParamIndex < ParamCount; ++ParamIndex)
	{
		const FString& Name = MutableParameters->GetName(ParamIndex);
		const FGuid& Uid = MutableParameters->GetUid(ParamIndex);
		const mu::PARAMETER_TYPE MutableType = MutableParameters->GetType(ParamIndex);

		switch (MutableType)
		{
		case mu::PARAMETER_TYPE::T_BOOL:
		{
			for (const FCustomizableObjectBoolParameterValue& BoolParameter : BoolParameters)
			{
				if (BoolParameter.ParameterName == Name || (Uid.IsValid() && BoolParameter.Id == Uid))
				{
					MutableParameters->SetBoolValue(ParamIndex,  BoolParameter.ParameterValue);
					break;
				}
			}

			break;
		}

		case mu::PARAMETER_TYPE::T_INT:
		{
			for (const FCustomizableObjectIntParameterValue& IntParameter : IntParameters)
			{
				if (IntParameter.ParameterName.Equals(Name, ESearchCase::CaseSensitive) || (Uid.IsValid() && IntParameter.Id == Uid))
				{
					if (mu::RangeIndexPtr RangeIdxPtr = MutableParameters->NewRangeIndex(ParamIndex))
					{
						for (int32 RangeIndex = 0; RangeIndex < IntParameter.ParameterRangeValueNames.Num(); ++RangeIndex)
						{
							RangeIdxPtr->SetPosition(0, RangeIndex);

							const FString& RangeValueName = IntParameter.ParameterRangeValueNames[RangeIndex];
							const int32 Value = CustomizableObject->FindIntParameterValue(ParamIndex, RangeValueName);
							MutableParameters->SetIntValue(ParamIndex, Value, RangeIdxPtr);
						}
					}
					else
					{
						const int32 Value = CustomizableObject->FindIntParameterValue(ParamIndex, IntParameter.ParameterValueName);
						MutableParameters->SetIntValue(ParamIndex, Value);
					}

					break;
				}
			}

			break;
		}

		case mu::PARAMETER_TYPE::T_FLOAT:
		{
			for (const FCustomizableObjectFloatParameterValue& FloatParameter : FloatParameters)
			{
				if (FloatParameter.ParameterName == Name || (Uid.IsValid() && FloatParameter.Id == Uid))
				{
					if (mu::RangeIndexPtr RangeIdxPtr = MutableParameters->NewRangeIndex(ParamIndex))
					{
						for (int32 RangeIndex = 0; RangeIndex < FloatParameter.ParameterRangeValues.Num(); ++RangeIndex)
						{
							RangeIdxPtr->SetPosition(0, RangeIndex);
							MutableParameters->SetFloatValue(ParamIndex, FloatParameter.ParameterRangeValues[RangeIndex], RangeIdxPtr);
						}
					}
					else
					{
						MutableParameters->SetFloatValue(ParamIndex, FloatParameter.ParameterValue);
					}

					break;
				}
			}

			break;
		}

		case mu::PARAMETER_TYPE::T_COLOUR:
		{
			for (const FCustomizableObjectVectorParameterValue& VectorParameter : VectorParameters)
			{
				if (VectorParameter.ParameterName == Name || (Uid.IsValid() && VectorParameter.Id == Uid))
				{
					MutableParameters->SetColourValue(ParamIndex, VectorParameter.ParameterValue.R, VectorParameter.ParameterValue.G, 
																  VectorParameter.ParameterValue.B, VectorParameter.ParameterValue.A);

					break;
				}
			}

			break;
		}

		case mu::PARAMETER_TYPE::T_PROJECTOR:
		{
			for (const auto& ProjectorParameter : ProjectorParameters)
			{
				if (ProjectorParameter.ParameterName == Name || (Uid.IsValid() && ProjectorParameter.Id == Uid))
				{
					auto CopyProjector = [&MutableParameters, ParamIndex](const FCustomizableObjectProjector& Value, const mu::RangeIndexPtr RangeIdxPtr = nullptr)
					{
						switch (Value.ProjectionType)
						{
						case ECustomizableObjectProjectorType::Planar:
						case ECustomizableObjectProjectorType::Wrapping:
							MutableParameters->SetProjectorValue(ParamIndex,
								Value.Position,
								Value.Direction,
								Value.Up,
								Value.Scale,
								Value.Angle,
								RangeIdxPtr);
							break;

						case ECustomizableObjectProjectorType::Cylindrical:
						{
							// Apply strange swizzle for scales
							// TODO: try to avoid this
							const float Radius = FMath::Abs(Value.Scale[0] / 2.0f);
							const float Height = Value.Scale[2];
							// TODO: try to avoid this
							MutableParameters->SetProjectorValue(ParamIndex,
									Value.Position,
									-Value.Direction,
									-Value.Up,
									FVector3f(-Height, Radius, Radius),
									Value.Angle,
									RangeIdxPtr);
							break;
						}

						default:
							check(false); // Not implemented.
						}
					};

					CopyProjector(ProjectorParameter.Value);

					if (const mu::RangeIndexPtr RangeIdxPtr = MutableParameters->NewRangeIndex(ParamIndex))
					{
						for (int32 RangeIndex = 0; RangeIndex < ProjectorParameter.RangeValues.Num(); ++RangeIndex)
						{
							RangeIdxPtr->SetPosition(0, RangeIndex);
							CopyProjector(ProjectorParameter.RangeValues[RangeIndex], RangeIdxPtr);
						}
					}
				}
			}

			break;
		}

		case mu::PARAMETER_TYPE::T_IMAGE:
		{
			for (const FCustomizableObjectTextureParameterValue& TextureParameter : TextureParameters)
			{
				if (TextureParameter.ParameterName == Name || (Uid.IsValid() && TextureParameter.Id == Uid))
				{
					if (mu::RangeIndexPtr RangeIdxPtr = MutableParameters->NewRangeIndex(ParamIndex))
					{
						for (int32 RangeIndex = 0; RangeIndex < TextureParameter.ParameterRangeValues.Num(); ++RangeIndex)
						{
							RangeIdxPtr->SetPosition(0, RangeIndex);
							MutableParameters->SetImageValue(ParamIndex, TextureParameter.ParameterRangeValues[RangeIndex], RangeIdxPtr);
						}
					}
					else
					{
						MutableParameters->SetImageValue(ParamIndex, TextureParameter.ParameterValue);
					}

					break;
				}
			}

			break;
		}

		default:
			check(false); // Missing case.
			break;
		}
	}
	
	return MutableParameters;
}


FString FCustomizableObjectInstanceDescriptor::ToString() const
{
	const UScriptStruct* Struct = StaticStruct();
	FString ExportedText;

	Struct->ExportText(ExportedText, this, nullptr, nullptr, (PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited | PPF_IncludeTransient), nullptr);

	return ExportedText;
}


void FCustomizableObjectInstanceDescriptor::ReloadParameters()
{
	if (IsRunningCookCommandlet())
	{
		return;
	}

	if (!CustomizableObject || !CustomizableObject->IsCompiled())
	{
		return;
	}

	SetState(FMath::Clamp(GetState(), 0, CustomizableObject->GetStateCount() - 1));
	
	RequestedLODLevels.Init(MAX_uint16, CustomizableObject->GetComponentCount());

	TArray<FCustomizableObjectBoolParameterValue> OldBoolParameters = BoolParameters;
	TArray<FCustomizableObjectIntParameterValue> OldIntParameters = IntParameters;
	TArray<FCustomizableObjectFloatParameterValue> OldFloatParameters = FloatParameters;
	TArray<FCustomizableObjectTextureParameterValue> OldTextureParameters = TextureParameters;
	TArray<FCustomizableObjectVectorParameterValue> OldVectorParameters = VectorParameters;
	TArray<FCustomizableObjectProjectorParameterValue> OldProjectorParameters = ProjectorParameters;

	BoolParameters.Reset();
	IntParameters.Reset();
	FloatParameters.Reset();
	TextureParameters.Reset();
	VectorParameters.Reset();
	ProjectorParameters.Reset();
	
	if (!CustomizableObject->GetPrivate()->GetModel())
	{
		UE_LOG(LogMutable, Warning, TEXT("[ReloadParametersFromObject] No model in object [%s], generated empty parameters for [%s] "), *CustomizableObject->GetName(), *CustomizableObject->GetName());
		return;
	}

	mu::ParametersPtr MutableParameters = mu::Model::NewParameters(CustomizableObject->GetPrivate()->GetModel());

	int32 ParamCount = MutableParameters->GetCount();
	for (int32 ParamIndex = 0; ParamIndex < ParamCount; ++ParamIndex)
	{
		const FString& Name = MutableParameters->GetName(ParamIndex);
		const FGuid& Uid = MutableParameters->GetUid(ParamIndex);
		const mu::PARAMETER_TYPE MutableType = MutableParameters->GetType(ParamIndex);

		switch (MutableType)
		{
		case mu::PARAMETER_TYPE::T_BOOL:
		{
			FCustomizableObjectBoolParameterValue Param;
			Param.ParameterName = Name;
			Param.Id = Uid;

			auto FindByNameAndUid = [&](const FCustomizableObjectBoolParameterValue& P)
			{
				return P.ParameterName == Name || (Uid.IsValid() && P.Id == Uid);
			};

			if (FCustomizableObjectBoolParameterValue* Result = OldBoolParameters.FindByPredicate(FindByNameAndUid))
			{	
				Param.ParameterValue = Result->ParameterValue;
			} 
			else // Not found in Instance Parameters. Use Mutable Parameters.
			{
				Param.ParameterValue = MutableParameters->GetBoolValue(ParamIndex);
			}

			BoolParameters.Add(Param);
			break;
		}

		case mu::PARAMETER_TYPE::T_INT:
		{
			FCustomizableObjectIntParameterValue Param;
			Param.ParameterName = Name;
			Param.Id = Uid;

			auto FindByNameAndUid = [&](const FCustomizableObjectIntParameterValue& P)
			{
				return P.ParameterName == Name || (Uid.IsValid() && P.Id == Uid);
			};
			
			if (FCustomizableObjectIntParameterValue* Result = OldIntParameters.FindByPredicate(FindByNameAndUid))
			{
				const int32 NumValueIndex = MutableParameters->GetIntPossibleValueCount(ParamIndex);

				auto ValueExists = [&](const FString& ValueName)
				{
					for (int32 ValueIndex = 0; ValueIndex < NumValueIndex; ++ValueIndex)
					{
						if (ValueName == MutableParameters->GetIntPossibleValueName(ParamIndex, ValueIndex))
						{
							return true;
						}
					}

					return false;
				};
				
				if (mu::RangeIndexPtr RangeIdxPtr = MutableParameters->NewRangeIndex(ParamIndex)) // Is multidimensional
				{
					// Get num of ranges (layers) from the instance
					int32 ValueCount = Result->ParameterRangeValueNames.Num();
					Param.ParameterRangeValueNames.Reserve(ValueCount);

					for (int32 RangeIndex = 0; RangeIndex < ValueCount; ++RangeIndex)
					{
						// Checking if the selected value still exists as option in the parameter
						if (const FString& OldValue = Result->ParameterRangeValueNames[RangeIndex]; ValueExists(OldValue))
						{
							Param.ParameterRangeValueNames.Add(OldValue);
						}
						else
						{
							const int32 Value = MutableParameters->GetIntValue(ParamIndex, RangeIdxPtr);
							const FString AuxParameterValueName = CustomizableObject->FindIntParameterValueName(ParamIndex, Value);
							Param.ParameterRangeValueNames.Add(AuxParameterValueName);
						}
					}
				}
				else
				{
					if (ValueExists(Result->ParameterValueName))
					{
						Param.ParameterValueName = Result->ParameterValueName;
					}
					else
					{
						const int32 ParamValue = MutableParameters->GetIntValue(ParamIndex);
						Param.ParameterValueName = CustomizableObject->FindIntParameterValueName(ParamIndex, ParamValue);
					}

					// Multilayer ints with one option are not multidimensional parameters. However, we need to preserve the layer 
					// information in case that we add a new option to the parameter, and it is converted to multidimensional.
					for (int32 RangeIndex = 0; RangeIndex < Result->ParameterRangeValueNames.Num(); ++RangeIndex)
					{
						const int32 Value = MutableParameters->GetIntValue(ParamIndex);
						const FString AuxParameterValueName = CustomizableObject->FindIntParameterValueName(ParamIndex, Value);
						Param.ParameterRangeValueNames.Add(AuxParameterValueName);
					}
				}
			} 
			else // Not found in Instance Parameters. Use Mutable Parameters.
			{
				if (mu::RangeIndexPtr RangeIdxPtr = MutableParameters->NewRangeIndex(ParamIndex))
				{
					const int32 ValueCount = MutableParameters->GetValueCount(ParamIndex);

					for (int32 ValueIndex = 0; ValueIndex < ValueCount; ++ValueIndex)
					{	
						const mu::RangeIndexPtr RangeValueIdxPtr = MutableParameters->GetValueIndex(ParamIndex, ValueIndex);
						const int32 RangeIndex = RangeValueIdxPtr->GetPosition(0);

						if (!Param.ParameterRangeValueNames.IsValidIndex(RangeIndex))
						{
							Param.ParameterRangeValueNames.AddDefaulted(RangeIndex + 1 - Param.ParameterRangeValueNames.Num());
						}

						const int32 Value = MutableParameters->GetIntValue(ParamIndex, RangeValueIdxPtr);
						const FString AuxParameterValueName = CustomizableObject->FindIntParameterValueName(ParamIndex, Value);
						Param.ParameterRangeValueNames[RangeIndex] = AuxParameterValueName;
					}
				}
				else
				{
					const int32 ParamValue = MutableParameters->GetIntValue(ParamIndex);
					Param.ParameterValueName = CustomizableObject->FindIntParameterValueName(ParamIndex, ParamValue);
				}
			}

			IntParameters.Add(Param);

			break;
		}

		case mu::PARAMETER_TYPE::T_FLOAT:
		{
			FCustomizableObjectFloatParameterValue Param;
			Param.ParameterName = Name;
			Param.Id = Uid;

			auto FindByNameAndUid = [&](const FCustomizableObjectFloatParameterValue& P)
			{
				return P.ParameterName == Name || (Uid.IsValid() && P.Id == Uid);
			};
			
			if (FCustomizableObjectFloatParameterValue* Result = OldFloatParameters.FindByPredicate(FindByNameAndUid))
			{	
				if (mu::RangeIndexPtr RangeIdxPtr = MutableParameters->NewRangeIndex(ParamIndex))
				{
					Param.ParameterRangeValues = Result->ParameterRangeValues;
				}
				else
				{
					Param.ParameterValue = Result->ParameterValue;
				}
			} 
			else // Not found in Instance Parameters. Use Mutable Parameters.
			{
				if (mu::RangeIndexPtr RangeIdxPtr = MutableParameters->NewRangeIndex(ParamIndex))
				{
					const int32 ValueCount = MutableParameters->GetValueCount(ParamIndex);

					for (int32 ValueIndex = 0; ValueIndex < ValueCount; ++ValueIndex)
					{	
						mu::RangeIndexPtr RangeValueIdxPtr = MutableParameters->GetValueIndex(ParamIndex, ValueIndex);
						int32 RangeIndex = RangeValueIdxPtr->GetPosition(0);

						if (!Param.ParameterRangeValues.IsValidIndex(RangeIndex))
						{
							Param.ParameterRangeValues.AddDefaulted(RangeIndex + 1 - Param.ParameterRangeValues.Num());
						}

						Param.ParameterRangeValues[RangeIndex] = MutableParameters->GetFloatValue(ParamIndex, RangeValueIdxPtr);
					}
				}
				else
				{
					Param.ParameterValue = MutableParameters->GetFloatValue(ParamIndex);
				}
			}

			FloatParameters.Add(Param);
			break;
		}

		case mu::PARAMETER_TYPE::T_COLOUR:
		{
			FCustomizableObjectVectorParameterValue Param;
			Param.ParameterName = Name;
			Param.Id = Uid;

			auto FindByNameAndUid = [&](const FCustomizableObjectVectorParameterValue& P)
			{
				return P.ParameterName == Name || (Uid.IsValid() && P.Id == Uid);
			};

			if (FCustomizableObjectVectorParameterValue* Result = OldVectorParameters.FindByPredicate(FindByNameAndUid))
			{	
				Param.ParameterValue = Result->ParameterValue;
			}
			else // Not found in Instance Parameters. Use Mutable Parameters.
			{
				MutableParameters->GetColourValue(ParamIndex, &Param.ParameterValue.R, &Param.ParameterValue.G,
															  &Param.ParameterValue.B, &Param.ParameterValue.A);
			}

			VectorParameters.Add(Param);
			break;
		}

		case mu::PARAMETER_TYPE::T_PROJECTOR:
		{
			FCustomizableObjectProjectorParameterValue Param;
			Param.ParameterName = Name;
			Param.Id = Uid;

			// Projector to check if the porojector's type has changed
			FCustomizableObjectProjector DefaultProjectorValue = CustomizableObject->GetProjectorParameterDefaultValue(Name);

			auto FindByNameAndUid = [&](const FCustomizableObjectProjectorParameterValue& P)
			{
				return P.ParameterName == Name || (Uid.IsValid() && P.Id == Uid);
			};
			
			if (FCustomizableObjectProjectorParameterValue* Result = OldProjectorParameters.FindByPredicate(FindByNameAndUid))
			{	
				if (mu::RangeIndexPtr RangeIdxPtr = MutableParameters->NewRangeIndex(ParamIndex))
				{
					Param.RangeValues = Result->RangeValues;
					Param.Value.ProjectionType = DefaultProjectorValue.ProjectionType;

					for (FCustomizableObjectProjector& Projector : Param.RangeValues)
					{
						Projector.ProjectionType = DefaultProjectorValue.ProjectionType;
					}
				}
				else
				{
					Param.Value = Result->Value;
					Param.Value.ProjectionType = DefaultProjectorValue.ProjectionType;
				}
			} 
			else // Not found in Instance Parameters. Use Mutable Parameters.
			{
				auto GetProjector = [&MutableParameters, ParamIndex](FCustomizableObjectProjector &Value, const mu::RangeIndexPtr RangeIndex = nullptr)
				{
					mu::PROJECTOR_TYPE Type;
					MutableParameters->GetProjectorValue(ParamIndex,
						&Type,
						&Value.Position,
						&Value.Direction,
						&Value.Up,
						&Value.Scale,
						&Value.Angle,
						RangeIndex);
					
					Value.ProjectionType = ProjectorUtils::GetEquivalentProjectorType(Type);
					if (Value.ProjectionType == ECustomizableObjectProjectorType::Cylindrical)
					{
						// Unapply strange swizzle for scales.
						// TODO: try to avoid this
						Value.Direction = -Value.Direction;
						Value.Up = -Value.Up;
						Value.Scale[2] = -Value.Scale[0];
						Value.Scale[0] = Value.Scale[1] = Value.Scale[1] * 2.0f;
					}
				};

				GetProjector(Param.Value);

				if (mu::RangeIndexPtr RangeIdxPtr = MutableParameters->NewRangeIndex(ParamIndex))
				{
					const int32 ValueCount = MutableParameters->GetValueCount(ParamIndex);

					for (int32 ValueIndex = 0; ValueIndex < ValueCount; ++ValueIndex)
					{	
						mu::RangeIndexPtr RangeValueIdxPtr = MutableParameters->GetValueIndex(ParamIndex, ValueIndex);
						int32 RangeIndex = RangeValueIdxPtr->GetPosition(0);

						if (!Param.RangeValues.IsValidIndex(RangeIndex))
						{
							Param.RangeValues.AddDefaulted(RangeIndex + 1 - Param.RangeValues.Num());
						}

						GetProjector(Param.RangeValues[RangeIndex], RangeValueIdxPtr);
					}
				}
			}

			ProjectorParameters.Add(Param);
			break;
		}

		case mu::PARAMETER_TYPE::T_IMAGE:
		{
			FCustomizableObjectTextureParameterValue Param;
			Param.ParameterName = Name;
			Param.Id = Uid;

			auto FindByNameAndUid = [&](const FCustomizableObjectTextureParameterValue& P)
			{
				return P.ParameterName == Name || (Uid.IsValid() && P.Id == Uid);
			};
			
			if (FCustomizableObjectTextureParameterValue* Result = OldTextureParameters.FindByPredicate(FindByNameAndUid))
			{	
				if (mu::RangeIndexPtr RangeIdxPtr = MutableParameters->NewRangeIndex(ParamIndex))
				{
					Param.ParameterRangeValues = Result->ParameterRangeValues;
				}
				else
				{
					Param.ParameterValue = Result->ParameterValue;
				}
			} 
			else // Not found in Instance Parameters. Use Mutable Parameters.
			{
				if (mu::RangeIndexPtr RangeIdxPtr = MutableParameters->NewRangeIndex(ParamIndex))
				{
					const int32 ValueCount = MutableParameters->GetValueCount(ParamIndex);

					for (int32 ValueIndex = 0; ValueIndex < ValueCount; ++ValueIndex)
					{	
						mu::RangeIndexPtr RangeValueIdxPtr = MutableParameters->GetValueIndex(ParamIndex, ValueIndex);
						int32 RangeIndex = RangeValueIdxPtr->GetPosition(0);

						if (!Param.ParameterRangeValues.IsValidIndex(RangeIndex))
						{
							const int32 PreviousNum = Param.ParameterRangeValues.Num();
							Param.ParameterRangeValues.AddUninitialized(RangeIndex + 1 - Param.ParameterRangeValues.Num());

							for (int32 Index = PreviousNum; Index < Param.ParameterRangeValues.Num(); ++Index)
							{
								Param.ParameterRangeValues[Index] = FName();
							}
						}

						Param.ParameterRangeValues[RangeIndex] = MutableParameters->GetImageValue(ParamIndex, RangeValueIdxPtr);
					}
				}
				else
				{
					Param.ParameterValue = MutableParameters->GetImageValue(ParamIndex);
				}
			}

			TextureParameters.Add(Param);
			break;
		}

		default:
			check(false); // Missing case.
			break;
		}
	}
	
	CreateParametersLookupTable();
}


int32 FCustomizableObjectInstanceDescriptor::GetMinLod() const
{
	return MinLOD;
}


void FCustomizableObjectInstanceDescriptor::SetMinLod(int32 InMinLOD)
{
	MinLOD = InMinLOD;
}


void FCustomizableObjectInstanceDescriptor::SetRequestedLODLevels(const TArray<uint16>& InRequestedLODLevels)
{
	RequestedLODLevels = InRequestedLODLevels;
}


const TArray<uint16>& FCustomizableObjectInstanceDescriptor::GetRequestedLODLevels() const
{
	return RequestedLODLevels;
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

#if WITH_EDITOR 

#define RETURN_ON_UNCOMPILED_CO(CustomizableObject, ErrorMessage) \
	if (!CustomizableObject->IsCompiled()) \
	{ \
		FString AdditionalLoggingInfo = FString::Printf(TEXT("Calling function: %hs.  %s"), __FUNCTION__, ErrorMessage); \
		CustomizableObject->GetPrivate()->AddUncompiledCOWarning(AdditionalLoggingInfo);\
		return; \
	} \

#else

#define RETURN_ON_UNCOMPILED_CO(CustomizableObject, ErrorMessage) \
	if (!ensureMsgf(CustomizableObject->IsCompiled(), TEXT("Customizable Object (%s) was not compiled."), *GetNameSafe(CustomizableObject))) \
	{ \
		FString AdditionalLoggingInfo = FString::Printf(TEXT("Calling function: %hs.  %s"), __FUNCTION__, ErrorMessage); \
		CustomizableObject->GetPrivate()->AddUncompiledCOWarning(AdditionalLoggingInfo);\
		return; \
	} \
 
#endif
	


void LogParameterNotFoundWarning(const FString& ParameterName, const int32 ObjectParameterIndex, const int32 InstanceParameterIndex, const UCustomizableObject* CustomizableObject, char const* CallingFunction)
{
#if !WITH_EDITOR
	// Ensuring to help make sure we catch these critical data mismatches, but since we can handle these gracefully,
	// don't ensure in Shipping builds.  We need to always log an error afterwards since the ensure will only fire
	// once per session, and we don't want to always ensure since that might interfere with unrelated debugging.
	ensureMsgf(false,
		TEXT("Failed to find parameter (%s) on CO (%s). CO parameter index: (%d). COI parameter index: (%d)"),
		*ParameterName, *GetNameSafe(CustomizableObject), ObjectParameterIndex, InstanceParameterIndex
	);
#endif
	UE_LOG(LogMutable, Error,
		TEXT("%hs: Failed to find parameter (%s) on CO (%s). CO parameter index: (%d). COI parameter index: (%d)s"),
		CallingFunction, *ParameterName, *GetNameSafe(CustomizableObject), ObjectParameterIndex, InstanceParameterIndex
	);
}


const FString& FCustomizableObjectInstanceDescriptor::GetIntParameterSelectedOption(const FString& ParamName, const int32 RangeIndex) const
{
	check(CustomizableObject);
	
	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(ParamName);
	const int32 ParameterIndexInInstance = FindIntParameterNameIndex(ParamName);

	if (ParameterIndexInObject >= 0 && ParameterIndexInInstance >= 0)
	{
		if (RangeIndex == -1)
		{
			check(!CustomizableObject->IsParameterMultidimensional(ParameterIndexInObject)); // This param is multidimensional, it must have a RangeIndex of 0 or more
			return IntParameters[ParameterIndexInInstance].ParameterValueName;
		}
		else
		{
			check(CustomizableObject->IsParameterMultidimensional(ParameterIndexInObject)); // This param is not multidimensional, it must have a RangeIndex of -1

			if (IntParameters[ParameterIndexInInstance].ParameterRangeValueNames.IsValidIndex(RangeIndex))
			{
				return IntParameters[ParameterIndexInInstance].ParameterRangeValueNames[RangeIndex];
			}
		}
	}

	LogParameterNotFoundWarning(ParamName, ParameterIndexInObject, ParameterIndexInInstance, CustomizableObject, __FUNCTION__);

	return FCustomizableObjectBoolParameterValue::DEFAULT_PARAMETER_VALUE_NAME;
}


void FCustomizableObjectInstanceDescriptor::SetIntParameterSelectedOption(const int32 ParameterIndexInInstance, const FString& SelectedOption, const int32 RangeIndex)
{
	check(CustomizableObject);
	RETURN_ON_UNCOMPILED_CO(CustomizableObject, TEXT("Error: Cannot set Int parameter "));

	const int32 ParameterIndexInObject = IntParameters.IsValidIndex(ParameterIndexInInstance) ? CustomizableObject->FindParameter(IntParameters[ParameterIndexInInstance].ParameterName) : INDEX_NONE; //-V781

	if (ParameterIndexInObject < 0 || ParameterIndexInInstance < 0)
	{
		// Warn and early out since we could not find the parameter to set.
		LogParameterNotFoundWarning(TEXT("Unknown Int Parmeter"), ParameterIndexInObject, ParameterIndexInInstance, CustomizableObject, __FUNCTION__);
		return;
	}

	const bool bValid = SelectedOption == TEXT("None") || CustomizableObject->FindIntParameterValue(ParameterIndexInObject, SelectedOption) >= 0;
	if (!bValid)
	{
#if !UE_BUILD_SHIPPING
		const FString Message = FString::Printf(
			TEXT("Tried to set the invalid value [%s] to parameter [%d, %s]! Value index=[%d]. Correct values=[%s]."),
			*SelectedOption, ParameterIndexInObject,
			*IntParameters[ParameterIndexInInstance].ParameterName,
			CustomizableObject->FindIntParameterValue(ParameterIndexInObject, SelectedOption),
			*GetAvailableOptionsString(*CustomizableObject, ParameterIndexInObject)
		);
		UE_LOG(LogMutable, Error, TEXT("%s"), *Message);
#endif
		return;
	}

	if (RangeIndex == -1)
	{
		// If this param were multidimensional, it must have a RangeIndex of 0 or more
		check(!CustomizableObject->IsParameterMultidimensional(ParameterIndexInObject));
		IntParameters[ParameterIndexInInstance].ParameterValueName = SelectedOption;
	}
	else
	{
		// If this param were not multidimensional, it must have a RangeIndex of -1
		check(CustomizableObject->IsParameterMultidimensional(ParameterIndexInObject));

		if (!IntParameters[ParameterIndexInInstance].ParameterRangeValueNames.IsValidIndex(RangeIndex))
		{
			const int32 InsertionIndex = IntParameters[ParameterIndexInInstance].ParameterRangeValueNames.Num();
			const int32 NumInsertedElements = RangeIndex + 1 - IntParameters[ParameterIndexInInstance].ParameterRangeValueNames.Num();
			IntParameters[ParameterIndexInInstance].ParameterRangeValueNames.InsertDefaulted(InsertionIndex, NumInsertedElements);
		}

		check(IntParameters[ParameterIndexInInstance].ParameterRangeValueNames.IsValidIndex(RangeIndex));
		IntParameters[ParameterIndexInInstance].ParameterRangeValueNames[RangeIndex] = SelectedOption;
	}
}


void FCustomizableObjectInstanceDescriptor::SetIntParameterSelectedOption(const FString& ParamName, const FString& SelectedOptionName, const int32 RangeIndex)
{
	check(CustomizableObject);
	RETURN_ON_UNCOMPILED_CO(CustomizableObject, TEXT("Error: Cannot set Int parameter "));

	const int32 ParamIndexInInstance = FindIntParameterNameIndex(ParamName);
	if (ParamIndexInInstance == INDEX_NONE)
	{
		// Warn and early out since we could not find the parameter to set.
		LogParameterNotFoundWarning(ParamName, ParamIndexInInstance, ParamIndexInInstance, CustomizableObject, __FUNCTION__);
		return;
	}

	SetIntParameterSelectedOption(ParamIndexInInstance, SelectedOptionName, RangeIndex);
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
			check(!CustomizableObject->IsParameterMultidimensional(ParameterIndexInObject)); // This param is multidimensional, it must have a RangeIndex of 0 or more
			return FloatParameters[FloatParamIndex].ParameterValue;
		}
		else
		{
			check(CustomizableObject->IsParameterMultidimensional(ParameterIndexInObject)); // This param is not multidimensional, it must have a RangeIndex of -1

			if (FloatParameters[FloatParamIndex].ParameterRangeValues.IsValidIndex(RangeIndex))
			{
				return FloatParameters[FloatParamIndex].ParameterRangeValues[RangeIndex];
			}
		}
	}

	LogParameterNotFoundWarning(FloatParamName, ParameterIndexInObject, FloatParamIndex, CustomizableObject, __FUNCTION__);
	
	return 	FCustomizableObjectFloatParameterValue::DEFAULT_PARAMETER_VALUE; 
}


void FCustomizableObjectInstanceDescriptor::SetFloatParameterSelectedOption(const FString& FloatParamName, const float FloatValue, const int32 RangeIndex)
{
	check(CustomizableObject);
	RETURN_ON_UNCOMPILED_CO(CustomizableObject, TEXT("Error: Cannot set Int parameter "));

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(FloatParamName);
	const int32 ParameterIndexInInstance = FindFloatParameterNameIndex(FloatParamName);

	if (ParameterIndexInObject < 0 || ParameterIndexInInstance < 0)
	{
		// Warn and early out since we could not find the parameter to set.
		LogParameterNotFoundWarning(FloatParamName, ParameterIndexInObject, ParameterIndexInInstance, CustomizableObject, __FUNCTION__);
		return;
	}

	if (RangeIndex == INDEX_NONE)
	{
		check(!CustomizableObject->IsParameterMultidimensional(ParameterIndexInObject)); // This param is multidimensional, it must have a RangeIndex of 0 or more
		FloatParameters[ParameterIndexInInstance].ParameterValue = FloatValue;
	}
	else
	{
		check(CustomizableObject->IsParameterMultidimensional(ParameterIndexInObject)); // This param is not multidimensional, it must have a RangeIndex of -1

		if (!FloatParameters[ParameterIndexInInstance].ParameterRangeValues.IsValidIndex(RangeIndex))
		{
			const int32 InsertionIndex = FloatParameters[ParameterIndexInInstance].ParameterRangeValues.Num();
			const int32 NumInsertedElements = RangeIndex + 1 - FloatParameters[ParameterIndexInInstance].ParameterRangeValues.Num();
			FloatParameters[ParameterIndexInInstance].ParameterRangeValues.InsertDefaulted(InsertionIndex, NumInsertedElements);
		}

		check(FloatParameters[ParameterIndexInInstance].ParameterRangeValues.IsValidIndex(RangeIndex));
		FloatParameters[ParameterIndexInInstance].ParameterRangeValues[RangeIndex] = FloatValue;
	}

}


FName FCustomizableObjectInstanceDescriptor::GetTextureParameterSelectedOption(const FString& TextureParamName, const int32 RangeIndex) const
{
	check(CustomizableObject);

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(TextureParamName);
	const int32 ParameterIndexInInstance = FindTextureParameterNameIndex(TextureParamName);

	if (ParameterIndexInObject >= 0 && ParameterIndexInInstance >= 0)
	{
		if (RangeIndex == -1)
		{
			check(!CustomizableObject->IsParameterMultidimensional(ParameterIndexInObject)); // This param is multidimensional, it must have a RangeIndex of 0 or more
			return TextureParameters[ParameterIndexInInstance].ParameterValue;
		}
		else
		{
			check(CustomizableObject->IsParameterMultidimensional(ParameterIndexInObject)); // This param is not multidimensional, it must have a RangeIndex of -1

			if (TextureParameters[ParameterIndexInInstance].ParameterRangeValues.IsValidIndex(RangeIndex))
			{
				return TextureParameters[ParameterIndexInInstance].ParameterRangeValues[RangeIndex];
			}
		}
	}
	
	LogParameterNotFoundWarning(TextureParamName, ParameterIndexInObject, ParameterIndexInInstance, CustomizableObject, __FUNCTION__);

	return FName(); 
}


void FCustomizableObjectInstanceDescriptor::SetTextureParameterSelectedOption(const FString& TextureParamName, const FString& TextureValue, const int32 RangeIndex)
{
	check(CustomizableObject);
	RETURN_ON_UNCOMPILED_CO(CustomizableObject, TEXT("Error: Cannot set Int parameter "));

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(TextureParamName);
	const int32 ParameterIndexInInstance = FindTextureParameterNameIndex(TextureParamName);

	if (ParameterIndexInObject < 0 || ParameterIndexInInstance < 0)
	{
		// Early out since we could not find the parameter to set.
		LogParameterNotFoundWarning(TextureParamName, ParameterIndexInObject, ParameterIndexInInstance, CustomizableObject, __FUNCTION__);
		return;
	}

	if (RangeIndex == INDEX_NONE)
	{
		check(!CustomizableObject->IsParameterMultidimensional(ParameterIndexInObject)); // This param is multidimensional, it must have a RangeIndex of 0 or more
		TextureParameters[ParameterIndexInInstance].ParameterValue = FName(TextureValue);
	}
	else
	{
		check(CustomizableObject->IsParameterMultidimensional(ParameterIndexInObject)); // This param is not multidimensional, it must have a RangeIndex of -1

		if (!TextureParameters[ParameterIndexInInstance].ParameterRangeValues.IsValidIndex(RangeIndex))
		{
			const int32 InsertionIndex = TextureParameters[ParameterIndexInInstance].ParameterRangeValues.Num();
			const int32 NumInsertedElements = RangeIndex + 1 - TextureParameters[ParameterIndexInInstance].ParameterRangeValues.Num();
			TextureParameters[ParameterIndexInInstance].ParameterRangeValues.InsertDefaulted(InsertionIndex, NumInsertedElements);
		}

		check(TextureParameters[ParameterIndexInInstance].ParameterRangeValues.IsValidIndex(RangeIndex));
		TextureParameters[ParameterIndexInInstance].ParameterRangeValues[RangeIndex] = FName(TextureValue);
	}
}


FLinearColor FCustomizableObjectInstanceDescriptor::GetColorParameterSelectedOption(const FString& ColorParamName) const
{
	check(CustomizableObject);

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(ColorParamName);
	const int32 ColorParamIndex = FindVectorParameterNameIndex(ColorParamName);

	if (ColorParamIndex == INDEX_NONE)
	{
		LogParameterNotFoundWarning(ColorParamName, ParameterIndexInObject, ColorParamIndex, CustomizableObject, __FUNCTION__);
		return FCustomizableObjectVectorParameterValue::DEFAULT_PARAMETER_VALUE;
	}
	
	return VectorParameters[ColorParamIndex].ParameterValue;
}


void FCustomizableObjectInstanceDescriptor::SetColorParameterSelectedOption(const FString& ColorParamName, const FLinearColor& ColorValue)
{
	check(CustomizableObject);
	RETURN_ON_UNCOMPILED_CO(CustomizableObject, TEXT("Error: Cannot set Int parameter "));

	SetVectorParameterSelectedOption(ColorParamName, ColorValue);
}


bool FCustomizableObjectInstanceDescriptor::GetBoolParameterSelectedOption(const FString& BoolParamName) const
{
	check(CustomizableObject);

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(BoolParamName);
	const int32 BoolParamIndex = FindBoolParameterNameIndex(BoolParamName);

	if (BoolParamIndex == INDEX_NONE)
	{
		LogParameterNotFoundWarning(BoolParamName, ParameterIndexInObject, BoolParamIndex, CustomizableObject, __FUNCTION__);
		return FCustomizableObjectBoolParameterValue::DEFAULT_PARAMETER_VALUE;
	}

	return BoolParameters[BoolParamIndex].ParameterValue;
}


void FCustomizableObjectInstanceDescriptor::SetBoolParameterSelectedOption(const FString& BoolParamName, const bool BoolValue)
{
	check(CustomizableObject);
	RETURN_ON_UNCOMPILED_CO(CustomizableObject, TEXT("Error: Cannot set Int parameter "));

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(BoolParamName);
	const int32 ParameterIndexInInstance = FindBoolParameterNameIndex(BoolParamName);

	if (ParameterIndexInObject < 0 || ParameterIndexInInstance < 0)
	{
		// Early out since we could not find the parameter to set.
		LogParameterNotFoundWarning(BoolParamName, ParameterIndexInObject, ParameterIndexInInstance, CustomizableObject, __FUNCTION__);
		return;
	}

	BoolParameters[ParameterIndexInInstance].ParameterValue = BoolValue;
}


void FCustomizableObjectInstanceDescriptor::SetVectorParameterSelectedOption(const FString& VectorParamName, const FLinearColor& VectorValue)
{
	check(CustomizableObject);
	RETURN_ON_UNCOMPILED_CO(CustomizableObject, TEXT("Error: Cannot set Int parameter "));

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(VectorParamName);
	const int32 ParameterIndexInInstance = FindVectorParameterNameIndex(VectorParamName);

	if (ParameterIndexInObject < 0 || ParameterIndexInInstance < 0)
	{
		// Early out since we could not find the parameter to set.
		LogParameterNotFoundWarning(VectorParamName, ParameterIndexInObject, ParameterIndexInInstance, CustomizableObject, __FUNCTION__);
		return;
	}

	VectorParameters[ParameterIndexInInstance].ParameterValue = VectorValue;
}


void FCustomizableObjectInstanceDescriptor::SetProjectorValue(const FString& ProjectorParamName,
	const FVector& Pos, const FVector& Direction, const FVector& Up, const FVector& Scale,
	const float Angle,
	const int32 RangeIndex)
{
	check(CustomizableObject);
	RETURN_ON_UNCOMPILED_CO(CustomizableObject, TEXT("Error: Cannot set Projector parameter "))

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(ProjectorParamName);
	const int32 ParameterIndexInInstance = FindProjectorParameterNameIndex(ProjectorParamName);

	if (ParameterIndexInObject < 0 || ParameterIndexInInstance < 0)
	{
		// Early out since we could not find the parameter to set.
		LogParameterNotFoundWarning(ProjectorParamName, ParameterIndexInObject, ParameterIndexInInstance, CustomizableObject, __FUNCTION__);
		return;
	}

	// Parameter to modify
	FCustomizableObjectProjectorParameterValue& ProjectorParameter = ProjectorParameters[ParameterIndexInInstance];

	// New Value
	FCustomizableObjectProjector ProjectorData;
	ProjectorData.Position = static_cast<FVector3f>(Pos);
	ProjectorData.Direction = static_cast<FVector3f>(Direction);
	ProjectorData.Up = static_cast<FVector3f>(Up);
	ProjectorData.Scale = static_cast<FVector3f>(Scale);
	ProjectorData.Angle = Angle;
	ProjectorData.ProjectionType = ProjectorParameter.Value.ProjectionType;

	if (RangeIndex == -1)
	{
		check(!CustomizableObject->IsParameterMultidimensional(ParameterIndexInObject)); // This param is multidimensional, it must have a RangeIndex of 0 or more
		ProjectorParameter.Value = ProjectorData;
	}
	else
	{
		check(CustomizableObject->IsParameterMultidimensional(ParameterIndexInObject)); // This param is not multidimensional, it must have a RangeIndex of -1

		if (!ProjectorParameter.RangeValues.IsValidIndex(RangeIndex))
		{
			const int32 InsertionIndex = ProjectorParameter.RangeValues.Num();
			const int32 NumInsertedElements = RangeIndex + 1 - ProjectorParameter.RangeValues.Num();
			ProjectorParameter.RangeValues.InsertDefaulted(InsertionIndex, NumInsertedElements);
		}

		check(ProjectorParameter.RangeValues.IsValidIndex(RangeIndex));
		ProjectorParameter.RangeValues[RangeIndex] = ProjectorData;
	}
}


void FCustomizableObjectInstanceDescriptor::SetProjectorPosition(const FString& ProjectorParamName, const FVector& Pos, const int32 RangeIndex)
{
	FVector DummyPos, Direction, Up, Scale;
	float Angle;
	ECustomizableObjectProjectorType Type;
   	GetProjectorValue(ProjectorParamName, DummyPos, Direction, Up, Scale, Angle, Type, RangeIndex);
	
	SetProjectorValue(ProjectorParamName, static_cast<FVector>(Pos), Direction, Up, Scale, Angle, RangeIndex);
}


void FCustomizableObjectInstanceDescriptor::SetProjectorDirection(const FString& ProjectorParamName, const FVector& Direction, int32 RangeIndex)
{
	FVector Position, DummyDirection, Up, Scale;
	float Angle;
	ECustomizableObjectProjectorType Type;
	GetProjectorValue(ProjectorParamName, Position, DummyDirection, Up, Scale, Angle, Type, RangeIndex);
		
	SetProjectorValue(ProjectorParamName, Position, Direction, Up, Scale, Angle, RangeIndex);
}


void FCustomizableObjectInstanceDescriptor::SetProjectorUp(const FString& ProjectorParamName, const FVector& Up, int32 RangeIndex)
{
	FVector Position, Direction, DummyUp, Scale;
	float Angle;
	ECustomizableObjectProjectorType Type;
	GetProjectorValue(ProjectorParamName, Position, Direction, DummyUp, Scale, Angle, Type, RangeIndex);

	SetProjectorValue(ProjectorParamName, Position, Direction, Up, Scale, Angle, RangeIndex);
}


void FCustomizableObjectInstanceDescriptor::SetProjectorScale(const FString& ProjectorParamName, const FVector& Scale, int32 RangeIndex)
{
	FVector Position, Direction, Up, DummyScale;
	float Angle;
	ECustomizableObjectProjectorType Type;
	GetProjectorValue(ProjectorParamName, Position, Direction, Up, DummyScale, Angle, Type, RangeIndex);
	
	SetProjectorValue(ProjectorParamName, Position, Direction, Up, Scale, Angle, RangeIndex);
}


void FCustomizableObjectInstanceDescriptor::SetProjectorAngle(const FString& ProjectorParamName, float Angle, int32 RangeIndex)
{
	FVector Position, Direction, Up, Scale;
	float DummyAngle;
	ECustomizableObjectProjectorType Type;
	GetProjectorValue(ProjectorParamName, Position, Direction, Up, Scale, DummyAngle, Type, RangeIndex);
	
	SetProjectorValue(ProjectorParamName, Position, Direction, Up, Scale, Angle, RangeIndex);
}


void FCustomizableObjectInstanceDescriptor::GetProjectorValue(const FString& ProjectorParamName,
                                                              FVector& OutPos, FVector& OutDirection, FVector& OutUp, FVector& OutScale,
                                                              float& OutAngle, ECustomizableObjectProjectorType& OutType,
                                                              const int32 RangeIndex) const
{
	FVector3f Pos, Direction, Up, Scale;
	GetProjectorValueF(ProjectorParamName, Pos, Direction, Up, Scale, OutAngle, OutType, RangeIndex);

	OutPos = static_cast<FVector>(Pos);
	OutDirection = static_cast<FVector>(Direction);
	OutUp = static_cast<FVector>(Up);
	OutScale = static_cast<FVector>(Scale);
}


void FCustomizableObjectInstanceDescriptor::GetProjectorValueF(const FString& ProjectorParamName,
	FVector3f& OutPos, FVector3f& OutDirection, FVector3f& OutUp, FVector3f& OutScale,
	float& OutAngle, ECustomizableObjectProjectorType& OutType,
	const int32 RangeIndex) const
{
	check(CustomizableObject);

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(ProjectorParamName);
	const int32 ParameterIndexInInstance = FindProjectorParameterNameIndex(ProjectorParamName);

	if (ParameterIndexInObject >= 0 && ParameterIndexInInstance >= 0)
	{
		const FCustomizableObjectProjectorParameterValue& ProjectorParameter = ProjectorParameters[ParameterIndexInInstance];
		const FCustomizableObjectProjector* ProjectorData;

		if (RangeIndex == -1)
		{
			check(!CustomizableObject->IsParameterMultidimensional(ParameterIndexInObject)); // This param is multidimensional, it must have a RangeIndex of 0 or more

			ProjectorData = &ProjectorParameter.Value;
		}
		else
		{
			check(CustomizableObject->IsParameterMultidimensional(ParameterIndexInObject)); // This param is not multidimensional, it must have a RangeIndex of -1
			check(ProjectorParameter.RangeValues.IsValidIndex(RangeIndex));

			ProjectorData = &ProjectorParameter.RangeValues[RangeIndex];
		}

		if (ProjectorData)
		{
			OutPos = ProjectorData->Position;
			OutDirection = ProjectorData->Direction;
			OutUp = ProjectorData->Up;
			OutScale = ProjectorData->Scale;
			OutAngle = ProjectorData->Angle;
			OutType = ProjectorData->ProjectionType;
		}
	}
	else
	{
		LogParameterNotFoundWarning(ProjectorParamName, ParameterIndexInObject, ParameterIndexInInstance, CustomizableObject, __FUNCTION__);
	}
}


FVector FCustomizableObjectInstanceDescriptor::GetProjectorPosition(const FString& ParamName, const int32 RangeIndex) const
{
	check(CustomizableObject);

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(ParamName);
	const int32 ParameterIndexInInstance = FindProjectorParameterNameIndex(ParamName);

	if (ParameterIndexInObject >= 0 && ParameterIndexInInstance >= 0)
	{
		if (RangeIndex == -1)
		{
			return static_cast<FVector>(ProjectorParameters[ParameterIndexInInstance].Value.Position);
		}
		else if (ProjectorParameters[ParameterIndexInInstance].RangeValues.IsValidIndex(RangeIndex))
		{
			return static_cast<FVector>(ProjectorParameters[ParameterIndexInInstance].RangeValues[RangeIndex].Position);
		}
	}

	LogParameterNotFoundWarning(ParamName, ParameterIndexInObject, ParameterIndexInInstance, CustomizableObject, __FUNCTION__);

	return FVector(-0.0,-0.0,-0.0);
}


FVector FCustomizableObjectInstanceDescriptor::GetProjectorDirection(const FString& ParamName, const int32 RangeIndex) const
{
	check(CustomizableObject);

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(ParamName);
	const int32 ParameterIndexInInstance = FindProjectorParameterNameIndex(ParamName);

	if (ParameterIndexInObject >= 0 && ParameterIndexInInstance >= 0)
	{
		if (RangeIndex == -1)
		{
			return static_cast<FVector>(ProjectorParameters[ParameterIndexInInstance].Value.Direction);
		}
		else if (ProjectorParameters[ParameterIndexInInstance].RangeValues.IsValidIndex(RangeIndex))
		{
			return static_cast<FVector>(ProjectorParameters[ParameterIndexInInstance].RangeValues[RangeIndex].Direction);
		}
	}
	
	LogParameterNotFoundWarning(ParamName, ParameterIndexInObject, ParameterIndexInInstance, CustomizableObject, __FUNCTION__);

	return FVector(-0.0, -0.0, -0.0);
}


FVector FCustomizableObjectInstanceDescriptor::GetProjectorUp(const FString& ParamName, const int32 RangeIndex) const
{
	check(CustomizableObject);

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(ParamName);
	const int32 ParameterIndexInInstance = FindProjectorParameterNameIndex(ParamName);

	if (ParameterIndexInObject >= 0 && ParameterIndexInInstance >= 0)
	{
		if (RangeIndex == -1)
		{
			return static_cast<FVector>(ProjectorParameters[ParameterIndexInInstance].Value.Up);
		}
		else if (ProjectorParameters[ParameterIndexInInstance].RangeValues.IsValidIndex(RangeIndex))
		{
			return static_cast<FVector>(ProjectorParameters[ParameterIndexInInstance].RangeValues[RangeIndex].Up);
		}
	}
	
	LogParameterNotFoundWarning(ParamName, ParameterIndexInObject, ParameterIndexInInstance, CustomizableObject, __FUNCTION__);

	return FVector(-0.0, -0.0, -0.0);	
}


FVector FCustomizableObjectInstanceDescriptor::GetProjectorScale(const FString& ParamName, const int32 RangeIndex) const
{
	check(CustomizableObject);

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(ParamName);
	const int32 ParameterIndexInInstance = FindProjectorParameterNameIndex(ParamName);

	if (ParameterIndexInObject >= 0 && ParameterIndexInInstance >= 0)
	{
		if (RangeIndex == -1)
		{
			return static_cast<FVector>(ProjectorParameters[ParameterIndexInInstance].Value.Scale);
		}
		else if (ProjectorParameters[ParameterIndexInInstance].RangeValues.IsValidIndex(RangeIndex))
		{
			return static_cast<FVector>(ProjectorParameters[ParameterIndexInInstance].RangeValues[RangeIndex].Scale);
		}
	}
	
	LogParameterNotFoundWarning(ParamName, ParameterIndexInObject, ParameterIndexInInstance, CustomizableObject, __FUNCTION__);

	return FVector(-0.0, -0.0, -0.0);
}


float FCustomizableObjectInstanceDescriptor::GetProjectorAngle(const FString& ParamName, const int32 RangeIndex) const
{
	check(CustomizableObject);

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(ParamName);
	const int32 ParameterIndexInInstance = FindProjectorParameterNameIndex(ParamName);

	if (ParameterIndexInObject >= 0 && ParameterIndexInInstance >= 0)
	{
		if (RangeIndex == -1)
		{
			return ProjectorParameters[ParameterIndexInInstance].Value.Angle;
		}
		else if (ProjectorParameters[ParameterIndexInInstance].RangeValues.IsValidIndex(RangeIndex))
		{
			return ProjectorParameters[ParameterIndexInInstance].RangeValues[RangeIndex].Angle;
		}
	}

	LogParameterNotFoundWarning(ParamName, ParameterIndexInObject, ParameterIndexInInstance, CustomizableObject, __FUNCTION__);

	return 0.0;
}


ECustomizableObjectProjectorType FCustomizableObjectInstanceDescriptor::GetProjectorParameterType(const FString& ParamName, const int32 RangeIndex) const
{
	check(CustomizableObject);

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(ParamName);
	const int32 ParameterIndexInInstance = FindProjectorParameterNameIndex(ParamName);

	if (ParameterIndexInObject >= 0 && ParameterIndexInInstance >= 0)
	{
		if (RangeIndex == -1)
		{
			return ProjectorParameters[ParameterIndexInInstance].Value.ProjectionType;
		}
		else if (ProjectorParameters[ParameterIndexInInstance].RangeValues.IsValidIndex(RangeIndex))
		{
			return ProjectorParameters[ParameterIndexInInstance].RangeValues[RangeIndex].ProjectionType;
		}
	}

	LogParameterNotFoundWarning(ParamName, ParameterIndexInObject, ParameterIndexInInstance, CustomizableObject, __FUNCTION__);

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

	LogParameterNotFoundWarning(ParamName, Index, Index, CustomizableObject, __FUNCTION__);

	return FCustomizableObjectProjectorParameterValue::DEFAULT_PARAMETER_VALUE;
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


int32 FCustomizableObjectInstanceDescriptor::FindTextureParameterNameIndex(const FString& ParamName) const
{
	for (int32 i = 0; i < TextureParameters.Num(); ++i)
	{
		if (TextureParameters[i].ParameterName == ParamName)
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

int32 FCustomizableObjectInstanceDescriptor::GetProjectorValueRange(const FString& ParamName) const
{
	check(CustomizableObject);

	const int32 ProjectorParamIndex = FindProjectorParameterNameIndex(ParamName);
	if (ProjectorParamIndex < 0)
	{
		return -1;
	}

	return ProjectorParameters[ProjectorParamIndex].RangeValues.Num();
}

int32 FCustomizableObjectInstanceDescriptor::GetIntValueRange(const FString& ParamName) const
{
	check(CustomizableObject);

	const int32 IntParamIndex = FindIntParameterNameIndex(ParamName);
	if (IntParamIndex < 0)
	{
		return -1;
	}

	return IntParameters[IntParamIndex].ParameterRangeValueNames.Num();
}


int32 FCustomizableObjectInstanceDescriptor::GetFloatValueRange(const FString& ParamName) const
{
	check(CustomizableObject);

	const int32 FloatParamIndex = FindFloatParameterNameIndex(ParamName);
	if (FloatParamIndex < 0)
	{
		return -1;
	}

	return FloatParameters[FloatParamIndex].ParameterRangeValues.Num();
}


int32 FCustomizableObjectInstanceDescriptor::GetTextureValueRange(const FString& ParamName) const
{
	check(CustomizableObject);

	const int32 TextureParamIndex = FindTextureParameterNameIndex(ParamName);
	if (TextureParamIndex < 0)
	{
		return -1;
	}

	return TextureParameters[TextureParamIndex].ParameterRangeValues.Num();
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
		return intParameter.ParameterRangeValueNames.Add(DefaultValue);
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
		return floatParameter.ParameterRangeValues.Add(0.5f);
	}
	return -1;
}


int32 FCustomizableObjectInstanceDescriptor::AddValueToTextureRange(const FString& ParamName)
{
	const int32 TextureParameterIndex = FindTextureParameterNameIndex(ParamName);
	if (TextureParameterIndex != -1)
	{
		FCustomizableObjectTextureParameterValue& TextureParameter = TextureParameters[TextureParameterIndex];
		return TextureParameter.ParameterRangeValues.Add(FName());		
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
		const FCustomizableObjectProjector Projector = GetCustomizableObject()->GetProjectorParameterDefaultValue(ParamName);
		return ProjectorParameter.RangeValues.Add(Projector);
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


int32 FCustomizableObjectInstanceDescriptor::RemoveValueFromTextureRange(const FString& ParamName)
{
	const int32 TextureParameterIndex = FindTextureParameterNameIndex(ParamName);
	if (TextureParameterIndex != -1)
	{
		FCustomizableObjectTextureParameterValue& TextureParameter = TextureParameters[TextureParameterIndex];
		if (TextureParameter.ParameterRangeValues.Num() > 0)
		{
			TextureParameter.ParameterRangeValues.Pop();
			return TextureParameter.ParameterRangeValues.Num() - 1;
		}
	}
	return -1;
}


int32 FCustomizableObjectInstanceDescriptor::RemoveValueFromTextureRange(const FString& ParamName, const int32 RangeIndex)
{
	const int32 TextureParameterIndex = FindTextureParameterNameIndex(ParamName);
	if (TextureParameterIndex != -1)
	{
		FCustomizableObjectTextureParameterValue& TextureParameter = TextureParameters[TextureParameterIndex];
		if (TextureParameter.ParameterRangeValues.Num() > 0)
		{
			TextureParameter.ParameterRangeValues.RemoveAt(RangeIndex);
			return TextureParameter.ParameterRangeValues.Num() - 1;
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
	RETURN_ON_UNCOMPILED_CO(CustomizableObject, TEXT("Error: Cannot set state"))

	const int32 Result = CustomizableObject->FindState(StateName);
#if WITH_EDITOR
	if (Result != INDEX_NONE)
#else
	if (ensureMsgf(Result != INDEX_NONE, TEXT("Unknown %s state."), *StateName))
#endif
	{
		SetState(Result);
	}
	else
	{
		UE_LOG(LogMutable, Error, TEXT("%hs: Unknown %s state."), __FUNCTION__, *StateName);
	}
}


void FCustomizableObjectInstanceDescriptor::SetRandomValues()
{
	const int32 RandomSeed = FMath::SRand() * TNumericLimits<int32>::Max();
	FRandomStream RandomStream{RandomSeed};
	SetRandomValuesFromStream(RandomSeed);
}


void FCustomizableObjectInstanceDescriptor::SetRandomValuesFromStream(const FRandomStream& InStream)
{
	check(CustomizableObject);

	if (!CustomizableObject)
	{
		return;
	}
	
	RETURN_ON_UNCOMPILED_CO(CustomizableObject, TEXT("Error: Cannot set random values"))
	
	for (FCustomizableObjectFloatParameterValue& FloatParameter : FloatParameters)
	{
		FloatParameter.ParameterValue = InStream.GetFraction();

		for (float& RangeValue : FloatParameter.ParameterRangeValues)
		{
			RangeValue = InStream.GetFraction();
		}
	}

	for (FCustomizableObjectBoolParameterValue& BoolParameter : BoolParameters)
	{
		BoolParameter.ParameterValue = static_cast<bool>(InStream.RandRange(0, 1));
	}
	
	for (FCustomizableObjectIntParameterValue& IntParameter : IntParameters)
	{
		const int32 ParamIndex = CustomizableObject->FindParameter(IntParameter.ParameterName);
		const int32 NumValues = CustomizableObject->GetIntParameterNumOptions(ParamIndex);

		if (NumValues)
		{
			IntParameter.ParameterValueName = CustomizableObject->GetIntParameterAvailableOption(ParamIndex, NumValues * InStream.GetFraction());

			for (FString& RangeValue : IntParameter.ParameterRangeValueNames)
			{
				RangeValue = CustomizableObject->GetIntParameterAvailableOption(ParamIndex, NumValues * InStream.GetFraction());
			}
		}		
	}

	for (FCustomizableObjectVectorParameterValue& VectorParameter : VectorParameters)
	{
		VectorParameter.ParameterValue.R = InStream.GetFraction();
		VectorParameter.ParameterValue.G = InStream.GetFraction();
		VectorParameter.ParameterValue.B = InStream.GetFraction();
		VectorParameter.ParameterValue.A = InStream.GetFraction();
	}

	const UCustomizableObjectSystemPrivate* SystemPrivate = UCustomizableObjectSystem::GetInstance()->GetPrivate();

	TArray<FName> PossibleValues;

	// Get all possible values
	TArray<FCustomizableObjectExternalTexture> ProviderValues;
	for (const TWeakObjectPtr<UCustomizableSystemImageProvider>& Provider : SystemPrivate->ImageProvider->ImageProviders)
	{
		ProviderValues.Reset();
		Provider->GetTextureParameterValues(ProviderValues);

		for (const FCustomizableObjectExternalTexture& ProviderValue : ProviderValues)
		{
			PossibleValues.Add(ProviderValue.Value);
		}
	}

	if (const int32 NumPossibleValues = PossibleValues.Num())
	{
		for (FCustomizableObjectTextureParameterValue& TextureParameter : TextureParameters)
		{
			TextureParameter.ParameterValue = PossibleValues[NumPossibleValues * InStream.GetFraction()];
		
			for (FName& RangeValue : TextureParameter.ParameterRangeValues)
			{
				RangeValue = PossibleValues[NumPossibleValues * InStream.GetFraction()];
			}				
		}		
	}
	
	// Currently we are not randomizing the projectors since we do not know the valid range of values.
}


bool FCustomizableObjectInstanceDescriptor::CreateMultiLayerProjector(const FName& ProjectorParamName)
{
	if (!FMultilayerProjector::AreDescriptorParametersValid(*this, ProjectorParamName.ToString()))
	{
#if WITH_EDITOR
		UE_LOG(LogMutable, Error, TEXT("%s"), *FMultilayerProjector::DESCRIPTOR_PARAMETERS_INVALID);
#else
		ensureAlwaysMsgf(false, TEXT("%s"), *FMultilayerProjector::DESCRIPTOR_PARAMETERS_INVALID);
#endif
		return false;
	}

	if (!MultilayerProjectors.Contains(ProjectorParamName))
	{
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


#undef RETURN_ON_CO_UNCOMPILED
