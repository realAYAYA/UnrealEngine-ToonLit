// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/PlatformMath.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Image.h"
#include "MuR/MutableMath.h"
#include "MuR/MutableTrace.h"
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
#include "MuT/ASTOpSwitch.h"
#include "MuT/CodeGenerator.h"
#include "MuT/CodeGenerator_FirstPass.h"
#include "MuT/ErrorLog.h"
#include "MuT/ErrorLogPrivate.h"
#include "MuT/Node.h"
#include "MuT/NodeColour.h"
#include "MuT/NodeColourArithmeticOperation.h"
#include "MuT/NodeColourArithmeticOperationPrivate.h"
#include "MuT/NodeColourConstant.h"
#include "MuT/NodeColourConstantPrivate.h"
#include "MuT/NodeColourFromScalars.h"
#include "MuT/NodeColourFromScalarsPrivate.h"
#include "MuT/NodeColourParameter.h"
#include "MuT/NodeColourParameterPrivate.h"
#include "MuT/NodeColourSampleImage.h"
#include "MuT/NodeColourSampleImagePrivate.h"
#include "MuT/NodeColourSwitch.h"
#include "MuT/NodeColourSwitchPrivate.h"
#include "MuT/NodeColourTable.h"
#include "MuT/NodeColourTablePrivate.h"
#include "MuT/NodeColourVariation.h"
#include "MuT/NodeColourVariationPrivate.h"
#include "MuT/NodeImage.h"
#include "MuT/NodeImageProject.h"
#include "MuT/NodeMeshGeometryOperation.h"
#include "MuT/NodeMeshTable.h"
#include "MuT/NodeRange.h"
#include "MuT/NodeScalar.h"
#include "MuT/NodeScalarConstant.h"
#include "MuT/Table.h"
#include "MuT/TablePrivate.h"

#include <memory>
#include <utility>


