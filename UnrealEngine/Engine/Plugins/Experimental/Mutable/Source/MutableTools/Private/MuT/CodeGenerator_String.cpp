// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/PlatformCrt.h"
#include "HAL/UnrealMemory.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Operations.h"
#include "MuR/Parameters.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/AST.h"
#include "MuT/ASTOpConstantString.h"
#include "MuT/ASTOpParameter.h"
#include "MuT/CodeGenerator.h"
#include "MuT/NodeRange.h"
#include "MuT/NodeString.h"
#include "MuT/NodeStringConstant.h"
#include "MuT/NodeStringConstantPrivate.h"
#include "MuT/NodeStringParameter.h"
#include "MuT/NodeStringParameterPrivate.h"

namespace mu
{


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateString(FStringGenerationResult& result, const FGenericGenerationOptions& Options, const Ptr<const NodeString>& Untyped)
	{
		if (!Untyped)
		{
			result = FStringGenerationResult();
			return;
		}

		// See if it was already generated
		FGeneratedCacheKey Key;
		Key.Node = Untyped;
		Key.Options = Options;
		FGeneratedStringsMap::ValueType* it = GeneratedStrings.Find(Key);
		if (it)
		{
			result = *it;
			return;
		}

		// Generate for each different type of node
		if (Untyped->GetType()==NodeStringConstant::GetStaticType())
		{
			GenerateString_Constant(result, Options, static_cast<const NodeStringConstant*>(Untyped.get()));
		}
		else if (Untyped->GetType() == NodeStringParameter::GetStaticType())
		{
			GenerateString_Parameter(result, Options, static_cast<const NodeStringParameter*>(Untyped.get()));
		}
		else
		{
			check(false);
		}

		// Cache the result
		GeneratedStrings.Add(Key, result);
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateString_Constant(FStringGenerationResult& result, const FGenericGenerationOptions& Options, const Ptr<const NodeStringConstant>& Typed)
	{
		const NodeStringConstant::Private& node = *Typed->GetPrivate();

		Ptr<ASTOpConstantString> op = new ASTOpConstantString();
		op->value = node.m_value;

		result.op = op;
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateString_Parameter(FStringGenerationResult& result, const FGenericGenerationOptions& Options, const Ptr<const NodeStringParameter>& Typed)
	{
		const NodeStringParameter::Private& node = *Typed->GetPrivate();

		Ptr<ASTOpParameter> op;

		Ptr<ASTOpParameter>* it = m_firstPass.ParameterNodes.Find(node.m_pNode);
		if (!it)
		{
			FParameterDesc param;
			param.m_name = node.m_name;
			const TCHAR* CStr = ToCStr(node.m_uid);
			param.m_uid.ImportTextItem(CStr, 0, nullptr, nullptr);
			param.m_type = PARAMETER_TYPE::T_FLOAT;
			param.m_defaultValue.Set<ParamStringType>(node.m_defaultValue);

			op = new ASTOpParameter();
			op->type = OP_TYPE::ST_PARAMETER;
			op->parameter = param;

			// Generate the code for the ranges
			for (int32 a = 0; a < node.m_ranges.Num(); ++a)
			{
				FRangeGenerationResult rangeResult;
				GenerateRange(rangeResult, Options, node.m_ranges[a]);
				op->ranges.Emplace(op.get(), rangeResult.sizeOp, rangeResult.rangeName, rangeResult.rangeUID);
			}

			m_firstPass.ParameterNodes.Add(node.m_pNode, op);
		}
		else
		{
			op = *it;
		}

		result.op = op;
	}


}
