#include "EditorProjectController.hpp"

#include <QClipboard>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <algorithm>

namespace {

int normalizedSpriteSize(int value)
{
    return value >= 16 ? 16 : 8;
}

bool sameSpritePattern(const auto& left, const auto& right)
{
    return left.baselinePixels == right.baselinePixels
        && left.f18aPixels == right.f18aPixels
        && left.f18aOverride == right.f18aOverride;
}

} // namespace

int EditorProjectController::activeSpriteColorDepth() const
{
    if (spriteSets_.empty()) return 1;
    const auto& placement = spriteSets_[static_cast<std::size_t>(activeSpriteSet_)]
                                .placements[static_cast<std::size_t>(activeSprite_)];
    return editScope_ == 1 ? placement.colorDepth : 1;
}

bool EditorProjectController::canPasteSpritePattern() const
{
    const auto data = spritePatternFromClipboard();
    if (!data.has_value() || data->size != activeSpriteSize_) return false;
    const int maximum = editScope_ == 1
        ? (1 << activeSpriteColorDepth()) - 1 : 1;
    const int count = data->size * data->size;
    for (int index = 0; index < count; ++index) {
        if (data->pixels[static_cast<std::size_t>(index)] > maximum) return false;
    }
    return true;
}

void EditorProjectController::selectSpritePattern(int spriteIndex, int size)
{
    if (spriteSets_.empty()) return;
    spriteIndex = std::clamp(spriteIndex, 0, 31);
    size = normalizedSpriteSize(size);
    if (spritePanActive_
        && (spriteIndex != activeSprite_ || size != activeSpriteSize_)) {
        finishSpritePan();
    }

    bool changed = activeSprite_ != spriteIndex || activeSpriteSize_ != size;
    activeSprite_ = spriteIndex;
    activeSpriteSize_ = size;
    if (editScope_ == 0) {
        if (spriteGlobalSize_ != size) {
            spriteGlobalSize_ = size;
            changed = true;
        }
    } else {
        auto& placement = spriteSets_[static_cast<std::size_t>(activeSpriteSet_)]
                              .placements[static_cast<std::size_t>(activeSprite_)];
        if (placement.size != size) {
            placement.size = size;
            changed = true;
        }
    }
    changed = assignActiveSpriteToEditor() || changed;
    syncSpriteDrawingColor();
    if (changed) emit projectChanged();
}

void EditorProjectController::setActiveSpriteSize(int value)
{
    value = normalizedSpriteSize(value);
    if (spritePanActive_ && value != activeSpriteSize_) finishSpritePan();
    bool changed = activeSpriteSize_ != value;
    activeSpriteSize_ = value;
    if (editScope_ == 0) {
        if (spriteGlobalSize_ != value) {
            spriteGlobalSize_ = value;
            changed = true;
        }
    } else if (!spriteSets_.empty()) {
        auto& placement = spriteSets_[static_cast<std::size_t>(activeSpriteSet_)]
                              .placements[static_cast<std::size_t>(activeSprite_)];
        if (placement.size != value) {
            placement.size = value;
            changed = true;
        }
    }
    changed = assignActiveSpriteToEditor() || changed;
    if (changed) emit projectChanged();
}

void EditorProjectController::setSpriteGlobalSize(int value)
{
    value = normalizedSpriteSize(value);
    if (spriteGlobalSize_ == value) return;
    if (spritePanActive_) finishSpritePan();
    spriteGlobalSize_ = value;
    if (editScope_ == 0) {
        activeSpriteSize_ = value;
        activateSpriteEditorBank(value);
        syncSpriteDrawingColor();
    }
    emit projectChanged();
}

void EditorProjectController::setSpriteDrawingColorIndex(int value)
{
    if (spriteSets_.empty()) return;
    auto& placement = spriteSets_[static_cast<std::size_t>(activeSpriteSet_)]
                          .placements[static_cast<std::size_t>(activeSprite_)];
    const int maximum = editScope_ == 1 ? (1 << placement.colorDepth) - 1 : 15;
    value = std::clamp(value, 1, maximum);
    bool changed = spriteDrawingColorIndex_ != value;
    spriteDrawingColorIndex_ = value;
    if (editScope_ == 0 && placement.color != value) {
        placement.color = value;
        changed = true;
    }
    if (changed) emit projectChanged();
}

