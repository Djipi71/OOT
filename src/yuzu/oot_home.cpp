// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <cmath>

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
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

/// The look, with every size scaled for the window (1 = a 1280x760 area).
QString Style(double scale) {
    const auto px = [scale](int size) { return QString::number(qRound(size * scale)); };
    QString style = QStringLiteral(R"(
OotHome { background: #121009; }
OotHome QLabel { color: #efe6d2; background: transparent; }
QLabel#eyebrow { color: #d9a93f; font-size: @11px; font-weight: 600; letter-spacing: 2px; }
QLabel#title { font-family: "Cinzel", "Trajan Pro", Georgia, serif; font-size: @34px; font-weight: 700; }
QLabel#chip { color: #a89a7c; font-size: @13px; border: 1px solid #3a3120; border-radius: @11px; padding: @3px @11px; }
QLabel#summary { font-size: @16px; }
QLabel#muted, QLabel#hint { color: #a89a7c; font-size: @12px; }
QLabel#cover { border: none; background: transparent; }
QWidget#content { background: transparent; }
QLabel#label { color: #a89a7c; font-size: @11px; font-weight: 600; letter-spacing: 1px; }
QLabel#section { color: #d9a93f; font-family: "Cinzel", Georgia, serif; font-size: @19px; font-weight: 700; }
QWidget#settings { background: #1c1810; border: 1px solid #3a3120; border-radius: @14px; }
QPushButton { color: #efe6d2; background: transparent; font-size: @13px; min-width: 0px;
    border: 1px solid #3a3120; border-radius: @8px; padding: @7px @13px; }
QPushButton:hover { border-color: #8a6a28; }
QPushButton:focus { border-color: #d9a93f; }
QPushButton#danger { color: #e7a3a3; }
QPushButton#play { color: #1a1205; font-size: @19px; font-weight: 600; padding: @12px @40px;
    border: 1px solid #f6d784; border-radius: @10px;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f0c45a, stop:1 #c48f26); }
QPushButton#play:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f6d070, stop:1 #d29b30); }
QPushButton#seg { border-radius: 0px; padding: @7px @6px; }
QPushButton#seg:checked { color: #1a1205; background: #d9a93f; border-color: #d9a93f; }
QPushButton#link { color: #d9a93f; border: none; text-decoration: underline; padding: 0px; }
QComboBox { color: #efe6d2; background: #121009; font-size: @13px; border: 1px solid #3a3120; border-radius: @8px; padding: @6px @9px; }
QComboBox QAbstractItemView { color: #efe6d2; background: #1c1810; selection-background-color: #8a6a28; }
QCheckBox { color: #efe6d2; background: transparent; font-size: @13px; spacing: @8px; }
QCheckBox::indicator { width: @15px; height: @15px; border: 1px solid #8a6a28; border-radius: 3px; background: #121009; }
QCheckBox::indicator:checked { background: #d9a93f; }
QSlider { background: transparent; }
QSlider::groove:horizontal { height: @4px; background: #3a3120; border-radius: 2px; }
QSlider::sub-page:horizontal { background: #d9a93f; border-radius: 2px; }
QSlider::handle:horizontal { width: @16px; margin: -@6px 0px; border-radius: @8px; background: #d9a93f; }
)");
    // Every "@<n>px" is a size to scale.
    static const QRegularExpression size(QStringLiteral("@(\\d+)px"));
    QString out;
    qsizetype last = 0;
    for (auto it = size.globalMatch(style); it.hasNext();) {
        const auto match = it.next();
        out += style.mid(last, match.capturedStart() - last);
        out += px(match.captured(1).toInt()) + QStringLiteral("px");
        last = match.capturedEnd();
    }
    out += style.mid(last);
    return out;
}

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
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
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

    // Everything sits in one block kept in the middle of the window, scaled with it.
    auto* root = new QVBoxLayout(this);
    root->addStretch(1);
    auto* centre = new QHBoxLayout;
    centre->addStretch(1);
    content = new QWidget(this);
    content->setObjectName(QStringLiteral("content"));
    content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    centre->addWidget(content, 100);
    centre->addStretch(1);
    root->addLayout(centre);
    root->addStretch(1);

    auto* column = new QVBoxLayout(content);
    column->setContentsMargins(0, 0, 0, 0);
    spacings.push_back({column, 28});

    // The game: cover, title, version, summary, Play and the dump's own buttons.
    auto* game = new QHBoxLayout;
    spacings.push_back({game, 28});
    cover = Label({}, "cover", content);
    cover->setAlignment(Qt::AlignCenter);
    game->addWidget(cover, 0, Qt::AlignTop);

    auto* text = new QVBoxLayout;
    spacings.push_back({text, 10});
    text->addWidget(Label(tr("JEU CHARGÉ"), "eyebrow", content));
    title = Label({}, "title", content);
    title->setWordWrap(true);
    text->addWidget(title);
    auto* chips = new QHBoxLayout;
    chips->addWidget(Label(tr("Switch 2"), "chip", content));
    version = Label({}, "chip", content);
    chips->addWidget(version);
    program_id = Label({}, "chip", content);
    chips->addWidget(program_id);
    chips->addStretch();
    text->addLayout(chips);
    auto* summary = Label(
        tr("Link, un jeune garçon de la forêt Kokiri, part à la recherche des trois pierres "
           "ancestrales pour empêcher Ganondorf de s'emparer de la Triforce. Entre les temples, "
           "l'ocarina et les voyages dans le temps, l'aventure fondatrice de Zelda en 3D revient "
           "au goût du jour sur Switch 2."),
        "summary", content);
    summary->setWordWrap(true);
    text->addWidget(summary);
    text->addSpacing(6);

    auto* play_row = new QHBoxLayout;
    spacings.push_back({play_row, 18});
    play = new QPushButton(tr("▶  Jouer"), content);
    play->setObjectName(QStringLiteral("play"));
    play->setCursor(Qt::PointingHandCursor);
    play->setDefault(true);
    connect(play, &QPushButton::clicked, this, [this] { hooks.play(); });
    play_row->addWidget(play);
    dump_file = Label({}, "muted", content);
    dump_file->setWordWrap(true);
    dump_file->setTextInteractionFlags(Qt::TextSelectableByMouse);
    play_row->addWidget(dump_file, 1);
    text->addLayout(play_row);

    auto* links = new QHBoxLayout;
    auto* choose = new QPushButton(tr("Choisir un autre dump…"), content);
    connect(choose, &QPushButton::clicked, this, [this] { hooks.choose_dump(); });
    auto* folder = new QPushButton(tr("Ouvrir le dossier portable"), content);
    connect(folder, &QPushButton::clicked, this, [] {
        QtCommon::Game::OpenEdenFolder(Common::FS::EdenPath::EdenDir);
    });
    auto* cache = new QPushButton(tr("Supprimer le cache de shaders…"), content);
    cache->setObjectName(QStringLiteral("danger"));
    connect(cache, &QPushButton::clicked, this, [this] { hooks.delete_shader_cache(); });
    links->addWidget(choose);
    links->addWidget(folder);
    links->addWidget(cache);
    links->addStretch();
    text->addLayout(links);
    game->addLayout(text, 1);
    column->addLayout(game);

    // Quick settings, under the game: two rows of two.
    auto* settings = new QWidget(content);
    settings->setObjectName(QStringLiteral("settings"));
    settings->setAttribute(Qt::WA_StyledBackground);
    auto* form = new QVBoxLayout(settings);
    margins.push_back({form, 22});
    spacings.push_back({form, 16});
    auto* header = new QHBoxLayout;
    header->addWidget(Label(tr("Réglages rapides"), "section", settings));
    header->addStretch();
    auto* all = new QPushButton(tr("Tous les réglages…"), settings);
    all->setObjectName(QStringLiteral("link"));
    all->setCursor(Qt::PointingHandCursor);
    connect(all, &QPushButton::clicked, this, [this] { hooks.open_settings(); });
    header->addWidget(all);
    form->addLayout(header);

    auto* grid = new QGridLayout;
    spacings.push_back({grid, 30});
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    form->addLayout(grid);
    const auto cell = [this, grid](int row, int col) {
        auto* box = new QVBoxLayout;
        spacings.push_back({box, 8});
        grid->addLayout(box, row, col, Qt::AlignTop);
        return box;
    };

    auto* fps_box = cell(0, 0);
    fps_box->addWidget(Label(tr("IMAGES PAR SECONDE"), "label", settings));
    fps = Segments(settings, fps_box,
                   {{tr("30 FPS"), 30}, {tr("60 FPS"), 60}, {tr("Haute fréquence"), 1}});
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
    fps_box->addLayout(fps_row);
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

    auto* res_box = cell(0, 1);
    res_box->addWidget(Label(tr("RÉSOLUTION DE RENDU"), "label", settings));
    resolution = Segments(settings, res_box,
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
    res_box->addWidget(fullscreen);

    auto* aa_box = cell(1, 0);
    aa_box->addWidget(Label(tr("ANTI-ALIASING"), "label", settings));
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
    aa_box->addWidget(anti_aliasing);

    auto* volume_box = cell(1, 1);
    auto* volume_row = new QHBoxLayout;
    volume_row->addWidget(Label(tr("VOLUME"), "label", settings));
    volume_row->addStretch();
    volume_value = Label({}, "hint", settings);
    volume_row->addWidget(volume_value);
    volume_box->addLayout(volume_row);
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
    volume_box->addWidget(volume);
    auto* controls = new QPushButton(tr("Configurer la manette…"), settings);
    connect(controls, &QPushButton::clicked, this, [this] { hooks.open_controls(); });
    volume_box->addWidget(controls, 0, Qt::AlignLeft);
    column->addWidget(settings);

    ApplyScale(1.0);

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

    cover_source = info.icon.isNull()
                       ? QPixmap(QStringLiteral(":/icons/default/256x256/lichii.png"))
                       : QPixmap::fromImage(info.icon);
    ShowCover();
}

void OotHome::ShowCover() {
    if (cover_source.isNull()) {
        cover->clear();
        return;
    }
    // The image fills a rounded card with its own gold edge: no square corners show.
    const qreal ratio = devicePixelRatioF();
    const QSize target = cover->size() * ratio;
    QPixmap image = cover_source.scaled(target, Qt::KeepAspectRatioByExpanding,
                                        Qt::SmoothTransformation);
    image = image.copy((image.width() - target.width()) / 2,
                       (image.height() - target.height()) / 2, target.width(), target.height());
    QPixmap card(target);
    card.fill(Qt::transparent);
    {
        QPainter painter(&card);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        const qreal edge = 1.5 * ratio;
        const qreal radius = 16 * scale * ratio;
        QPainterPath path;
        path.addRoundedRect(QRectF(card.rect()).adjusted(edge / 2, edge / 2, -edge / 2, -edge / 2),
                            radius, radius);
        painter.setClipPath(path);
        painter.drawPixmap(0, 0, image);
        painter.setClipping(false);
        painter.setPen(QPen(QColor(0x8a, 0x6a, 0x28), edge));
        painter.drawPath(path);
    }
    card.setDevicePixelRatio(ratio);
    cover->setPixmap(card);
}

void OotHome::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    const double fit = std::min(width() / 1280.0, height() / 760.0);
    const double next = std::clamp(fit, 0.8, 2.4);
    if (std::abs(next - scale) > 0.04) {
        ApplyScale(next);
    }
}

void OotHome::ApplyScale(double next) {
    scale = next;
    const auto px = [next](int size) { return static_cast<int>(std::lround(size * next)); };
    setStyleSheet(Style(next));
    content->setMaximumWidth(px(1180));
    cover->setFixedSize(px(260), px(260));
    for (const auto& [layout, size] : spacings) {
        layout->setSpacing(px(size));
    }
    for (const auto& [layout, size] : margins) {
        layout->setContentsMargins(px(size), px(size), px(size), px(size));
    }
    ShowCover();
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
