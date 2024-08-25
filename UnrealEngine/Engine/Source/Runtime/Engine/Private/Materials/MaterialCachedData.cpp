// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialCachedData.h"
#include "MaterialCachedHLSLTree.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionDynamicParameter.h"
#include "Materials/MaterialExpressionQualitySwitch.h"
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Materials/MaterialExpressionPerInstanceCustomData.h"
#include "Materials/MaterialExpressionPerInstanceRandom.h"
#include "Materials/MaterialExpressionVertexInterpolator.h"
#include "Materials/MaterialExpressionSceneColor.h"
#include "Materials/MaterialExpressionRuntimeVirtualTextureOutput.h"
#include "Materials/MaterialExpressionLandscapeGrassOutput.h"
#include "Materials/MaterialExpressionSetMaterialAttributes.h"
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Materials/MaterialExpressionMaterialAttributeLayers.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionStaticSwitch.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionStaticBool.h"
#include "Materials/MaterialExpressionLandscapeGrassOutput.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "MaterialHLSLTree.h"
#include "HLSLTree/HLSLTreeEmit.h"
#include "VT/RuntimeVirtualTexture.h"
#include "SparseVolumeTexture/SparseVolumeTexture.h"
#include "Engine/Font.h"
#include "LandscapeGrassType.h"
#include "Logging/LogScopedVerbosityOverride.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/CurveLinearColorAtlas.h"
#include "UObject/UE5MainStreamObjectVersion.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MaterialCachedData)

const FMaterialCachedParameterEntry FMaterialCachedParameterEntry::EmptyData{};
const FMaterialCachedExpressionData FMaterialCachedExpressionData::EmptyData{};
const FMaterialCachedExpressionEditorOnlyData FMaterialCachedExpressionEditorOnlyData::EmptyData{};

static_assert((uint64)(EMaterialProperty::MP_MaterialAttributes)-1 < (8 * sizeof(FMaterialCachedExpressionData::PropertyConnectedMask)), "PropertyConnectedMask cannot contain entire EMaterialProperty enumeration.");

static bool GExperimentalMaterialCachedDataAnalysisEnabled = false;
static FAutoConsoleVariableRef CVarExperimentalMaterialCachedDataAnalysisEnabled(TEXT("r.Material.ExperimentalMaterialCachedDataAnalysisEnabled"), GExperimentalMaterialCachedDataAnalysisEnabled, TEXT("Enables material cached data experimental graph based analysis"));

