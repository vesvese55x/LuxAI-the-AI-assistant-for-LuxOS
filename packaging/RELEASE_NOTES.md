# LuxAI 3.1.0 — %100 çok dilli sürüm

Bu sürüm, orijinal LuxAI 2.0.2 üzerine BELGESELSEMO topluluk yamalarını taşır:

- **14 dil desteği**: Arayüz dil listesine Türkçe, Rusça, Çince, Japonca ve
  **Hintçe (हिन्दी)** eklendi (mevcut 9 dilin tamamı korunur: Čeština,
  Slovenčina, English, Deutsch, Polski, Français, Español, Italiano,
  Українська). **17 tam çeviri dosyası** ile gelir — 457 arayüz anahtarının
  tamamı her dilde eksiksiz.
- **Dinamik sistem dili**: GUI, sistem dilini otomatik algılar; desteklenmeyen
  dillerde İngilizce'ye düşer.
- **Modern pencere düğmeleri**: Küçült/büyüt/kapat düğmeleri yuvarlak hover,
  ölçekli semboller ve kırmızı kapat efektleriyle yeniden stillendirildi;
  açık/koyu temaya otomatik uyar.
- **Dropdown düzeltmesi**: Dil ve ayar listeleri ilk açılışta artık şeffaf
  çalışmıyor (opak arka plan + yuvarlatılmış köşeler).
- **BELGESELSEMO imzası**: Sohbet alanının üzerindeki boş alanda, seçili dilde
  görünür; belgeselsemo.com.tr bağlantısı içerir.
- **Menü ikonu düzeltmesi**: Tüm paketler hicolor ikonlarını kurar
  (16–512px) + tam çevrilmiş .desktop dosyası.
- **Windows kurucusu**: `LuxAI-3.1.0-Setup.exe`, LuxAI'yi WSL(WSLg) içine
  kurar ve Başlat menüsü kısayolu oluşturur (Windows 11).

Paketler: `.deb` · `.rpm` · `.AppImage` · `.pkg.tar.zst` (Arch) · `.exe` (Windows)

Not: Bu build'ler GitHub Actions ile derlenir; kaynak olarak ISO'dan alınan
değiştirilmemiş LuxAI paketleri `src/vendor/` içinde saklanır. Runner, derlenen
.deb'i gerçekten kurup GUI'yi Xvfb altında çalıştırır ve tr/en/hi ekran
görüntülerini repoya işler — promo'daki görüntüler gerçek uygulamadan gelir.
