// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/PlatformMath.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Image.h"
#include "MuR/MutableMath.h"
#include "MuR/Operations.h"
#include "MuR/Parameters.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/Platform.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/AST.h"
#include "MuT/ASTOpConditional.h"
#include "MuT/ASTOpParameter.h"
#include "MuT/ASTOpScalarCurve.h"
#include "MuT/ASTOpSwitch.h"
#include "MuT/CodeGenerator.h"
#include "MuT/CodeGenerator_FirstPass.h"
#include "MuT/ErrorLog.h"
#include "MuT/ErrorLogPrivate.h"
#include "MuT/NodeImage.h"
#include "MuT/NodeMeshGeometryOperation.h"
#include "MuT/NodeMeshTable.h"
#include "MuT/NodeRange.h"
#include "MuT/NodeScalar.h"
#include "MuT/NodeScalarArithmeticOperation.h"
#include "MuT/NodeScalarArithmeticOperationPrivate.h"
#include "MuT/NodeScalarConstant.h"
#include "MuT/NodeScalarConstantPrivate.h"
#include "MuT/NodeScalarCurve.h"
#include "MuT/NodeScalarCurvePrivate.h"
#include "MuT/NodeScalarEnumParameter.h"
#include "MuT/NodeScalarEnumParameterPrivate.h"
#include "MuT/NodeScalarParameter.h"
#include "MuT/NodeScalarParameterPrivate.h"
#include "MuT/NodeScalarSwitch.h"
#include "MuT/NodeScalarSwitchPrivate.h"
#include "MuT/NodeScalarTable.h"
#include "MuT/NodeScalarTablePrivate.h"
#include "MuT/NodeScalarVariation.h"
#include "MuT/NodeScalarVariationPrivate.h"
#include "MuT/Table.h"
#include "MuT/TablePrivate.h"


namespace mu
{

