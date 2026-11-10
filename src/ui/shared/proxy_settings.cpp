// src/ui/shared/proxy_settings.cpp — app-wide proxy (Tor/I2P) settings.

#include "proxy_settings.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

#include "core/http_client.hpp"
#include "core/thread_pool.hpp"

#include <chrono>

namespace pcp::desktop {

// ---- persistence helpers ----

ProxyConfig proxyConfigFromSettings() {
    QSettings s;
    ProxyConfig cfg;
    cfg.enabled = s.value("proxy/enabled", false).toBool();
    cfg.host = s.value("proxy/host", "127.0.0.1").toString().toStdString();
    cfg.port = s.value("proxy/port", 9050).toInt();
    QString type = s.value("proxy/type", "socks5h").toString();
    if (type == "socks5") {
        cfg.type = ProxyConfig::Type::Socks5;
    } else if (type == "http") {
        cfg.type = ProxyConfig::Type::Http;
    } else {
        cfg.type = ProxyConfig::Type::Socks5Hostname;
    }
    cfg.username = s.value("proxy/user", "").toString().toStdString();
    cfg.password = s.value("proxy/pass", "").toString().toStdString();
    return cfg;
}

void saveProxyConfig(const ProxyConfig& cfg) {
    QSettings s;
    s.setValue("proxy/enabled", cfg.enabled);
    s.setValue("proxy/host", QString::fromStdString(cfg.host));
    s.setValue("proxy/port", cfg.port);
    QString type = "socks5h";
    if (cfg.type == ProxyConfig::Type::Socks5) type = "socks5";
    else if (cfg.type == ProxyConfig::Type::Http) type = "http";
    s.setValue("proxy/type", type);
    s.setValue("proxy/user", QString::fromStdString(cfg.username));
    s.setValue("proxy/pass", QString::fromStdString(cfg.password));
}

void applyProxyFromSettings() {
    setGlobalProxy(proxyConfigFromSettings());
}

void applyProxyConfig(const ProxyConfig& cfg) {
    saveProxyConfig(cfg);
    setGlobalProxy(cfg);
}

// ---- widget ----

static QString proxyTypeName(ProxyConfig::Type t) {
    switch (t) {
        case ProxyConfig::Type::Http:           return "HTTP";
        case ProxyConfig::Type::Socks5:         return "SOCKS5 (local DNS)";
        case ProxyConfig::Type::Socks5Hostname: return "SOCKS5 (Tor, DNS via proxy)";
    }
    return "SOCKS5 (Tor, DNS via proxy)";
}

static QString proxyTypeKey(ProxyConfig::Type t) {
    switch (t) {
        case ProxyConfig::Type::Http:           return "http";
        case ProxyConfig::Type::Socks5:         return "socks5";
        case ProxyConfig::Type::Socks5Hostname: return "socks5h";
    }
    return "socks5h";
}

ProxySettingsWidget::ProxySettingsWidget(std::function<std::string()> testUrlFn,
                                         QWidget* parent)
    : QWidget(parent), testUrlFn_(std::move(testUrlFn)) {

    auto* form = new QFormLayout(this);
    form->setContentsMargins(0, 0, 0, 0);

    enabledCheck_ = new QCheckBox("Use a proxy for all traffic", this);
    enabledCheck_->setToolTip("Routes EVERY network request (login, sync, "
        "media, keys) through the proxy. If the proxy is unreachable, "
        "requests fail — the app NEVER falls back to a direct connection.");
    form->addRow("", enabledCheck_);

    typeCombo_ = new QComboBox(this);
    typeCombo_->addItem("SOCKS5 (Tor, DNS via proxy)", "socks5h");
    typeCombo_->addItem("SOCKS5 (local DNS)", "socks5");
    typeCombo_->addItem("HTTP", "http");
    typeCombo_->setToolTip("socks5h (recommended for Tor): the proxy resolves "
        "DNS itself, so hostnames never touch your system resolver.\n"
        "socks5 resolves DNS locally — hostnames ARE visible to your "
        "system/ISP.\nhttp: plain HTTP proxy.");
    form->addRow("Type:", typeCombo_);

    hostEdit_ = new QLineEdit(this);
    hostEdit_->setPlaceholderText("127.0.0.1");
    hostEdit_->setToolTip("Proxy address. Tor usually listens on 127.0.0.1:9050.");
    portSpin_ = new QSpinBox(this);
    portSpin_->setRange(1, 65535);
    portSpin_->setValue(9050);
    auto* hostRow = new QHBoxLayout;
    hostRow->addWidget(hostEdit_, 1);
    hostRow->addWidget(portSpin_);
    form->addRow("Proxy:", hostRow);

    userEdit_ = new QLineEdit(this);
    userEdit_->setPlaceholderText("(optional)");
    passEdit_ = new QLineEdit(this);
    passEdit_->setEchoMode(QLineEdit::Password);
    passEdit_->setPlaceholderText("(optional)");
    auto* authRow = new QHBoxLayout;
    authRow->addWidget(userEdit_, 1);
    authRow->addWidget(passEdit_, 1);
    form->addRow("Auth:", authRow);

    leakWarnLabel_ = new QLabel(this);
    leakWarnLabel_->setWordWrap(true);
    leakWarnLabel_->setStyleSheet("color:#e0a030;");
    leakWarnLabel_->setVisible(false);
    form->addRow("", leakWarnLabel_);

    auto* testRow = new QHBoxLayout;
    testBtn_ = new QPushButton("Test connection", this);
    testBtn_->setToolTip("Performs one small HTTPS request THROUGH the proxy "
        "to confirm it works. DNS is resolved by the proxy (socks5h).");
    statusLabel_ = new QLabel(this);
    statusLabel_->setWordWrap(true);
    testRow->addWidget(testBtn_);
    testRow->addWidget(statusLabel_, 1);
    form->addRow("", testRow);

    ProxyConfig cfg = proxyConfigFromSettings();
    enabledCheck_->setChecked(cfg.enabled);
    int idx = typeCombo_->findData(proxyTypeKey(cfg.type));
    typeCombo_->setCurrentIndex(idx >= 0 ? idx : 0);
    hostEdit_->setText(QString::fromStdString(cfg.host));
    portSpin_->setValue(cfg.port);
    userEdit_->setText(QString::fromStdString(cfg.username));
    passEdit_->setText(QString::fromStdString(cfg.password));
    updateLeakWarning();

    connect(enabledCheck_, &QCheckBox::toggled, this,
            [this](bool) { onAnythingChanged(); });
    connect(typeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { onAnythingChanged(); });
    connect(hostEdit_, &QLineEdit::textChanged, this,
            [this](const QString&) { onAnythingChanged(); });
    connect(portSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int) { onAnythingChanged(); });
    connect(userEdit_, &QLineEdit::textChanged, this,
            [this](const QString&) { onAnythingChanged(); });
    connect(passEdit_, &QLineEdit::textChanged, this,
            [this](const QString&) { onAnythingChanged(); });
    connect(testBtn_, &QPushButton::clicked, this, [this]() { runTest(); });

