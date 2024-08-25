// Copyright Epic Games, Inc. All Rights Reserved.

#include "Expressions/TG_Expression_MaterialBase.h"

#include "Materials/Material.h"
#include "FxMat/RenderMaterial_BP.h"
#include "Job/Job.h"
#include "Transform/Utility/T_CombineTiledBlob.h"
#include "Transform/Utility/T_SplitToTiles.h"
#include "TG_Texture.h"
#include "TG_Var.h"
#include "FxMat/MaterialManager.h"
#include "Materials/MaterialAttributeDefinitionMap.h"


EDrawMaterialAttributeTarget UTG_Expression_MaterialBase::ConvertEMaterialPropertyToEDrawMaterialAttributeTarget(EMaterialProperty InMaterialProperty)
{
	switch (InMaterialProperty)
	{
		case MP_EmissiveColor:
			return EDrawMaterialAttributeTarget::Emissive;
		case MP_Opacity:
			return EDrawMaterialAttributeTarget::Opacity;
		case MP_OpacityMask:
			return EDrawMaterialAttributeTarget::OpacityMask;
		case MP_BaseColor:
			return EDrawMaterialAttributeTarget::BaseColor;
		case MP_Metallic:
			return EDrawMaterialAttributeTarget::Metallic;
		case MP_Specular:
			return EDrawMaterialAttributeTarget::Specular;
		case MP_Roughness:
			return EDrawMaterialAttributeTarget::Roughness;
		case MP_Anisotropy:
			return EDrawMaterialAttributeTarget::Anisotropy;
		case MP_Normal:
			return EDrawMaterialAttributeTarget::Normal;
		case MP_Tangent:
			return EDrawMaterialAttributeTarget::Tangent;

		case MP_DiffuseColor:
		case MP_SpecularColor:
		case MP_WorldPositionOffset:
		case MP_WorldDisplacement_DEPRECATED:
		case MP_TessellationMultiplier_DEPRECATED:
		case MP_SubsurfaceColor:
		case MP_AmbientOcclusion:
		case MP_Refraction:
		case MP_CustomData0:
		case MP_CustomData1:
		case MP_CustomizedUVs0:
		case MP_CustomizedUVs1:
		case MP_CustomizedUVs2:
		case MP_CustomizedUVs3:
		case MP_CustomizedUVs4:
		case MP_CustomizedUVs5:
		case MP_CustomizedUVs6:
		case MP_CustomizedUVs7:
		case MP_PixelDepthOffset:
		case MP_ShadingModel:
		case MP_FrontMaterial:
		case MP_SurfaceThickness:
		case MP_Displacement:
		case MP_MaterialAttributes:
		case MP_CustomOutput:
		default:
			return EDrawMaterialAttributeTarget::Count; // Not supported yet in the shader
	 }
}


bool UTG_Expression_MaterialBase::Validate(MixUpdateCyclePtr	Cycle)
{
	return true;
}


void UTG_Expression_MaterialBase::SetMaterialInternal(UMaterialInterface* InMaterial)
{

	if (!InMaterial)
	{
		MaterialInstance = nullptr;
	}
	else {
		if (InMaterial->IsA<UMaterial>())
		{
			// create a material instance
			MaterialInstance = UMaterialInstanceDynamic::Create(InMaterial, this);
		}
		else if (InMaterial->IsA<UMaterialInstance>())
		{
			MaterialInstance = UMaterialInstanceDynamic::Create(InMaterial, this);
		}
	}

	// Detect the set of available material properties for rendering
	GenerateMaterialAttributeOptions();
	
	// Signature is reset, notify the owning node / graph to update itself
	NotifySignatureChanged();
}


void UTG_Expression_MaterialBase::GenerateMaterialAttributeOptions()
{
	// Detect the set of available material properties for rendering
	AvailableMaterialAttributeIds.Empty();
	AvailableMaterialAttributeNames.Empty();
	if (MaterialInstance)
	{
		UMaterial* RefMaterial = MaterialInstance->GetMaterial();
		for (int i = MP_EmissiveColor; i < MP_MAX; ++i)
		{
			if (RefMaterial->IsPropertyConnected(EMaterialProperty(i)))
			{
				EDrawMaterialAttributeTarget Attribute = ConvertEMaterialPropertyToEDrawMaterialAttributeTarget(EMaterialProperty(i));
				if (Attribute != EDrawMaterialAttributeTarget::Count)
				{
					AvailableMaterialAttributeIds.Add(Attribute);


					FName AttributeName = FName(UEnum::GetDisplayValueAsText(Attribute).ToString());
					AttributeName = FName(FMaterialAttributeDefinitionMap::GetDisplayNameForMaterial(EMaterialProperty(i), RefMaterial).ToString());

					AvailableMaterialAttributeNames.Add(AttributeName);
				}
			}
		}
	}
}

