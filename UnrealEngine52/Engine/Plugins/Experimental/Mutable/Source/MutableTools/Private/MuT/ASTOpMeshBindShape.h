// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
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
	class ASTOpMeshBindShape : public ASTOp
	{
	public:

		ASTChild Mesh;
		ASTChild Shape;

		TArray<string> BonesToDeform;
		TArray<string> PhysicsToDeform;

		uint32 BindingMethod = 0;
		
		uint32 bReshapeSkeleton	      : 1;
		uint32 bEnableRigidParts      : 1;
		uint32 bReshapePhysicsVolumes : 1;
		uint32 bReshapeVertices       : 1;

	public:

		ASTOpMeshBindShape();
		ASTOpMeshBindShape(const ASTOpMeshBindShape&) = delete;
		~ASTOpMeshBindShape();

		OP_TYPE GetOpType() const override { return OP_TYPE::ME_BINDSHAPE; }
		uint64 Hash() const override;
		bool IsEqual(const ASTOp& otherUntyped) const override;
		Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		void Link(FProgram& program, const FLinkerOptions* Options) override;
		Ptr<ASTOp> OptimiseSink(const FModelOptimizationOptions&, FOptimizeSinkContext&) const override;

	};

}