    // First paint: show the current state (proxy already applied at startup).
    if (cfg.enabled) {
        statusLabel_->setText("Proxy enabled — press \"Test connection\" to verify it.");
    }
}

void ProxySettingsWidget::updateLeakWarning() {
    if (!leakWarnLabel_) return;
    QString type = typeCombo_->currentData().toString();
    if (type == "socks5") {
        leakWarnLabel_->setText("WARNING: \"SOCKS5 (local DNS)\" resolves "
            "hostnames on THIS machine — your system/ISP can see which "
            "servers you connect to. Use the Tor (socks5h) type to keep "
            "hostnames private.");
        leakWarnLabel_->setVisible(true);
    } else {
        leakWarnLabel_->setVisible(false);
    }
}

void ProxySettingsWidget::onAnythingChanged() {
    updateLeakWarning();
    applyNow();
    emit proxyChanged();
}

void ProxySettingsWidget::applyNow() {
    ProxyConfig cfg;
    cfg.enabled = enabledCheck_->isChecked();
    cfg.host = hostEdit_->text().trimmed().toStdString();
    cfg.port = portSpin_->value();
    QString type = typeCombo_->currentData().toString();
    if (type == "socks5") cfg.type = ProxyConfig::Type::Socks5;
    else if (type == "http") cfg.type = ProxyConfig::Type::Http;
    else cfg.type = ProxyConfig::Type::Socks5Hostname;
    cfg.username = userEdit_->text().toStdString();
    cfg.password = passEdit_->text().toStdString();
    applyProxyConfig(cfg);
}

void ProxySettingsWidget::runTest() {
    // Apply first — the test must go through the exact proxy the user set.
    applyNow();
    std::string server = testUrlFn_ ? testUrlFn_() : std::string();
    std::string url = server.empty()
        ? "https://matrix.org/.well-known/matrix/client"
        : "https://" + server + "/.well-known/matrix/client";
    statusLabel_->setText("Testing through the proxy...");
    auto self = QPointer<ProxySettingsWidget>(this);
    ThreadPool::instance().enqueue([self, url]() {
        auto t0 = std::chrono::steady_clock::now();
        // Public well-known probe, through the global proxy (applyNow above
        // already pushed the user's settings into the core — if the proxy is
        // down this request FAILS instead of falling back to direct).
        auto resp = pcp::desktop::httpGet(url, {}, 20000);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0).count();
        QMetaObject::invokeMethod(self, [self, url, resp, ms]() {
            if (!self) return;
            if (resp.success) {
                self->statusLabel_->setText(
                    QString("OK — %1 (%2 ms, HTTP %3)").arg(
                        QString::fromStdString(url), QString::number(ms),
                        QString::number(resp.statusCode)));
            } else {
                self->statusLabel_->setText(
                    QString("FAILED — %1: %2").arg(
                        QString::fromStdString(url),
                        QString::fromStdString(resp.errorMessage)));
            }
        }, Qt::QueuedConnection);
    });
}

} // namespace pcp::desktop
