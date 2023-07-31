// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/MutableMath.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuT/AST.h"


namespace mu
{
struct PROGRAM;


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	class ASTOpMeshTransform : public ASTOp
	{
	public:

		ASTChild source;

		mat4f matrix;

	public:

		ASTOpMeshTransform();
		ASTOpMeshTransform(const ASTOpMeshTransform&) = delete;
		~ASTOpMeshTransform();

		OP_TYPE GetOpType() const override { return OP_TYPE::ME_TRANSFORM; }
		uint64 Hash() const override;
		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		void Link(PROGRAM& program, const FLinkerOptions* Options) override;

	};


}

