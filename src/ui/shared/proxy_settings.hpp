// src/ui/shared/proxy_settings.hpp — app-wide proxy (Tor/I2P) settings.
//
// Persisted in QSettings and applied to the core's global curl proxy
// (setGlobalProxy) so EVERY request — sync, media, login, well-known —
// goes through the proxy. Default type socks5h: the proxy resolves DNS
// itself (Tor), so no hostname ever leaks to the system resolver.
// Used in the login dialog AND the preferences dialog.

#pragma once

#include <QWidget>

#include <functional>
#include <string>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace progressive::desktop {

struct ProxyConfig;

// Read the persisted proxy settings (QSettings).
ProxyConfig proxyConfigFromSettings();
// Persist the settings (QSettings).
void saveProxyConfig(const ProxyConfig& cfg);
// Apply the persisted settings to the core's global proxy (no-op when disabled).
void applyProxyFromSettings();
// Persist + apply to the core's global proxy immediately.
void applyProxyConfig(const ProxyConfig& cfg);

// Widget: enable checkbox, type (socks5h/socks5/http), host, port, optional
// auth, a "Test connection" button and a live status/warning line. Every
// change is persisted AND applied immediately — the login flow itself (well-
// known discovery, login, registration) then already runs through the proxy.
class ProxySettingsWidget : public QWidget {
    Q_OBJECT
public:
    // testUrlFn: returns the server the user will connect to ("" → default
    // well-known test target). Called on each "Test connection".
    explicit ProxySettingsWidget(std::function<std::string()> testUrlFn = nullptr,
                                 QWidget* parent = nullptr);

    void applyNow();

signals:
    void proxyChanged();

private:
    void onAnythingChanged();
    void runTest();
    void updateLeakWarning();

    QCheckBox* enabledCheck_ = nullptr;
    QComboBox* typeCombo_ = nullptr;
    QLineEdit* hostEdit_ = nullptr;
    QSpinBox* portSpin_ = nullptr;
    QLineEdit* userEdit_ = nullptr;
    QLineEdit* passEdit_ = nullptr;
    QPushButton* testBtn_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QLabel* leakWarnLabel_ = nullptr;
    std::function<std::string()> testUrlFn_;
};

} // namespace progressive::desktop
