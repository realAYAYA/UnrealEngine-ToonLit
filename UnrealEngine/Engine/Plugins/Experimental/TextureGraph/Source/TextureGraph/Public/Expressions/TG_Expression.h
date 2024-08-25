// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TG_Node.h"
#include "TG_Var.h"
#include "TG_GraphEvaluation.h"

#include "TG_Expression.generated.h"

#if WITH_EDITOR
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTSExpressionChanged, UTG_Expression*);
#endif

class TEXTUREGRAPH_API TG_Category
{
public:
	static const FName Default;
	static const FName Output;
	static const FName Input;
	static const FName Adjustment;
	static const FName Channel;
	static const FName DevOnly;
	static const FName Procedural;
	static const FName Maths;
	static const FName Utilities;
	static const FName Filter;
	static const FName Custom;
};

UCLASS(abstract)
class TEXTUREGRAPH_API UTG_Expression : public UObject
{
	GENERATED_BODY()

protected:
	// UTG_Expression classes have a per instance expression class version number recovered from serialization
	UPROPERTY() // Only to retreive the version of a loaded expression and compare it against the 
	int32 InstanceExpressionClassVersion = 0;

	// Method returning this Expression class version that can be overwritten in a sub class requiring to
	// support version change
	virtual int32		GetExpressionClassVersion() const { return 0; }

public:
	// Override serialization to save Class Serialization version
	virtual	void		Serialize(FArchive& Ar) override;


	virtual FTG_Name GetDefaultName() const;
	virtual FName GetCategory() const { return TG_Category::Default; }
	virtual FTG_SignaturePtr GetSignature() const { return nullptr; }
	virtual FText GetTooltipText() const { return FText::FromString(TEXT("Texture Scripting Node")); } 

	virtual void SetTitleName(FName NewName) {}
	virtual FName GetTitleName() const { return GetDefaultName(); }
	virtual bool CanRenameTitle() const;

	virtual bool CanHandleAsset(UObject* Asset) { return false; };
	virtual void SetAsset(UObject* Asset) { check(CanHandleAsset((Asset))); };

	// This is THE evaluation call to overwrite
	virtual void Evaluate(FTG_EvaluationContext* InContext) {}

#if WITH_EDITOR
	void PropertyChangeTriggered(FProperty* Property, EPropertyChangeType::Type ChangeType);
	virtual bool CanEditChange( const FProperty* InProperty ) const override;	
#endif
	
	// Validate internal checks, warnings and errors
	virtual bool Validate(MixUpdateCyclePtr	Cycle) { return true; }

	// Validate that the expression conforms to a conformant function (e.g. Clamp etc.)
	void ValidateGenerateConformer(UTG_Pin* InPin);

	// Expression notifies its parent node on key events
	virtual	UTG_Node* GetParentNode() const final;
protected:
	friend class UTG_Graph;
	friend class UTG_Node;
	friend struct FTG_Var;
	friend struct FTG_Evaluation;

	// Initialize the expression in cascade from the node allowing it to re create transient data
	// This is called in the PostLoad of the Graph
	virtual void Initialize() {}

	// Called first from the Graph::Evaluate/Traverse which then call the Evaluate function.
	// This is where the Var values are copied over to the matching Expression's properties
	// This is NOT the function you want to derive, unless you know exactly what you are doing.
	// Instead overwrite the Execute function.
	virtual void SetupAndEvaluate(FTG_EvaluationContext* InContext);

	virtual void CopyVarToExpressionArgument(const FTG_Argument& Arg, FTG_Var* InVar);
	virtual void CopyVarFromExpressionArgument(const FTG_Argument& Arg, FTG_Var* InVar);
	virtual void CopyVarGeneric(const FTG_Argument& Arg, FTG_Var* InVar, bool CopyVarToArg) {}

	// Log the actual values for vars and the expression evaluation
	// Called from SetupAndEvaluate call if Context ask for it
	virtual void LogEvaluation(FTG_EvaluationContext* InContext);

	virtual FTG_Signature::FInit GetSignatureInitArgsFromClass() const final;


