// src/ui/image_loader.cpp
#include "image_loader.hpp"
#include "core/matrix_client.hpp"
#include "core/debug_log.hpp"
#include "core/thread_pool.hpp"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QMetaObject>
#include <QPointer>
#include <QSet>
#include <QThread>
#include <QString>

namespace progressive::desktop {

// Bounds the GIF movie pool — otherwise it grows without limit on small devices.
static const int kMaxCachedMovies = 20;

// Cap for the negative-cache (failed mxcs) — beyond this, drop everything.
static const int kMaxFailedEntries = 1024;

// Mxcs uploaded by this process (send path) — exempt from the negative cache
// because fresh uploads 404 briefly while the media server replicates.
static QSet<QString>& freshUploadSet() {
    static QSet<QString> set;
    return set;
}

void markMxcFreshUpload(const std::string& mxcUrl) {
    auto& s = freshUploadSet();
    s.insert(QString::fromStdString(mxcUrl));
    if (s.size() > 512) s.clear();  // bounded; old entries fall out naturally
}

ImageLoader::ImageLoader(std::shared_ptr<MatrixClient> client, QObject* parent)
    : QObject(parent), client_(std::move(client)) {}

bool ImageLoader::isFailed(const QString& key) const {
    if (freshUploadSet().contains(key)) return false;  // own upload — always retry
    auto it = failedUntil_.constFind(key);
    if (it == failedUntil_.constEnd()) return false;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now >= it.value()) {
        const_cast<ImageLoader*>(this)->failedUntil_.erase(it);
        const_cast<ImageLoader*>(this)->failedReason_.remove(key);
        return false;
    }
    return true;
}

void ImageLoader::markFailed(const QString& key, const std::string& context,
                             const QString& reason) {
    if (freshUploadSet().contains(key)) {
        LOG(LogChannel::NET, "imageLoader: fresh upload mxc=%.80s failed — retrying on next paint",
            key.toStdString().c_str());
        return;
    }
    const bool avatar = context == "avatar" || context == "room avatar";
    const bool image = context == "timeline image";
    const qint64 cooldown = avatar ? kFailedAvatarCooldownMs
                            : image ? kFailedImageCooldownMs
                                    : kFailedMxcCooldownMs;
    failedUntil_.insert(key, QDateTime::currentMSecsSinceEpoch() + cooldown);
    failedReason_.insert(key, reason.isEmpty()
        ? QString::fromStdString(context.empty() ? "fetch failed" : context)
        : reason);
    if (failedUntil_.size() > kMaxFailedEntries) {
        failedUntil_.clear();
        failedReason_.clear();
    }
    LOG(LogChannel::NET, "imageLoader: %s failed for mxc=%.80s — not retrying for %lld min",
        context.empty() ? "fetch" : context.c_str(), key.toStdString().c_str(),
        cooldown / 60000);
}

QString ImageLoader::failureReason(const std::string& mxcUrl) const {
    return failedReason_.value(QString::fromStdString(mxcUrl));
}

bool ImageLoader::beginFetch(const QString& key,
                             std::function<void(const QImage&)> cb,
                             std::function<void()> run) {
    auto it = inFlight_.find(key);
    if (it != inFlight_.end()) {
        it.value().append(std::move(cb));
        return false;
    }
    inFlight_.insert(key, QList<std::function<void(const QImage&)>>{std::move(cb)});
    run();
    return true;
}

QString ImageLoader::mediaCacheDir() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + "/media_cache";
}

QString ImageLoader::cachePathFor(const QString& key) const {
    QByteArray hash = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha256).toHex();
    return mediaCacheDir() + "/" + QString::fromLatin1(hash) + ".img";
}

bool ImageLoader::loadFromDiskCache(const QString& key, QByteArray& bytes) {
    QString path = cachePathFor(key);
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    bytes = f.readAll();
    return !bytes.isEmpty();
}

void ImageLoader::evictIfOverBudget() {
    QDir dir(mediaCacheDir());
    if (!dir.exists()) return;
    qint64 total = 0;
    QFileInfoList infos;
    const auto entries = dir.entryInfoList(QDir::Files, QDir::Time);  // newest first
    for (const auto& fi : entries) {
        total += fi.size();
        infos.append(fi);
    }
    // Delete oldest beyond the budget.
    for (int i = infos.size() - 1; i >= 0 && total > kMediaCacheBudget; --i) {
        total -= infos[i].size();
        QFile::remove(infos[i].absoluteFilePath());
    }
}

