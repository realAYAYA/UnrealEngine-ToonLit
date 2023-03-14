// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/PlatformMath.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Image.h"
#include "MuR/MemoryPrivate.h"
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

#include <memory>
#include <utility>


namespace mu
{
	class Node;


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateScalar(SCALAR_GENERATION_RESULT& result, const NodeScalarPtrConst& untyped)
	{
		if (!untyped)
		{
			result = SCALAR_GENERATION_RESULT();
			return;
		}

		// See if it was already generated
		VISITED_MAP_KEY key = GetCurrentCacheKey(untyped);
		GeneratedScalarsMap::ValueType* it = m_generatedScalars.Find(key);
		if (it)
		{
			result = *it;
			return;
		}


		// Generate for each different type of node
		if (auto Constant = dynamic_cast<const NodeScalarConstant*>(untyped.get()))
		{
			GenerateScalar_Constant(result, Constant);
		}
		else if (auto Param = dynamic_cast<const NodeScalarParameter*>(untyped.get()))
		{
			GenerateScalar_Parameter(result, Param);
		}
		else if (auto Switch = dynamic_cast<const NodeScalarSwitch*>(untyped.get()))
		{
			GenerateScalar_Switch(result, Switch);
		}
		else if (auto EnumParam = dynamic_cast<const NodeScalarEnumParameter*>(untyped.get()))
		{
			GenerateScalar_EnumParameter(result, EnumParam);
		}
		else if (auto Curve = dynamic_cast<const NodeScalarCurve*>(untyped.get()))
		{
			GenerateScalar_Curve(result, Curve);
		}
		else if (auto Arithmetic = dynamic_cast<const NodeScalarArithmeticOperation*>(untyped.get()))
		{
			GenerateScalar_Arithmetic(result, Arithmetic);
		}
		else if (auto Variation = dynamic_cast<const NodeScalarVariation*>(untyped.get()))
		{
			GenerateScalar_Variation(result, Variation);
		}
		else if (auto Table = dynamic_cast<const NodeScalarTable*>(untyped.get()))
		{
			GenerateScalar_Table(result, Table);
		}
		else
		{
			check(false);
			mu::Halt();
		}

		// Cache the result
		m_generatedScalars.Add(key, result);
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateScalar_Constant(SCALAR_GENERATION_RESULT& result, const Ptr<const NodeScalarConstant>& Typed)
	{
		const NodeScalarConstant::Private& node = *Typed->GetPrivate();

		Ptr<ASTOpFixed> op = new ASTOpFixed();
		op->op.type = OP_TYPE::SC_CONSTANT;
		op->op.args.ScalarConstant.value = node.m_value;

		result.op = op;
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateScalar_Parameter(SCALAR_GENERATION_RESULT& result, const Ptr<const NodeScalarParameter>& Typed)
	{
		const NodeScalarParameter::Private& node = *Typed->GetPrivate();

		Ptr<ASTOpParameter> op;

		auto it = m_nodeVariables.find(node.m_pNode);
		if (it == m_nodeVariables.end())
		{
			PARAMETER_DESC param;
			param.m_name = node.m_name;
			param.m_uid = node.m_uid;
			param.m_type = PARAMETER_TYPE::T_FLOAT;
			param.m_defaultValue.m_float = node.m_defaultValue;
			param.m_detailedType = node.m_detailedType;

			op = new ASTOpParameter();
			op->type = OP_TYPE::SC_PARAMETER;
			op->parameter = param;

			// Generate the code for the ranges
			for (int32 a = 0; a < node.m_ranges.Num(); ++a)
			{
				RANGE_GENERATION_RESULT rangeResult;
				GenerateRange(rangeResult, node.m_ranges[a]);
				op->ranges.Emplace(op.get(), rangeResult.sizeOp, rangeResult.rangeName, rangeResult.rangeUID);
			}

			// Generate the code for the additional images in the parameter
			for (int32 a = 0; a < node.m_additionalImages.Num(); ++a)
			{
				Ptr<ASTOp> descAd;
				if (node.m_additionalImages[a])
				{
					// We take whatever size will be produced
					IMAGE_STATE newState;
					FImageDesc desc = CalculateImageDesc(*node.m_additionalImages[a]->GetBasePrivate());
					newState.m_imageSize = desc.m_size;
					newState.m_imageRect.min[0] = 0;
					newState.m_imageRect.min[1] = 0;
					newState.m_imageRect.size = desc.m_size;
					newState.m_layoutBlock = -1;
					m_imageState.Add(newState);

					// Generate
					descAd = Generate(node.m_additionalImages[a]);
					check(descAd);

					// Restore rect
					m_imageState.Pop();
				}
				op->additionalImages.Emplace(op, descAd);
			}

			m_nodeVariables[node.m_pNode] = op;
		}
		else
		{
			op = it->second;
		}

		result.op = op;
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateScalar_EnumParameter(SCALAR_GENERATION_RESULT& result, const Ptr<const NodeScalarEnumParameter>& Typed)
	{
		const NodeScalarEnumParameter::Private& node = *Typed->GetPrivate();

		Ptr<ASTOpParameter> op;

		auto it = m_nodeVariables.find(node.m_pNode);
		if (it == m_nodeVariables.end())
		{
			PARAMETER_DESC param;
			param.m_name = node.m_name;
			param.m_uid = node.m_uid;
			param.m_type = PARAMETER_TYPE::T_INT;
			param.m_defaultValue.m_int = node.m_defaultValue;
			param.m_detailedType = node.m_detailedType;

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
				RANGE_GENERATION_RESULT rangeResult;
				GenerateRange(rangeResult, node.m_ranges[a]);
				op->ranges.Emplace(op.get(), rangeResult.sizeOp, rangeResult.rangeName, rangeResult.rangeUID);
			}

			m_nodeVariables[node.m_pNode] = op;

		}
		else
		{
			op = it->second;
		}

		result.op = op;
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateScalar_Switch(SCALAR_GENERATION_RESULT& result, const Ptr<const NodeScalarSwitch>& Typed)
	{
		const NodeScalarSwitch::Private& node = *Typed->GetPrivate();

		if (node.m_options.Num() == 0)
		{
			// No options in the switch!
			Ptr<ASTOp> missingOp = GenerateMissingScalarCode("Switch option",
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
			SCALAR_GENERATION_RESULT ChildResult;
			GenerateScalar(ChildResult, node.m_pParameter.get());
			op->variable = ChildResult.op;
		}
		else
		{
			// This argument is required
			op->variable = GenerateMissingScalarCode("Switch variable", 0.0f, node.m_errorContext);
		}

		// Options
		for (int32 t = 0; t < node.m_options.Num(); ++t)
		{
			Ptr<ASTOp> branch;
			if (node.m_options[t])
			{
				SCALAR_GENERATION_RESULT ChildResult;
				GenerateScalar(ChildResult, node.m_options[t].get());
				branch = ChildResult.op;
			}
			else
			{
				// This argument is required
				branch = GenerateMissingScalarCode("Switch option", 1.0f, node.m_errorContext);
			}
			op->cases.Emplace((int16_t)t, op, branch);
		}

		result.op = op;
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateScalar_Variation(SCALAR_GENERATION_RESULT& result, const Ptr<const NodeScalarVariation>& Typed)
	{
		const NodeScalarVariation::Private& node = *Typed->GetPrivate();

		Ptr<ASTOp> op;

		// Default case
		if (node.m_defaultScalar)
		{
			FMeshGenerationResult branchResults;

			SCALAR_GENERATION_RESULT ChildResult;
			GenerateScalar(ChildResult, node.m_defaultScalar);
			op = ChildResult.op;
		}
		else
		{
			// This argument is required
			op = GenerateMissingScalarCode("Variation default", 0.0f, node.m_errorContext);
		}

		// Process variations in reverse order, since conditionals are built bottom-up.
		for (int t = node.m_variations.Num() - 1; t >= 0; --t)
		{
			int tagIndex = -1;
			const string& tag = node.m_variations[t].m_tag;
			for (int i = 0; i < m_firstPass.m_tags.Num(); ++i)
			{
				if (m_firstPass.m_tags[i].tag == tag)
				{
					tagIndex = i;
				}
			}

			if (tagIndex < 0)
			{
				char buf[256];
				mutable_snprintf(buf, 256, "Unknown tag found in image variation [%s].",
					tag.c_str());

				m_pErrorLog->GetPrivate()->Add(buf, ELMT_WARNING, node.m_errorContext);
				continue;
			}

			Ptr<ASTOp> variationOp;
			if (node.m_variations[t].m_scalar)
			{
				SCALAR_GENERATION_RESULT ChildResult;
				GenerateScalar(ChildResult, node.m_variations[t].m_scalar);
				variationOp = ChildResult.op;
			}
			else
			{
				// This argument is required
				variationOp = GenerateMissingScalarCode("Variation option", 0.0f,
					node.m_errorContext);
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
	void CodeGenerator::GenerateScalar_Curve(SCALAR_GENERATION_RESULT& result, const Ptr<const NodeScalarCurve>& Typed)
	{
		const NodeScalarCurve::Private& node = *Typed->GetPrivate();

		Ptr<ASTOpScalarCurve> op = new ASTOpScalarCurve();

		// T
		if (Node* pA = node.m_input_scalar.get())
		{
			op->time = Generate(pA);
		}
		else
		{
			op->time = CodeGenerator::GenerateMissingScalarCode("Curve T", 0.5f, node.m_errorContext);
		}

		op->curve = node.m_curve;

		result.op = op;
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateScalar_Arithmetic(SCALAR_GENERATION_RESULT& result, const Ptr<const NodeScalarArithmeticOperation>& Typed)
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
			op->SetChild(op->op.args.ScalarArithmetic.a, Generate(pA));
		}
		else
		{
			op->SetChild(op->op.args.ScalarArithmetic.a,
				CodeGenerator::GenerateMissingScalarCode
				(
					"ScalarArithmetic A",
					1.0f,
					node.m_errorContext
				)
			);
		}

		// B
		if (Node* pB = node.m_pB.get())
		{
			op->SetChild(op->op.args.ScalarArithmetic.b, Generate(pB));
		}
		else
		{
			op->SetChild(op->op.args.ScalarArithmetic.b,
				CodeGenerator::GenerateMissingScalarCode
				(
					"ScalarArithmetic B",
					1.0f,
					node.m_errorContext
				)
			);
		}

		result.op = op;
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateScalar_Table(SCALAR_GENERATION_RESULT& result, const Ptr<const NodeScalarTable>& Typed)
	{
		const NodeScalarTable::Private& node = *Typed->GetPrivate();

		Ptr<ASTOp> Op = GenerateTableSwitch<NodeScalarTable::Private, TCT_SCALAR, OP_TYPE::SC_SWITCH>(node,
			[this](const NodeScalarTable::Private& node, int colIndex, int row, ErrorLog* pErrorLog)
			{
				NodeScalarConstantPtr pCell = new NodeScalarConstant();
				float scalar = node.m_pTable->GetPrivate()->m_rows[row].m_values[colIndex].m_scalar;
				pCell->SetValue(scalar);
				return Generate(pCell);
			});

		result.op = Op;
	}


	//---------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> CodeGenerator::GenerateMissingScalarCode(const char* strWhere, float value, const void* errorContext)
	{
		// Log a warning
		char buf[256];
		mutable_snprintf
		(
			buf, 256,
			"Required connection not found: %s",
			strWhere
		);
		m_pErrorLog->GetPrivate()->Add(buf, ELMT_ERROR, errorContext);

		// Create a constant node
		NodeScalarConstantPtr pNode = new NodeScalarConstant();
		pNode->SetValue(value);

		Ptr<ASTOp> result = Generate(pNode);

		return result;
	}

}