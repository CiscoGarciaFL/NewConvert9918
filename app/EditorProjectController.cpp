#include "EditorProjectController.hpp"

#include "ImageInputController.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <algorithm>

namespace {

QString workspaceName(int mode)
{
    switch (mode) {
    case 1: return QStringLiteral("character");
    case 2: return QStringLiteral("sprite");
    default: return QStringLiteral("screen-image");
    }
}

int workspaceValue(const QString& name)
{
    if (name == QStringLiteral("character")) return 1;
    if (name == QStringLiteral("sprite")) return 2;
    return 0;
}

QString previewName(int target)
{
    switch (target) {
    case 1: return QStringLiteral("f18a");
    case 2: return QStringLiteral("compare");
    default: return QStringLiteral("tms9918a");
    }
}

int previewValue(const QString& name)
{
    if (name == QStringLiteral("f18a")) return 1;
    if (name == QStringLiteral("compare")) return 2;
    return 0;
}

} // namespace

EditorProjectController::EditorProjectController(ImageInputController* imageInput,
                                                 QObject* parent)
    : QObject(parent), imageInput_(imageInput)
{
    spriteSets_.push_back(makeSpriteSet(1));
}

QVariantList EditorProjectController::characterSetNames() const
{
    QVariantList result;
    for (const QString& name : characterSetNames_) result.push_back(name);
    return result;
}

QVariantList EditorProjectController::spriteSetNames() const
{
    QVariantList result;
    for (const auto& set : spriteSets_) result.push_back(set.name);
    return result;
}

QVariantList EditorProjectController::activeSpritePlacements() const
{
    QVariantList result;
    if (spriteSets_.empty()) return result;
    const auto& placements = spriteSets_[static_cast<std::size_t>(activeSpriteSet_)].placements;
    for (int index = 0; index < static_cast<int>(placements.size()); ++index) {
        const auto& placement = placements[static_cast<std::size_t>(index)];
        result.push_back(QVariantMap{{QStringLiteral("index"), index},
                                     {QStringLiteral("x"), placement.x},
                                     {QStringLiteral("y"), placement.y},
                                     {QStringLiteral("visible"), placement.visible}});
    }
    return result;
}

void EditorProjectController::setWorkspaceMode(int value)
{
    value = std::clamp(value, 0, 2);
    if (workspaceMode_ == value) return;
    workspaceMode_ = value;
    emit projectChanged();
}

void EditorProjectController::setF18aEnabled(bool value)
{
    if (f18aEnabled_ == value) return;
    f18aEnabled_ = value;
    if (!value) {
        previewTarget_ = 0;
        editScope_ = 0;
    }
    emit projectChanged();
}

void EditorProjectController::setPreviewTarget(int value)
{
    value = std::clamp(value, 0, f18aEnabled_ ? 2 : 0);
    if (previewTarget_ == value) return;
    previewTarget_ = value;
    emit projectChanged();
}

void EditorProjectController::setEditScope(int value)
{
    value = std::clamp(value, 0, f18aEnabled_ ? 1 : 0);
    if (editScope_ == value) return;
    editScope_ = value;
    emit projectChanged();
}

void EditorProjectController::setActiveCharacterSet(int value)
{
    value = std::clamp(value, 0, 2);
    if (activeCharacterSet_ == value) return;
    activeCharacterSet_ = value;
    emit projectChanged();
}

void EditorProjectController::setActiveCharacterPattern(int value)
{
    value = std::clamp(value, 0, 255);
    if (activeCharacterPattern_ == value) return;
    activeCharacterPattern_ = value;
    emit projectChanged();
}

void EditorProjectController::setActiveSpriteSet(int value)
{
    const int maximum = std::max(0, static_cast<int>(spriteSets_.size()) - 1);
    value = std::clamp(value, 0, maximum);
    if (activeSpriteSet_ == value) return;
    activeSpriteSet_ = value;
    emit projectChanged();
}

void EditorProjectController::setActiveSprite(int value)
{
    value = std::clamp(value, 0, 31);
    if (activeSprite_ == value) return;
    activeSprite_ = value;
    emit projectChanged();
}

void EditorProjectController::setSpritePlacementMode(bool value)
{
    if (spritePlacementMode_ == value) return;
    spritePlacementMode_ = value;
    emit projectChanged();
}

void EditorProjectController::setPlacementWidth(int value)
{
    value = std::clamp(value, 8, 1024);
    if (placementWidth_ == value) return;
    placementWidth_ = value;
    emit projectChanged();
}

void EditorProjectController::setPlacementHeight(int value)
{
    value = std::clamp(value, 8, 1024);
    if (placementHeight_ == value) return;
    placementHeight_ = value;
    emit projectChanged();
}

void EditorProjectController::addSpriteSet()
{
    if (spriteSets_.size() >= 32U) {
        setStatus({}, QStringLiteral("A project can currently contain up to 32 sprite sets."));
        return;
    }
    spriteSets_.push_back(makeSpriteSet(static_cast<int>(spriteSets_.size()) + 1));
    activeSpriteSet_ = static_cast<int>(spriteSets_.size()) - 1;
    emit projectChanged();
}

