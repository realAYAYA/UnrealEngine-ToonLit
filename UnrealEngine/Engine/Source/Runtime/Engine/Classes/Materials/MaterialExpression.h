// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/UnrealType.h"
#include "Misc/Guid.h"
#include "MaterialShared.h"
#include "MaterialExpressionIO.h"

#include "MaterialExpression.generated.h"

class UEdGraphNode;
class UMaterial;
class UTexture;
struct FPropertyChangedEvent;
struct FMaterialShadingModelField;
struct FStrataOperator;

class UMaterialExpression;
class UMaterialExpressionComment;
class UMaterialExpressionExecBegin;
class UMaterialExpressionExecEnd;

class FMaterialHLSLGenerator;
enum class EMaterialNewScopeFlag : uint8;

namespace UE
{
namespace HLSLTree
{
class FScope;
class FStatement;
class FExpression;
}
}

//@warning: FExpressionInput is mirrored in MaterialShared.h and manually "subclassed" in Material.h (FMaterialInput)
#if !CPP      //noexport struct
USTRUCT(noexport)
struct FExpressionInput
{
	/** UMaterial expression that this input is connected to, or NULL if not connected. */
	UPROPERTY()
	TObjectPtr<class UMaterialExpression> Expression;

	/** Index into Expression's outputs array that this input is connected to. */
	UPROPERTY()
	int32 OutputIndex;

	/** 
	 * optional FName of the input.  
	 * Note that this is the only member which is not derived from the output currently connected. 
	 */
	UPROPERTY()
	FName InputName;


	UPROPERTY()
	int32 Mask;

	UPROPERTY()
	int32 MaskR;

	UPROPERTY()
	int32 MaskG;

	UPROPERTY()
	int32 MaskB;

	UPROPERTY()
	int32 MaskA;
};

USTRUCT(noexport)
struct FMaterialAttributesInput : public FExpressionInput
{
	UPROPERTY(transient)
	int32 PropertyConnectedBitmask;
};

#endif

#if !CPP      //noexport struct
/** Struct that represents an expression's output. */
USTRUCT(noexport)
struct FExpressionOutput
{
	UPROPERTY()
	FName OutputName;

	UPROPERTY()
	int32 Mask;

	UPROPERTY()
	int32 MaskR;

	UPROPERTY()
	int32 MaskG;

	UPROPERTY()
	int32 MaskB;

	UPROPERTY()
	int32 MaskA;
};
#endif

USTRUCT()
struct FExpressionExecOutput
{
	GENERATED_BODY()
public:
#if WITH_EDITOR
	ENGINE_API int32 Compile(class FMaterialCompiler* Compiler) const;

	ENGINE_API void Connect(class UMaterialExpression* InExpression);

	inline class UMaterialExpression* GetExpression() const { return Expression; }

	/** Returns the statement for the expression connected to this input */ 
	ENGINE_API bool GenerateHLSLStatements(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope) const;

	/** Creates a new scope, and populates it with the expression connected to this input */
	ENGINE_API UE::HLSLTree::FScope* NewOwnedScopeWithStatements(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FStatement& Owner) const;
	ENGINE_API UE::HLSLTree::FScope* NewScopeWithStatements(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, EMaterialNewScopeFlag Flags) const;
#endif // WITH_EDITOR
private:
	UPROPERTY()
	TObjectPtr<class UMaterialExpression> Expression = nullptr;
};

struct FExpressionExecOutputEntry
{
	FName Name;
	FExpressionExecOutput* Output = nullptr;
};

enum class EMaterialExpressionSetParameterValueFlags : uint32
{
	None = 0u,
	SendPostEditChangeProperty = (1u << 0), // Send PostEditChangeProperty events for all properties that are modified
	NoUpdateExpressionGuid = (1u << 1), // By default ExpressionGUI will be updated for static parameters
	AssignGroupAndSortPriority  = (1u << 2), //  Update the Group and SortPriority along with parameter value
};
ENUM_CLASS_FLAGS(EMaterialExpressionSetParameterValueFlags);

USTRUCT()
struct ENGINE_API FMaterialExpressionCollection
{
	GENERATED_BODY()

