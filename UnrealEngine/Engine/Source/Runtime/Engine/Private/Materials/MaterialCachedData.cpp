// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialCachedData.h"
#include "MaterialCachedHLSLTree.h"
#include "Materials/MaterialExpressionBreakMaterialAttributes.h"
#include "Materials/MaterialExpressionShadingModel.h"
#include "Materials/MaterialExpressionReroute.h"
#include "Materials/MaterialExpressionSingleLayerWaterMaterialOutput.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionCustomOutput.h"
#include "Materials/MaterialExpressionDynamicParameter.h"
#include "Materials/MaterialExpressionFontSampleParameter.h"
#include "Materials/MaterialExpressionQualitySwitch.h"
#include "Materials/MaterialExpressionFeatureLevelSwitch.h"
#include "Materials/MaterialExpressionShadingPathSwitch.h"
#include "Materials/MaterialExpressionShaderStageSwitch.h"
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionPerInstanceCustomData.h"
#include "Materials/MaterialExpressionPerInstanceRandom.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionVertexInterpolator.h"
#include "Materials/MaterialExpressionSceneColor.h"
#include "Materials/MaterialExpressionRuntimeVirtualTextureOutput.h"
#include "Materials/MaterialExpressionLandscapeGrassOutput.h"
#include "Materials/MaterialExpressionCurveAtlasRowParameter.h"
#include "Materials/MaterialExpressionSetMaterialAttributes.h"
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionInstance.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialParameterCollection.h"
#include "MaterialHLSLTree.h"
#include "HLSLTree/HLSLTreeEmit.h"
#include "HLSLTree/HLSLTree.h"
#include "VT/RuntimeVirtualTexture.h"
#include "Engine/Font.h"
#include "LandscapeGrassType.h"
#include "ProfilingDebugging/LoadTimeTracker.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/CurveLinearColorAtlas.h"
#include "Misc/MemStack.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MaterialCachedData)

const FMaterialCachedParameterEntry FMaterialCachedParameterEntry::EmptyData{};
const FMaterialCachedExpressionData FMaterialCachedExpressionData::EmptyData{};
const FMaterialCachedExpressionEditorOnlyData FMaterialCachedExpressionEditorOnlyData::EmptyData{};

static_assert((uint32)(EMaterialProperty::MP_MAX)-1 <= (8 * sizeof(FMaterialCachedExpressionData::PropertyConnectedBitmask)), "PropertyConnectedBitmask cannot contain entire EMaterialProperty enumeration.");

FMaterialCachedExpressionData::FMaterialCachedExpressionData()
	: bHasMaterialLayers(false)
	, bHasRuntimeVirtualTextureOutput(false)
	, bHasSceneColor(false)
	, bHasPerInstanceCustomData(false)
	, bHasPerInstanceRandom(false)
	, bHasVertexInterpolator(false)
{
	QualityLevelsUsed.AddDefaulted(EMaterialQualityLevel::Num);
#if WITH_EDITORONLY_DATA
	EditorOnlyData = MakeShared<FMaterialCachedExpressionEditorOnlyData>();
#endif // WITH_EDITORONLY_DATA
}

void FMaterialCachedExpressionData::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(ReferencedTextures);
	Collector.AddReferencedObjects(GrassTypes);
	Collector.AddReferencedObjects(MaterialLayers.Layers);
	Collector.AddReferencedObjects(MaterialLayers.Blends);
	for (FMaterialFunctionInfo& FunctionInfo : FunctionInfos)
	{
		Collector.AddReferencedObject(FunctionInfo.Function);
	}
	for (FMaterialParameterCollectionInfo& ParameterCollectionInfo : ParameterCollectionInfos)
	{
		Collector.AddReferencedObject(ParameterCollectionInfo.ParameterCollection);
	}
}

void FMaterialCachedExpressionData::AppendReferencedFunctionIdsTo(TArray<FGuid>& Ids) const
{
	Ids.Reserve(Ids.Num() + FunctionInfos.Num());
	for (const FMaterialFunctionInfo& FunctionInfo : FunctionInfos)
	{
		Ids.AddUnique(FunctionInfo.StateId);
	}
}

void FMaterialCachedExpressionData::AppendReferencedParameterCollectionIdsTo(TArray<FGuid>& Ids) const
{
	Ids.Reserve(Ids.Num() + ParameterCollectionInfos.Num());
	for (const FMaterialParameterCollectionInfo& CollectionInfo : ParameterCollectionInfos)
	{
		Ids.AddUnique(CollectionInfo.StateId);
	}
}

