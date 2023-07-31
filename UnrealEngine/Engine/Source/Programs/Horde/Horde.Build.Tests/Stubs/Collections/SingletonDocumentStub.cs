// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading.Tasks;
using Horde.Build.Server;
using Horde.Build.Utilities;

namespace Horde.Build.Tests.Stubs.Collections
{
	class SingletonDocumentStub<T> : ISingletonDocument<T> where T : SingletonBase, new()
	{
		T _document = new T();

		public Task<T> GetAsync()
		{
			return Task.FromResult(_document);
		}

		public Task<bool> TryUpdateAsync(T value)
		{
			_document = value;
			return Task.FromResult(true);
		}
	}
}
