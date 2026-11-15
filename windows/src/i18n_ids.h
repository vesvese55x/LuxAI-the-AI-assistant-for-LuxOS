/* Ceviri anahtarlari: TR + EN kaynak; diger 16 dil i18n_data.c (gen_i18n.py) */
#ifndef LUXAI_I18N_IDS_H
#define LUXAI_I18N_IDS_H

#define STRS(X) \
    X(S_TITLE,        "LuxAI",                              "LuxAI") \
    X(S_NEW,          "Yeni sohbet",                        "New chat") \
    X(S_SEND,         "Gönder",                             "Send") \
    X(S_STOP,         "Durdur",                             "Stop") \
    X(S_CHAT,         "Sohbet",                             "Chat") \
    X(S_AGENT,        "Ajan",                               "Agent") \
    X(S_SETTINGS,     "Ayarlar",                            "Settings") \
    X(S_LOGIN,        "Giriş yap",                          "Sign in") \
    X(S_LOGOUT,       "Çıkış yap",                          "Sign out") \
    X(S_EMAIL,        "E-posta",                            "E-mail") \
    X(S_PASSWORD,     "Parola",                             "Password") \
    X(S_REGISTER,     "Hesap oluştur (web)",                "Create account (web)") \
    X(S_CANCEL,       "Vazgeç",                             "Cancel") \
    X(S_OKBTN,           "Tamam",                              "OK") \
    X(S_SAVE,         "Kaydet",                             "Save") \
    X(S_MODEL,        "Model",                              "Model") \
    X(S_MODE,         "Mod",                                "Mode") \
    X(S_LANGUAGE,     "Arayüz dili",                        "Interface language") \
    X(S_THEME,        "Tema",                               "Theme") \
    X(S_THEME_SYS,    "Sistem",                             "System") \
    X(S_THEME_LIGHT,  "Açık",                               "Light") \
    X(S_THEME_DARK,   "Koyu",                               "Dark") \
    X(S_SENDKEY,      "Gönderme tuşu",                      "Send key") \
    X(S_ENTER,        "Enter",                              "Enter") \
    X(S_CTRLENTER,    "Ctrl+Enter",                         "Ctrl+Enter") \
    X(S_WORKDIR,      "Çalışma klasörü",                    "Working folder") \
    X(S_AUTOAPPROVE,  "Ajan eylemlerini otomatik onayla",   "Auto-approve agent actions") \
    X(S_MAXTOK,       "Yanıt uzunluğu (max_tokens)",        "Answer length (max_tokens)") \
    X(S_ENDPOINT,     "Uç nokta (Endpoint)",                "Endpoint") \
    X(S_APIKEY,       "API anahtarı (OpenAI uyumlu)",       "API key (OpenAI compatible)") \
    X(S_OPENAI,       "OpenAI uyumlu uç nokta kullan",      "Use OpenAI-compatible endpoint") \
    X(S_BROWSE,       "Gözat…",                             "Browse…") \
    X(S_ACCOUNT,      "Hesap",                              "Account") \
    X(S_SIGNED_IN_AS, "Oturum: %s",                         "Signed in as %s") \
    X(S_NOT_SIGNED,   "Oturum yok — giriş yapın",           "Not signed in — please sign in") \
    X(S_LOGIN_TITLE,  "LuxAI hesabıyla giriş",              "Sign in with your LuxAI account") \
    X(S_LOGIN_HINT,   "Hesabınız yoksa lux-ai.cz üzerinden oluşturabilirsiniz.", \
                       "No account yet? Create one at lux-ai.cz.") \
    X(S_LOGIN_FAIL,   "Giriş başarısız",                    "Sign-in failed") \
    X(S_BAD_CREDS,    "E-posta veya parola hatalı.",        "Wrong e-mail or password.") \
    X(S_NET_ERR,      "LuxAI sunucusuna ulaşılamadı. İnternet bağlantısını denetleyin.", \
                       "Cannot reach the LuxAI server. Check your connection.") \
    X(S_THINKING,     "LuxAI yanıtlıyor…",                  "LuxAI is answering…") \
    X(S_READY,        "Hazır",                              "Ready") \
    X(S_EMPTY_TITLE,  "LuxAI",                              "LuxAI") \
    X(S_EMPTY_TEXT,   "Yapay zekayla sohbet edin veya bilgisayarınızda çalışmasına izin verin. LuxAI hesabınızla oturum açın – tüm LuxOS uygulamaları için tek hesap.", \
                       "Chat with AI or let it work with your computer. Sign in with your LuxAI account – one account for all LuxOS apps.") \
    X(S_WELCOME,      "LuxAI'ye hoş geldiniz",               "Welcome to LuxAI") \
    X(S_SIGNIN_PILL,  "LuxAI hesabıyla oturum aç",           "Sign in with LuxAI account") \
    X(S_SIGNUP_LINK,  "Ücretsiz hesap oluştur",              "Create a free account") \
    X(S_SIGNED_OUT,   "Oturum açılmadı",                     "Not signed in") \
    X(S_SEARCH_PH,    "Sohbetlerde ara",                     "Search conversations") \
    X(S_NO_CONVS,     "Henüz sohbet yok",                    "No conversations yet") \
    X(S_MODEL_FLASH,  "Flash – daha akıllı",                 "Flash – smarter") \
    X(S_MODEL_LITE,   "Lite – daha hızlı",                   "Lite – faster") \
    X(S_MSG_HINT,     "Bir mesaj yazın…",                    "Type a message…") \
    X(S_CONNECTION,   "Bağlantı",                            "Connection") \
    X(S_CONFIRM,      "Ajan izni",                          "Agent permission") \
    X(S_ALLOW,        "Bu aracı çalıştırmak istiyor:",       "LuxAI wants to run this tool:") \
    X(S_BLOCKED,      "Bu eylem güvenlik kuralları nedeniyle reddedildi.", \
                       "This action was refused by the safety rules.") \
    X(S_DENIED,       "Eylem sizin tarafınızdan reddedildi.", "You denied the action.") \
    X(S_YOU,          "Siz",                                "You") \
    X(S_TOOL,         "Araç",                               "Tool") \
    X(S_ERROR,        "Hata",                               "Error") \
    X(S_NEW_CONV,     "Yeni sohbet",                        "New conversation") \
    X(S_CONVERSATIONS,"Sohbetler",                          "Conversations") \
    X(S_DELETE,       "Bu sohbeti sil?",                    "Delete this conversation?") \
    X(S_ABOUT,        "LuxAI 3.1 — Windows için native port (BELGESELSEMO topluluk yapısı, WSL'siz)", \
                       "LuxAI 3.1 — native Windows port (community build, no WSL)") \
    X(S_TLS7,         "Not: Windows 7'de TLS 1.2 güncellemesi (KB3140245) gerekir.", \
                       "Note: Windows 7 needs the TLS 1.2 update (KB3140245).") \
    X(S_STREAMING,    "akış",                               "streaming") \
    X(S_ALLOWBTN,     "İzin ver",                            "Allow") \
    X(S_DENYBTN,      "Reddet",                              "Deny") \
    X(S_DELETEBTN,    "Sil",                                 "Delete") \
    X(S_CLOSEBTN,     "Kapat",                               "Close") \
    X(S_LOGOUT_Q,     "LuxAI oturumunu kapatayım mı?",       "Sign out of LuxAI?") \
    X(S_LOGOUT_BODY,  "Bu bilgisayardaki tüm LuxOS uygulamalarında LuxAI oturumu kapanır. Sohbetleriniz bu bilgisayarda kalır.", \
                       "You will be signed out of your LuxAI account in all LuxOS apps on this computer. Your conversations stay on this computer.") \
    X(S_MANAGE,       "Hesabı yönet",                        "Manage account") \
    X(S_EXPORT,       "Verilerimi dışa aktar",               "Export Your Data") \
    X(S_TERMS,        "Koşullar ve Gizlilik",                "Terms and Privacy") \
    X(S_DELACC,       "Hesabı sil",                          "Delete Account") \
    X(S_DELREQ,       "Hesap silme talebi",                  "Request Account Deletion") \
    X(S_USAGE,        "Kullanım",                            "Usage") \
    X(S_PROFILE,      "Profil",                              "Profile") \
    X(S_PREFS,        "Tercihler",                           "Preferences") \
    X(S_ACCT_TITLE,   "LuxAI Hesabı",                        "LuxAI Account") \
    X(S_EXPORTED_TO,  "Dışa aktarıldı: %s",                  "Exported to %s") \
    X(S_OFFLINE_INFO, "Sunucuya ulaşılamadı — çevrimdışı bilgiler gösteriliyor.", \
                       "Cannot reach the server — showing offline information.") \
    X(S_ATTACH,       "Dosya ekle",                          "Attach file") \
    X(S_NOT_NOW,      "Şimdi değil",                         "Not now") \
    X(S_NOMODEL,      "Bu model yanıt vermedi.",             "The model returned no answer.")

enum {
#define X(id, tr, en) id,
    STRS(X)
#undef X
    S_COUNT
};

#endif