void EditorProjectController::setActiveSpriteColorDepth(int value)
{
    if (editScope_ != 1 || spriteSets_.empty()) return;
    value = std::clamp(value, 1, 3);
    auto& placement = spriteSets_[static_cast<std::size_t>(activeSpriteSet_)]
                          .placements[static_cast<std::size_t>(activeSprite_)];
    if (placement.colorDepth == value) return;
    placement.colorDepth = value;
    syncSpriteDrawingColor();
    emit projectChanged();
}

void EditorProjectController::syncSpriteDrawingColor()
{
    if (spriteSets_.empty()) {
        spriteDrawingColorIndex_ = 15;
        return;
    }
    const auto& placement = spriteSets_[static_cast<std::size_t>(activeSpriteSet_)]
                                .placements[static_cast<std::size_t>(activeSprite_)];
    if (editScope_ == 0) {
        spriteDrawingColorIndex_ = placement.color;
        return;
    }
    spriteDrawingColorIndex_ = std::clamp(
        spriteDrawingColorIndex_, 1, (1 << placement.colorDepth) - 1);
}

bool EditorProjectController::activateSpriteEditorBank(int size)
{
    size = normalizedSpriteSize(size);
    if (activeSpriteEditor_ >= 0
        && activeSpriteEditor_ < static_cast<int>(spriteEditorSlots_.size())
        && spriteEditorSlots_[static_cast<std::size_t>(activeSpriteEditor_)].size
            == size) {
        return false;
    }

    int matchingIndex = -1;
    for (int index = 0; index < static_cast<int>(spriteEditorSlots_.size()); ++index) {
        const auto& slot = spriteEditorSlots_[static_cast<std::size_t>(index)];
        if (slot.size != size) continue;
        matchingIndex = index;
        if (slot.loaded) break;
    }
    if (matchingIndex < 0) {
        constexpr std::size_t maximumEditors = 32;
        if (spriteEditorSlots_.size() >= maximumEditors) {
            setStatus({}, QStringLiteral("A sprite tray can contain up to 32 unique sprite editors."));
            return false;
        }
        spriteEditorSlots_.push_back(
            {false, activeSpriteSet_, activeSprite_, size});
        matchingIndex = static_cast<int>(spriteEditorSlots_.size()) - 1;
    }

    activeSpriteEditor_ = matchingIndex;
    activeSpriteSize_ = size;
    const auto& slot = spriteEditorSlots_[static_cast<std::size_t>(matchingIndex)];
    if (slot.loaded) {
        activeSpriteSet_ = slot.setIndex;
        activeSprite_ = slot.spriteIndex;
    }
    return true;
}

