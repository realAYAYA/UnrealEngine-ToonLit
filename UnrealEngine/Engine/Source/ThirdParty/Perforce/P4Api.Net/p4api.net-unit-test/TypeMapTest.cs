using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using NLog;

namespace p4api.net.unit.test
{
	/// <summary>
	///This is a test class for TypeMapTest and is intended
	///to contain all TypeMapTest Unit Tests
	///</summary>
	[TestClass()]
	public class TypeMapTest
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
        ///A test for TypeMap Constructor
        ///</summary>
        [TestMethod()]
		public void TypeMapConstructorTest()
		{
			FileType ft = new FileType("binary");
			TypeMapEntry mapping = new TypeMapEntry(ft, "//depot/main/...");
			FormSpec spec = null; 
			TypeMap target = new TypeMap(mapping, spec);
			Assert.IsNotNull(target);
			Assert.AreEqual(target.Mapping.FileType.BaseType, BaseFileType.Binary);
			Assert.AreEqual(target.Mapping.Path, "//depot/main/...");
		}

		/// <summary>
		///A test for Mapping
		///</summary>
		[TestMethod()]
		public void MappingTest()
		{
			FileType ft = new FileType("binary");
			FileType ft1 = new FileType("apple");
			FileType ft2 = new FileType("text");
			TypeMapEntry mapping = new TypeMapEntry(ft, "//depot/main/...");
			TypeMapEntry mapping1 = new TypeMapEntry(ft1, "//depot/dev/...");
			TypeMapEntry mapping2 = new TypeMapEntry(ft2, "//depot/rel/...");
			TypeMap target = new TypeMap();
			target.Add(mapping);
			target.Add(mapping1);
			target.Add(mapping2);
			Assert.IsNotNull(target);
			Assert.AreEqual(target.Count, 3);
			Assert.AreEqual(target[1].Path, "//depot/dev/...");
		}
	}
}
