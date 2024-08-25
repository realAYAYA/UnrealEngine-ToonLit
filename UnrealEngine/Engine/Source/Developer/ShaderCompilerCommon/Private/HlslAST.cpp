// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HlslAST.cpp - Abstract Syntax Tree implementation for HLSL.
=============================================================================*/

#include "HlslAST.h"

namespace CrossCompiler
{
	namespace AST
	{
		static void WriteOptionArraySize(FASTWriter& Writer, bool bIsArray, const TLinearArray<FExpression*>& ArraySize)
		{
			if (bIsArray && ArraySize.Num() == 0)
			{
				Writer << TEXT("[]");
			}
			else
			{
				for (const auto* Dimension : ArraySize)
				{
					Writer << (TCHAR)'[';
					if (Dimension)
					{
						Dimension->Write(Writer);
					}
					Writer << (TCHAR)']';
				}
			}
		}

#if 0
		FNode::FNode()/* :
			Prev(nullptr),
			Next(nullptr)*/
		{
		}
#endif // 0

		FNode::FNode(FLinearAllocator* Allocator, const FSourceInfo& InInfo) :
			SourceInfo(InInfo),
			Attributes(Allocator)/*,
			Prev(nullptr),
			Next(nullptr)*/
		{
		}

		void FASTWriter::DoIndent()
		{
			int32 N = Indent;
			while (--N >= 0)
			{
				(*this) << (TCHAR)'\t';
			}
		}

		void FNode::WriteAttributes(FASTWriter& Writer) const
		{
			if (Attributes.Num() > 0)
			{
				for (auto* Attr : Attributes)
				{
					Attr->Write(Writer);
				}

				Writer << (TCHAR)' ';
			}
		}

		FPragma::FPragma(FLinearAllocator* InAllocator, const TCHAR* InPragma, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo)
		{
			Pragma = InAllocator->Strdup(InPragma);
		}

		void FPragma::Write(FASTWriter& Writer) const
		{
			Writer << Pragma << TEXT("\n");
		}