bool EditorProjectController::assignActiveSpriteToEditor()
{
    if (spriteEditorSlots_.empty()) {
        spriteEditorSlots_.push_back(
            {false, activeSpriteSet_, activeSprite_, activeSpriteSize_});
        activeSpriteEditor_ = 0;
    }
    for (int index = 0; index < static_cast<int>(spriteEditorSlots_.size()); ++index) {
        const auto& slot = spriteEditorSlots_[static_cast<std::size_t>(index)];
        if (slot.loaded && slot.setIndex == activeSpriteSet_
            && slot.spriteIndex == activeSprite_
            && slot.size == activeSpriteSize_) {
            activeSpriteEditor_ = index;
            return true;
        }
    }

    int targetIndex = -1;
    if (activeSpriteEditor_ >= 0
        && activeSpriteEditor_ < static_cast<int>(spriteEditorSlots_.size())) {
        const auto& activeEditor = spriteEditorSlots_[
            static_cast<std::size_t>(activeSpriteEditor_)];
        if (!activeEditor.loaded || activeEditor.size == activeSpriteSize_) {
            targetIndex = activeSpriteEditor_;
        }
    } else {
        for (int index = 0; index < static_cast<int>(spriteEditorSlots_.size()); ++index) {
            const auto& candidate = spriteEditorSlots_[static_cast<std::size_t>(index)];
            if (!candidate.loaded && candidate.size == activeSpriteSize_) {
                targetIndex = index;
                break;
            }
        }
    }
    if (targetIndex < 0) {
        constexpr std::size_t maximumEditors = 32;
        if (spriteEditorSlots_.size() >= maximumEditors) return false;
        spriteEditorSlots_.push_back(
            {false, activeSpriteSet_, activeSprite_, activeSpriteSize_});
        targetIndex = static_cast<int>(spriteEditorSlots_.size()) - 1;
    }
    activeSpriteEditor_ = targetIndex;
    auto& slot = spriteEditorSlots_[static_cast<std::size_t>(targetIndex)];
    slot.loaded = true;
    slot.setIndex = activeSpriteSet_;
    slot.spriteIndex = activeSprite_;
    slot.size = activeSpriteSize_;
    return true;
}

void EditorProjectController::setSpritePanActive(bool value)
{
    if (spritePanActive_ == value) return;
    if (!value) {
        finishSpritePan();
        return;
    }
    if (spritePlacementMode_ || spriteSets_.empty()) return;
    endSpriteEdit();
    spritePanSetIndex_ = activeSpriteSet_;
    spritePanSpriteIndex_ = activeSprite_;
    spritePanSize_ = activeSpriteSize_;
    spritePanEnhanced_ = editScope_ == 1;
    spritePanOriginal_ = spritePattern(activeSpriteSet_, activeSprite_, activeSpriteSize_);
    spritePanX_ = 0;
    spritePanY_ = 0;
    spritePanActive_ = true;
    ++spriteRevision_;
    emit projectChanged();
}

QVariantList EditorProjectController::spritePatternPixels(int setIndex,
                                                          int spriteIndex,
                                                          int size) const
{
    QVariantList result;
    size = normalizedSpriteSize(size);
    if (setIndex < 0 || setIndex >= static_cast<int>(spriteSets_.size())
        || spriteIndex < 0 || spriteIndex >= 32) {
        return result;
    }
    std::array<std::uint8_t, 256> panPixels{};
    const std::array<std::uint8_t, 256>* pixels =
        &visibleSpritePixels(spritePattern(setIndex, spriteIndex, size));
    if (spritePanActive_ && setIndex == spritePanSetIndex_
        && spriteIndex == spritePanSpriteIndex_ && size == spritePanSize_) {
        panPixels = pannedSpritePixels();
        pixels = &panPixels;
    }
    const int count = size * size;
    result.reserve(count);
    for (int index = 0; index < count; ++index) {
        result.push_back((*pixels)[static_cast<std::size_t>(index)]);
    }
    return result;
}

void EditorProjectController::paintSpritePixel(int setIndex,
                                               int spriteIndex,
                                               int size,
                                               int row,
                                               int column,
                                               bool foreground)
{
    size = normalizedSpriteSize(size);
    if (setIndex < 0 || setIndex >= static_cast<int>(spriteSets_.size())
        || spriteIndex < 0 || spriteIndex >= 32
        || row < 0 || row >= size || column < 0 || column >= size
        || spritePanActive_) {
        return;
    }
    if (spriteEditActive_
        && (spriteEditSetIndex_ != setIndex
            || spriteEditSpriteIndex_ != spriteIndex
            || spriteEditSize_ != size)) {
        endSpriteEdit();
    }
    const bool standalone = !spriteEditActive_;
    if (standalone) beginSpriteEdit(setIndex, spriteIndex, size);
    auto& pattern = spritePattern(setIndex, spriteIndex, size);
    std::array<std::uint8_t, 256>* pixels = &pattern.baselinePixels;
    int value = foreground ? 1 : 0;
    if (editScope_ == 1) {
        ensureF18aSpriteOverride(pattern);
        pixels = &pattern.f18aPixels;
        value = foreground ? spriteDrawingColorIndex_ : 0;
    }
    auto& pixel = (*pixels)[static_cast<std::size_t>(row * size + column)];
    if (pixel != value) {
        pixel = static_cast<std::uint8_t>(value);
        ++spriteRevision_;
        emit projectChanged();
    }
    if (standalone) endSpriteEdit();
}