#if WITH_EDITOR
static int32 TryAddParameter(FMaterialCachedExpressionData& CachedData,
	EMaterialParameterType Type,
	const FMaterialParameterInfo& ParameterInfo,
	const FMaterialCachedParameterEditorInfo& InEditorInfo)
{
	check(CachedData.EditorOnlyData);
	FMaterialCachedParameterEntry& Entry = CachedData.GetParameterTypeEntry(Type);
	FMaterialCachedParameterEditorEntry& EditorEntry = CachedData.EditorOnlyData->EditorEntries[(int32)Type];

	FSetElementId ElementId = Entry.ParameterInfoSet.FindId(ParameterInfo);
	int32 Index = INDEX_NONE;
	if (!ElementId.IsValidId())
	{
		ElementId = Entry.ParameterInfoSet.Add(ParameterInfo);
		Index = ElementId.AsInteger();
		EditorEntry.EditorInfo.Insert(InEditorInfo, Index);
		// should be valid as long as we don't ever remove elements from ParameterInfoSet
		check(Entry.ParameterInfoSet.Num() == EditorEntry.EditorInfo.Num());
		return Index;
	}

	// Update any editor values that haven't been set yet
	// TODO still need to do this??
	Index = ElementId.AsInteger();
	FMaterialCachedParameterEditorInfo& EditorInfo = EditorEntry.EditorInfo[Index];
	if (!EditorInfo.ExpressionGuid.IsValid())
	{
		EditorInfo.ExpressionGuid = InEditorInfo.ExpressionGuid;
	}
	if (EditorInfo.Description.IsEmpty())
	{
		EditorInfo.Description = InEditorInfo.Description;
	}
	if (EditorInfo.Group.IsNone())
	{
		EditorInfo.Group = InEditorInfo.Group;
		EditorInfo.SortPriority = InEditorInfo.SortPriority;
	}
	
	// Still return INDEX_NONE, to signify this parameter was already added (don't want to add it again)
	return INDEX_NONE;
}

void FMaterialCachedExpressionData::AddParameter(const FMaterialParameterInfo& ParameterInfo, const FMaterialParameterMetadata& ParameterMeta, UObject*& OutReferencedTexture)
{
	check(EditorOnlyData);
	int32 AssetIndex = INDEX_NONE;
	if (!ParameterMeta.AssetPath.IsEmpty())
	{
		AssetIndex = EditorOnlyData->AssetPaths.AddUnique(ParameterMeta.AssetPath);
	}

	const FMaterialCachedParameterEditorInfo EditorInfo(ParameterMeta.ExpressionGuid, ParameterMeta.Description, ParameterMeta.Group, ParameterMeta.SortPriority, AssetIndex);
	const int32 Index = TryAddParameter(*this, ParameterMeta.Value.Type, ParameterInfo, EditorInfo);
	if (Index != INDEX_NONE)
	{
		switch (ParameterMeta.Value.Type)
		{
		case EMaterialParameterType::Scalar:
			ScalarValues.Insert(ParameterMeta.Value.AsScalar(), Index);
			EditorOnlyData->ScalarMinMaxValues.Insert(FVector2D(ParameterMeta.ScalarMin, ParameterMeta.ScalarMax), Index);
			ScalarPrimitiveDataIndexValues.Insert(ParameterMeta.PrimitiveDataIndex, Index);
			if (ParameterMeta.bUsedAsAtlasPosition)
			{
				EditorOnlyData->ScalarCurveValues.Insert(ParameterMeta.ScalarCurve.Get(), Index);
				EditorOnlyData->ScalarCurveAtlasValues.Insert(ParameterMeta.ScalarAtlas.Get(), Index);
				OutReferencedTexture = ParameterMeta.ScalarAtlas.Get();
			}
			else
			{
				EditorOnlyData->ScalarCurveValues.Insert(nullptr, Index);
				EditorOnlyData->ScalarCurveAtlasValues.Insert(nullptr, Index);
			}
			break;
		case EMaterialParameterType::Vector:
			VectorValues.Insert(ParameterMeta.Value.AsLinearColor(), Index);
			EditorOnlyData->VectorChannelNameValues.Insert(ParameterMeta.ChannelNames, Index);
			EditorOnlyData->VectorUsedAsChannelMaskValues.Insert(ParameterMeta.bUsedAsChannelMask, Index);
			VectorPrimitiveDataIndexValues.Insert(ParameterMeta.PrimitiveDataIndex, Index);
			break;
		case EMaterialParameterType::DoubleVector:
			DoubleVectorValues.Insert(ParameterMeta.Value.AsVector4d(), Index);
			break;
		case EMaterialParameterType::Texture:
			TextureValues.Insert(ParameterMeta.Value.Texture, Index);
			EditorOnlyData->TextureChannelNameValues.Insert(ParameterMeta.ChannelNames, Index);
			OutReferencedTexture = ParameterMeta.Value.Texture;
			break;
		case EMaterialParameterType::Font:
			FontValues.Insert(ParameterMeta.Value.Font.Value, Index);
			FontPageValues.Insert(ParameterMeta.Value.Font.Page, Index);
			if (ParameterMeta.Value.Font.Value && ParameterMeta.Value.Font.Value->Textures.IsValidIndex(ParameterMeta.Value.Font.Page))
			{
				OutReferencedTexture = ParameterMeta.Value.Font.Value->Textures[ParameterMeta.Value.Font.Page];
			}
			break;
		case EMaterialParameterType::RuntimeVirtualTexture:
			RuntimeVirtualTextureValues.Insert(ParameterMeta.Value.RuntimeVirtualTexture, Index);
			OutReferencedTexture = ParameterMeta.Value.RuntimeVirtualTexture;
			break;
		case EMaterialParameterType::StaticSwitch:
			EditorOnlyData->StaticSwitchValues.Insert(ParameterMeta.Value.AsStaticSwitch(), Index);
			break;
		case EMaterialParameterType::StaticComponentMask:
			EditorOnlyData->StaticComponentMaskValues.Insert(ParameterMeta.Value.AsStaticComponentMask(), Index);
			break;
		default:
			checkNoEntry();
			break;
		}
	}
}

