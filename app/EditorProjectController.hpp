#pragma once

#include <QObject>
#include <QPoint>
#include <QStringList>
#include <QUrl>
#include <QVariantList>

#include <array>
#include <vector>

class ImageInputController;

class EditorProjectController final : public QObject {
    Q_OBJECT

    Q_PROPERTY(int workspaceMode READ workspaceMode WRITE setWorkspaceMode NOTIFY projectChanged)
    Q_PROPERTY(bool f18aEnabled READ f18aEnabled WRITE setF18aEnabled NOTIFY projectChanged)
    Q_PROPERTY(int previewTarget READ previewTarget WRITE setPreviewTarget NOTIFY projectChanged)
    Q_PROPERTY(int editScope READ editScope WRITE setEditScope NOTIFY projectChanged)
    Q_PROPERTY(int activeCharacterSet READ activeCharacterSet WRITE setActiveCharacterSet NOTIFY projectChanged)
    Q_PROPERTY(int activeCharacterPattern READ activeCharacterPattern WRITE setActiveCharacterPattern NOTIFY projectChanged)
    Q_PROPERTY(QVariantList characterSetNames READ characterSetNames NOTIFY projectChanged)
    Q_PROPERTY(int activeSpriteSet READ activeSpriteSet WRITE setActiveSpriteSet NOTIFY projectChanged)
    Q_PROPERTY(int activeSprite READ activeSprite WRITE setActiveSprite NOTIFY projectChanged)
    Q_PROPERTY(QVariantList spriteSetNames READ spriteSetNames NOTIFY projectChanged)
    Q_PROPERTY(QVariantList activeSpritePlacements READ activeSpritePlacements NOTIFY projectChanged)
    Q_PROPERTY(bool spritePlacementMode READ spritePlacementMode WRITE setSpritePlacementMode NOTIFY projectChanged)
    Q_PROPERTY(int placementWidth READ placementWidth WRITE setPlacementWidth NOTIFY projectChanged)
    Q_PROPERTY(int placementHeight READ placementHeight WRITE setPlacementHeight NOTIFY projectChanged)
    Q_PROPERTY(QString recipePath READ recipePath NOTIFY projectChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY statusChanged)

public:
    explicit EditorProjectController(ImageInputController* imageInput,
                                     QObject* parent = nullptr);

    [[nodiscard]] int workspaceMode() const { return workspaceMode_; }
    [[nodiscard]] bool f18aEnabled() const { return f18aEnabled_; }
    [[nodiscard]] int previewTarget() const { return previewTarget_; }
    [[nodiscard]] int editScope() const { return editScope_; }
    [[nodiscard]] int activeCharacterSet() const { return activeCharacterSet_; }
    [[nodiscard]] int activeCharacterPattern() const { return activeCharacterPattern_; }
    [[nodiscard]] QVariantList characterSetNames() const;
    [[nodiscard]] int activeSpriteSet() const { return activeSpriteSet_; }
    [[nodiscard]] int activeSprite() const { return activeSprite_; }
    [[nodiscard]] QVariantList spriteSetNames() const;
    [[nodiscard]] QVariantList activeSpritePlacements() const;
    [[nodiscard]] bool spritePlacementMode() const { return spritePlacementMode_; }
    [[nodiscard]] int placementWidth() const { return placementWidth_; }
    [[nodiscard]] int placementHeight() const { return placementHeight_; }
    [[nodiscard]] QString recipePath() const { return recipePath_; }
    [[nodiscard]] QString statusMessage() const { return statusMessage_; }
    [[nodiscard]] QString errorMessage() const { return errorMessage_; }

    void setWorkspaceMode(int value);
    void setF18aEnabled(bool value);
    void setPreviewTarget(int value);
    void setEditScope(int value);
    void setActiveCharacterSet(int value);
    void setActiveCharacterPattern(int value);
    void setActiveSpriteSet(int value);
    void setActiveSprite(int value);
    void setSpritePlacementMode(bool value);
    void setPlacementWidth(int value);
    void setPlacementHeight(int value);

    Q_INVOKABLE void addSpriteSet();
    Q_INVOKABLE void removeActiveSpriteSet();
    Q_INVOKABLE void moveSprite(int spriteIndex, int x, int y);
    Q_INVOKABLE bool saveRecipe(const QUrl& fileUrl);
    Q_INVOKABLE bool loadRecipe(const QUrl& fileUrl);
    Q_INVOKABLE void clearStatus();

signals:
    void projectChanged();
    void statusChanged();

private:
    struct SpritePlacement {
        int x{};
        int y{};
        bool visible{true};
    };
    struct SpriteSet {
        QString name;
        std::array<SpritePlacement, 32> placements;
    };

    [[nodiscard]] SpriteSet makeSpriteSet(int ordinal) const;
    void setStatus(QString message, QString error = {});

    ImageInputController* imageInput_{};
    int workspaceMode_{};
    bool f18aEnabled_{true};
    int previewTarget_{};
    int editScope_{};
    int activeCharacterSet_{};
    int activeCharacterPattern_{};
    QStringList characterSetNames_{QStringLiteral("Set 1"),
                                   QStringLiteral("Set 2"),
                                   QStringLiteral("Set 3")};
    int activeSpriteSet_{};
    int activeSprite_{};
    std::vector<SpriteSet> spriteSets_;
    bool spritePlacementMode_{};
    int placementWidth_{256};
    int placementHeight_{192};
    QString recipePath_;
    QString statusMessage_;
    QString errorMessage_;
};
