// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading;
using System.Threading.Tasks;
using Horde.Server.Utilities;

namespace Horde.Server.Tests.Stubs.Collections
{
	class SingletonDocumentStub<T> : ISingletonDocument<T> where T : SingletonBase, new()
	{
		T _document = new T();

		public Task<T> GetAsync(CancellationToken cancellationToken)
		{
			return Task.FromResult(_document);
		}

		public Task<bool> TryUpdateAsync(T value, CancellationToken cancellationToken)
		{
			_document = value;
			return Task.FromResult(true);
		}
	}
}