void EditorProjectController::beginSpriteEdit(int setIndex, int spriteIndex, int size)
{
    size = normalizedSpriteSize(size);
    if (setIndex < 0 || setIndex >= static_cast<int>(spriteSets_.size())
        || spriteIndex < 0 || spriteIndex >= 32 || spritePanActive_) {
        return;
    }
    if (spriteEditActive_) {
        if (spriteEditSetIndex_ == setIndex
            && spriteEditSpriteIndex_ == spriteIndex
            && spriteEditSize_ == size) {
            return;
        }
        endSpriteEdit();
    }
    spriteEditActive_ = true;
    spriteEditSetIndex_ = setIndex;
    spriteEditSpriteIndex_ = spriteIndex;
    spriteEditSize_ = size;
    spriteEditBefore_ = spritePattern(setIndex, spriteIndex, size);
}

void EditorProjectController::endSpriteEdit()
{
    if (!spriteEditActive_) return;
    const int setIndex = spriteEditSetIndex_;
    const int spriteIndex = spriteEditSpriteIndex_;
    const int size = spriteEditSize_;
    spriteEditActive_ = false;
    recordSpriteEdit(setIndex, spriteIndex, size, spriteEditBefore_,
                     spritePattern(setIndex, spriteIndex, size));
    emit projectChanged();
}

void EditorProjectController::rotateActiveSpritePattern()
{
    if (spritePanActive_) finishSpritePan();
    endSpriteEdit();
    auto& pattern = spritePattern(activeSpriteSet_, activeSprite_, activeSpriteSize_);
    const SpritePattern before = pattern;
    if (editScope_ == 1) ensureF18aSpriteOverride(pattern);
    auto& pixels = editScope_ == 1 ? pattern.f18aPixels : pattern.baselinePixels;
    const auto source = pixels;
    const int size = activeSpriteSize_;
    for (int row = 0; row < size; ++row) {
        for (int column = 0; column < size; ++column) {
            pixels[static_cast<std::size_t>(column * size + (size - 1 - row))] =
                source[static_cast<std::size_t>(row * size + column)];
        }
    }
    recordSpriteEdit(activeSpriteSet_, activeSprite_, size, before, pattern);
    if (sameSpritePattern(before, pattern)) return;
    ++spriteRevision_;
    emit projectChanged();
}

void EditorProjectController::mirrorActiveSpritePattern()
{
    if (spritePanActive_) finishSpritePan();
    endSpriteEdit();
    auto& pattern = spritePattern(activeSpriteSet_, activeSprite_, activeSpriteSize_);
    const SpritePattern before = pattern;
    if (editScope_ == 1) ensureF18aSpriteOverride(pattern);
    auto& pixels = editScope_ == 1 ? pattern.f18aPixels : pattern.baselinePixels;
    const int size = activeSpriteSize_;
    for (int row = 0; row < size; ++row) {
        for (int column = 0; column < size / 2; ++column) {
            std::swap(pixels[static_cast<std::size_t>(row * size + column)],
                      pixels[static_cast<std::size_t>(row * size + size - 1 - column)]);
        }
    }
    recordSpriteEdit(activeSpriteSet_, activeSprite_, size, before, pattern);
    if (sameSpritePattern(before, pattern)) return;
    ++spriteRevision_;
    emit projectChanged();
}

