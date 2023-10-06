using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using NLog;

namespace p4api.net.unit.test
{
    /// <summary>
    ///This is a test class for FileSpecTest and is intended
    ///to contain all FileSpecTest Unit Tests
    ///</summary>
	[TestClass()]
	public class FileSpecTest
	{
        private static Logger logger = LogManager.GetCurrentClassLogger();
        public TestContext TestContext { get; set; }

        [TestInitialize]
        public void SetupTest()
        {
            Utilities.LogTestStart(TestContext);
        }
        [TestCleanup]
        public void CleanupTest()
        {
            Utilities.LogTestFinish(TestContext);
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

        /// <summary>
        ///A test for DateTime
        ///</summary>
        [TestMethod()]
        public void DateTimeTest()
        {
            DateTime dateTime = DateTime.Now;
            ClientPath cp = new ClientPath("c:\foobarclient");
            VersionSpec vs = new DateTimeVersion(dateTime);
            FileSpec target = new FileSpec(cp, vs);
            string dateTimeString =  vs.ToString();
            string expected = String.Format("@{0}", dateTime.ToString("yyyy/MM/dd:HH:mm:ss"));
            Assert.AreEqual(expected, dateTimeString);

            dateTime = DateTime.Now;
            cp = new ClientPath("c:\foobarclient");
            vs = new DateTimeVersion(dateTime);
            target = new FileSpec(cp, vs);
            dateTimeString = vs.ToString();
            expected = cp.ToString() + "@" + dateTime.ToString("yyyy/MM/dd:HH:mm:ss");
            Assert.AreEqual(expected, FileSpec.ToStrings(target)[0]);
        }

        /// <summary>
        ///A test for ClientPath
        ///</summary>
        [TestMethod()]
		public void ClientPathTest()
		{
			ClientPath expected = new ClientPath("c:\foobarclient");
			VersionSpec version = new VersionRange(new LabelNameVersion("my_label"), new LabelNameVersion("my_old_label"));
			FileSpec target = new FileSpec(expected, version);
			ClientPath actual;
			target.ClientPath = expected;
			actual = target.ClientPath;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for DepotPath
		///</summary>
		[TestMethod()]
		public void DepotPathTest()
		{
			DepotPath path = new DepotPath("c:\foobardepot"); 
			VersionSpec version = new VersionRange(new LabelNameVersion("my_label"), new LabelNameVersion("my_old_label"));
			FileSpec target = new FileSpec(path, version);
			DepotPath expected = path; // 
			DepotPath actual;
			target.DepotPath = expected;
			actual = target.DepotPath;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for LocalPath
		///</summary>
		[TestMethod()]
		public void LocalPathTest()
		{
			LocalPath path = new LocalPath("c:\foobarlocal");
			VersionSpec version = new VersionRange(new LabelNameVersion("my_label"), new LabelNameVersion("my_old_label"));
			FileSpec target = new FileSpec(path, version);
			LocalPath expected = path; // 
			LocalPath actual;
			target.LocalPath = expected;
			actual = target.LocalPath;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for Version
		///</summary>
		[TestMethod()]
		public void VersionTest()
		{
			DepotPath path = new DepotPath("c:\foobarversion");
			VersionSpec version = new VersionRange(new LabelNameVersion("my_label"), new LabelNameVersion("my_old_label"));
			FileSpec target = new FileSpec(path, version);
			DepotPath expected = path; // 
			PathSpec actual;
			target.DepotPath = expected;
			actual = target.DepotPath;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for Equals
		///</summary>
		[TestMethod()]
		public void EqualsTest()
		{
			FileSpec left = new FileSpec(new DepotPath("//depot/main/test.txt"),null,null,new VersionRange(1,2));
			FileSpec rightpos = new FileSpec(new DepotPath("//depot/main/test.txt"), null, null, new VersionRange(1, 2));
			FileSpec rightneg1 = new FileSpec(new DepotPath("//depot/main/empty.bmp"), null, null, new VersionRange(1, 2));
			FileSpec rightneg2 = new FileSpec(new DepotPath("//depot/main/test.txt"), null, null, new VersionRange(7,8));
			FileSpec rightnull = null;
						
			Assert.IsTrue(left.Equals(rightpos));
			Assert.IsFalse(left.Equals(rightneg1));
			Assert.IsFalse(left.Equals(rightneg2));
			Assert.IsFalse(left.Equals(rightnull));

			left = new FileSpec(null, new ClientPath("//user_workspace/main/test.txt"), null, new Revision(2));
			rightpos = new FileSpec(null, new ClientPath("//user_workspace/main/test.txt"), null, new Revision(2));
			rightneg1 = new FileSpec(null, new ClientPath("//user_workspace/main/empty.bmp"), null, new Revision(2));
			rightneg2 = new FileSpec(null, new ClientPath("//user_workspace/main/test.txt"), null, new Revision(4));
			rightnull = null;

			Assert.IsTrue(left.Equals(rightpos));
			Assert.IsFalse(left.Equals(rightneg1));
			Assert.IsFalse(left.Equals(rightneg2));
			Assert.IsFalse(left.Equals(rightnull));

			left = new FileSpec(null, null, new LocalPath(@"C:\workspace_root\test.txt"),  new NoneRevision());
			rightpos = new FileSpec(null, null, new LocalPath(@"C:\workspace_root\test.txt"), new NoneRevision());
			rightneg1 = new FileSpec(null, null, new LocalPath(@"C:\workspace_root\empty.bmp"), new NoneRevision());
			rightneg2 = new FileSpec(null, null, new LocalPath(@"C:\workspace_root\test.txt"), new HeadRevision());
			rightnull = null;

			Assert.IsTrue(left.Equals(rightpos));
			Assert.IsFalse(left.Equals(rightneg1));
			Assert.IsFalse(left.Equals(rightneg2));
			Assert.IsFalse(left.Equals(rightnull));

			left = new FileSpec(new DepotPath("//depot/main/test.txt"), new VersionRange(1, 2));
			rightpos = new FileSpec(new DepotPath("//depot/main/test.txt"), new VersionRange(1, 2));
			rightneg1 = new FileSpec(new DepotPath("//depot/main/empty.bmp"), new VersionRange(1, 2));
			rightneg2 = new FileSpec(new DepotPath("//depot/main/test.txt"), new VersionRange(7, 8));
			rightnull = null;

			Assert.IsTrue(left.Equals(rightpos));
			Assert.IsFalse(left.Equals(rightneg1));
			Assert.IsFalse(left.Equals(rightneg2));
			Assert.IsFalse(left.Equals(rightnull));

			left = new FileSpec(new ClientPath("//user_workspace/main/test.txt"), new Revision(2));
			rightpos = new FileSpec(new ClientPath("//user_workspace/main/test.txt"), new Revision(2));
			rightneg1 = new FileSpec(new ClientPath("//user_workspace/main/empty.bmp"), new Revision(2));
			rightneg2 = new FileSpec(new ClientPath("//user_workspace/main/test.txt"), new Revision(4));
			rightnull = null;

			Assert.IsTrue(left.Equals(rightpos));
			Assert.IsFalse(left.Equals(rightneg1));
			Assert.IsFalse(left.Equals(rightneg2));
			Assert.IsFalse(left.Equals(rightnull));

			left = new FileSpec(new LocalPath(@"C:\workspace_root\test.txt"), new NoneRevision());
			rightpos = new FileSpec(new LocalPath(@"C:\workspace_root\test.txt"), new NoneRevision());
			rightneg1 = new FileSpec(new LocalPath(@"C:\workspace_root\empty.bmp"), new NoneRevision());
			rightneg2 = new FileSpec(new LocalPath(@"C:\workspace_root\test.txt"), new HeadRevision());
			rightnull = null;

			Assert.IsTrue(left.Equals(rightpos));
			Assert.IsFalse(left.Equals(rightneg1));
			Assert.IsFalse(left.Equals(rightneg2));
			Assert.IsFalse(left.Equals(rightnull));
		}

		/// <summary>
		///A test for ToString
		///</summary>
		[TestMethod()]
		public void ToStringTest()
		{
			string target = @"C:\workspace@root\test#1%2.txt";
			FileSpec LocalSpec = new FileSpec(null, null, new LocalPath(target), new NoneRevision());

			string expected = @"c:\workspace@root\test#1%2.txt#none";

			string actual = LocalSpec.ToString();

			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for ToEscapedString
		///</summary>
		[TestMethod()]
		public void ToEscapedPathTest()
		{
			string target = @"C:\workspace@root\test#1%2.txt";
			FileSpec LocalSpec = new FileSpec(null, null, new LocalPath(target), new NoneRevision());

			string expected = @"c:\workspace%40root\test%231%252.txt#none";

			string actual = LocalSpec.ToEscapedString();

			Assert.AreEqual(expected, actual);

			target = @"c:\workspace%40root\test%231%252.txt";
			actual = PathSpec.UnescapePath(target);
			expected = @"c:\workspace@root\test#1%2.txt";

			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for ToString
		///</summary>
		[TestMethod()]
		public void ToStringTest1()
		{
			string expected = "0";
			DepotPath dp = new DepotPath(expected);
			FileSpec target = new FileSpec(dp);
			string actual;
			actual = target.ToString();
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for ToEscapedString
		///</summary>
		[TestMethod()]
		public void ToEscapedStringTest()
		{
			string expected = "0";
			DepotPath dp = new DepotPath(expected);
			FileSpec target = new FileSpec(dp);
			string actual;
			actual = target.ToEscapedString();
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for ToString
		///</summary>
		[TestMethod()]
		public void ToStringTest2()
		{
			Type pathType = typeof(DepotPath); // TODO: Initialize to an appropriate value
			string expected = "0";
			DepotPath dp = new DepotPath(expected);
			FileSpec target = new FileSpec(dp);
			string actual;
			actual = target.ToString(pathType);
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test fr ToPaths
		///</summary>
		[TestMethod()]
		public void ToPathsTest()
		{
			string expected0 = "0";
			FileSpec[] list = new FileSpec[1];
			list[0] = new DepotPath(expected0);
			string[] expected = new string[1];
			expected[0] = expected0;

			string[] actual;
			actual = FileSpec.ToPaths(list);
			Assert.AreEqual(expected[0], actual[0]);
		}

		/// <summary>
		///A test for ToEscapedPaths
		///</summary>
		[TestMethod()]
		public void ToEscapedPathsTest()
		{
			string expected0 = "0";
			FileSpec[] list = new FileSpec[1];
			list[0] = new DepotPath(expected0);
			string[] expected = new string[1];
			expected[0] = expected0;

			string[] actual;
			actual = FileSpec.ToEscapedPaths(list);
			Assert.AreEqual(expected[0], actual[0]);
		}

		/// <summary>
		///A test for ToEscapedLocalPaths
		///</summary>
		[TestMethod()]
		public void ToEscapedLocalPathsTest()
		{
			string expected0 = "0";
			FileSpec[] list = new FileSpec[1];
			list[0] = new LocalPath(expected0);
			string[] expected = new string[1];
			expected[0] = expected0;

			string[] actual;
			actual = FileSpec.ToEscapedLocalPaths(list);
			Assert.AreEqual(expected[0], actual[0]);
		}
	}
}
