// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialExpression.h"
#include "SceneTypes.h"

#include "MaterialEditingLibrary.generated.h"

class UMaterialFunction;
class UMaterialInstance;
class UMaterialInstanceConstant;
class URuntimeVirtualTexture;

USTRUCT(BlueprintType)
struct FMaterialStatistics
{
	GENERATED_BODY()

	/** Number of instructions used by most expensive vertex shader in the material */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Statistics)
	int32 NumVertexShaderInstructions = 0;

	/** Number of instructions used by most expensive pixel shader in the material */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Statistics)
	int32 NumPixelShaderInstructions = 0;

	/** Number of samplers required by the material */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Statistics)
	int32 NumSamplers = 0;

	/** Number of textures sampled by the vertex shader */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Statistics)
	int32 NumVertexTextureSamples = 0;

	/** Number of textures sampled by the pixel shader */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Statistics)
	int32 NumPixelTextureSamples = 0;

	/** Number of virtual textures sampled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Statistics)
	int32 NumVirtualTextureSamples = 0;

	/** Number of interpolator scalars required for UVs */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Statistics)
	int32 NumUVScalars = 0;

	/** Number of interpolator scalars required for user-defined interpolators */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Statistics)
	int32 NumInterpolatorScalars = 0;
};

/** Blueprint library for creating/editing Materials */
UCLASS()
class MATERIALEDITOR_API UMaterialEditingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	*	Create a new material expression node within the supplied material, optionally specifying asset to use
	*	@note	If a MaterialFunction and Material are specified, expression is added to Material and not MaterialFunction, assuming Material is a preview that will be copied to Function later by user.
	*	@param	Material					Material asset to add an expression to
	*	@param	MaterialFunction			Specified if adding an expression to a MaterialFunction, used as Outer for new expression object
	*	@param	SelectedAsset				If specified, new node will attempt to use this asset, if of the appropriate type (e.g. Texture for a TextureSampler)
	*	@param	ExpressionClass				Class of expression to add
	*	@param	NodePosX					X position of new expression node
	*	@param	NodePosY					Y position of new expression node
	*	@param	bAllowMarkingPackageDirty	Packages can't be marked dirty outside of the game thread. If this is false, package will need to be dirtied through other means. 
	*/
	static UMaterialExpression* CreateMaterialExpressionEx(UMaterial* Material, UMaterialFunction* MaterialFunction, TSubclassOf<UMaterialExpression> ExpressionClass,
		UObject* SelectedAsset = nullptr, int32 NodePosX = 0, int32 NodePosY = 0, bool bAllowMarkingPackageDirty = true);

	/**
	*	Rebuilds dependent Material Instance Editors
	*	@param	BaseMaterial	Material that MaterialInstance must be based on for Material Instance Editor to be rebuilt
	*/
	static void RebuildMaterialInstanceEditors(UMaterial* BaseMaterial);

	/**
	*	Rebuilds dependent Material Instance Editors
	*	@param	BaseMaterial	Material that MaterialInstance must be based on for Material Instance Editor to be rebuilt
	*/
	static void RebuildMaterialInstanceEditors(UMaterialFunction* BaseFunction);

	///////////// MATERIAL EDITING


	/** Returns number of material expressions in the supplied material */
	UFUNCTION(BlueprintPure, Category = "MaterialEditing")
	static int32 GetNumMaterialExpressions(const UMaterial* Material);

	/** Delete all material expressions in the supplied material */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static void DeleteAllMaterialExpressions(UMaterial* Material);

	/** Delete a specific expression from a material. Will disconnect from other expressions. */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static void DeleteMaterialExpression(UMaterial* Material, UMaterialExpression* Expression);

	/** 
	 *	Create a new material expression node within the supplied material 
	 *	@param	Material			Material asset to add an expression to
	 *	@param	ExpressionClass		Class of expression to add
	 *	@param	NodePosX			X position of new expression node
	 *	@param	NodePosY			Y position of new expression node
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static UMaterialExpression* CreateMaterialExpression(UMaterial* Material, TSubclassOf<UMaterialExpression> ExpressionClass, int32 NodePosX=0, int32 NodePosY=0);

	/** 
	 *	Duplicates the provided material expression adding it to the same material / material function, and copying parameters.
	 *  Note: Does not duplicate transient properties (Ex: GraphNode).
	 *
	 *	@param	Material			Material asset to add an expression to
	 *	@param	MaterialFunction	Specified if adding an expression to a MaterialFunction, used as Outer for new expression object
	 *	@param	SourceExpression	Expression to be duplicated
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static UMaterialExpression* DuplicateMaterialExpression(UMaterial* Material, UMaterialFunction* MaterialFunction, UMaterialExpression* Expression);

	/** 
	 *	Enable a particular usage for the supplied material (e.g. SkeletalMesh, ParticleSprite etc)
	 *	@param	Material			Material to change usage for
	 *	@param	Usage				New usage type to enable for this material
	 *	@param	bNeedsRecompile		Returned to indicate if material needs recompiling after this change
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static bool SetMaterialUsage(UMaterial* Material, EMaterialUsage Usage, bool& bNeedsRecompile);

	/**
	 *	Check if a particular usage is enabled for the supplied material (e.g. SkeletalMesh, ParticleSprite etc)
	 *	@param	Material			Material to check usage for
	 *	@param	Usage				Usage type to check for this material
	 */
	UFUNCTION(BlueprintPure, Category = "MaterialEditing")
	static bool HasMaterialUsage(UMaterial* Material, EMaterialUsage Usage);

	/** 
	 *	Connect a material expression output to one of the material property inputs (e.g. diffuse color, opacity etc)
	 *	@param	FromExpression		Expression to make connection from
	 *	@param	FromOutputName		Name of output of FromExpression to make connection from
	 *	@param	Property			Property input on material to make connection to
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static bool ConnectMaterialProperty(UMaterialExpression* FromExpression, FString FromOutputName, EMaterialProperty Property);

	/**
	 *	Create connection between two material expressions
	 *	@param	FromExpression		Expression to make connection from
	 *	@param	FromOutputName		Name of output of FromExpression to make connection from. Leave empty to use first output.
	 *	@param	ToExpression		Expression to make connection to
	 *	@param	ToInputName			Name of input of ToExpression to make connection to. Leave empty to use first input.
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static bool ConnectMaterialExpressions(UMaterialExpression* FromExpression, FString FromOutputName, UMaterialExpression* ToExpression, FString ToInputName);

	/** 
	 *	Trigger a recompile of a material. Must be performed after making changes to the graph to have changes reflected.
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static void RecompileMaterial(UMaterial* Material);

	/**
	 *	Layouts the expressions in a grid pattern
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static void LayoutMaterialExpressions(UMaterial* Material);

	/** Get the default scalar (float) parameter value from a Material */
	UFUNCTION(BlueprintPure, Category = "MaterialEditing")
	static float GetMaterialDefaultScalarParameterValue(UMaterial* Material, FName ParameterName);


	/** Get the default texture parameter value from a Material  */
	UFUNCTION(BlueprintPure, Category = "MaterialEditing")
	static UTexture* GetMaterialDefaultTextureParameterValue(UMaterial* Material, FName ParameterName);

	/** Get the default vector parameter value from a Material */
	UFUNCTION(BlueprintPure, Category = "MaterialEditing")
	static FLinearColor GetMaterialDefaultVectorParameterValue(UMaterial* Material, FName ParameterName);

	/** Get the default static switch parameter value from a Material */
	UFUNCTION(BlueprintPure, Category = "MaterialEditing")
	static bool GetMaterialDefaultStaticSwitchParameterValue(UMaterial* Material, FName ParameterName);

	/** Get the set of selected nodes from an active material editor */
	UFUNCTION(BlueprintPure, Category = "MaterialEditing")
	static TSet<UObject*> GetMaterialSelectedNodes(UMaterial* Material);

	/** Get the node providing the output for a given material property from an active material editor */
	UFUNCTION(BlueprintPure, Category = "MaterialEditing")
	static UMaterialExpression* GetMaterialPropertyInputNode(UMaterial* Material, EMaterialProperty Property);

	/** Get the node output name providing the output for a given material property from an active material editor */
	UFUNCTION(BlueprintPure, Category = "MaterialEditing")
	static FString GetMaterialPropertyInputNodeOutputName(UMaterial* Material, EMaterialProperty Property);

	/** Get the array of input pin names for a material expression */
	UFUNCTION(BlueprintPure, Category = "MaterialEditing")
	static TArray<FString> GetMaterialExpressionInputNames(UMaterialExpression* MaterialExpression);

	/** Get the array of input pin types for a material expression */
	UFUNCTION(BlueprintPure, Category = "MaterialEditing")
	static TArray<int32> GetMaterialExpressionInputTypes(UMaterialExpression* MaterialExpression);

	/** Get the set of nodes acting as inputs to a node from an active material editor */
	UFUNCTION(BlueprintPure, Category = "MaterialEditing")
	static TArray<UMaterialExpression*> GetInputsForMaterialExpression(UMaterial* Material, UMaterialExpression* MaterialExpression);

	/** Get the output name of input node connected to MaterialExpression from an active material editor */
	UFUNCTION(BlueprintPure, Category = "MaterialEditing")
	static bool GetInputNodeOutputNameForMaterialExpression(UMaterialExpression* MaterialExpression, UMaterialExpression* InputNode, FString& OutputName);

	/** Get the position of the MaterialExpression node. */
	UFUNCTION(BlueprintPure, Category = "MaterialEditing")
	static void GetMaterialExpressionNodePosition(UMaterialExpression* MaterialExpression, int32& NodePosX, int32& NodePosY);

	/** Get the list of textures used by a material */
	UFUNCTION(BlueprintPure, Category = "MaterialEditing")
	static TArray<UTexture*> GetUsedTextures(UMaterial* Material);
	
	//////// MATERIAL FUNCTION EDITING

	/** Returns number of material expressions in the supplied material */
	UFUNCTION(BlueprintPure, Category = "MaterialEditing")
	static int32 GetNumMaterialExpressionsInFunction(const UMaterialFunction* MaterialFunction);

	/**
	*	Create a new material expression node within the supplied material function
	*	@param	MaterialFunction	Material function asset to add an expression to
	*	@param	ExpressionClass		Class of expression to add
	*	@param	NodePosX			X position of new expression node
	*	@param	NodePosY			Y position of new expression node
	*/
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static UMaterialExpression* CreateMaterialExpressionInFunction(UMaterialFunction* MaterialFunction, TSubclassOf<UMaterialExpression> ExpressionClass, int32 NodePosX = 0, int32 NodePosY = 0);

	/** Delete all material expressions in the supplied material function */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static void DeleteAllMaterialExpressionsInFunction(UMaterialFunction* MaterialFunction);

	/** Delete a specific expression from a material function. Will disconnect from other expressions. */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static void DeleteMaterialExpressionInFunction(UMaterialFunction* MaterialFunction, UMaterialExpression* Expression);

	/**
	 *	Update a Material Function after edits have been made.
	 *	Will recompile any Materials that use the supplied Material Function.
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing", meta = (HidePin = "PreviewMaterial"))
	static void UpdateMaterialFunction(UMaterialFunctionInterface* MaterialFunction, UMaterial* PreviewMaterial = nullptr);

	/**
	 *	Layouts the expressions in a grid pattern
	 */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static void LayoutMaterialFunctionExpressions(UMaterialFunction* MaterialFunction);

	//////// MATERIAL INSTANCE CONSTANT EDITING

	/** Set the parent Material or Material Instance to use for this Material Instance */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static void SetMaterialInstanceParent(UMaterialInstanceConstant* Instance, UMaterialInterface* NewParent);

	/** Clears all material parameters set by this Material Instance */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static void ClearAllMaterialInstanceParameters(UMaterialInstanceConstant* Instance);


	/** Get the current scalar (float) parameter value from a Material Instance */
	UFUNCTION(BlueprintPure, Category = "MaterialEditing")
	static float GetMaterialInstanceScalarParameterValue(UMaterialInstanceConstant* Instance, FName ParameterName, EMaterialParameterAssociation Association = EMaterialParameterAssociation::GlobalParameter);

	/** Set the scalar (float) parameter value for a Material Instance */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static bool SetMaterialInstanceScalarParameterValue(UMaterialInstanceConstant* Instance, FName ParameterName, float Value, EMaterialParameterAssociation Association = EMaterialParameterAssociation::GlobalParameter);


	/** Get the current texture parameter value from a Material Instance */
	UFUNCTION(BlueprintPure, Category = "MaterialEditing")
	static UTexture* GetMaterialInstanceTextureParameterValue(UMaterialInstanceConstant* Instance, FName ParameterName, EMaterialParameterAssociation Association = EMaterialParameterAssociation::GlobalParameter);

	/** Set the texture parameter value for a Material Instance */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static bool SetMaterialInstanceTextureParameterValue(UMaterialInstanceConstant* Instance, FName ParameterName, UTexture* Value, EMaterialParameterAssociation Association = EMaterialParameterAssociation::GlobalParameter);


	/** Get the current texture parameter value from a Material Instance */
	UFUNCTION(BlueprintPure, Category = "MaterialEditing")
	static URuntimeVirtualTexture* GetMaterialInstanceRuntimeVirtualTextureParameterValue(UMaterialInstanceConstant* Instance, FName ParameterName, EMaterialParameterAssociation Association = EMaterialParameterAssociation::GlobalParameter);

	/** Set the texture parameter value for a Material Instance */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static bool SetMaterialInstanceRuntimeVirtualTextureParameterValue(UMaterialInstanceConstant* Instance, FName ParameterName, URuntimeVirtualTexture* Value, EMaterialParameterAssociation Association = EMaterialParameterAssociation::GlobalParameter);


	/** Get the current texture parameter value from a Material Instance */
	UFUNCTION(BlueprintPure, Category = "MaterialEditing")
	static USparseVolumeTexture* GetMaterialInstanceSparseVolumeTextureParameterValue(UMaterialInstanceConstant* Instance, FName ParameterName, EMaterialParameterAssociation Association = EMaterialParameterAssociation::GlobalParameter);

	/** Set the texture parameter value for a Material Instance */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static bool SetMaterialInstanceSparseVolumeTextureParameterValue(UMaterialInstanceConstant* Instance, FName ParameterName, USparseVolumeTexture* Value, EMaterialParameterAssociation Association = EMaterialParameterAssociation::GlobalParameter);


	/** Get the current vector parameter value from a Material Instance */
	UFUNCTION(BlueprintPure, Category = "MaterialEditing")
	static FLinearColor GetMaterialInstanceVectorParameterValue(UMaterialInstanceConstant* Instance, FName ParameterName, EMaterialParameterAssociation Association = EMaterialParameterAssociation::GlobalParameter);

	/** Set the vector parameter value for a Material Instance */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static bool SetMaterialInstanceVectorParameterValue(UMaterialInstanceConstant* Instance, FName ParameterName, FLinearColor Value, EMaterialParameterAssociation Association = EMaterialParameterAssociation::GlobalParameter);

	/** Get the current static switch parameter value from a Material Instance */
	UFUNCTION(BlueprintPure, Category = "MaterialEditing")
	static bool GetMaterialInstanceStaticSwitchParameterValue(UMaterialInstanceConstant* Instance, FName ParameterName, EMaterialParameterAssociation Association = EMaterialParameterAssociation::GlobalParameter);

	/** Set the static switch parameter value for a Material Instance */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static bool SetMaterialInstanceStaticSwitchParameterValue(UMaterialInstanceConstant* Instance, FName ParameterName, bool Value, EMaterialParameterAssociation Association = EMaterialParameterAssociation::GlobalParameter);

	/** Called after making modifications to a Material Instance to recompile shaders etc. */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static void UpdateMaterialInstance(UMaterialInstanceConstant* Instance);

	/** Gets all direct child mat instances */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static void GetChildInstances(UMaterialInterface* Parent, TArray<FAssetData>& ChildInstances);

	/** Gets all scalar parameter names */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static void GetScalarParameterNames(UMaterialInterface* Material, TArray<FName>& ParameterNames);

	/** Gets all vector parameter names */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static void GetVectorParameterNames(UMaterialInterface* Material, TArray<FName>& ParameterNames);

	/** Gets all texture parameter names */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static void GetTextureParameterNames(UMaterialInterface* Material, TArray<FName>& ParameterNames);

	/** Gets all static switch parameter names */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static void GetStaticSwitchParameterNames(UMaterialInterface* Material, TArray<FName>& ParameterNames);

	/**
	*	Returns the path of the asset where the parameter originated, as well as true/false if it was found
	*	@param	Material	The material or material instance you want to look up a parameter from
	*	@param	ParameterName		The parameter name
	*	@param	ParameterSource		The soft object path of the asset the parameter originates in 
	*	@return	Whether or not the parameter was found in this material
	*/
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static bool GetScalarParameterSource(UMaterialInterface* Material, const FName ParameterName, FSoftObjectPath& ParameterSource);

	/**
	*	Returns the path of the asset where the parameter originated, as well as true/false if it was found
	*	@param	Material	The material or material instance you want to look up a parameter from
	*	@param	ParameterName		The parameter name
	*	@param	ParameterSource		The soft object path of the asset the parameter originates in
	*	@return	Whether or not the parameter was found in this material
	*/
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static bool GetVectorParameterSource(UMaterialInterface* Material, const FName ParameterName, FSoftObjectPath& ParameterSource);

	/**
	*	Returns the path of the asset where the parameter originated, as well as true/false if it was found
	*	@param	Material	The material or material instance you want to look up a parameter from
	*	@param	ParameterName		The parameter name
	*	@param	ParameterSource		The soft object path of the asset the parameter originates in
	*	@return	Whether or not the parameter was found in this material
	*/
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static bool GetTextureParameterSource(UMaterialInterface* Material, const FName ParameterName, FSoftObjectPath& ParameterSource);

	/**
	*	Returns the path of the asset where the parameter originated, as well as true/false if it was found
	*	@param	Material	The material or material instance you want to look up a parameter from
	*	@param	ParameterName		The parameter name
	*	@param	ParameterSource		The soft object path of the asset the parameter originates in
	*	@return	Whether or not the parameter was found in this material
	*/
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static bool GetStaticSwitchParameterSource(UMaterialInterface* Material, const FName ParameterName, FSoftObjectPath& ParameterSource);

	/** Returns statistics about the given material */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static FMaterialStatistics GetStatistics(UMaterialInterface* Material);

	/** Returns any nanite override material for the given material */
	UFUNCTION(BlueprintCallable, Category = "MaterialEditing")
	static UMaterialInterface* GetNaniteOverrideMaterial(UMaterialInterface* Material);
};