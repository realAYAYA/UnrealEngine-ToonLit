// Copyright Epic Games, Inc. All Rights Reserved.

using Horde.Agent.Execution;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Agent.Tests;

[TestClass]
public class PerforceExecutorTest
{
	[TestMethod]
	public void ShouldUseHaveTable()
	{
		Assert.IsTrue(PerforceExecutor.ShouldUseHaveTable(null));
		Assert.IsTrue(PerforceExecutor.ShouldUseHaveTable(""));
		Assert.IsTrue(PerforceExecutor.ShouldUseHaveTable("name=managedWorkspace"));
		Assert.IsTrue(PerforceExecutor.ShouldUseHaveTable("name=managedWorkspace$#@!@#"));
		Assert.IsTrue(PerforceExecutor.ShouldUseHaveTable("name=managedWorkspace$#@!@#"));
		Assert.IsTrue(PerforceExecutor.ShouldUseHaveTable("name=managedWorkspace&useHaveTable=true"));
		
		Assert.IsFalse(PerforceExecutor.ShouldUseHaveTable("name=managedWorkspace&useHaveTable=false"));
		Assert.IsFalse(PerforceExecutor.ShouldUseHaveTable("name=ManagedWorkspace&useHaveTable=FalsE"));
	}
}
