// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using System;
using System.Linq.Expressions;
using System.Reflection;

namespace EpicGames.Serialization.Converters
{
	class CbPrimitiveConverter<T> : CbConverterBase<T>, ICbConverterMethods
	{
		public MethodInfo ReadMethod { get; }
		public Func<CbField, T> ReadFunc { get; }

		public MethodInfo WriteMethod { get; }
		public Action<CbWriter, T> WriteFunc { get; }

		public MethodInfo WriteNamedMethod { get; }
		public Action<CbWriter, Utf8String, T> WriteNamedFunc { get; }

		public CbPrimitiveConverter(Expression<Func<CbField, T>> read, Expression<Action<CbWriter, T>> write, Expression<Action<CbWriter, Utf8String, T>> writeNamed)
		{
			ReadMethod = ((MethodCallExpression)read.Body).Method;
			ReadFunc = read.Compile();

			WriteMethod = ((MethodCallExpression)write.Body).Method;
			WriteFunc = write.Compile();

			WriteNamedMethod = ((MethodCallExpression)writeNamed.Body).Method;
			WriteNamedFunc = writeNamed.Compile();
		}

		public override T Read(CbField field) => ReadFunc(field);

		public override void Write(CbWriter writer, T value) => WriteFunc(writer, value);

		public override void WriteNamed(CbWriter writer, Utf8String name, T value) => WriteNamedFunc(writer, name, value);
	}
}
