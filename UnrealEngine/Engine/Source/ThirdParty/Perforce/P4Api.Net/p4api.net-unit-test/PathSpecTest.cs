using Perforce.P4.PCOM;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;

namespace UnitTests
{
    
    
    /// <summary>
    ///This is a test class for PathSpecTest and is intended
    ///to contain all PathSpecTest Unit Tests
    ///</summary>
	[TestClass()]
	public class PathSpecTest
	{


		private TestContext testContextInstance;

		/// <summary>
		///Gets or sets the test context which provides
		///information about and functionality for the current test run.
		///</summary>
		public TestContext TestContext
		{
			get
			{
				return testContextInstance;
			}
			set
			{
				testContextInstance = value;
			}
		}

		#region Additional test attributes
		// 
		//You can use the following additional attributes as you write your tests:
		//
		//Use ClassInitialize to run code before running the first test in the class
		//[ClassInitialize()]
		//public static void MyClassInitialize(TestContext testContext)
		//{
		//}
		//
		//Use ClassCleanup to run code after all tests in a class have run
		//[ClassCleanup()]
		//public static void MyClassCleanup()
		//{
		//}
		//
		//Use TestInitialize to run code before running each test
		//[TestInitialize()]
		//public void MyTestInitialize()
		//{
		//}
		//
		//Use TestCleanup to run code after each test has run
		//[TestCleanup()]
		//public void MyTestCleanup()
		//{
		//}
		//
		#endregion


		internal virtual PathSpec CreatePathSpec()
		{
			// TODO: Instantiate an appropriate concrete class.
			PathSpec target = null;
			return target;
		}

		/// <summary>
		///A test for Equals
		///</summary>
		[TestMethod()]
		public void EqualsTest()
		{
			PathSpec left = new DepotPath("//depot/main/test.txt");
			PathSpec rightpos = new DepotPath("//depot/main/test.txt");
			PathSpec rightneg1 = new DepotPath("//depot/main/empty.bmp");
			PathSpec rightneg2 = new ClientPath("//depot/main/test/txt");
			PathSpec rightnull = null;

			Assert.IsTrue(left.Equals(rightpos));
			Assert.IsFalse(left.Equals(rightneg1));
			Assert.IsFalse(left.Equals(rightneg2));
			Assert.IsFalse(left.Equals(rightnull));

			left = new ClientPath("//user_workspace/main/test.txt");
			rightpos = new ClientPath("//user_workspace/main/test.txt");
			rightneg1 = new ClientPath("//user_workspace/main/empty.bmp");
			rightneg2 = new DepotPath("//user_workspace/main/test.txt");
			rightnull = null;

			Assert.IsTrue(left.Equals(rightpos));
			Assert.IsFalse(left.Equals(rightneg1));
			Assert.IsFalse(left.Equals(rightneg2));
			Assert.IsFalse(left.Equals(rightnull));

			left = new LocalPath(@"C:\workspace_root\test.txt");
			rightpos = new LocalPath(@"C:\workspace_root\test.txt");
			rightneg1 = new LocalPath(@"C:\workspace_root\empty.bmp");
			rightneg2 = new DepotPath(@"C:\workspace_root\test.txt");
			rightnull = null;

			Assert.IsTrue(left.Equals(rightpos));
			Assert.IsFalse(left.Equals(rightneg1));
			Assert.IsFalse(left.Equals(rightneg2));
			Assert.IsFalse(left.Equals(rightnull));
		}
	}
}
