// Copyright Epic Games, Inc. All Rights Reserved.

using Horde.Agent.Utility;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Agent.Tests;

[TestClass]
public class WorkspaceInfoTest
{
	[TestMethod]
	public void ShouldUseHaveTable()
	{
		Assert.IsTrue(WorkspaceInfo.ShouldUseHaveTable(null));
		Assert.IsTrue(WorkspaceInfo.ShouldUseHaveTable(""));
		Assert.IsTrue(WorkspaceInfo.ShouldUseHaveTable("name=managedWorkspace"));
		Assert.IsTrue(WorkspaceInfo.ShouldUseHaveTable("name=managedWorkspace$#@!@#"));
		Assert.IsTrue(WorkspaceInfo.ShouldUseHaveTable("name=managedWorkspace$#@!@#"));
		Assert.IsTrue(WorkspaceInfo.ShouldUseHaveTable("name=managedWorkspace&useHaveTable=true"));
		
		Assert.IsFalse(WorkspaceInfo.ShouldUseHaveTable("name=managedWorkspace&useHaveTable=false"));
		Assert.IsFalse(WorkspaceInfo.ShouldUseHaveTable("name=ManagedWorkspace&useHaveTable=FalsE"));
	}
}
