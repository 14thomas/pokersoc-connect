using System;
using System.IO;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;

namespace pokersoc_connect
{
    public static class CurrencyImageGenerator
    {
        public static void GeneratePlaceholders()
        {
            string currencyPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "Assets", "Currency");
            Directory.CreateDirectory(currencyPath);

            // Australian notes - using actual RBA colors (simplified)
            GenerateNote(currencyPath, "note_100.png", "$100", Color.FromRgb(28, 163, 78));    // Green
            GenerateNote(currencyPath, "note_50.png", "$50", Color.FromRgb(255, 221, 0));      // Yellow
            GenerateNote(currencyPath, "note_20.png", "$20", Color.FromRgb(239, 65, 53));      // Red/Orange
            GenerateNote(currencyPath, "note_10.png", "$10", Color.FromRgb(0, 107, 179));      // Blue
            GenerateNote(currencyPath, "note_5.png", "$5", Color.FromRgb(230, 76, 157));       // Pink/Purple

            // Australian coins
            GenerateCoin(currencyPath, "coin_2.png", "$2", Color.FromRgb(184, 134, 11));       // Gold
            GenerateCoin(currencyPath, "coin_1.png", "$1", Color.FromRgb(184, 134, 11));       // Gold
            GenerateCoin(currencyPath, "coin_50c.png", "50¢", Color.FromRgb(192, 192, 192));   // Silver
            GenerateCoin(currencyPath, "coin_20c.png", "20¢", Color.FromRgb(192, 192, 192));   // Silver
            GenerateCoin(currencyPath, "coin_10c.png", "10¢", Color.FromRgb(192, 192, 192));   // Silver
            GenerateCoin(currencyPath, "coin_5c.png", "5¢", Color.FromRgb(192, 192, 192));     // Silver

            // Poker chips and plaques
            string chipsPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "Assets", "Chips");
            Directory.CreateDirectory(chipsPath);

            // Chips (round) - common poker chip colors
            GenerateChip(chipsPath, "chip_5c.png", "5¢", Color.FromRgb(255, 255, 255));        // White
            GenerateChip(chipsPath, "chip_25c.png", "25¢", Color.FromRgb(255, 0, 0));          // Red
            GenerateChip(chipsPath, "chip_1.png", "$1", Color.FromRgb(0, 0, 255));             // Blue
            GenerateChip(chipsPath, "chip_5.png", "$5", Color.FromRgb(0, 128, 0));             // Green