FMaterialCachedExpressionData::FMaterialCachedExpressionData()
	: FunctionInfosStateCRC(0xffffffff)
	, bHasMaterialLayers(false)
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
	Collector.AddStableReferenceArray(&ReferencedTextures);
	Collector.AddStableReferenceArray(&GrassTypes);
	Collector.AddStableReferenceArray(&MaterialLayers.Layers);
	Collector.AddStableReferenceArray(&MaterialLayers.Blends);
	for (FMaterialFunctionInfo& FunctionInfo : FunctionInfos)
	{
		Collector.AddStableReference(&FunctionInfo.Function);
	}
	for (FMaterialParameterCollectionInfo& ParameterCollectionInfo : ParameterCollectionInfos)
	{
		Collector.AddStableReference(&ParameterCollectionInfo.ParameterCollection);
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
static bool TryAddParameter(FMaterialCachedExpressionData& CachedData,
	EMaterialParameterType Type,
	const FMaterialParameterInfo& ParameterInfo,
	const FMaterialCachedParameterEditorInfo& InEditorInfo,
	int32& OutIndex)
{
	check(CachedData.EditorOnlyData);
	FMaterialCachedParameterEntry& Entry = CachedData.GetParameterTypeEntry(Type);
	FMaterialCachedParameterEditorEntry& EditorEntry = CachedData.EditorOnlyData->EditorEntries[(int32)Type];

	FSetElementId ElementId = Entry.ParameterInfoSet.FindId(ParameterInfo);
	OutIndex = INDEX_NONE;
	if (!ElementId.IsValidId())
	{
		ElementId = Entry.ParameterInfoSet.Add(ParameterInfo);
		OutIndex = ElementId.AsInteger();
		EditorEntry.EditorInfo.Insert(InEditorInfo, OutIndex);
		// should be valid as long as we don't ever remove elements from ParameterInfoSet
		check(Entry.ParameterInfoSet.Num() == EditorEntry.EditorInfo.Num());
		return true;
	}

	// Update any editor values that haven't been set yet
	// TODO still need to do this??
	OutIndex = ElementId.AsInteger();
	FMaterialCachedParameterEditorInfo& EditorInfo = EditorEntry.EditorInfo[OutIndex];
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
	
	// Still return false, to signify this parameter was already added (don't want to add it again)
	return false;
}

bool FMaterialCachedExpressionData::AddParameter(const FMaterialParameterInfo& ParameterInfo, const FMaterialParameterMetadata& ParameterMeta, UObject*& OutReferencedTexture)
{
	check(EditorOnlyData);
	int32 AssetIndex = INDEX_NONE;
	if (!ParameterMeta.AssetPath.IsEmpty())
	{
		AssetIndex = EditorOnlyData->AssetPaths.AddUnique(ParameterMeta.AssetPath);
	}

	const FMaterialCachedParameterEditorInfo EditorInfo(ParameterMeta.ExpressionGuid, ParameterMeta.Description, ParameterMeta.Group, ParameterMeta.SortPriority, AssetIndex);
	int32 Index = INDEX_NONE;
	if (TryAddParameter(*this, ParameterMeta.Value.Type, ParameterInfo, EditorInfo, Index))
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

		case EMaterialParameterType::SparseVolumeTexture:
			SparseVolumeTextureValues.Insert(ParameterMeta.Value.SparseVolumeTexture, Index);
			OutReferencedTexture = ParameterMeta.Value.SparseVolumeTexture;
			break;

		case EMaterialParameterType::StaticSwitch:
			StaticSwitchValues.Insert(ParameterMeta.Value.AsStaticSwitch(), Index);
			DynamicSwitchValues.Insert(ParameterMeta.bDynamicSwitchParameter, Index);
			break;

		case EMaterialParameterType::StaticComponentMask:
			EditorOnlyData->StaticComponentMaskValues.Insert(ParameterMeta.Value.AsStaticComponentMask(), Index);
			break;

		default:
			checkNoEntry();
			break;
		}
	}
	else
	{
		bool bSameValue;
		switch (ParameterMeta.Value.Type)
		{
		case EMaterialParameterType::Scalar:
			bSameValue = ScalarValues[Index] == ParameterMeta.Value.AsScalar();
			break;

		case EMaterialParameterType::Vector:
			bSameValue = VectorValues[Index] == ParameterMeta.Value.AsLinearColor();
			break;

		case EMaterialParameterType::DoubleVector:
			bSameValue = DoubleVectorValues[Index] == ParameterMeta.Value.AsVector4d();
			break;

		case EMaterialParameterType::Texture:
			bSameValue = TextureValues[Index] == ParameterMeta.Value.Texture;
			break;

		case EMaterialParameterType::Font:
			bSameValue = FontValues[Index] == ParameterMeta.Value.Font.Value && FontPageValues[Index] == ParameterMeta.Value.Font.Page;
			break;

		case EMaterialParameterType::RuntimeVirtualTexture:
			bSameValue = RuntimeVirtualTextureValues[Index] == ParameterMeta.Value.RuntimeVirtualTexture;
			break;

		case EMaterialParameterType::SparseVolumeTexture:
			bSameValue = SparseVolumeTextureValues[Index] == ParameterMeta.Value.SparseVolumeTexture;
			break;

		default:
			bSameValue = true;
			break;
		}

		return bSameValue;
	}

	return true;
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
		Self->FunctionInfosStateCRC = FCrc::TypeCrc32(InFunction->StateId, Self->FunctionInfosStateCRC);

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

			// Try add the parameter. If this fails, the parameter is being added twice with different values. Report it as error.
			if (!AddParameter(ParameterInfo, ParameterMeta, ReferencedTexture))
			{
				DuplicateParameterErrors.AddUnique({ Expression, ParameterName });
			}
		}

		// We first try to extract the referenced texture from the parameter value, that way we'll also get the proper texture in case value is overriden by a function instance
		const bool bCanReferenceTexture = Expression->CanReferenceTexture();
		if (!ReferencedTexture && bCanReferenceTexture)
		{
			const UMaterialExpression::ReferencedTextureArray ExpressionReferencedTextures = Expression->GetReferencedTextures();
			for (UObject* ExpressionReferencedTexture : ExpressionReferencedTextures)
			{
				ReferencedTextures.AddUnique(ExpressionReferencedTexture);
			}
		}
		else if (ReferencedTexture)
		{
			ReferencedTextures.AddUnique(ReferencedTexture);
		}

		Expression->GetLandscapeLayerNames(EditorOnlyData->LandscapeLayerNames);

		Expression->GetIncludeFilePaths(EditorOnlyData->ExpressionIncludeFilePaths);

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
			SetMatAttributeConditionally(EMaterialProperty::MP_Displacement, MakeMatAttributes->Displacement.IsConnected());
		}
	}
}

