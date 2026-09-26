#include "EditorProjectController.hpp"

#include "ImageInputController.hpp"

#include <QColor>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>

#include <algorithm>

namespace {

constexpr std::uint8_t defaultCharacterColor = 0xf1;

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

int snapCharacterTileCoordinate(int value, int maximum)
{
    return std::clamp(((value + 4) / 8) * 8, 0, maximum);
}

bool sameCharacterPattern(const auto& left, const auto& right)
{
    return left.bitmap == right.bitmap && left.colors == right.colors;
}

std::uint8_t reverseCharacterBits(std::uint8_t value)
{
    value = static_cast<std::uint8_t>(((value & 0x55U) << 1U)
                                      | ((value & 0xaaU) >> 1U));
    value = static_cast<std::uint8_t>(((value & 0x33U) << 2U)
                                      | ((value & 0xccU) >> 2U));
    return static_cast<std::uint8_t>((value << 4U) | (value >> 4U));
}

} // namespace

EditorProjectController::EditorProjectController(ImageInputController* imageInput,
                                                 QObject* parent)
    : QObject(parent), imageInput_(imageInput)
{
    for (int index = 0; index < static_cast<int>(characterSets_.size()); ++index) {
        characterSets_[static_cast<std::size_t>(index)] = makeCharacterSet(index + 1);
    }
    characterEditorSlots_.push_back({true, 0, 0, 0, 0});
    spriteSets_.push_back(makeSpriteSet(1));
    connect(imageInput_, &ImageInputController::settingsChanged,
            this, &EditorProjectController::projectChanged);
    if (QClipboard* clipboard = QGuiApplication::clipboard()) {
        connect(clipboard, &QClipboard::dataChanged,
                this, &EditorProjectController::projectChanged);
    }
}

QVariantList EditorProjectController::characterSetNames() const
{
    QVariantList result;
    for (const auto& set : characterSets_) result.push_back(set.name);
    return result;
}

QVariantList EditorProjectController::characterPaletteColors() const
{
    // Hardware color order, including transparent color zero. The baseline
    // palette is fixed; an F18A edit scope uses the current programmable
    // working palette mapped back to hardware color indexes.
    if (editScope_ == 1 && f18aEnabled_) {
        const QVariantList working = imageInput_->workingPaletteColors();
        if (working.size() == 15) {
            static constexpr std::array<int, 16> workingIndex{
                -1, 1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 2, 0};
            QVariantList result;
            result.reserve(16);
            for (const int index : workingIndex) {
                result.push_back(index < 0
                                     ? QVariant::fromValue(QColor(0, 0, 0, 0))
                                     : working.at(index));
            }
            return result;
        }
    }

    return {
        QColor(0, 0, 0, 0), QColor(0, 0, 0), QColor(32, 200, 64),
        QColor(88, 216, 120), QColor(80, 80, 232), QColor(120, 112, 248),
        QColor(208, 80, 72), QColor(64, 232, 240), QColor(248, 80, 80),
        QColor(248, 120, 120), QColor(208, 192, 80), QColor(224, 200, 128),
        QColor(32, 176, 56), QColor(200, 88, 184), QColor(200, 200, 200),
        QColor(248, 248, 248),
    };
}

QVariantList EditorProjectController::characterEditorSlots() const
{
    QVariantList result;
    result.reserve(static_cast<qsizetype>(characterEditorSlots_.size()));
    for (int index = 0; index < static_cast<int>(characterEditorSlots_.size()); ++index) {
        const auto& slot = characterEditorSlots_[static_cast<std::size_t>(index)];
        result.push_back(QVariantMap{{QStringLiteral("index"), index},
                                     {QStringLiteral("loaded"), slot.loaded},
                                     {QStringLiteral("setIndex"), slot.setIndex},
                                     {QStringLiteral("patternIndex"), slot.patternIndex},
                                     {QStringLiteral("tileX"), slot.tileX},
                                     {QStringLiteral("tileY"), slot.tileY}});
    }
    return result;
}

bool EditorProjectController::canPasteCharacterPattern() const
{
    return characterPatternFromClipboard().has_value();
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
        const int effectiveSize = editScope_ == 1 ? placement.size : spriteGlobalSize_;
        result.push_back(QVariantMap{{QStringLiteral("index"), index},
                                     {QStringLiteral("x"), placement.x},
                                     {QStringLiteral("y"), placement.y},
                                     {QStringLiteral("visible"), placement.visible},
                                     {QStringLiteral("size"), effectiveSize},
                                     {QStringLiteral("storedSize"), placement.size},
                                     {QStringLiteral("color"), placement.color},
                                     {QStringLiteral("colorDepth"),
                                      editScope_ == 1 ? placement.colorDepth : 1},
                                     {QStringLiteral("palette"), placement.palette},
                                     {QStringLiteral("flipX"),
                                      editScope_ == 1 && placement.flipX},
                                     {QStringLiteral("flipY"),
                                      editScope_ == 1 && placement.flipY},
                                     {QStringLiteral("profile"),
                                      editScope_ == 1 ? QStringLiteral("f18a")
                                                      : QStringLiteral("tms9918a")}});
    }
    return result;
}

void EditorProjectController::setWorkspaceMode(int value)
{
    value = std::clamp(value, 0, 2);
    if (workspaceMode_ == value) return;
    if (workspaceMode_ == 1 && characterPanActive_) finishCharacterPan();
    if (workspaceMode_ == 2 && spritePanActive_) finishSpritePan();
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
        activeSpriteSize_ = spriteGlobalSize_;
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
    if (spritePanActive_) finishSpritePan();
    editScope_ = value;
    if (workspaceMode_ == 2 && !spriteSets_.empty()) {
        const auto& placement = spriteSets_[static_cast<std::size_t>(activeSpriteSet_)]
                                    .placements[static_cast<std::size_t>(activeSprite_)];
        activeSpriteSize_ = value == 1 ? placement.size : spriteGlobalSize_;
        spriteDrawingColorIndex_ = value == 1
            ? std::clamp(spriteDrawingColorIndex_, 1,
                         (1 << placement.colorDepth) - 1)
            : placement.color;
    }
    emit projectChanged();
}

