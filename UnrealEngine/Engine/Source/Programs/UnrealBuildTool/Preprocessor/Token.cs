// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Core;

namespace UnrealBuildTool
{
	/// <summary>
	/// Flags for a single token
	/// </summary>
	[Flags]
	enum TokenFlags : byte
	{
		/// <summary>
		/// No flags
		/// </summary>
		None = 0x00,

		/// <summary>
		/// The token has space before it
		/// </summary>
		HasLeadingSpace = 0x01,

		/// <summary>
		/// Do not replace any instances of this token with the corresponding macro.
		/// </summary>
		DisableExpansion = 0x02,
	}

	/// <summary>
	/// Enumeration for token types
	/// </summary>
	enum TokenType : byte
	{
		End,
		LeftParen,
		RightParen,
		Comma,
		Identifier,
		Number,
		String,
		Character,
		Newline,
		Ellipsis,
		StringOfTokens,
		SystemInclude,
		Placemarker,
		Dot,
		QuestionMark,
		Colon,
		LogicalNot,
		LogicalAnd,
		LogicalOr,
		BitwiseXor,
		BitwiseAnd,
		BitwiseNot,
		BitwiseOr,
		Equals,
		LeftShift,
		RightShift,
		CompareEqual,
		CompareNotEqual,
		CompareLessOrEqual,
		CompareLess,
		CompareGreaterOrEqual,
		CompareGreater,
		Plus,
		Minus,
		Multiply,
		Divide,
		Modulo,
		Hash,
		HashHash,
		Unknown,
		Max
	}

	/// <summary>
	/// Single lexical token
	/// </summary>
	[DebuggerDisplay("{Text}")]
	sealed class Token : IEquatable<Token>
	{
		/// <summary>
		/// Static token names
		/// </summary>
		static string[] StaticTokens;

		/// <summary>
		/// Contains the type of the token
		/// </summary>
		public TokenType Type
		{
			get;
		}

		/// <summary>
		/// Properties for this token
		/// </summary>
		public TokenFlags Flags
		{
			get;
		}

		/// <summary>
		/// If this token is identifier, contains the identifier name
		/// </summary>
		public Identifier? Identifier
		{
			get;
		}