void EditorProjectController::flipActiveSpritePattern()
{
    if (spritePanActive_) finishSpritePan();
    endSpriteEdit();
    auto& pattern = spritePattern(activeSpriteSet_, activeSprite_, activeSpriteSize_);
    const SpritePattern before = pattern;
    if (editScope_ == 1) ensureF18aSpriteOverride(pattern);
    auto& pixels = editScope_ == 1 ? pattern.f18aPixels : pattern.baselinePixels;
    const int size = activeSpriteSize_;
    for (int row = 0; row < size / 2; ++row) {
        for (int column = 0; column < size; ++column) {
            std::swap(pixels[static_cast<std::size_t>(row * size + column)],
                      pixels[static_cast<std::size_t>((size - 1 - row) * size + column)]);
        }
    }
    recordSpriteEdit(activeSpriteSet_, activeSprite_, size, before, pattern);
    if (sameSpritePattern(before, pattern)) return;
    ++spriteRevision_;
    emit projectChanged();
}

void EditorProjectController::blankActiveSpritePattern()
{
    if (spritePanActive_) finishSpritePan();
    endSpriteEdit();
    auto& pattern = spritePattern(activeSpriteSet_, activeSprite_, activeSpriteSize_);
    const SpritePattern before = pattern;
    if (editScope_ == 1) ensureF18aSpriteOverride(pattern);
    auto& pixels = editScope_ == 1 ? pattern.f18aPixels : pattern.baselinePixels;
    const int count = activeSpriteSize_ * activeSpriteSize_;
    std::fill_n(pixels.begin(), count, 0);
    recordSpriteEdit(activeSpriteSet_, activeSprite_, activeSpriteSize_, before, pattern);
    if (sameSpritePattern(before, pattern)) return;
    ++spriteRevision_;
    emit projectChanged();
}

void EditorProjectController::nudgeSpritePan(int horizontal, int vertical)
{
    if (!spritePanActive_) return;
    const int nextX = std::clamp(spritePanX_ + horizontal, -spritePanSize_, spritePanSize_);
    const int nextY = std::clamp(spritePanY_ + vertical, -spritePanSize_, spritePanSize_);
    if (nextX == spritePanX_ && nextY == spritePanY_) return;
    spritePanX_ = nextX;
    spritePanY_ = nextY;
    ++spriteRevision_;
    emit projectChanged();
}

void EditorProjectController::centerSpritePan()
{
    if (!spritePanActive_ || (spritePanX_ == 0 && spritePanY_ == 0)) return;
    spritePanX_ = 0;
    spritePanY_ = 0;
    ++spriteRevision_;
    emit projectChanged();
}

void EditorProjectController::undoSpriteEdit()
{
    if (spritePanActive_ || spriteUndoHistory_.empty()) return;
    endSpriteEdit();
    const SpriteHistoryEntry entry = spriteUndoHistory_.back();
    spriteUndoHistory_.pop_back();
    spritePattern(entry.setIndex, entry.spriteIndex, entry.size) = entry.before;
    spriteRedoHistory_.push_back(entry);
    ++spriteRevision_;
    emit projectChanged();
}

void EditorProjectController::redoSpriteEdit()
{
    if (spritePanActive_ || spriteRedoHistory_.empty()) return;
    endSpriteEdit();
    const SpriteHistoryEntry entry = spriteRedoHistory_.back();
    spriteRedoHistory_.pop_back();
    spritePattern(entry.setIndex, entry.spriteIndex, entry.size) = entry.after;
    spriteUndoHistory_.push_back(entry);
    ++spriteRevision_;
    emit projectChanged();
}

