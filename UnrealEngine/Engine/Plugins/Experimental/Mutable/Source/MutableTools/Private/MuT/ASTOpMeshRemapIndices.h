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
	//! \TODO: Deprecated?
	//---------------------------------------------------------------------------------------------
	class ASTOpMeshRemapIndices : public ASTOp
	{
	public:

		//! Mesh that will have the vertex indices remapped.
		ASTChild source;

		//! Mesh used to obtain the final vertex indices.
		ASTChild reference;

	public:

		ASTOpMeshRemapIndices();
		ASTOpMeshRemapIndices(const ASTOpMeshRemapIndices&) = delete;
		~ASTOpMeshRemapIndices() override;

		// ASTOp interface
		OP_TYPE GetOpType() const override { return OP_TYPE::ME_REMAPINDICES; }
		uint64 Hash() const override;
		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		void Link(PROGRAM& program, const FLinkerOptions*) override;
	};


}

