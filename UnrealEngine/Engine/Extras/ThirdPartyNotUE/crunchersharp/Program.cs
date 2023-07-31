using System;
using System.Windows.Forms;

namespace CruncherSharp
{
    static class Program
    {
        /// <summary>
        /// The main entry point for the application.
        /// </summary>
        [STAThread]
        static void Main(string[] args)
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            var mainForm = new CruncherSharpForm(new SymbolAnalyzer());
            if (args.Length > 0)
            {
                // Let's assume our arg is indeed the full path of a pdb file...
                mainForm.LoadPdb(args[0], false);
            }
            Application.Run(mainForm);
        }
    }
}