void ImageLoader::saveToDiskCache(const QString& key, const QByteArray& bytes) {
    if (bytes.isEmpty()) return;
    QDir().mkpath(mediaCacheDir());
    QFile f(cachePathFor(key));
    if (f.open(QIODevice::WriteOnly)) {
        f.write(bytes);
        f.close();
        evictIfOverBudget();
    }
}

void ImageLoader::fetchThumbnail(const std::string& mxcUrl, int w, int h,
                                   std::function<void(const QImage&)> cb,
                                   const std::string& context) {
    QString key = QString::fromStdString(mxcUrl);
    if (auto* cached = imageCache_.object(key)) {
        cb(*cached);
        return;
    }
    // Persistent disk cache (Element/Nheko parity): media seen before stays
    // visible even if the sender's server later purges the file.
    {
        QByteArray diskBytes;
        if (loadFromDiskCache(key, diskBytes)) {
            QImage diskImg;
            diskImg.loadFromData(diskBytes);
            if (!diskImg.isNull()) {
                imageCache_.insert(key, new QImage(diskImg));
                cb(diskImg);
                return;
            }
        }
    }
    if (!client_) { cb(QImage()); return; }
    if (isFailed(key)) { cb(QImage()); return; }

    auto self = QPointer<ImageLoader>(this);
    auto client = client_;
    auto run = [self, client, mxcUrl, key, w, h, context]() {
        ThreadPool::instance().enqueue([self, client, mxcUrl, key, w, h, context]() {
            auto result = client->downloadMedia(mxcUrl, w, h);
            QImage img;
            int httpStatus = result.httpStatus;
            if (result.ok && !result.data.empty()) {
                img.loadFromData(result.data.data(), static_cast<int>(result.data.size()));
            }
            // Never download the FULL file to preview a video (video bytes
            // never decode as an image, and it wastes the peer's bandwidth).
            const bool videoPreview = context == "timeline video";
            if (img.isNull() && (w > 0 || h > 0) && !videoPreview) {
                // Some servers/CDNs 404 the thumbnail endpoint — fall back to the
                // full file so avatars still render.
                auto full = client->downloadMedia(mxcUrl, 0, 0);
                if (full.ok && !full.data.empty())
                    img.loadFromData(full.data.data(), static_cast<int>(full.data.size()));
                else
                    httpStatus = full.httpStatus;
            }
            QMetaObject::invokeMethod(self, [self, key, img, httpStatus, context]() {
                if (self.isNull()) return;
                if (!img.isNull()) {
                    self->imageCache_.insert(key, new QImage(img));
                    self->failedReason_.remove(key);
                    QByteArray raw;
                    {
                        QBuffer buf(&raw);
                        buf.open(QIODevice::WriteOnly);
                        img.save(&buf, "PNG");
                    }
                    self->saveToDiskCache(key, raw);
                } else if (httpStatus >= 400) {
                    // Only HTTP failures (404/5xx) are negative-cached — a
                    // decryption failure or network blip (status 0) must be
                    // retryable (reason shown meanwhile).
                    self->markFailed(key, context,
                        "HTTP " + QString::number(httpStatus));
                } else {
                    self->failedReason_.insert(key, "network/decrypt failed — retrying");
                }
                auto queued = self->inFlight_.take(key);
                for (auto& c : queued) c(img);
            }, Qt::QueuedConnection);
        });
    };
    beginFetch(key, std::move(cb), std::move(run));
}