		FExpression::FExpression(FLinearAllocator* InAllocator, EOperators InOperator, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Operator(InOperator),
			Identifier(nullptr),
			Expressions(InAllocator)
		{
			TypeSpecifier = nullptr;
		}

		FExpression::FExpression(FLinearAllocator* InAllocator, EOperators InOperator, FExpression* E0, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Operator(InOperator),
			Identifier(nullptr),
			Expressions(InAllocator)
		{
			Expressions.SetNumUninitialized(1);
			Expressions[0] = E0;
			TypeSpecifier = nullptr;
		}

		FExpression::FExpression(FLinearAllocator* InAllocator, EOperators InOperator, FExpression* E0, FExpression* E1, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Operator(InOperator),
			Identifier(nullptr),
			Expressions(InAllocator)
		{
			Expressions.SetNumUninitialized(2);
			Expressions[0] = E0;
			Expressions[1] = E1;
			TypeSpecifier = nullptr;
		}

		FExpression::FExpression(FLinearAllocator* InAllocator, EOperators InOperator, FExpression* E0, FExpression* E1, FExpression* E2, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Operator(InOperator),
			Identifier(nullptr),
			Expressions(InAllocator)
		{
			Expressions.SetNumUninitialized(3);
			Expressions[0] = E0;
			Expressions[1] = E1;
			Expressions[2] = E2;
			TypeSpecifier = nullptr;
		}

		void FExpression::WriteOperator(FASTWriter& Writer) const
		{
			switch (Operator)
			{
			case EOperators::Plus:
				Writer << TEXT("+");
				break;

			case EOperators::Minus:
				Writer << TEXT("-");
				break;

			case EOperators::Assign:
				Writer << TEXT("=");
				break;

			case EOperators::AddAssign:
				Writer << TEXT("+=");
				break;

			case EOperators::SubAssign:
				Writer << TEXT("-=");
				break;

			case EOperators::MulAssign:
				Writer << TEXT("*=");
				break;

			case EOperators::DivAssign:
				Writer << TEXT("/=");
				break;

			case EOperators::ModAssign:
				Writer << TEXT("%=");
				break;

			case EOperators::RSAssign:
				Writer << TEXT(">>=");
				break;

			case EOperators::LSAssign:
				Writer << TEXT("<<=");
				break;

			case EOperators::AndAssign:
				Writer << TEXT("&=");
				break;

			case EOperators::OrAssign:
				Writer << TEXT("|=");
				break;

			case EOperators::XorAssign:
				Writer << TEXT("^=");
				break;

			case EOperators::Conditional:
				Writer << TEXT("?");
				break;

			case EOperators::LogicOr:
				Writer << TEXT("||");
				break;

			case EOperators::LogicAnd:
				Writer << TEXT("&&");
				break;

			case EOperators::LogicNot:
				Writer << TEXT("!");
				break;

			case EOperators::BitOr:
				Writer << TEXT("|");
				break;

			case EOperators::BitXor:
				Writer << TEXT("^");
				break;

			case EOperators::BitAnd:
				Writer << TEXT("&");
				break;

			case EOperators::BitNeg:
				Writer << TEXT("~");
				break;

			case EOperators::Equal:
				Writer << TEXT("==");
				break;

			case EOperators::NEqual:
				Writer << TEXT("!=");
				break;

			case EOperators::Less:
				Writer << TEXT("<");
				break;

			case EOperators::Greater:
				Writer << TEXT(">");
				break;

			case EOperators::LEqual:
				Writer << TEXT("<=");
				break;

			case EOperators::GEqual:
				Writer << TEXT(">=");
				break;

			case EOperators::LShift:
				Writer << TEXT("<<");
				break;

			case EOperators::RShift:
				Writer << TEXT(">>");
				break;

			case EOperators::Add:
				Writer << TEXT("+");
				break;

			case EOperators::Sub:
				Writer << TEXT("-");
				break;

			case EOperators::Mul:
				Writer << TEXT("*");
				break;

			case EOperators::Div:
				Writer << TEXT("/");
				break;

			case EOperators::Mod:
				Writer << TEXT("%");
				break;

			case EOperators::PreInc:
				Writer << TEXT("++");
				break;

			case EOperators::PreDec:
				Writer << TEXT("--");
				break;

			case EOperators::PostInc:
			case EOperators::PostDec:
			case EOperators::FieldSelection:
			case EOperators::ArrayIndex:
				break;

			case EOperators::TypeCast:
				Writer << (TCHAR)'(';
				TypeSpecifier->Write(Writer);
				Writer << TEXT(")");
				break;

			default:
				Writer << TEXT("*MISSING_");
				Writer << (uint32)Operator;
				Writer << (TCHAR)'*';
				checkf(0, TEXT("Unhandled AST Operator %d!"), (uint32)Operator);
				break;
			}
		}

		void FExpression::Write(FASTWriter& Writer) const
		{
			switch (Operator)
			{
			case EOperators::Conditional:
				Writer << (TCHAR)'(';
				Expressions[0]->Write(Writer);
				Writer << TEXT(" ? ");
				Expressions[1]->Write(Writer);
				Writer << TEXT(" : ");
				Expressions[2]->Write(Writer);
				Writer << TEXT(")");
				break;

			case EOperators::Literal:
				Writer << Identifier;
				break;

			case EOperators::Identifier:
				Writer << Identifier;
				break;

			default:
				Writer << TEXT("*MISSING_");
				Writer << (uint32)Operator;
				Writer << (TCHAR)'*';
				checkf(0, TEXT("Unhandled AST Operator %d!"), (int32)Operator);
				break;
			}
		}

		bool FExpression::GetConstantIntValue(int32& OutValue) const
		{
			if (IsConstant())
			{
				checkf(Identifier!=nullptr, TEXT("Null identifier, literaltype %d"), (int32)LiteralType);
				OutValue = (int32)FCString::Atoi(Identifier);
				return true;
			}

			return false;
		}

		FExpression::~FExpression()
		{
			for (auto* Expr : Expressions)
			{
				if (Expr)
				{
					delete Expr;
				}
			}

			for (int32 Index = 0; Index < Expressions.Num(); ++Index)
			{
				if (Expressions[Index])
				{
					delete Expressions[Index];
				}
				else
				{
					break;
				}
			}
		}

		FUnaryExpression::FUnaryExpression(FLinearAllocator* InAllocator, EOperators InOperator, FExpression* Expr, const FSourceInfo& InInfo) :
			FExpression(InAllocator, InOperator, Expr, InInfo)
		{
		}

		void FUnaryExpression::Write(FASTWriter& Writer) const
		{
			WriteOperator(Writer);

			if (Writer.ExpressionScope != 0 && Operator != EOperators::FieldSelection)
			{
				Writer << (TCHAR)'(';
			}

			if (Expressions.Num() != 0)
			{
				++Writer.ExpressionScope;
				Expressions[0]->Write(Writer);
				--Writer.ExpressionScope;
			}

			// Suffix
			switch (Operator)
			{
			case EOperators::PostInc:
				Writer << TEXT("++");
				break;

			case EOperators::PostDec:
				Writer << TEXT("--");
				break;

			case EOperators::FieldSelection:
				Writer << (TCHAR)'.';
				Writer << Identifier;
				break;

			default:
				break;
			}

			if (Writer.ExpressionScope != 0 && Operator != EOperators::FieldSelection)
			{
				Writer << (TCHAR)')';
			}
		}

		FBinaryExpression::FBinaryExpression(FLinearAllocator* InAllocator, EOperators InOperator, FExpression* E0, FExpression* E1, const FSourceInfo& InInfo) :
			FExpression(InAllocator, InOperator, E0, E1, InInfo)
		{
		}

		void FBinaryExpression::Write(FASTWriter& Writer) const
		{
			switch (Operator)
			{
			case EOperators::ArrayIndex:
				if (Expressions[0]->AsUnaryExpression() && Expressions[0]->Operator == EOperators::Identifier)
				{
					Expressions[0]->Write(Writer);
				}
				else
				{
					Writer << (TCHAR)'(';
					Expressions[0]->Write(Writer);
					Writer << (TCHAR)')';
				}
				Writer << (TCHAR)'[';
				Expressions[1]->Write(Writer);
				Writer << (TCHAR)']';
				break;

			default:
				if (Writer.ExpressionScope != 0 && !IsAssignmentOperator(Operator))
				{
					Writer << (TCHAR)'(';
				}
				++Writer.ExpressionScope;
				Expressions[0]->Write(Writer);
				Writer << (TCHAR)' ';
				WriteOperator(Writer);
				Writer << (TCHAR)' ';
				Expressions[1]->Write(Writer);
				--Writer.ExpressionScope;
				if (Writer.ExpressionScope != 0 && !IsAssignmentOperator(Operator))
				{
					Writer << (TCHAR)')';
				}
				break;
			}
		}

		bool FBinaryExpression::GetConstantIntValue(int32& OutValue) const
		{
			int32 LHS = 0;
			int32 RHS = 0;
			if (!Expressions[0]->GetConstantIntValue(LHS) || !Expressions[1]->GetConstantIntValue(RHS))
			{
				return false;
			}

			switch (Operator)
			{
			default:
				return false;

			case EOperators::LogicOr:	OutValue = LHS || RHS; break;
			case EOperators::LogicAnd:	OutValue = LHS && RHS; break;
			case EOperators::BitOr:		OutValue = LHS | RHS; break;
			case EOperators::BitXor:	OutValue = LHS ^ RHS; break;
			case EOperators::BitAnd:	OutValue = LHS ^ RHS; break;
			case EOperators::Equal:		OutValue = LHS == RHS; break;
			case EOperators::NEqual:	OutValue = LHS != RHS; break;
			case EOperators::Less:		OutValue = LHS < RHS; break;
			case EOperators::Greater:	OutValue = LHS > RHS; break;
			case EOperators::LEqual:	OutValue = LHS <= RHS; break;
			case EOperators::GEqual:	OutValue = LHS >= RHS; break;
			case EOperators::LShift:	OutValue = LHS << RHS; break;
			case EOperators::RShift:	OutValue = LHS >> RHS; break;
			case EOperators::Add:		OutValue = LHS + RHS; break;
			case EOperators::Sub:		OutValue = LHS - RHS; break;
			case EOperators::Mul:		OutValue = LHS * RHS; break;
			case EOperators::Div:		OutValue = LHS / RHS; break;
			case EOperators::Mod:		OutValue = LHS % RHS; break;
			}
			return true;
		}

		FExpressionStatement::FExpressionStatement(FLinearAllocator* InAllocator, FExpression* InExpr, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Expression(InExpr)
		{
		}

		void FExpressionStatement::Write(FASTWriter& Writer) const
		{
			Writer.DoIndent();
			Expression->Write(Writer);
			Writer << TEXT(";\n");
		}

		FExpressionStatement::~FExpressionStatement()
		{
			if (Expression)
			{
				delete Expression;
			}
		}

		FCompoundStatement::FCompoundStatement(FLinearAllocator* InAllocator, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Statements(InAllocator)
		{
		}

		void FCompoundStatement::Write(FASTWriter& Writer) const
		{
			Writer.DoIndent();
			Writer << TEXT("{\n");
			for (auto* Statement : Statements)
			{
				FASTWriterIncrementScope Scope(Writer);
				Statement->Write(Writer);
			}
			Writer.DoIndent();
			Writer << TEXT("}\n");
		}

		FCompoundStatement::~FCompoundStatement()
		{
			for (auto* Statement : Statements)
			{
				if (Statement)
				{
					delete Statement;
				}
			}
		}

		FFunctionDefinition::FFunctionDefinition(FLinearAllocator* InAllocator, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Prototype(nullptr),
			Body(nullptr)
		{
		}

		void FFunctionDefinition::Write(FASTWriter& Writer) const
		{
			WriteAttributes(Writer);
			Prototype->Write(Writer);
			if (Body)
			{
				Body->Write(Writer);
			}
		}

		FFunctionDefinition::~FFunctionDefinition()
		{
			delete Prototype;
			delete Body;
		}

		FFunction::FFunction(FLinearAllocator* InAllocator, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			ReturnType(nullptr),
			ScopeIdentifier(nullptr),
			Identifier(nullptr),
			ReturnSemantic(nullptr),
			Parameters(InAllocator),
			bIsDefinition(false),
			bIsOperator(false),
			bIsStatic(false)
		{
		}

		void FFunction::Write(FASTWriter& Writer) const
		{
			WriteAttributes(Writer);
			Writer << TEXT("\n");
			if (bIsStatic)
			{
				Writer << TEXT("static ");
			}
			ReturnType->Write(Writer);
			Writer << (TCHAR)' ';
			if (ScopeIdentifier)
			{
				Writer << ScopeIdentifier << TEXT("::");
			}
			if (bIsOperator)
			{
				Writer << TEXT("operator");
			}
			Writer << Identifier;
			Writer << (TCHAR)'(';
			bool bFirst = true;
			const int32 ParamsPerLine = 6;
			for (int32 Index = 0; Index < Parameters.Num(); ++Index)
			{
				if (Index > 0)
				{
					if ((Index % ParamsPerLine) == 0)
					{
						Writer << TEXT(",\n\t\t");
					}
					else
					{
						Writer << TEXT(", ");
					}
				}
				Parameters[Index]->Write(Writer);
			}

			Writer << TEXT(")");
			if (ReturnSemantic)
			{
				ReturnSemantic->Write(Writer);
			}
			if (bIsDefinition)
			{
				Writer << TEXT(";\n");
			}
			else
			{
				Writer << TEXT("\n");
			}
		}

		FFunction::~FFunction()
		{
			for (auto* Param : Parameters)
			{
				delete Param;
			}
		}

		FJumpStatement::FJumpStatement(FLinearAllocator* InAllocator, EJumpType InType, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Type(InType),
			OptionalExpression(nullptr)
		{
		}

		void FJumpStatement::Write(FASTWriter& Writer) const
		{
			Writer.DoIndent();

			switch (Type)
			{
			case EJumpType::Return:
				Writer << TEXT("return");
				break;

			case EJumpType::Break:
				Writer << TEXT("break");
				break;

			case EJumpType::Continue:
				Writer << TEXT("continue");
				break;

			default:
				Writer << TEXT("*MISSING_");
				Writer << (uint32)Type;
				Writer << (TCHAR)'*';
				checkf(0, TEXT("Unhandled AST jump type %d!"), (int32)Type);
				break;
			}

			if (OptionalExpression)
			{
				Writer << TEXT(" ");
				OptionalExpression->Write(Writer);
			}
			Writer << TEXT(";\n");
		}

		FJumpStatement::~FJumpStatement()
		{
			if (OptionalExpression)
			{
				delete OptionalExpression;
			}
		}

		FSelectionStatement::FSelectionStatement(FLinearAllocator* InAllocator, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Condition(nullptr),
			ThenStatement(nullptr),
			ElseStatement(nullptr)
		{
		}

		void FSelectionStatement::Write(FASTWriter& Writer) const
		{
			Writer.DoIndent();
			WriteAttributes(Writer);
			Writer << TEXT("if (");
			Condition->Write(Writer);
			Writer << TEXT(")\n");
			ThenStatement->Write(Writer);
			if (ElseStatement)
			{
				Writer.DoIndent();
				Writer << TEXT("else\n");
				ElseStatement->Write(Writer);
			}
		}

		FSelectionStatement::~FSelectionStatement()
		{
			delete Condition;
			delete ThenStatement;
			if (ElseStatement)
			{
				delete ElseStatement;
			}
		}

		FTypeSpecifier::FTypeSpecifier(FLinearAllocator* InAllocator, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			TypeName(nullptr),
			InnerType(nullptr),
			Structure(nullptr),
			TextureMSNumSamples(1),
			PatchSize(0),
			bIsArray(false),
			//bIsUnsizedArray(false),
			ArraySize(nullptr)
		{
		}

		void FTypeSpecifier::Write(FASTWriter& Writer) const
		{
			if (Structure)
			{
				Structure->Write(Writer);
			}
			else
			{
				if (bPrecise)
				{
					Writer << TEXT("precise ");
				}
				Writer << TypeName;
				if (TextureMSNumSamples > 1)
				{
					Writer << (TCHAR)'<';
					Writer << InnerType;
					Writer << TEXT(", ");
					Writer << (uint32)TextureMSNumSamples;
					Writer << (TCHAR)'>';
				}
				else if (InnerType && *InnerType)
				{
					Writer << (TCHAR)'<';
					Writer << InnerType;
					Writer << (TCHAR)'>';
				}
			}

			if (bIsArray)
			{
				Writer << TEXT("[ ");

				if (ArraySize)
				{
					ArraySize->Write(Writer);
				}

				printf("]");
			}
		}

		FTypeSpecifier::~FTypeSpecifier()
		{
			if (Structure)
			{
				delete Structure;
			}

			if (ArraySize)
			{
				delete ArraySize;
			}
		}

		FCBufferDeclaration::FCBufferDeclaration(FLinearAllocator* InAllocator, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Name(nullptr),
			Declarations(InAllocator)
		{
		}

		void FCBufferDeclaration::Write(FASTWriter& Writer) const
		{
			Writer.DoIndent();
			Writer << TEXT("cbuffer ");
			Writer << Name;
			Writer << (TCHAR)'\n';

			Writer.DoIndent();
			Writer << TEXT("{\n");

			for (auto* Declaration : Declarations)
			{
				FASTWriterIncrementScope Scope(Writer);
				Declaration->Write(Writer);
			}

			Writer.DoIndent();
			Writer << TEXT("}\n\n");
		}

		FCBufferDeclaration::~FCBufferDeclaration()
		{
			for (auto* Decl : Declarations)
			{
				delete Decl;
			}
		}

		FTypeQualifier::FTypeQualifier()
		{
			Raw = 0;
		}

		void FTypeQualifier::Write(FASTWriter& Writer) const
		{
			if (bIsStatic)
			{
				Writer << TEXT("static ");
			}

			if (bConstant)
			{
				Writer << TEXT("const ");
			}

			if (bShared)
			{
				Writer << TEXT("groupshared ");
			}
			else if (bIn && bOut)
			{
				Writer << TEXT("inout ");
			}
			else if (bIn)
			{
				Writer << TEXT("in ");
			}
			else if (bOut)
			{
				Writer << TEXT("out ");
			}

			if (bLinear)
			{
				Writer << TEXT("linear ");
			}
			if (bCentroid)
			{
				Writer << TEXT("centroid ");
			}
			if (NoInterpolatorType)
			{
				Writer << NoInterpolatorType;
				Writer << (TCHAR)' ';
			}
			if (NoPerspectiveType)
			{
				Writer << NoPerspectiveType;
				Writer << (TCHAR)' ';
			}
			if (bSample)
			{
				Writer << TEXT("sample ");
			}

			if (bRowMajor)
			{
				Writer << TEXT("row_major ");
			}

			if (PrimitiveType)
			{
				Writer << PrimitiveType;
				Writer << (TCHAR)' ';
			}
		}

		FFullySpecifiedType::FFullySpecifiedType(FLinearAllocator* InAllocator, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Specifier(nullptr)
		{
		}

		void FFullySpecifiedType::Write(FASTWriter& Writer) const
		{
			Qualifier.Write(Writer);
			Specifier->Write(Writer);
		}

		FFullySpecifiedType::~FFullySpecifiedType()
		{
			delete Specifier;
		}

		FSemanticSpecifier::FSemanticSpecifier(FLinearAllocator* InAllocator, FSemanticSpecifier::ESpecType InType, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Arguments(InAllocator),
			Type(InType),
			Semantic(nullptr)
		{
		}

		FSemanticSpecifier::FSemanticSpecifier(FLinearAllocator* InAllocator, const TCHAR* InSemantic, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Arguments(InAllocator),
			Type(FSemanticSpecifier::ESpecType::Semantic)
		{
			Semantic = InAllocator->Strdup(InSemantic);
		}

		FSemanticSpecifier::~FSemanticSpecifier()
		{
			for (FExpression* Expr : Arguments)
			{
				delete Expr;
			}
		}

		void FSemanticSpecifier::Write(FASTWriter& Writer) const
		{
			Writer << TEXT(" : ");
			switch (Type)
			{
			case ESpecType::Semantic:
				Writer << Semantic;
				Writer << TEXT(" ");
				break;
			case ESpecType::Register:
				Writer << TEXT("register");
				break;
			case ESpecType::PackOffset:
				Writer << TEXT("packoffset");
				break;
			default:
				Writer << *FString::Printf(TEXT("<Unknown Type value %d!>"), (int32)Type);
			}
			if (Arguments.Num() > 0)
			{
				Writer << TEXT("(");
				for (int32 Index = 0, Num = Arguments.Num(); Index < Num; ++Index)
				{
					Arguments[Index]->Write(Writer);
					if (Index + 1 < Num)
					{
						Writer << TEXT(", ");
					}
				}
				Writer << TEXT(")");
			}
		}

		FDeclaration::FDeclaration(FLinearAllocator* InAllocator, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Identifier(nullptr),
			Semantic(nullptr),
			bIsArray(false),
			ArraySize(InAllocator),
			Initializer(nullptr)
		{
		}

		void FDeclaration::Write(FASTWriter& Writer) const
		{
			WriteAttributes(Writer);
			Writer << Identifier;

			WriteOptionArraySize(Writer, bIsArray, ArraySize);

			if (Initializer)
			{
				Writer << TEXT(" = ");
				Initializer->Write(Writer);
			}

			if (Semantic)
			{
				Semantic->Write(Writer);
			}
		}

		FDeclaration::~FDeclaration()
		{
			for (auto* Expr : ArraySize)
			{
				delete Expr;
			}

			if (Initializer)
			{
				delete Initializer;
			}
		}

		FDeclaratorList::FDeclaratorList(FLinearAllocator* InAllocator, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Type(nullptr),
			Declarations(InAllocator)
		{
		}

		void FDeclaratorList::WriteNoEOL(FASTWriter& Writer) const
		{
			WriteAttributes(Writer);
			if (bTypedef)
			{
				Writer << TEXT("typedef ");
			}

			if (Type)
			{
				Type->Write(Writer);
				Writer << TEXT(" ");
			}

			bool bFirst = true;
			for (auto* Decl : Declarations)
			{
				if (bFirst)
				{
					bFirst = false;
				}
				else
				{
					Writer << TEXT(", ");
				}

				Decl->Write(Writer);
			}

			Writer << TEXT(";");
		}

		void FDeclaratorList::Write(FASTWriter& Writer) const
		{
			Writer.DoIndent();

			WriteNoEOL(Writer);

			Writer << TEXT("\n");
		}

		FDeclaratorList::~FDeclaratorList()
		{
			delete Type;

			for (auto* Decl : Declarations)
			{
				delete Decl;
			}
		}

		FExpressionList::FExpressionList(FLinearAllocator* InAllocator, FExpressionList::EType InType, const FSourceInfo& InInfo) :
			FExpression(InAllocator, EOperators::ExpressionList, InInfo),
			Type(InType)
		{
		}

		void FExpressionList::Write(FASTWriter& Writer) const
		{
			switch (Type)
			{
			case EType::Braced:			Writer << TEXT("{"); break;
			case EType::Parenthesized:	Writer << TEXT("("); break;
			default: break;
			}
			bool bFirst = true;
			for (auto* Expr : Expressions)
			{
				if (bFirst)
				{
					bFirst = false;
				}
				else
				{
					Writer << TEXT(", ");
				}

				Expr->Write(Writer);
			}
			switch (Type)
			{
			case EType::Braced:			Writer << TEXT("}"); break;
			case EType::Parenthesized:	Writer << TEXT(")"); break;
			default: break;
			}
		}

		FParameterDeclarator::FParameterDeclarator(FLinearAllocator* InAllocator, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Type(nullptr),
			Identifier(nullptr),
			Semantic(nullptr),
			bIsArray(false),
			ArraySize(InAllocator),
			DefaultValue(nullptr)
		{
		}

		void FParameterDeclarator::Write(FASTWriter& Writer) const
		{
			WriteAttributes(Writer);
			Type->Write(Writer);
			Writer << (TCHAR)' ' << Identifier;

			WriteOptionArraySize(Writer, bIsArray, ArraySize);

			if (Semantic)
			{
				Semantic->Write(Writer);
			}

			if (DefaultValue)
			{
				Writer << TEXT(" = ");
				DefaultValue->Write(Writer);
			}
		}

		FParameterDeclarator* FParameterDeclarator::CreateFromDeclaratorList(FDeclaratorList* List, FLinearAllocator* Allocator)
		{
			check(List);
			check(List->Declarations.Num() == 1);

			auto* Source = (FDeclaration*)List->Declarations[0];
			auto* New = new(Allocator) FParameterDeclarator(Allocator, Source->SourceInfo);
			New->Type = List->Type;
			New->Identifier = Source->Identifier;
			New->Semantic = Source->Semantic;
			New->bIsArray = Source->bIsArray;
			New->ArraySize = Source->ArraySize;
			New->DefaultValue = Source->Initializer;
			return New;
		}

		FParameterDeclarator::~FParameterDeclarator()
		{
			delete Type;

			for (auto* Expr : ArraySize)
			{
				delete Expr;
			}

			if (DefaultValue)
			{
				delete DefaultValue;
			}
		}

		FIterationStatement::FIterationStatement(FLinearAllocator* InAllocator, const FSourceInfo& InInfo, EIterationType InType) :
			FNode(InAllocator, InInfo),
			Type(InType),
			InitStatement(nullptr),
			Condition(nullptr),
			RestExpression(nullptr),
			Body(nullptr)
		{
		}

		void FIterationStatement::Write(FASTWriter& Writer) const
		{
			Writer.DoIndent();
			WriteAttributes(Writer);
			switch (Type)
			{
			case EIterationType::For:
				Writer << TEXT("for (");
				if (InitStatement)
				{
					auto* DeclList = InitStatement->AsDeclaratorList();
					if (DeclList)
					{
						DeclList->WriteNoEOL(Writer);
					}
					else
					{
						InitStatement->Write(Writer);
						Writer << TEXT(";");
					}
				}
				else
				{
					Writer << TEXT(" ;");
				}
				Writer << TEXT(" ");
				if (Condition)
				{
					Condition->Write(Writer);
				}
				Writer << TEXT("; ");
				if (RestExpression)
				{
					RestExpression->Write(Writer);
				}

				Writer << TEXT(")\n");
				if (Body)
				{
					Body->Write(Writer);
				}
				else
				{
					Writer.DoIndent();
					Writer << TEXT("{\n");
					Writer.DoIndent();
					Writer << TEXT("}\n");
				}
				break;

			case EIterationType::While:
				Writer << TEXT("while (");
				Condition->Write(Writer);
				Writer << TEXT(")\n");
				Writer.DoIndent();
				Writer << TEXT("{\n");
				if (Body)
				{
					FASTWriterIncrementScope Scope(Writer);
					Body->Write(Writer);
				}
				Writer.DoIndent();
				Writer << TEXT("}\n");
				break;

			case EIterationType::DoWhile:
				Writer << TEXT("do\n");
				Writer.DoIndent();
				Writer << TEXT("{\n");
				if (Body)
				{
					FASTWriterIncrementScope Scope(Writer);
					Body->Write(Writer);
				}
				Writer.DoIndent();
				Writer << TEXT("}\n");
				Writer.DoIndent();
				Writer << TEXT("while (");
				Condition->Write(Writer);
				Writer << TEXT(");\n");
				break;

			default:
				checkf(0, TEXT("Unhandled AST iteration type %d!"), (int32)Type);
				break;
			}
		}

		FIterationStatement::~FIterationStatement()
		{
			if (InitStatement)
			{
				delete InitStatement;
			}
			if (Condition)
			{
				delete Condition;
			}

			if (RestExpression)
			{
				delete RestExpression;
			}

			if (Body)
			{
				delete Body;
			}
		}

		FFunctionExpression::FFunctionExpression(FLinearAllocator* InAllocator, const FSourceInfo& InInfo, FExpression* InCallee) :
			FExpression(InAllocator, EOperators::FunctionCall, InInfo)
			, Callee(InCallee)
		{
		}

		void FFunctionExpression::Write(FASTWriter& Writer) const
		{
			Callee->Write(Writer);
			Writer << (TCHAR)'(';
			bool bFirst = true;
			for (auto* Expr : Expressions)
			{
				if (bFirst)
				{
					bFirst = false;
				}
				else
				{
					Writer << TEXT(", ");
				}
				Expr->Write(Writer);
			}
			Writer << TEXT(")");
		}

		FSwitchStatement::FSwitchStatement(FLinearAllocator* InAllocator, const FSourceInfo& InInfo, FExpression* InCondition, FSwitchBody* InBody) :
			FNode(InAllocator, InInfo),
			Condition(InCondition),
			Body(InBody)
		{
		}

		void FSwitchStatement::Write(FASTWriter& Writer) const
		{
			Writer.DoIndent();
			Writer << TEXT("switch (");
			Condition->Write(Writer);
			Writer << TEXT(")\n");
			Body->Write(Writer);
		}

		FSwitchStatement::~FSwitchStatement()
		{
			delete Condition;
			delete Body;
		}

		FSwitchBody::FSwitchBody(FLinearAllocator* InAllocator, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			CaseList(nullptr)
		{
		}

		void FSwitchBody::Write(FASTWriter& Writer) const
		{
			Writer.DoIndent();
			Writer << TEXT("{\n");
			{
				FASTWriterIncrementScope Scope(Writer);
				CaseList->Write(Writer);
			}
			Writer.DoIndent();
			Writer << TEXT("}\n");
		}

		FSwitchBody::~FSwitchBody()
		{
			delete CaseList;
		}

		FCaseLabel::FCaseLabel(FLinearAllocator* InAllocator, const FSourceInfo& InInfo, AST::FExpression* InExpression) :
			FNode(InAllocator, InInfo),
			TestExpression(InExpression)
		{
		}

		void FCaseLabel::Write(FASTWriter& Writer) const
		{
			Writer.DoIndent();
			if (TestExpression)
			{
				Writer << TEXT("case ");
				TestExpression->Write(Writer);
			}
			else
			{
				Writer << TEXT("default");
			}

			Writer << TEXT(":\n");
		}

		FCaseLabel::~FCaseLabel()
		{
			if (TestExpression)
			{
				delete TestExpression;
			}
		}


		FCaseStatement::FCaseStatement(FLinearAllocator* InAllocator, const FSourceInfo& InInfo, FCaseLabelList* InLabels) :
			FNode(InAllocator, InInfo),
			Labels(InLabels),
			Statements(InAllocator)
		{
		}

		void FCaseStatement::Write(FASTWriter& Writer) const
		{
			Labels->Write(Writer);

			if (Statements.Num() > 1)
			{
				Writer.DoIndent();
				Writer << TEXT("{\n");
				for (auto* Statement : Statements)
				{
					FASTWriterIncrementScope Scope(Writer);
					Statement->Write(Writer);
				}
				Writer.DoIndent();
				Writer << TEXT("}\n");
			}
			else if (Statements.Num() > 0)
			{
				FASTWriterIncrementScope Scope(Writer);
				Statements[0]->Write(Writer);
			}
		}

		FCaseStatement::~FCaseStatement()
		{
			delete Labels;
			for (auto* Statement : Statements)
			{
				if (Statement)
				{
					delete Statement;
				}
			}
		}

		FCaseLabelList::FCaseLabelList(FLinearAllocator* InAllocator, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Labels(InAllocator)
		{
		}

		void FCaseLabelList::Write(FASTWriter& Writer) const
		{
			for (auto* Label : Labels)
			{
				Label->Write(Writer);
			}
		}

		FCaseLabelList::~FCaseLabelList()
		{
			for (auto* Label : Labels)
			{
				delete Label;
			}
		}

		FCaseStatementList::FCaseStatementList(FLinearAllocator* InAllocator, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Cases(InAllocator)
		{
		}

		void FCaseStatementList::Write(FASTWriter& Writer) const
		{
			for (auto* Case : Cases)
			{
				Case->Write(Writer);
			}
		}

		FCaseStatementList::~FCaseStatementList()
		{
			for (auto* Case : Cases)
			{
				delete Case;
			}
		}

		FStructSpecifier::FStructSpecifier(FLinearAllocator* InAllocator, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			Name(nullptr),
			ParentName(nullptr),
			Members(InAllocator),
			bForwardDeclaration(false)
		{
		}

		void FStructSpecifier::Write(FASTWriter& Writer) const
		{
			Writer << TEXT("struct ");
			Writer << (Name ? Name : TEXT(""));
			if (ParentName && *ParentName)
			{
				Writer << TEXT(" : ");
				Writer << ParentName;
			}
			if (bForwardDeclaration)
			{
				Writer << TEXT(";\n");
			}
			else
			{
				Writer << TEXT('\n');
				Writer.DoIndent();
				Writer << TEXT("{\n");

				for (auto* Member : Members)
				{
					FASTWriterIncrementScope Scope(Writer);
					Member->Write(Writer);
				}

				Writer.DoIndent();
				Writer << TEXT("}");
			}
		}

		FStructSpecifier::~FStructSpecifier()
		{
			for (auto* Member : Members)
			{
				delete Member;
			}
		}

		FAttribute::FAttribute(FLinearAllocator* InAllocator, const FSourceInfo& InInfo, const TCHAR* InName) :
			FNode(InAllocator, InInfo),
			Name(InName),
			Arguments(InAllocator)
		{
		}

		void FAttribute::Write(FASTWriter& Writer) const
		{
			Writer << (TCHAR)'[';
			Writer << Name;

			bool bFirst = true;
			for (auto* Arg : Arguments)
			{
				if (bFirst)
				{
					Writer << (TCHAR)'(';
					bFirst = false;
				}
				else
				{
					Writer << TEXT(", ");
				}

				Arg->Write(Writer);
			}

			if (!bFirst)
			{
				Writer << TEXT(")");
			}

			Writer << TEXT("]");
		}

		FAttribute::~FAttribute()
		{
			for (auto* Arg : Arguments)
			{
				delete Arg;
			}
		}

		FAttributeArgument::FAttributeArgument(FLinearAllocator* InAllocator, const FSourceInfo& InInfo) :
			FNode(InAllocator, InInfo),
			StringArgument(nullptr),
			ExpressionArgument(nullptr)
		{
		}

		void FAttributeArgument::Write(FASTWriter& Writer) const
		{
			if (ExpressionArgument)
			{
				ExpressionArgument->Write(Writer);
			}
			else
			{
				Writer << (TCHAR)'"';
				Writer << StringArgument;
				Writer << (TCHAR)'"';
			}
		}

		FAttributeArgument::~FAttributeArgument()
		{
			if (ExpressionArgument)
			{
				delete ExpressionArgument;
			}
		}
	}
}
