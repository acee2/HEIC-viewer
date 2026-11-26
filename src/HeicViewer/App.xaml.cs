using System;
using System.IO;
using System.Linq;
using System.Windows;
using System.Windows.Threading;

namespace HeicViewer
{
    public partial class App : Application
    {
        protected override void OnStartup(StartupEventArgs e)
        {
            base.OnStartup(e);

            DispatcherUnhandledException += OnUnhandledException;

            string? path = e.Args.FirstOrDefault(a => !string.IsNullOrWhiteSpace(a));
            if (path != null && !File.Exists(path)) path = null;

            var window = new MainWindow(path);
            MainWindow = window;
            window.Show();
        }

        private static void OnUnhandledException(object sender, DispatcherUnhandledExceptionEventArgs e)
        {
            Log.Error("Errore non gestito", e.Exception);
            MessageBox.Show("Si è verificato un errore:\n\n" + e.Exception.Message, "HEIC Viewer",
                MessageBoxButton.OK, MessageBoxImage.Warning);
            e.Handled = true;
        }
    }
}