	void AddExpression(UMaterialExpression* InExpression);
	void RemoveExpression(UMaterialExpression* InExpression);
	void AddComment(UMaterialExpressionComment* InExpression);
	void RemoveComment(UMaterialExpressionComment* InExpression);
	void Empty();

	/** Array of material expressions, excluding Comments.  Used by the material editor. */
	UPROPERTY()
	TArray<TObjectPtr<UMaterialExpression>> Expressions;

	/** Array of comments associated with this material; viewed in the material editor. */
	UPROPERTY()
	TArray<TObjectPtr<UMaterialExpressionComment>> EditorComments;

	/** The execution begin expression, if material is using an exec wire */
	UPROPERTY()
	TObjectPtr<UMaterialExpressionExecBegin> ExpressionExecBegin = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialExpressionExecEnd> ExpressionExecEnd = nullptr;
};

UCLASS(abstract, Optional, BlueprintType, hidecategories=Object)
class ENGINE_API UMaterialExpression : public UObject
{
	GENERATED_UCLASS_BODY()

	static constexpr int32 CompileExecutionOutputIndex = -2;

#if WITH_EDITOR
	static void InitializeNumExecutionInputs(TArrayView<UMaterialExpression*> Expressions);
#endif

	UPROPERTY(BlueprintReadWrite, Category = MaterialEditing)
	int32 MaterialExpressionEditorX;

	UPROPERTY(BlueprintReadWrite, Category = MaterialEditing)
	int32 MaterialExpressionEditorY;

	/** Expression's Graph representation */
	UPROPERTY(transient)
	TObjectPtr<UEdGraphNode>	GraphNode;

	/** If exists, expresssion containing this expression within its subgraph. */
	UPROPERTY()
	TObjectPtr<UMaterialExpression> SubgraphExpression;

	/** Text of last error for this expression */
	FString LastErrorText;

	/** GUID to uniquely identify this node, to help the tutorials out */
	UPROPERTY()
	FGuid MaterialExpressionGuid;

	/** 
	 * The material that this expression is currently being compiled in.  
	 * This is not necessarily the object which owns this expression, for example a preview material compiling a material function's expressions.
	 */
	UPROPERTY()
	TObjectPtr<class UMaterial> Material;

	/** 
	 * The material function that this expression is being used with, if any.
	 * This will be NULL if the expression belongs to a function that is currently being edited, 
	 */
	UPROPERTY()
	TObjectPtr<class UMaterialFunction> Function;

	/** A description that level designers can add (shows in the material editor UI). */
	UPROPERTY(EditAnywhere, Category=MaterialExpression, meta=(MultiLine=true, DisplayAfter = "SortPriority"))
	FString Desc;

	/** Number of expressions connected to this expression's execution input */
	int32 NumExecutionInputs;

	/** Set to true by RecursiveUpdateRealtimePreview() if the expression's preview needs to be updated in realtime in the material editor. */
	UPROPERTY()
	uint32 bRealtimePreview:1;

	/** If true, we should update the preview next render. This is set when changing bRealtimePreview. */
	UPROPERTY(transient)
	uint32 bNeedToUpdatePreview:1;

	/** Indicates that this is a 'parameter' type of expression and should always be loaded (ie not cooked away) because we might want the default parameter. */
	UPROPERTY()
	uint8 bIsParameterExpression : 1;

	/** If true, the comment bubble will be visible in the graph editor */
	UPROPERTY()
	uint32 bCommentBubbleVisible:1;

	/** If true, use the output name as the label for the pin */
	UPROPERTY()
	uint32 bShowOutputNameOnPin:1;

	/** If true, changes the pin color to match the output mask */
	UPROPERTY()
	uint32 bShowMaskColorsOnPin:1;

	/** If true, do not render the preview window for the expression */
	UPROPERTY()
	uint32 bHidePreviewWindow:1;

	/** If true, show a collapsed version of the node */
	UPROPERTY()
	uint32 bCollapsed:1;

	/** Whether the node represents an input to the shader or not.  Used to color the node's background. */
	UPROPERTY()
	uint32 bShaderInputData:1;

	/** Whether to draw the expression's inputs. */
	UPROPERTY()
	uint32 bShowInputs:1;

	/** Whether to draw the expression's outputs. */
	UPROPERTY()
	uint32 bShowOutputs:1;

