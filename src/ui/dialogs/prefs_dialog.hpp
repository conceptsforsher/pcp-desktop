// src/ui/prefs_dialog.hpp — user preferences dialog.
#pragma once
#include <QDialog>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QSettings>
#include <functional>
#include <memory>

namespace pcp::desktop { class SessionStore; }

class QVBoxLayout;

namespace pcp::desktop {

class MatrixClient;
class VerificationHandler;
class Decryptor;

class ProxySettingsWidget;

class PrefsDialog : public QDialog {
    Q_OBJECT
public:
    explicit PrefsDialog(QWidget* parent = nullptr);

    void setClient(std::shared_ptr<MatrixClient> c) {
        client_ = std::move(c);
        // The devices section is built in the constructor BEFORE setClient —
        // reload it now that the client exists (the "Not logged in." bug).
        if (devicesSection_) loadDevices(devicesSection_);
        refreshAccountSection();
    }
    void setSessionStore(SessionStore* s) { store_ = s; }
    void setVerificationHandler(VerificationHandler* vh) { verifyHandler_ = vh; }
    void setDecryptor(Decryptor* d) { decryptor_ = d; }
    using SetupCrossSigningFn = std::function<bool()>;
    using SetupCrossSigningWithPasswordFn = std::function<bool(const std::string&)>;
    using UiaSessionFn = std::function<std::string()>;
    using ResetCrossSigningFn = std::function<bool()>;
    using CreateKeyBackupFn = std::function<std::string()>;          // returns the recovery key
    using UploadKeyBackupFn = std::function<bool()>;
    using DeleteKeyBackupFn = std::function<bool()>;
    using RestoreKeyBackupFn = std::function<int(const std::string&)>;
    using SsssFn = std::function<bool(const std::string&)>;
    using ResetDeviceKeysFn = std::function<void()>;
    void setSetupCrossSigningFn(SetupCrossSigningFn fn) { setupCrossSigningFn_ = std::move(fn); }
    void setResetCrossSigningFn(ResetCrossSigningFn fn) { resetCrossSigningFn_ = std::move(fn); }
    void setCreateKeyBackupFn(CreateKeyBackupFn fn) { createKeyBackupFn_ = std::move(fn); }
    void setUploadKeyBackupFn(UploadKeyBackupFn fn) { uploadKeyBackupFn_ = std::move(fn); }
    void setDeleteKeyBackupFn(DeleteKeyBackupFn fn) { deleteKeyBackupFn_ = std::move(fn); }
    void setRestoreKeyBackupFn(RestoreKeyBackupFn fn) { restoreKeyBackupFn_ = std::move(fn); }
    void setUploadSsssFn(SsssFn fn) { uploadSsssFn_ = std::move(fn); }
    void setRetrieveSsssFn(SsssFn fn) { retrieveSsssFn_ = std::move(fn); }
    void setResetDeviceKeysFn(ResetDeviceKeysFn fn) { resetDeviceKeysFn_ = std::move(fn); }
    void setSetupCrossSigningWithPasswordFn(SetupCrossSigningWithPasswordFn fn) { setupCrossSigningWithPasswordFn_ = std::move(fn); }
    void setUiaSessionFn(UiaSessionFn fn) { uiaSessionFn_ = std::move(fn); }

    static int imageCacheSize() {
        QSettings s;
        return s.value("cache/imageCount", 20).toInt();
    }

    // Developer settings: OTK claim policy (see core Decryptor setters).
    static int otkClaimRateLimitMin() {
        QSettings s;
        return s.value("dev/otkClaimRateLimitMin", 5).toInt();
    }
    static int otkDrainBudget() {
        QSettings s;
        return s.value("dev/otkDrainBudget", 200).toInt();
    }

    static int pollTimeoutMs() {
        QSettings s;
        // Default 20000: the delivery latency equals the long-poll timeout
        // (a 3000ms default made every message ~3s late on the receiver).
        // Element uses 30s+ and delivers near-instantly.
        return s.value("sync/pollTimeoutMs", 20000).toInt();
    }

    static int historyLoadLimit() {
        QSettings s;
        return s.value("sync/historyLoadLimit", 50).toInt();
    }

    static bool shareKeysVerifiedOnly() {
        QSettings s;
        return s.value("e2ee/shareKeysVerifiedOnly", false).toBool();
    }

    static bool invisibleMode() {
        QSettings s;
        return s.value("privacy/invisibleMode", false).toBool();
    }

private slots:
    void onSave();

signals:
    void settingsChanged();

private:
    void loadDevices(QVBoxLayout* sectionLayout);
    // Account & Security: token reveal/copy, change password, logout other
    // sessions. Populated from the CURRENT client (account switch = new
    // setClient -> refresh), so the section always shows the ACTIVE account.
    void refreshAccountSection();
    void onChangePassword();
    void onLogoutOtherDevices();
    // Run a security action (setup/reset) off the UI thread with a busy
    // button + marshalled result; the UIA password retry runs async too.
    void runSecurityAction(std::function<bool()> fn, QPushButton* btn,
                           const QString& busyText, const QString& okLabel,
                           const QString& okMsg, const QString& failMsg);
    // Generic busy/async/marshal for the backup/SSSS buttons.
    void runAsyncButton(QPushButton* btn, const QString& busyText, const QString& idleText,
                        std::function<bool()> fn, std::function<void(bool)> onDone);

    std::shared_ptr<MatrixClient> client_;
    SessionStore* store_ = nullptr;
    QVBoxLayout* devicesSection_ = nullptr;
    QVBoxLayout* accountSection_ = nullptr;
    QLabel* accountIdentity_ = nullptr;
    QLineEdit* tokenEdit_ = nullptr;
    VerificationHandler* verifyHandler_ = nullptr;
    Decryptor* decryptor_ = nullptr;
    SetupCrossSigningFn setupCrossSigningFn_;
    ResetCrossSigningFn resetCrossSigningFn_;
    CreateKeyBackupFn createKeyBackupFn_;
    UploadKeyBackupFn uploadKeyBackupFn_;
    DeleteKeyBackupFn deleteKeyBackupFn_;
    RestoreKeyBackupFn restoreKeyBackupFn_;
    SsssFn uploadSsssFn_;
    SsssFn retrieveSsssFn_;
    ResetDeviceKeysFn resetDeviceKeysFn_;
    SetupCrossSigningWithPasswordFn setupCrossSigningWithPasswordFn_;
    UiaSessionFn uiaSessionFn_;
    QSpinBox* cacheSpin_;
    QSpinBox* otkClaimSpin_ = nullptr;
        ProxySettingsWidget* proxyWidget_ = nullptr;
    QSpinBox* otkDrainSpin_ = nullptr;
    QSpinBox* syncTimeoutSpin_ = nullptr;
    QSpinBox* historySpin_ = nullptr;
    QCheckBox* invisibleCheck_ = nullptr;
    QCheckBox* verifiedOnlyCheck_ = nullptr;
    QLabel* xsStatusLabel_ = nullptr;
};

} // namespace pcp::desktop
