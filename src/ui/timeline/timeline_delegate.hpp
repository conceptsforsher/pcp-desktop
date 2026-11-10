// src/ui/timeline_delegate.hpp — paints timeline events.
#pragma once
#include <QStyledItemDelegate>
#include <QHash>
#include <unordered_set>
#include <map>
#include <memory>

#include "timeline_layout.hpp"

namespace progressive::desktop {

class ImageLoader;

class TimelineDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit TimelineDelegate(ImageLoader* loader, QObject* parent = nullptr);

    void setMyUserId(const std::string& id) {
        QString q = QString::fromStdString(id);
        if (q != myUserId_) { myUserId_ = q; layoutCache_.clear(); }
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;
    bool editorEvent(QEvent* event, QAbstractItemModel* model,
                     const QStyleOptionViewItem& option,
                     const QModelIndex& index) override;

signals:
    void imageClicked(const QString& eventId, const QString& mxcUrl);
    void messageClicked(const QString& eventId);
    void linkClicked(const QString& url);
    void reactionClicked(const QString& eventId, const QString& emoji);
    void doubleClicked(const QString& eventId);
    void threadIndicatorClicked(const QString& eventId);

public:
    void invalidateLayoutCache() const { layoutCache_.clear(); }
    // Erase cached layouts for one event (all widths). Used for targeted
    // dataChanged handling — a full clear on every reaction/edit redo costs
    // every visible row its cached layout.
    void invalidateEvent(const QString& eventId) const;
private:
    // Layout cache key: "<contentWidth>|<eventId>". Keyed by event (NOT row:
    // rows shift on every prepend/insert) and by width (resize changes
    // wrapping). Rows without an event id (shouldn't reach the cache —
    // dividers/system rows return early) fall back to "#<row>".
    QString cacheKey(const QModelIndex& idx, int width) const;
    const timeline_layout::BubbleLayout& layoutFor(const QModelIndex& idx, int width) const;
    ImageLoader* loader_;
    QString myUserId_;
    mutable std::unordered_set<std::string> pendingFetches_;
    mutable std::map<QString, timeline_layout::BubbleLayout> layoutCache_;
    // Bound the cache: room switches keep old rooms' event ids alive until
    // the next reset; evict-all past this instead of growing forever.
    static constexpr int kLayoutCacheMax = 2048;
};

} // namespace progressive::desktop