/**
 * A helper struct used to crawl through the graph starting from material outputs in order to detect which material parameters are
 * written to. This information is used to mark the connected properties in a MaterialCachedData instance.
 * The graph is explored starting from the material outputs and walking backwards to its inputs. Whenever a static switch node is
 * encountered, the algorithm finds out the value of the associated static parameter and continues exploration only along the
 * activated subgraph. Because of this, unused subgraphs aren't visited so that any potential property writes in them don't interfere
 * with the final result.
 */
struct FMaterialConnectedPropertiesAnalyzer
{
	/** The target FMaterialCachedExpressionData to set connected parameters to */
	FMaterialCachedExpressionData&	CachedExpressionData;
	
	/** The current expression (if inside one, otherwise nullptr) */
	UMaterialFunctionInterface*		CurrentFunction;
	
	/** The list of "fringe" material expressions that are yet to be explored */
	TArray<UMaterialExpression*>	UnexploredExpressions;
	
	/** The set of visited expressions, used to avoid visiting the same expressions twice */
	TSet<UMaterialExpression*>		VisitedExpressions;

	/** Constructor */
	FMaterialConnectedPropertiesAnalyzer(FMaterialCachedExpressionData& CachedExpressionData, UMaterialFunctionInterface* CurrentFunction = nullptr)
		: CachedExpressionData(CachedExpressionData)
		, CurrentFunction(CurrentFunction)
	{
	}

	/** Pops an expression from the unexplored list, returning null if list is empty */
	UMaterialExpression* PopUnvisitedExpression()
	{
		if (UnexploredExpressions.IsEmpty())
		{
			return nullptr;
		}
		UMaterialExpression* Expression = UnexploredExpressions.Last();
		UnexploredExpressions.Pop();
		return Expression;
	}

	/** Pushes an expression to the unexplored list if has not yet been explored */
	void PushUnexploredExpression(UMaterialExpression* Expression)
	{
		if (!Expression || VisitedExpressions.Contains(Expression))
		{
			return;
		}
		UnexploredExpressions.Add(Expression);
		VisitedExpressions.Add(Expression);
	}

