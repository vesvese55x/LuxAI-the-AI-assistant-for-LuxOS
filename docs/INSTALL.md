# Kurulum / Installation

## Linux

| Dağıtım | Paket | Komut |
|---|---|---|
| Debian 12+, Ubuntu 22.04+, Mint | `luxai_3.1.0_amd64.deb` | `sudo apt install ./luxai_3.1.0_amd64.deb` |
| Fedora 39+, RHEL, openSUSE | `luxai-3.1.0-1.x86_64.rpm` | `sudo dnf install ./luxai-3.1.0-1.x86_64.rpm` |
| Arch / CachyOS / Manjaro | `luxai-3.1.0-1-x86_64.pkg.tar.zst` | `sudo pacman -U luxai-3.1.0-1-x86_64.pkg.tar.zst` |
| Herhangi biri | `LuxAI-3.1.0-x86_64.AppImage` | `chmod +x LuxAI-3.1.0-x86_64.AppImage && ./LuxAI-3.1.0-x86_64.AppImage` |

Sistem bağımlılıkları (deb/rpm/zst paketleri gerçek bağımlılık olarak ister,
paket içinde LuxOS'a özel bağımlılık YOKTUR):

- `libgtk-3` (GTK 3.24+)
- `python3` + `python3-gi` / `python3-gobject` (PyGObject, hesap penceresi için)
- `libcurl`
- isteğe bağlı: `polkit` — yönetici (root) modu için

AppImage, bu kütüphanelerin sistemde kurulu olmasını bekler (masaüstü
dağıtımlarının tamamında mevcuttur).

## Windows 10/11

1. `LuxAI-3.1.0-Setup.exe` indirin ve çalıştırın.
2. WSL kurulu değilse kurucu `wsl --install -d Debian` çalıştırır
   (birkaç dakika + olası yeniden başlatma).
3. LuxAI, Debian WSL içine kurulur; Başlat menüsüne **LuxAI** kısayolu eklenir.
4. Kısayoldan çalıştırın (WSLg penceresi açılır) ve LuxAI hesabınızla oturum açın.

## Windows (native — WSL'siz)

`LuxAI-3.1.0-Setup-native.exe` veya tek dosya `LuxAI-3.1.0-Windows-native-x64.zip` içindeki
`LuxAI.exe`. Windows 7 SP1 / 8 / 8.1 / 10 / 11 (x64).

1. Kurucuyu çalıştırın (yönetici gerekmez): `%LOCALAPPDATA%\LuxAI` içine kurar,
   Başlat menüsü + masaüstü kısayolu oluşturur, "Programlar ve Özellikler" kaydı ekler.
2. İlk açılışta LuxAI hesabınızla giriş yapın (e-posta + parola). Hesap yoksa
   <https://lux-ai.cz/register>.
3. Ajan modunda araçlar Windows üzerinde çalışır (`cmd.exe`, dosya araçları, ekran görüntüsü…).

Derleme: `bash windows/build-win.sh` → `windows/build/LuxAI.exe` (mingw-w64; ~257 KB, harici DLL yok).
Teknik ayrıntılar ve repo analizi: [`docs/ANALIZ-WINDOWS.md`](ANALIZ-WINDOWS.md),
kullanım: [`windows/README-TR.md`](../windows/README-TR.md).

> Windows 7 notu: `lux-ai.cz` TLS 1.2 istediği için KB3140245 + `DefaultSecureProtocols=0xA00`
> gerekir (ayrıntı windows/README-TR.md).

## Kaldırma / Uninstall

- deb: `sudo apt remove luxai`
- rpm: `sudo dnf remove luxai`
- Arch: `sudo pacman -R luxai`
- AppImage: dosyayı silin
- Windows: WSL içinde `sudo apt remove luxai` + Başlat menüsü kısayolunu silin
  (isterseniz tüm distro: `wsl --unregister Debian`)

## Konfigürasyon dosyaları

- Ayarlar: `~/.config/luxai/settings.json`
- Hesap belirteci: `~/.config/luxai/account.json` (yalnızca `luxos-account` yazar)
- Sohbetler: `~/.local/share/luxai/conversations/`
- Yönetici komut kaydı: `~/.local/state/luxai/root-commands.log`
