using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Windows.Forms;


namespace BrowerLawncher
{
    static class Program
    {
        /// <summary>
        /// 应用程序的主入口点。
        /// </summary>
        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);

            System.Resources.ResourceManager resources =
                new System.Resources.ResourceManager("myResource", System.Reflection.Assembly.GetExecutingAssembly());
            NotifyIcon ni = new NotifyIcon();

            ni.BalloonTipIcon = System.Windows.Forms.ToolTipIcon.Warning;
            ni.BalloonTipText = "test!";
            ni.BalloonTipTitle = "test.";
            //ni.ContextMenuStrip = contextMenu;  
            //ni.Icon = ((System.Drawing.Icon)(resources.GetObject("ni.Icon")));
            ni.Text = "Test";
            ni.Visible = true;
            ni.MouseClick += delegate (object sender, MouseEventArgs e)
            {
                ni.ShowBalloonTip(0);
            };

            new Thread(new StartBrowser(Application.ExecutablePath).Start).Start();

            Application.Run();
        }
    }
}
