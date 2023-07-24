// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/CustomizableObjectInstanceDescriptor.h"

#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/DefaultImageProvider.h"
#include "MuR/Model.h"
#include "MuR/MutableMemory.h"
#include "MuR/Parameters.h"
#include "MuR/Ptr.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CustomizableObjectInstanceDescriptor)


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
	SetCustomizableObject(Object);
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
			TArray<uint64> RangeValues;

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
			TArray<uint64> RangeValues;
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


bool FCustomizableObjectInstanceDescriptor::GetBuildParameterDecorations() const
{
	return bBuildParameterDecorations;
}


void FCustomizableObjectInstanceDescriptor::SetBuildParameterDecorations(const bool Value)
{
	bBuildParameterDecorations = Value;
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
		const FString& Uid = MutableParameters->GetUid(ParamIndex);
		const mu::PARAMETER_TYPE MutableType = MutableParameters->GetType(ParamIndex);

		switch (MutableType)
		{
		case mu::PARAMETER_TYPE::T_BOOL:
		{
			for (const FCustomizableObjectBoolParameterValue& BoolParameter : BoolParameters)
			{
				if (BoolParameter.ParameterName == Name || (!Uid.IsEmpty() && BoolParameter.Uid == Uid))
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
				if (IntParameter.ParameterName.Equals(Name, ESearchCase::CaseSensitive) || (!Uid.IsEmpty() && IntParameter.Uid == Uid))
				{
					if (mu::RangeIndexPtr RangeIdxPtr = MutableParameters->NewRangeIndex(ParamIndex))
					{
						for (int RangeIndex = 0; RangeIndex < IntParameter.ParameterRangeValueNames.Num(); ++RangeIndex)
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
				if (FloatParameter.ParameterName == Name || (!Uid.IsEmpty() && FloatParameter.Uid == Uid))
				{
					if (mu::RangeIndexPtr RangeIdxPtr = MutableParameters->NewRangeIndex(ParamIndex))
					{
						for (int RangeIndex = 0; RangeIndex < FloatParameter.ParameterRangeValues.Num(); ++RangeIndex)
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
				if (VectorParameter.ParameterName == Name || (!Uid.IsEmpty() && VectorParameter.Uid == Uid))
				{
					MutableParameters->SetColourValue(ParamIndex, VectorParameter.ParameterValue.R, VectorParameter.ParameterValue.G, VectorParameter.ParameterValue.B);

					break;
				}
			}

			break;
		}

		case mu::PARAMETER_TYPE::T_PROJECTOR:
		{
			for (const auto& ProjectorParameter : ProjectorParameters)
			{
				if (ProjectorParameter.ParameterName == Name || (!Uid.IsEmpty() && ProjectorParameter.Uid == Uid))
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

					if (const mu::RangeIndexPtr RangeIdxPtr = MutableParameters->NewRangeIndex(ParamIndex))
					{
						for (int RangeIndex = 0; RangeIndex < ProjectorParameter.RangeValues.Num(); ++RangeIndex)
						{
							RangeIdxPtr->SetPosition(0, RangeIndex);
							CopyProjector(ProjectorParameter.RangeValues[RangeIndex], RangeIdxPtr);
						}
					}
					else
					{
						CopyProjector(ProjectorParameter.Value);
					}
				}
			}

			break;
		}

		case mu::PARAMETER_TYPE::T_IMAGE:
		{
			for (const FCustomizableObjectTextureParameterValue& TextureParameter : TextureParameters)
			{
				if (TextureParameter.ParameterName == Name || (!Uid.IsEmpty() && TextureParameter.Uid == Uid))
				{
					if (mu::RangeIndexPtr RangeIdxPtr = MutableParameters->NewRangeIndex(ParamIndex))
					{
						for (int RangeIndex = 0; RangeIndex < TextureParameter.ParameterRangeValues.Num(); ++RangeIndex)
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


void FCustomizableObjectInstanceDescriptor::ReloadParameters()
{
	if (!CustomizableObject)
	{
		return;
	}

	if (!CustomizableObject->IsCompiled())
	{	
		return;
	}

	SetState(FMath::Clamp(GetState(), 0, CustomizableObject->GetStateCount() - 1));
	
	MaxLOD = FMath::Max(0, CustomizableObject->GetNumLODs() - 1);
	RequestedLODLevels.Init(0, CustomizableObject->GetComponentCount());

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
		UE_LOG(LogMutable, Warning, TEXT("[ReloadParametersFromObject] No model in object [%s], generated empty parameters for [%s] "), *CustomizableObject->GetName());
		return;
	}

	mu::ParametersPtr MutableParameters = mu::Model::NewParameters(CustomizableObject->GetPrivate()->GetModel());

	int32 ParamCount = MutableParameters->GetCount();
	for (int32 ParamIndex = 0; ParamIndex < ParamCount; ++ParamIndex)
	{
		const FString& Name = MutableParameters->GetName(ParamIndex);
		const FString& Uid = MutableParameters->GetUid(ParamIndex);
		const mu::PARAMETER_TYPE MutableType = MutableParameters->GetType(ParamIndex);

		switch (MutableType)
		{
		case mu::PARAMETER_TYPE::T_BOOL:
		{
			FCustomizableObjectBoolParameterValue Param;
			Param.ParameterName = Name;
			Param.Uid = Uid;

			auto FindByNameAndUid = [&](const FCustomizableObjectBoolParameterValue& P)
			{
				return P.ParameterName == Name || (!Uid.IsEmpty() && P.Uid == Uid);
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
			Param.Uid = Uid;

			auto FindByNameAndUid = [&](const FCustomizableObjectIntParameterValue& P)
			{
				return P.ParameterName == Name || (!Uid.IsEmpty() && P.Uid == Uid);
			};
			
			if (FCustomizableObjectIntParameterValue* Result = OldIntParameters.FindByPredicate(FindByNameAndUid))
			{	
				if (mu::RangeIndexPtr RangeIdxPtr = MutableParameters->NewRangeIndex(ParamIndex))
				{
					Param.ParameterRangeValueNames = Result->ParameterRangeValueNames;
				}
				else
				{
					Param.ParameterValueName = Result->ParameterValueName;
				}
			} 
			else // Not found in Instance Parameters. Use Mutable Parameters.
			{
				if (mu::RangeIndexPtr RangeIdxPtr = MutableParameters->NewRangeIndex(ParamIndex))
				{
					const int32 ValueCount = MutableParameters->GetValueCount(ParamIndex);

					for (int ValueIndex = 0; ValueIndex < ValueCount; ++ValueIndex)
					{	
						const mu::RangeIndexPtr RangeValueIdxPtr = MutableParameters->GetValueIndex(ParamIndex, ValueIndex);
						const int32 RangeIndex = RangeValueIdxPtr->GetPosition(0);

						if (!Param.ParameterRangeValueNames.IsValidIndex(ValueIndex))
						{
							Param.ParameterRangeValueNames.AddDefaulted(ValueIndex + 1 - Param.ParameterRangeValueNames.Num());
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
			Param.Uid = Uid;

			auto FindByNameAndUid = [&](const FCustomizableObjectFloatParameterValue& P)
			{
				return P.ParameterName == Name || (!Uid.IsEmpty() && P.Uid == Uid);
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

					for (int ValueIndex = 0; ValueIndex < ValueCount; ++ValueIndex)
					{	
						mu::RangeIndexPtr RangeValueIdxPtr = MutableParameters->GetValueIndex(ParamIndex, ValueIndex);
						int32 RangeIndex = RangeValueIdxPtr->GetPosition(0);

						if (!Param.ParameterRangeValues.IsValidIndex(ValueIndex))
						{
							Param.ParameterRangeValues.AddDefaulted(ValueIndex + 1 - Param.ParameterRangeValues.Num());
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
			Param.Uid = Uid;

			auto FindByNameAndUid = [&](const FCustomizableObjectVectorParameterValue& P)
			{
				return P.ParameterName == Name || (!Uid.IsEmpty() && P.Uid == Uid);
			};

			if (FCustomizableObjectVectorParameterValue* Result = OldVectorParameters.FindByPredicate(FindByNameAndUid))
			{	
				Param.ParameterValue = Result->ParameterValue;
			}
			else // Not found in Instance Parameters. Use Mutable Parameters.
			{
				MutableParameters->GetColourValue(ParamIndex, &Param.ParameterValue.R, &Param.ParameterValue.G, &Param.ParameterValue.B);
				Param.ParameterValue.A = 1.0f;
			}

			VectorParameters.Add(Param);
			break;
		}

		case mu::PARAMETER_TYPE::T_PROJECTOR:
		{
			FCustomizableObjectProjectorParameterValue Param;
			Param.ParameterName = Name;
			Param.Uid = Uid;

			auto FindByNameAndUid = [&](const FCustomizableObjectProjectorParameterValue& P)
			{
				return P.ParameterName == Name || (!Uid.IsEmpty() && P.Uid == Uid);
			};
			
			if (FCustomizableObjectProjectorParameterValue* Result = OldProjectorParameters.FindByPredicate(FindByNameAndUid))
			{	
				if (mu::RangeIndexPtr RangeIdxPtr = MutableParameters->NewRangeIndex(ParamIndex))
				{
					Param.RangeValues = Result->RangeValues;
				}
				else
				{
					Param.Value = Result->Value;
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
					
					switch (Type)
					{
					case mu::PROJECTOR_TYPE::PLANAR:
						Value.ProjectionType = ECustomizableObjectProjectorType::Planar;
						break;

					case mu::PROJECTOR_TYPE::CYLINDRICAL:
						// Unapply strange swizzle for scales.
						// TODO: try to avoid this
						Value.ProjectionType = ECustomizableObjectProjectorType::Cylindrical;
						Value.Direction = -Value.Direction;
						Value.Up = -Value.Up;
						Value.Scale[2] = -Value.Scale[0];
						Value.Scale[0] = Value.Scale[1] = Value.Scale[1] * 2.0f;
						break;

					case mu::PROJECTOR_TYPE::WRAPPING:
						Value.ProjectionType = ECustomizableObjectProjectorType::Wrapping;
						break;

					default:
						check(false); // Not implemented.
					}
				}; 

				if (mu::RangeIndexPtr RangeIdxPtr = MutableParameters->NewRangeIndex(ParamIndex))
				{
					const int32 ValueCount = MutableParameters->GetValueCount(ParamIndex);

					for (int ValueIndex = 0; ValueIndex < ValueCount; ++ValueIndex)
					{	
						mu::RangeIndexPtr RangeValueIdxPtr = MutableParameters->GetValueIndex(ParamIndex, ValueIndex);
						int32 RangeIndex = RangeValueIdxPtr->GetPosition(0);

						if (!Param.RangeValues.IsValidIndex(ValueIndex))
						{
							Param.RangeValues.AddDefaulted(ValueIndex + 1 - Param.RangeValues.Num());
						}

						GetProjector(Param.RangeValues[RangeIndex], RangeValueIdxPtr);
					}
				}
				else
				{
					GetProjector(Param.Value);
				}
			}

			ProjectorParameters.Add(Param);
			break;
		}

		case mu::PARAMETER_TYPE::T_IMAGE:
		{
			FCustomizableObjectTextureParameterValue Param;
			Param.ParameterName = Name;
			Param.Uid = Uid;

			auto FindByNameAndUid = [&](const FCustomizableObjectTextureParameterValue& P)
			{
				return P.ParameterName == Name || (!Uid.IsEmpty() && P.Uid == Uid);
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

					for (int ValueIndex = 0; ValueIndex < ValueCount; ++ValueIndex)
					{	
						mu::RangeIndexPtr RangeValueIdxPtr = MutableParameters->GetValueIndex(ParamIndex, ValueIndex);
						int32 RangeIndex = RangeValueIdxPtr->GetPosition(0);

						if (!Param.ParameterRangeValues.IsValidIndex(ValueIndex))
						{
							Param.ParameterRangeValues.AddDefaulted(ValueIndex + 1 - Param.ParameterRangeValues.Num());
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


int32 FCustomizableObjectInstanceDescriptor::GetMaxLod() const
{
	return MaxLOD;
}


void FCustomizableObjectInstanceDescriptor::SetMaxLod(int32 InMaxLOD)
{
	MaxLOD = InMaxLOD;
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


uint64 FCustomizableObjectInstanceDescriptor::GetTextureParameterSelectedOption(const FString& TextureParamName, const int32 RangeIndex) const
{
	check(CustomizableObject);

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(TextureParamName);

	const int32 TextureParamIndex = FindTextureParameterNameIndex(TextureParamName);

	if (ParameterIndexInObject >= 0 && TextureParamIndex >= 0)
	{
		if (RangeIndex == -1)
		{
			check(!IsParamMultidimensional(ParameterIndexInObject)); // This param is multidimensional, it must have a RangeIndex of 0 or more
			return TextureParameters[TextureParamIndex].ParameterValue;
		}
		else
		{
			check(IsParamMultidimensional(ParameterIndexInObject)); // This param is not multidimensional, it must have a RangeIndex of -1

			if (TextureParameters[TextureParamIndex].ParameterRangeValues.IsValidIndex(RangeIndex))
			{
				return TextureParameters[TextureParamIndex].ParameterRangeValues[RangeIndex];
			}
		}
	}

	return -1; 
}


UTexture2D* FCustomizableObjectInstanceDescriptor::GetTextureParameterSelectedOptionT(const FString& TextureParamName, const int32 RangeIndex) const
{
	UDefaultImageProvider& ImageProvider = UCustomizableObjectSystem::GetInstance()->GetOrCreateDefaultImageProvider();

	const uint64 TextureValue = GetTextureParameterSelectedOption(TextureParamName, RangeIndex);
	return ImageProvider.Get(TextureValue);
}


void FCustomizableObjectInstanceDescriptor::SetTextureParameterSelectedOption(const FString& TextureParamName, const uint64 TextureValue, const int32 RangeIndex)
{
	check(CustomizableObject);

	const int32 ParameterIndexInObject = CustomizableObject->FindParameter(TextureParamName);

	const int32 TextureParamIndex = FindTextureParameterNameIndex(TextureParamName);

	if (ParameterIndexInObject >= 0 && TextureParamIndex >= 0)
	{
		if (RangeIndex == -1)
		{
			check(!IsParamMultidimensional(ParameterIndexInObject)); // This param is multidimensional, it must have a RangeIndex of 0 or more
			TextureParameters[TextureParamIndex].ParameterValue = TextureValue;
		}
		else
		{
			check(IsParamMultidimensional(ParameterIndexInObject)); // This param is not multidimensional, it must have a RangeIndex of -1

			if (!TextureParameters[TextureParamIndex].ParameterRangeValues.IsValidIndex(RangeIndex))
			{
				const int32 InsertionIndex = TextureParameters[TextureParamIndex].ParameterRangeValues.Num();
				const int32 NumInsertedElements = RangeIndex + 1 - TextureParameters[TextureParamIndex].ParameterRangeValues.Num();
				TextureParameters[TextureParamIndex].ParameterRangeValues.InsertDefaulted(InsertionIndex, NumInsertedElements);
			}

			check(TextureParameters[TextureParamIndex].ParameterRangeValues.IsValidIndex(RangeIndex));
			TextureParameters[TextureParamIndex].ParameterRangeValues[RangeIndex] = TextureValue;
		}
	}
}


void FCustomizableObjectInstanceDescriptor::SetTextureParameterSelectedOptionT(const FString& TextureParamName, UTexture2D* TextureValue, const int32 RangeIndex)
{
	UDefaultImageProvider& ImageProvider = UCustomizableObjectSystem::GetInstance()->GetOrCreateDefaultImageProvider();

	const uint64 NewUid = ImageProvider.GetOrAdd(TextureValue);
	SetTextureParameterSelectedOption(TextureParamName, NewUid, RangeIndex);
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


bool FCustomizableObjectInstanceDescriptor::IsParamMultidimensional(const FString& ParamName) const
{
	check(CustomizableObject);

	const int32 ParameterIndex = CustomizableObject->FindParameter(ParamName);
	return IsParamMultidimensional(ParameterIndex);
}


bool FCustomizableObjectInstanceDescriptor::IsParamMultidimensional(const int32 ParamIndex) const
{
	check(CustomizableObject);

	const mu::ParametersPtr MutableParameters = mu::Model::NewParameters(CustomizableObject->GetPrivate()->GetModel());
	check(ParamIndex < MutableParameters->GetCount());
	const mu::RangeIndexPtr RangeIdxPtr = MutableParameters->NewRangeIndex(ParamIndex);

	return RangeIdxPtr.get() != nullptr;
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


int32 FCustomizableObjectInstanceDescriptor::AddValueToProjectorRange(const FString& ParamName)
{
	check(CustomizableObject);

	const int32 projectorParameterIndex = FindProjectorParameterNameIndex(ParamName);
	if (projectorParameterIndex != -1)
	{
		FCustomizableObjectProjectorParameterValue& ProjectorParameter = ProjectorParameters[projectorParameterIndex];
		const FCustomizableObjectProjector Projector = GetProjectorDefaultValue(CustomizableObject->FindParameter(ParamName));
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


FCustomizableObjectProjector FCustomizableObjectInstanceDescriptor::GetProjectorDefaultValue(int32 const ParamIndex) const
{
	check(CustomizableObject);

	const mu::ParametersPtr MutableParameters = mu::Model::NewParameters(CustomizableObject->GetPrivate()->GetModel());
	check(ParamIndex < MutableParameters->GetCount());

	FCustomizableObjectProjector Projector;

	mu::PROJECTOR_TYPE type;
	
	MutableParameters->GetProjectorValue(ParamIndex,
		&type,
		&Projector.Position,
		&Projector.Direction,
		&Projector.Up,
		&Projector.Scale,
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

	const int32 Result = CustomizableObject->FindState(StateName);
	if (ensureMsgf(Result != -1, TEXT("Unknown %s state."), *StateName))
	{
		SetState(Result);
	}
	else
	{
		UE_LOG(LogMutable, Error, TEXT("%s: Unknown %s state."), ANSI_TO_TCHAR(__FUNCTION__), *StateName);
	}
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

FDescriptorHash::FDescriptorHash(const FCustomizableObjectInstanceDescriptor& Descriptor)
{
	if (Descriptor.CustomizableObject)
	{
		Hash = HashCombine(Hash, GetTypeHash(Descriptor.CustomizableObject->GetPathName()));
		Hash = HashCombine(Hash, GetTypeHash(Descriptor.CustomizableObject->GetCompilationGuid()));
	}

	for (const FCustomizableObjectBoolParameterValue& Value : Descriptor.BoolParameters)
	{
		Hash = HashCombine(Hash, GetTypeHash(Value));
	}	

	for (const FCustomizableObjectIntParameterValue& Value : Descriptor.IntParameters)
	{
		Hash = HashCombine(Hash, GetTypeHash(Value));
	}
	
	for (const FCustomizableObjectFloatParameterValue& Value : Descriptor.FloatParameters)
	{
		Hash = HashCombine(Hash, GetTypeHash(Value));
	}

	for (const FCustomizableObjectTextureParameterValue& Value : Descriptor.TextureParameters)
	{
		Hash = HashCombine(Hash, GetTypeHash(Value));
	}

	for (const FCustomizableObjectVectorParameterValue& Value : Descriptor.VectorParameters)
	{
		Hash = HashCombine(Hash, GetTypeHash(Value));
	}

	for (const FCustomizableObjectProjectorParameterValue& Value : Descriptor.ProjectorParameters)
	{
		Hash = HashCombine(Hash, GetTypeHash(Value));
	}
	
	Hash = HashCombine(Hash, GetTypeHash(Descriptor.State));
	Hash = HashCombine(Hash, GetTypeHash(Descriptor.bBuildParameterDecorations));

	for (const TTuple<FName, FMultilayerProjector>& Pair : Descriptor.MultilayerProjectors)
	{
		// Hash = HashCombine(Hash, GetTypeHash(Pair.Key)); // Already hashed by the FMultilayerProjector.
		Hash = HashCombine(Hash, GetTypeHash(Pair.Value));
	}
}


bool FDescriptorHash::operator==(const FDescriptorHash& Other) const
{
	return Hash == Other.Hash;
}


bool FDescriptorHash::operator!=(const FDescriptorHash& Other) const
{
	return Hash != Other.Hash;	
}


bool FDescriptorHash::operator<(const FDescriptorHash& Other) const
{
	return Hash < Other.Hash;
}


FString FDescriptorHash::ToString() const
{
	return FString::Printf(TEXT("%u"), Hash);
}


FDescriptorRuntimeHash::FDescriptorRuntimeHash(const FCustomizableObjectInstanceDescriptor& Descriptor) :
	FDescriptorHash(Descriptor)
{
	MinLOD = Descriptor.MinLOD;
	MaxLOD = Descriptor.MaxLOD;	

	RequestedLODsPerComponent = Descriptor.RequestedLODLevels;
}


bool FDescriptorRuntimeHash::IsSubset(const FDescriptorRuntimeHash& Other) const
{
	if (FDescriptorHash::operator!=(Other) || MinLOD < Other.MinLOD || MaxLOD != Other.MaxLOD)
	{
		return false;
	}

	if (RequestedLODsPerComponent == Other.RequestedLODsPerComponent)
	{
		return true;
	}

	for (int32 ComponentIndex = 0; ComponentIndex < RequestedLODsPerComponent.Num(); ++ComponentIndex)
	{
		int32 RequestedLODs = RequestedLODsPerComponent[ComponentIndex];
		int32 OtherRequestedLODs = Other.RequestedLODsPerComponent[ComponentIndex];
		for (uint16 LODIndex = MinLOD; LODIndex <= MaxLOD; ++LODIndex)
		{
			// To be a subset all bits set in RequestedLODs must be set in OtherRequestedLODs. OtherRequestedLODs can have additional bits set
			if (RequestedLODs & (1 << LODIndex) && !(OtherRequestedLODs & (1 << LODIndex)))
			{
				return false;
			}
		}
	}

	return true;
}


void FDescriptorRuntimeHash::UpdateMinMaxLOD(const int32 InMinLOD, const int32 InMaxLOD)
{
	MinLOD = InMinLOD;
	MaxLOD = InMaxLOD;	
}


int32 FDescriptorRuntimeHash::GetMinLOD() const
{
	return MinLOD;
}


int32 FDescriptorRuntimeHash::GetMaxLOD() const
{
	return MaxLOD;
}


void FDescriptorRuntimeHash::UpdateRequestedLODs(const TArray<uint16>& InRequestedLODs)
{
	RequestedLODsPerComponent = InRequestedLODs;
}


const TArray<uint16>& FDescriptorRuntimeHash::GetRequestedLODs() const
{
	return RequestedLODsPerComponent;
}