void EditorProjectController::setActiveCharacterSet(int value)
{
    value = std::clamp(value, 0, 2);
    if (characterPanActive_ && value != activeCharacterSet_) finishCharacterPan();
    bool changed = activeCharacterSet_ != value;
    activeCharacterSet_ = value;
    if (!characterEditorSlots_.empty()) {
        auto& slot = characterEditorSlots_[static_cast<std::size_t>(activeCharacterEditor_)];
        if (slot.loaded && slot.setIndex != value) {
            slot.setIndex = value;
            changed = true;
        }
    }
    if (!changed) return;
    emit projectChanged();
}

void EditorProjectController::setActiveCharacterPattern(int value)
{
    value = std::clamp(value, 0, 255);
    if (characterPanActive_ && value != activeCharacterPattern_) finishCharacterPan();
    bool changed = activeCharacterPattern_ != value;
    activeCharacterPattern_ = value;
    if (!characterEditorSlots_.empty()) {
        auto& slot = characterEditorSlots_[static_cast<std::size_t>(activeCharacterEditor_)];
        if (!slot.loaded || slot.setIndex != activeCharacterSet_
            || slot.patternIndex != value) {
            slot.loaded = true;
            slot.setIndex = activeCharacterSet_;
            slot.patternIndex = value;
            changed = true;
        }
    }
    if (!changed) return;
    emit projectChanged();
}

void EditorProjectController::setCharacterForegroundColorIndex(int value)
{
    value = std::clamp(value, 0, 15);
    if (characterForegroundColorIndex_ == value) return;
    characterForegroundColorIndex_ = value;
    emit projectChanged();
}

void EditorProjectController::setCharacterBackgroundColorIndex(int value)
{
    value = std::clamp(value, 0, 15);
    if (characterBackgroundColorIndex_ == value) return;
    characterBackgroundColorIndex_ = value;
    emit projectChanged();
}

void EditorProjectController::setActiveCharacterEditor(int value)
{
    if (characterEditorSlots_.empty()) return;
    value = std::clamp(value, 0, static_cast<int>(characterEditorSlots_.size()) - 1);
    if (characterPanActive_ && value != activeCharacterEditor_) finishCharacterPan();
    if (activeCharacterEditor_ == value) return;
    activeCharacterEditor_ = value;
    const auto& slot = characterEditorSlots_[static_cast<std::size_t>(value)];
    if (slot.loaded) {
        activeCharacterSet_ = slot.setIndex;
        activeCharacterPattern_ = slot.patternIndex;
    }
    emit projectChanged();
}

void EditorProjectController::setCharacterTilingMode(bool value)
{
    if (characterTilingMode_ == value) return;
    if (value && characterPanActive_) finishCharacterPan();
    characterTilingMode_ = value;
    emit projectChanged();
}

void EditorProjectController::setCharacterPanActive(bool value)
{
    if (characterPanActive_ == value) return;
    if (!value) {
        finishCharacterPan();
        return;
    }
    if (characterTilingMode_ || characterEditorSlots_.empty()) return;
    const auto& slot = characterEditorSlots_[static_cast<std::size_t>(activeCharacterEditor_)];
    if (!slot.loaded) return;
    endCharacterEdit();
    characterPanSetIndex_ = slot.setIndex;
    characterPanPatternIndex_ = slot.patternIndex;
    characterPanOriginal_ = characterSets_[static_cast<std::size_t>(slot.setIndex)]
                                .patterns[static_cast<std::size_t>(slot.patternIndex)];
    characterPanX_ = 0;
    characterPanY_ = 0;
    characterPanActive_ = true;
    ++characterRevision_;
    emit projectChanged();
}

void EditorProjectController::setActiveSpriteSet(int value)
{
    const int maximum = std::max(0, static_cast<int>(spriteSets_.size()) - 1);
    value = std::clamp(value, 0, maximum);
    if (spritePanActive_ && value != activeSpriteSet_) finishSpritePan();
    if (activeSpriteSet_ == value) return;
    activeSpriteSet_ = value;
    const auto& placement = spriteSets_[static_cast<std::size_t>(activeSpriteSet_)]
                                .placements[static_cast<std::size_t>(activeSprite_)];
    activeSpriteSize_ = editScope_ == 1 ? placement.size : spriteGlobalSize_;
    emit projectChanged();
}

void EditorProjectController::setActiveSprite(int value)
{
    value = std::clamp(value, 0, 31);
    if (spritePanActive_ && value != activeSprite_) finishSpritePan();
    if (activeSprite_ == value) return;
    activeSprite_ = value;
    if (!spriteSets_.empty()) {
        const auto& placement = spriteSets_[static_cast<std::size_t>(activeSpriteSet_)]
                                    .placements[static_cast<std::size_t>(activeSprite_)];
        activeSpriteSize_ = editScope_ == 1 ? placement.size : spriteGlobalSize_;
        if (editScope_ == 0) spriteDrawingColorIndex_ = placement.color;
    }
    emit projectChanged();
}

void EditorProjectController::setSpritePlacementMode(bool value)
{
    if (spritePlacementMode_ == value) return;
    if (value && spritePanActive_) finishSpritePan();
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
    activeSprite_ = 0;
    activeSpriteSize_ = editScope_ == 1 ? 8 : spriteGlobalSize_;
    emit projectChanged();
}

void EditorProjectController::removeActiveSpriteSet()
{
    if (spritePanActive_) finishSpritePan();
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
    const int effectiveSize = editScope_ == 1 ? placement.size : spriteGlobalSize_;
    x = std::clamp(x, -effectiveSize, placementWidth_ - 1);
    y = std::clamp(y, -effectiveSize, placementHeight_ - 1);
    if (placement.x == x && placement.y == y) return;
    placement.x = x;
    placement.y = y;
    emit projectChanged();
}