namespace mu
{


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateColor(FColorGenerationResult& result, const NodeColourPtrConst& untyped)
	{
		if (!untyped)
		{
			result = FColorGenerationResult();
			return;
		}

		// See if it was already generated
		FVisitedKeyMap key = GetCurrentCacheKey(untyped);
		GeneratedColorsMap::ValueType* it = m_generatedColors.Find(key);
		if (it)
		{
			result = *it;
			return;
		}

		// Generate for each different type of node
		if (auto Constant = dynamic_cast<const NodeColourConstant*>(untyped.get()))
		{
			GenerateColor_Constant(result, Constant);
		}
		else if (auto Param = dynamic_cast<const NodeColourParameter*>(untyped.get()))
		{
			GenerateColor_Parameter(result, Param);
		}
		else if (auto Switch = dynamic_cast<const NodeColourSwitch*>(untyped.get()))
		{
			GenerateColor_Switch(result, Switch);
		}
		else if (auto Sample = dynamic_cast<const NodeColourSampleImage*>(untyped.get()))
		{
			GenerateColor_SampleImage(result, Sample);
		}
		else if (auto From = dynamic_cast<const NodeColourFromScalars*>(untyped.get()))
		{
			GenerateColor_FromScalars(result, From);
		}
		else if (auto Arithmetic = dynamic_cast<const NodeColourArithmeticOperation*>(untyped.get()))
		{
			GenerateColor_Arithmetic(result, Arithmetic);
		}
		else if (auto Variation = dynamic_cast<const NodeColourVariation*>(untyped.get()))
		{
			GenerateColor_Variation(result, Variation);
		}
		else if (auto Table = dynamic_cast<const NodeColourTable*>(untyped.get()))
		{
			GenerateColor_Table(result, Table);
		}
		else
		{
			check(false);
		}

		// Cache the result
		m_generatedColors.Add(key, result);
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateColor_Constant(FColorGenerationResult& result, const Ptr<const NodeColourConstant>& Typed)
	{
		const NodeColourConstant::Private& node = *Typed->GetPrivate();

		Ptr<ASTOpFixed> op = new ASTOpFixed();
		op->op.type = OP_TYPE::CO_CONSTANT;
		op->op.args.ColourConstant.value[0] = node.m_value.X;
		op->op.args.ColourConstant.value[1] = node.m_value.Y;
		op->op.args.ColourConstant.value[2] = node.m_value.Z;
		op->op.args.ColourConstant.value[3] = node.m_value.W;

		result.op = op;
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateColor_Parameter(FColorGenerationResult& result, const Ptr<const NodeColourParameter>& Typed)
	{
		const NodeColourParameter::Private& node = *Typed->GetPrivate();

		Ptr<ASTOpParameter> op;

		auto it = m_nodeVariables.find(node.m_pNode);

		if (it == m_nodeVariables.end())
		{
			FParameterDesc param;
			param.m_name = node.m_name;
			param.m_uid = node.m_uid;
			param.m_type = PARAMETER_TYPE::T_COLOUR;

			ParamColorType Value;
			Value[0] =  node.m_defaultValue[0];
			Value[1] = node.m_defaultValue[1];
			Value[2] = node.m_defaultValue[2];

			param.m_defaultValue.Set<ParamColorType>(Value);
			
			op = new ASTOpParameter();
			op->type = OP_TYPE::CO_PARAMETER;
			op->parameter = param;

			// Generate the code for the ranges
			for (int32 a = 0; a < node.m_ranges.Num(); ++a)
			{
				FRangeGenerationResult rangeResult;
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
	void CodeGenerator::GenerateColor_Switch(FColorGenerationResult& result, const Ptr<const NodeColourSwitch>& Typed)
	{
		const NodeColourSwitch::Private& node = *Typed->GetPrivate();

		MUTABLE_CPUPROFILER_SCOPE(NodeColourSwitch);

		if (node.m_options.Num() == 0)
		{
			// No options in the switch!
			Ptr<ASTOp> missingOp = GenerateMissingColourCode(TEXT("Switch option"), node.m_errorContext);
			result.op = missingOp;
			return;
		}

		Ptr<ASTOpSwitch> op = new ASTOpSwitch();
		op->type = OP_TYPE::CO_SWITCH;

		// Variable value
		if (node.m_pParameter)
		{
			op->variable = Generate(node.m_pParameter.get());
		}
		else
		{
			// This argument is required
			op->variable = GenerateMissingScalarCode(TEXT("Switch variable"), 0.0f, node.m_errorContext);
		}

		// Options
		for (std::size_t t = 0; t < node.m_options.Num(); ++t)
		{
			Ptr<ASTOp> branch;
			if (node.m_options[t])
			{
				branch = Generate(node.m_options[t].get());
			}
			else
			{
				// This argument is required
				branch = GenerateMissingColourCode(TEXT("Switch option"), node.m_errorContext);
			}
			op->cases.Emplace((int16)t, op, branch);
		}

		result.op = op;
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateColor_Variation(FColorGenerationResult& result, const Ptr<const NodeColourVariation>& Typed)
	{
		const NodeColourVariation::Private& node = *Typed->GetPrivate();

		Ptr<ASTOp> currentOp;

		// Default case
		if (node.m_defaultColour)
		{
			FMeshGenerationResult branchResults;
			currentOp = Generate(node.m_defaultColour);
		}
		else
		{
			// This argument is required
			currentOp = GenerateMissingColourCode(TEXT("Variation default"), node.m_errorContext);
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
				FString Msg = FString::Printf(TEXT("Unknown tag found in color variation [%s]."), *FString(tag.c_str()));
				m_pErrorLog->GetPrivate()->Add(Msg, ELMT_WARNING, node.m_errorContext);
				continue;
			}

			Ptr<ASTOp> variationOp;
			if (node.m_variations[t].m_colour)
			{
				variationOp = Generate(node.m_variations[t].m_colour);
			}
			else
			{
				// This argument is required
				variationOp = GenerateMissingColourCode(TEXT("Variation option"), node.m_errorContext);
			}


			Ptr<ASTOpConditional> conditional = new ASTOpConditional;
			conditional->type = OP_TYPE::CO_CONDITIONAL;
			conditional->no = currentOp;
			conditional->yes = variationOp;
			conditional->condition = m_firstPass.m_tags[tagIndex].genericCondition;

			currentOp = conditional;
		}

		result.op = currentOp;
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateColor_SampleImage(FColorGenerationResult& result, const Ptr<const NodeColourSampleImage>& Typed)
	{
		const NodeColourSampleImage::Private& node = *Typed->GetPrivate();

		// Generate the code
		Ptr<ASTOpFixed> op = new ASTOpFixed();
		op->op.type = OP_TYPE::CO_SAMPLEIMAGE;

		// Source image
		FImageGenerationOptions ImageOptions;
		ImageOptions.CurrentStateIndex = m_currentStateIndex;
		if (!m_activeTags.IsEmpty())
		{
			ImageOptions.ActiveTags = m_activeTags.Last();
		}

		Ptr<ASTOp> base;
		if (node.m_pImage)
		{
			// Generate
			FImageGenerationResult MapResult;
			GenerateImage(ImageOptions, MapResult, node.m_pImage);
			base = MapResult.op;
		}
		else
		{
			// This argument is required
			base = GenerateMissingImageCode(TEXT("Sample image"), EImageFormat::IF_RGB_UBYTE, node.m_errorContext, ImageOptions);
		}
		base = GenerateImageFormat(base, EImageFormat::IF_RGB_UBYTE);
		op->SetChild(op->op.args.ColourSampleImage.image, base);


		// X
		if (Node* pX = node.m_pX.get())
		{
			op->SetChild(op->op.args.ColourSampleImage.x, Generate(pX));
		}
		else
		{
			// Set a constant 0.5 value
			NodeScalarConstantPtr pNode = new NodeScalarConstant();
			pNode->SetValue(0.5f);
			op->SetChild(op->op.args.ColourSampleImage.x, Generate(pNode));
		}


		// Y
		if (Node* pY = node.m_pY.get())
		{
			op->SetChild(op->op.args.ColourSampleImage.y, Generate(pY));
		}
		else
		{
			// Set a constant 0.5 value
			NodeScalarConstantPtr pNode = new NodeScalarConstant();
			pNode->SetValue(0.5f);
			op->SetChild(op->op.args.ColourSampleImage.y, Generate(pNode));
		}

		// TODO
		op->op.args.ColourSampleImage.filter = 0;

		result.op = op;
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateColor_FromScalars(FColorGenerationResult& result, const Ptr<const NodeColourFromScalars>& Typed)
	{
		const NodeColourFromScalars::Private& node = *Typed->GetPrivate();

		Ptr<ASTOpFixed> op = new ASTOpFixed();
		op->op.type = OP_TYPE::CO_FROMSCALARS;

		// X
		if (Node* pX = node.m_pX.get())
		{
			op->SetChild(op->op.args.ColourFromScalars.v[0], Generate(pX));
		}
		else
		{
			NodeScalarConstantPtr pNode = new NodeScalarConstant();
			pNode->SetValue(1.0f);
			op->SetChild(op->op.args.ColourFromScalars.v[0], Generate(pNode));
		}

		// Y
		if (Node* pY = node.m_pY.get())
		{
			op->SetChild(op->op.args.ColourFromScalars.v[1], Generate(pY));
		}
		else
		{
			NodeScalarConstantPtr pNode = new NodeScalarConstant();
			pNode->SetValue(1.0f);
			op->SetChild(op->op.args.ColourFromScalars.v[1], Generate(pNode));
		}

		// Z
		if (Node* pZ = node.m_pZ.get())
		{
			op->SetChild(op->op.args.ColourFromScalars.v[2], Generate(pZ));
		}
		else
		{
			NodeScalarConstantPtr pNode = new NodeScalarConstant();
			pNode->SetValue(1.0f);
			op->SetChild(op->op.args.ColourFromScalars.v[2], Generate(pNode));
		}

		// W
		if (Node* pW = node.m_pW.get())
		{
			op->SetChild(op->op.args.ColourFromScalars.v[3], Generate(pW));
		}
		else
		{
			NodeScalarConstantPtr pNode = new NodeScalarConstant();
			pNode->SetValue(1.0f);
			op->SetChild(op->op.args.ColourFromScalars.v[3], Generate(pNode));
		}

		result.op = op;
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateColor_Arithmetic(FColorGenerationResult& result, const Ptr<const NodeColourArithmeticOperation>& Typed)
	{
		const NodeColourArithmeticOperation::Private& node = *Typed->GetPrivate();

		Ptr<ASTOpFixed> op = new ASTOpFixed();
		op->op.type = OP_TYPE::CO_ARITHMETIC;

		switch (node.m_operation)
		{
		case NodeColourArithmeticOperation::AO_ADD: op->op.args.ColourArithmetic.operation = OP::ArithmeticArgs::ADD; break;
		case NodeColourArithmeticOperation::AO_SUBTRACT: op->op.args.ColourArithmetic.operation = OP::ArithmeticArgs::SUBTRACT; break;
		case NodeColourArithmeticOperation::AO_MULTIPLY: op->op.args.ColourArithmetic.operation = OP::ArithmeticArgs::MULTIPLY; break;
		case NodeColourArithmeticOperation::AO_DIVIDE: op->op.args.ColourArithmetic.operation = OP::ArithmeticArgs::DIVIDE; break;
		default:
			checkf(false, TEXT("Unknown arithmetic operation."));
			op->op.args.ColourArithmetic.operation = OP::ArithmeticArgs::NONE;
			break;
		}

		// A
		if (Node* pA = node.m_pA.get())
		{
			op->SetChild(op->op.args.ColourArithmetic.a, Generate(pA));
		}
		else
		{
			op->SetChild(op->op.args.ColourArithmetic.a,
				CodeGenerator::GenerateMissingColourCode(TEXT("ColourArithmetic A"), node.m_errorContext));
		}

		// B
		if (Node* pB = node.m_pB.get())
		{
			op->SetChild(op->op.args.ColourArithmetic.b, Generate(pB));
		}
		else
		{
			op->SetChild(op->op.args.ColourArithmetic.b,
				CodeGenerator::GenerateMissingColourCode(TEXT("ColourArithmetic B"),node.m_errorContext));
		}

		result.op = op;
	}


	//-------------------------------------------------------------------------------------------------
	void CodeGenerator::GenerateColor_Table(FColorGenerationResult& result, const Ptr<const NodeColourTable>& Typed)
	{
		const NodeColourTable::Private& node = *Typed->GetPrivate();

		result.op = GenerateTableSwitch<NodeColourTable::Private, TCT_COLOUR, OP_TYPE::CO_SWITCH>(node,
			[this](const NodeColourTable::Private& node, int colIndex, int row, ErrorLog* pErrorLog)
			{
				NodeColourConstantPtr CellData = new NodeColourConstant();
				FVector4f Colour = node.m_pTable->GetPrivate()->m_rows[row].m_values[colIndex].m_colour;
				CellData->SetValue(Colour);
				return Generate(CellData);
			});
	}


	//---------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> CodeGenerator::GenerateMissingColourCode(const TCHAR* strWhere, const void* errorContext)
	{
		// Log a warning
		FString Msg = FString::Printf(TEXT("Required connection not found: %s"), strWhere);
		m_pErrorLog->GetPrivate()->Add(Msg, ELMT_ERROR, errorContext);

		// Create a constant colour node
		NodeColourConstantPtr pNode = new NodeColourConstant();
		pNode->SetValue(FVector4f(1, 1, 0, 1));

		FColorGenerationResult Result;
		GenerateColor(Result, pNode);

		return Result.op;
	}


}
