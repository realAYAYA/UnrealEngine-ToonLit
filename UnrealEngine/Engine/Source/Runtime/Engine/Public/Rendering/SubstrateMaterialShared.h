// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SubstrateDefinitions.h"
#include "Serialization/MemoryImage.h"


// Structures in this files are only used a compilation result return by the compiler.
// They are also used to present material information in the editor UI.


struct FSubstrateRegisteredSharedLocalBasis
{
	DECLARE_TYPE_LAYOUT(FSubstrateRegisteredSharedLocalBasis, NonVirtual);
public:
	FSubstrateRegisteredSharedLocalBasis();

	LAYOUT_FIELD_EDITORONLY(int32, NormalCodeChunk);
	LAYOUT_FIELD_EDITORONLY(int32, TangentCodeChunk);
	LAYOUT_FIELD_EDITORONLY(uint64, NormalCodeChunkHash);
	LAYOUT_FIELD_EDITORONLY(uint64, TangentCodeChunkHash);
	LAYOUT_FIELD_EDITORONLY(uint8, GraphSharedLocalBasisIndex);
};

struct FSubstrateOperator
{
	DECLARE_TYPE_LAYOUT(FSubstrateOperator, NonVirtual);
public:
	FSubstrateOperator();

	// !!!!!!!!!!
	// Not using LAYOUT_BITFIELD_EDITORONLY because it seems to cause issue with bit being shifted around when copy happens.
	// So in the meantime we find it out, LAYOUT_FIELD_EDITORONLY using uint8 is used.
	// !!!!!!!!!!

	LAYOUT_FIELD_EDITORONLY(int32, OperatorType);
	LAYOUT_FIELD_EDITORONLY(uint8, bNodeRequestParameterBlending);

	LAYOUT_FIELD_EDITORONLY(int32, Index);			// Index into the array of operators
	LAYOUT_FIELD_EDITORONLY(int32, ParentIndex);	// Parent operator index
	LAYOUT_FIELD_EDITORONLY(int32, LeftIndex);		// Left child operator index
	LAYOUT_FIELD_EDITORONLY(int32, RightIndex);		// Right child operator index
	LAYOUT_FIELD_EDITORONLY(int32, ThicknessIndex);	// Thickness expression index

	// Data used for BSDF type nodes only
	LAYOUT_FIELD_EDITORONLY(int32, BSDFIndex);		// Index in the array of BSDF if a BSDF operator
	LAYOUT_FIELD_EDITORONLY(int32, BSDFType);
	LAYOUT_FIELD_EDITORONLY(FSubstrateRegisteredSharedLocalBasis, BSDFRegisteredSharedLocalBasis);
	LAYOUT_FIELD_EDITORONLY(uint8, bBSDFHasSSS);
	LAYOUT_FIELD_EDITORONLY(uint8, bBSDFHasMFPPluggedIn);
	LAYOUT_FIELD_EDITORONLY(uint8, bBSDFHasEdgeColor);
	LAYOUT_FIELD_EDITORONLY(uint8, bBSDFHasFuzz);
	LAYOUT_FIELD_EDITORONLY(uint8, bBSDFHasSecondRoughnessOrSimpleClearCoat);
	LAYOUT_FIELD_EDITORONLY(uint8, bBSDFHasAnisotropy);
	LAYOUT_FIELD_EDITORONLY(uint8, bBSDFHasGlint);
	LAYOUT_FIELD_EDITORONLY(uint8, bBSDFHasSpecularProfile);

	LAYOUT_FIELD_EDITORONLY(uint8, bBSDFWritesEmissive);
	LAYOUT_FIELD_EDITORONLY(uint8, bBSDFWritesAmbientOcclusion);

	// Data derived after the tree has been built.
	LAYOUT_FIELD_EDITORONLY(int32, MaxDistanceFromLeaves);
	LAYOUT_FIELD_EDITORONLY(int32, LayerDepth);
	LAYOUT_FIELD_EDITORONLY(uint8, bIsTop);
	LAYOUT_FIELD_EDITORONLY(uint8, bIsBottom);

