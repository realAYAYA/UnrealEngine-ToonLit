// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Text;
using EpicGames.UHT.Types;
using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Exporters.CodeGen
{
	internal struct UhtMacroCreator : IDisposable
	{
		private readonly StringBuilder _builder;
		private readonly int _startingLength;

		public UhtMacroCreator(StringBuilder builder, UhtHeaderCodeGenerator generator, int lineNumber, string macroSuffix)
		{
			builder.Append("#define ").AppendMacroName(generator, lineNumber, macroSuffix).Append(" \\\r\n");
			_builder = builder;
			_startingLength = builder.Length;
		}

		public UhtMacroCreator(StringBuilder builder, UhtHeaderCodeGenerator generator, UhtClass classObj, string macroSuffix)
		{
			builder.Append("#define ").AppendMacroName(generator, classObj, macroSuffix).Append(" \\\r\n");
			_builder = builder;
			_startingLength = builder.Length;
		}

		public UhtMacroCreator(StringBuilder builder, UhtHeaderCodeGenerator generator, UhtScriptStruct scriptStruct, string macroSuffix)
		{
			builder.Append("#define ").AppendMacroName(generator, scriptStruct, macroSuffix).Append(" \\\r\n");
			_builder = builder;
			_startingLength = builder.Length;
		}

		public UhtMacroCreator(StringBuilder builder, UhtHeaderCodeGenerator generator, UhtFunction function, string macroSuffix)
		{
			builder.Append("#define ").AppendMacroName(generator, function, macroSuffix).Append(" \\\r\n");
			_builder = builder;
			_startingLength = builder.Length;
		}

		public void Dispose()
		{
			int finalLength = _builder.Length;
			if (finalLength < 4 ||
				_builder[finalLength - 4] != ' ' ||
				_builder[finalLength - 3] != '\\' ||
				_builder[finalLength - 2] != '\r' ||
				_builder[finalLength - 1] != '\n')
			{
				throw new UhtException("Macro line must end in ' \\\\\\r\\n'");
			}

			_builder.Length -= 4;
			if (finalLength == _startingLength)
			{
				_builder.Append("\r\n");
			}
			else
			{
				_builder.Append("\r\n\r\n\r\n");
			}
		}
	}
}
