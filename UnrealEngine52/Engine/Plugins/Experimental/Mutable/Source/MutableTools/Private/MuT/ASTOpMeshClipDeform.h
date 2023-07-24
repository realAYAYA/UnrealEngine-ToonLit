// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuT/AST.h"


namespace mu
{
struct FProgram;


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	class ASTOpMeshClipDeform : public ASTOp
	{
	public:

		ASTChild Mesh;
		ASTChild ClipShape;

	public:

		ASTOpMeshClipDeform();
		ASTOpMeshClipDeform(const ASTOpMeshClipDeform&) = delete;
		~ASTOpMeshClipDeform();

		OP_TYPE GetOpType() const override { return OP_TYPE::ME_CLIPDEFORM; }
		uint64 Hash() const override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		void Link(FProgram& program, const FLinkerOptions* Options) override;
	};


}

