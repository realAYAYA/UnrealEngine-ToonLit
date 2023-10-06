// Copyright Epic Games, Inc. All Rights Reserved.

#include "StrataMaterialShared.h"


IMPLEMENT_TYPE_LAYOUT(FStrataMaterialCompilationOutput);

FStrataMaterialCompilationOutput::FStrataMaterialCompilationOutput()
	: StrataMaterialType(0)
	, StrataBSDFCount(0)
	, StrataUintPerPixel(0)
	, bUsesComplexSpecialRenderPath(0)
#if WITH_EDITOR
	, StrataMaterialDescription()
	, SharedLocalBasesCount(0)
	, RequestedBytePixePixel(0)
	, PlatformBytePixePixel(0)
	, bIsThin(0)
	, MaterialType(0)
	, bMaterialOutOfBudgetHasBeenSimplified(0)
	, RootOperatorIndex(0)
#endif
{
#if WITH_EDITOR
	for (uint32 i = 0; i < STRATA_COMPILATION_OUTPUT_MAX_OPERATOR; ++i)
	{
		Operators[i] = FStrataOperator();
	}
#endif
}


IMPLEMENT_TYPE_LAYOUT(FStrataOperator);

FStrataOperator::FStrataOperator()
{
#if WITH_EDITOR
	OperatorType = INDEX_NONE;
	bNodeRequestParameterBlending = false;
	Index = INDEX_NONE;
	ParentIndex = INDEX_NONE;
	LeftIndex = INDEX_NONE;
	RightIndex = INDEX_NONE;
	ThicknessIndex = INDEX_NONE;

	BSDFIndex = INDEX_NONE;
	BSDFType = 0;
	BSDFRegisteredSharedLocalBasis = FStrataRegisteredSharedLocalBasis();
	bBSDFHasSSS = false;
	bBSDFHasMFPPluggedIn = false;
	bBSDFHasEdgeColor = false;
	bBSDFHasFuzz = false;
	bBSDFHasSecondRoughnessOrSimpleClearCoat = false;
	bBSDFHasAnisotropy = false;
	bBSDFHasGlint = false;
	bBSDFHasSpecularProfile = false;

	MaxDistanceFromLeaves = 0;
	LayerDepth = 0;
	bIsTop = false;
	bIsBottom = false;
	bUseParameterBlending = false;
	bRootOfParameterBlendingSubTree = false;
	MaterialExpressionGuid = FGuid();
#endif
}

void FStrataOperator::CombineFlagsForParameterBlending(FStrataOperator& A, FStrataOperator& B)
{
#if WITH_EDITOR
	bBSDFHasSSS = A.bBSDFHasSSS || B.bBSDFHasSSS;
	bBSDFHasMFPPluggedIn = A.bBSDFHasMFPPluggedIn || B.bBSDFHasMFPPluggedIn;
	bBSDFHasEdgeColor = A.bBSDFHasEdgeColor || B.bBSDFHasEdgeColor;
	bBSDFHasFuzz = A.bBSDFHasFuzz || B.bBSDFHasFuzz;
	bBSDFHasSecondRoughnessOrSimpleClearCoat = A.bBSDFHasSecondRoughnessOrSimpleClearCoat || B.bBSDFHasSecondRoughnessOrSimpleClearCoat;
	bBSDFHasAnisotropy = A.bBSDFHasAnisotropy || B.bBSDFHasAnisotropy;
	bBSDFHasGlint = A.bBSDFHasGlint || B.bBSDFHasGlint;
	bBSDFHasSpecularProfile = A.bBSDFHasSpecularProfile || B.bBSDFHasSpecularProfile;
#endif
}

void FStrataOperator::CopyFlagsForParameterBlending(FStrataOperator& A)
{
#if WITH_EDITOR
	bBSDFHasSSS = A.bBSDFHasSSS;
	bBSDFHasMFPPluggedIn = A.bBSDFHasMFPPluggedIn;
	bBSDFHasEdgeColor = A.bBSDFHasEdgeColor;
	bBSDFHasFuzz = A.bBSDFHasFuzz;
	bBSDFHasSecondRoughnessOrSimpleClearCoat = A.bBSDFHasSecondRoughnessOrSimpleClearCoat;
	bBSDFHasAnisotropy = A.bBSDFHasAnisotropy;
	bBSDFHasGlint = A.bBSDFHasGlint;
	bBSDFHasSpecularProfile = A.bBSDFHasSpecularProfile;
#endif
}

bool FStrataOperator::IsDiscarded() const
{
#if WITH_EDITOR
	return bUseParameterBlending && !bRootOfParameterBlendingSubTree;
#else
	return true;
#endif
}


IMPLEMENT_TYPE_LAYOUT(FStrataRegisteredSharedLocalBasis);

FStrataRegisteredSharedLocalBasis::FStrataRegisteredSharedLocalBasis()
{
#if WITH_EDITOR
	NormalCodeChunk = INDEX_NONE;
	TangentCodeChunk = INDEX_NONE;
	NormalCodeChunkHash = 0;
	TangentCodeChunkHash = 0;
	GraphSharedLocalBasisIndex = 0;
#endif
}