QVariantList EditorProjectController::characterPatternRows(int setIndex,
                                                           int patternIndex) const
{
    QVariantList result;
    if (setIndex < 0 || setIndex >= static_cast<int>(characterSets_.size())
        || patternIndex < 0 || patternIndex >= 256) {
        return result;
    }
    CharacterPattern panPattern;
    const CharacterPattern* pattern = &characterSets_[static_cast<std::size_t>(setIndex)]
                                           .patterns[static_cast<std::size_t>(patternIndex)];
    if (characterPanActive_ && setIndex == characterPanSetIndex_
        && patternIndex == characterPanPatternIndex_) {
        panPattern = pannedCharacterPattern();
        pattern = &panPattern;
    }
    result.reserve(8);
    for (int row = 0; row < 8; ++row) {
        const int bitmap = pattern->bitmap[static_cast<std::size_t>(row)];
        const int color = pattern->colors[static_cast<std::size_t>(row)];
        result.push_back(QVariantMap{{QStringLiteral("row"), row},
                                     {QStringLiteral("pattern"), bitmap},
                                     {QStringLiteral("color"), color},
                                     {QStringLiteral("foreground"), color >> 4},
                                     {QStringLiteral("background"), color & 0x0f}});
    }
    return result;
}

void EditorProjectController::paintCharacterPixel(int setIndex,
                                                   int patternIndex,
                                                   int row,
                                                   int column,
                                                   bool foreground)
{
    if (setIndex < 0 || setIndex >= static_cast<int>(characterSets_.size())
        || patternIndex < 0 || patternIndex >= 256
        || row < 0 || row >= 8 || column < 0 || column >= 8) {
        return;
    }
    if (characterPanActive_) return;
    if (characterEditActive_
        && (characterEditSetIndex_ != setIndex
            || characterEditPatternIndex_ != patternIndex)) {
        endCharacterEdit();
    }
    const bool standaloneEdit = !characterEditActive_;
    if (standaloneEdit) beginCharacterEdit(setIndex, patternIndex);
    auto& pattern = characterSets_[static_cast<std::size_t>(setIndex)]
                        .patterns[static_cast<std::size_t>(patternIndex)];
    auto& bitmap = pattern.bitmap[static_cast<std::size_t>(row)];
    auto& color = pattern.colors[static_cast<std::size_t>(row)];
    const auto mask = static_cast<std::uint8_t>(0x80U >> column);
    const std::uint8_t nextBitmap = foreground
        ? static_cast<std::uint8_t>(bitmap | mask)
        : static_cast<std::uint8_t>(bitmap & static_cast<std::uint8_t>(~mask));
    const auto nextColor = static_cast<std::uint8_t>(
        (characterForegroundColorIndex_ << 4) | characterBackgroundColorIndex_);
    if (bitmap != nextBitmap || color != nextColor) {
        bitmap = nextBitmap;
        color = nextColor;
        ++characterRevision_;
        emit projectChanged();
    }
    if (standaloneEdit) endCharacterEdit();
}

void EditorProjectController::beginCharacterEdit(int setIndex, int patternIndex)
{
    if (setIndex < 0 || setIndex >= static_cast<int>(characterSets_.size())
        || patternIndex < 0 || patternIndex >= 256 || characterPanActive_) {
        return;
    }
    if (characterEditActive_) {
        if (characterEditSetIndex_ == setIndex
            && characterEditPatternIndex_ == patternIndex) {
            return;
        }
        endCharacterEdit();
    }
    characterEditActive_ = true;
    characterEditSetIndex_ = setIndex;
    characterEditPatternIndex_ = patternIndex;
    characterEditBefore_ = characterSets_[static_cast<std::size_t>(setIndex)]
                               .patterns[static_cast<std::size_t>(patternIndex)];
}

void EditorProjectController::endCharacterEdit()
{
    if (!characterEditActive_) return;
    const int setIndex = characterEditSetIndex_;
    const int patternIndex = characterEditPatternIndex_;
    characterEditActive_ = false;
    const auto& after = characterSets_[static_cast<std::size_t>(setIndex)]
                            .patterns[static_cast<std::size_t>(patternIndex)];
    recordCharacterEdit(setIndex, patternIndex, characterEditBefore_, after);
    emit projectChanged();
}

void EditorProjectController::rotateActiveCharacterPattern()
{
    if (characterEditorSlots_.empty()) return;
    if (characterPanActive_) finishCharacterPan();
    endCharacterEdit();
    const auto& slot = characterEditorSlots_[static_cast<std::size_t>(activeCharacterEditor_)];
    if (!slot.loaded) return;
    auto& pattern = characterSets_[static_cast<std::size_t>(slot.setIndex)]
                        .patterns[static_cast<std::size_t>(slot.patternIndex)];
    const CharacterPattern before = pattern;
    pattern.bitmap.fill(0);
    for (int row = 0; row < 8; ++row) {
        for (int column = 0; column < 8; ++column) {
            if ((before.bitmap[static_cast<std::size_t>(row)]
                 & (0x80U >> column)) != 0) {
                pattern.bitmap[static_cast<std::size_t>(column)] |=
                    static_cast<std::uint8_t>(0x80U >> (7 - row));
            }
        }
    }
    if (sameCharacterPattern(before, pattern)) return;
    recordCharacterEdit(slot.setIndex, slot.patternIndex, before, pattern);
    ++characterRevision_;
    emit projectChanged();
}

