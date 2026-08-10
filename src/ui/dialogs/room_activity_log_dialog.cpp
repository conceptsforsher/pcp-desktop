// src/ui/dialogs/room_activity_log_dialog.cpp — shows the room open log.

#include "room_activity_log_dialog.hpp"
#include "../shared/room_activity_log.hpp"

#include <QHBoxLayout>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

#include <ctime>
#include <cstdio>

namespace progressive::desktop {

RoomActivityLogDialog::RoomActivityLogDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Room activity log");
    setMinimumSize(480, 320);

    view_ = new QTextEdit(this);
    view_->setReadOnly(true);
    view_->setStyleSheet("QTextEdit{background:#101318; color:#c8d0dc; font-family:monospace;}");

    auto entries = roomOpenLogSnapshot();
    QString text;
    for (const auto& e : entries) {
        std::time_t t = static_cast<std::time_t>(e.ts / 1000);
        std::tm tm{};
        localtime_r(&t, &tm);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                      tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                      tm.tm_hour, tm.tm_min, tm.tm_sec);
        text += QString("%1  %2  (%3)\n")
            .arg(buf)
            .arg(QString::fromStdString(e.roomName))
            .arg(QString::fromStdString(e.roomId));
    }
    if (text.isEmpty()) {
        text = "(no rooms opened yet in this session)\n";
    }
    view_->setPlainText(text);

    auto* closeBtn = new QPushButton("Close", this);
    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);

    auto* root = new QVBoxLayout(this);
    root->addWidget(view_, 1);
    root->addLayout(btnRow);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
}

} // namespace progressive::desktop
