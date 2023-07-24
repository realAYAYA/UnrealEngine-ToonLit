using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System.Collections.Generic;
using NLog;

namespace p4api.net.unit.test
{
	/// <summary>
	///This is a test class for ViewMapTest and is intended
	///to contain all ViewMapTest Unit Tests
	///</summary>
	[TestClass()]
	public class ViewMapTest
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
        ///A test for ViewMap Constructor
        ///</summary>
        [TestMethod()]
		public void ViewMapConstructorTest()
		{
			ViewMap target = new ViewMap();

			Assert.IsNotNull(target);
		}

		/// <summary>
		///A test for ViewMap Constructor
		///</summary>
		[TestMethod()]
		public void ViewMapConstructorTest1()
		{
			string[] lines = new string[] {
			"	//depot/main/p4/... //user_win-user/main/p4/...",
			"-//usr/... //user_win-user/usr/...",
			"+//spec/... //user_win-user/spec/..."
			};
			ViewMap target = new ViewMap(lines);

			MapEntry Entry0 = new MapEntry(MapType.Include,
				new DepotPath("//depot/main/p4/..."),
				new ClientPath("//user_win-user/main/p4/..."));
			MapEntry Entry1 = new MapEntry(MapType.Exclude,
				new DepotPath("//usr/..."),
				new ClientPath("//user_win-user/usr/..."));
			MapEntry Entry2 = new MapEntry(MapType.Overlay,
				new DepotPath("//spec/..."),
				new ClientPath("//user_win-user/spec/..."));

			Assert.AreEqual(Entry0.Type, target[0].Type);
			Assert.AreEqual(Entry0.Right.Path, target[0].Right.Path);
			Assert.AreEqual(Entry0.Left.Path, target[0].Left.Path);

			Assert.AreEqual(Entry1.Type, target[1].Type);
			Assert.AreEqual(Entry1.Right.Path, target[1].Right.Path);
			Assert.AreEqual(Entry1.Left.Path, target[1].Left.Path);

			Assert.AreEqual(Entry2.Type, target[2].Type);
			Assert.AreEqual(Entry2.Right.Path, target[2].Right.Path);
			Assert.AreEqual(Entry2.Left.Path, target[2].Left.Path);
		}

		/// <summary>
		///A test for ViewMap Constructor
		///</summary>
		[TestMethod()]
		public void ViewMapConstructorTest2()
		{
			List<string> lines = new List<string>();

			lines.Add("	//depot/main/p4/... //user_win-user/main/p4/...");
			lines.Add("-//usr/... //user_win-user/usr/...");
			lines.Add("+//spec/... //user_win-user/spec/...");

			ViewMap target = new ViewMap(lines);

			MapEntry Entry0 = new MapEntry(MapType.Include,
				new DepotPath("//depot/main/p4/..."),
				new ClientPath("//user_win-user/main/p4/..."));
			MapEntry Entry1 = new MapEntry(MapType.Exclude,
				new DepotPath("//usr/..."),
				new ClientPath("//user_win-user/usr/..."));
			MapEntry Entry2 = new MapEntry(MapType.Overlay,
				new DepotPath( "//spec/..."),
				new ClientPath("//user_win-user/spec/..."));

			Assert.AreEqual(Entry0.Type, target[0].Type);
			Assert.AreEqual(Entry0.Right.Path, target[0].Right.Path);
			Assert.AreEqual(Entry0.Left.Path, target[0].Left.Path);

			Assert.AreEqual(Entry1.Type, target[1].Type);
			Assert.AreEqual(Entry1.Right.Path, target[1].Right.Path);
			Assert.AreEqual(Entry1.Left.Path, target[1].Left.Path);

			Assert.AreEqual(Entry2.Type, target[2].Type);
			Assert.AreEqual(Entry2.Right.Path, target[2].Right.Path);
			Assert.AreEqual(Entry2.Left.Path, target[2].Left.Path);
		}

		/// <summary>
		///A test for Add
		///</summary>
		[TestMethod()]
		public void AddTest()
		{
			ViewMap target = new ViewMap();

			MapEntry Entry0 = new MapEntry(MapType.Include,
				new DepotPath("//depot/main/p4/..."),
				new ClientPath("//user_win-user/main/p4/..."));
			MapEntry Entry1 = new MapEntry(MapType.Exclude,
				new DepotPath("//usr/..."),
				new ClientPath("//user_win-user/usr/..."));
			MapEntry Entry2 = new MapEntry(MapType.Overlay,
				new DepotPath("//spec/..."),
				new ClientPath("//user_win-user/spec/..."));

			target.Add(Entry0);
			target.Add(Entry1);
			target.Add(Entry2);

			Assert.AreEqual(Entry0.Type, target[0].Type);
			Assert.AreEqual(Entry0.Right.Path, target[0].Right.Path);
			Assert.AreEqual(Entry0.Left.Path, target[0].Left.Path);

			Assert.AreEqual(Entry1.Type, target[1].Type);
			Assert.AreEqual(Entry1.Right.Path, target[1].Right.Path);
			Assert.AreEqual(Entry1.Left.Path, target[1].Left.Path);

			Assert.AreEqual(Entry2.Type, target[2].Type);
			Assert.AreEqual(Entry2.Right.Path, target[2].Right.Path);
			Assert.AreEqual(Entry2.Left.Path, target[2].Left.Path);
		}

