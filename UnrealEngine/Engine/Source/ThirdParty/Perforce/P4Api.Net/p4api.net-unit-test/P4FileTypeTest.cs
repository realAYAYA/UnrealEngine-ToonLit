 using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;

namespace UnitTests
{
    
    
    /// <summary>
    ///This is a test class for FileTypeTest and is intended
    ///to contain all FileTypeTest Unit Tests
    ///</summary>
    [TestClass()]
    public class P4FileTypeTest
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


        /// <summary>
        ///A test for Parse
        ///</summary>
        [TestMethod()]
        public void ParseAndToStringTest()
        {
            // Base type, no flags
            string text = "text";
            FileType expected = new FileType(FileType.Type.Text, FileType.Modifier.None); 
            FileType actual;
            actual = FileType.Parse(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.MaxRevisions, actual.MaxRevisions);
            String toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "binary";
            expected = new FileType(FileType.Type.Binary, FileType.Modifier.None); 
            actual = FileType.Parse(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.MaxRevisions, actual.MaxRevisions);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);


            text = "symlink";
            expected = new FileType(FileType.Type.Symlink, FileType.Modifier.None); 
            actual = FileType.Parse(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.MaxRevisions, actual.MaxRevisions);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);


            text = "apple";
            expected = new FileType(FileType.Type.Apple, FileType.Modifier.None); 
            actual = FileType.Parse(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.MaxRevisions, actual.MaxRevisions);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);


            text = "resource";
            expected = new FileType(FileType.Type.Resource, FileType.Modifier.None); 
            actual = FileType.Parse(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.MaxRevisions, actual.MaxRevisions);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);


            text = "unicode";
            expected = new FileType(FileType.Type.Unicode, FileType.Modifier.None); 
            actual = FileType.Parse(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.MaxRevisions, actual.MaxRevisions);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);


            text = "utf16";
            expected = new FileType(FileType.Type.Utf16, FileType.Modifier.None); 
            actual = FileType.Parse(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.MaxRevisions, actual.MaxRevisions);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            // add each flag individually (even if not strictly valid with base type)
            text = "utf16+m";
            expected = new FileType(FileType.Type.Utf16, FileType.Modifier.ModTime);
            actual = FileType.Parse(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.MaxRevisions, actual.MaxRevisions);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+w";
            expected = new FileType(FileType.Type.Utf16, FileType.Modifier.Writable);
            actual = FileType.Parse(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.MaxRevisions, actual.MaxRevisions);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+x";
            expected = new FileType(FileType.Type.Utf16, FileType.Modifier.Executable);
            actual = FileType.Parse(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.MaxRevisions, actual.MaxRevisions);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+k";
            expected = new FileType(FileType.Type.Utf16, FileType.Modifier.KeywordExpand); 
            actual = FileType.Parse(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.MaxRevisions, actual.MaxRevisions);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+ko";
            expected = new FileType(FileType.Type.Utf16, FileType.Modifier.KeywordHeader); 
            actual = FileType.Parse(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.MaxRevisions, actual.MaxRevisions);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+l";
            expected = new FileType(FileType.Type.Utf16, FileType.Modifier.LockAlways); 
            actual = FileType.Parse(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.MaxRevisions, actual.MaxRevisions);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+C";
            expected = new FileType(FileType.Type.Utf16, FileType.Modifier.Compressed); 
            actual = FileType.Parse(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.MaxRevisions, actual.MaxRevisions);

            text = "utf16+D";
            expected = new FileType(FileType.Type.Utf16, FileType.Modifier.DeltaRCS); 
            actual = FileType.Parse(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.MaxRevisions, actual.MaxRevisions);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+F";
            expected = new FileType(FileType.Type.Utf16, FileType.Modifier.FullFile); 
            actual = FileType.Parse(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.MaxRevisions, actual.MaxRevisions);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+S";
            expected = new FileType(FileType.Type.Utf16, FileType.Modifier.HeadOnly); 
            actual = FileType.Parse(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.MaxRevisions, actual.MaxRevisions);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+S3";
            expected = new FileType(FileType.Type.Utf16, FileType.Modifier.StoreNRevisions, 3); 
            actual = FileType.Parse(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.MaxRevisions, actual.MaxRevisions);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+S10";
            expected = new FileType(FileType.Type.Utf16, FileType.Modifier.StoreNRevisions, 10);
            actual = FileType.Parse(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.MaxRevisions, actual.MaxRevisions);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+S16";
            expected = new FileType(FileType.Type.Utf16, FileType.Modifier.StoreNRevisions, 16);
            actual = FileType.Parse(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.MaxRevisions, actual.MaxRevisions);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+S32";
            expected = new FileType(FileType.Type.Utf16, FileType.Modifier.StoreNRevisions, 32);
            actual = FileType.Parse(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.MaxRevisions, actual.MaxRevisions);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+S64";
            expected = new FileType(FileType.Type.Utf16, FileType.Modifier.StoreNRevisions, 64);
            actual = FileType.Parse(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.MaxRevisions, actual.MaxRevisions);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+S128";
            expected = new FileType(FileType.Type.Utf16, FileType.Modifier.StoreNRevisions, 128);
            actual = FileType.Parse(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.MaxRevisions, actual.MaxRevisions);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+S256";
            expected = new FileType(FileType.Type.Utf16, FileType.Modifier.StoreNRevisions, 256);
            actual = FileType.Parse(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.MaxRevisions, actual.MaxRevisions);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+S512";
            expected = new FileType(FileType.Type.Utf16, FileType.Modifier.StoreNRevisions, 512);
            actual = FileType.Parse(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.MaxRevisions, actual.MaxRevisions);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+X";
            expected = new FileType(FileType.Type.Utf16, FileType.Modifier.TrigggerArchive); 
            actual = FileType.Parse(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.MaxRevisions, actual.MaxRevisions);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            // add all the flags (even if not strictly valid with base type)
            text = "utf16+mwxklCDFSX";
            expected = new FileType(FileType.Type.Utf16, FileType.Modifier.ModTime |
                FileType.Modifier.Writable | FileType.Modifier.Executable |
                FileType.Modifier.KeywordExpand | FileType.Modifier.LockAlways |
                FileType.Modifier.Compressed | FileType.Modifier.DeltaRCS |
                FileType.Modifier.FullFile | FileType.Modifier.HeadOnly |
                FileType.Modifier.TrigggerArchive);
            actual = FileType.Parse(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.MaxRevisions, actual.MaxRevisions);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+mwxkolCDFSX";
            expected = new FileType(FileType.Type.Utf16, FileType.Modifier.ModTime |
                FileType.Modifier.Writable | FileType.Modifier.Executable |
                FileType.Modifier.KeywordHeader | FileType.Modifier.LockAlways |
                FileType.Modifier.Compressed | FileType.Modifier.DeltaRCS |
                FileType.Modifier.FullFile | FileType.Modifier.HeadOnly |
                FileType.Modifier.TrigggerArchive);
            actual = FileType.Parse(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.MaxRevisions, actual.MaxRevisions);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+mwxklCDFS3X";
            expected = new FileType(FileType.Type.Utf16, FileType.Modifier.ModTime |
                FileType.Modifier.Writable | FileType.Modifier.Executable |
                FileType.Modifier.KeywordExpand | FileType.Modifier.LockAlways |
                FileType.Modifier.Compressed | FileType.Modifier.DeltaRCS |
                FileType.Modifier.FullFile | FileType.Modifier.StoreNRevisions |
                FileType.Modifier.TrigggerArchive, 3);
            actual = FileType.Parse(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.MaxRevisions, actual.MaxRevisions);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+mwxkolCDFS3X";
            expected = new FileType(FileType.Type.Utf16, FileType.Modifier.ModTime |
                FileType.Modifier.Writable | FileType.Modifier.Executable |
                FileType.Modifier.KeywordHeader | FileType.Modifier.LockAlways |
                FileType.Modifier.Compressed | FileType.Modifier.DeltaRCS |
                FileType.Modifier.FullFile | FileType.Modifier.StoreNRevisions |
                FileType.Modifier.TrigggerArchive, 3);
            actual = FileType.Parse(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.MaxRevisions, actual.MaxRevisions);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+mwxkolCDFS16X";
            expected = new FileType(FileType.Type.Utf16, FileType.Modifier.ModTime |
                FileType.Modifier.Writable | FileType.Modifier.Executable |
                FileType.Modifier.KeywordHeader | FileType.Modifier.LockAlways |
                FileType.Modifier.Compressed | FileType.Modifier.DeltaRCS |
                FileType.Modifier.FullFile | FileType.Modifier.StoreNRevisions |
                FileType.Modifier.TrigggerArchive, 16);
            actual = FileType.Parse(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.MaxRevisions, actual.MaxRevisions);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);

            text = "utf16+mwxkolCDFS512X";
            expected = new FileType(FileType.Type.Utf16, FileType.Modifier.ModTime |
                FileType.Modifier.Writable | FileType.Modifier.Executable |
                FileType.Modifier.KeywordHeader | FileType.Modifier.LockAlways |
                FileType.Modifier.Compressed | FileType.Modifier.DeltaRCS |
                FileType.Modifier.FullFile | FileType.Modifier.StoreNRevisions |
                FileType.Modifier.TrigggerArchive, 512);
            actual = FileType.Parse(text);
            Assert.AreEqual(expected.BaseType, actual.BaseType);
            Assert.AreEqual(expected.Modifiers, actual.Modifiers);
            Assert.AreEqual(expected.MaxRevisions, actual.MaxRevisions);
            toString = actual.ToString();
            Assert.AreEqual(text, toString);
        }
    }
}