	/** Localized categories to sort this expression into... */
	UPROPERTY()
	TArray<FText> MenuCategories;

	/** The expression's outputs, which are set in default properties by derived classes. */
	UPROPERTY()
	TArray<FExpressionOutput> Outputs;

	//~ Begin UObject Interface.
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditImport() override;
	virtual bool CanEditChange( const FProperty* InProperty ) const override;
	
	virtual bool Modify( bool bAlwaysMarkDirty=true ) override;
#endif // WITH_EDITOR
	virtual void Serialize( FStructuredArchive::FRecord Record ) override;
	//~ End UObject Interface.

	UObject* GetAssetOwner() const;
	FString GetAssetPathName() const;

#if WITH_EDITOR
	/**
	 * Create the new shader code chunk needed for the Abs expression
	 *
	 * @param	Compiler - UMaterial compiler that knows how to handle this expression.
	 * @return	Index to the new FMaterialCompiler::CodeChunk entry for this expression
	 */	
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) { return INDEX_NONE; }
	virtual int32 CompilePreview(class FMaterialCompiler* Compiler, int32 OutputIndex) { return Compile(Compiler, OutputIndex); }

	/**
	 * A given UMaterial implementation should implement at least one of these methods in order to generate HLSL code
	 * It's valid to implement more than one, if the expression can be used in multiple ways.
	 * For example, a for-loop expression might generate a statement for the execution input, but also generate an expression to access the loop index
	 * These methods replace the Compile() method; once we switch over to the new system, Compile() will be removed
	 */
	virtual bool GenerateHLSLStatements(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope) const;
	virtual bool GenerateHLSLExpression(FMaterialHLSLGenerator& Generator, UE::HLSLTree::FScope& Scope, int32 OutputIndex, UE::HLSLTree::FExpression const*& OutExpression) const;

	bool IsUsingNewHLSLGenerator() const;

#endif // WITH_EDITOR

	/**
	* Fill the array with all textures dependence that should trig a recompile of the material.
	*/
	virtual void GetTexturesForceMaterialRecompile(TArray<UTexture *> &Textures) const { }

	/** 
	 * Callback to get any texture reference this expression emits.
	 * This is used to link the compiled uniform expressions with their default texture values. 
	 * Any UMaterialExpression whose compilation creates a texture uniform expression (eg Compiler->Texture, Compiler->TextureParameter) must implement this.
	 */
	virtual UObject* GetReferencedTexture() const { return nullptr; }
	/** Returns true if GetReferencedTexture() can ever return a valid pointer. */
	virtual bool CanReferenceTexture() const { return false; }

#if WITH_EDITOR
	/**
	 *	Get the outputs supported by this expression.
	 *
	 *	@param	Outputs		The TArray of outputs to fill in.
	 */
	virtual TArray<FExpressionOutput>& GetOutputs();
	/** Get the expression inputs supported by this expression (Note: property inputs NOT included). */
	virtual const TArray<FExpressionInput*> GetInputs();
	virtual FExpressionInput* GetInput(int32 InputIndex);
	virtual FName GetInputName(int32 InputIndex) const;
	virtual bool IsInputConnectionRequired(int32 InputIndex) const;
	virtual bool CanUserDeleteExpression() const
	{
		return true;
	};

	/** Find the property that is associated with the input pin. */
	virtual TArray<FProperty*> GetInputPinProperty(int32 PinIndex);
	virtual FName GetInputPinSubCategory(int32 PinIndex);
	virtual UObject* GetInputPinSubCategoryObject(int32 PinIndex);
	virtual void PinDefaultValueChanged(int32 PinIndex, const FString& DefaultValue);
	virtual FString GetInputPinDefaultValue(int32 PinIndex);
	virtual TArray<FProperty*> GetPropertyInputs() const;

	virtual uint32 GetInputType(int32 InputIndex);
	virtual uint32 GetOutputType(int32 OutputIndex);

	virtual void GetExecOutputs(TArray<FExpressionExecOutputEntry>& Outputs);

	virtual bool HasExecInput();

	virtual FText GetCreationDescription() const;
	virtual FText GetCreationName() const;

	/**
	 *	Get the width required by this expression (in the material editor).
	 *
	 *	@return	int32			The width in pixels.
	 */
	virtual int32 GetWidth() const;
	virtual int32 GetHeight() const;
	virtual bool UsesLeftGutter() const;
	virtual bool UsesRightGutter() const;

	/**
	 *	Returns the text to display on the material expression (in the material editor).
	 *
	 *	@return	FString		The text to display.
	 */
	virtual void GetCaption(TArray<FString>& OutCaptions) const;
	/** Get a single line description of the material expression (used for lists) */
	virtual FString GetDescription() const;
	/** Get a tooltip for the specified connector. */
	virtual void GetConnectorToolTip(int32 InputIndex, int32 OutputIndex, TArray<FString>& OutToolTip);

	/** Get a tooltip for the expression itself. */
	virtual void GetExpressionToolTip(TArray<FString>& OutToolTip);
	/**
	 *	Returns the amount of padding to use for the label.
	 *
	 *	@return int32			The padding (in pixels).
	 */
	virtual int GetLabelPadding() { return 0; }
	virtual int32 CompilerError(class FMaterialCompiler* Compiler, const TCHAR* pcMessage);
