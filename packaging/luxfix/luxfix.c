/* LuxAI — kenar cubugu "Oturumu kapat" duzeltmesi (LuxOS 2.0.2 hatasi).
 *
 * Hata: soldaki hesap popover'inda "Oturumu kapat..." onay penceresi acilir,
 * "Oturumu kapat"a basinca HICBIR SEY olmaz (upstream ikili dosyada onay
 * dalinin govdesi bos; strace ile dogrulandi: ne exec, ne connect, ne unlink).
 * Hamburger menusundeki hesap penceresi ise calisir cunku ayri bir program.
 *
 * Cozum: gtk_dialog_run sarmalanir. Onay penceresi (18 dildeki basligiyla
 * taninir) onaylanirsa:
 *   1) `luxos-account logout` calistirilir (POST /api/ide/logout + yerel token silinir)
 *   2) uygulama ayni argumanlarla yeniden baslatilir -> arayuz "oturum kapali"
 *      durumuna gecer (uygulama hesap durumunu bellekte tuttugu icin yeniden
 *      baslatma sart).
 *
 * Derleme: gcc -shared -fPIC -O2 -o luxfix.so luxfix.c $(pkg-config --cflags --libs gtk+-3.0)
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

/* "Sign out of LuxAI?" — ingilizce + cevrilmis tum varyantlar (gtk ceviri) */
static const char *const SIGNOUT_TITLES[] = {
    "Sign out of LuxAI?",
    "LuxAI oturumu kapatılsın mı?",
    "Выйти из LuxAI?",
    "LuxAI からサインアウトしますか？",
    "退出 LuxAI？",
    "LuxAI से साइन आउट करना है?",
    NULL
};

/* baslik eslesmesi: bazi dillerde dakika/saniye gibi ekler olabilir -> onek kontrolu */
static int title_matches(const char *t)
{
    if (!t || !*t) return 0;
    for (int i = 0; SIGNOUT_TITLES[i]; i++) {
        const char *s = SIGNOUT_TITLES[i];
        size_t n = strlen(s);
        if (strncmp(t, s, n) == 0) return 1;
    }
    return 0;
}

static int walk_labels(GtkWidget *w)
{
    if (GTK_IS_LABEL(w) && title_matches(gtk_label_get_text(GTK_LABEL(w))))
        return 1;
    if (GTK_IS_CONTAINER(w)) {
        GList *ch = gtk_container_get_children(GTK_CONTAINER(w));
        for (GList *i = ch; i; i = i->next) {
            if (walk_labels(GTK_WIDGET(i->data))) { g_list_free(ch); return 1; }
        }
        g_list_free(ch);
    }
    return 0;
}

static void restart_app(void)
{
    /* /proc/self/cmdline -> argv (uygulama ayni argumanlarla yeniden acilir) */
    char buf[4096];
    int fd = open("/proc/self/cmdline", O_RDONLY);
    if (fd >= 0) {
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0) {
            buf[n] = 0;
            char *argv[64];
            int argc = 0;
            for (char *p = buf; p < buf + n && argc < 63; p += strlen(p) + 1)
                argv[argc++] = p;
            argv[argc] = NULL;
            execv("/proc/self/exe", argv);
        }
    }
    execl("/proc/self/exe", "luxai", (char *)NULL);
}

static void do_logout(void)
{
    /* 1) hesap aracini calistir: sunucuda oturumu kapatir + account.json siler */
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        execl("/usr/bin/luxos-account", "luxos-account", "logout", (char *)NULL);
        execlp("luxos-account", "luxos-account", "logout", (char *)NULL);
        _exit(127);
    } else if (pid > 0) {
        int st = 0;
        for (int i = 0; i < 60; i++) {          /* en fazla ~6 sn */
            if (waitpid(pid, &st, WNOHANG) == pid) { st = 1; break; }
            usleep(100000);
        }
        if (!st) { kill(pid, SIGTERM); waitpid(pid, &st, 0); }
    }
    /* 2) arac yoksa/başarisizsa en azindan yerel token'i temizle */
    const char *xdg = getenv("XDG_CONFIG_HOME");
    char path[1024];
    if (xdg && *xdg) snprintf(path, sizeof(path), "%s/luxai/account.json", xdg);
    else {
        const char *home = getenv("HOME");
        snprintf(path, sizeof(path), "%s/.config/luxai/account.json", home ? home : "");
    }
    if (!access(path, F_OK)) unlink(path);
}

int gtk_dialog_run(GtkDialog *dlg)
{
    static int (*real)(GtkDialog *);
    if (!real) real = (int (*)(GtkDialog *))dlsym(RTLD_NEXT, "gtk_dialog_run");
    int is_signout = walk_labels(GTK_WIDGET(dlg));
    int r = real(dlg);
    if (is_signout && r != GTK_RESPONSE_CANCEL && r != GTK_RESPONSE_DELETE_EVENT &&
        r != GTK_RESPONSE_NONE) {
        do_logout();
        restart_app();
    }
    return r;
}