bool EditorProjectController::copyActiveSpritePattern()
{
    if (spritePanActive_) return false;
    endSpriteEdit();
    const auto& pattern = spritePattern(activeSpriteSet_, activeSprite_, activeSpriteSize_);
    const auto& pixels = visibleSpritePixels(pattern);
    QJsonArray rows;
    for (int row = 0; row < activeSpriteSize_; ++row) {
        QJsonArray values;
        for (int column = 0; column < activeSpriteSize_; ++column) {
            values.push_back(pixels[static_cast<std::size_t>(
                row * activeSpriteSize_ + column)]);
        }
        rows.push_back(values);
    }
    const QJsonObject root{
        {QStringLiteral("format"), QStringLiteral("newconvert9918.sprite-pattern")},
        {QStringLiteral("version"), 1},
        {QStringLiteral("size"), activeSpriteSize_},
        {QStringLiteral("scope"), editScope_ == 1
                                      ? QStringLiteral("f18a-override")
                                      : QStringLiteral("tms9918a-baseline")},
        {QStringLiteral("source"),
         QJsonObject{{QStringLiteral("set"), activeSpriteSet_},
                     {QStringLiteral("sprite"), activeSprite_}}},
        {QStringLiteral("colorDepth"), activeSpriteColorDepth()},
        {QStringLiteral("pixels"), rows},
    };
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard == nullptr) return false;
    clipboard->setText(QString::fromUtf8(QJsonDocument(root).toJson(
        QJsonDocument::Indented)));
    setStatus(QStringLiteral("Copied %1x%1 sprite %2 to the clipboard.")
                  .arg(activeSpriteSize_)
                  .arg(activeSprite_));
    return true;
}

bool EditorProjectController::pasteActiveSpritePattern()
{
    if (spritePanActive_) return false;
    endSpriteEdit();
    const auto data = spritePatternFromClipboard();
    if (!data.has_value() || data->size != activeSpriteSize_) {
        setStatus({}, QStringLiteral("Clipboard sprite size does not match the active editor."));
        return false;
    }
    const int maximum = editScope_ == 1
        ? (1 << activeSpriteColorDepth()) - 1 : 1;
    const int count = data->size * data->size;
    for (int index = 0; index < count; ++index) {
        if (data->pixels[static_cast<std::size_t>(index)] > maximum) {
            setStatus({}, editScope_ == 0
                ? QStringLiteral("Enhanced-color sprite pixels cannot be pasted into the TMS9918A baseline.")
                : QStringLiteral("Clipboard pixel indexes exceed the active F18A color depth."));
            return false;
        }
    }
    auto& pattern = spritePattern(activeSpriteSet_, activeSprite_, activeSpriteSize_);
    const SpritePattern before = pattern;
    if (editScope_ == 1) {
        ensureF18aSpriteOverride(pattern);
        pattern.f18aPixels = data->pixels;
    } else {
        pattern.baselinePixels = data->pixels;
    }
    recordSpriteEdit(activeSpriteSet_, activeSprite_, activeSpriteSize_, before, pattern);
    if (!sameSpritePattern(before, pattern)) {
        ++spriteRevision_;
        emit projectChanged();
    }
    setStatus(QStringLiteral("Pasted clipboard data into sprite %1.").arg(activeSprite_));
    return true;
}

EditorProjectController::SpritePattern&
EditorProjectController::spritePattern(int setIndex, int spriteIndex, int size)
{
    auto& set = spriteSets_[static_cast<std::size_t>(setIndex)];
    return normalizedSpriteSize(size) == 16
        ? set.patterns16[static_cast<std::size_t>(spriteIndex)]
        : set.patterns8[static_cast<std::size_t>(spriteIndex)];
}

const EditorProjectController::SpritePattern&
EditorProjectController::spritePattern(int setIndex, int spriteIndex, int size) const
{
    const auto& set = spriteSets_[static_cast<std::size_t>(setIndex)];
    return normalizedSpriteSize(size) == 16
        ? set.patterns16[static_cast<std::size_t>(spriteIndex)]
        : set.patterns8[static_cast<std::size_t>(spriteIndex)];
}

const std::array<std::uint8_t, 256>&
EditorProjectController::visibleSpritePixels(const SpritePattern& pattern) const
{
    return editScope_ == 1 && pattern.f18aOverride
        ? pattern.f18aPixels : pattern.baselinePixels;
}

