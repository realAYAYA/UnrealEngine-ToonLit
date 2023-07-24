// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace EpicGames.Perforce.Managed.Tests;

[TestClass]
public class DepotStreamTreeBuilderTest
{
	private readonly StreamFile _file1 = new ("//UE5/Main/main.cpp", 100, new FileContentId(Md5Hash.Zero, "text"), 10);
	private readonly StreamFile _file2 = new ("//UE5/Main/Data/data2.bin", 200, new FileContentId(Md5Hash.Zero, "binary"), 20);
	private readonly StreamFile _file3 = new ("//UE5/Main/Data/data3.bin", 300, new FileContentId(Md5Hash.Zero, "binary"), 30);
	private readonly StreamFile _file4 = new ("//UE5/Main/Data/Audio/Samples/data4.bin", 400, new FileContentId(Md5Hash.Zero, "binary"), 40);
	
	[TestMethod]
	public void Basic()
	{
		DepotStreamTreeBuilder builder = new ();
		builder.AddFile("/main.cpp", _file1);
		builder.AddFile("Data/data2.bin", _file2);
		builder.AddFile("Data/data3.bin", _file3);
		builder.AddFile("Data/Audio/Samples/data4.bin", _file4);
		StreamSnapshotFromMemory snapshot = new (builder);

		StreamTree root = snapshot.Lookup(snapshot.Root);
		{
			Assert.AreEqual(1, root.NameToFile.Count);
			Assert.AreEqual(100, root.NameToFile["main.cpp"].Length);
			
			Assert.AreEqual(1, root.NameToTree.Count);
		}
		
		StreamTree data = snapshot.Lookup(root.NameToTree["Data"]);
		{
			Assert.AreEqual(2, data.NameToFile.Count);
			Assert.AreEqual(1, data.NameToTree.Count);
			
			Assert.AreEqual(_file2.Length, data.NameToFile["data2.bin"].Length);
			Assert.AreEqual(_file3.Length, data.NameToFile["data3.bin"].Length);
		}
		
		StreamTree audio = snapshot.Lookup(data.NameToTree["Audio"]);
		{
			Assert.AreEqual(0, audio.NameToFile.Count);
			Assert.AreEqual(1, audio.NameToTree.Count);
		}
		
		StreamTree samples = snapshot.Lookup(audio.NameToTree["Samples"]);
		{
			Assert.AreEqual(1, samples.NameToFile.Count);
			Assert.AreEqual(0, samples.NameToTree.Count);
			
			Assert.AreEqual(_file4.Length, samples.NameToFile["data4.bin"].Length);
		}
	}
}