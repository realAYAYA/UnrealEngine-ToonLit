// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuT/AST.h"


namespace mu
{
	struct FProgram;

	/** Add tags to a mesh. */
	class ASTOpMeshAddTags final : public ASTOp
	{
	public:

		/** Source mesh to add tags to. */
		ASTChild Source;

		/** Tags to add. */
		TArray<FString> Tags;

	public:

		ASTOpMeshAddTags();
		ASTOpMeshAddTags(const ASTOpMeshAddTags&) = delete;
		virtual ~ASTOpMeshAddTags() override;

		// ASTOp interface
		virtual OP_TYPE GetOpType() const override { return OP_TYPE::ME_ADDTAGS; }
		virtual uint64 Hash() const override;
		virtual void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		virtual bool IsEqual(const ASTOp& otherUntyped) const override;
		virtual Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		virtual void Link(FProgram& program, FLinkerOptions*) override;
	};

}

