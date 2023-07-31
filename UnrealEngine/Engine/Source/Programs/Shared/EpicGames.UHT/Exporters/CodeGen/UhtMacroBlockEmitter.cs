// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Text;

namespace EpicGames.UHT.Exporters.CodeGen
{
	internal struct UhtMacroBlockEmitter : IDisposable
	{
		private readonly StringBuilder _builder;
		private readonly string _macroName;
		private bool _bEmitted;

		public UhtMacroBlockEmitter(StringBuilder builder, string macroName, bool initialState = false)
		{
			_builder = builder;
			_macroName = macroName;
			_bEmitted = false;
			Set(initialState);
		}

		public void Set(bool emit)
		{
			if (_bEmitted == emit)
			{
				return;
			}
			_bEmitted = emit;
			if (_bEmitted)
			{
				_builder.Append("#if ").Append(_macroName).Append("\r\n");
			}
			else
			{
				_builder.Append("#endif // ").Append(_macroName).Append("\r\n");
			}
		}

		public void Dispose()
		{
			Set(false);
		}
	}
}
