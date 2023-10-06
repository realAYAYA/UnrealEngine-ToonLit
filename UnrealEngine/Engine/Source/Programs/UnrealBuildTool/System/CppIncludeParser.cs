// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Text;

namespace UnrealBuildTool
{
	/// <summary>
	/// Parses include directives from cpp source files, to make dedicated PCHs
	/// </summary>
	static class CppIncludeParser
	{
		/// <summary>
		/// Copies include directives from one file to another, until an unsafe directive (or non-#include line) is found.
		/// </summary>
		/// <param name="Reader">Stream to read from</param>
		/// <param name="Writer">Stream to write directives to</param>
		public static void CopyIncludeDirectives(StreamReader Reader, StringBuilder Writer)
		{
			StringBuilder Token = new StringBuilder();
			while (TryReadToken(Reader, Token))
			{
				if (Token.Length > 1 || Token[0] != '\n')
				{
					if (Token.Length != 1 || Token[0] != '#')
					{
						break;
					}
					if (!TryReadToken(Reader, Token))
					{
						break;
					}

					string Directive = Token.ToString();
					if (Directive == "pragma")
					{
						if (!TryReadToken(Reader, Token) || Token.ToString() != "once")
						{
							break;
						}
						if (!TryReadToken(Reader, Token) || Token.ToString() != "\n")
						{
							break;
						}
					}
					else if (Directive == "include")
					{
						if (!TryReadToken(Reader, Token) || Token[0] != '\"')
						{
							break;
						}

						string IncludeFile = Token.ToString();

						if (!IncludeFile.EndsWith(".h\"") && !IncludeFile.EndsWith(".h>"))
						{
							break;
						}
						if (IncludeFile.Equals("\"RequiredProgramMainCPPInclude.h\"", StringComparison.OrdinalIgnoreCase))
						{
							break;
						}
						if (!TryReadToken(Reader, Token) || Token.ToString() != "\n")
						{
							break;
						}

						Writer.Append("#include ").AppendLine(IncludeFile);
					}
					else
					{
						break;
					}
				}
			}
		}

		/// <summary>
		/// Reads an individual token from the input stream
		/// </summary>
		/// <param name="Reader">Stream to read from</param>
		/// <param name="Token">Buffer to store token read from the stream</param>
		/// <returns>True if a token was read, false otherwise</returns>
		static bool TryReadToken(StreamReader Reader, StringBuilder Token)
		{
			Token.Clear();

			int NextChar;
			for (; ; )
			{
				NextChar = Reader.Read();
				if (NextChar == -1)
				{
					return false;
				}
				if (NextChar != ' ' && NextChar != '\t' && NextChar != '\r')
				{
					if (NextChar != '/')
					{
						break;
					}
					else if (Reader.Peek() == '/')
					{
						Reader.Read();
						for (; ; )
						{
							NextChar = Reader.Read();
							if (NextChar == -1)
							{
								return false;
							}
							if (NextChar == '\n')
							{
								break;
							}
						}
					}
					else if (Reader.Peek() == '*')
					{
						Reader.Read();
						for (; ; )
						{
							NextChar = Reader.Read();
							if (NextChar == -1)
							{
								return false;
							}
							if (NextChar == '*' && Reader.Peek() == '/')
							{
								break;
							}
						}
						Reader.Read();
					}
					else
					{
						break;
					}
				}
			}

			Token.Append((char)NextChar);

			if (Char.IsLetterOrDigit((char)NextChar))
			{
				for (; ; )
				{
					NextChar = Reader.Read();
					if (NextChar == -1 || !Char.IsLetterOrDigit((char)NextChar))
					{
						break;
					}
					Token.Append((char)NextChar);
				}
			}
			else if (NextChar == '\"' || NextChar == '<')
			{
				for (; ; )
				{
					NextChar = Reader.Read();
					if (NextChar == -1)
					{
						break;
					}
					Token.Append((char)NextChar);
					if (NextChar == '\"' || NextChar == '>')
					{
						break;
					}
				}
			}

			return true;
		}
	}
}