void EditorProjectController::removeActiveSpriteSet()
{
    if (spriteSets_.size() <= 1U) return;
    spriteSets_.erase(spriteSets_.begin() + activeSpriteSet_);
    activeSpriteSet_ = std::min(activeSpriteSet_, static_cast<int>(spriteSets_.size()) - 1);
    emit projectChanged();
}

void EditorProjectController::moveSprite(int spriteIndex, int x, int y)
{
    if (spriteSets_.empty() || spriteIndex < 0 || spriteIndex >= 32) return;
    auto& placement = spriteSets_[static_cast<std::size_t>(activeSpriteSet_)]
                          .placements[static_cast<std::size_t>(spriteIndex)];
    x = std::clamp(x, -32, placementWidth_ - 1);
    y = std::clamp(y, -32, placementHeight_ - 1);
    if (placement.x == x && placement.y == y) return;
    placement.x = x;
    placement.y = y;
    emit projectChanged();
}

bool EditorProjectController::saveRecipe(const QUrl& fileUrl)
{
    QString path = fileUrl.toLocalFile();
    if (path.isEmpty()) {
        setStatus({}, QStringLiteral("Choose a local recipe file."));
        return false;
    }
    if (QFileInfo(path).suffix().isEmpty()) path += QStringLiteral(".nc9918.json");

    QJsonArray characterSets;
    for (const QString& name : characterSetNames_) {
        characterSets.push_back(QJsonObject{{QStringLiteral("name"), name},
                                             {QStringLiteral("capacity"), 256}});
    }
    QJsonArray spriteSets;
    for (const auto& set : spriteSets_) {
        QJsonArray placements;
        for (const auto& placement : set.placements) {
            placements.push_back(QJsonObject{{QStringLiteral("x"), placement.x},
                                              {QStringLiteral("y"), placement.y},
                                              {QStringLiteral("visible"), placement.visible}});
        }
        spriteSets.push_back(QJsonObject{{QStringLiteral("name"), set.name},
                                          {QStringLiteral("capacity"), 32},
                                          {QStringLiteral("placements"), placements}});
    }

    const QJsonObject root{
        {QStringLiteral("kind"), QStringLiteral("newconvert9918-recipe")},
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("workspace"), workspaceName(workspaceMode_)},
        {QStringLiteral("source"),
         QJsonObject{{QStringLiteral("path"), imageInput_->sourcePath()}}},
        {QStringLiteral("profiles"),
         QJsonObject{{QStringLiteral("tms9918a"),
                      QJsonObject{{QStringLiteral("enabled"), true}}},
                     {QStringLiteral("f18a"),
                      QJsonObject{{QStringLiteral("enabled"), f18aEnabled_},
                                  {QStringLiteral("inherits"),
                                   QStringLiteral("tms9918a")}}}}},
        {QStringLiteral("preview"), previewName(previewTarget_)},
        {QStringLiteral("editScope"), editScope_ == 1
                                             ? QStringLiteral("f18a-enhancements")
                                             : QStringLiteral("shared-baseline")},
        {QStringLiteral("characterEditor"),
         QJsonObject{{QStringLiteral("activeSet"), activeCharacterSet_},
                     {QStringLiteral("activePattern"), activeCharacterPattern_},
                     {QStringLiteral("sets"), characterSets}}},
        {QStringLiteral("spriteEditor"),
         QJsonObject{{QStringLiteral("activeSet"), activeSpriteSet_},
                     {QStringLiteral("activeSprite"), activeSprite_},
                     {QStringLiteral("placementMode"), spritePlacementMode_},
                     {QStringLiteral("placementWidth"), placementWidth_},
                     {QStringLiteral("placementHeight"), placementHeight_},
                     {QStringLiteral("sets"), spriteSets}}},
        {QStringLiteral("conversion"), imageInput_->recipeSettings()},
    };

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        setStatus({}, QStringLiteral("Could not open the recipe for writing: %1")
                          .arg(file.errorString()));
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        setStatus({}, QStringLiteral("Could not save the recipe: %1").arg(file.errorString()));
        return false;
    }
    recipePath_ = QFileInfo(path).absoluteFilePath();
    setStatus(QStringLiteral("Saved recipe %1").arg(QFileInfo(path).fileName()));
    emit projectChanged();
    return true;
}