void UTG_Expression_MaterialBase::Evaluate(FTG_EvaluationContext* InContext)
{
	Super::Evaluate(InContext);

	TiledBlobPtr Result = TextureHelper::GetBlack();

	/// Set it to false always
	TiledMode = true;

	if (GetMaterial() && MaterialInstance)
	{
		FString AssetName = GetMaterial()->GetName();

		const auto RenderMaterial = std::make_shared<RenderMaterial_BP>(AssetName, MaterialInstance->GetMaterial(), MaterialInstance);
		Result = CreateRenderMaterialJob(InContext, RenderMaterial, Output.GetBufferDescriptor(), GetRenderedAttributeId());
	}

	Output = Result;
}

void UTG_Expression_MaterialBase::Initialize()
{
	if(GetMaterial() && !MaterialInstance)
	{
		MaterialInstance = UMaterialInstanceDynamic::Create(GetMaterial(), this);
	}
	GenerateMaterialAttributeOptions(); // also populate the attributes availables
}

TiledBlobPtr UTG_Expression_MaterialBase::CreateRenderMaterialJob(FTG_EvaluationContext* InContext, const FString& InName, const FString& InMaterialPath, const BufferDescriptor& InDescriptor, EDrawMaterialAttributeTarget InDrawMaterialAttributeTarget)
{
	const RenderMaterial_BPPtr RenderMaterial = TextureGraphEngine::GetMaterialManager()->CreateMaterial_BP(InName, InMaterialPath);
	return CreateRenderMaterialJob(InContext, RenderMaterial, InDescriptor, InDrawMaterialAttributeTarget);
}

TiledBlobPtr UTG_Expression_MaterialBase::CreateRenderMaterialJob(FTG_EvaluationContext* InContext, const RenderMaterial_BPPtr& InRenderMaterial, const BufferDescriptor& InDescriptor, EDrawMaterialAttributeTarget InDrawMaterialAttributeTarget)
{
	/// Set it to false always
	TiledMode = true;

	check(InRenderMaterial);
	InRenderMaterial->Instance()->EnsureIsComplete();
	InRenderMaterial->Instance()->SetForceMipLevelsToBeResident(true, true, -1);

	JobUPtr MaterialJob = std::make_unique<Job>(InContext->Cycle->GetMix(), InContext->TargetId, std::static_pointer_cast<BlobTransform>(InRenderMaterial), GetParentNode());

	FLinearColor PSControl;
	PSControl.R = (int32)InDrawMaterialAttributeTarget;
	PSControl.G = (int32)EDrawMaterialAttributeTarget::Count;
	PSControl.B = 0.0; // Debug: Control the blend with UV colors 
	PSControl.A = 0.0; // Debug: Control the blend with TileUV colors

	FTileInfo tileInfo;

	MaterialJob->AddArg(ARG_STRING(InRenderMaterial->GetMaterial()->GetPathName(), "Material"));
	MaterialJob->AddArg(ARG_VECTOR(PSControl, "PSControl"));
	MaterialJob->AddArg(std::make_shared<JobArg_TileInfo>(tileInfo, "TileInfo")); /// Enable the tileinfo parameters
	MaterialJob->AddArg(std::make_shared<JobArg_ForceTiling>()); /// Force hashing individual tiles differently

	BufferDescriptor Desc = InDescriptor;

	if (Desc.IsAuto())
		Desc.Format = BufferFormat::Byte;

	if (Desc.ItemsPerPoint <= 0)
		Desc.ItemsPerPoint = 4;

	Desc.DefaultValue = FLinearColor::Black;

	LinkMaterialParameters(InContext, MaterialJob, InRenderMaterial->GetMaterial(), Desc);

	const TiledBlob_PromisePtr MaterialResult = std::static_pointer_cast<TiledBlob_Promise>(MaterialJob->InitResult(InRenderMaterial->GetName(), &Desc));
	MaterialJob->AddArg(WithUnbounded(ARG_BOOL(TiledMode, "TiledMode")));
	MaterialJob->SetTiled(TiledMode);
	

	InContext->Cycle->AddJob(InContext->TargetId, std::move(MaterialJob));

	if (TiledMode)
	{
		return MaterialResult;
	}
	else
	{
		MaterialResult->MakeSingleBlob();
		return T_SplitToTiles::Create(InContext->Cycle, InContext->TargetId, MaterialResult);
	}

}