		/// <summary>
		/// If this token is a literal, contains the raw utf-8 representation of it.
		/// </summary>
		public byte[]? Literal
		{
			get;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Type">The token type</param>
		/// <param name="Flags">Flags for this token</param>
		public Token(TokenType Type, TokenFlags Flags)
		{
			this.Type = Type;
			this.Flags = Flags;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Identifier">The token identifier</param>
		/// <param name="Flags">Flags for this token</param>
		public Token(Identifier Identifier, TokenFlags Flags)
		{
			this.Type = TokenType.Identifier;
			this.Flags = Flags;
			this.Identifier = Identifier;
		}

		/// <summary>
		/// Constructs a literal token
		/// </summary>
		public Token(TokenType Type, TokenFlags Flags, byte[] Literal) 
			: this(Type, Flags)
		{
			Debug.Assert(IsLiteral(Type));
			this.Literal = Literal;
		}

		/// <summary>
		/// Constructs a literal token
		/// </summary>
		public Token(TokenType Type, TokenFlags Flags, string Literal) 
			: this(Type, Flags)
		{
			Debug.Assert(IsLiteral(Type));
			this.Literal = Encoding.UTF8.GetBytes(Literal);
		}

		/// <summary>
		/// Reads a token from a binary archive
		/// </summary>
		/// <param name="Reader">Archive to read from</param>
		public Token(BinaryArchiveReader Reader)
		{
			Type = (TokenType)Reader.ReadByte();
			Flags = (TokenFlags)Reader.ReadByte();
			if(Type == TokenType.Identifier)
			{
				Identifier = Reader.ReadIdentifier();
			}
			else if(IsLiteral(Type))
			{
				Literal = Reader.ReadByteArray();
			}
		}

		/// <summary>
		/// Writes a token to a binary archive
		/// </summary>
		/// <param name="Writer">The writer to serialize to</param>
		public void Write(BinaryArchiveWriter Writer)
		{
			Writer.WriteByte((byte)Type);
			Writer.WriteByte((byte)Flags);
			if(Type == TokenType.Identifier)
			{
				Writer.WriteIdentifier(Identifier!);
			}
			else if(IsLiteral(Type))
			{
				Writer.WriteByteArray(Literal);
			}
		}

		/// <summary>
		/// Checks if a token is equal to another object
		/// </summary>
		/// <param name="Other">The object to compare against</param>
		/// <returns>True if the objects are equal, false otherwise</returns>
		public override bool Equals(object? Other)
		{
			return Equals(Other as Token);
		}

		/// <summary>
		/// Checks if two tokens are equivalent
		/// </summary>
		/// <param name="Other">The object to compare against</param>
		/// <returns>True if the tokens are equal, false otherwise</returns>
		public bool Equals(Token? Other)
		{
			if(ReferenceEquals(Other, null))
			{
				return false;
			}
			if(Type != Other.Type || Flags != Other.Flags || Identifier != Other.Identifier)
			{
				return false;
			}
			if(Literal != null)
			{
				if(Other.Literal == null || Literal.Length != Other.Literal.Length || !Enumerable.SequenceEqual(Literal, Other.Literal))
				{
					return false;
				}
			}
			else
			{
				if(Other.Literal != null)
				{
					return false;
				}
			}
			return true;
		}

		/// <summary>
		/// Compares two tokens for equality
		/// </summary>
		/// <param name="A">The first token to compare</param>
		/// <param name="B">The second token to compare</param>
		/// <returns>True if the objects are equal, false otherwise</returns>
		public static bool operator==(Token A, Token B)
		{
			if((object)A == null)
			{
				return ((object)B == null);
			}
			else
			{
				return A.Equals(B);
			}
		}

		/// <summary>
		/// Compares two tokens for inequality
		/// </summary>
		/// <param name="A">The first token to compare</param>
		/// <param name="B">The second token to compare</param>
		/// <returns>True if the objects are not equal, false otherwise</returns>
		public static bool operator!=(Token A, Token B)
		{
			return !(A == B);
		}

		/// <summary>
		/// Gets a hash code for this object
		/// </summary>
		/// <returns>Hash code for the object</returns>
		public override int GetHashCode()
		{
			int Result = (int)Type + (int)Flags * 7;
			if(Identifier != null)
			{
				Result = (Result * 11) + Identifier.GetHashCode();
			}
			if(Literal != null)
			{
				for(int Idx = 0; Idx < Literal.Length; Idx++)
				{
					Result = (Result * 13) + Literal[Idx];
				}
			}
			return base.GetHashCode();
		}

		/// <summary>
		/// Text corresponding to this token
		/// </summary>
		public string Text
		{
			get
			{
				if(Identifier != null)
				{
					return Identifier.ToString();
				}
				else if(Literal != null)
				{
					return Encoding.UTF8.GetString(Literal);
				}
				else
				{
					return StaticTokens[(int)Type];
				}
			}
		}

		/// <summary>
		/// Returns a new token with different flags
		/// </summary>
		/// <param name="FlagsToAdd">Flags to add from the token</param>
		/// <returns>New token with updated flags</returns>
		public Token AddFlags(TokenFlags FlagsToAdd)
		{
			if(Identifier != null)
			{
				return new Token(Identifier, Flags | FlagsToAdd);
			}
			else if(Literal != null)
			{
				return new Token(Type, Flags | FlagsToAdd, Literal);
			}
			else
			{
				return new Token(Type, Flags | FlagsToAdd);
			}
		}

		/// <summary>
		/// Returns a new token with different flags
		/// </summary>
		/// <param name="FlagsToRemove">Flags to remove from the token</param>
		/// <returns>New token with updated flags</returns>
		public Token RemoveFlags(TokenFlags FlagsToRemove)
		{
			if(Identifier != null)
			{
				return new Token(Identifier, Flags & ~FlagsToRemove);
			}
			else if(Literal != null)
			{
				return new Token(Type, Flags & ~FlagsToRemove, Literal);
			}
			else
			{
				return new Token(Type, Flags & ~FlagsToRemove);
			}
		}

		/// <summary>
		/// Accessor for whether this token has leading whitespace
		/// </summary>
		public bool HasLeadingSpace
		{
			get { return (Flags & TokenFlags.HasLeadingSpace) != 0; }
		}

		/// <summary>
		/// Checks whether two tokens match
		/// </summary>
		/// <param name="Other">The token to compare against</param>
		/// <returns>True if the tokens match, false otherwise</returns>
		public bool Matches(Token Other)
		{
			if(Type != Other.Type)
			{
				return false;
			}

			if(Literal == null)
			{
				return Identifier == Other.Identifier;
			}
			else 
			{
				return Other.Literal != null && Enumerable.SequenceEqual(Literal, Other.Literal);
			}
		}

		/// <summary>
		/// Determines whether the given token type is a literal
		/// </summary>
		/// <param name="Type">The type to test</param>
		/// <returns>True if the given type is a literal</returns>
		public static bool IsLiteral(TokenType Type)
		{
			return Type == TokenType.Unknown || Type == TokenType.Character || Type == TokenType.String || Type == TokenType.Number || Type == TokenType.StringOfTokens || Type == TokenType.SystemInclude;
		}

		/// <summary>
		/// Concatenate a sequence of tokens into a string
		/// </summary>
		/// <param name="Tokens">The sequence of tokens to concatenate</param>
		/// <returns>String containing the concatenated tokens</returns>
		public static string Format(IEnumerable<Token> Tokens)
		{
			StringBuilder Result = new StringBuilder();
			Format(Tokens, Result);
			return Result.ToString();
		}

		/// <summary>
		/// Concatenate a sequence of tokens into a string
		/// </summary>
		/// <param name="Tokens">The sequence of tokens to concatenate</param>
		/// <param name="Result">Receives the formatted string</param>
		public static void Format(IEnumerable<Token> Tokens, StringBuilder Result)
		{
			IEnumerator<Token> Enumerator = Tokens.GetEnumerator();
			if(Enumerator.MoveNext())
			{
				Result.Append(Enumerator.Current.Text);

				Token LastToken = Enumerator.Current;
				while(Enumerator.MoveNext())
				{
					Token Token = Enumerator.Current;
					if(Token.HasLeadingSpace && (Token.Type != TokenType.LeftParen || LastToken.Type != TokenType.Identifier || LastToken.Identifier != Identifiers.__pragma))
					{
						Result.Append(" ");
					}
					Result.Append(Token.Text);
					LastToken = Token;
				}
			}
		}

		/// <summary>
		/// Concatenate two tokens together
		/// </summary>
		/// <param name="FirstToken">The first token</param>
		/// <param name="SecondToken">The second token</param>
		/// <param name="Context">Current preprocessor context</param>
		/// <returns>The combined token</returns>
		public static Token Concatenate(Token FirstToken, Token SecondToken, PreprocessorContext Context)
		{
			string Text = FirstToken.Text.ToString() + SecondToken.Text.ToString();

			List<Token> Tokens = new List<Token>();

			TokenReader Reader = new TokenReader(Text);
			while(Reader.MoveNext())
			{
				Tokens.Add(Reader.Current);
			}

			if(Tokens.Count == 0)
			{
				return new Token(TokenType.Placemarker, TokenFlags.None);
			}
			else if(Tokens.Count == 1)
			{
				return Tokens[0];
			}
			else
			{
				return new Token(TokenType.Unknown, TokenFlags.None, Text);
			}
		}

		/// <summary>
		/// Static constructor. Initializes the StaticTokens array.
		/// </summary>
		static Token()
		{
			StaticTokens = new string[(int)TokenType.Max];
			StaticTokens[(int)TokenType.LeftParen] = "(";
			StaticTokens[(int)TokenType.RightParen] = ")";
			StaticTokens[(int)TokenType.Comma] = ",";
			StaticTokens[(int)TokenType.Newline] = "\n";
			StaticTokens[(int)TokenType.Ellipsis] = "...";
			StaticTokens[(int)TokenType.Placemarker] = "";
			StaticTokens[(int)TokenType.Dot] = ".";
			StaticTokens[(int)TokenType.QuestionMark] = "?";
			StaticTokens[(int)TokenType.Colon] = ":";
			StaticTokens[(int)TokenType.LogicalNot] = "!";
			StaticTokens[(int)TokenType.LogicalAnd] = "&&";
			StaticTokens[(int)TokenType.LogicalOr] = "||";
			StaticTokens[(int)TokenType.BitwiseXor] = "^";
			StaticTokens[(int)TokenType.BitwiseAnd] = "&";
			StaticTokens[(int)TokenType.BitwiseNot] = "~";
			StaticTokens[(int)TokenType.BitwiseOr] = "|";
			StaticTokens[(int)TokenType.Equals] = "=";
			StaticTokens[(int)TokenType.LeftShift] = "<<";
			StaticTokens[(int)TokenType.RightShift] = ">>";
			StaticTokens[(int)TokenType.CompareEqual] = "==";
			StaticTokens[(int)TokenType.CompareNotEqual] = "!=";
			StaticTokens[(int)TokenType.CompareLessOrEqual] = "<=";
			StaticTokens[(int)TokenType.CompareLess] = "<";
			StaticTokens[(int)TokenType.CompareGreaterOrEqual] = ">=";
			StaticTokens[(int)TokenType.CompareGreater] = ">";
			StaticTokens[(int)TokenType.Plus] = "+";
			StaticTokens[(int)TokenType.Minus] = "-";
			StaticTokens[(int)TokenType.Multiply] = "*";
			StaticTokens[(int)TokenType.Divide] = "/";
			StaticTokens[(int)TokenType.Modulo] = "%";
			StaticTokens[(int)TokenType.Hash] = "#";
			StaticTokens[(int)TokenType.HashHash] = "##";
		}
	}

	/// <summary>
	/// Helper functions for serialization
	/// </summary>
	static class TokenExtensionMethods
	{
		/// <summary>
		/// Read a token from a binary archive
		/// </summary>
		/// <param name="Reader">Reader to serialize data from</param>
		/// <returns>Token read from the archive</returns>
		public static Token ReadToken(this BinaryArchiveReader Reader)
		{
			return new Token(Reader);
		}

		/// <summary>
		/// Write a token to a binary archive
		/// </summary>
		/// <param name="Writer">Writer to serialize data to</param>
		/// <param name="Token">Token to write</param>
		public static void WriteToken(this BinaryArchiveWriter Writer, Token Token)
		{
			Token.Write(Writer);
		}
	}
}
