using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using NLog;

namespace p4api.net.unit.test
{
	/// <summary>
	///This is a test class for ClientOptionEnumTest and is intended
	///to contain all ClientOptionEnumTest Unit Tests
	///</summary>
	[TestClass()]
	public class ClientOptionEnumTest
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

        private static string v0 = "noallwrite noclobber nocompress unlocked nomodtime normdir";
		private static string v42 = "allwrite noclobber compress unlocked modtime normdir";
		private static string v21 = "noallwrite clobber nocompress locked nomodtime rmdir";
		private static string v64 = "allwrite clobber compress locked modtime rmdir";

		private static ClientOptionEnum e0 = new ClientOptionEnum(ClientOption.None);
		private static ClientOptionEnum e42 = new ClientOptionEnum(ClientOption.AllWrite | ClientOption.Compress | ClientOption.ModTime);
		private static ClientOptionEnum e21 = new ClientOptionEnum(ClientOption.Clobber | ClientOption.Locked | ClientOption.RmDir);
		private static ClientOptionEnum e64 = new ClientOptionEnum(ClientOption.AllWrite | ClientOption.Clobber | ClientOption.Compress | ClientOption.Locked | ClientOption.ModTime | ClientOption.RmDir);

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
		///A test for ClientOptionEnum Constructor
		///</summary>
		[TestMethod()]
		public void ClientOptionEnumConstructorTest()
		{
			ClientOptionEnum target = new ClientOptionEnum(v0);
			Assert.AreEqual(target, ClientOption.None);
			Assert.AreEqual(ClientOption.None, target & ClientOption.AllWrite);
			Assert.AreEqual(ClientOption.None, target & ClientOption.Clobber);
			Assert.AreEqual(ClientOption.None, target & ClientOption.Compress);
			Assert.AreEqual(ClientOption.None, target & ClientOption.Locked);
			Assert.AreEqual(ClientOption.None, target & ClientOption.ModTime);
			Assert.AreEqual(ClientOption.None, target & ClientOption.RmDir);

			target = new ClientOptionEnum(v42);
			Assert.AreNotEqual(ClientOption.None, target);
			Assert.AreNotEqual(ClientOption.None, target & ClientOption.AllWrite);
			Assert.AreEqual(ClientOption.None, target & ClientOption.Clobber);
			Assert.AreNotEqual(ClientOption.None, target & ClientOption.Compress);
			Assert.AreEqual(ClientOption.None, target & ClientOption.Locked);
			Assert.AreNotEqual(ClientOption.None, target & ClientOption.ModTime);
			Assert.AreEqual(ClientOption.None, target & ClientOption.RmDir);

			target = new ClientOptionEnum(v21);
			Assert.AreNotEqual(ClientOption.None, target);
			Assert.AreEqual(ClientOption.None, target & ClientOption.AllWrite);
			Assert.AreNotEqual(ClientOption.None, target & ClientOption.Clobber);
			Assert.AreEqual(ClientOption.None, target & ClientOption.Compress);
			Assert.AreNotEqual(ClientOption.None, target & ClientOption.Locked);
			Assert.AreEqual(ClientOption.None, target & ClientOption.ModTime);
			Assert.AreNotEqual(ClientOption.None, target & ClientOption.RmDir);

			target = new ClientOptionEnum(v64);
			Assert.AreNotEqual(ClientOption.None, target);
			Assert.AreNotEqual(ClientOption.None, target & ClientOption.AllWrite);
			Assert.AreNotEqual(ClientOption.None, target & ClientOption.Clobber);
			Assert.AreNotEqual(ClientOption.None, target & ClientOption.Compress);
			Assert.AreNotEqual(ClientOption.None, target & ClientOption.Locked);
			Assert.AreNotEqual(ClientOption.None, target & ClientOption.ModTime);
			Assert.AreNotEqual(ClientOption.None, target & ClientOption.RmDir);
		}