	/** Visits all expressions in the UnexploredExpressions list */
	void VisitUnexploredExpressions(EMaterialParameterAssociation Association, int32 ParameterIndex)
	{
		for (;;)
		{
			// Pop the current expression.
			UMaterialExpression* Expression = PopUnvisitedExpression();
			if (!Expression)
			{
				break;
			}

			// By default explore all input expressions. If static switch nodes are encountered, they
			// may be able to continue exploration only along the active input and set this flag to false.
			bool bVisitAllInputExpressions = true;

			// Explore the current expression
			if (UMaterialExpressionCollectionParameter* ExpressionCollectionParameter = Cast<UMaterialExpressionCollectionParameter>(Expression))
			{
				UMaterialParameterCollection* Collection = ExpressionCollectionParameter->Collection;
				if (Collection)
				{
					FMaterialParameterCollectionInfo NewInfo;
					NewInfo.ParameterCollection = Collection;
					NewInfo.StateId = Collection->StateId;
					CachedExpressionData.ParameterCollectionInfos.AddUnique(NewInfo);
				}
			}
			else if (UMaterialExpressionDynamicParameter* ExpressionDynamicParameter = Cast< UMaterialExpressionDynamicParameter>(Expression))
			{
				CachedExpressionData.DynamicParameterNames.Empty(ExpressionDynamicParameter->ParamNames.Num());
				for (const FString& Name : ExpressionDynamicParameter->ParamNames)
				{
					CachedExpressionData.DynamicParameterNames.Add(*Name);
				}
			}
			else if (UMaterialExpressionLandscapeGrassOutput* ExpressionGrassOutput = Cast<UMaterialExpressionLandscapeGrassOutput>(Expression))
			{
				for (const auto& Type : ExpressionGrassOutput->GrassTypes)
				{
					CachedExpressionData.GrassTypes.AddUnique(Type.GrassType);
				}
			}
			else if (Expression->IsA(UMaterialExpressionRuntimeVirtualTextureOutput::StaticClass()))
			{
				CachedExpressionData.bHasRuntimeVirtualTextureOutput = true;
			}
			else if (Expression->IsA(UMaterialExpressionSceneColor::StaticClass()))
			{
				CachedExpressionData.bHasSceneColor = true;
			}
			else if (Expression->IsA(UMaterialExpressionPerInstanceRandom::StaticClass()))
			{
				CachedExpressionData.bHasPerInstanceRandom = true;
			}
			else if (Expression->IsA(UMaterialExpressionPerInstanceCustomData::StaticClass()))
			{
				CachedExpressionData.bHasPerInstanceCustomData = true;
			}
			else if (Expression->IsA(UMaterialExpressionPerInstanceCustomData3Vector::StaticClass()))
			{
				CachedExpressionData.bHasPerInstanceCustomData = true;
			}
			else if (Expression->IsA(UMaterialExpressionVertexInterpolator::StaticClass()))
			{
				CachedExpressionData.bHasVertexInterpolator = true;
			}
			else if (UMaterialExpressionQualitySwitch* ExpressionQualitySwitch = Cast<UMaterialExpressionQualitySwitch>(Expression))
			{
				const FExpressionInput DefaultInput = ExpressionQualitySwitch->Default.GetTracedInput();
				for (int32 InputIndex = 0; InputIndex < EMaterialQualityLevel::Num; InputIndex++)
				{
					if (ExpressionQualitySwitch->Inputs[InputIndex].IsConnected())
					{
						// We can ignore quality levels that are defined the same way as 'Default'
						// This avoids compiling a separate explicit quality level resource, that will end up exactly the same as the default resource
						const FExpressionInput Input = ExpressionQualitySwitch->Inputs[InputIndex].GetTracedInput();
						if (Input.Expression != DefaultInput.Expression || Input.OutputIndex != DefaultInput.OutputIndex)
						{
							CachedExpressionData.QualityLevelsUsed[InputIndex] = true;
						}
					}
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
						CachedExpressionData.SetPropertyConnected(MaterialProperty);
					}
				}
			}
			else if (UMaterialExpressionMakeMaterialAttributes* MakeMatAttributes = Cast<UMaterialExpressionMakeMaterialAttributes>(Expression))
			{
				auto SetMatAttributeConditionally = [&](EMaterialProperty InMaterialProperty, bool InIsConnected)
				{
					if (InIsConnected)
					{
						CachedExpressionData.SetPropertyConnected(InMaterialProperty);
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
				SetMatAttributeConditionally(EMaterialProperty::MP_Displacement, MakeMatAttributes->Displacement.IsConnected());
			}
			else if (UMaterialExpressionMaterialAttributeLayers* LayersExpression = Cast<UMaterialExpressionMaterialAttributeLayers>(Expression))
			{
				const FMaterialLayersFunctions& LayerFunctions = LayersExpression->DefaultLayers;

				for (int32 LayerIndex = 0; LayerIndex < LayerFunctions.Layers.Num(); ++LayerIndex)
				{
					VisitFunction(LayerFunctions.Layers[LayerIndex], LayerParameter, LayerIndex);
				}

				for (int32 BlendIndex = 0; BlendIndex < LayerFunctions.Blends.Num(); ++BlendIndex)
				{
					VisitFunction(LayerFunctions.Blends[BlendIndex], BlendParameter, BlendIndex);
				}
			}
			else if (UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
			{
				VisitFunction(FunctionCall->MaterialFunction, Association, ParameterIndex);
			}
			else if (UMaterialExpressionStaticSwitchParameter* ExpressionStaticSwitchParameter = Cast<UMaterialExpressionStaticSwitchParameter>(Expression))
			{
				// Read the static switch parameter and only continue exploration along active branch.
				FMaterialParameterMetadata ParameterMeta;
				if (CachedExpressionData.GetParameterValue(EMaterialParameterType::StaticSwitch, FMemoryImageMaterialParameterInfo(Expression->GetParameterName()), ParameterMeta))
				{
					FExpressionInput* ExpressionInput = ParameterMeta.Value.AsStaticSwitch() ? &ExpressionStaticSwitchParameter->A : &ExpressionStaticSwitchParameter->B;
					PushUnexploredExpression(ExpressionInput->Expression);
					bVisitAllInputExpressions = false;
				}
			}
			else if (UMaterialExpressionStaticSwitch* ExpressionStaticSwitch = Cast<UMaterialExpressionStaticSwitch>(Expression))
			{
				bool ParamValue = (bool)ExpressionStaticSwitch->DefaultValue;

				// If no input expression specified, use the default value otherwise try to evaluate the input expression to the static bool.
				if (!ExpressionStaticSwitch->Value.Expression || EvaluateStaticBoolExpression(ExpressionStaticSwitch->Value.Expression, ParamValue, Association, ParameterIndex))
				{
					// Push the active input expression.
					FExpressionInput* ExpressionInput = ParamValue ? &ExpressionStaticSwitch->A : &ExpressionStaticSwitch->B;
					PushUnexploredExpression(ExpressionInput->Expression);
					bVisitAllInputExpressions = false;
				}
			}
			else if (UMaterialExpressionNamedRerouteUsage* ExpressionRerouteUsage = Cast<UMaterialExpressionNamedRerouteUsage>(Expression))
			{
				if (ExpressionRerouteUsage->Declaration)
				{
					PushUnexploredExpression(ExpressionRerouteUsage->Declaration->Input.Expression);
				}
			}

			// If otherwise specified, explore all input expressions to this node.
			if (bVisitAllInputExpressions)
			{
				for (FExpressionInput* ExpressionInput : Expression->GetInputsView())
				{
					PushUnexploredExpression(ExpressionInput->Expression);
				}
			}
		}
	}

	/** Visits the inner expressions of a function call */
	void VisitFunction(UMaterialFunctionInterface* Function, EMaterialParameterAssociation Association, int32 ParameterIndex)
	{
		if (!Function)
		{
			return;
		}

		// Get function outputs.
		TArray<FFunctionExpressionInput> Inputs;
		TArray<FFunctionExpressionOutput> Outputs;
		Function->GetInputsAndOutputs(Inputs, Outputs);

		// Push all function outputs as unexplored expressions to a new analyzer.
		// Note: we use a different analyzer here so that multiple function calls to the same function are treated separately, since
		// their actual state can be different due to static parameter overrides within a function.
		FMaterialConnectedPropertiesAnalyzer FunctionAnalyzer{ CachedExpressionData, Function };
		for (auto& Output : Outputs)
		{
			FunctionAnalyzer.PushUnexploredExpression(Output.ExpressionOutput);
		}
		
		// Visit inner function expressions.
		FunctionAnalyzer.VisitUnexploredExpressions(Association, ParameterIndex);
	}

	/** If expression is of type "static bool" this function walks the graph to find the actual value of the referenced static bool and writes the result to OutValue.
	 * @return true if the boolean value could be determined.
	 */
	bool EvaluateStaticBoolExpression(UMaterialExpression* Expression, bool& bOutValue, EMaterialParameterAssociation Association, int32 ParameterIndex)
	{
		if (!Expression)
		{
			return false;
		}
		else if (UMaterialExpressionStaticBoolParameter* ExpressionStaticBoolParameter = Cast<UMaterialExpressionStaticBoolParameter>(Expression))
		{
			FMaterialParameterMetadata ParameterMeta;
			if (CachedExpressionData.GetParameterValue(EMaterialParameterType::StaticSwitch, FMemoryImageMaterialParameterInfo(Expression->GetParameterName()), ParameterMeta))
			{
				bOutValue = ParameterMeta.Value.AsStaticSwitch();
				return true;
			}
			return false;
		}
		else if (UMaterialExpressionStaticSwitchParameter* ExpressionStaticSwitchParameter = Cast<UMaterialExpressionStaticSwitchParameter>(Expression))
		{
			FMaterialParameterMetadata ParameterMeta;
			if (CachedExpressionData.GetParameterValue(EMaterialParameterType::StaticSwitch, FMemoryImageMaterialParameterInfo(Expression->GetParameterName()), ParameterMeta))
			{
				FExpressionInput* OperandExpressionInput = ParameterMeta.Value.AsStaticSwitch() ? &ExpressionStaticSwitchParameter->A : &ExpressionStaticSwitchParameter->B;
				return EvaluateStaticBoolExpression(OperandExpressionInput->Expression, bOutValue, Association, ParameterIndex);
			}
			return false;
		}
		else if (UMaterialExpressionStaticSwitch* ExpressionStaticSwitch = Cast<UMaterialExpressionStaticSwitch>(Expression))
		{
			bool Value = (bool)ExpressionStaticSwitch->DefaultValue;
			// If node has a bool expression connected, try to evaluate that subgraph to a static boolean value.
			if (ExpressionStaticSwitch->Value.Expression && !EvaluateStaticBoolExpression(ExpressionStaticSwitch->Value.Expression, Value, Association, ParameterIndex))
			{
				// Node has the bool expression slot connected, but we could not evaluate it to a static boolean value.
				return false;
			}
			FExpressionInput* OperandExpressionInput = Value ? &ExpressionStaticSwitch->A : &ExpressionStaticSwitch->B;
			return EvaluateStaticBoolExpression(OperandExpressionInput->Expression, bOutValue, Association, ParameterIndex);
		}
		else if (UMaterialExpressionNamedRerouteUsage* ExpressionRerouteUsage = Cast<UMaterialExpressionNamedRerouteUsage>(Expression))
		{
			if (!ExpressionRerouteUsage->Declaration)
			{
				return false;
			}
			return EvaluateStaticBoolExpression(ExpressionRerouteUsage->Declaration->Input.Expression, bOutValue, Association, ParameterIndex);
		}
		else if (UMaterialExpressionFunctionInput* ExpressionFunctionInput = Cast<UMaterialExpressionFunctionInput>(Expression))
		{
			return EvaluateStaticBoolExpression(ExpressionFunctionInput->Preview.Expression, bOutValue, Association, ParameterIndex);
		}
		else if (UMaterialExpressionStaticBool* ExpressionStaticBool = Cast<UMaterialExpressionStaticBool>(Expression))
		{
			bOutValue = ExpressionStaticBool->Value;
			return true;
		}

		return false;
	}
};

void FMaterialCachedExpressionData::AnalyzeMaterial(UMaterial& Material)
{
	if (!GExperimentalMaterialCachedDataAnalysisEnabled)
	{
		if (!Material.bUseMaterialAttributes)
		{
			for (int32 PropertyIndex = 0; PropertyIndex < MP_MAX; ++PropertyIndex)
			{
				const EMaterialProperty Property = (EMaterialProperty)PropertyIndex;
				const FExpressionInput* Input = Material.GetExpressionInputForProperty(Property);
				if (Input && Input->IsConnected())
				{
					SetPropertyConnected(Property);
				}
			}
		}

		FMaterialCachedExpressionContext Context;
		UpdateForExpressions(Context, Material.GetExpressions(), EMaterialParameterAssociation::GlobalParameter, -1);
	}
	else
	{
		// Now crawl the graph from outputs to inputs skipping unused subgraphs ("compiled out" by static switches).
		FMaterialConnectedPropertiesAnalyzer Analyzer{ *this };

		// Add material outputs depending on whether material uses material attributes.
		if (Material.bUseMaterialAttributes)
		{
			const FExpressionInput* Input = Material.GetExpressionInputForProperty(MP_MaterialAttributes);
			if (Input && Input->IsConnected())
			{
				Analyzer.PushUnexploredExpression(Input->Expression);
			}
		}
		else
		{
			for (int32 PropertyIndex = 0; PropertyIndex < MP_MAX; ++PropertyIndex)
			{
				const EMaterialProperty Property = (EMaterialProperty)PropertyIndex;
				const FExpressionInput* Input = Material.GetExpressionInputForProperty(Property);
				if (Input && Input->IsConnected())
				{
					Analyzer.PushUnexploredExpression(Input->Expression);
					SetPropertyConnected(Property);
				}
			}
		}
	
		// If there are any connected function output expressions, mark them as unexplored expressions.
		// This occurs when the user opens a MaterialFunction in the editor.
		for (UMaterialExpression* Expression : Material.GetExpressions())
		{
			if (Expression && Expression->IsA<UMaterialExpressionFunctionOutput>())
			{
				Analyzer.PushUnexploredExpression(Expression);
			}
		}

		Analyzer.VisitUnexploredExpressions(GlobalParameter, -1);
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
	CachedTree.SetRequestedFields(EmitContext, RequestedAttributesType);

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
				if (Property != MP_CustomOutput && CachedTree.IsAttributeUsed(EmitContext, *EmitResultScope, ResultType, Property))
				{
					CachedData.SetPropertyConnected(Property);
				}
			}
		}
	}
}

}

