#pragma once

#include <QObject>

class ImageInputController;

class AppPreferencesController final : public QObject {
    Q_OBJECT

    Q_PROPERTY(int previewLayout READ previewLayout WRITE setPreviewLayout NOTIFY preferencesChanged)
    Q_PROPERTY(bool sidePanelVisible READ sidePanelVisible WRITE setSidePanelVisible NOTIFY preferencesChanged)
    Q_PROPERTY(int sidePanelMode READ sidePanelMode WRITE setSidePanelMode NOTIFY preferencesChanged)
    Q_PROPERTY(bool restoreWindowGeometry READ restoreWindowGeometry WRITE setRestoreWindowGeometry NOTIFY preferencesChanged)
    Q_PROPERTY(bool rememberWorkspaceMode READ rememberWorkspaceMode WRITE setRememberWorkspaceMode NOTIFY preferencesChanged)
    Q_PROPERTY(int lastWorkspaceMode READ lastWorkspaceMode WRITE setLastWorkspaceMode NOTIFY preferencesChanged)
    Q_PROPERTY(bool rememberConversionSettings READ rememberConversionSettings WRITE setRememberConversionSettings NOTIFY preferencesChanged)
    Q_PROPERTY(int defaultPreset READ defaultPreset WRITE setDefaultPreset NOTIFY preferencesChanged)
    Q_PROPERTY(int defaultExportFormat READ defaultExportFormat WRITE setDefaultExportFormat NOTIFY preferencesChanged)
    Q_PROPERTY(bool hasWindowGeometry READ hasWindowGeometry NOTIFY preferencesChanged)
    Q_PROPERTY(int windowX READ windowX NOTIFY preferencesChanged)
    Q_PROPERTY(int windowY READ windowY NOTIFY preferencesChanged)
    Q_PROPERTY(int windowWidth READ windowWidth NOTIFY preferencesChanged)
    Q_PROPERTY(int windowHeight READ windowHeight NOTIFY preferencesChanged)

public:
    explicit AppPreferencesController(ImageInputController* imageInput,
                                      QObject* parent = nullptr);

    [[nodiscard]] int previewLayout() const { return previewLayout_; }
    [[nodiscard]] bool sidePanelVisible() const { return sidePanelVisible_; }
    [[nodiscard]] int sidePanelMode() const { return sidePanelMode_; }
    [[nodiscard]] bool restoreWindowGeometry() const { return restoreWindowGeometry_; }
    [[nodiscard]] bool rememberWorkspaceMode() const { return rememberWorkspaceMode_; }
    [[nodiscard]] int lastWorkspaceMode() const { return lastWorkspaceMode_; }
    [[nodiscard]] bool rememberConversionSettings() const {
        return rememberConversionSettings_;
    }
    [[nodiscard]] int defaultPreset() const { return defaultPreset_; }
    [[nodiscard]] int defaultExportFormat() const { return defaultExportFormat_; }
    [[nodiscard]] bool hasWindowGeometry() const { return hasWindowGeometry_; }
    [[nodiscard]] int windowX() const { return windowX_; }
    [[nodiscard]] int windowY() const { return windowY_; }
    [[nodiscard]] int windowWidth() const { return windowWidth_; }
    [[nodiscard]] int windowHeight() const { return windowHeight_; }

    void setPreviewLayout(int value);
    void setSidePanelVisible(bool value);
    void setSidePanelMode(int value);
    void setRestoreWindowGeometry(bool value);
    void setRememberWorkspaceMode(bool value);
    void setLastWorkspaceMode(int value);
    void setRememberConversionSettings(bool value);
    void setDefaultPreset(int value);
    void setDefaultExportFormat(int value);

    Q_INVOKABLE void saveWindowGeometry(int x, int y, int width, int height);
    Q_INVOKABLE void applyStartupPreferences();
    Q_INVOKABLE void resetInterfaceSettings();
    Q_INVOKABLE void resetBehaviorSettings();
    Q_INVOKABLE void resetDefaultSettings();
    Q_INVOKABLE void restoreAllDefaults();

signals:
    void preferencesChanged();

private:
    void loadSettings();
    void saveSettings() const;

    ImageInputController* imageInput_{};
    int previewLayout_{1};
    bool sidePanelVisible_{true};
    int sidePanelMode_{};
    bool restoreWindowGeometry_{true};
    bool rememberWorkspaceMode_{};
    int lastWorkspaceMode_{};
    bool rememberConversionSettings_{true};
    int defaultPreset_{};
    int defaultExportFormat_{};
    bool hasWindowGeometry_{};
    int windowX_{};
    int windowY_{};
    int windowWidth_{1360};
    int windowHeight_{860};
};