#endif // WITH_EDITOR

	/**
	 * @return whether the expression preview needs realtime update
	 */
#if WITH_EDITOR
	virtual bool NeedsRealtimePreview() { return false; }

	/**
	 * @return text overlaid over the preview in the material editor
	 */
	virtual FText GetPreviewOverlayText() const { return FText(); }

	/**
	 * MatchesSearchQuery: Check this expression to see if it matches the search query
	 * @param SearchQuery - User's search query (never blank)
	 * @return true if the expression matches the search query
	 */
	virtual bool MatchesSearchQuery( const TCHAR* SearchQuery );

	/**
	 * Copy the SrcExpressions into the specified material, preserving internal references.
	 * New material expressions are created within the specified material.
	 */
	static void CopyMaterialExpressions(const TArray<class UMaterialExpression*>& SrcExpressions, const TArray<class UMaterialExpressionComment*>& SrcExpressionComments, 
		class UMaterial* Material, class UMaterialFunction* Function, TArray<class UMaterialExpression*>& OutNewExpressions, TArray<class UMaterialExpression*>& OutNewComments);

	/**
	 * Marks certain expression types as outputting material attributes. Allows the material editor preview material to know if it should use its material attributes pin.
	 */
	virtual bool IsResultMaterialAttributes(int32 OutputIndex) { return false; }

	/**
	 * Marks certain expression types as outputting Strata material. Allows the material functions to directly return a Strata material as output pin.
	 */
	virtual bool IsResultStrataMaterial(int32 OutputIndex) { return false; }

	/**
	 * Recursively parse nodes outputing strata material in order to gather all the possible shading models used in a material graph output a Strata material.
	 */
	virtual void GatherStrataMaterialInfo(FStrataMaterialInfo& StrataMaterialInfo, int32 OutputIndex) { }

	/**
	 * A starta material is a tree with FrontMateiral being its root and BSDF being leaves, with operators in the middle.
	 * This recursively parse nodes outputing strata material in order to gather the maximum distance to any leaves. 
	 * This is used to drive the bottom up order processing of those nodes.
	 */
	virtual FStrataOperator* StrataGenerateMaterialTopologyTree(class FMaterialCompiler* Compiler, class UMaterialExpression* Parent, int32 OutputIndex);

	/**
	 * If true, discards the output index when caching this expression which allows more cases to re-use the output instead of adding a separate instruction
	 */
	virtual bool CanIgnoreOutputIndex() { return false; }

	/**
	 * Connects the specified output to the passed material for previewing. 
	 */
	void ConnectToPreviewMaterial(UMaterial* InMaterial, int32 OutputIndex);
#endif // WITH_EDITOR

#if WITH_EDITOR
	/**
	 * Check if input exppresion is directly connected to the material.
	 */
	virtual bool IsExpressionConnected(FExpressionInput* Input, int32 OutputIndex);

	/**
	 * Connects the specified input expression to the specified output of this expression.
	 */
	virtual void ConnectExpression(FExpressionInput* Input, int32 OutputIndex);