            // Plaques (rectangular) - higher denominations
            GeneratePlaque(chipsPath, "plaque_25.png", "$25", Color.FromRgb(128, 0, 128));     // Purple
            GeneratePlaque(chipsPath, "plaque_100.png", "$100", Color.FromRgb(0, 0, 0));       // Black
        }

        private static void GenerateNote(string basePath, string filename, string text, Color color)
        {
            string filepath = Path.Combine(basePath, filename);
            // Don't overwrite if file exists and is larger than 10KB (likely a real image, not a placeholder)
            if (File.Exists(filepath) && new FileInfo(filepath).Length > 10 * 1024) return;

            int width = 200;
            int height = 100;

            var visual = new DrawingVisual();
            using (var context = visual.RenderOpen())
            {
                // Background
                context.DrawRectangle(new SolidColorBrush(color), null, new Rect(0, 0, width, height));

                // Border
                var borderPen = new Pen(Brushes.Black, 3);
                context.DrawRectangle(null, borderPen, new Rect(1.5, 1.5, width - 3, height - 3));

                // Text
                var formattedText = new FormattedText(
                    text,
                    System.Globalization.CultureInfo.CurrentCulture,
                    FlowDirection.LeftToRight,
                    new Typeface("Arial"),
                    48,
                    Brushes.White,
                    VisualTreeHelper.GetDpi(visual).PixelsPerDip);

                context.DrawText(formattedText, 
                    new Point((width - formattedText.Width) / 2, (height - formattedText.Height) / 2));
            }

            RenderTargetBitmap bitmap = new RenderTargetBitmap(width, height, 96, 96, PixelFormats.Pbgra32);
            bitmap.Render(visual);

            var encoder = new PngBitmapEncoder();
            encoder.Frames.Add(BitmapFrame.Create(bitmap));

            using (var stream = File.Create(filepath))
            {
                encoder.Save(stream);
            }
        }

        private static void GenerateCoin(string basePath, string filename, string text, Color color)
        {
            string filepath = Path.Combine(basePath, filename);
            // Don't overwrite if file exists and is larger than 10KB (likely a real image, not a placeholder)
            if (File.Exists(filepath) && new FileInfo(filepath).Length > 10 * 1024) return;

            int size = 100;

            var visual = new DrawingVisual();
            using (var context = visual.RenderOpen())
            {
                // Circle background
                var center = new Point(size / 2, size / 2);
                var radius = size / 2 - 3;
                
                context.DrawEllipse(new SolidColorBrush(color), new Pen(Brushes.Black, 3), center, radius, radius);

                // Text
                var formattedText = new FormattedText(
                    text,
                    System.Globalization.CultureInfo.CurrentCulture,
                    FlowDirection.LeftToRight,
                    new Typeface("Arial"),
                    28,
                    Brushes.Black,
                    VisualTreeHelper.GetDpi(visual).PixelsPerDip);

                context.DrawText(formattedText, 
                    new Point((size - formattedText.Width) / 2, (size - formattedText.Height) / 2));
            }

            RenderTargetBitmap bitmap = new RenderTargetBitmap(size, size, 96, 96, PixelFormats.Pbgra32);
            bitmap.Render(visual);

            var encoder = new PngBitmapEncoder();
            encoder.Frames.Add(BitmapFrame.Create(bitmap));

            using (var stream = File.Create(filepath))
            {
                encoder.Save(stream);
            }
        }

        private static void GenerateChip(string basePath, string filename, string text, Color color)
        {
            string filepath = Path.Combine(basePath, filename);
            // Don't overwrite if file exists and is larger than 10KB (likely a real image, not a placeholder)
            if (File.Exists(filepath) && new FileInfo(filepath).Length > 10 * 1024) return;

            int size = 120;

            var visual = new DrawingVisual();
            using (var context = visual.RenderOpen())
            {
                var center = new Point(size / 2, size / 2);
                var outerRadius = size / 2 - 3;
                var innerRadius = outerRadius - 8;
                
                // Outer circle (chip edge)
                context.DrawEllipse(new SolidColorBrush(color), new Pen(Brushes.Black, 2), center, outerRadius, outerRadius);
                
                // Inner circle (chip face) - slightly darker
                var darkerColor = Color.FromRgb(
                    (byte)Math.Max(0, color.R - 30),
                    (byte)Math.Max(0, color.G - 30),
                    (byte)Math.Max(0, color.B - 30));
                context.DrawEllipse(new SolidColorBrush(darkerColor), null, center, innerRadius, innerRadius);

                // Edge markings (like casino chips)
                var edgePen = new Pen(Brushes.White, 4);
                for (int i = 0; i < 8; i++)
                {
                    double angle = i * Math.PI / 4;
                    double x1 = center.X + (outerRadius - 2) * Math.Cos(angle);
                    double y1 = center.Y + (outerRadius - 2) * Math.Sin(angle);
                    double x2 = center.X + (innerRadius + 2) * Math.Cos(angle);
                    double y2 = center.Y + (innerRadius + 2) * Math.Sin(angle);
                    context.DrawLine(edgePen, new Point(x1, y1), new Point(x2, y2));
                }

                // Text - use contrasting color
                var textColor = (color.R + color.G + color.B) / 3 > 128 ? Brushes.Black : Brushes.White;
                var formattedText = new FormattedText(
                    text,
                    System.Globalization.CultureInfo.CurrentCulture,
                    FlowDirection.LeftToRight,
                    new Typeface(new FontFamily("Arial"), FontStyles.Normal, FontWeights.Bold, FontStretches.Normal),
                    28,
                    textColor,
                    VisualTreeHelper.GetDpi(visual).PixelsPerDip);

                context.DrawText(formattedText, 
                    new Point((size - formattedText.Width) / 2, (size - formattedText.Height) / 2));
            }

            RenderTargetBitmap bitmap = new RenderTargetBitmap(size, size, 96, 96, PixelFormats.Pbgra32);
            bitmap.Render(visual);

            var encoder = new PngBitmapEncoder();
            encoder.Frames.Add(BitmapFrame.Create(bitmap));

            using (var stream = File.Create(filepath))
            {
                encoder.Save(stream);
            }
        }

        private static void GeneratePlaque(string basePath, string filename, string text, Color color)
        {
            string filepath = Path.Combine(basePath, filename);
            // Don't overwrite if file exists and is larger than 10KB (likely a real image, not a placeholder)
            if (File.Exists(filepath) && new FileInfo(filepath).Length > 10 * 1024) return;

            int width = 160;
            int height = 80;

            var visual = new DrawingVisual();
            using (var context = visual.RenderOpen())
            {
                // Rounded rectangle background
                var rect = new Rect(3, 3, width - 6, height - 6);
                var geometry = new RectangleGeometry(rect, 10, 10);
                context.DrawGeometry(new SolidColorBrush(color), new Pen(Brushes.Gold, 3), geometry);

                // Inner border
                var innerRect = new Rect(8, 8, width - 16, height - 16);
                var innerGeometry = new RectangleGeometry(innerRect, 8, 8);
                context.DrawGeometry(null, new Pen(Brushes.Gold, 1), innerGeometry);

                // Text - use contrasting color
                var textColor = (color.R + color.G + color.B) / 3 > 128 ? Brushes.Black : Brushes.White;
                var formattedText = new FormattedText(
                    text,
                    System.Globalization.CultureInfo.CurrentCulture,
                    FlowDirection.LeftToRight,
                    new Typeface(new FontFamily("Arial"), FontStyles.Normal, FontWeights.Bold, FontStretches.Normal),
                    32,
                    textColor,
                    VisualTreeHelper.GetDpi(visual).PixelsPerDip);

                context.DrawText(formattedText, 
                    new Point((width - formattedText.Width) / 2, (height - formattedText.Height) / 2));
            }

            RenderTargetBitmap bitmap = new RenderTargetBitmap(width, height, 96, 96, PixelFormats.Pbgra32);
            bitmap.Render(visual);

            var encoder = new PngBitmapEncoder();
            encoder.Frames.Add(BitmapFrame.Create(bitmap));

            using (var stream = File.Create(filepath))
            {
                encoder.Save(stream);
            }
        }
    }
}