	LAYOUT_FIELD_EDITORONLY(uint8, bUseParameterBlending);			// True when part of a sub tree where parameter blending is in use
	LAYOUT_FIELD_EDITORONLY(uint8, bRootOfParameterBlendingSubTree);// True when the root of a sub tree where parameter blending is in use. Only this node will register a BSDF
	LAYOUT_FIELD_EDITORONLY(FGuid, MaterialExpressionGuid);			// Material expression Guid for mapping between UMaterialExpression and FSubstrateOperator

	void CombineFlagsForParameterBlending(FSubstrateOperator& A, FSubstrateOperator& B);

	void CopyFlagsForParameterBlending(FSubstrateOperator& A);

	bool IsDiscarded() const;
};

#define SUBSTRATE_COMPILATION_OUTPUT_MAX_OPERATOR 24


#define SUBSTRATE_MATERIAL_TYPE_SINGLESLAB			0
#define SUBSTRATE_MATERIAL_TYPE_MULTIPLESLABS		1
#define SUBSTRATE_MATERIAL_TYPE_VOLUMETRICFOGCLOUD	2
#define SUBSTRATE_MATERIAL_TYPE_UNLIT				3
#define SUBSTRATE_MATERIAL_TYPE_HAIR				4
#define SUBSTRATE_MATERIAL_TYPE_SINGLELAYERWATER	5
#define SUBSTRATE_MATERIAL_TYPE_EYE					6
#define SUBSTRATE_MATERIAL_TYPE_LIGHTFUNCTION		7
#define SUBSTRATE_MATERIAL_TYPE_POSTPROCESS			8
#define SUBSTRATE_MATERIAL_TYPE_UI					9
#define SUBSTRATE_MATERIAL_TYPE_DECAL				10

struct FSubstrateMaterialCompilationOutput
{
	DECLARE_TYPE_LAYOUT(FSubstrateMaterialCompilationOutput, NonVirtual);
public:

	FSubstrateMaterialCompilationOutput();

	////
	//// The following data is required at runtime
	////

	/** Substrate material type, at compile time (Possible values from SUBSTRATE_MATERIAL_TYPE_XXX: simple/single/complex/complex special) */
	LAYOUT_FIELD(uint8, SubstrateMaterialType);

	/** Substrate closure count, at compile time (0-7) */
	LAYOUT_FIELD(uint8, SubstrateClosureCount);

	/** Substrate uint per pixel, at compile time (0-255) */
	LAYOUT_FIELD(uint8, SubstrateUintPerPixel);

	////
	//// The following data is only needed when compiling with the editor.
	////

	// Note we use LAYOUT_FIELD_EDITORONLY for bools because LAYOUT_BITFIELD_EDITORONLY was causing issues when serialising the structure.
	// SUBSTRATE_TODO pack that data.

	/** The Substrate verbose description */
	LAYOUT_FIELD_EDITORONLY(FMemoryImageString, SubstrateMaterialDescription);

	/** The number of local normal/tangent bases */
	LAYOUT_FIELD_EDITORONLY(uint8, SharedLocalBasesCount);
	/** Material requested byte count per pixel */
	LAYOUT_FIELD_EDITORONLY(uint8, RequestedBytePerPixel);
	/** The byte count per pixel supported by the platform the material has been compiled against */
	LAYOUT_FIELD_EDITORONLY(uint8, PlatformBytePerPixel);

	/** Material requested closure count per pixel */
	LAYOUT_FIELD_EDITORONLY(uint8, RequestedClosurePerPixel);
	/** The closure count per pixel supported by the platform the material has been compiled against */
	LAYOUT_FIELD_EDITORONLY(uint8, PlatformClosurePixel);

	/** Indicate that the material is considered a thin surface instead of a volume filled up with matter */
	LAYOUT_FIELD_EDITORONLY(uint8, bIsThin);
	/** Indicate the final material type */
	LAYOUT_FIELD_EDITORONLY(uint8, MaterialType);

	/** The byte per pixel count supported by the platform the material has been compiled against */
	LAYOUT_FIELD_EDITORONLY(uint8, bMaterialOutOfBudgetHasBeenSimplified, 1);

	LAYOUT_FIELD_EDITORONLY(int8, RootOperatorIndex);
	LAYOUT_ARRAY_EDITORONLY(FSubstrateOperator, Operators, SUBSTRATE_COMPILATION_OUTPUT_MAX_OPERATOR);
};