void EditorProjectController::mirrorActiveCharacterPattern()
{
    if (characterEditorSlots_.empty()) return;
    if (characterPanActive_) finishCharacterPan();
    endCharacterEdit();
    const auto& slot = characterEditorSlots_[static_cast<std::size_t>(activeCharacterEditor_)];
    if (!slot.loaded) return;
    auto& pattern = characterSets_[static_cast<std::size_t>(slot.setIndex)]
                        .patterns[static_cast<std::size_t>(slot.patternIndex)];
    const CharacterPattern before = pattern;
    for (auto& row : pattern.bitmap) row = reverseCharacterBits(row);
    if (sameCharacterPattern(before, pattern)) return;
    recordCharacterEdit(slot.setIndex, slot.patternIndex, before, pattern);
    ++characterRevision_;
    emit projectChanged();
}

void EditorProjectController::flipActiveCharacterPattern()
{
    if (characterEditorSlots_.empty()) return;
    if (characterPanActive_) finishCharacterPan();
    endCharacterEdit();
    const auto& slot = characterEditorSlots_[static_cast<std::size_t>(activeCharacterEditor_)];
    if (!slot.loaded) return;
    auto& pattern = characterSets_[static_cast<std::size_t>(slot.setIndex)]
                        .patterns[static_cast<std::size_t>(slot.patternIndex)];
    const CharacterPattern before = pattern;
    std::reverse(pattern.bitmap.begin(), pattern.bitmap.end());
    std::reverse(pattern.colors.begin(), pattern.colors.end());
    if (sameCharacterPattern(before, pattern)) return;
    recordCharacterEdit(slot.setIndex, slot.patternIndex, before, pattern);
    ++characterRevision_;
    emit projectChanged();
}

void EditorProjectController::blankActiveCharacterPattern()
{
    if (characterEditorSlots_.empty()) return;
    if (characterPanActive_) finishCharacterPan();
    endCharacterEdit();
    const auto& slot = characterEditorSlots_[static_cast<std::size_t>(activeCharacterEditor_)];
    if (!slot.loaded) return;
    auto& pattern = characterSets_[static_cast<std::size_t>(slot.setIndex)]
                        .patterns[static_cast<std::size_t>(slot.patternIndex)];
    const CharacterPattern before = pattern;
    pattern.bitmap.fill(0);
    if (sameCharacterPattern(before, pattern)) return;
    recordCharacterEdit(slot.setIndex, slot.patternIndex, before, pattern);
    ++characterRevision_;
    emit projectChanged();
}

void EditorProjectController::nudgeCharacterPan(int horizontal, int vertical)
{
    if (!characterPanActive_) return;
    const int nextX = std::clamp(characterPanX_ + horizontal, -8, 8);
    const int nextY = std::clamp(characterPanY_ + vertical, -8, 8);
    if (nextX == characterPanX_ && nextY == characterPanY_) return;
    characterPanX_ = nextX;
    characterPanY_ = nextY;
    ++characterRevision_;
    emit projectChanged();
}

void EditorProjectController::centerCharacterPan()
{
    if (!characterPanActive_ || (characterPanX_ == 0 && characterPanY_ == 0)) return;
    characterPanX_ = 0;
    characterPanY_ = 0;
    ++characterRevision_;
    emit projectChanged();
}

void EditorProjectController::undoCharacterEdit()
{
    if (characterPanActive_ || characterUndoHistory_.empty()) return;
    endCharacterEdit();
    const CharacterHistoryEntry entry = characterUndoHistory_.back();
    characterUndoHistory_.pop_back();
    characterSets_[static_cast<std::size_t>(entry.setIndex)]
        .patterns[static_cast<std::size_t>(entry.patternIndex)] = entry.before;
    characterRedoHistory_.push_back(entry);
    ++characterRevision_;
    emit projectChanged();
}

void EditorProjectController::redoCharacterEdit()
{
    if (characterPanActive_ || characterRedoHistory_.empty()) return;
    endCharacterEdit();
    const CharacterHistoryEntry entry = characterRedoHistory_.back();
    characterRedoHistory_.pop_back();
    characterSets_[static_cast<std::size_t>(entry.setIndex)]
        .patterns[static_cast<std::size_t>(entry.patternIndex)] = entry.after;
    characterUndoHistory_.push_back(entry);
    ++characterRevision_;
    emit projectChanged();
}

bool EditorProjectController::copyActiveCharacterPattern()
{
    if (characterPanActive_) {
        setStatus({}, QStringLiteral("Finish pattern panning before copying."));
        return false;
    }
    endCharacterEdit();
    if (characterEditorSlots_.empty()) return false;
    const auto& slot = characterEditorSlots_[static_cast<std::size_t>(activeCharacterEditor_)];
    if (!slot.loaded) return false;
    const auto& pattern = characterSets_[static_cast<std::size_t>(slot.setIndex)]
                              .patterns[static_cast<std::size_t>(slot.patternIndex)];
    QJsonArray bitmap;
    QJsonArray colors;
    for (int row = 0; row < 8; ++row) {
        bitmap.push_back(QStringLiteral("%1")
                             .arg(pattern.bitmap[static_cast<std::size_t>(row)],
                                  2, 16, QLatin1Char('0'))
                             .toUpper());
        colors.push_back(QStringLiteral("%1")
                             .arg(pattern.colors[static_cast<std::size_t>(row)],
                                  2, 16, QLatin1Char('0'))
                             .toUpper());
    }
    const QJsonObject root{
        {QStringLiteral("format"), QStringLiteral("newconvert9918.character-pattern")},
        {QStringLiteral("version"), 1},
        {QStringLiteral("size"),
         QJsonObject{{QStringLiteral("width"), 8},
                     {QStringLiteral("height"), 8}}},
        {QStringLiteral("source"),
         QJsonObject{{QStringLiteral("set"), slot.setIndex},
                     {QStringLiteral("pattern"), slot.patternIndex}}},
        {QStringLiteral("bitmap"), bitmap},
        {QStringLiteral("colors"), colors},
    };
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard == nullptr) {
        setStatus({}, QStringLiteral("The system clipboard is unavailable."));
        return false;
    }
    clipboard->setText(QString::fromUtf8(QJsonDocument(root).toJson(
        QJsonDocument::Indented)));
    setStatus(QStringLiteral("Copied character pattern %1 to the clipboard.")
                  .arg(slot.patternIndex, 2, 16, QLatin1Char('0'))
                  .toUpper());
    return true;
}