void FMaterialCachedExpressionData::UpdateForFunction(const FMaterialCachedExpressionContext& Context, UMaterialFunctionInterface* Function, EMaterialParameterAssociation Association, int32 ParameterIndex)
{
	if (!Function)
	{
		return;
	}

	// Update expressions for all dependent functions first, before processing the remaining expressions in this function
	// This is important so we add parameters in the proper order (parameter values are latched the first time a given parameter name is encountered)
	FMaterialCachedExpressionContext LocalContext(Context);
	LocalContext.CurrentFunction = Function;
	LocalContext.bUpdateFunctionExpressions = false; // we update functions explicitly
	
	FMaterialCachedExpressionData* Self = this;
	auto ProcessFunction = [Self, &LocalContext, Association, ParameterIndex](UMaterialFunctionInterface* InFunction) -> bool
	{
		Self->UpdateForExpressions(LocalContext, InFunction->GetExpressions(), Association, ParameterIndex);

		FMaterialFunctionInfo NewFunctionInfo;
		NewFunctionInfo.Function = InFunction;
		NewFunctionInfo.StateId = InFunction->StateId;
		Self->FunctionInfos.Add(NewFunctionInfo);

		return true;
	};
	Function->IterateDependentFunctions(ProcessFunction);

	ProcessFunction(Function);
}

void FMaterialCachedExpressionData::UpdateForLayerFunctions(const FMaterialCachedExpressionContext& Context, const FMaterialLayersFunctions& LayerFunctions)
{
	for (int32 LayerIndex = 0; LayerIndex < LayerFunctions.Layers.Num(); ++LayerIndex)
	{
		UpdateForFunction(Context, LayerFunctions.Layers[LayerIndex], LayerParameter, LayerIndex);
	}

	for (int32 BlendIndex = 0; BlendIndex < LayerFunctions.Blends.Num(); ++BlendIndex)
	{
		UpdateForFunction(Context, LayerFunctions.Blends[BlendIndex], BlendParameter, BlendIndex);
	}
}

