#include "layoutcanvas.h"
#include <QtWidgets>
#include <cmath>

LayoutCanvas::LayoutCanvas(QMainWindow *w, const QList<QDockWidget *> &p, QWidget *parent)
    : QWidget(parent), window(w), panels(p) {
    setObjectName("layoutCanvas");
    setMinimumSize(340, 280);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    setAcceptDrops(true);
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName("Plan de disposition : glissez une vue ou un séparateur. Les flèches redimensionnent "
                      "la vue sélectionnée.");
}
QRectF LayoutCanvas::frame() const {
    const auto line=fontMetrics().height();return QRectF(16,line+16,width()-32,qMax(40,height()-2*line-36));
}
QList<QPair<QDockWidget *, QRectF>> LayoutCanvas::visibleRects() const {
    QList<QPair<QDockWidget *, QRectF>> result;
    const auto f = frame();
    const qreal sx = f.width() / qMax(1, window->width()), sy = f.height() / qMax(1, window->height());
    for (auto *panel : panels) {
        if (panel->isHidden() || panel->isFloating() || panel->visibleRegion().isEmpty())
            continue;
        auto origin = panel->mapTo(window, QPoint());
        result.append({panel, QRectF(f.left() + origin.x() * sx, f.top() + origin.y() * sy,
                                     panel->width() * sx, panel->height() * sy)});
    }
    return result;
}
QRectF LayoutCanvas::panelRect(const QString &id) const {
    for (const auto &entry : visibleRects())
        if (entry.first->objectName() == id)
            return entry.second;
    return {};
}
void LayoutCanvas::select(QDockWidget *panel) {
    selected = panel;
    update();
}
QList<LayoutCanvas::Splitter> LayoutCanvas::splitters() const {
    QList<Splitter> result;
    const auto rectangles = visibleRects();
    for (int i = 0; i < rectangles.size(); ++i)
        for (int j = i + 1; j < rectangles.size(); ++j) {
            auto a = rectangles[i], b = rectangles[j];
            const qreal y0 = qMax(a.second.top(), b.second.top()),
                        y1 = qMin(a.second.bottom(), b.second.bottom());
            if (y1 - y0 > 22 && (qAbs(a.second.right() - b.second.left()) < 8 ||
                                 qAbs(b.second.right() - a.second.left()) < 8)) {
                if (a.second.left() > b.second.left())
                    std::swap(a, b);
                qreal x = (a.second.right() + b.second.left()) / 2;
                result.append({a.first, b.first, Qt::Horizontal, QRectF(x - 5, y0 + 5, 10, y1 - y0 - 10)});
            }
            const qreal x0 = qMax(a.second.left(), b.second.left()),
                        x1 = qMin(a.second.right(), b.second.right());
            if (x1 - x0 > 22 && (qAbs(a.second.bottom() - b.second.top()) < 8 ||
                                 qAbs(b.second.bottom() - a.second.top()) < 8)) {
                if (a.second.top() > b.second.top())
                    std::swap(a, b);
                qreal y = (a.second.bottom() + b.second.top()) / 2;
                result.append({a.first, b.first, Qt::Vertical, QRectF(x0 + 5, y - 5, x1 - x0 - 10, 10)});
            }
        }
    return result;
}
void LayoutCanvas::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto f = frame();
    auto muted = palette().color(QPalette::PlaceholderText), accent = palette().color(QPalette::Highlight);
    p.setPen(muted);
    p.drawText(QRectF(18,3,width()-36,fontMetrics().height()+4),Qt::AlignLeft,p.fontMetrics().elidedText("Déplacer les panneaux",Qt::ElideRight,width()-36));
    p.setBrush(palette().color(QPalette::Base));
    p.setPen(palette().color(QPalette::Mid));
    p.drawRoundedRect(f, 14, 14);
    for (const auto &entry : visibleRects()) {
        auto r = entry.second.adjusted(3, 3, -3, -3);
        auto fill = palette().color(QPalette::Button);
        if (entry.first == selected)
            fill = fill.lighter(115);
        p.setBrush(fill);
        p.setPen(QPen(entry.first == selected ? accent : palette().color(QPalette::Mid),
                      entry.first == selected ? 1.5 : 1));
        p.drawRoundedRect(r, 9, 9);
        p.setPen(palette().color(QPalette::Text));
        p.drawText(r.adjusted(9, 9, -9, -9), Qt::AlignCenter,
                   p.fontMetrics().elidedText(entry.first->windowTitle(), Qt::ElideRight, qMax(0, int(r.width()) - 18)));
    }
    for (const auto &handle : splitters()) {
        p.setPen(Qt::NoPen);
        p.setBrush(muted);
        const auto c = handle.rect.center();
        if (handle.orientation == Qt::Horizontal)
            p.drawRoundedRect(QRectF(c.x() - 2, c.y() - 15, 4, 30), 2, 2);
        else
            p.drawRoundedRect(QRectF(c.x() - 15, c.y() - 2, 30, 4), 2, 2);
    }
    if (dragged && moved) {
        auto target = targetAt(pointer);
        if (target.first) {
            auto r = panelRect(target.first->objectName()).adjusted(5, 5, -5, -5);
            if (target.second == 0)
                r.setWidth(r.width() / 2);
            if (target.second == 1)
                r.setLeft(r.center().x());
            if (target.second == 2)
                r.setHeight(r.height() / 2);
            if (target.second == 3)
                r.setTop(r.center().y());
            if(target.second>=5){r=f.adjusted(5,5,-5,-5);if(target.second==5)r.setWidth(qMin(qreal(80),r.width()/4));if(target.second==6)r.setLeft(r.right()-qMin(qreal(80),r.width()/4));if(target.second==7)r.setHeight(qMin(qreal(50),r.height()/4));if(target.second==8)r.setTop(r.bottom()-qMin(qreal(50),r.height()/4));}
            auto shade = accent;
            shade.setAlpha(45);
            p.setBrush(shade);
            p.setPen(QPen(accent, 2));
            p.drawRoundedRect(r, 9, 9);
            p.setPen(palette().color(QPalette::Text));
            p.drawText(r, Qt::AlignCenter, target.second>=5?QStringList{"Gauche","Droite","Haut","Bas"}[target.second-5]:(target.second == 4 ? "Regrouper en onglet" : "Déplacer ici"));
        }
    }
    p.setPen(muted);
    const auto caption=selected ? selected->windowTitle()+" · "+QString::number(selected->width())+" × "+QString::number(selected->height())+" px" : QString("Glissez un panneau depuis la liste");
    p.drawText(QRectF(18,height()-fontMetrics().height()-8,width()-36,fontMetrics().height()+4),Qt::AlignLeft,p.fontMetrics().elidedText(caption,Qt::ElideRight,width()-36));
}
void LayoutCanvas::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton)
        return;
    setFocus();
    start = pointer = event->position();
    moved = false;
    before = window->saveState(2);
    handles = splitters();
    for (int i = 0; i < handles.size(); ++i)
        if (handles[i].rect.contains(start)) {
            splitter = i;
            const auto &h = handles[i];
            firstSize = h.orientation == Qt::Horizontal ? h.first->width() : h.first->height();
            secondSize = h.orientation == Qt::Horizontal ? h.second->width() : h.second->height();
            emit interactionStarted();
            return;
        }
    for (const auto &entry : visibleRects())
        if (entry.second.contains(start)) {
            selected = dragged = entry.first;
            emit selectedChanged(selected);
            update();
            return;
        }
}
void LayoutCanvas::mouseMoveEvent(QMouseEvent *event) {
    pointer = event->position();
    if (splitter >= 0) {
        const auto &h = handles[splitter];
        const auto f = frame();
        const int delta = qRound(h.orientation == Qt::Horizontal
                                     ? (pointer.x() - start.x()) * window->width() / f.width()
                                     : (pointer.y() - start.y()) * window->height() / f.height());
        const int total = firstSize + secondSize;
        const int minimumFirst =
                      h.orientation == Qt::Horizontal ? h.first->minimumWidth() : h.first->minimumHeight(),
                  minimumSecond =
                      h.orientation == Qt::Horizontal ? h.second->minimumWidth() : h.second->minimumHeight();
        const int value = qBound(qMax(80, minimumFirst), firstSize + delta,
                                 qMax(qMax(80, minimumFirst), total - qMax(80, minimumSecond)));
        window->resizeDocks({h.first, h.second}, {value, qMax(80, total - value)}, h.orientation);
        moved = true;
        update();
        return;
    }
    if (dragged && (event->buttons() & Qt::LeftButton)) {
        if ((pointer - start).manhattanLength() > QApplication::startDragDistance())
            moved = true;
        update();
        return;
    }
    for (const auto &h : splitters())
        if (h.rect.contains(pointer)) {
            setCursor(h.orientation == Qt::Horizontal ? Qt::SplitHCursor : Qt::SplitVCursor);
            return;
        }
    setCursor(Qt::OpenHandCursor);
}
QPair<QDockWidget *, int> LayoutCanvas::targetAt(const QPointF &point) const {
    const auto outer=frame();
    if(outer.contains(point)){
        int edge=-1;const qreal margin=18;
        if(point.x()-outer.left()<margin)edge=5;else if(outer.right()-point.x()<margin)edge=6;else if(point.y()-outer.top()<margin)edge=7;else if(outer.bottom()-point.y()<margin)edge=8;
        if(edge>=5){for(const auto &entry:visibleRects())if(entry.first!=dragged)return {entry.first,edge};if(dragged)return {dragged,edge};}
    }
    for (const auto &entry : visibleRects())
        if (entry.first != dragged && entry.second.contains(point)) {
            const auto r = entry.second;
            const qreal left = (point.x() - r.left()) / r.width(),
                        right = (r.right() - point.x()) / r.width(), top = (point.y() - r.top()) / r.height(),
                        bottom = (r.bottom() - point.y()) / r.height();
            const qreal smallest = qMin(qMin(left, right), qMin(top, bottom));
            int side = 4;
            if (smallest < .28) {
                if (smallest == left)
                    side = 0;
                else if (smallest == right)
                    side = 1;
                else if (smallest == top)
                    side = 2;
                else
                    side = 3;
            }
            return {entry.first, side};
        }
    return {nullptr, -1};
}
void LayoutCanvas::place(const QPointF &point) {
    if (!dragged)
        return;
    auto target = targetAt(point);
    if (!target.first)
        return;
    emit interactionStarted();
    auto *dock = dragged;
    auto *reference = target.first;
    if(target.second>=5){const Qt::DockWidgetArea areas[]={Qt::LeftDockWidgetArea,Qt::RightDockWidgetArea,Qt::TopDockWidgetArea,Qt::BottomDockWidgetArea};dock->setFloating(false);window->addDockWidget(areas[target.second-5],dock);dock->show();dock->raise();selected=dock;emit selectedChanged(dock);emit layoutChanged();return;}
    auto siblings = window->tabifiedDockWidgets(reference);
    dock->setFloating(false);
    window->addDockWidget(window->dockWidgetArea(reference), dock);
    dock->show();
    if (target.second == 4)
        window->tabifyDockWidget(reference, dock);
    else {
        auto orientation = target.second < 2 ? Qt::Horizontal : Qt::Vertical;
        window->splitDockWidget(reference, dock, orientation);
        if (target.second == 0 || target.second == 2) {
            window->splitDockWidget(dock, reference, orientation);
            for (auto *sibling : siblings)
                if (sibling != dock)
                    window->tabifyDockWidget(reference, sibling);
        }
    }
    dock->raise();
    selected = dock;
    emit selectedChanged(dock);
    emit layoutChanged();
}
void LayoutCanvas::mouseReleaseEvent(QMouseEvent *event) {
    if (splitter >= 0) {
        splitter = -1;
        if (moved)
            emit layoutChanged();
    } else if (dragged && moved)
        place(event->position());
    dragged = nullptr;
    moved = false;
    unsetCursor();
    update();
}
void LayoutCanvas::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        if (splitter >= 0 || dragged) {
            window->restoreState(before, 2);
            splitter = -1;
            dragged = nullptr;
            moved = false;
            update();
            emit layoutChanged();
        } else QWidget::keyPressEvent(event);
        return;
    }
    if (!selected) {
        QWidget::keyPressEvent(event);
        return;
    }
    const auto key = event->key();
    if (key != Qt::Key_Left && key != Qt::Key_Right && key != Qt::Key_Up && key != Qt::Key_Down) {
        QWidget::keyPressEvent(event);
        return;
    }
    emit interactionStarted();
    const int delta = (event->modifiers() & Qt::ShiftModifier ? 20 : 5) *
                      ((key == Qt::Key_Left || key == Qt::Key_Up) ? -1 : 1);
    const auto orientation = (key == Qt::Key_Left || key == Qt::Key_Right) ? Qt::Horizontal : Qt::Vertical;
    window->resizeDocks(
        {selected},
        {qMax(80, (orientation == Qt::Horizontal ? selected->width() : selected->height()) + delta)},
        orientation);
    update();
    emit layoutChanged();
}
void LayoutCanvas::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasFormat("application/x-github-enhanced-panel")) {
        const auto id = QString::fromUtf8(event->mimeData()->data("application/x-github-enhanced-panel"));
        for (auto *panel : panels)
            if (panel->objectName() == id) {
                dragged = panel;
                before = window->saveState(2);
                moved = true;
                event->acceptProposedAction();
                return;
            }
    }
}
void LayoutCanvas::dragMoveEvent(QDragMoveEvent *event) {
    pointer = event->position();
    event->acceptProposedAction();
    update();
}
void LayoutCanvas::dropEvent(QDropEvent *event) {
    place(event->position());
    dragged = nullptr;
    moved = false;
    event->acceptProposedAction();
    update();
}
void LayoutCanvas::leaveEvent(QEvent *) {
    if (splitter < 0)
        unsetCursor();
}
void LayoutCanvas::dragLeaveEvent(QDragLeaveEvent *) {dragged=nullptr;moved=false;update();}
void PanelList::startDrag(Qt::DropActions) {
    auto *item = currentItem();
    if (!item)
        return;
    auto *mime = new QMimeData;
    mime->setData("application/x-github-enhanced-panel", item->data(Qt::UserRole).toString().toUtf8());
    auto *drag = new QDrag(this);
    drag->setMimeData(mime);
    drag->exec(Qt::MoveAction);
}

void LayoutCanvas::setPanels(const QList<QDockWidget *> &items){panels=items;selected=nullptr;dragged=nullptr;splitter=-1;handles.clear();update();}
