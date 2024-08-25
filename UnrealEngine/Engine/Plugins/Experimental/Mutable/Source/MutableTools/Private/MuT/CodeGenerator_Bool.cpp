// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/PlatformCrt.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Operations.h"
#include "MuR/Parameters.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/AST.h"
#include "MuT/ASTOpConstantBool.h"
#include "MuT/ASTOpParameter.h"
#include "MuT/CodeGenerator.h"
#include "MuT/NodeBool.h"
#include "MuT/NodeBoolPrivate.h"
#include "MuT/NodeRange.h"


namespace mu
{

	void CodeGenerator::GenerateBool(FBoolGenerationResult& Result, const FGenericGenerationOptions& Options, const Ptr<const NodeBool>& Untyped)
	{
		if (!Untyped)
		{
			Result = FBoolGenerationResult();
			return;
		}

		// See if it was already generated
		FGeneratedCacheKey Key;
		Key.Node = Untyped;
		Key.Options = Options;
		FGeneratedBoolsMap::ValueType* it = GeneratedBools.Find(Key);
		if (it)
		{
			Result = *it;
			return;
		}

		// Generate for each different type of node
		if (Untyped->GetType()==NodeBoolConstant::GetStaticType())
		{
			const NodeBoolConstant* Constant = static_cast<const NodeBoolConstant*>(Untyped.get());
			GenerateBool_Constant(Result, Options, Constant);
		}
		else if (Untyped->GetType() == NodeBoolParameter::GetStaticType())
		{
			const NodeBoolParameter* Param = static_cast<const NodeBoolParameter*>(Untyped.get());
			GenerateBool_Parameter(Result, Options, Param);
		}
		else if (Untyped->GetType() == NodeBoolNot::GetStaticType())
		{
			const NodeBoolNot* Sample = static_cast<const NodeBoolNot*>(Untyped.get());
			GenerateBool_Not(Result, Options, Sample);
		}
		else if (Untyped->GetType() == NodeBoolAnd::GetStaticType())
		{
			const NodeBoolAnd* From = static_cast<const NodeBoolAnd*>(Untyped.get());
			GenerateBool_And(Result, Options, From);
		}
		else
		{
			check(false);
		}

		// Cache the result
		GeneratedBools.Add(Key, Result);
	}


	void CodeGenerator::GenerateBool_Constant(FBoolGenerationResult& result, const FGenericGenerationOptions& Options, const Ptr<const NodeBoolConstant>& Typed)
	{
		const NodeBoolConstant::Private& node = *Typed->GetPrivate();

		Ptr<ASTOpConstantBool> op = new ASTOpConstantBool();
		op->value = node.m_value;

		result.op = op;
	}


	void CodeGenerator::GenerateBool_Parameter(FBoolGenerationResult& result, const FGenericGenerationOptions& Options, const Ptr<const NodeBoolParameter>& Typed)
	{
		const NodeBoolParameter::Private& node = *Typed->GetPrivate();

		Ptr<ASTOpParameter> op;

		Ptr<ASTOpParameter>* it = m_firstPass.ParameterNodes.Find(node.m_pNode);
		if (!it)
		{
			FParameterDesc param;
			param.m_name = node.m_name;
			const TCHAR* CStr = ToCStr(node.m_uid);
			param.m_uid.ImportTextItem(CStr, 0, nullptr, nullptr);
			param.m_type = PARAMETER_TYPE::T_BOOL;
			param.m_defaultValue.Set<ParamBoolType>(node.m_defaultValue);

			op = new ASTOpParameter();
			op->type = OP_TYPE::BO_PARAMETER;
			op->parameter = param;

			// Generate the code for the ranges
			for (int32 a = 0; a < node.m_ranges.Num(); ++a)
			{
				FRangeGenerationResult rangeResult;
				GenerateRange(rangeResult, Options, node.m_ranges[a]);
				op->ranges.Emplace(op.get(), rangeResult.sizeOp, rangeResult.rangeName, rangeResult.rangeUID);
			}

			m_firstPass.ParameterNodes.Add(node.m_pNode,op);
		}
		else
		{
			op = *it;
		}

		result.op = op;
	}


	void CodeGenerator::GenerateBool_Not(FBoolGenerationResult& result, const FGenericGenerationOptions& Options, const Ptr<const NodeBoolNot>& Typed)
	{
		const NodeBoolNot::Private& node = *Typed->GetPrivate();

		Ptr<ASTOpFixed> op = new ASTOpFixed();
		op->op.type = OP_TYPE::BO_NOT;

		FBoolGenerationResult ChildResult;
		GenerateBool(ChildResult, Options, node.m_pSource);
		op->SetChild(op->op.args.BoolNot.source, ChildResult.op);

		result.op = op;
	}


	void CodeGenerator::GenerateBool_And(FBoolGenerationResult& result, const FGenericGenerationOptions& Options, const Ptr<const NodeBoolAnd>& Typed)
	{
		const NodeBoolAnd::Private& node = *Typed->GetPrivate();

		Ptr<ASTOpFixed> op = new ASTOpFixed();
		op->op.type = OP_TYPE::BO_AND;

		FBoolGenerationResult ChildResult;
		GenerateBool(ChildResult, Options, node.m_pA);
		op->SetChild(op->op.args.BoolBinary.a, ChildResult.op);

		GenerateBool(ChildResult, Options, node.m_pB);
		op->SetChild(op->op.args.BoolBinary.b, ChildResult.op);

		result.op = op;
	}


}
