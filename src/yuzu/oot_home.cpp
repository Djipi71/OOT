// SPDX-License-Identifier: GPL-3.0-or-later

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include "common/fs/path_util.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/file_sys/control_metadata.h"
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/vfs/vfs.h"
#include "core/loader/loader.h"
#include "qt_common/config/uisettings.h"
#include "qt_common/qt_common.h"
#include "qt_common/util/game.h"
#include "yuzu/oot_home.h"

namespace {

constexpr u64 OcarinaOfTime = 0x0400486024DEC000ULL;

const QString Style = QStringLiteral(R"(
OotHome { background: #121009; }
OotHome QLabel { color: #efe6d2; background: transparent; }
QLabel#eyebrow { color: #d9a93f; font-size: 11px; font-weight: 600; letter-spacing: 2px; }
QLabel#title { font-family: "Cinzel", "Trajan Pro", Georgia, serif; font-size: 28px; font-weight: 700; }
QLabel#chip { color: #a89a7c; border: 1px solid #3a3120; border-radius: 10px; padding: 2px 10px; }
QLabel#summary { font-size: 14px; }
QLabel#muted, QLabel#hint { color: #a89a7c; font-size: 12px; }
QLabel#cover { border: 1px solid #8a6a28; border-radius: 14px; background: #1f180c; }
QLabel#label { color: #a89a7c; font-size: 11px; font-weight: 600; letter-spacing: 1px; }
QLabel#section { color: #d9a93f; font-family: "Cinzel", Georgia, serif; font-size: 17px; font-weight: 700; }
QWidget#settings { background: #1c1810; border: 1px solid #3a3120; border-radius: 12px; }
QPushButton { color: #efe6d2; background: transparent; border: 1px solid #3a3120; border-radius: 8px; padding: 7px 12px; }
QPushButton:hover { border-color: #8a6a28; }
QPushButton:focus { border-color: #d9a93f; }
QPushButton#danger { color: #e7a3a3; }
QPushButton#play { color: #1a1205; font-size: 17px; font-weight: 600; padding: 12px 34px;
    border: 1px solid #f6d784; border-radius: 10px;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f0c45a, stop:1 #c48f26); }
QPushButton#play:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f6d070, stop:1 #d29b30); }
QPushButton#seg { border-radius: 0; padding: 6px 10px; }
QPushButton#seg:checked { color: #1a1205; background: #d9a93f; border-color: #d9a93f; }
QPushButton#link { color: #d9a93f; border: none; text-decoration: underline; padding: 0; }
QComboBox { color: #efe6d2; background: #121009; border: 1px solid #3a3120; border-radius: 8px; padding: 5px 8px; }
QComboBox QAbstractItemView { color: #efe6d2; background: #1c1810; selection-background-color: #8a6a28; }
QCheckBox { color: #efe6d2; }
QSlider::groove:horizontal { height: 4px; background: #3a3120; border-radius: 2px; }
QSlider::sub-page:horizontal { background: #d9a93f; border-radius: 2px; }
QSlider::handle:horizontal { width: 14px; margin: -6px 0; border-radius: 7px; background: #d9a93f; }
)");

QLabel* Label(const QString& text, const char* name, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setObjectName(QString::fromLatin1(name));
    return label;
}

/// A row of toggle buttons; the id of each is its value.
QButtonGroup* Segments(QWidget* parent, QBoxLayout* into,
                       std::initializer_list<std::pair<QString, int>> entries) {
    auto* group = new QButtonGroup(parent);
    auto* row = new QHBoxLayout;
    row->setSpacing(0);
    for (const auto& [text, id] : entries) {
        auto* button = new QPushButton(text, parent);
        button->setObjectName(QStringLiteral("seg"));
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        group->addButton(button, id);
        row->addWidget(button);
    }
    into->addLayout(row);
    return group;
}

enum AntiAliasingChoice { AaGame, AaNoSharpening, AaDlss, AaFsr3, AaFsr4 };

} // namespace

OotHome::OotHome(QWidget* parent, OotHomeHooks hooks_) : QWidget(parent), hooks{std::move(hooks_)} {
    setAttribute(Qt::WA_StyledBackground);
    setStyleSheet(Style);

    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(24, 24, 24, 24);
    outer->setSpacing(24);

    // The game: cover, title, version, summary, Play and the dump's own buttons.
    auto* game = new QHBoxLayout;
    game->setSpacing(20);
    cover = Label({}, "cover", this);
    cover->setFixedSize(200, 200);
    cover->setAlignment(Qt::AlignCenter);
    game->addWidget(cover, 0, Qt::AlignTop);

    auto* text = new QVBoxLayout;
    text->setSpacing(8);
    text->addWidget(Label(tr("JEU CHARGÉ"), "eyebrow", this));
    title = Label({}, "title", this);
    title->setWordWrap(true);
    text->addWidget(title);
    auto* chips = new QHBoxLayout;
    chips->addWidget(Label(tr("Switch 2"), "chip", this));
    version = Label({}, "chip", this);
    chips->addWidget(version);
    program_id = Label({}, "chip", this);
    chips->addWidget(program_id);
    chips->addStretch();
    text->addLayout(chips);
    auto* summary = Label(
        tr("Link, un jeune garçon de la forêt Kokiri, part à la recherche des trois pierres "
           "ancestrales pour empêcher Ganondorf de s'emparer de la Triforce. Entre les temples, "
           "l'ocarina et les voyages dans le temps, l'aventure fondatrice de Zelda en 3D revient "
           "au goût du jour sur Switch 2."),
        "summary", this);
    summary->setWordWrap(true);
    summary->setMaximumWidth(560);
    text->addWidget(summary);
    text->addSpacing(8);

    play = new QPushButton(tr("▶  Jouer"), this);
    play->setObjectName(QStringLiteral("play"));
    play->setCursor(Qt::PointingHandCursor);
    play->setDefault(true);
    connect(play, &QPushButton::clicked, this, [this] { hooks.play(); });
    text->addWidget(play, 0, Qt::AlignLeft);

    dump_file = Label({}, "muted", this);
    dump_file->setWordWrap(true);
    dump_file->setTextInteractionFlags(Qt::TextSelectableByMouse);
    text->addWidget(dump_file);

    auto* links = new QHBoxLayout;
    auto* choose = new QPushButton(tr("Choisir un autre dump…"), this);
    connect(choose, &QPushButton::clicked, this, [this] { hooks.choose_dump(); });
    auto* folder = new QPushButton(tr("Ouvrir le dossier portable"), this);
    connect(folder, &QPushButton::clicked, this, [] {
        QtCommon::Game::OpenEdenFolder(Common::FS::EdenPath::EdenDir);
    });
    auto* cache = new QPushButton(tr("Supprimer le cache de shaders…"), this);
    cache->setObjectName(QStringLiteral("danger"));
    connect(cache, &QPushButton::clicked, this, [this] { hooks.delete_shader_cache(); });
    links->addWidget(choose);
    links->addWidget(folder);
    links->addWidget(cache);
    links->addStretch();
    text->addSpacing(6);
    text->addLayout(links);
    text->addStretch();
    game->addLayout(text, 1);
    outer->addLayout(game, 3);

    // Quick settings.
    auto* settings = new QWidget(this);
    settings->setObjectName(QStringLiteral("settings"));
    settings->setAttribute(Qt::WA_StyledBackground);
    settings->setMinimumWidth(320);
    settings->setMaximumWidth(420);
    auto* form = new QVBoxLayout(settings);
    form->setContentsMargins(18, 18, 18, 18);
    form->setSpacing(8);
    form->addWidget(Label(tr("Réglages rapides"), "section", settings));
    form->addSpacing(6);

    form->addWidget(Label(tr("IMAGES PAR SECONDE"), "label", settings));
    fps = Segments(settings, form, {{tr("30 FPS"), 30}, {tr("60 FPS"), 60}, {tr("Haute fréquence"), 1}});
    auto* fps_row = new QHBoxLayout;
    fps_hint = Label({}, "hint", settings);
    fps_hint->setWordWrap(true);
    fps_row->addWidget(fps_hint, 1);
    refresh_cap = new QComboBox(settings);
    refresh_cap->addItem(tr("Écran"), static_cast<int>(Settings::OotRefreshCap::Display));
    refresh_cap->addItem(tr("90 Hz"), static_cast<int>(Settings::OotRefreshCap::Hz90));
    refresh_cap->addItem(tr("120 Hz"), static_cast<int>(Settings::OotRefreshCap::Hz120));
    refresh_cap->addItem(tr("144 Hz"), static_cast<int>(Settings::OotRefreshCap::Hz144));
    refresh_cap->addItem(tr("165 Hz"), static_cast<int>(Settings::OotRefreshCap::Hz165));
    refresh_cap->setToolTip(tr("Limite de la haute fréquence"));
    fps_row->addWidget(refresh_cap);
    form->addLayout(fps_row);
    connect(fps, &QButtonGroup::idClicked, this, [this](int id) {
        Settings::values.oot_30fps.SetValue(id == 30);
        Settings::values.oot_high_refresh.SetValue(id == 1);
        Settings::values.oot_fps_asked.SetValue(true);
        UpdateFpsHint();
        hooks.settings_changed();
    });
    connect(refresh_cap, &QComboBox::activated, this, [this] {
        Settings::values.oot_high_refresh_cap.SetValue(
            static_cast<Settings::OotRefreshCap>(refresh_cap->currentData().toInt()));
        hooks.settings_changed();
    });
    form->addSpacing(10);

    form->addWidget(Label(tr("RÉSOLUTION DE RENDU"), "label", settings));
    resolution = Segments(settings, form,
                          {{tr("1x"), static_cast<int>(Settings::ResolutionSetup::Res1X)},
                           {tr("1,5x"), static_cast<int>(Settings::ResolutionSetup::Res3_2X)},
                           {tr("2x"), static_cast<int>(Settings::ResolutionSetup::Res2X)},
                           {tr("3x"), static_cast<int>(Settings::ResolutionSetup::Res3X)}});
    connect(resolution, &QButtonGroup::idClicked, this, [this](int id) {
        Settings::values.resolution_setup.SetValue(static_cast<Settings::ResolutionSetup>(id));
        hooks.settings_changed();
    });
    fullscreen = new QCheckBox(tr("Démarrer en plein écran"), settings);
    connect(fullscreen, &QCheckBox::toggled, this, [this](bool on) {
        hooks.set_fullscreen(on);
        hooks.settings_changed();
    });
    form->addWidget(fullscreen);
    form->addSpacing(10);

    form->addWidget(Label(tr("ANTI-ALIASING"), "label", settings));
    anti_aliasing = new QComboBox(settings);
    anti_aliasing->addItem(tr("Celui du jeu"), AaGame);
    anti_aliasing->addItem(tr("Celui du jeu, sans netteté artificielle"), AaNoSharpening);
    anti_aliasing->addItem(tr("NVIDIA DLSS (cartes RTX)"), AaDlss);
    anti_aliasing->addItem(tr("AMD FSR 3.1"), AaFsr3);
    anti_aliasing->addItem(tr("AMD FSR 4"), AaFsr4);
    connect(anti_aliasing, &QComboBox::activated, this, [this] {
        const int choice = anti_aliasing->currentData().toInt();
        Settings::values.nvidia_dlss.SetValue(choice == AaDlss);
        Settings::OotAntiAliasing value = Settings::OotAntiAliasing::Game;
        if (choice == AaNoSharpening) {
            value = Settings::OotAntiAliasing::NoSharpening;
        } else if (choice == AaFsr3) {
            value = Settings::OotAntiAliasing::Fsr3;
        } else if (choice == AaFsr4) {
            value = Settings::OotAntiAliasing::Fsr4;
        }
        Settings::values.oot_anti_aliasing.SetValue(value);
        hooks.settings_changed();
    });
    form->addWidget(anti_aliasing);
    form->addSpacing(10);

    auto* volume_row = new QHBoxLayout;
    volume_row->addWidget(Label(tr("VOLUME"), "label", settings));
    volume_row->addStretch();
    volume_value = Label({}, "hint", settings);
    volume_row->addWidget(volume_value);
    form->addLayout(volume_row);
    volume = new QSlider(Qt::Horizontal, settings);
    volume->setRange(0, 200);
    volume->setPageStep(10);
    connect(volume, &QSlider::valueChanged, this, [this](int value) {
        volume_value->setText(tr("%1 %").arg(value));
        Settings::values.volume.SetValue(static_cast<u8>(value));
        if (!volume->isSliderDown()) {
            hooks.settings_changed();
        }
    });
    connect(volume, &QSlider::sliderReleased, this, [this] { hooks.settings_changed(); });
    form->addWidget(volume);
    auto* controls = new QPushButton(tr("Configurer la manette…"), settings);
    connect(controls, &QPushButton::clicked, this, [this] { hooks.open_controls(); });
    form->addWidget(controls);
    form->addSpacing(10);

    auto* all = new QPushButton(tr("Tous les réglages…"), settings);
    all->setObjectName(QStringLiteral("link"));
    all->setCursor(Qt::PointingHandCursor);
    connect(all, &QPushButton::clicked, this, [this] { hooks.open_settings(); });
    form->addWidget(all, 0, Qt::AlignLeft);
    form->addStretch();
    outer->addWidget(settings, 2, Qt::AlignTop);

    connect(&dump_watcher, &QFutureWatcher<DumpInfo>::finished, this,
            [this] { ShowDump(dump_watcher.result()); });
    Refresh();
}

void OotHome::Refresh() {
    ShowSettings();

    const QString path = QString::fromStdString(UISettings::values.oot_game_path);
    if (path == shown_path && !path.isEmpty()) {
        return;
    }
    shown_path = path;
    ShowDump({});
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        dump_file->setText(tr("Aucun dump choisi : « Jouer » te le demandera."));
        return;
    }
    dump_file->setText(tr("Dump : %1").arg(QDir::toNativeSeparators(path)));
    if (!dump_watcher.isRunning()) {
        dump_watcher.setFuture(QtConcurrent::run(&OotHome::ReadDump, path));
    } else {
        // Read the newer path once the current read ends.
        connect(&dump_watcher, &QFutureWatcher<DumpInfo>::finished, this,
                [this] { shown_path.clear(); Refresh(); }, Qt::SingleShotConnection);
    }
}

OotHome::DumpInfo OotHome::ReadDump(const QString& path) {
    DumpInfo info;
    const auto file = Core::GetGameFileFromPath(QtCommon::vfs, path.toStdString());
    if (file == nullptr) {
        return info;
    }
    const auto loader = Loader::GetLoader(*QtCommon::system, file);
    if (loader == nullptr) {
        return info;
    }
    u64 id = 0;
    if (loader->ReadProgramId(id) == Loader::ResultStatus::Success) {
        info.program_id = id;
    }

    // An installed update's control data wins: its version is the one the game runs.
    if (id != 0) {
        const FileSys::PatchManager pm{id, QtCommon::system->GetFileSystemController(),
                                       QtCommon::system->GetContentProvider()};
        const auto [nacp, icon_file] = pm.GetControlMetadata();
        if (nacp != nullptr) {
            info.title = QString::fromStdString(nacp->GetApplicationName());
            info.version = QString::fromStdString(nacp->GetVersionString());
        }
        if (icon_file != nullptr) {
            const auto bytes = icon_file->ReadAllBytes();
            info.icon.loadFromData(bytes.data(), static_cast<int>(bytes.size()));
        }
    }
    if (info.version.isEmpty() || info.title.isEmpty()) {
        FileSys::NACP nacp;
        if (loader->ReadControlData(nacp) == Loader::ResultStatus::Success) {
            if (info.title.isEmpty()) {
                info.title = QString::fromStdString(nacp.GetApplicationName());
            }
            if (info.version.isEmpty()) {
                info.version = QString::fromStdString(nacp.GetVersionString());
            }
        }
    }
    if (info.icon.isNull()) {
        std::vector<u8> bytes;
        if (loader->ReadIcon(bytes) == Loader::ResultStatus::Success) {
            info.icon.loadFromData(bytes.data(), static_cast<int>(bytes.size()));
        }
    }
    return info;
}

void OotHome::ShowDump(const DumpInfo& info) {
    title->setText(info.title.isEmpty() ? tr("The Legend of Zelda: Ocarina of Time")
                                        : info.title);
    version->setText(info.version.isEmpty() ? tr("Version inconnue")
                                            : tr("Version %1").arg(info.version));
    version->setVisible(!shown_path.isEmpty());
    const u64 id = info.program_id != 0 ? info.program_id : OcarinaOfTime;
    program_id->setText(QStringLiteral("%1").arg(id, 16, 16, QLatin1Char('0')).toUpper());

    const QSize size = cover->size() - QSize(8, 8);
    QPixmap image = info.icon.isNull() ? QPixmap(QStringLiteral(":/icons/default/256x256/lichii.png"))
                                       : QPixmap::fromImage(info.icon);
    if (!image.isNull()) {
        image = image.scaled(size * devicePixelRatioF(), Qt::KeepAspectRatio,
                             Qt::SmoothTransformation);
        image.setDevicePixelRatio(devicePixelRatioF());
    }
    cover->setPixmap(image);
}

void OotHome::ShowSettings() {
    const bool fps30 = Settings::values.oot_30fps.GetValue();
    const bool high = Settings::values.oot_high_refresh.GetValue();
    fps->button(fps30 ? 30 : high ? 1 : 60)->setChecked(true);
    const int cap = refresh_cap->findData(
        static_cast<int>(Settings::values.oot_high_refresh_cap.GetValue()));
    refresh_cap->setCurrentIndex(cap < 0 ? 0 : cap);
    UpdateFpsHint();

    if (auto* res = resolution->button(
            static_cast<int>(Settings::values.resolution_setup.GetValue()))) {
        res->setChecked(true);
    } else if (auto* checked = resolution->checkedButton()) {
        // Another scale chosen in the full settings: none of the four is lit.
        resolution->setExclusive(false);
        checked->setChecked(false);
        resolution->setExclusive(true);
    }
    {
        const QSignalBlocker blocker(fullscreen);
        fullscreen->setChecked(hooks.fullscreen());
    }

    int aa = AaGame;
    switch (Settings::values.oot_anti_aliasing.GetValue()) {
    case Settings::OotAntiAliasing::NoSharpening:
        aa = AaNoSharpening;
        break;
    case Settings::OotAntiAliasing::Fsr3:
        aa = AaFsr3;
        break;
    case Settings::OotAntiAliasing::Fsr4:
        aa = AaFsr4;
        break;
    default:
        break;
    }
    // DLSS replaces Game and No sharpening; an FSR choice wins over it.
    if (Settings::values.nvidia_dlss.GetValue() && (aa == AaGame || aa == AaNoSharpening)) {
        aa = AaDlss;
    }
    anti_aliasing->setCurrentIndex(anti_aliasing->findData(aa));

    {
        const QSignalBlocker blocker(volume);
        volume->setValue(Settings::values.volume.GetValue());
    }
    volume_value->setText(tr("%1 %").arg(volume->value()));
}

void OotHome::UpdateFpsHint() {
    const int id = fps->checkedId();
    refresh_cap->setVisible(id == 1);
    if (id == 30) {
        fps_hint->setText(tr("30 FPS : plus stable sur les PC modestes."));
    } else if (id == 1) {
        fps_hint->setText(tr("Au-delà de 60 FPS, à vitesse de jeu normale (Ctrl+U en jeu)."));
    } else {
        fps_hint->setText(tr("60 FPS : la vitesse d'origine du jeu."));
    }
}