#endif // WITH_EDITOR

	/** 
	* Generates a GUID for the parameter expression if one doesn't already exist and we are one.
	*
	* @param bForceGeneration	Whether we should generate a GUID even if it is already valid.
	*/
	void UpdateParameterGuid(bool bForceGeneration, bool bAllowMarkingPackageDirty);

	/** Callback to access derived classes' parameter expression id. */
	virtual FGuid& GetParameterExpressionId()
	{
		checkf(!bIsParameterExpression, TEXT("Expressions with bIsParameterExpression==true must implement their own GetParameterExpressionId!"));
		static FGuid Dummy;
		return Dummy;
	}

	/**
	* Generates a GUID for this expression if one doesn't already exist.
	*
	* @param bForceGeneration	Whether we should generate a GUID even if it is already valid.
	*/
	void UpdateMaterialExpressionGuid(bool bForceGeneration, bool bAllowMarkingPackageDirty);
	
	/** Return the material expression guid. */
	virtual FGuid& GetMaterialExpressionId()
	{		
#if WITH_EDITORONLY_DATA
		return MaterialExpressionGuid;
#else
		static FGuid Dummy;
		return Dummy;
#endif
	}

	/** Asserts if the expression is not contained by its Material or Function's expressions array. */
	void ValidateState();

#if WITH_EDITOR
	/** Returns the keywords that should be used when searching for this expression */
	virtual FText GetKeywords() const {return FText::GetEmpty();}

	/**
	 * Recursively gets a list of all expressions that are connected to this
	 * Checks for repeats so that it can't end up in an infinite loop
	 *
	 * @param InputExpressions Array to contain/pass on expressions
	 *
	 * @return Whether a repeat was found while getting expressions
	 */
	bool GetAllInputExpressions(TArray<UMaterialExpression*>& InputExpressions);

	/**
	 * Can this node be renamed?
	 */
	virtual bool CanRenameNode() const;

	/**
	 * Returns the current 'name' of the node (typically a parameter name).
	 * Only valid to call on a node that previously returned CanRenameNode() = true.
	 */
	virtual FString GetEditableName() const;

	/**
	 * Sets the current 'name' of the node (typically a parameter name)
	 * Only valid to call on a node that previously returned CanRenameNode() = true.
	 */
	virtual void SetEditableName(const FString& NewName);

	/** 
	* Parameter Name functions, this is requires as multiple class have ParameterName
	* but are not UMaterialExpressionParameter due to class hierarchy. */
	virtual bool HasAParameterName() const { return false; }
	virtual void ValidateParameterName(const bool bAllowDuplicateName = true);
	virtual bool HasClassAndNameCollision(UMaterialExpression* OtherExpression) const;

	EMaterialParameterType GetParameterType() const;

	virtual FName GetParameterName() const { return NAME_None; }
	virtual void SetParameterName(const FName& Name) {}
	virtual bool GetParameterValue(FMaterialParameterMetadata& OutMeta) const { return false; }
	virtual bool SetParameterValue(const FName& Name, const FMaterialParameterMetadata& Meta, EMaterialExpressionSetParameterValueFlags Flags = EMaterialExpressionSetParameterValueFlags::None) { return false; }

	virtual void GetLandscapeLayerNames(TArray<FName>& OutLayers) const {}

	/**
	 * Called after a node copy, once the Material and Function properties are set correctly and that all new expressions are added to Material->Expressions
	 * @param	CopiedExpressions	The expressions copied in this copy
	 */
	virtual void PostCopyNode(const TArray<UMaterialExpression*>& CopiedExpressions) {}

	virtual bool HasConnectedOutputs() const;

	/** Checks whether any inputs to this expression create a loop */
	bool ContainsInputLoop(const bool bStopOnFunctionCall = true);

protected:
	/**
	 * Checks whether any inputs to this expression create a loop by recursively
	 * calling itself and keeping a list of inputs as expression keys.
	 *
	 * @param ExpressionStack    List of expression keys that have been checked already in the current stack
	 * @param VisitedExpressions List of all expression keys that have been visited
	 */
	bool ContainsInputLoopInternal(TArray<class FMaterialExpressionKey>& ExpressionStack, TSet<class FMaterialExpressionKey>& VisitedExpressions, const bool bStopOnFunctionCall);
#endif // WITH_EDITOR
};