		/// <summary>
		///A test for Add
		///</summary>
		[TestMethod()]
		public void AddTest1()
		{
			ViewMap target = new ViewMap();

			MapEntry Entry0 = new MapEntry(MapType.Include,
				new DepotPath("//depot/main/p4/..."),
				new ClientPath("//user_win-user/main/p4/..."));
			MapEntry Entry1 = new MapEntry(MapType.Exclude,
				new DepotPath("//usr/..."),
				new ClientPath("//user_win-user/usr/..."));
			MapEntry Entry2 = new MapEntry(MapType.Overlay,
				new DepotPath("//spec/..."),
				new ClientPath("//user_win-user/spec/..."));

			target.Add("	//depot/main/p4/... //user_win-user/main/p4/...");
			target.Add("-//usr/... //user_win-user/usr/...");
			target.Add("+//spec/... //user_win-user/spec/...");

			Assert.AreEqual(Entry0.Type, target[0].Type);
			Assert.AreEqual(Entry0.Right.Path, target[0].Right.Path);
			Assert.AreEqual(Entry0.Left.Path, target[0].Left.Path);

			Assert.AreEqual(Entry1.Type, target[1].Type);
			Assert.AreEqual(Entry1.Right.Path, target[1].Right.Path);
			Assert.AreEqual(Entry1.Left.Path, target[1].Left.Path);

			Assert.AreEqual(Entry2.Type, target[2].Type);
			Assert.AreEqual(Entry2.Right.Path, target[2].Right.Path);
			Assert.AreEqual(Entry2.Left.Path, target[2].Left.Path);
		}

		/// <summary>
		///A test for SplitViewLine
		///</summary>
		[TestMethod()]
		public void SplitViewLineTest()
		{
			string line = "	//depot/main/p4/... //user_win-user/main/p4/...";
			string[] expected = new string[] { "//depot/main/p4/...", "//user_win-user/main/p4/..." };
			string[] actual;
			actual = ViewMap.SplitViewLine(line);
			Assert.AreEqual(expected.Length, actual.Length);
			Assert.AreEqual(expected[0], actual[0]);
			Assert.AreEqual(expected[1], actual[1]);

			line = "	-//depot/main/p4/... \"//user_win-user/main/p4/...\"";
			actual = ViewMap.SplitViewLine(line);
			Assert.AreEqual(expected.Length, actual.Length);
			Assert.AreEqual(expected[0], actual[0]);
			Assert.AreEqual(expected[1], actual[1]);

			line = "	\"+//depot/main/p4/...\" \"//user_win-user/main/p4/...\"";
			actual = ViewMap.SplitViewLine(line);
			Assert.AreEqual(expected.Length, actual.Length);
			Assert.AreEqual(expected[0], actual[0]);
			Assert.AreEqual(expected[1], actual[1]);

			line = "	\"//depot/main line/p4/...\" \"//user_win-user/main/p4/...\"";
			expected = new string[] { "//depot/main line/p4/...", "//user_win-user/main/p4/..." };
			actual = ViewMap.SplitViewLine(line);
			Assert.AreEqual(expected.Length, actual.Length);
			Assert.AreEqual(expected[0], actual[0]);
			Assert.AreEqual(expected[1], actual[1]);
		}

		/// <summary>
		///A test for ToString
		///</summary>
		[TestMethod()]
		public void ToStringTest()
		{
			string[] lines = new string[] {
			"	//depot/main/p4/... //user_win-user/main/p4/...",
			"\"-//usr space/...\" \"//user_win-user/usr space/...\"",
			"+//spec/... //user_win-user/spec/..."
			};
			ViewMap target = new ViewMap(lines);
			string actual = target.ToString();

			string expected =
"//depot/main/p4/... //user_win-user/main/p4/...\r\n\"-//usr space/...\" \"//user_win-user/usr space/...\"\r\n+//spec/... //user_win-user/spec/...\r\n";

			Assert.AreEqual(expected, actual);
		}
	}
}
