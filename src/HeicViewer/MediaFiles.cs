using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;

namespace HeicViewer
{
    internal enum MediaKind { None, Image, Video }

    /// <summary>Tipi di file supportati e elenco dei file nella stessa cartella.</summary>
    internal static class MediaFiles
    {
        public static readonly string[] ImageExtensions =
        {
            ".heic", ".heif", ".hif", ".avif", ".jpg", ".jpeg", ".jpe", ".jfif", ".png", ".gif",
            ".bmp", ".dib", ".tif", ".tiff", ".webp", ".ico", ".jxl", ".tga", ".psd",
        };

        public static readonly string[] VideoExtensions =
        {
            ".mp4", ".m4v", ".mov", ".mkv", ".avi", ".wmv", ".webm", ".3gp", ".3g2", ".mts", ".m2ts",
            ".ts", ".flv", ".mpg", ".mpeg", ".ogv",
        };

        private static readonly HashSet<string> Images = new(ImageExtensions, StringComparer.OrdinalIgnoreCase);
        private static readonly HashSet<string> Videos = new(VideoExtensions, StringComparer.OrdinalIgnoreCase);

        public static MediaKind KindOf(string path)
        {
            string ext = Path.GetExtension(path);
            if (Images.Contains(ext)) return MediaKind.Image;
            if (Videos.Contains(ext)) return MediaKind.Video;
            return MediaKind.None;
        }

        /// <summary>Foto e video della cartella, ordinati come in Esplora file.</summary>
        public static List<string> InSameFolder(string path)
        {
            try
            {
                string? dir = Path.GetDirectoryName(path);
                if (dir == null) return new List<string> { path };
                var list = Directory.EnumerateFiles(dir)
                    .Where(f => KindOf(f) != MediaKind.None)
                    .Where(f => (File.GetAttributes(f) & FileAttributes.Hidden) == 0)
                    .ToList();
                list.Sort(NaturalCompare);
                if (!list.Any(f => string.Equals(f, path, StringComparison.OrdinalIgnoreCase))) list.Add(path);
                return list;
            }
            catch (Exception ex)
            {
                Log.Error("Elenco cartella", ex);
                return new List<string> { path };
            }
        }

        [DllImport("shlwapi.dll", CharSet = CharSet.Unicode)]
        private static extern int StrCmpLogicalW(string a, string b);

        private static int NaturalCompare(string a, string b) =>
            StrCmpLogicalW(Path.GetFileName(a), Path.GetFileName(b));

        public static string OpenFileFilter()
        {
            string Join(IEnumerable<string> exts) => string.Join(";", exts.Select(e => "*" + e));
            return $"Foto e video|{Join(ImageExtensions.Concat(VideoExtensions))}" +
                   $"|Foto|{Join(ImageExtensions)}" +
                   $"|Video|{Join(VideoExtensions)}" +
                   "|Tutti i file|*.*";
        }
    }
}
