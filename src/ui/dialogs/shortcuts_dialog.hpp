// src/ui/dialogs/shortcuts_dialog.hpp — keyboard shortcuts reference.
#pragma once
#include <QDialog>

namespace pcp::desktop {

class ShortcutsDialog : public QDialog {
    Q_OBJECT
public:
    explicit ShortcutsDialog(QWidget* parent = nullptr);
};

} // namespace pcp::desktop