std::array<std::uint8_t, 256> EditorProjectController::pannedSpritePixels() const
{
    const auto& source = spritePanEnhanced_ && spritePanOriginal_.f18aOverride
        ? spritePanOriginal_.f18aPixels : spritePanOriginal_.baselinePixels;
    std::array<std::uint8_t, 256> result{};
    for (int row = 0; row < spritePanSize_; ++row) {
        const int sourceRow = row - spritePanY_;
        if (sourceRow < 0 || sourceRow >= spritePanSize_) continue;
        for (int column = 0; column < spritePanSize_; ++column) {
            const int sourceColumn = column - spritePanX_;
            if (sourceColumn < 0 || sourceColumn >= spritePanSize_) continue;
            result[static_cast<std::size_t>(row * spritePanSize_ + column)] =
                source[static_cast<std::size_t>(
                    sourceRow * spritePanSize_ + sourceColumn)];
        }
    }
    return result;
}

std::optional<EditorProjectController::SpriteClipboardData>
EditorProjectController::spritePatternFromClipboard() const
{
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard == nullptr) return std::nullopt;
    const QString text = clipboard->text();
    if (text.isEmpty() || text.size() > 262144) return std::nullopt;
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(text.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return std::nullopt;
    }
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("format")).toString()
            != QStringLiteral("newconvert9918.sprite-pattern")
        || root.value(QStringLiteral("version")).toInt() != 1) {
        return std::nullopt;
    }
    SpriteClipboardData data;
    data.size = normalizedSpriteSize(root.value(QStringLiteral("size")).toInt());
    if (root.value(QStringLiteral("size")).toInt() != data.size) return std::nullopt;
    data.enhanced = root.value(QStringLiteral("scope")).toString()
        == QStringLiteral("f18a-override");
    data.colorDepth = std::clamp(
        root.value(QStringLiteral("colorDepth")).toInt(1), 1, 3);
    const QJsonArray rows = root.value(QStringLiteral("pixels")).toArray();
    if (rows.size() != data.size) return std::nullopt;
    for (int row = 0; row < data.size; ++row) {
        const QJsonArray values = rows[row].toArray();
        if (values.size() != data.size) return std::nullopt;
        for (int column = 0; column < data.size; ++column) {
            const int value = values[column].toInt(-1);
            if (value < 0 || value > (1 << data.colorDepth) - 1) {
                return std::nullopt;
            }
            data.pixels[static_cast<std::size_t>(row * data.size + column)] =
                static_cast<std::uint8_t>(value);
        }
    }
    return data;
}

void EditorProjectController::ensureF18aSpriteOverride(SpritePattern& pattern)
{
    if (pattern.f18aOverride) return;
    pattern.f18aPixels = pattern.baselinePixels;
    pattern.f18aOverride = true;
}

void EditorProjectController::recordSpriteEdit(int setIndex, int spriteIndex, int size,
                                               const SpritePattern& before,
                                               const SpritePattern& after)
{
    if (sameSpritePattern(before, after)) return;
    constexpr std::size_t maximumHistory = 128;
    if (spriteUndoHistory_.size() >= maximumHistory) {
        spriteUndoHistory_.erase(spriteUndoHistory_.begin());
    }
    spriteUndoHistory_.push_back({setIndex, spriteIndex, size, before, after});
    spriteRedoHistory_.clear();
}

void EditorProjectController::finishSpritePan()
{
    if (!spritePanActive_) return;
    const auto shifted = pannedSpritePixels();
    const SpritePattern before = spritePanOriginal_;
    spritePanActive_ = false;
    spritePanX_ = 0;
    spritePanY_ = 0;
    auto& pattern = spritePattern(spritePanSetIndex_, spritePanSpriteIndex_,
                                  spritePanSize_);
    pattern = before;
    if (spritePanEnhanced_) {
        ensureF18aSpriteOverride(pattern);
        pattern.f18aPixels = shifted;
    } else {
        pattern.baselinePixels = shifted;
    }
    recordSpriteEdit(spritePanSetIndex_, spritePanSpriteIndex_, spritePanSize_,
                     before, pattern);
    ++spriteRevision_;
    emit projectChanged();
}
