// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealBuildTool
{
	/// <summary>
	/// Class to allow evaluating a preprocessor expression
	/// </summary>
	static class PreprocessorExpression
	{
		/// <summary>
		/// Precedence table for binary operators (indexed by TokenType). Lower values indicate higher precedence.
		/// </summary>
		static byte[] Precedence;

		/// <summary>
		/// Static constructor. Initializes the Precedence table.
		/// </summary>
		static PreprocessorExpression()
		{
			Precedence = new byte[(int)TokenType.Max];
			Precedence[(int)TokenType.Multiply] = 10;
			Precedence[(int)TokenType.Divide] = 10;
			Precedence[(int)TokenType.Modulo] = 10;
			Precedence[(int)TokenType.Plus] = 9;
			Precedence[(int)TokenType.Minus] = 9;
			Precedence[(int)TokenType.LeftShift] = 8;
			Precedence[(int)TokenType.RightShift] = 8;
			Precedence[(int)TokenType.CompareLess] = 7;
			Precedence[(int)TokenType.CompareLessOrEqual] = 7;
			Precedence[(int)TokenType.CompareGreater] = 7;
			Precedence[(int)TokenType.CompareGreaterOrEqual] = 7;
			Precedence[(int)TokenType.CompareEqual] = 6;
			Precedence[(int)TokenType.CompareNotEqual] = 6;
			Precedence[(int)TokenType.BitwiseAnd] = 5;
			Precedence[(int)TokenType.BitwiseXor] = 4;
			Precedence[(int)TokenType.BitwiseOr] = 3;
			Precedence[(int)TokenType.LogicalAnd] = 2;
			Precedence[(int)TokenType.LogicalOr] = 1;
		}

		/// <summary>
		/// Evaluate a preprocessor expression
		/// </summary>
		/// <param name="Context">The current preprocessor context</param>
		/// <param name="Tokens">List of tokens in the expression</param>
		/// <returns>Value of the expression</returns>
		public static long Evaluate(PreprocessorContext Context, List<Token> Tokens)
		{
			int Idx = 0;
			long Result = EvaluateTernary(Context, Tokens, ref Idx);
			if (Idx != Tokens.Count)
			{
				throw new PreprocessorException(Context, $"Garbage after end of expression: {Token.Format(Tokens)}");
			}
			return Result;
		}

		/// <summary>
		/// Evaluates a ternary expression (a? b :c)
		/// </summary>
		/// <param name="Context">The current preprocessor context</param>
		/// <param name="Tokens">List of tokens in the expression</param>
		/// <param name="Idx">Index into the token list of the next token to read</param>
		/// <returns>Value of the expression</returns>
		static long EvaluateTernary(PreprocessorContext Context, List<Token> Tokens, ref int Idx)
		{
			long Result = EvaluateUnary(Context, Tokens, ref Idx);
			if(Idx < Tokens.Count && Precedence[(int)Tokens[Idx].Type] != 0)
			{
				Result = EvaluateBinary(Context, Tokens, ref Idx, Result, 1);
			}
			if(Idx < Tokens.Count && Tokens[Idx].Type == TokenType.QuestionMark)
			{
				// Read the left expression
				Idx++;
				long Lhs = EvaluateTernary(Context, Tokens, ref Idx);

				// Check for the colon in the middle
				if(Tokens[Idx].Type != TokenType.Colon)
				{
					throw new PreprocessorException(Context, $"Expected colon for conditional operator, found {Tokens[Idx].Text}");
				}

				// Read the right expression
				Idx++;
				long Rhs = EvaluateTernary(Context, Tokens, ref Idx);

				// Evaluate it
				Result = (Result != 0) ? Lhs : Rhs;
			}
			return Result;
		}

		/// <summary>
		/// Recursive path for evaluating a sequence of binary operators, given a known LHS. Implementation of the shunting yard algorithm.
		/// </summary>
		/// <param name="Context">The current preprocessor context</param>
		/// <param name="Tokens">List of tokens in the expression</param>
		/// <param name="Idx">Index into the token list of the next token to read</param>
		/// <param name="Lhs">The LHS value for the binary expression</param>
		/// <param name="MinPrecedence">Minimum precedence value to consume</param>
		/// <returns>Value of the expression</returns>
		static long EvaluateBinary(PreprocessorContext Context, List<Token> Tokens, ref int Idx, long Lhs, byte MinPrecedence)
		{
			while(Idx < Tokens.Count)
			{
				// Read the next operator token
				Token Operator = Tokens[Idx];

				// Check if this operator binds more tightly than the outer expression, and exit if it does.
				int OperatorPrecedence = Precedence[(int)Operator.Type];
				if(OperatorPrecedence < MinPrecedence)
				{
					break;
				}

				// Move to the next token
				Idx++;

				// Evaluate the immediate RHS value, then recursively evaluate any subsequent binary operators that bind more tightly to it than this.
				long Rhs = EvaluateUnary(Context, Tokens, ref Idx);
				while(Idx < Tokens.Count)
				{
					byte NextOperatorPrecedence = Precedence[(int)Tokens[Idx].Type];
					if(NextOperatorPrecedence <= OperatorPrecedence)
					{
						break;
					}
					Rhs = EvaluateBinary(Context, Tokens, ref Idx, Rhs, NextOperatorPrecedence);
				}

				// Evalute the result of applying the operator to the LHS and RHS values
				switch(Operator.Type)
				{
					case TokenType.LogicalOr:
						Lhs = ((Lhs != 0) || (Rhs != 0))? 1 : 0;
						break;
					case TokenType.LogicalAnd:
						Lhs = ((Lhs != 0) && (Rhs != 0))? 1 : 0;
						break;
					case TokenType.BitwiseOr:
						Lhs |= Rhs;
						break;
					case TokenType.BitwiseXor:
						Lhs ^= Rhs;
						break;
					case TokenType.BitwiseAnd:
						Lhs &= Rhs;
						break;
					case TokenType.CompareEqual:
						Lhs = (Lhs == Rhs)? 1 : 0;
						break;
					case TokenType.CompareNotEqual:
						Lhs = (Lhs != Rhs)? 1 : 0;
						break;
					case TokenType.CompareLess:
						Lhs = (Lhs < Rhs)? 1 : 0;
						break;
					case TokenType.CompareGreater:
						Lhs = (Lhs > Rhs)? 1 : 0;
						break;
					case TokenType.CompareLessOrEqual:
						Lhs = (Lhs <= Rhs)? 1 : 0;
						break;
					case TokenType.CompareGreaterOrEqual:
						Lhs = (Lhs >= Rhs)? 1 : 0;
						break;
					case TokenType.LeftShift:
						Lhs <<= (int)Rhs;
						break;
					case TokenType.RightShift:
						Lhs >>= (int)Rhs;
						break;
					case TokenType.Plus:
						Lhs += Rhs;
						break;
					case TokenType.Minus:
						Lhs -= Rhs;
						break;
					case TokenType.Multiply:
						Lhs *= Rhs;
						break;
					case TokenType.Divide:
						Lhs /= Rhs;
						break;
					case TokenType.Modulo:
						Lhs %= Rhs;
						break;
					default:
						throw new NotImplementedException($"Binary operator '{Operator.Type}' has not been implemented");
				}
			}

			return Lhs;
		}

		/// <summary>
		/// Evaluates a unary expression (+, -, !, ~)
		/// </summary>
		/// <param name="Context">The current preprocessor context</param>
		/// <param name="Tokens">List of tokens in the expression</param>
		/// <param name="Idx">Index into the token list of the next token to read</param>
		/// <returns>Value of the expression</returns>
		static long EvaluateUnary(PreprocessorContext Context, List<Token> Tokens, ref int Idx)
		{
			if(Idx == Tokens.Count)
			{
				throw new PreprocessorException(Context, "Early end of expression");
			}

			switch(Tokens[Idx].Type)
			{
				case TokenType.Plus:
					Idx++;
					return +EvaluateUnary(Context, Tokens, ref Idx);
				case TokenType.Minus:
					Idx++;
					return -EvaluateUnary(Context, Tokens, ref Idx);
				case TokenType.LogicalNot:
					Idx++;
					return (EvaluateUnary(Context, Tokens, ref Idx) == 0) ? 1 : 0;
				case TokenType.BitwiseNot:
					Idx++;
					return ~EvaluateUnary(Context, Tokens, ref Idx);
				case TokenType.Identifier:
					Identifier Text = Tokens[Idx].Identifier!;
					if(Text == Identifiers.Sizeof)
					{
						throw new NotImplementedException(Text.ToString());
					}
					else if(Text == Identifiers.Alignof)
					{
						throw new NotImplementedException(Text.ToString());
					}
					else if (Text == Identifiers.__has_builtin || Text == Identifiers.__has_feature
						|| Text == Identifiers.__has_warning || Text == Identifiers.__building_module
						|| Text == Identifiers.__pragma || Text == Identifiers.__builtin_return_address
						|| Text == Identifiers.__builtin_frame_address || Text == Identifiers.__has_keyword
						|| Text == Identifiers.__has_extension || Text == Identifiers.__is_target_arch
						|| Text == Identifiers.__is_identifier)
					{
						if (Tokens[Idx + 1].Type != TokenType.LeftParen || Tokens[Idx + 3].Type != TokenType.RightParen)
						{
							throw new NotImplementedException(Text.ToString());
						}
						Idx += 4;
						return 0;
					}
					else if (Text == Identifiers.__has_attribute || Text == Identifiers.__has_declspec_attribute
						|| Text == Identifiers.__has_c_attribute || Text == Identifiers.__has_cpp_attribute
						|| Text == Identifiers.__has_include || Text == Identifiers.__has_include_next)
					{
						Idx += Tokens.Count - Idx;
						return 0;
					}

					else
					{
						return EvaluatePrimary(Context, Tokens, ref Idx);
					}
				default:
					return EvaluatePrimary(Context, Tokens, ref Idx);
			}
		}

		/// <summary>
		/// Evaluates a primary expression
		/// </summary>
		/// <param name="Context">The current preprocessor context</param>
		/// <param name="Tokens">List of tokens in the expression</param>
		/// <param name="Idx">Index into the token list of the next token to read</param>
		/// <returns>Value of the expression</returns>
		static long EvaluatePrimary(PreprocessorContext Context, List<Token> Tokens, ref int Idx)
		{
			if(Tokens[Idx].Type == TokenType.Identifier)
			{
				Idx++;
				return 0;
			}
			else if(Tokens[Idx].Type == TokenType.LeftParen)
			{
				// Read the expression
				Idx++;
				long Result = EvaluateTernary(Context, Tokens, ref Idx);

				// Check for a closing parenthesis
				if (Tokens[Idx].Type != TokenType.RightParen)
				{
					throw new PreprocessorException(Context, "Missing closing parenthesis");
				}

				// Return the value
				Idx++;
				return Result;
			}
			else if(Tokens[Idx].Type == TokenType.Number)
			{
				return ParseNumericLiteral(Context, Tokens[Idx++].Literal!);
			}
			else
			{
				throw new PreprocessorException(Context, "Unexpected token in expression: {0}", Tokens[Idx]);
			}
		}

		/// <summary>
		/// Parse a numeric literal from the given token
		/// </summary>
		/// <param name="Context">The current preprocessor context</param>
		/// <param name="Literal">The utf-8 literal characters</param>
		/// <returns>The literal value of the token</returns>
		static long ParseNumericLiteral(PreprocessorContext Context, byte[] Literal)
		{
			// Parse the token
			long Value = 0;
			if(Literal.Length >= 2 && Literal[0] == '0' && (Literal[1] == 'x' || Literal[1] == 'X'))
			{
				for (int Idx = 2; Idx < Literal.Length; Idx++)
				{
					if (Literal[Idx] >= '0' && Literal[Idx] <= '9')
					{
						Value = (Value << 4) + (Literal[Idx] - '0');
					}
					else if (Literal[Idx] >= 'a' && Literal[Idx] <= 'f')
					{
						Value = (Value << 4) + (Literal[Idx] - 'a') + 10;
					}
					else if (Literal[Idx] >= 'A' && Literal[Idx] <= 'F')
					{
						Value = (Value << 4) + (Literal[Idx] - 'A') + 10;
					}
					else if(IsValidIntegerSuffix(Literal, Idx))
					{
						break;
					}
					else
					{
						throw new PreprocessorException(Context, "Invalid hexadecimal literal: '{0}'", Encoding.UTF8.GetString(Literal));
					}
				}
			}
			else if(Literal[0] == '0')
			{
				for (int Idx = 1; Idx < Literal.Length; Idx++)
				{
					if (Literal[Idx] >= '0' && Literal[Idx] <= '7')
					{
						Value = (Value << 3) + (Literal[Idx] - '0');
					}
					else if(IsValidIntegerSuffix(Literal, Idx))
					{
						break;
					}
					else
					{
						throw new PreprocessorException(Context, "Invalid octal literal: '{0}'", Encoding.UTF8.GetString(Literal));
					}
				}
			}
			else
			{
				for (int Idx = 0; Idx < Literal.Length; Idx++)
				{
					if (Literal[Idx] >= '0' && Literal[Idx] <= '9')
					{
						Value = (Value * 10) + (Literal[Idx] - '0');
					}
					else if(IsValidIntegerSuffix(Literal, Idx))
					{
						break;
					}
					else
					{
						throw new PreprocessorException(Context, "Invalid decimal literal: '{0}'", Encoding.UTF8.GetString(Literal));
					}
				}
			}
			return Value;
		}

		/// <summary>
		/// Checks whether the given literal has a valid integer suffix, starting at the given position
		/// </summary>
		/// <param name="Literal">The literal token</param>
		/// <param name="Index">Index within the literal to start reading</param>
		/// <returns>The literal value of the token</returns>
		static bool IsValidIntegerSuffix(byte[] Literal, int Index)
		{
			bool bAllowTrailingUnsigned = true;

			if(Literal[Index] == 'u' || Literal[Index] == 'U')
			{
				// 'U'
				Index++;
				bAllowTrailingUnsigned = false;
			}

			if(Index < Literal.Length && (Literal[Index] == 'l' || Literal[Index] == 'L'))
			{
				// 'LL' or 'L'
				if(Index + 1 < Literal.Length && Literal[Index + 1] == Literal[Index])
				{
					Index += 2;
				}
				else
				{
					Index++;
				}
			}
			else if (Index + 2 < Literal.Length && (Literal[Index] == 'i' || Literal[Index] == 'I') && Literal[Index + 1] == '3' && Literal[Index + 2] == '2')
			{
				// 'I32' (Microsoft extension)
				Index += 3;
				bAllowTrailingUnsigned = false;
			}
			else if(Index + 2 < Literal.Length && (Literal[Index] == 'i' || Literal[Index] == 'I') && Literal[Index + 1] == '6' && Literal[Index + 2] == '4')
			{
				// 'I64' (Microsoft extension)
				Index += 3;
				bAllowTrailingUnsigned = false;
			}

			if(bAllowTrailingUnsigned && Index < Literal.Length && (Literal[Index] == 'u' || Literal[Index] == 'U'))
			{
				// 'U'
				Index++;
			}

			return Index == Literal.Length;
		}
	}
}