void FMaterialCachedExpressionData::UpdateForExpressions(const FMaterialCachedExpressionContext& Context, TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions, EMaterialParameterAssociation Association, int32 ParameterIndex)
{
	check(EditorOnlyData);

	for (UMaterialExpression* Expression : Expressions)
	{
		if (!Expression)
		{
			continue;
		}

		UObject* ReferencedTexture = nullptr;

		FMaterialParameterMetadata ParameterMeta;
		if (Expression->GetParameterValue(ParameterMeta))
		{
			const FName ParameterName = Expression->GetParameterName();

			// If we're processing a function, give that a chance to override the parameter value
			if (Context.CurrentFunction)
			{
				FMaterialParameterMetadata OverrideParameterMeta;
				if (Context.CurrentFunction->GetParameterOverrideValue(ParameterMeta.Value.Type, ParameterName, OverrideParameterMeta))
				{
					ParameterMeta.Value = OverrideParameterMeta.Value;
					ParameterMeta.ExpressionGuid = OverrideParameterMeta.ExpressionGuid;
					ParameterMeta.bUsedAsAtlasPosition = OverrideParameterMeta.bUsedAsAtlasPosition;
					ParameterMeta.ScalarAtlas = OverrideParameterMeta.ScalarAtlas;
					ParameterMeta.ScalarCurve = OverrideParameterMeta.ScalarCurve;
				}
			}

			const FMaterialParameterInfo ParameterInfo(ParameterName, Association, ParameterIndex);
			AddParameter(ParameterInfo, ParameterMeta, ReferencedTexture);
		}

		// We first try to extract the referenced texture from the parameter value, that way we'll also get the proper texture in case value is overriden by a function instance
		const bool bCanReferenceTexture = Expression->CanReferenceTexture();
		if (!ReferencedTexture && bCanReferenceTexture)
		{
			ReferencedTexture = Expression->GetReferencedTexture();
		}

		if (ReferencedTexture)
		{
			checkf(bCanReferenceTexture, TEXT("CanReferenceTexture() returned false, but found a referenced texture"));
			ReferencedTextures.AddUnique(ReferencedTexture);
		}

		Expression->GetLandscapeLayerNames(EditorOnlyData->LandscapeLayerNames);

		if (UMaterialExpressionCollectionParameter* ExpressionCollectionParameter = Cast<UMaterialExpressionCollectionParameter>(Expression))
		{
			UMaterialParameterCollection* Collection = ExpressionCollectionParameter->Collection;
			if (Collection)
			{
				FMaterialParameterCollectionInfo NewInfo;
				NewInfo.ParameterCollection = Collection;
				NewInfo.StateId = Collection->StateId;
				ParameterCollectionInfos.AddUnique(NewInfo);
			}
		}
		else if (UMaterialExpressionDynamicParameter* ExpressionDynamicParameter = Cast< UMaterialExpressionDynamicParameter>(Expression))
		{
			DynamicParameterNames.Empty(ExpressionDynamicParameter->ParamNames.Num());
			for (const FString& Name : ExpressionDynamicParameter->ParamNames)
			{
				DynamicParameterNames.Add(*Name);
			}
		}
		else if (UMaterialExpressionLandscapeGrassOutput* ExpressionGrassOutput = Cast<UMaterialExpressionLandscapeGrassOutput>(Expression))
		{
			for (const auto& Type : ExpressionGrassOutput->GrassTypes)
			{
				GrassTypes.AddUnique(Type.GrassType);
			}
		}
		else if (UMaterialExpressionQualitySwitch* QualitySwitchNode = Cast<UMaterialExpressionQualitySwitch>(Expression))
		{
			const FExpressionInput DefaultInput = QualitySwitchNode->Default.GetTracedInput();

			for (int32 InputIndex = 0; InputIndex < EMaterialQualityLevel::Num; InputIndex++)
			{
				if (QualitySwitchNode->Inputs[InputIndex].IsConnected())
				{
					// We can ignore quality levels that are defined the same way as 'Default'
					// This avoids compiling a separate explicit quality level resource, that will end up exactly the same as the default resource
					const FExpressionInput Input = QualitySwitchNode->Inputs[InputIndex].GetTracedInput();
					if (Input.Expression != DefaultInput.Expression ||
						Input.OutputIndex != DefaultInput.OutputIndex)
					{
						QualityLevelsUsed[InputIndex] = true;
					}
				}
			}
		}
		else if (Expression->IsA(UMaterialExpressionRuntimeVirtualTextureOutput::StaticClass()))
		{
			bHasRuntimeVirtualTextureOutput = true;
		}
		else if (Expression->IsA(UMaterialExpressionSceneColor::StaticClass()))
		{
			bHasSceneColor = true;
		}
		else if (Expression->IsA(UMaterialExpressionPerInstanceRandom::StaticClass()))
		{
			bHasPerInstanceRandom = true;
		}
		else if (Expression->IsA(UMaterialExpressionPerInstanceCustomData::StaticClass()))
		{
			bHasPerInstanceCustomData = true;
		}
		else if (Expression->IsA(UMaterialExpressionPerInstanceCustomData3Vector::StaticClass()))
		{
			bHasPerInstanceCustomData = true;
		}
		else if (Expression->IsA(UMaterialExpressionVertexInterpolator::StaticClass()))
		{
			bHasVertexInterpolator = true;
		}
		else if (UMaterialExpressionMaterialAttributeLayers* LayersExpression = Cast<UMaterialExpressionMaterialAttributeLayers>(Expression))
		{
			checkf(Association == GlobalParameter, TEXT("UMaterialExpressionMaterialAttributeLayers can't be nested"));
			// Only a single layers expression is allowed/expected...creating additional layer expression will cause a compile error
			if (!bHasMaterialLayers)
			{
				const FMaterialLayersFunctions& Layers = Context.LayerOverrides ? *Context.LayerOverrides : LayersExpression->DefaultLayers;
				UpdateForLayerFunctions(Context, Layers);

				// TODO(?) - Layers for MIs are currently duplicated here and in FStaticParameterSet
				bHasMaterialLayers = true;
				MaterialLayers = Layers.GetRuntime();
				EditorOnlyData->MaterialLayers = Layers.EditorOnly;
				FMaterialLayersFunctions::Validate(MaterialLayers, EditorOnlyData->MaterialLayers);
				LayersExpression->RebuildLayerGraph(false);
			}
		}
		else if (UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
		{
			if (Context.bUpdateFunctionExpressions)
			{
				UpdateForFunction(Context, FunctionCall->MaterialFunction, GlobalParameter, -1);

				// Update the function call node, so it can relink inputs and outputs as needed
				// Update even if MaterialFunctionNode->MaterialFunction is NULL, because we need to remove the invalid inputs in that case
				FunctionCall->UpdateFromFunctionResource();
			}
		}
		else if (UMaterialExpressionSetMaterialAttributes* SetMatAttributes = Cast<UMaterialExpressionSetMaterialAttributes>(Expression))
		{
			for (int32 PinIndex = 0; PinIndex < SetMatAttributes->AttributeSetTypes.Num(); ++PinIndex)
			{
				// For this material attribute pin do we have something connected?
				const FGuid& Guid = SetMatAttributes->AttributeSetTypes[PinIndex];
				const FExpressionInput& AttributeInput = SetMatAttributes->Inputs[PinIndex + 1];
				const EMaterialProperty MaterialProperty = FMaterialAttributeDefinitionMap::GetProperty(Guid);
				if (AttributeInput.Expression)
				{
					SetPropertyConnected(MaterialProperty);
				}
			}
		}
		else if (UMaterialExpressionMakeMaterialAttributes* MakeMatAttributes = Cast<UMaterialExpressionMakeMaterialAttributes>(Expression))
		{
			auto SetMatAttributeConditionally = [&](EMaterialProperty InMaterialProperty, bool InIsConnected)
			{
				if (InIsConnected)
				{
					SetPropertyConnected(InMaterialProperty);
				}
			};

			SetMatAttributeConditionally(EMaterialProperty::MP_BaseColor, MakeMatAttributes->BaseColor.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_Metallic, MakeMatAttributes->Metallic.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_Specular, MakeMatAttributes->Specular.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_Roughness, MakeMatAttributes->Roughness.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_Anisotropy, MakeMatAttributes->Anisotropy.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_EmissiveColor, MakeMatAttributes->EmissiveColor.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_Opacity, MakeMatAttributes->Opacity.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_OpacityMask, MakeMatAttributes->OpacityMask.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_Normal, MakeMatAttributes->Normal.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_Tangent, MakeMatAttributes->Tangent.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_WorldPositionOffset, MakeMatAttributes->WorldPositionOffset.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_SubsurfaceColor, MakeMatAttributes->SubsurfaceColor.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_CustomData0, MakeMatAttributes->ClearCoat.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_CustomData1, MakeMatAttributes->ClearCoatRoughness.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_AmbientOcclusion, MakeMatAttributes->AmbientOcclusion.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_Refraction, MakeMatAttributes->Refraction.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_CustomizedUVs0, MakeMatAttributes->CustomizedUVs[0].IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_CustomizedUVs1, MakeMatAttributes->CustomizedUVs[1].IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_CustomizedUVs2, MakeMatAttributes->CustomizedUVs[2].IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_CustomizedUVs3, MakeMatAttributes->CustomizedUVs[3].IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_CustomizedUVs4, MakeMatAttributes->CustomizedUVs[4].IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_CustomizedUVs5, MakeMatAttributes->CustomizedUVs[5].IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_CustomizedUVs6, MakeMatAttributes->CustomizedUVs[6].IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_CustomizedUVs7, MakeMatAttributes->CustomizedUVs[7].IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_PixelDepthOffset, MakeMatAttributes->PixelDepthOffset.IsConnected());
			SetMatAttributeConditionally(EMaterialProperty::MP_ShadingModel, MakeMatAttributes->ShadingModel.IsConnected());
		}
	}
}

namespace Private
{
void PrepareHLSLTree(UE::HLSLTree::FEmitContext& EmitContext,
	const FMaterialCachedHLSLTree& CachedTree,
	FMaterialCachedExpressionData& CachedData,
	EShaderFrequency ShaderFrequency)
{
	using namespace UE::HLSLTree;
	using namespace UE::Shader;

	EmitContext.ShaderFrequency = ShaderFrequency;
	EmitContext.bUseAnalyticDerivatives = true; // We want to consider expressions used for analytic derivatives
	EmitContext.bMarkLiveValues = false;
	FEmitScope* EmitResultScope = EmitContext.PrepareScope(CachedTree.GetResultScope());

	FRequestedType RequestedAttributesType(CachedTree.GetMaterialAttributesType(), false);
	CachedTree.SetRequestedFields(ShaderFrequency, RequestedAttributesType);

	const FPreparedType& ResultType = EmitContext.PrepareExpression(CachedTree.GetResultExpression(), *EmitResultScope, RequestedAttributesType);
	if (!ResultType.IsVoid())
	{
		EmitContext.bMarkLiveValues = true;
		EmitContext.PrepareExpression(CachedTree.GetResultExpression(), *EmitResultScope, RequestedAttributesType);

		const TArray<FGuid>& OrderedVisibleAttributes = FMaterialAttributeDefinitionMap::GetOrderedVisibleAttributeList();
		for (const FGuid& AttributeID : OrderedVisibleAttributes)
		{
			if (FMaterialAttributeDefinitionMap::GetShaderFrequency(AttributeID) == ShaderFrequency)
			{
				const EMaterialProperty Property = FMaterialAttributeDefinitionMap::GetProperty(AttributeID);
				if (CachedTree.IsAttributeUsed(EmitContext, *EmitResultScope, ResultType, Property))
				{
					CachedData.SetPropertyConnected(Property);
				}
			}
		}
	}
}

}

void FMaterialCachedExpressionData::UpdateForCachedHLSLTree(const FMaterialCachedHLSLTree& CachedTree, const FStaticParameterSet* StaticParameters)
{
	using namespace UE::HLSLTree;

	// We ignore errors here, errors will be captured when we actually emit HLSL from the tree
	FNullErrorHandler NullErrorHandler;

	FMemStackBase Allocator;
	FEmitContext EmitContext(Allocator, FTargetParameters(), NullErrorHandler, CachedTree.GetTypeRegistry());

	Material::FEmitData& EmitMaterialData = EmitContext.AcquireData<Material::FEmitData>();
	EmitMaterialData.CachedExpressionData = this;
	EmitMaterialData.StaticParameters = StaticParameters;

	::Private::PrepareHLSLTree(EmitContext, CachedTree, *this, SF_Pixel);
	::Private::PrepareHLSLTree(EmitContext, CachedTree, *this, SF_Vertex);
}

void FMaterialCachedExpressionData::Validate()
{
	if (EditorOnlyData)
	{
		for (int32 TypeIndex = 0; TypeIndex < NumMaterialParameterTypes; ++TypeIndex)
		{
			const FMaterialCachedParameterEditorEntry& EditorEntry = EditorOnlyData->EditorEntries[TypeIndex];
			const FMaterialCachedParameterEntry& Entry = GetParameterTypeEntry((EMaterialParameterType)TypeIndex);
			check(EditorEntry.EditorInfo.Num() == Entry.ParameterInfoSet.Num());
		}
		FMaterialLayersFunctions::Validate(MaterialLayers, EditorOnlyData->MaterialLayers);
	}
}

#endif // WITH_EDITOR

int32 FMaterialCachedExpressionData::FindParameterIndex(EMaterialParameterType Type, const FMemoryImageMaterialParameterInfo& ParameterInfo) const
{
	const FMaterialCachedParameterEntry& Entry = GetParameterTypeEntry(Type);
	const FSetElementId ElementId = Entry.ParameterInfoSet.FindId(FMaterialParameterInfo(ParameterInfo));
	return ElementId.AsInteger();
}

void FMaterialCachedExpressionData::GetParameterValueByIndex(EMaterialParameterType Type, int32 ParameterIndex, FMaterialParameterMetadata& OutResult) const
{
#if WITH_EDITORONLY_DATA
	bool bIsEditorOnlyDataStripped = true;
	if (EditorOnlyData)
	{
		const FMaterialCachedParameterEditorEntry& EditorEntry = EditorOnlyData->EditorEntries[(int32)Type];
		bIsEditorOnlyDataStripped = EditorEntry.EditorInfo.Num() == 0;
		if (!bIsEditorOnlyDataStripped)
		{
			const FMaterialCachedParameterEditorInfo& EditorInfo = EditorEntry.EditorInfo[ParameterIndex];
			OutResult.ExpressionGuid = EditorInfo.ExpressionGuid;
			OutResult.Description = EditorInfo.Description;
			OutResult.Group = EditorInfo.Group;
			OutResult.SortPriority = EditorInfo.SortPriority;
			if (EditorInfo.AssetIndex != INDEX_NONE)
			{
				OutResult.AssetPath = EditorOnlyData->AssetPaths[EditorInfo.AssetIndex];
			}
		}
	}
#endif // WITH_EDITORONLY_DATA

	switch (Type)
	{
	case EMaterialParameterType::Scalar:
		OutResult.Value = ScalarValues[ParameterIndex];
		OutResult.PrimitiveDataIndex = ScalarPrimitiveDataIndexValues[ParameterIndex];
#if WITH_EDITORONLY_DATA
		if (EditorOnlyData && !bIsEditorOnlyDataStripped)
		{
			OutResult.ScalarMin = EditorOnlyData->ScalarMinMaxValues[ParameterIndex].X;
			OutResult.ScalarMax = EditorOnlyData->ScalarMinMaxValues[ParameterIndex].Y;
			{
				const TSoftObjectPtr<UCurveLinearColor>& Curve = EditorOnlyData->ScalarCurveValues[ParameterIndex];
				const TSoftObjectPtr<UCurveLinearColorAtlas>& Atlas = EditorOnlyData->ScalarCurveAtlasValues[ParameterIndex];
				if (!Curve.IsNull() && !Atlas.IsNull())
				{
					OutResult.ScalarCurve = Curve;
					OutResult.ScalarAtlas = Atlas;
					OutResult.bUsedAsAtlasPosition = true;
				}
			}
		}
#endif // WITH_EDITORONLY_DATA
		break;
	case EMaterialParameterType::Vector:
		OutResult.Value = VectorValues[ParameterIndex];
		OutResult.PrimitiveDataIndex = VectorPrimitiveDataIndexValues[ParameterIndex];
#if  WITH_EDITORONLY_DATA
		if (EditorOnlyData && !bIsEditorOnlyDataStripped)
		{
			OutResult.ChannelNames = EditorOnlyData->VectorChannelNameValues[ParameterIndex];
			OutResult.bUsedAsChannelMask = EditorOnlyData->VectorUsedAsChannelMaskValues[ParameterIndex];
		}
#endif // WITH_EDITORONLY_DATA
		break;
	case EMaterialParameterType::DoubleVector:
		OutResult.Value = DoubleVectorValues[ParameterIndex];
		break;
	case EMaterialParameterType::Texture:
		OutResult.Value = TextureValues[ParameterIndex].LoadSynchronous();
#if WITH_EDITORONLY_DATA
		if (EditorOnlyData && !bIsEditorOnlyDataStripped)
		{
			OutResult.ChannelNames = EditorOnlyData->TextureChannelNameValues[ParameterIndex];
		}
#endif // WITH_EDITORONLY_DATA
		break;
	case EMaterialParameterType::RuntimeVirtualTexture:
		OutResult.Value = RuntimeVirtualTextureValues[ParameterIndex].LoadSynchronous();
		break;
	case EMaterialParameterType::Font:
		OutResult.Value = FMaterialParameterValue(FontValues[ParameterIndex].LoadSynchronous(), FontPageValues[ParameterIndex]);
		break;
#if WITH_EDITORONLY_DATA
	case EMaterialParameterType::StaticSwitch:
		if (EditorOnlyData && !bIsEditorOnlyDataStripped)
		{
			OutResult.Value = EditorOnlyData->StaticSwitchValues[ParameterIndex];
		}
		break;
	case EMaterialParameterType::StaticComponentMask:
		if (EditorOnlyData && !bIsEditorOnlyDataStripped)
		{
			OutResult.Value = EditorOnlyData->StaticComponentMaskValues[ParameterIndex];
		}
		break;
#endif // WITH_EDITORONLY_DATA
	default:
		checkNoEntry();
		break;
	}
}

bool FMaterialCachedExpressionData::GetParameterValue(EMaterialParameterType Type, const FMemoryImageMaterialParameterInfo& ParameterInfo, FMaterialParameterMetadata& OutResult) const
{
	const int32 Index = FindParameterIndex(Type, ParameterInfo);
	if (Index != INDEX_NONE)
	{
		GetParameterValueByIndex(Type, Index, OutResult);
		return true;
	}

	return false;
}

const FGuid& FMaterialCachedExpressionData::GetExpressionGuid(EMaterialParameterType Type, int32 Index) const
{
#if WITH_EDITORONLY_DATA
	if (EditorOnlyData)
	{
		// cooked materials can strip out expression guids
		if (EditorOnlyData->EditorEntries[(int32)Type].EditorInfo.Num() != 0)
		{
			return EditorOnlyData->EditorEntries[(int32)Type].EditorInfo[Index].ExpressionGuid;
		}
	}
#endif // WITH_EDITORONLY_DATA
	static const FGuid EmptyGuid;
	return EmptyGuid;
}

void FMaterialCachedExpressionData::GetAllParametersOfType(EMaterialParameterType Type, TMap<FMaterialParameterInfo, FMaterialParameterMetadata>& OutParameters) const
{
	const FMaterialCachedParameterEntry& Entry = GetParameterTypeEntry(Type);
	const int32 NumParameters = Entry.ParameterInfoSet.Num();
	OutParameters.Reserve(OutParameters.Num() + NumParameters);

	for (int32 ParameterIndex = 0; ParameterIndex < NumParameters; ++ParameterIndex)
	{
		const FMaterialParameterInfo& ParameterInfo = Entry.ParameterInfoSet[FSetElementId::FromInteger(ParameterIndex)];
		FMaterialParameterMetadata& Result = OutParameters.Emplace(ParameterInfo);
		GetParameterValueByIndex(Type, ParameterIndex, Result);
	}
}

void FMaterialCachedExpressionData::GetAllParameterInfoOfType(EMaterialParameterType Type, TArray<FMaterialParameterInfo>& OutParameterInfo, TArray<FGuid>& OutParameterIds) const
{
	const FMaterialCachedParameterEntry& Entry = GetParameterTypeEntry(Type);
	const int32 NumParameters = Entry.ParameterInfoSet.Num();
	OutParameterInfo.Reserve(OutParameterInfo.Num() + NumParameters);
	OutParameterIds.Reserve(OutParameterIds.Num() + NumParameters);

	for (TSet<FMaterialParameterInfo>::TConstIterator It(Entry.ParameterInfoSet); It; ++It)
	{
		const int32 ParameterIndex = It.GetId().AsInteger();
		OutParameterInfo.Add(*It);
		OutParameterIds.Add(GetExpressionGuid(Type, ParameterIndex));
	}
}

void FMaterialCachedExpressionData::GetAllGlobalParametersOfType(EMaterialParameterType Type, TMap<FMaterialParameterInfo, FMaterialParameterMetadata>& OutParameters) const
{
	const FMaterialCachedParameterEntry& Entry = GetParameterTypeEntry(Type);
	const int32 NumParameters = Entry.ParameterInfoSet.Num();
	OutParameters.Reserve(OutParameters.Num() + NumParameters);

	for (int32 ParameterIndex = 0; ParameterIndex < NumParameters; ++ParameterIndex)
	{
		const FMaterialParameterInfo& ParameterInfo = Entry.ParameterInfoSet[FSetElementId::FromInteger(ParameterIndex)];
		if (ParameterInfo.Association == GlobalParameter)
		{
			FMaterialParameterMetadata& Meta = OutParameters.FindOrAdd(ParameterInfo);
			if (Meta.Value.Type == EMaterialParameterType::None)
			{
				GetParameterValueByIndex(Type, ParameterIndex, Meta);
			}
		}
	}
}

void FMaterialCachedExpressionData::GetAllGlobalParameterInfoOfType(EMaterialParameterType Type, TArray<FMaterialParameterInfo>& OutParameterInfo, TArray<FGuid>& OutParameterIds) const
{
	const FMaterialCachedParameterEntry& Entry = GetParameterTypeEntry(Type);
	const int32 NumParameters = Entry.ParameterInfoSet.Num();
	OutParameterInfo.Reserve(OutParameterInfo.Num() + NumParameters);
	OutParameterIds.Reserve(OutParameterIds.Num() + NumParameters);

	for (TSet<FMaterialParameterInfo>::TConstIterator It(Entry.ParameterInfoSet); It; ++It)
	{
		const FMaterialParameterInfo& ParameterInfo = *It;
		if (ParameterInfo.Association == GlobalParameter)
		{
			const int32 ParameterIndex = It.GetId().AsInteger();
			OutParameterInfo.Add(*It);
			OutParameterIds.Add(GetExpressionGuid(Type, ParameterIndex));
		}
	}
}


