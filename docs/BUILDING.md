# Derleme / Building

## GitHub Actions (önerilen)

Depoya `main` dalına push → `.github/workflows/build.yml` çalışır:

1. **linux-packages** — `packaging/build-all.sh`
   - `src/vendor/` içindeki değiştirilmemiş LuxOS paketlerini açar
     (`luxai 2.0.2-3`, `luxos-account`, `luxos-i18n`)
   - `src/luxai/i18n/*` parçalarını birleştirip `verify_tsv.py` ile doğrular
   - `patch_binary.py`: binary içindeki sabit boyutlu dil listesini 14 dile
     genişletir, CSS'i modernize eder (pencere düğmeleri + popup düzeltmesi)
   - `finalize_i18n.py`: 17 tam çeviri dosyası + BELGESELSEMO imzaları +
     hesap penceresi tsv'leri
   - `assemble_tree.py`: hicolor ikonları + tam çevrilmiş .desktop + python
     vendor + polkit politikası
   - `build_packages.py`: deb / rpm / Arch zst / AppImage + SHA256SUMS
2. **windows-native** — `windows/build-win.sh` (native Win32 x64, WSL'siz)
3. **screenshots** — derlenen .deb'i runner'a kurar, GUI'yi Xvfb'de gerçekten
   çalıştırır ve tr/en/hi ekran görüntülerini `promo/assets/screens/runner/`
   içine işler (repo'ya `[skip ci]` commit'iyle yazar)
4. **release** — `v*` etiketinde veya workflow_dispatch'ta: 5 paket +
   SHA256SUMS → GitHub Release (var olan release'in üzerine yazar)

Yerelde aynı akışı çalıştırmak için:

```bash
bash packaging/build-all.sh   # çıktı: .work/dist
bash packaging/build-exe.sh   # çıktı: .work/dist/LuxAI-2.0.2-Setup.exe
```

## Binary yaması nasıl çalışır?

LuxAI tek bir C/GTK3 çalıştırılabilir dosyasıdır (`/usr/bin/luxai`). Dil
listesi, ayar şeması ve CSS binary içine `[u64 tip][u64 uzunluk][veri]`
biçiminde gömülüdür; başlatma fonksiyonu her kaynağı doğrudan `lea` talimatı
ile parse eder.

`patch_binary.py` üç şey yapar:

1. Gömülü ana CSS'i küçültür (tek satır, yorum/yeni satır temizliği) — bütçe
   8736 baytın altında kalır.
2. Boşalan alana YENİ kaynaklar yazar: 14 dilli dropdown listesi, genişletilmiş
   ayar şeması ve İngilizce dil adı haritası.
3. Init fonksiyonundaki üç `lea` talimatının disp32 alanını yeni kaynak
   başlıklarına yönlendirir.

Kaynak başlığı formatı ve uzunluk alanı korunduğu için parser'lar değişiklikten
habersiz çalışır; hiçbir byte kaydırılmaz.

## Çeviri ekleme

1. `src/luxai/i18n/<lang>_a.tsv`, `_b.tsv`, `_c.tsv` parçalarını İngilizce
   anahtar<TAB>çeviri biçiminde doldurun (457 anahtar; `{0}`/`{1}`/`{2}`
   yer tutucuları korunmalı).
2. `build-all.sh` birleştirir; `verify_tsv.py` eksik/ fazla anahtar ve yer
   tutucu uyuşmazlığı arar.
3. Dropdown'da görünmesi için `packaging/patch_binary.py` içindeki
   `langs` listesine `("xx", "Dil adı")` ekleyin ve şema listesini güncelleyin.
