using System;
using System.IO;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using ImageMagick;

namespace HeicViewer
{
    /// <summary>Carica una foto di qualsiasi formato in una BitmapSource WPF.</summary>
    internal static class ImageLoader
    {
        public static BitmapSource Load(string path)
        {
            try
            {
                return LoadWithMagick(path);
            }
            catch (Exception magickError)
            {
                // Ripiego: decoder di Windows (WIC) per i formati comuni.
                try
                {
                    return LoadWithWic(path);
                }
                catch
                {
                    Log.Error("Caricamento foto " + path, magickError);
                    throw;
                }
            }
        }

        private static BitmapSource LoadWithMagick(string path)
        {
            using var image = new MagickImage(path);
            image.AutoOrient();                       // rotazione EXIF (foto verticali)
            image.ColorSpace = ColorSpace.sRGB;

            int width = (int)image.Width;
            int height = (int)image.Height;
            byte[] pixels = image.ToByteArray(MagickFormat.Bgra);

            var bitmap = BitmapSource.Create(width, height, 96, 96, PixelFormats.Bgra32, null, pixels, width * 4);
            bitmap.Freeze();
            return bitmap;
        }

        private static BitmapSource LoadWithWic(string path)
        {
            using var stream = File.OpenRead(path);
            var bitmap = new BitmapImage();
            bitmap.BeginInit();
            bitmap.CacheOption = BitmapCacheOption.OnLoad;
            bitmap.CreateOptions = BitmapCreateOptions.IgnoreColorProfile;
            bitmap.StreamSource = stream;
            bitmap.EndInit();
            bitmap.Freeze();
            return bitmap;
        }
    }
}
