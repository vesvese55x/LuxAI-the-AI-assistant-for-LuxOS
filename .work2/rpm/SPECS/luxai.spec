Name:           luxai
Version:        3.1.0
Release:        1%{?dist}
Summary:        LuxAI – the AI assistant for LuxOS (unofficial build)
License:        MIT and LSL
URL:            http://belgeselsemo.com.tr
Source0:        luxai-3.1.0.tar.gz
Requires:       gtk3, python3, python3-gobject, libcurl, glib2
BuildArch:      x86_64
AutoReqProv:    no

%description
LuxAI – the AI assistant for LuxOS. Chat with AI or let the agent work with your computer. Unofficial multi-distro build by BELGESELSEMO (Z.AI-assisted extraction from the original LuxOS distribution) with 17 fully translated languages: tr, ru, zh, ja, hi, de, fr, es, it, nl, pl, pt, sk, sv, cs, hu, uk.

%prep
%setup -q -c

%build

%install
mkdir -p %{buildroot}/usr
cp -a usr/. %{buildroot}/usr/

%files
/usr/lib/luxos/python/luxai_account.py
/usr/lib/luxos/python/luxai_account_gtk.py
/usr/lib/luxos/python/luxai_projects_gtk.py
/usr/lib/luxos/python/luxai_providers_gtk.py
/usr/lib/luxos/python/luxos_i18n.py
/usr/lib/luxos/python/providers_common.py
/usr/lib/luxos/python/__pycache__/luxai_account.cpython-313.pyc
/usr/lib/luxos/python/__pycache__/luxai_account_gtk.cpython-313.pyc
/usr/lib/luxos/python/__pycache__/luxai_projects_gtk.cpython-313.pyc
/usr/lib/luxos/python/__pycache__/luxai_providers_gtk.cpython-313.pyc
/usr/lib/luxos/python/__pycache__/luxos_i18n.cpython-313.pyc
/usr/lib/luxos/python/__pycache__/providers_common.cpython-313.pyc
/usr/lib/luxai/luxai-bin
/usr/lib/luxai/luxai-root
/usr/lib/luxai/luxai-root-auto
/usr/lib/luxai/luxai-root-setup
/usr/lib/luxai/providers_bridge.py
/usr/share/luxos/i18n/cs.tsv
/usr/share/luxos/i18n/de.tsv
/usr/share/luxos/i18n/en.tsv
/usr/share/luxos/i18n/es.tsv
/usr/share/luxos/i18n/fr.tsv
/usr/share/luxos/i18n/hi.tsv
/usr/share/luxos/i18n/hu.tsv
/usr/share/luxos/i18n/it.tsv
/usr/share/luxos/i18n/ja.tsv
/usr/share/luxos/i18n/nl.tsv
/usr/share/luxos/i18n/pl.tsv
/usr/share/luxos/i18n/pt.tsv
/usr/share/luxos/i18n/ru.tsv
/usr/share/luxos/i18n/sk.tsv
/usr/share/luxos/i18n/sv.tsv
/usr/share/luxos/i18n/tr.tsv
/usr/share/luxos/i18n/uk.tsv
/usr/share/luxos/i18n/zh.tsv
/usr/share/icons/hicolor/256x256/apps/luxai.png
/usr/share/icons/hicolor/64x64/apps/luxai.png
/usr/share/icons/hicolor/48x48/apps/luxai.png
/usr/share/icons/hicolor/32x32/apps/luxai.png
/usr/share/icons/hicolor/16x16/apps/luxai.png
/usr/share/icons/hicolor/128x128/apps/luxai.png
/usr/share/icons/hicolor/512x512/apps/luxai.png
/usr/share/icons/hicolor/24x24/apps/luxai.png
/usr/share/doc/luxai/LSL-ISSUES.md
/usr/share/doc/luxai/LSL-MIGRATION.md
/usr/share/doc/luxai/README.md
/usr/share/doc/luxai/REMOTE-SESSION.md
/usr/share/doc/luxai/lsl-verification.json
/usr/share/applications/luxai.desktop
/usr/share/licenses/luxai/LSL-LICENSE.txt
/usr/share/polkit-1/actions/org.luxai.root.policy
/usr/share/luxai/i18n/cs.tsv
/usr/share/luxai/i18n/de.tsv
/usr/share/luxai/i18n/en.tsv
/usr/share/luxai/i18n/es.tsv
/usr/share/luxai/i18n/fr.tsv
/usr/share/luxai/i18n/hi.tsv
/usr/share/luxai/i18n/hu.tsv
/usr/share/luxai/i18n/it.tsv
/usr/share/luxai/i18n/ja.tsv
/usr/share/luxai/i18n/nl.tsv
/usr/share/luxai/i18n/pl.tsv
/usr/share/luxai/i18n/pt.tsv
/usr/share/luxai/i18n/ru.tsv
/usr/share/luxai/i18n/sk.tsv
/usr/share/luxai/i18n/sv.tsv
/usr/share/luxai/i18n/tr.tsv
/usr/share/luxai/i18n/uk.tsv
/usr/share/luxai/i18n/zh.tsv
/usr/share/luxai/data/luxai.desktop
/usr/share/luxai/data/luxai.png
/usr/share/luxai/data/icons/luxai-128.png
/usr/share/luxai/data/icons/luxai-16.png
/usr/share/luxai/data/icons/luxai-24.png
/usr/share/luxai/data/icons/luxai-256.png
/usr/share/luxai/data/icons/luxai-32.png
/usr/share/luxai/data/icons/luxai-48.png
/usr/share/luxai/data/icons/luxai-64.png
/usr/share/metainfo/luxai.metainfo.xml
/usr/bin/luxai
/usr/bin/luxos-account

%post
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database -q /usr/share/applications || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -qf /usr/share/icons/hicolor || true
fi
exit 0
