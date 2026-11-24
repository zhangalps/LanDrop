using System;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Text;
using Microsoft.Win32;

namespace LanDropSetup
{
    internal sealed class InstallOptions
    {
        public string InstallDir;
        public bool CreateDesktopShortcut;
        public bool CreateStartMenuShortcut;
    }

    internal static class InstallEngine
    {
        public const string AppName = "LanDrop";
        public const string AppVersion = "1.0.0";
        public const string Publisher = "LanDrop";
        private const string PayloadResourceName = "LanDrop.Payload.zip";
        private const string UninstallRegPath = @"Software\Microsoft\Windows\CurrentVersion\Uninstall\LanDrop";

        public static string DefaultInstallDir()
        {
            string localAppData = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
            return Path.Combine(localAppData, AppName);
        }

        public static string Install(InstallOptions options, Action<int, string> onProgress)
        {
            if (options == null || string.IsNullOrWhiteSpace(options.InstallDir))
                throw new ArgumentException("安装路径不能为空。");

            string targetDir = Path.GetFullPath(options.InstallDir.Trim());
            if (!targetDir.EndsWith(Path.DirectorySeparatorChar.ToString()))
                targetDir += Path.DirectorySeparatorChar;

            Report(onProgress, 2, "正在准备安装目录...");
            Directory.CreateDirectory(targetDir);

            ExtractPayload(targetDir, onProgress);

            string exePath = Path.Combine(targetDir, "LanDrop.exe");
            if (!File.Exists(exePath))
                throw new FileNotFoundException("安装包中缺少 LanDrop.exe 主程序。", exePath);

            Report(onProgress, 92, "正在生成卸载程序与注册信息...");
            string uninstallerPath = WriteUninstallerScript(targetDir);
            RegisterUninstallEntry(targetDir, exePath, uninstallerPath);

            Report(onProgress, 96, "正在创建快捷方式...");
            if (options.CreateDesktopShortcut)
            {
                string desktop = Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory);
                CreateShortcut(Path.Combine(desktop, AppName + ".lnk"), exePath, targetDir, exePath);
            }

            if (options.CreateStartMenuShortcut)
            {
                string programs = Environment.GetFolderPath(Environment.SpecialFolder.Programs);
                string menuDir = Path.Combine(programs, AppName);
                Directory.CreateDirectory(menuDir);
                CreateShortcut(Path.Combine(menuDir, AppName + ".lnk"), exePath, targetDir, exePath);
                CreateShortcut(Path.Combine(menuDir, "卸载 " + AppName + ".lnk"), uninstallerPath, targetDir, exePath);
            }

            Report(onProgress, 100, "安装完成");
            return exePath;
        }

        private static void ExtractPayload(string targetDir, Action<int, string> onProgress)
        {
            Assembly asm = Assembly.GetExecutingAssembly();
            using (Stream stream = asm.GetManifestResourceStream(PayloadResourceName))
            {
                if (stream == null)
                    throw new InvalidOperationException("安装包内部资源已损坏（未找到内嵌压缩包）。");

                using (ZipArchive archive = new ZipArchive(stream, ZipArchiveMode.Read))
                {
                    int total = archive.Entries.Count;
                    if (total == 0)
                        throw new InvalidOperationException("安装包内容为空。");

                    int done = 0;
                    foreach (ZipArchiveEntry entry in archive.Entries)
                    {
                        string relative = entry.FullName.Replace('/', Path.DirectorySeparatorChar);
                        string destPath = Path.GetFullPath(Path.Combine(targetDir, relative));
                        if (!destPath.StartsWith(targetDir, StringComparison.OrdinalIgnoreCase))
                            continue;

                        if (string.IsNullOrEmpty(entry.Name))
                        {
                            Directory.CreateDirectory(destPath);
                        }
                        else
                        {
                            string parent = Path.GetDirectoryName(destPath);
                            if (!string.IsNullOrEmpty(parent))
                                Directory.CreateDirectory(parent);
                            entry.ExtractToFile(destPath, true);
                        }

                        done++;
                        if (done == total || done % 5 == 0)
                        {
                            int percent = 5 + (int)((done * 85L) / total);
                            Report(onProgress, percent, string.Format("正在解压文件 ({0}/{1})...", done, total));
                        }
                    }
                }
            }
        }

        private static string WriteUninstallerScript(string targetDir)
        {
            string scriptPath = Path.Combine(targetDir, "Uninstall.cmd");
            StringBuilder sb = new StringBuilder();
            sb.AppendLine("@echo off");
            sb.AppendLine("chcp 65001 >nul");
            sb.AppendLine("taskkill /f /im LanDrop.exe >nul 2>&1");
            sb.AppendLine("del /f /q \"%USERPROFILE%\\Desktop\\LanDrop.lnk\" >nul 2>&1");
            sb.AppendLine("rmdir /s /q \"%APPDATA%\\Microsoft\\Windows\\Start Menu\\Programs\\LanDrop\" >nul 2>&1");
            sb.AppendLine("reg delete \"HKCU\\" + UninstallRegPath + "\" /f >nul 2>&1");
            sb.AppendLine("echo LanDrop 已卸载。");
            sb.AppendLine("start \"\" /b cmd /c \"ping 127.0.0.1 -n 2 >nul & rmdir /s /q \"\"%~dp0\"\"\"");
            File.WriteAllText(scriptPath, sb.ToString(), new UTF8Encoding(false));
            return scriptPath;
        }

        private static void RegisterUninstallEntry(string targetDir, string exePath, string uninstallerPath)
        {
            using (RegistryKey key = Registry.CurrentUser.CreateSubKey(UninstallRegPath))
            {
                if (key == null) return;
                key.SetValue("DisplayName", AppName);
                key.SetValue("DisplayVersion", AppVersion);
                key.SetValue("Publisher", Publisher);
                key.SetValue("InstallLocation", targetDir.TrimEnd(Path.DirectorySeparatorChar));
                key.SetValue("DisplayIcon", exePath);
                key.SetValue("UninstallString", "\"" + uninstallerPath + "\"");
                key.SetValue("NoModify", 1, RegistryValueKind.DWord);
                key.SetValue("NoRepair", 1, RegistryValueKind.DWord);
            }
        }

        private static void CreateShortcut(string shortcutPath, string targetPath, string workingDir, string iconPath)
        {
            Type shellType = Type.GetTypeFromProgID("WScript.Shell");
            if (shellType == null) return;
            object shell = Activator.CreateInstance(shellType);
            object shortcut = shellType.InvokeMember(
                "CreateShortcut",
                BindingFlags.InvokeMethod,
                null,
                shell,
                new object[] { shortcutPath });
            Type scType = shortcut.GetType();
            scType.InvokeMember("TargetPath", BindingFlags.SetProperty, null, shortcut, new object[] { targetPath });
            scType.InvokeMember("WorkingDirectory", BindingFlags.SetProperty, null, shortcut, new object[] { workingDir });
            if (!string.IsNullOrEmpty(iconPath))
                scType.InvokeMember("IconLocation", BindingFlags.SetProperty, null, shortcut, new object[] { iconPath });
            scType.InvokeMember("Save", BindingFlags.InvokeMethod, null, shortcut, null);
        }

        private static void Report(Action<int, string> onProgress, int percent, string status)
        {
            if (onProgress != null)
                onProgress(percent, status);
        }
    }
}
