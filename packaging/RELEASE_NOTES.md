# LuxAI 3.1.0 — %100 çok dilli sürüm (Linux + Windows native)

Bu sürüm, orijinal LuxAI 2.0.2 üzerine BELGESELSEMO topluluk yamalarını taşır:

- **18 dil desteği**: Arayüz dil listesine Türkçe, Rusça, Çince, Japonca ve
  **Hintçe (हिन्दी)** eklendi (mevcut 9 dilin tamamı korunur: Čeština,
  Slovenčina, English, Deutsch, Polski, Français, Español, Italiano,
  Українська). **18 tam çeviri dosyası** ile gelir — 457 arayüz anahtarının
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
- **Windows native sürümü (WSL yok)**: Arayüz Win32/GDI+ ile yeniden yazıldı —
  GTK, WSL, WSLg, sanal makine yok; tek `LuxAI.exe` (~475 KB) yalnızca Windows
  DLL'lerine bağlı, **Windows 7 SP1 → 11 (x64)**. Linux sürümüyle birebir aynı
  arayüz: headerbar, 260 px kenar çubuğu, gradyan kullanıcı balonu, Markdown +
  kod blokları, araç kartları, **çizilmiş onay/izin pencereleri** (klasik Win32
  MessageBox kullanılmaz), **LuxAI Hesabı** penceresi (kullanım, hesabı yönet,
  verileri dışa aktar, koşullar, hesap silme talebi, oturumu kapat), ayarlar
  sayfası, açık/koyu tema, sistem tepsisi ve **18 dil**.
- **cmd ile kurulum**: `windows/cmd/luxai-kur.bat` tek satırla indirir, kurar,
  Başlat menüsü + masaüstü kısayolu ve *Ayarlar › Uygulamalar* kaldırma kaydı
  oluşturur (`luxai-kaldir.bat` ile kaldırılır).
- **Arch/Manjaro/EndeavourOS/CachyOS düzeltmesi**: `.pkg.tar.zst` artık pacman'in
  beklediği biçimde (`.PKGINFO` kökte) üretiliyor — "eksik paket bilgisi" hatası
  bitti, gerçek `pacman -U` ile doğrulandı.
- **Tek satır kurucu** (`belgeselsemo-luxai-installer`): sürüm sabit URL, pacman
  önce denenir, SHA256 doğrulaması, sudo/doas otomatik seçimi, `--uninstall`.

Paketler: `.deb` · `.rpm` · `.AppImage` · `.pkg.tar.zst` (Arch) · `Setup-native.exe` · `Windows-native-x64.zip` · `luxai-kur.bat` (Windows native)

Not: Bu build'ler GitHub Actions ile derlenir; kaynak olarak ISO'dan alınan
değiştirilmemiş LuxAI paketleri `src/vendor/` içinde saklanır. Runner, derlenen
.deb'i gerçekten kurup GUI'yi Xvfb altında çalıştırır ve tr/en/hi ekran
görüntülerini repoya işler — promo'daki görüntüler gerçek uygulamadan gelir.