void FMaterialCachedExpressionData::UpdateForCachedHLSLTree(const FMaterialCachedHLSLTree& CachedTree, const FStaticParameterSet* StaticParameters, const UMaterialInterface* TargetMaterial)
{
	using namespace UE::HLSLTree;

	// We ignore errors here, errors will be captured when we actually emit HLSL from the tree
	FNullErrorHandler NullErrorHandler;

	FMemStackBase Allocator;
	FEmitContext EmitContext(Allocator, FTargetParameters(), NullErrorHandler, CachedTree.GetTypeRegistry());
	EmitContext.MaterialInterface = TargetMaterial;

	Material::FEmitData& EmitMaterialData = EmitContext.AcquireData<Material::FEmitData>();
	EmitMaterialData.CachedExpressionData = this;
	EmitMaterialData.StaticParameters = StaticParameters;

	::Private::PrepareHLSLTree(EmitContext, CachedTree, *this, SF_Pixel);
	::Private::PrepareHLSLTree(EmitContext, CachedTree, *this, SF_Vertex);

	for (const UMaterialExpressionCustomOutput* CustomOutput : CachedTree.GetMaterialCustomOutputs())
	{
		if (const UMaterialExpressionLandscapeGrassOutput* ExpressionGrassOutput = Cast<UMaterialExpressionLandscapeGrassOutput>(CustomOutput))
		{
			for (const FGrassInput& GrassInput : ExpressionGrassOutput->GrassTypes)
			{
				GrassTypes.AddUnique(GrassInput.GrassType);
			}
		}
	}
}

