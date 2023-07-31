// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuT/AST.h"


namespace mu
{
struct PROGRAM;


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	class ASTOpMeshGeometryOperation : public ASTOp
	{
	public:

		ASTChild meshA;
		ASTChild meshB;
		ASTChild scalarA;
		ASTChild scalarB;

	public:

		ASTOpMeshGeometryOperation();
		ASTOpMeshGeometryOperation(const ASTOpMeshGeometryOperation&) = delete;
		~ASTOpMeshGeometryOperation();

		OP_TYPE GetOpType() const override { return OP_TYPE::ME_GEOMETRYOPERATION; }
		uint64 Hash() const override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		void Link(PROGRAM& program, const FLinkerOptions* Options) override;
	};


}