		/// <summary>
		///A test for Parse
		///</summary>
		[TestMethod()]
		public void ParseTest()
		{
			ClientOptionEnum target = new ClientOptionEnum(ClientOption.None);
			target.Parse(v0);
			Assert.AreEqual(target, ClientOption.None);
			Assert.AreEqual(ClientOption.None, target & ClientOption.AllWrite);
			Assert.AreEqual(ClientOption.None, target & ClientOption.Clobber);
			Assert.AreEqual(ClientOption.None, target & ClientOption.Compress);
			Assert.AreEqual(ClientOption.None, target & ClientOption.Locked);
			Assert.AreEqual(ClientOption.None, target & ClientOption.ModTime);
			Assert.AreEqual(ClientOption.None, target & ClientOption.RmDir);

			target.Parse(v42);
			Assert.AreNotEqual(ClientOption.None, target);
			Assert.AreNotEqual(ClientOption.None, target & ClientOption.AllWrite);
			Assert.AreEqual(ClientOption.None, target & ClientOption.Clobber);
			Assert.AreNotEqual(ClientOption.None, target & ClientOption.Compress);
			Assert.AreEqual(ClientOption.None, target & ClientOption.Locked);
			Assert.AreNotEqual(ClientOption.None, target & ClientOption.ModTime);
			Assert.AreEqual(ClientOption.None, target & ClientOption.RmDir);

			target.Parse(v21);
			Assert.AreNotEqual(ClientOption.None, target);
			Assert.AreEqual(ClientOption.None, target & ClientOption.AllWrite);
			Assert.AreNotEqual(ClientOption.None, target & ClientOption.Clobber);
			Assert.AreEqual(ClientOption.None, target & ClientOption.Compress);
			Assert.AreNotEqual(ClientOption.None, target & ClientOption.Locked);
			Assert.AreEqual(ClientOption.None, target & ClientOption.ModTime);
			Assert.AreNotEqual(ClientOption.None, target & ClientOption.RmDir);

			target.Parse(v64);
			Assert.AreNotEqual(ClientOption.None, target);
			Assert.AreNotEqual(ClientOption.None, target & ClientOption.AllWrite);
			Assert.AreNotEqual(ClientOption.None, target & ClientOption.Clobber);
			Assert.AreNotEqual(ClientOption.None, target & ClientOption.Compress);
			Assert.AreNotEqual(ClientOption.None, target & ClientOption.Locked);
			Assert.AreNotEqual(ClientOption.None, target & ClientOption.ModTime);
			Assert.AreNotEqual(ClientOption.None, target & ClientOption.RmDir);
		}

		/// <summary>
		///A test for ToString
		///</summary>
		[TestMethod()]
		public void ToStringTest()
		{
			ClientOptionEnum v = new ClientOptionEnum(ClientOption.None);
			string expected = v0;
			string actual = v.ToString();
			Assert.AreEqual(expected, actual);

			v = new ClientOptionEnum(ClientOption.AllWrite | ClientOption.Compress | ClientOption.ModTime);
			expected = v42;
			actual = v.ToString();
			Assert.AreEqual(expected, actual);

			v = new ClientOptionEnum(ClientOption.Clobber | ClientOption.Locked | ClientOption.RmDir);
			expected = v21;
			actual = v.ToString();
			Assert.AreEqual(expected, actual);

			v = new ClientOptionEnum(ClientOption.AllWrite | ClientOption.Clobber | ClientOption.Compress | ClientOption.Locked | ClientOption.ModTime | ClientOption.RmDir);
			expected = v64;
			actual = v.ToString();
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for op_Implicit
		///</summary>
		[TestMethod()]
		public void op_ImplicitTest()
		{
			string s = v0;
			ClientOptionEnum expected = e0;
			ClientOptionEnum actual;
			actual = s;
			Assert.IsTrue(expected == actual);

			s = v42;
			expected = e42;
			actual = s;
			Assert.IsTrue(expected == actual);

			s = v21;
			expected = e21;
			actual = s;
			Assert.IsTrue(expected == actual);

			s = v64;
			expected = e64;
			actual = s;
			Assert.IsTrue(expected == actual);
		}

		/// <summary>
		///A test for op_Implicit
		///</summary>
		[TestMethod()]
		public void op_ImplicitTest1()
		{
			string expected = v0; 
			string actual;
			actual = e0;
			Assert.AreEqual(expected, actual);

			expected = v42;
			actual = e42;
			Assert.AreEqual(expected, actual);

			expected = v21;
			actual = e21;
			Assert.AreEqual(expected, actual);

			expected = v64;
			actual = e64;
			Assert.AreEqual(expected, actual);
		}

		/// <summary>
		///A test for op_Implicit
		///</summary>
		[TestMethod()]
		public void op_ImplicitTest2()
		{
			ClientOption v = ClientOption.Clobber | ClientOption.Locked | ClientOption.RmDir;
			ClientOptionEnum expected = new ClientOptionEnum(ClientOption.Clobber | ClientOption.Locked | ClientOption.RmDir);
			ClientOptionEnum actual;
			actual = v;
			Assert.AreEqual(expected, actual);

			v = actual;
			Assert.AreEqual(ClientOption.Clobber | ClientOption.Locked | ClientOption.RmDir, v);
		}
	}
}
