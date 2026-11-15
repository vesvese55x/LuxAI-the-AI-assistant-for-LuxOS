# Kenar çubuğu “Oturumu kapat” hatası — kök neden ve düzeltme

## Belirti
Sol kenar çubuğunda kullanıcı adına tıklayınca açılan popover'da **Oturumu kapat…**
satırı onay penceresini açıyor (`LuxAI oturumu kapatılsın mı?` → **İptal** / **Oturumu kapat**),
ancak **Oturumu kapat**'a basınca hiçbir şey olmuyor: oturum kapanmıyor, kullanıcı hâlâ
giriş yapmış görünüyor. Aynı işlem hamburger menüsü → **LuxAI hesabı…** (harici
`luxos-account` programı) üzerinden çalışıyordu.

## Kök neden
1. **Onay penceresinin gövdesi boş.** `strace -f -e trace=execve,connect,unlink,unlinkat`
   ile izlendi: diyalog kapandıktan sonra uygulama **tek bir sistem çağrısı bile**
   yapmıyor (ne `fork/exec`, ne `connect`, ne `unlink`). Yani onay dalı hiçbir iş yapmıyor.
2. **Dönüş değeri hatası değil.** `gtk_dialog_run()` bir `LD_PRELOAD` shim'i ile
   sarmalanıp dönen değer günlüğe alındı: diyalog `-5 (GTK_RESPONSE_OK)` dönüyor.
   Değer zorla `1, 0, -2, -3, -4, -8, -9` yapılarak da denendi — hiçbiri çıkışı
   tetiklemedi; yani sorun response-id uyuşmazlığı değil.
3. **Harici araç hiç çağrılmıyor.** `PATH`'e konan sarmalayıcı bir `luxos-account`
   ile yapılan denemede ikili dosyanın bu yolu hiç çalıştırmadığı görüldü.
   Buna karşılık hamburger → **LuxAI hesabı…** menüsü `setsid -- luxos-account`
   çağırıyor ve oradaki **Çıkış yap** düğmesi `POST /api/ide/logout` + yerel
   `~/.config/luxai/account.json` silme işini doğru yapıyor.
4. Uygulama hesap durumunu **bellekte** tutuyor: `account.json` dışarıdan silinse bile
   arayüz "giriş yapılmış" görünmeye devam ediyor (yeniden başlatma gerekli).

Sonuç: LuxOS 2.0.2 ikilisinde kenar çubuğu çıkışının eylemi uygulanmamış (ölü dal).

## Düzeltme (paket düzeyinde, kaynak olmadan)
`packaging/luxfix/` altında iki parça:

| Dosya | Görevi |
|---|---|
| `luxai-wrapper` | `/usr/bin/luxai` olarak kurulur; gerçek ikiliyi (`/usr/lib/luxai/luxai-bin`) `LD_PRELOAD=…/luxfix.so` ile başlatır. |
| `luxfix.c` → `luxfix.so` | `gtk_dialog_run()` sarmalar. Onay penceresi, 18 dildeki başlık metinleriyle (yalnızca çevirisi olan 6 varyant + İngilizce) tanınır. Onaylanırsa: ① `luxos-account logout` çalıştırılır (`POST /api/ide/logout`, token silinir), ② uygulama aynı argümanlarla yeniden başlatılır → arayüz “Oturum açılmadı” durumuna geçer. `İptal` seçilirse hiçbir şey yapılmaz. |

AppImage için aynı iş `AppRun` içinde yapılır (mutlak yol yerine `$APPDIR`).

## Doğrulama (yerel, gerçek paketle)
Sahte hesap API'si (`/api/me` + `/api/ide/logout`) ve Xvfb üzerinde:

```
GET /api/me  Cookie: lux_account_session=good-token   -> 200
# popover: Ayarlar… / Hesabı yönet / Oturumu kapat…
# onay: Oturumu kapat
POST /api/ide/logout  {}                              -> 200   ✅
~/.config/luxai/account.json                          -> silindi ✅
arayüz: "LuxAI'ye hoş geldiniz" + "Oturum açılmadı"    ✅
```

`İptal` yolunda: `POST` yok, `account.json` korunuyor. ✅

## Notlar
- Düzeltme yalnızca **onay penceresi onaylandığında** devreye girer; başka
  diyaloglar (geçmişi temizle, hesap silme vb.) etkilenmez.
- `luxos-account` yoksa/başarısızsa `luxfix.so` yine de yerel `account.json`
  dosyasını siler ve uygulamayı yeniden başlatır (çevrimdışı çıkış).
- Uygulama yeniden başladığı için açık sohbet penceresi kapanır; sohbetler
  `~/.config/luxai/conversations` altında korunur.
