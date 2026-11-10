#pragma once
#include <QDialog>
#include <QString>
#include <string>

class QLineEdit;
class QListWidget;

namespace pcp::desktop {

class RoomListModel;

class RoomSwitcherDialog : public QDialog {
    Q_OBJECT
public:
    explicit RoomSwitcherDialog(RoomListModel* roomModel, QWidget* parent = nullptr);

signals:
    void roomSelected(const QString& roomId);

private:
    void filterRooms(const QString& text);
    bool eventFilter(QObject* obj, QEvent* event) override;

    RoomListModel* roomModel_;
    QLineEdit* filterEdit_;
    QListWidget* roomList_;
};

} // namespace pcp::desktop
