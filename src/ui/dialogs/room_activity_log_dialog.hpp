// src/ui/dialogs/room_activity_log_dialog.hpp — "when did I open which room".
#pragma once

#include <QDialog>

class QTextEdit;

namespace pcp::desktop {

class RoomActivityLogDialog : public QDialog {
    Q_OBJECT
public:
    explicit RoomActivityLogDialog(QWidget* parent = nullptr);
private:
    QTextEdit* view_ = nullptr;
};

} // namespace pcp::desktop
