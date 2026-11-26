using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
using LibVLCSharp.Shared;
using LibVLCSharp.WPF;
using Microsoft.Win32;
using VlcMedia = LibVLCSharp.Shared.Media;
using VlcPlayer = LibVLCSharp.Shared.MediaPlayer;

namespace HeicViewer
{
    public partial class MainWindow : Window
    {
        // ----- elenco dei file -----
        private List<string> _files = new();
        private int _index = -1;
        private string? Current => _index >= 0 && _index < _files.Count ? _files[_index] : null;
        private MediaKind _currentKind = MediaKind.None;

        // ----- foto -----
        private BitmapSource? _bitmap;
        private double _scale = 1.0;
        private int _rotation;            // 0, 90, 180, 270 (senso orario)
        private Vector _offset;           // spostamento dal centro
        private bool _fitMode = true;
        private bool _panning;
        private Point _panStart;
        private Vector _panStartOffset;
        private int _loadVersion;

        // ----- video -----
        private LibVLC? _libVlc;
        private VlcPlayer? _player;
        private VideoView? _videoView;
        private bool _videoViewReady;
        private string? _pendingVideo;
        private bool _seeking;
        private readonly DispatcherTimer _videoTimer;

        // ----- schermo intero / presentazione -----
        private bool _fullScreen;
        private WindowState _savedState;
        private WindowStyle _savedStyle;
        private ResizeMode _savedResize;
        private readonly DispatcherTimer _slideshowTimer;
        private bool _slideshow;

        public MainWindow(string? path)
        {
            InitializeComponent();

            // Finestra grande ma non a tutto schermo, centrata (circa l'80% dello schermo).
            Width = Math.Max(MinWidth, SystemParameters.WorkArea.Width * 0.8);
            Height = Math.Max(MinHeight, SystemParameters.WorkArea.Height * 0.85);

            _videoTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(250) };
            _videoTimer.Tick += (_, _) => UpdateVideoUi();

            _slideshowTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(4) };
            _slideshowTimer.Tick += (_, _) => Navigate(+1, imagesOnly: true);

            Closed += (_, _) => DisposeVideo();