void UTG_Expression_MaterialBase::LinkMaterialParameters(FTG_EvaluationContext* InContext, JobUPtr& InMaterialJob, const UMaterial* InMaterial, BufferDescriptor InDescriptor)
{
	if (InMaterialJob)
	{	
		TArray<FMaterialParameterInfo> OutParameterInfo;
		TArray<FGuid> OutParameterIds;

		InMaterial->GetAllScalarParameterInfo(OutParameterInfo, OutParameterIds);

		for (auto ParameterInfo : OutParameterInfo)
		{
			FTG_Var* Var = InContext->Inputs.GetVar(ParameterInfo.Name);
			const FTG_Argument* VarArgument = InContext->Inputs.GetVarArgument(ParameterInfo.Name);
			
			if (Var && !Var->IsEmpty())
			{
				FName CPPType = VarArgument->GetCPPTypeName();

				float ParamValue;
				
				if (CPPType == TEXT("int32"))
				{
					ParamValue = Var->GetAs<int32>();
				}
				else if (CPPType == TEXT("uint32"))
				{
					ParamValue = Var->GetAs<uint32>();
				}
				else
				{
					ParamValue = Var->GetAs<float>();
				}
				
				InMaterialJob->AddArg(ARG_FLOAT(ParamValue, TCHAR_TO_UTF8(*ParameterInfo.Name.ToString())));
			}
		}
		
		InMaterial->GetAllTextureParameterInfo(OutParameterInfo, OutParameterIds);

		for (auto ParameterInfo : OutParameterInfo)
		{
			FTG_Var* Var = InContext->Inputs.GetVar(ParameterInfo.Name);

			if (Var && !Var->IsEmpty())
			{
				FTG_Texture& ParamValue = Var->EditAs<FTG_Texture>();

				// there could be a case where the var has an empty blob, we don't need to do anything
				// we fallback to the default internal material parameter value for this input pin
				if (ParamValue.RasterBlob)
				{
					auto CombinedBlob = T_CombineTiledBlob::Create(InContext->Cycle, ParamValue.GetBufferDescriptor(), 0, ParamValue.RasterBlob);

					auto ArgBlob = ARG_BLOB(CombinedBlob, TCHAR_TO_UTF8(*ParameterInfo.Name.ToString()));
	
					ArgBlob->SetHandleTiles(TiledMode);
				
					InMaterialJob->AddArg(ArgBlob);
				}
			}
		}

		InMaterial->GetAllVectorParameterInfo(OutParameterInfo, OutParameterIds);

		for (auto ParameterInfo : OutParameterInfo)
		{
			FTG_Var* Var = InContext->Inputs.GetVar(ParameterInfo.Name);
			
			if (Var && !Var->IsEmpty())
			{
				auto ParamValue = Var->EditAs<FLinearColor>();
				InMaterialJob->AddArg(ARG_VECTOR(ParamValue, TCHAR_TO_UTF8(*ParameterInfo.Name.ToString())));
			}
		}
#if WITH_EDITORONLY_DATA
		InMaterial->GetAllStaticSwitchParameterInfo(OutParameterInfo, OutParameterIds);
		for (auto ParameterInfo : OutParameterInfo)
		{
			FTG_Var* Var = InContext->Inputs.GetVar(ParameterInfo.Name);
			
			if (Var && !Var->IsEmpty())
			{
				auto ParamValue = Var->EditAs<bool>();
				InMaterialJob->AddArg(ARG_INT(ParamValue, TCHAR_TO_UTF8(*ParameterInfo.Name.ToString())));
			}
		}
#endif
		//TODO: We need to add double Vector
		/*InMaterial->GetAllDoubleVectorParameterInfo(OutParameterInfo, OutParameterIds);

		for (auto ParameterInfo : OutParameterInfo)
		{
			FTG_Var* Var = InContext->Inputs.GetVar(ParameterInfo.Name);

			if (Var)
			{
				auto ParamValue = Var->EditAs<FVector4d>();
				Job->AddArg(ARG_VECTOR4(ParamValue, TCHAR_TO_UTF8(*ParameterInfo.Name.ToString())));
			}
		}*/
	}
}

void UTG_Expression_MaterialBase::AddSignatureParam(TArray<FMaterialParameterInfo> OutParameterInfo, FName CPPTypeName,  
                                                         FTG_Signature::FInit& SignatureInit, bool IsScalar /*= false*/) const
{
	for (FMaterialParameterInfo parameter : OutParameterInfo)
	{
		TMap<FName, FString> MetaDataMap;
#if WITH_EDITOR
		if(IsScalar)
		{
			// In case of scalar parameter, material has input range. We use that range to handle the input in the node.
			float MinValue, MaxValue;

			if(GetMaterial()->GetScalarParameterSliderMinMax(parameter.Name, MinValue, MaxValue))
			{
				MetaDataMap.Add("MinValue", FString::SanitizeFloat(MinValue));
				MetaDataMap.Add("MaxValue", FString::SanitizeFloat(MaxValue));
			}
		}
#endif		
		FTG_Argument Arg{ parameter.Name, CPPTypeName, { ETG_Access::In }, MetaDataMap };
		
		Arg.SetPersistentSelfVar(); // Set the material parameter persistent selfvar in order to save the state
		SignatureInit.Arguments.Add(Arg);
	}
}

