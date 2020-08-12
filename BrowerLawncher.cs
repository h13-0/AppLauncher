using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Windows.Forms;

namespace BrowerLawncher
{
    class StartBrowser
    {
        private String path;
        

        public StartBrowser(String path)
        {
            this.path = path;
        }

        public void Start()
        {
            //准备cmd
            System.Diagnostics.Process p = new System.Diagnostics.Process();
            p.StartInfo.FileName = "cmd.exe";
            p.StartInfo.UseShellExecute = false;    //是否使用操作系统shell启动
            p.StartInfo.RedirectStandardInput = true;//接受来自调用程序的输入信息
            p.StartInfo.RedirectStandardOutput = true;//由调用程序获取输出信息
            p.StartInfo.RedirectStandardError = true;//重定向标准错误输出
            p.StartInfo.CreateNoWindow = true;//不显示程序窗口
            p.Start();//启动程序

            //读取配置
            IEnumerable<String> conf = File.ReadLines(path + ".conf");

            if (!File.Exists(conf.ElementAt<String>(0)))
            {
                //如果不存在对应浏览器,则提示后退出
                MessageBox.Show("Target Browser: \"" + conf.ElementAt<String>(0) + "\" Not exist." , "Brower Lawncher:");
                System.Environment.Exit(0);
            }

            Console.Out.WriteLine(conf.ElementAt<String>(0));
            //向cmd窗口发送输入信息
            String command = "";
            for(int index = 0;index < conf.ToArray().Length;index++)
            {
                if(index != 0)
                {
                    command = command + " " + conf.ElementAt<String>(index);
                }
                else
                {
                    command = "\"" + conf.ElementAt<String>(0) + "\"";
                }
            }

            command += "&exit";

            Console.Out.WriteLine(command);
            p.StandardInput.WriteLine(command);

            //开完等一下再跑
            System.Threading.Thread.Sleep(1000);
            System.Environment.Exit(0);
        }
    }
}