            if (path != null) OpenPath(path);
            else ShowMessage("Nessun file aperto.", showOpenButton: true);
        }

        // =====================================================================
        // Apertura e navigazione
        // =====================================================================

        private void OpenPath(string path)
        {
            _files = MediaFiles.InSameFolder(path);
            _index = _files.FindIndex(f => string.Equals(f, path, StringComparison.OrdinalIgnoreCase));
            if (_index < 0) { _files.Insert(0, path); _index = 0; }
            ShowCurrent();
        }

        private void Navigate(int delta, bool imagesOnly = false)
        {
            if (_files.Count == 0) return;
            int i = _index;
            for (int n = 0; n < _files.Count; n++)
            {
                i = (i + delta + _files.Count) % _files.Count;
                if (!imagesOnly || MediaFiles.KindOf(_files[i]) == MediaKind.Image) break;
            }
            if (i == _index && _files.Count > 1 && imagesOnly) return;
            _index = i;
            ShowCurrent();
        }

        private void ShowCurrent()
        {
            string? path = Current;
            if (path == null)
            {
                StopVideo();
                SetMode(MediaKind.None);
                Photo.Source = null;
                Title = "HEIC Viewer";
                NameText.Text = "";
                InfoText.Text = "";
                ShowMessage("Nessun file da mostrare.", showOpenButton: true);
                return;
            }

            string name = Path.GetFileName(path);
            Title = name + " - HEIC Viewer";
            NameText.Text = name;
            InfoText.Text = $"{_index + 1} di {_files.Count}";

            var kind = MediaFiles.KindOf(path);
            if (kind == MediaKind.Video)
            {
                StopSlideshow();
                SetMode(MediaKind.Video);
                HideMessage();
                PlayVideo(path);
            }
            else
            {
                StopVideo();
                SetMode(MediaKind.Image);
                LoadImage(path);
            }
        }

        private void SetMode(MediaKind kind)
        {
            _currentKind = kind;
            bool video = kind == MediaKind.Video;
            ImageHost.Visibility = video ? Visibility.Collapsed : Visibility.Visible;
            VideoHost.Visibility = video ? Visibility.Visible : Visibility.Collapsed;
            ImageBar.Visibility = video ? Visibility.Collapsed : Visibility.Visible;
            VideoBar.Visibility = video ? Visibility.Visible : Visibility.Collapsed;
            ContentArea.Background = video || _fullScreen ? Brushes.Black : Brushes.Transparent;
            if (_fullScreen && !video) ImageBar.Visibility = Visibility.Collapsed;
            if (!video) _videoTimer.Stop();
        }

        // =====================================================================
        // Foto
        // =====================================================================

        private async void LoadImage(string path)
        {
            int version = ++_loadVersion;
            _bitmap = null;
            Photo.Source = null;

            // Mostra "Caricamento..." solo se ci mette un po'.
            _ = Task.Delay(300).ContinueWith(_ => Dispatcher.InvokeAsync(() =>
            {
                if (version == _loadVersion && _bitmap == null) ShowMessage("Caricamento...");
            }));

            BitmapSource? bitmap = null;
            string? error = null;
            try
            {
                bitmap = await Task.Run(() => ImageLoader.Load(path));
            }
            catch (Exception ex)
            {
                error = ex.Message;
            }

            if (version != _loadVersion) return;  // nel frattempo l'utente è andato avanti

            if (bitmap == null)
            {
                ShowMessage("Impossibile aprire questa foto.\n\n" + error);
                return;
            }

            HideMessage();
            _bitmap = bitmap;
            _rotation = 0;
            Photo.Source = bitmap;
            Photo.Width = bitmap.PixelWidth;
            Photo.Height = bitmap.PixelHeight;
            InfoText.Text = $"{bitmap.PixelWidth} × {bitmap.PixelHeight}    {_index + 1} di {_files.Count}";
            FitToWindow();
        }

        private double FitScale()
        {
            if (_bitmap == null) return 1;
            double w = _bitmap.PixelWidth, h = _bitmap.PixelHeight;
            if (_rotation == 90 || _rotation == 270) (w, h) = (h, w);
            double availW = Math.Max(1, ImageHost.ActualWidth - 16);
            double availH = Math.Max(1, ImageHost.ActualHeight - 16);
            return Math.Min(1.0, Math.Min(availW / w, availH / h));  // le foto piccole non vengono ingrandite
        }

        private void FitToWindow()
        {
            _fitMode = true;
            _scale = FitScale();
            _offset = new Vector();
            ApplyTransform();
        }

        private void ApplyTransform()
        {
            var group = new TransformGroup();
            group.Children.Add(new ScaleTransform(_scale, _scale));
            group.Children.Add(new RotateTransform(_rotation));
            group.Children.Add(new TranslateTransform(_offset.X, _offset.Y));
            Photo.RenderTransform = group;
            FitButton.Content = _fitMode ? "1:1" : "⤢";
            FitButton.ToolTip = _fitMode ? "Dimensioni effettive (0)" : "Adatta alla finestra (0)";
            Photo.Cursor = !_fitMode ? Cursors.SizeAll : null;
        }

        private void ZoomBy(double factor, Point? around = null)
        {
            if (_bitmap == null) return;
            double fit = FitScale();
            double newScale = Math.Clamp(_scale * factor, Math.Min(fit, 0.05), 32);
            if (newScale <= fit && factor < 1)
            {
                FitToWindow();
                return;
            }

            // Mantiene fermo il punto sotto il mouse.
            var center = new Point(ImageHost.ActualWidth / 2, ImageHost.ActualHeight / 2);
            Vector p = (around ?? center) - center;
            _offset = p - (p - _offset) * (newScale / _scale);
            _scale = newScale;
            _fitMode = false;
            ApplyTransform();
        }

        private void OnImageWheel(object sender, MouseWheelEventArgs e)
        {
            ZoomBy(e.Delta > 0 ? 1.2 : 1 / 1.2, e.GetPosition(ImageHost));
            e.Handled = true;
        }

        private void OnImageMouseDown(object sender, MouseButtonEventArgs e)
        {
            if (e.ClickCount == 2)
            {
                ToggleFit(e.GetPosition(ImageHost));
                return;
            }
            if (_fitMode || _bitmap == null) return;
            _panning = true;
            _panStart = e.GetPosition(ImageHost);
            _panStartOffset = _offset;
            ImageHost.CaptureMouse();
        }

        private void OnImageMouseMove(object sender, MouseEventArgs e)
        {
            if (!_panning) return;
            _offset = _panStartOffset + (e.GetPosition(ImageHost) - _panStart);
            ApplyTransform();
        }

        private void OnImageMouseUp(object sender, MouseButtonEventArgs e)
        {
            if (!_panning) return;
            _panning = false;
            ImageHost.ReleaseMouseCapture();
        }

        private void OnImageHostSizeChanged(object sender, SizeChangedEventArgs e)
        {
            if (_fitMode) FitToWindow();
        }

        private void ToggleFit(Point? around = null)
        {
            if (_bitmap == null) return;
            if (_fitMode && FitScale() < 1) ZoomBy(1.0 / _scale, around);  // 100%
            else FitToWindow();
        }

        private void Rotate(int degrees)
        {
            if (_bitmap == null) return;
            _rotation = ((_rotation + degrees) % 360 + 360) % 360;
            FitToWindow();
        }

        private void OnZoomIn(object sender, RoutedEventArgs e) => ZoomBy(1.25);
        private void OnZoomOut(object sender, RoutedEventArgs e) => ZoomBy(1 / 1.25);
        private void OnToggleFit(object sender, RoutedEventArgs e) => ToggleFit();
        private void OnRotateLeft(object sender, RoutedEventArgs e) => Rotate(-90);
        private void OnRotateRight(object sender, RoutedEventArgs e) => Rotate(90);
        private void OnPrevious(object sender, RoutedEventArgs e) => Navigate(-1);
        private void OnNext(object sender, RoutedEventArgs e) => Navigate(+1);

        // =====================================================================
        // Presentazione
        // =====================================================================

        private void OnSlideshow(object sender, RoutedEventArgs e)
        {
            if (_slideshow) StopSlideshow();
            else StartSlideshow();
        }

        private void StartSlideshow()
        {
            if (_currentKind != MediaKind.Image) return;
            _slideshow = true;
            if (!_fullScreen) SetFullScreen(true);
            _slideshowTimer.Start();
        }

        private void StopSlideshow()
        {
            if (!_slideshow) return;
            _slideshow = false;
            _slideshowTimer.Stop();
        }

        // =====================================================================
        // Video (libVLC)
        // =====================================================================

        private void EnsureVideoEngine()
        {
            if (_libVlc != null) return;
            Core.Initialize();
            _libVlc = new LibVLC("--no-video-title-show", "--no-snapshot-preview");
            _player = new VlcPlayer(_libVlc)
            {
                EnableHardwareDecoding = true,
                EnableKeyInput = false,
                EnableMouseInput = false,
                Volume = (int)VolumeSlider.Value,
            };
            _player.EndReached += (_, _) => Dispatcher.InvokeAsync(() =>
            {
                PlayButton.Content = "";
                SeekSlider.Value = 1;
            });
            _player.EncounteredError += (_, _) => Dispatcher.InvokeAsync(() =>
            {
                ShowMessage("Impossibile riprodurre questo video.");
                Log.Error("Riproduzione video " + Current);
            });

            _videoView = new VideoView();
            _videoView.Loaded += (_, _) =>
            {
                if (_videoViewReady) return;
                _videoView.MediaPlayer = _player;
                _videoViewReady = true;
                if (_pendingVideo != null)
                {
                    string p = _pendingVideo;
                    _pendingVideo = null;
                    StartPlayback(p);
                }
            };
            VideoHost.Child = _videoView;
        }

        private void PlayVideo(string path)
        {
            try
            {
                EnsureVideoEngine();
                if (_videoViewReady) StartPlayback(path);
                else _pendingVideo = path;
            }
            catch (Exception ex)
            {
                Log.Error("Avvio lettore video", ex);
                ShowMessage("Impossibile avviare il lettore video.\n\n" + ex.Message);
            }
        }

        private void StartPlayback(string path)
        {
            if (_player == null || _libVlc == null) return;
            using var media = new VlcMedia(_libVlc, path, FromType.FromPath);
            _player.Play(media);
            PlayButton.Content = "";
            SeekSlider.Value = 0;
            _videoTimer.Start();
        }

        private void StopVideo()
        {
            _videoTimer.Stop();
            _pendingVideo = null;
            if (_player != null && (_player.IsPlaying || _player.State == VLCState.Paused))
            {
                var player = _player;
                // Stop() può richiedere un attimo: non blocchiamo l'interfaccia.
                ThreadPool.QueueUserWorkItem(_ => { try { player.Stop(); } catch { } });
            }
        }

        private void DisposeVideo()
        {
            _videoTimer.Stop();
            try
            {
                _player?.Stop();
                _player?.Dispose();
                _libVlc?.Dispose();
            }
            catch { }
        }

        private void UpdateVideoUi()
        {
            if (_player == null) return;
            long length = _player.Length;
            long time = _player.Time;
            if (!_seeking && length > 0) SeekSlider.Value = Math.Clamp(_player.Position, 0f, 1f);
            TimeText.Text = FormatTime(time);
            DurationText.Text = FormatTime(length);
            PlayButton.Content = _player.IsPlaying ? "" : "";
            MuteButton.Content = _player.Mute ? "" : "";
        }

        private static string FormatTime(long ms)
        {
            if (ms <= 0) return "0:00";
            var t = TimeSpan.FromMilliseconds(ms);
            return t.TotalHours >= 1 ? t.ToString(@"h\:mm\:ss") : t.ToString(@"m\:ss");
        }

        private void OnPlayPause(object sender, RoutedEventArgs e) => TogglePlay();

        private void TogglePlay()
        {
            if (_player == null) return;
            if (_player.State == VLCState.Ended || _player.State == VLCState.Stopped)
            {
                if (Current != null) StartPlayback(Current);
                return;
            }
            _player.Pause();  // alterna pausa/riproduzione
        }

        private void OnStop(object sender, RoutedEventArgs e)
        {
            if (_player == null) return;
            var player = _player;
            ThreadPool.QueueUserWorkItem(_ => { try { player.Stop(); } catch { } });
            SeekSlider.Value = 0;
            TimeText.Text = "0:00";
            PlayButton.Content = "";
        }

        private void OnSeekStart(object sender, MouseButtonEventArgs e) => _seeking = true;

        private void OnSeekEnd(object sender, MouseButtonEventArgs e)
        {
            if (_player != null && _player.IsSeekable) _player.Position = (float)SeekSlider.Value;
            _seeking = false;
        }

        private void SeekBy(long ms)
        {
            if (_player == null || !_player.IsSeekable) return;
            _player.Time = Math.Clamp(_player.Time + ms, 0, Math.Max(0, _player.Length - 500));
        }

        private void OnVolumeChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            if (_player != null) _player.Volume = (int)e.NewValue;
        }

        private void OnMute(object sender, RoutedEventArgs e)
        {
            if (_player == null) return;
            _player.Mute = !_player.Mute;
            MuteButton.Content = _player.Mute ? "" : "";
        }

        // =====================================================================
        // Schermo intero
        // =====================================================================

        private void OnToggleFullScreen(object sender, RoutedEventArgs e) => SetFullScreen(!_fullScreen);

        private void SetFullScreen(bool on)
        {
            if (on == _fullScreen) return;
            _fullScreen = on;
            if (on)
            {
                _savedState = WindowState;
                _savedStyle = WindowStyle;
                _savedResize = ResizeMode;
                WindowState = WindowState.Normal;   // necessario per coprire la barra delle applicazioni
                WindowStyle = WindowStyle.None;
                ResizeMode = ResizeMode.NoResize;
                WindowState = WindowState.Maximized;
                TopBar.Visibility = Visibility.Collapsed;
                FullScreenButton.Content = "";
            }
            else
            {
                StopSlideshow();
                WindowStyle = _savedStyle;
                ResizeMode = _savedResize;
                WindowState = _savedState;
                TopBar.Visibility = Visibility.Visible;
                FullScreenButton.Content = "";
            }
            SetMode(_currentKind);
        }

        // =====================================================================
        // Menu File
        // =====================================================================

        private void OnFileMenu(object sender, RoutedEventArgs e)
        {
            bool has = Current != null;
            MenuShowInFolder.IsEnabled = has;
            MenuCopy.IsEnabled = has;
            MenuDelete.IsEnabled = has;
            FileMenu.PlacementTarget = FileMenuButton;
            FileMenu.Placement = System.Windows.Controls.Primitives.PlacementMode.Bottom;
            FileMenu.IsOpen = true;
        }

        private void OnOpen(object sender, RoutedEventArgs e)
        {
            var dialog = new OpenFileDialog { Filter = MediaFiles.OpenFileFilter(), Title = "Apri foto o video" };
            if (Current != null) dialog.InitialDirectory = Path.GetDirectoryName(Current);
            if (dialog.ShowDialog(this) == true)
            {
                StopSlideshow();
                OpenPath(dialog.FileName);
            }
        }

        private void OnShowInFolder(object sender, RoutedEventArgs e)
        {
            if (Current == null) return;
            Process.Start(new ProcessStartInfo("explorer.exe", $"/select,\"{Current}\"") { UseShellExecute = true });
        }

        private void OnCopy(object sender, RoutedEventArgs e)
        {
            if (Current == null) return;
            var files = new StringCollection { Current };
            Clipboard.SetFileDropList(files);
        }

        private void OnDelete(object sender, RoutedEventArgs e)
        {
            string? path = Current;
            if (path == null) return;
            var answer = MessageBox.Show(this, $"Spostare \"{Path.GetFileName(path)}\" nel Cestino?", "Elimina",
                MessageBoxButton.YesNo, MessageBoxImage.Question);
            if (answer != MessageBoxResult.Yes) return;

            if (_currentKind == MediaKind.Video && _player != null)
            {
                _videoTimer.Stop();
                try { _player.Stop(); } catch { }   // il file deve essere libero
            }

            if (!Native.MoveToRecycleBin(path, new WindowInteropHelper(this).Handle))
            {
                MessageBox.Show(this, "Impossibile eliminare il file.", "Elimina", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            _files.RemoveAt(_index);
            if (_files.Count == 0) _index = -1;
            else if (_index >= _files.Count) _index = 0;
            ShowCurrent();
        }

        private void OnDefaultApps(object sender, RoutedEventArgs e)
        {
            try
            {
                Process.Start(new ProcessStartInfo("ms-settings:defaultapps") { UseShellExecute = true });
            }
            catch (Exception ex)
            {
                Log.Error("Apertura App predefinite", ex);
            }
        }

        private void OnExit(object sender, RoutedEventArgs e) => Close();

        // =====================================================================
        // Messaggi e tastiera
        // =====================================================================

        private void ShowMessage(string text, bool showOpenButton = false)
        {
            MessageText.Text = text;
            MessageOpenButton.Visibility = showOpenButton ? Visibility.Visible : Visibility.Collapsed;
            MessagePanel.Visibility = Visibility.Visible;
            MessageText.Foreground = _currentKind == MediaKind.Video
                ? new SolidColorBrush(Color.FromRgb(0xDD, 0xDD, 0xDD))
                : new SolidColorBrush(Color.FromRgb(0x4A, 0x66, 0x84));
        }

        private void HideMessage() => MessagePanel.Visibility = Visibility.Collapsed;

        private void OnPreviewKeyDown(object sender, KeyEventArgs e)
        {
            bool ctrl = (Keyboard.Modifiers & ModifierKeys.Control) != 0;
            switch (e.Key)
            {
                case Key.Left:
                    if (_currentKind == MediaKind.Video && ctrl) SeekBy(-5000); else Navigate(-1);
                    break;
                case Key.Right:
                    if (_currentKind == MediaKind.Video && ctrl) SeekBy(+5000); else Navigate(+1);
                    break;
                case Key.Space:
                    if (_currentKind == MediaKind.Video) TogglePlay();
                    else if (_slideshow) StopSlideshow(); else StartSlideshow();
                    break;
                case Key.F11:
                    SetFullScreen(!_fullScreen);
                    break;
                case Key.Escape:
                    if (_fullScreen) SetFullScreen(false);
                    break;
                case Key.Delete:
                    OnDelete(this, new RoutedEventArgs());
                    break;
                case Key.Add:
                case Key.OemPlus:
                    ZoomBy(1.25);
                    break;
                case Key.Subtract:
                case Key.OemMinus:
                    ZoomBy(1 / 1.25);
                    break;
                case Key.D0:
                case Key.NumPad0:
                    ToggleFit();
                    break;
                case Key.OemComma when ctrl:
                    Rotate(-90);
                    break;
                case Key.OemPeriod when ctrl:
                    Rotate(90);
                    break;
                case Key.O when ctrl:
                    OnOpen(this, new RoutedEventArgs());
                    break;
                case Key.C when ctrl:
                    OnCopy(this, new RoutedEventArgs());
                    break;
                default:
                    return;
            }
            e.Handled = true;
        }
    }
}