bool EditorProjectController::loadRecipe(const QUrl& fileUrl)
{
    const QString path = fileUrl.toLocalFile();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setStatus({}, QStringLiteral("Could not open the recipe: %1").arg(file.errorString()));
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setStatus({}, QStringLiteral("Invalid recipe JSON: %1").arg(parseError.errorString()));
        return false;
    }
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("kind")).toString()
            != QStringLiteral("newconvert9918-recipe")
        || root.value(QStringLiteral("schemaVersion")).toInt() != 1) {
        setStatus({}, QStringLiteral("This recipe type or schema version is not supported."));
        return false;
    }

    QString settingsError;
    if (!imageInput_->applyRecipeSettings(root.value(QStringLiteral("conversion")).toObject(),
                                          &settingsError)) {
        setStatus({}, settingsError);
        return false;
    }

    workspaceMode_ = workspaceValue(root.value(QStringLiteral("workspace")).toString());
    f18aEnabled_ = root.value(QStringLiteral("profiles")).toObject()
                        .value(QStringLiteral("f18a")).toObject()
                        .value(QStringLiteral("enabled")).toBool(true);
    previewTarget_ = previewValue(root.value(QStringLiteral("preview")).toString());
    if (!f18aEnabled_) previewTarget_ = 0;
    editScope_ = root.value(QStringLiteral("editScope")).toString()
                         == QStringLiteral("f18a-enhancements")
        && f18aEnabled_ ? 1 : 0;

    const QJsonObject character = root.value(QStringLiteral("characterEditor")).toObject();
    activeCharacterSet_ = std::clamp(character.value(QStringLiteral("activeSet")).toInt(),
                                     0, 2);
    activeCharacterPattern_ = std::clamp(
        character.value(QStringLiteral("activePattern")).toInt(), 0, 255);
    const QJsonArray savedCharacterSets = character.value(QStringLiteral("sets")).toArray();
    for (int index = 0; index < std::min(3, static_cast<int>(savedCharacterSets.size())); ++index) {
        const QString name = savedCharacterSets[index].toObject()
                                 .value(QStringLiteral("name")).toString();
        if (!name.isEmpty()) characterSetNames_[index] = name;
    }

    const QJsonObject sprite = root.value(QStringLiteral("spriteEditor")).toObject();
    placementWidth_ = std::clamp(sprite.value(QStringLiteral("placementWidth")).toInt(256),
                                 8, 1024);
    placementHeight_ = std::clamp(sprite.value(QStringLiteral("placementHeight")).toInt(192),
                                  8, 1024);
    spritePlacementMode_ = sprite.value(QStringLiteral("placementMode")).toBool();
    spriteSets_.clear();
    const QJsonArray savedSpriteSets = sprite.value(QStringLiteral("sets")).toArray();
    for (int setIndex = 0;
         setIndex < std::min(32, static_cast<int>(savedSpriteSets.size())); ++setIndex) {
        const QJsonObject savedSet = savedSpriteSets[setIndex].toObject();
        SpriteSet set = makeSpriteSet(setIndex + 1);
        const QString name = savedSet.value(QStringLiteral("name")).toString();
        if (!name.isEmpty()) set.name = name;
        const QJsonArray placements = savedSet.value(QStringLiteral("placements")).toArray();
        for (int index = 0; index < std::min(32, static_cast<int>(placements.size())); ++index) {
            const QJsonObject savedPlacement = placements[index].toObject();
            auto& placement = set.placements[static_cast<std::size_t>(index)];
            placement.x = std::clamp(savedPlacement.value(QStringLiteral("x")).toInt(),
                                     -32, placementWidth_ - 1);
            placement.y = std::clamp(savedPlacement.value(QStringLiteral("y")).toInt(),
                                     -32, placementHeight_ - 1);
            placement.visible = savedPlacement.value(QStringLiteral("visible")).toBool(true);
        }
        spriteSets_.push_back(std::move(set));
    }
    if (spriteSets_.empty()) spriteSets_.push_back(makeSpriteSet(1));
    activeSpriteSet_ = std::clamp(sprite.value(QStringLiteral("activeSet")).toInt(), 0,
                                  static_cast<int>(spriteSets_.size()) - 1);
    activeSprite_ = std::clamp(sprite.value(QStringLiteral("activeSprite")).toInt(), 0, 31);

    const QString sourcePath = root.value(QStringLiteral("source")).toObject()
                                   .value(QStringLiteral("path")).toString();
    if (!sourcePath.isEmpty()) {
        const QString resolved = QFileInfo(sourcePath).isAbsolute()
            ? sourcePath
            : QFileInfo(path).dir().absoluteFilePath(sourcePath);
        if (QFileInfo::exists(resolved)) imageInput_->openUrl(QUrl::fromLocalFile(resolved));
    }

    recipePath_ = QFileInfo(path).absoluteFilePath();
    setStatus(QStringLiteral("Loaded recipe %1").arg(QFileInfo(path).fileName()));
    emit projectChanged();
    return true;
}

void EditorProjectController::clearStatus()
{
    setStatus({});
}

EditorProjectController::SpriteSet EditorProjectController::makeSpriteSet(int ordinal) const
{
    SpriteSet set;
    set.name = QStringLiteral("Set %1").arg(ordinal);
    for (int index = 0; index < 32; ++index) {
        auto& placement = set.placements[static_cast<std::size_t>(index)];
        placement.x = 8 + (index % 8) * 30;
        placement.y = 8 + (index / 8) * 42;
    }
    return set;
}

void EditorProjectController::setStatus(QString message, QString error)
{
    statusMessage_ = std::move(message);
    errorMessage_ = std::move(error);
    emit statusChanged();
}
