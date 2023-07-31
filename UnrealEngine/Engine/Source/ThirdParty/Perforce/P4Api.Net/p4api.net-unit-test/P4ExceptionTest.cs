using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using NLog;

namespace p4api.net.unit.test
{   
    /// <summary>
    ///This is a test class for P4ExceptionTest and is intended
    ///to contain all P4ExceptionTest Unit Tests
    ///</summary>
    [TestClass()]
    public class P4ExceptionTest
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
        ///A test for Throw
        ///</summary>
        [TestMethod()]
        public void ThrowTest()
        {
            P4ClientErrorList errors = new P4ClientErrorList( "This is a test", ErrorSeverity.E_FAILED );

            P4Exception.MinThrowLevel = ErrorSeverity.E_FATAL;
            bool passed = true;
            try
            {
                P4Exception.Throw( errors );
            }
            catch
            {
                passed = false;  // should not have thrown
            }
            Assert.IsTrue( passed, "Threw an exception < MinThrowLevel" );

            P4Exception.MinThrowLevel = ErrorSeverity.E_FAILED;
            passed = false;
            try
            {
                P4Exception.Throw( errors );
            }
            catch
            {
                passed = true;  // should have thrown
            }
            Assert.IsTrue( passed, "Did not throw an exception >= MinThrowLevel" );
        }

        /// <summary>
        ///A test for Throw
        ///</summary>
        [TestMethod()]
        public void ThrowTest1()
        {
            P4ClientError error = new P4ClientError( ErrorSeverity.E_FAILED, "This is a test" );

            P4Exception.MinThrowLevel = ErrorSeverity.E_FATAL;
            bool passed = true;
            try
            {
                P4Exception.Throw( error );
            }
            catch
            {
                passed = false;  // should not have thrown
            }
            Assert.IsTrue( passed, "Threw an exception < MinThrowLevel" );

            P4Exception.MinThrowLevel = ErrorSeverity.E_FAILED;
            passed = false;
            try
            {
                P4Exception.Throw( error );
            }
            catch
            {
                passed = true;  // should have thrown
            }
            Assert.IsTrue( passed, "Did not throw an exception >= MinThrowLevel" );
        }

        /// <summary>
        ///A test for Throw
        ///</summary>
        [TestMethod()]
        public void ThrowTest2()
        {
            P4Exception.MinThrowLevel = ErrorSeverity.E_FATAL;
            bool passed = true;
            try
            {
                P4Exception.Throw( ErrorSeverity.E_FAILED, "This is a Test" );
            }
            catch
            {
                passed = false;  // should not have thrown
            }
            Assert.IsTrue( passed, "Threw an exception < MinThrowLevel" );

            P4Exception.MinThrowLevel = ErrorSeverity.E_FAILED;
            passed = false;
            try
            {
                P4Exception.Throw( ErrorSeverity.E_FAILED, "This is a Test" );
            }
            catch
            {
                passed = true;  // should have thrown
            }
            Assert.IsTrue( passed, "Did not throw an exception >= MinThrowLevel" );
        }
    }
}