bool EditorProjectController::pasteActiveCharacterPattern()
{
    if (characterPanActive_) {
        setStatus({}, QStringLiteral("Finish pattern panning before pasting."));
        return false;
    }
    endCharacterEdit();
    if (characterEditorSlots_.empty()) return false;
    const auto& slot = characterEditorSlots_[static_cast<std::size_t>(activeCharacterEditor_)];
    if (!slot.loaded) return false;
    const auto clipboardPattern = characterPatternFromClipboard();
    if (!clipboardPattern.has_value()) {
        setStatus({}, QStringLiteral("The clipboard does not contain a valid character pattern."));
        return false;
    }
    auto& pattern = characterSets_[static_cast<std::size_t>(slot.setIndex)]
                        .patterns[static_cast<std::size_t>(slot.patternIndex)];
    const CharacterPattern before = pattern;
    pattern = *clipboardPattern;
    if (!sameCharacterPattern(before, pattern)) {
        recordCharacterEdit(slot.setIndex, slot.patternIndex, before, pattern);
        ++characterRevision_;
        emit projectChanged();
    }
    setStatus(QStringLiteral("Pasted clipboard data into character pattern %1.")
                  .arg(slot.patternIndex, 2, 16, QLatin1Char('0'))
                  .toUpper());
    return true;
}

void EditorProjectController::addCharacterEditor()
{
    constexpr std::size_t maximumEditors = 768;
    if (characterEditorSlots_.size() >= maximumEditors) {
        setStatus({}, QStringLiteral("A character tray can contain up to 768 pattern tiles."));
        return;
    }
    std::array<bool, 32 * 24> occupied{};
    for (const auto& slot : characterEditorSlots_) {
        const int column = std::clamp(slot.tileX / 8, 0, 31);
        const int row = std::clamp(slot.tileY / 8, 0, 23);
        occupied[static_cast<std::size_t>(row * 32 + column)] = true;
    }
    int placement = 0;
    while (placement < static_cast<int>(occupied.size())
           && occupied[static_cast<std::size_t>(placement)]) {
        ++placement;
    }
    if (placement >= static_cast<int>(occupied.size())) placement = 0;
    characterEditorSlots_.push_back(
        {false, activeCharacterSet_, activeCharacterPattern_,
         (placement % 32) * 8, (placement / 32) * 8});
    activeCharacterEditor_ = static_cast<int>(characterEditorSlots_.size()) - 1;
    emit projectChanged();
}

void EditorProjectController::removeActiveCharacterEditor()
{
    if (characterEditorSlots_.size() <= 1U) return;
    characterEditorSlots_.erase(characterEditorSlots_.begin() + activeCharacterEditor_);
    activeCharacterEditor_ = std::min(
        activeCharacterEditor_, static_cast<int>(characterEditorSlots_.size()) - 1);
    const auto& slot = characterEditorSlots_[static_cast<std::size_t>(activeCharacterEditor_)];
    if (slot.loaded) {
        activeCharacterSet_ = slot.setIndex;
        activeCharacterPattern_ = slot.patternIndex;
    }
    emit projectChanged();
}

void EditorProjectController::moveCharacterEditor(int fromIndex, int toIndex)
{
    const int count = static_cast<int>(characterEditorSlots_.size());
    if (fromIndex < 0 || fromIndex >= count || toIndex < 0 || toIndex >= count
        || fromIndex == toIndex) {
        return;
    }
    CharacterEditorSlot slot = characterEditorSlots_[static_cast<std::size_t>(fromIndex)];
    characterEditorSlots_.erase(characterEditorSlots_.begin() + fromIndex);
    characterEditorSlots_.insert(characterEditorSlots_.begin() + toIndex, slot);
    if (activeCharacterEditor_ == fromIndex) {
        activeCharacterEditor_ = toIndex;
    } else if (fromIndex < activeCharacterEditor_ && activeCharacterEditor_ <= toIndex) {
        --activeCharacterEditor_;
    } else if (toIndex <= activeCharacterEditor_ && activeCharacterEditor_ < fromIndex) {
        ++activeCharacterEditor_;
    }
    emit projectChanged();
}

void EditorProjectController::moveCharacterTile(int editorIndex, int x, int y)
{
    if (editorIndex < 0
        || editorIndex >= static_cast<int>(characterEditorSlots_.size())) {
        return;
    }
    auto& slot = characterEditorSlots_[static_cast<std::size_t>(editorIndex)];
    const int snappedX = snapCharacterTileCoordinate(x, 248);
    const int snappedY = snapCharacterTileCoordinate(y, 184);
    if (slot.tileX == snappedX && slot.tileY == snappedY) return;
    slot.tileX = snappedX;
    slot.tileY = snappedY;
    emit projectChanged();
}