void ImageLoader::fetchEncryptedThumbnail(const std::string& mxcUrl,
    const std::string& key, const std::string& iv, const std::string& sha,
    std::function<void(const QImage&)> cb, const std::string& context) {
    QString qkey = QString::fromStdString(mxcUrl);
    if (auto* cached = imageCache_.object(qkey)) {
        cb(*cached);
        return;
    }
    // Persistent disk cache: store the DECRYPTED preview (same mxc = same keys).
    {
        QByteArray diskBytes;
        if (loadFromDiskCache(qkey, diskBytes)) {
            QImage diskImg;
            diskImg.loadFromData(diskBytes);
            if (!diskImg.isNull()) {
                imageCache_.insert(qkey, new QImage(diskImg));
                cb(diskImg);
                return;
            }
        }
    }
    if (!client_ || key.empty() || iv.empty()) {
        // Can never succeed as-is: a missing key/iv means no decrypt is
        // possible. Record the reason so the placeholder explains itself
        // instead of grey-forever with silent retries.
        markFailed(qkey, context, "missing key/iv — no preview");
        cb(QImage());
        return;
    }
    if (isFailed(qkey)) { cb(QImage()); return; }

    auto self = QPointer<ImageLoader>(this);
    auto client = client_;
    auto run = [self, client, mxcUrl, qkey, key, iv, sha, context]() {
        ThreadPool::instance().enqueue([self, client, mxcUrl, qkey, key, iv, sha, context]() {
            auto result = client->downloadMediaEncrypted(mxcUrl, key, iv, sha);
            QImage img;
            if (result.ok && !result.data.empty()) {
                img.loadFromData(result.data.data(), static_cast<int>(result.data.size()));
            }
            QMetaObject::invokeMethod(self, [self, qkey, img, httpStatus = result.httpStatus, context]() {
                if (self.isNull()) return;
                if (!img.isNull()) {
                    self->imageCache_.insert(qkey, new QImage(img));
                    self->failedReason_.remove(qkey);
                    QByteArray raw;
                    {
                        QBuffer buf(&raw);
                        buf.open(QIODevice::WriteOnly);
                        img.save(&buf, "PNG");
                    }
                    self->saveToDiskCache(qkey, raw);
                } else if (httpStatus >= 400) {
                    // Decrypt failures (status 0/200 — key not yet arrived)
                    // must NOT be negative-cached; only real HTTP failures.
                    self->markFailed(qkey, context,
                        "HTTP " + QString::number(httpStatus));
                } else {
                    self->failedReason_.insert(qkey, "decrypt failed — retrying");
                }
                auto queued = self->inFlight_.take(qkey);
                for (auto& c : queued) c(img);
            }, Qt::QueuedConnection);
        });
    };
    beginFetch(qkey, std::move(cb), std::move(run));
}

void ImageLoader::fetchMovie(const std::string& mxcUrl,
                               std::function<void(QMovie*)> cb) {
    QString key = QString::fromStdString(mxcUrl);
    if (auto* existing = moviePool_.value(key)) {
        cb(existing);
        return;
    }
    if (!client_) { cb(nullptr); return; }
    if (isFailed(key)) { cb(nullptr); return; }

    auto self = QPointer<ImageLoader>(this);
    auto client = client_;
    ThreadPool::instance().enqueue([self, client, mxcUrl, key, cb]() {
        auto result = client->downloadMedia(mxcUrl, 0, 0);
        QByteArray bytes;
        if (result.ok) {
            bytes = QByteArray(reinterpret_cast<const char*>(result.data.data()),
                               static_cast<int>(result.data.size()));
        }
        QMetaObject::invokeMethod(self, [self, key, bytes, httpStatus = result.httpStatus, cb]() {
            if (self.isNull()) return;
            if (bytes.isEmpty()) {
                if (httpStatus >= 400) self->markFailed(key, "movie");
                cb(nullptr);
                return;
            }
            auto* movie = new QMovie(self);
            movie->setFormat("GIF");
            auto* buf = new QBuffer(movie);
            buf->setData(bytes);
            buf->open(QIODevice::ReadOnly);
            movie->setDevice(buf);
            movie->setCacheMode(QMovie::CacheAll);
            if (self->moviePool_.size() >= kMaxCachedMovies && !self->moviePool_.contains(key)) {
                auto it = self->moviePool_.begin();
                if (it != self->moviePool_.end()) {
                    (*it)->deleteLater();
                    self->moviePool_.erase(it);
                }
            }
            self->moviePool_[key] = movie;
            cb(movie);
        }, Qt::QueuedConnection);
    });
}

bool ImageLoader::hasImage(const std::string& mxcUrl) const {
    return imageCache_.contains(QString::fromStdString(mxcUrl));
}

QImage ImageLoader::getCached(const std::string& mxcUrl) const {
    if (auto* cached = imageCache_.object(QString::fromStdString(mxcUrl))) {
        return *cached;
    }
    return {};
}

} // namespace progressive::desktop
