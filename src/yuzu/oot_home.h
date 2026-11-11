// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <QFutureWatcher>
#include <QImage>
#include <QWidget>
#include "common/common_types.h"

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSlider;

/// What the home screen asks the main window to do.
struct OotHomeHooks {
    std::function<void()> play;
    std::function<void()> choose_dump;
    std::function<void()> delete_shader_cache;
    std::function<void()> open_settings;
    std::function<void()> open_controls;
    std::function<void()> settings_changed; ///< saves the config and updates the status bar
    std::function<bool()> fullscreen;
    std::function<void(bool)> set_fullscreen;
};

/// OOT Remake's main screen, in place of the game list: the dump of Ocarina of Time (icon,
/// title and version read from it), a Play button and a few everyday settings.
class OotHome : public QWidget {
    Q_OBJECT
public:
    explicit OotHome(QWidget* parent, OotHomeHooks hooks);

    /// Reads the dump again and shows the current settings.
    void Refresh();

private:
    struct DumpInfo {
        QString title;
        QString version;
        u64 program_id = 0;
        QImage icon;
    };
    static DumpInfo ReadDump(const QString& path);

    void ShowDump(const DumpInfo& info);
    void ShowSettings();
    void UpdateFpsHint();

    OotHomeHooks hooks;
    QFutureWatcher<DumpInfo> dump_watcher;
    QString shown_path;

    QLabel* cover = nullptr;
    QLabel* title = nullptr;
    QLabel* version = nullptr;
    QLabel* program_id = nullptr;
    QLabel* dump_file = nullptr;
    QPushButton* play = nullptr;

    QButtonGroup* fps = nullptr;
    QLabel* fps_hint = nullptr;
    QComboBox* refresh_cap = nullptr;
    QButtonGroup* resolution = nullptr;
    QCheckBox* fullscreen = nullptr;
    QComboBox* anti_aliasing = nullptr;
    QSlider* volume = nullptr;
    QLabel* volume_value = nullptr;
};