bool EditorProjectController::saveRecipe(const QUrl& fileUrl)
{
    if (characterPanActive_) finishCharacterPan();
    endCharacterEdit();
    if (spritePanActive_) finishSpritePan();
    endSpriteEdit();
    QString path = fileUrl.toLocalFile();
    if (path.isEmpty()) {
        setStatus({}, QStringLiteral("Choose a local recipe file."));
        return false;
    }
    if (QFileInfo(path).suffix().isEmpty()) path += QStringLiteral(".nc9918.json");

    QJsonArray characterSets;
    for (const auto& set : characterSets_) {
        QJsonArray savedPatterns;
        for (int patternIndex = 0; patternIndex < 256; ++patternIndex) {
            const auto& pattern = set.patterns[static_cast<std::size_t>(patternIndex)];
            const bool hasBitmap = std::any_of(pattern.bitmap.begin(), pattern.bitmap.end(),
                                               [](std::uint8_t value) { return value != 0; });
            const bool hasCustomColors = std::any_of(
                pattern.colors.begin(), pattern.colors.end(),
                [](std::uint8_t value) { return value != defaultCharacterColor; });
            if (!hasBitmap && !hasCustomColors) continue;
            QJsonArray bitmap;
            QJsonArray colors;
            for (const std::uint8_t value : pattern.bitmap) bitmap.push_back(value);
            for (const std::uint8_t value : pattern.colors) colors.push_back(value);
            savedPatterns.push_back(
                QJsonObject{{QStringLiteral("index"), patternIndex},
                            {QStringLiteral("bitmap"), bitmap},
                            {QStringLiteral("colors"), colors}});
        }
        characterSets.push_back(QJsonObject{{QStringLiteral("name"), set.name},
                                             {QStringLiteral("capacity"), 256},
                                             {QStringLiteral("patterns"), savedPatterns}});
    }
    QJsonArray characterEditors;
    for (const auto& editor : characterEditorSlots_) {
        characterEditors.push_back(
            QJsonObject{{QStringLiteral("loaded"), editor.loaded},
                        {QStringLiteral("set"), editor.setIndex},
                        {QStringLiteral("pattern"), editor.patternIndex},
                        {QStringLiteral("tileX"), editor.tileX},
                        {QStringLiteral("tileY"), editor.tileY}});
    }
    QJsonArray spriteSets;
    for (const auto& set : spriteSets_) {
        const auto saveSpriteBank = [](const auto& patterns, int size) {
            QJsonArray saved;
            const int count = size * size;
            for (int index = 0; index < static_cast<int>(patterns.size()); ++index) {
                const auto& pattern = patterns[static_cast<std::size_t>(index)];
                const bool hasBaseline = std::any_of(
                    pattern.baselinePixels.begin(),
                    pattern.baselinePixels.begin() + count,
                    [](std::uint8_t value) { return value != 0; });
                if (!hasBaseline && !pattern.f18aOverride) continue;
                QJsonArray baseline;
                QJsonArray enhanced;
                for (int pixel = 0; pixel < count; ++pixel) {
                    baseline.push_back(pattern.baselinePixels[
                        static_cast<std::size_t>(pixel)]);
                    if (pattern.f18aOverride) {
                        enhanced.push_back(pattern.f18aPixels[
                            static_cast<std::size_t>(pixel)]);
                    }
                }
                saved.push_back(
                    QJsonObject{{QStringLiteral("index"), index},
                                {QStringLiteral("baseline"), baseline},
                                {QStringLiteral("f18aOverride"),
                                 pattern.f18aOverride},
                                {QStringLiteral("f18a"), enhanced}});
            }
            return saved;
        };
        QJsonArray placements;
        for (const auto& placement : set.placements) {
            placements.push_back(QJsonObject{{QStringLiteral("x"), placement.x},
                                              {QStringLiteral("y"), placement.y},
                                              {QStringLiteral("visible"), placement.visible},
                                              {QStringLiteral("size"), placement.size},
                                              {QStringLiteral("color"), placement.color},
                                              {QStringLiteral("colorDepth"),
                                               placement.colorDepth},
                                              {QStringLiteral("palette"),
                                               placement.palette},
                                              {QStringLiteral("flipX"), placement.flipX},
                                              {QStringLiteral("flipY"), placement.flipY}});
        }
        spriteSets.push_back(QJsonObject{{QStringLiteral("name"), set.name},
                                          {QStringLiteral("capacity"), 32},
                                          {QStringLiteral("patterns8"),
                                           saveSpriteBank(set.patterns8, 8)},
                                          {QStringLiteral("patterns16"),
                                           saveSpriteBank(set.patterns16, 16)},
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
                     {QStringLiteral("activeEditor"), activeCharacterEditor_},
                     {QStringLiteral("tilingMode"), characterTilingMode_},
                     {QStringLiteral("foregroundColor"),
                      characterForegroundColorIndex_},
                     {QStringLiteral("backgroundColor"),
                      characterBackgroundColorIndex_},
                     {QStringLiteral("editors"), characterEditors},
                     {QStringLiteral("sets"), characterSets}}},
        {QStringLiteral("spriteEditor"),
         QJsonObject{{QStringLiteral("activeSet"), activeSpriteSet_},
                     {QStringLiteral("activeSprite"), activeSprite_},
                     {QStringLiteral("activeSize"), activeSpriteSize_},
                     {QStringLiteral("globalSize"), spriteGlobalSize_},
                     {QStringLiteral("drawingColor"), spriteDrawingColorIndex_},
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
    characterPanActive_ = false;
    characterPanX_ = 0;
    characterPanY_ = 0;
    characterEditActive_ = false;
    characterUndoHistory_.clear();
    characterRedoHistory_.clear();
    spritePanActive_ = false;
    spritePanX_ = 0;
    spritePanY_ = 0;
    spriteEditActive_ = false;
    spriteUndoHistory_.clear();
    spriteRedoHistory_.clear();
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
    characterForegroundColorIndex_ = std::clamp(
        character.value(QStringLiteral("foregroundColor")).toInt(15), 0, 15);
    characterBackgroundColorIndex_ = std::clamp(
        character.value(QStringLiteral("backgroundColor")).toInt(1), 0, 15);
    characterTilingMode_ = character.value(QStringLiteral("tilingMode")).toBool();
    for (int index = 0; index < static_cast<int>(characterSets_.size()); ++index) {
        characterSets_[static_cast<std::size_t>(index)] = makeCharacterSet(index + 1);
    }
    const QJsonArray savedCharacterSets = character.value(QStringLiteral("sets")).toArray();
    for (int index = 0; index < std::min(3, static_cast<int>(savedCharacterSets.size())); ++index) {
        const QJsonObject savedSet = savedCharacterSets[index].toObject();
        auto& set = characterSets_[static_cast<std::size_t>(index)];
        const QString name = savedSet.value(QStringLiteral("name")).toString();
        if (!name.isEmpty()) set.name = name;
        const QJsonArray savedPatterns = savedSet.value(QStringLiteral("patterns")).toArray();
        for (const QJsonValue& patternValue : savedPatterns) {
            const QJsonObject savedPattern = patternValue.toObject();
            const int patternIndex = savedPattern.value(QStringLiteral("index")).toInt(-1);
            if (patternIndex < 0 || patternIndex >= 256) continue;
            auto& pattern = set.patterns[static_cast<std::size_t>(patternIndex)];
            const QJsonArray bitmap = savedPattern.value(QStringLiteral("bitmap")).toArray();
            const QJsonArray colors = savedPattern.value(QStringLiteral("colors")).toArray();
            for (int row = 0; row < std::min(8, static_cast<int>(bitmap.size())); ++row) {
                pattern.bitmap[static_cast<std::size_t>(row)] = static_cast<std::uint8_t>(
                    std::clamp(bitmap[row].toInt(), 0, 255));
            }
            for (int row = 0; row < std::min(8, static_cast<int>(colors.size())); ++row) {
                pattern.colors[static_cast<std::size_t>(row)] = static_cast<std::uint8_t>(
                    std::clamp(colors[row].toInt(defaultCharacterColor), 0, 255));
            }
        }
    }
    ++characterRevision_;

    characterEditorSlots_.clear();
    const QJsonArray savedEditors = character.value(QStringLiteral("editors")).toArray();
    for (int index = 0; index < std::min(768, static_cast<int>(savedEditors.size())); ++index) {
        const QJsonObject savedEditor = savedEditors[index].toObject();
        const int defaultX = (index % 32) * 8;
        const int defaultY = (index / 32) * 8;
        characterEditorSlots_.push_back(
            {savedEditor.value(QStringLiteral("loaded")).toBool(),
             std::clamp(savedEditor.value(QStringLiteral("set")).toInt(), 0, 2),
             std::clamp(savedEditor.value(QStringLiteral("pattern")).toInt(), 0, 255),
             snapCharacterTileCoordinate(
                 savedEditor.value(QStringLiteral("tileX")).toInt(defaultX), 248),
             snapCharacterTileCoordinate(
                 savedEditor.value(QStringLiteral("tileY")).toInt(defaultY), 184)});
    }
    if (characterEditorSlots_.empty()) {
        characterEditorSlots_.push_back(
            {true, activeCharacterSet_, activeCharacterPattern_, 0, 0});
    }
    activeCharacterEditor_ = std::clamp(
        character.value(QStringLiteral("activeEditor")).toInt(), 0,
        static_cast<int>(characterEditorSlots_.size()) - 1);
    const auto& activeEditor = characterEditorSlots_[
        static_cast<std::size_t>(activeCharacterEditor_)];
    if (activeEditor.loaded) {
        activeCharacterSet_ = activeEditor.setIndex;
        activeCharacterPattern_ = activeEditor.patternIndex;
    }

    const QJsonObject sprite = root.value(QStringLiteral("spriteEditor")).toObject();
    spriteGlobalSize_ = sprite.value(QStringLiteral("globalSize")).toInt(8) >= 16
        ? 16 : 8;
    activeSpriteSize_ = sprite.value(QStringLiteral("activeSize"))
                                .toInt(spriteGlobalSize_) >= 16 ? 16 : 8;
    spriteDrawingColorIndex_ = std::clamp(
        sprite.value(QStringLiteral("drawingColor")).toInt(1), 1, 15);
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
        const auto loadSpriteBank = [](const QJsonArray& saved,
                                       auto& patterns,
                                       int size) {
            const int count = size * size;
            for (const QJsonValue& value : saved) {
                const QJsonObject object = value.toObject();
                const int index = object.value(QStringLiteral("index")).toInt(-1);
                if (index < 0 || index >= static_cast<int>(patterns.size())) continue;
                auto& pattern = patterns[static_cast<std::size_t>(index)];
                const QJsonArray baseline = object.value(
                    QStringLiteral("baseline")).toArray();
                const QJsonArray enhanced = object.value(
                    QStringLiteral("f18a")).toArray();
                for (int pixel = 0; pixel < std::min(count,
                         static_cast<int>(baseline.size())); ++pixel) {
                    pattern.baselinePixels[static_cast<std::size_t>(pixel)] =
                        static_cast<std::uint8_t>(std::clamp(
                            baseline[pixel].toInt(), 0, 1));
                }
                pattern.f18aOverride = object.value(
                    QStringLiteral("f18aOverride")).toBool();
                if (pattern.f18aOverride) {
                    pattern.f18aPixels = pattern.baselinePixels;
                    for (int pixel = 0; pixel < std::min(count,
                             static_cast<int>(enhanced.size())); ++pixel) {
                        pattern.f18aPixels[static_cast<std::size_t>(pixel)] =
                            static_cast<std::uint8_t>(std::clamp(
                                enhanced[pixel].toInt(), 0, 7));
                    }
                }
            }
        };
        loadSpriteBank(savedSet.value(QStringLiteral("patterns8")).toArray(),
                       set.patterns8, 8);
        loadSpriteBank(savedSet.value(QStringLiteral("patterns16")).toArray(),
                       set.patterns16, 16);
        const QJsonArray placements = savedSet.value(QStringLiteral("placements")).toArray();
        for (int index = 0; index < std::min(32, static_cast<int>(placements.size())); ++index) {
            const QJsonObject savedPlacement = placements[index].toObject();
            auto& placement = set.placements[static_cast<std::size_t>(index)];
            placement.x = std::clamp(savedPlacement.value(QStringLiteral("x")).toInt(),
                                     -32, placementWidth_ - 1);
            placement.y = std::clamp(savedPlacement.value(QStringLiteral("y")).toInt(),
                                     -32, placementHeight_ - 1);
            placement.visible = savedPlacement.value(QStringLiteral("visible")).toBool(true);
            placement.size = savedPlacement.value(QStringLiteral("size")).toInt(8) >= 16
                ? 16 : 8;
            placement.color = std::clamp(
                savedPlacement.value(QStringLiteral("color")).toInt(15), 1, 15);
            placement.colorDepth = std::clamp(
                savedPlacement.value(QStringLiteral("colorDepth")).toInt(1), 1, 3);
            placement.palette = std::clamp(
                savedPlacement.value(QStringLiteral("palette")).toInt(), 0, 7);
            placement.flipX = savedPlacement.value(QStringLiteral("flipX")).toBool();
            placement.flipY = savedPlacement.value(QStringLiteral("flipY")).toBool();
        }
        spriteSets_.push_back(std::move(set));
    }
    if (spriteSets_.empty()) spriteSets_.push_back(makeSpriteSet(1));
    activeSpriteSet_ = std::clamp(sprite.value(QStringLiteral("activeSet")).toInt(), 0,
                                  static_cast<int>(spriteSets_.size()) - 1);
    activeSprite_ = std::clamp(sprite.value(QStringLiteral("activeSprite")).toInt(), 0, 31);
    if (editScope_ == 0) activeSpriteSize_ = spriteGlobalSize_;
    ++spriteRevision_;

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

std::optional<EditorProjectController::CharacterPattern>
EditorProjectController::characterPatternFromClipboard() const
{
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard == nullptr) return std::nullopt;
    const QString text = clipboard->text();
    if (text.isEmpty() || text.size() > 65536) return std::nullopt;
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(text.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return std::nullopt;
    }
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("format")).toString()
            != QStringLiteral("newconvert9918.character-pattern")
        || root.value(QStringLiteral("version")).toInt() != 1) {
        return std::nullopt;
    }
    const QJsonObject size = root.value(QStringLiteral("size")).toObject();
    if (size.value(QStringLiteral("width")).toInt() != 8
        || size.value(QStringLiteral("height")).toInt() != 8) {
        return std::nullopt;
    }

    CharacterPattern pattern;
    const auto parseBytes = [](const QJsonValue& value,
                               std::array<std::uint8_t, 8>& destination) {
        const QJsonArray values = value.toArray();
        if (values.size() != 8) return false;
        for (int index = 0; index < 8; ++index) {
            bool valid = false;
            int byte = -1;
            if (values[index].isDouble()) {
                byte = values[index].toInt(-1);
                valid = byte >= 0 && byte <= 255;
            } else if (values[index].isString()) {
                QString encoded = values[index].toString().trimmed();
                if (encoded.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
                    encoded.remove(0, 2);
                }
                byte = encoded.toInt(&valid, 16);
                valid = valid && byte >= 0 && byte <= 255;
            }
            if (!valid) return false;
            destination[static_cast<std::size_t>(index)] =
                static_cast<std::uint8_t>(byte);
        }
        return true;
    };
    if (!parseBytes(root.value(QStringLiteral("bitmap")), pattern.bitmap)
        || !parseBytes(root.value(QStringLiteral("colors")), pattern.colors)) {
        return std::nullopt;
    }
    return pattern;
}