	void CodeGenerator::GenerateScalar(FScalarGenerationResult& result, const FGenericGenerationOptions& Options, const Ptr<const NodeScalar>& Untyped)
	{
		if (!Untyped)
		{
			result = FScalarGenerationResult();
			return;
		}

		// See if it was already generated
		FGeneratedCacheKey Key;
		Key.Node = Untyped;
		Key.Options = Options;
		FGeneratedScalarsMap::ValueType* it = GeneratedScalars.Find(Key);
		if (it)
		{
			result = *it;
			return;
		}

		// Generate for each different type of node
		if (Untyped->GetType() == NodeScalarConstant::GetStaticType())
		{
			const NodeScalarConstant* Constant = static_cast<const NodeScalarConstant*>(Untyped.get());
			GenerateScalar_Constant(result, Options, Constant);
		}
		else if (Untyped->GetType() == NodeScalarParameter::GetStaticType())
		{
			const NodeScalarParameter* Param = static_cast<const NodeScalarParameter*>(Untyped.get());
			GenerateScalar_Parameter(result, Options, Param);
		}
		else if (Untyped->GetType() == NodeScalarSwitch::GetStaticType())
		{
			const NodeScalarSwitch* Switch = static_cast<const NodeScalarSwitch*>(Untyped.get());
			GenerateScalar_Switch(result, Options, Switch);
		}
		else if (Untyped->GetType() == NodeScalarEnumParameter::GetStaticType())
		{
			const NodeScalarEnumParameter* EnumParam = static_cast<const NodeScalarEnumParameter*>(Untyped.get());
			GenerateScalar_EnumParameter(result, Options, EnumParam);
		}
		else if (Untyped->GetType() == NodeScalarCurve::GetStaticType())
		{
			const NodeScalarCurve* Curve = static_cast<const NodeScalarCurve*>(Untyped.get());
			GenerateScalar_Curve(result, Options, Curve);
		}
		else if (Untyped->GetType() == NodeScalarArithmeticOperation::GetStaticType())
		{
			const NodeScalarArithmeticOperation* Arithmetic = static_cast<const NodeScalarArithmeticOperation*>(Untyped.get());
			GenerateScalar_Arithmetic(result, Options, Arithmetic);
		}
		else if (Untyped->GetType() == NodeScalarVariation::GetStaticType())
		{
			const NodeScalarVariation* Variation = static_cast<const NodeScalarVariation*>(Untyped.get());
			GenerateScalar_Variation(result, Options, Variation);
		}
		else if (Untyped->GetType() == NodeScalarTable::GetStaticType())
		{
			const NodeScalarTable* Table = static_cast<const NodeScalarTable*>(Untyped.get());
			GenerateScalar_Table(result, Options, Table);
		}
		else
		{
			check(false);
			return;
		}

		// Cache the result
		GeneratedScalars.Add(Key, result);
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateScalar_Constant(FScalarGenerationResult& result, const FGenericGenerationOptions& Options, const Ptr<const NodeScalarConstant>& Typed)
	{
		const NodeScalarConstant::Private& node = *Typed->GetPrivate();

		Ptr<ASTOpFixed> op = new ASTOpFixed();
		op->op.type = OP_TYPE::SC_CONSTANT;
		op->op.args.ScalarConstant.value = node.m_value;

		result.op = op;
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateScalar_Parameter(FScalarGenerationResult& result, const FGenericGenerationOptions& Options, const Ptr<const NodeScalarParameter>& Typed)
	{
		const NodeScalarParameter::Private& node = *Typed->GetPrivate();

		Ptr<ASTOpParameter> op;

		Ptr<ASTOpParameter>* it = m_firstPass.ParameterNodes.Find(node.m_pNode);
		if (!it)
		{
			FParameterDesc param;
			param.m_name = node.m_name;
			const TCHAR* CStr = ToCStr(node.m_uid);
			param.m_uid.ImportTextItem(CStr, 0, nullptr, nullptr);
			param.m_type = PARAMETER_TYPE::T_FLOAT;
			param.m_defaultValue.Set<ParamFloatType>(node.m_defaultValue);

			op = new ASTOpParameter();
			op->type = OP_TYPE::SC_PARAMETER;
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


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateScalar_EnumParameter(FScalarGenerationResult& result, const FGenericGenerationOptions& Options, const Ptr<const NodeScalarEnumParameter>& Typed)
	{
		const NodeScalarEnumParameter::Private& node = *Typed->GetPrivate();

		Ptr<ASTOpParameter> op;

		Ptr<ASTOpParameter>* it = m_firstPass.ParameterNodes.Find(node.m_pNode);
		if (!it)
		{
			FParameterDesc param;
			param.m_name = node.m_name;
			const TCHAR* CStr = ToCStr(node.m_uid);
			param.m_uid.ImportTextItem(CStr, 0, nullptr, nullptr);
			param.m_type = PARAMETER_TYPE::T_INT;
			param.m_defaultValue.Set<ParamIntType>(node.m_defaultValue);

			param.m_possibleValues.SetNum(node.m_options.Num());
			for (int32 i = 0; i < node.m_options.Num(); ++i)
			{
				param.m_possibleValues[i].m_value = (int16_t)node.m_options[i].value;
				param.m_possibleValues[i].m_name = node.m_options[i].name;
			}

			op = new ASTOpParameter();
			op->type = OP_TYPE::NU_PARAMETER;
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


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateScalar_Switch(FScalarGenerationResult& result, const FGenericGenerationOptions& Options, const Ptr<const NodeScalarSwitch>& Typed)
	{
		const NodeScalarSwitch::Private& node = *Typed->GetPrivate();

		if (node.m_options.Num() == 0)
		{
			// No options in the switch!
			Ptr<ASTOp> missingOp = GenerateMissingScalarCode(TEXT("Switch option"),
				1.0f,
				node.m_errorContext);
			result.op = missingOp;
			return;
		}

		Ptr<ASTOpSwitch> op = new ASTOpSwitch();
		op->type = OP_TYPE::SC_SWITCH;

		// Variable value
		if (node.m_pParameter)
		{
			FScalarGenerationResult ChildResult;
			GenerateScalar(ChildResult, Options, node.m_pParameter.get());
			op->variable = ChildResult.op;
		}
		else
		{
			// This argument is required
			op->variable = GenerateMissingScalarCode(TEXT("Switch variable"), 0.0f, node.m_errorContext);
		}

		// Options
		for (int32 t = 0; t < node.m_options.Num(); ++t)
		{
			Ptr<ASTOp> branch;
			if (node.m_options[t])
			{
				FScalarGenerationResult ChildResult;
				GenerateScalar(ChildResult, Options, node.m_options[t].get());
				branch = ChildResult.op;
			}
			else
			{
				// This argument is required
				branch = GenerateMissingScalarCode(TEXT("Switch option"), 1.0f, node.m_errorContext);
			}
			op->cases.Emplace((int16_t)t, op, branch);
		}

		result.op = op;
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateScalar_Variation(FScalarGenerationResult& result, const FGenericGenerationOptions& Options, const Ptr<const NodeScalarVariation>& Typed)
	{
		const NodeScalarVariation::Private& node = *Typed->GetPrivate();

		Ptr<ASTOp> op;

		// Default case
		if (node.m_defaultScalar)
		{
			FMeshGenerationResult branchResults;

			FScalarGenerationResult ChildResult;
			GenerateScalar(ChildResult, Options, node.m_defaultScalar);
			op = ChildResult.op;
		}

		// Process variations in reverse order, since conditionals are built bottom-up.
		for (int t = node.m_variations.Num() - 1; t >= 0; --t)
		{
			int tagIndex = -1;
			const FString& tag = node.m_variations[t].m_tag;
			for (int i = 0; i < m_firstPass.m_tags.Num(); ++i)
			{
				if (m_firstPass.m_tags[i].tag == tag)
				{
					tagIndex = i;
				}
			}

			if (tagIndex < 0)
			{
				FString Msg = FString::Printf(TEXT("Unknown tag found in image variation [%s]."), *tag);

				m_pErrorLog->GetPrivate()->Add(Msg, ELMT_WARNING, node.m_errorContext);
				continue;
			}

			Ptr<ASTOp> variationOp;
			if (node.m_variations[t].m_scalar)
			{
				FScalarGenerationResult ChildResult;
				GenerateScalar(ChildResult, Options, node.m_variations[t].m_scalar);
				variationOp = ChildResult.op;
			}
			else
			{
				// This argument is required
				variationOp = GenerateMissingScalarCode(TEXT("Variation option"), 0.0f, node.m_errorContext);
			}


			Ptr<ASTOpConditional> conditional = new ASTOpConditional;
			conditional->type = OP_TYPE::SC_CONDITIONAL;
			conditional->no = op;
			conditional->yes = variationOp;
			conditional->condition = m_firstPass.m_tags[tagIndex].genericCondition;

			op = conditional;
		}

		result.op = op;
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateScalar_Curve(FScalarGenerationResult& result, const FGenericGenerationOptions& Options, const Ptr<const NodeScalarCurve>& Typed)
	{
		const NodeScalarCurve::Private& node = *Typed->GetPrivate();

		Ptr<ASTOpScalarCurve> op = new ASTOpScalarCurve();

		// T
		if (Node* pA = node.m_input_scalar.get())
		{
			op->time = Generate(pA, Options);
		}
		else
		{
			op->time = CodeGenerator::GenerateMissingScalarCode(TEXT("Curve T"), 0.5f, node.m_errorContext);
		}

		op->curve = node.m_curve;

		result.op = op;
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateScalar_Arithmetic(FScalarGenerationResult& result, const FGenericGenerationOptions& Options, const Ptr<const NodeScalarArithmeticOperation>& Typed)
	{
		const NodeScalarArithmeticOperation::Private& node = *Typed->GetPrivate();

		Ptr<ASTOpFixed> op = new ASTOpFixed();
		op->op.type = OP_TYPE::SC_ARITHMETIC;

		switch (node.m_operation)
		{
		case NodeScalarArithmeticOperation::AO_ADD: op->op.args.ScalarArithmetic.operation = OP::ArithmeticArgs::ADD; break;
		case NodeScalarArithmeticOperation::AO_SUBTRACT: op->op.args.ScalarArithmetic.operation = OP::ArithmeticArgs::SUBTRACT; break;
		case NodeScalarArithmeticOperation::AO_MULTIPLY: op->op.args.ScalarArithmetic.operation = OP::ArithmeticArgs::MULTIPLY; break;
		case NodeScalarArithmeticOperation::AO_DIVIDE: op->op.args.ScalarArithmetic.operation = OP::ArithmeticArgs::DIVIDE; break;
		default:
			checkf(false, TEXT("Unknown arithmetic operation."));
			op->op.args.ScalarArithmetic.operation = OP::ArithmeticArgs::NONE;
			break;
		}

		// A
		if (Node* pA = node.m_pA.get())
		{
			op->SetChild(op->op.args.ScalarArithmetic.a, Generate(pA, Options));
		}
		else
		{
			op->SetChild(op->op.args.ScalarArithmetic.a,
				CodeGenerator::GenerateMissingScalarCode( TEXT("ScalarArithmetic A"), 1.0f, node.m_errorContext )
			);
		}

		// B
		if (Node* pB = node.m_pB.get())
		{
			op->SetChild(op->op.args.ScalarArithmetic.b, Generate(pB, Options));
		}
		else
		{
			op->SetChild(op->op.args.ScalarArithmetic.b,
				CodeGenerator::GenerateMissingScalarCode( TEXT("ScalarArithmetic B"), 1.0f, node.m_errorContext )
			);
		}

		result.op = op;
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateScalar_Table(FScalarGenerationResult& result, const FGenericGenerationOptions& Options, const Ptr<const NodeScalarTable>& Typed)
	{
		const NodeScalarTable::Private& node = *Typed->GetPrivate();

		Ptr<ASTOp> Op = GenerateTableSwitch<NodeScalarTable::Private, ETableColumnType::Scalar, OP_TYPE::SC_SWITCH>(node,
			[this,&Options](const NodeScalarTable::Private& node, int colIndex, int row, ErrorLog* pErrorLog)
			{
				NodeScalarConstantPtr pCell = new NodeScalarConstant();
				float scalar = node.Table->GetPrivate()->Rows[row].Values[colIndex].Scalar;
				pCell->SetValue(scalar);
				return Generate(pCell, Options);
			});

		result.op = Op;
	}


	//---------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> CodeGenerator::GenerateMissingScalarCode(const TCHAR* strWhere, float value, const void* errorContext)
	{
		// Log a warning
		FString Msg = FString::Printf(TEXT("Required connection not found: %s"), strWhere );
		m_pErrorLog->GetPrivate()->Add(Msg, ELMT_ERROR, errorContext);

		// Create a constant node
		NodeScalarConstantPtr pNode = new NodeScalarConstant();
		pNode->SetValue(value);

		FGenericGenerationOptions Options;
		Ptr<ASTOp> result = Generate(pNode, Options);

		return result;
	}

}