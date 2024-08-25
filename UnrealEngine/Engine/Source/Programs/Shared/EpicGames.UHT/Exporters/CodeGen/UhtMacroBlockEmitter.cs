// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Text;
using EpicGames.UHT.Types;

namespace EpicGames.UHT.Exporters.CodeGen
{
	internal struct UhtMacroBlockEmitter : IDisposable
	{
		private readonly StringBuilder _builder;
		private readonly UhtDefineScopeNames _defineScopeNames;
		private UhtDefineScope _current = UhtDefineScope.None;

		public UhtMacroBlockEmitter(StringBuilder builder, UhtDefineScopeNames defineScopeNames, UhtDefineScope initialState)
		{
			_builder = builder;
			_defineScopeNames = defineScopeNames;
			Set(initialState);
		}

		public void Set(UhtDefineScope defineScope)
		{
			if (defineScope == UhtDefineScope.Invalid)
			{
				defineScope = UhtDefineScope.None;
			}
			if (_current == defineScope)
			{
				return;
			}
			_builder.AppendEndIfPreprocessor(_current, _defineScopeNames);
			_builder.AppendIfPreprocessor(defineScope, _defineScopeNames);
			_current = defineScope;
		}

		public void Dispose()
		{
			Set(UhtDefineScope.None);
		}
	}
}