FTG_SignaturePtr UTG_Expression_MaterialBase::BuildSignatureDynamically() const
{
	FTG_Signature::FInit SignatureInit = GetSignatureInitArgsFromClass();

	// append
	if (GetMaterial())
	{
		TArray<FMaterialParameterInfo> OutParameterInfo;
		TArray<FGuid> OutParameterIds;

		// The texture param are declared as FTG_Texture so they can be connected from the standard nodes
		GetMaterial()->GetAllTextureParameterInfo(OutParameterInfo, OutParameterIds);
		AddSignatureParam(OutParameterInfo, TEXT("FTG_Texture"), SignatureInit);
		
#if WITH_EDITORONLY_DATA
		GetMaterial()->GetAllStaticSwitchParameterInfo(OutParameterInfo, OutParameterIds);
		AddSignatureParam(OutParameterInfo, TEXT("bool"), SignatureInit);
#endif
		GetMaterial()->GetAllScalarParameterInfo(OutParameterInfo, OutParameterIds);
		AddSignatureParam(OutParameterInfo, TEXT("float"), SignatureInit, true);

		GetMaterial()->GetAllVectorParameterInfo(OutParameterInfo, OutParameterIds);
		AddSignatureParam(OutParameterInfo, TEXT("FLinearColor"), SignatureInit);

		// TODO: We need to add Double Vector4 support.
		/*Material->GetAllDoubleVectorParameterInfo(OutParameterInfo, OutParameterIds);
		AddSignatureParam(OutParameterInfo, TEXT("FVector4"), SignatureInit);*/

		
	}
	
	return MakeShared<FTG_Signature>(SignatureInit);
}


void UTG_Expression_MaterialBase::CopyVarGeneric(const FTG_Argument& Arg, FTG_Var* InVar, bool CopyVarToArg)
{
	if(MaterialInstance)
	{
		// Try to find the MatParam matching the arg
		if (Arg.CPPTypeName.IsEqual(TEXT("bool")))
		{
	#if WITH_EDITORONLY_DATA
			if (CopyVarToArg)
			{
				MaterialInstance->SetStaticSwitchParameterValueEditorOnly(Arg.GetName(), InVar->GetAs<bool>());
			} else
			{
				FMaterialParameterInfo ParameterInfo(Arg.GetName());
				FGuid OutParameterId;
				MaterialInstance->GetStaticSwitchParameterValue(ParameterInfo, InVar->EditAs<bool>(), OutParameterId);
			}
	#endif
		}
		else if (Arg.CPPTypeName.IsEqual(TEXT("float")))
		{
			if (CopyVarToArg)
			{
				MaterialInstance->SetScalarParameterValue(Arg.GetName(), InVar->GetAs<float>());
			}
			else
			{
				FMaterialParameterInfo ParameterInfo(Arg.GetName());
				MaterialInstance->GetScalarParameterValue(ParameterInfo, InVar->EditAs<float>());
			}
		}
		else if (Arg.CPPTypeName.IsEqual(TEXT("FLinearColor")))
		{
			if (CopyVarToArg)
			{
				MaterialInstance->SetVectorParameterValue(Arg.GetName(), InVar->GetAs<FLinearColor>());
			} else
			{
				FMaterialParameterInfo ParameterInfo(Arg.GetName());
				MaterialInstance->GetVectorParameterValue(ParameterInfo, InVar->EditAs<FLinearColor>());
			}
		}
		/*else if (Arg.CPPTypeName.IsEqual(TEXT("FVector4")))
		{
			if (CopyVarToArg)
			{
				MaterialInstance->SetDoubleVectorParameterValue(Arg.GetName(), InVar->GetAs<FVector4>());
			} else
			{
				FMaterialParameterInfo ParameterInfo(Arg.GetName());
				MaterialInstance->GetDoubleVectorParameterValue(ParameterInfo, InVar->EditAs<FVector4>());
			}
		}*/
		else if (Arg.CPPTypeName.IsEqual(TEXT("FTG_Texture")))
		{
			// TODO: Support that case later ?
			/*	if (CopyVarToArg)
				{
					MaterialInstance->SetScalarParameterValue(Arg.GetName(), InVar->GetAs<float>());
				} else
				{
					FMaterialParameterInfo ParameterInfo(Arg.GetName());
					MaterialInstance->GetScalarParameterValue(ParameterInfo, InVar->EditAs<float>());
				}*/
		}
	}
	
}