	// Build the signature of the expression by collecting the FTG_ExpressionXXX UProperties of the class
	virtual FTG_SignaturePtr BuildSignatureFromClass() const final;

	// Build Signature in derived classes dynamically
	virtual FTG_SignaturePtr BuildSignatureDynamically() const { return nullptr; }


public:
	// If some state has changed in the expression that affects its representation
	// triggered when a property has changed and needs to be copied over to its corresponding Var
	// NB: required to be public for calling from TG_Variant customization
	virtual void NotifyExpressionChanged(const FPropertyChangedEvent& PropertyChangedEvent) const final;
protected:

	// If the signature changes and the node need to regenerate its own signature.
	// Only concrete implementation for Dynamic Expression
	virtual void NotifySignatureChanged() const;


	virtual FTG_Variant::EType GetCommonVariantType() const { return FTG_Variant::EType::Scalar; }
	virtual bool AssignCommonVariantType(FTG_Variant::EType InType) const { return false; }
	virtual void NotifyCommonVariantTypeChanged(FTG_Variant::EType NewType) const {}
	FTG_Variant::EType EvalExpressionCommonVariantType() const { return GetParentNode()->EvalExpressionCommonVariantType(); }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
#endif
};



#define TG_DECLARE_EXPRESSION(Category) \
		FTG_SignaturePtr GetSignature() const override { static FTG_SignaturePtr Signature = BuildSignatureFromClass(); return Signature; } \
		virtual FName GetCategory() const override { return Category; } 

#define TG_DECLARE_DYNAMIC_EXPRESSION(Category) \
	protected: mutable FTG_SignaturePtr DynSignature = nullptr; \
	virtual FTG_SignaturePtr BuildSignatureDynamically() const override; \
	virtual void NotifySignatureChanged() const final { DynSignature.Reset(); /*Where the DynSynature is reset!*/ Super::NotifySignatureChanged(); } \
    public: FTG_SignaturePtr GetSignature() const override { if (!DynSignature) DynSignature = BuildSignatureDynamically(); return DynSignature; } \
	public: virtual FName GetCategory() const override { return Category; } 

#define TG_DECLARE_VARIANT_EXPRESSION(Category) \
	protected: mutable FTG_Variant::EType CommonVariantType = FTG_Variant::EType::Scalar; \
	virtual void Initialize() override { \
		Super::Initialize(); \
		CommonVariantType = EvalExpressionCommonVariantType(); \
	} \
	virtual FTG_Variant::EType GetCommonVariantType() const final { return CommonVariantType; } \
	virtual bool AssignCommonVariantType(FTG_Variant::EType InType) const final { \
		if (CommonVariantType != InType) { \
			CommonVariantType = InType; \
			return true; \
		} \
		return false; \
	} \
	virtual void NotifyCommonVariantTypeChanged(FTG_Variant::EType InType) const final { \
		if (AssignCommonVariantType(InType)) \
			NotifySignatureChanged(); \
	} \
	protected: mutable FTG_SignaturePtr DynSignature = nullptr; \
	virtual FTG_SignaturePtr BuildSignatureDynamically() const final { \
		FTG_Signature::FInit SignatureInit = GetSignatureInitArgsFromClass(); \
		for (auto& a : SignatureInit.Arguments) { \
			if (a.IsOutput()) { \
				a.CPPTypeName = FTG_Variant::GetArgNameFromType(CommonVariantType); \
			} \
		} \
		return MakeShared<FTG_Signature>(SignatureInit); \
	} \
	virtual void NotifySignatureChanged() const final { DynSignature.Reset(); /*Where the DynSynature is reset!*/ Super::NotifySignatureChanged(); } \
	public: FTG_SignaturePtr GetSignature() const override { if (!DynSignature) DynSignature = BuildSignatureDynamically(); return DynSignature; } \
	public: virtual FName GetCategory() const override { return Category; } 
	
	

UCLASS(Hidden)
class TEXTUREGRAPH_API UTG_Expression_Null : public UTG_Expression
{
	GENERATED_BODY()

protected:
	TG_DECLARE_EXPRESSION(TG_Category::Default);
};