void FMaterialCachedExpressionData::Validate(const UMaterialInterface& Material)
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

		if (!FPlatformProperties::RequiresCookedData() && AllowShaderCompiling())
		{
			// Mute log errors created by GetShaderSourceFilePath during include path validation
			LOG_SCOPE_VERBOSITY_OVERRIDE(LogShaders, ELogVerbosity::Fatal);

			for (auto PathIt = EditorOnlyData->ExpressionIncludeFilePaths.CreateIterator(); PathIt; ++PathIt)
			{
				const FString& IncludeFilePath = *PathIt;
				bool bValidExpressionIncludePath = false;

				if (!IncludeFilePath.IsEmpty())
				{
					FString ValidatedPath = GetShaderSourceFilePath(IncludeFilePath);
					if (!ValidatedPath.IsEmpty())
					{
						ValidatedPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*ValidatedPath);
						if (FPaths::FileExists(ValidatedPath))
						{
							bValidExpressionIncludePath = true;
						}
					}
				}

				if (!bValidExpressionIncludePath)
				{
					UE_LOG(LogMaterial, Warning, TEXT("Expression include file path '%s' is invalid, removing from cached data for material '%s'."), *IncludeFilePath, *Material.GetPathName());
					PathIt.RemoveCurrent();
				}
			}
		}

		// Sort to make hashing less dependent on the order of expression visiting
		EditorOnlyData->ExpressionIncludeFilePaths.Sort(TLess<>());
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
	case EMaterialParameterType::SparseVolumeTexture:
		OutResult.Value = SparseVolumeTextureValues[ParameterIndex].LoadSynchronous();
		break;
	case EMaterialParameterType::Font:
		OutResult.Value = FMaterialParameterValue(FontValues[ParameterIndex].LoadSynchronous(), FontPageValues[ParameterIndex]);
		break;
	case EMaterialParameterType::StaticSwitch:
		OutResult.Value = StaticSwitchValues[ParameterIndex];
		OutResult.bDynamicSwitchParameter = DynamicSwitchValues[ParameterIndex];
		break;
#if WITH_EDITORONLY_DATA
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

bool FMaterialCachedExpressionData::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);
	return false;
}

void FMaterialCachedExpressionData::PostSerialize(const FArchive& Ar)
{
	if (Ar.IsLoading())
	{
		if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::IncreaseMaterialAttributesInputMask)
		{
			PropertyConnectedMask = uint64(PropertyConnectedBitmask_DEPRECATED);
		}
	}

#if WITH_EDITORONLY_DATA
	if(Ar.IsLoading())
	{
		bool bIsEditorOnlyDataStripped = true;
		if (EditorOnlyData)
		{
			const FMaterialCachedParameterEditorEntry& EditorEntry = EditorOnlyData->EditorEntries[(int32)EMaterialParameterType::StaticSwitch];
			bIsEditorOnlyDataStripped = EditorEntry.EditorInfo.Num() == 0;
		}

		if (EditorOnlyData && !bIsEditorOnlyDataStripped)
		{
			StaticSwitchValues = EditorOnlyData->StaticSwitchValues_DEPRECATED;
			check(DynamicSwitchValues.Num() == 0);
			DynamicSwitchValues.AddDefaulted(StaticSwitchValues.Num());
		}
	}
#endif
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


