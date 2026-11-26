using System;
using System.IO;

namespace HeicViewer
{
    /// <summary>Log degli errori in %LOCALAPPDATA%\HEIC Viewer\errori.log (max 1 MB).</summary>
    internal static class Log
    {
        private static readonly object Sync = new();

        public static void Error(string where, Exception? ex = null)
        {
            try
            {
                string dir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "HEIC Viewer");
                Directory.CreateDirectory(dir);
                string file = Path.Combine(dir, "errori.log");
                lock (Sync)
                {
                    var info = new FileInfo(file);
                    if (info.Exists && info.Length > 1024 * 1024) info.Delete();
                    File.AppendAllText(file, $"{DateTime.Now:yyyy-MM-dd HH:mm:ss} {where}: {ex}\r\n");
                }
            }
            catch
            {
                // il log non deve mai far fallire l'app
            }
        }
    }
}