EditorProjectController::CharacterPattern
EditorProjectController::pannedCharacterPattern() const
{
    CharacterPattern result = characterPanOriginal_;
    for (int row = 0; row < 8; ++row) {
        const int sourceRow = row - characterPanY_;
        result.bitmap[static_cast<std::size_t>(row)] = 0;
        if (sourceRow < 0 || sourceRow >= 8) continue;
        std::uint8_t bits = characterPanOriginal_.bitmap[
            static_cast<std::size_t>(sourceRow)];
        if (characterPanX_ > 0) {
            bits = static_cast<std::uint8_t>(bits >> characterPanX_);
        } else if (characterPanX_ < 0) {
            bits = static_cast<std::uint8_t>(bits << -characterPanX_);
        }
        result.bitmap[static_cast<std::size_t>(row)] = bits;
        result.colors[static_cast<std::size_t>(row)] =
            characterPanOriginal_.colors[static_cast<std::size_t>(sourceRow)];
    }
    return result;
}

void EditorProjectController::recordCharacterEdit(
    int setIndex, int patternIndex,
    const CharacterPattern& before, const CharacterPattern& after)
{
    if (sameCharacterPattern(before, after)) return;
    constexpr std::size_t maximumHistory = 128;
    if (characterUndoHistory_.size() >= maximumHistory) {
        characterUndoHistory_.erase(characterUndoHistory_.begin());
    }
    characterUndoHistory_.push_back({setIndex, patternIndex, before, after});
    characterRedoHistory_.clear();
}

void EditorProjectController::finishCharacterPan()
{
    if (!characterPanActive_) return;
    const CharacterPattern before = characterPanOriginal_;
    const CharacterPattern after = pannedCharacterPattern();
    characterPanActive_ = false;
    characterPanX_ = 0;
    characterPanY_ = 0;
    auto& pattern = characterSets_[static_cast<std::size_t>(characterPanSetIndex_)]
                        .patterns[static_cast<std::size_t>(
                            characterPanPatternIndex_)];
    pattern = after;
    recordCharacterEdit(characterPanSetIndex_, characterPanPatternIndex_,
                        before, after);
    ++characterRevision_;
    emit projectChanged();
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

EditorProjectController::CharacterSet
EditorProjectController::makeCharacterSet(int ordinal) const
{
    CharacterSet set;
    set.name = QStringLiteral("Set %1").arg(ordinal);
    for (auto& pattern : set.patterns) pattern.colors.fill(defaultCharacterColor);
    return set;
}

void EditorProjectController::setStatus(QString message, QString error)
{
    statusMessage_ = std::move(message);
    errorMessage_ = std::move(error);
    emit statusChanged();
}
