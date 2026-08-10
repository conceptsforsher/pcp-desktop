// src/ui/shared/room_activity_log.cpp — room open log storage.

#include "room_activity_log.hpp"

#include <chrono>
#include <mutex>

namespace progressive::desktop {

namespace {
constexpr size_t kMaxEntries = 500;
std::vector<RoomOpenEntry> g_entries;
std::mutex g_mtx;
} // namespace

void logRoomOpen(const std::string& roomId, const std::string& roomName) {
    std::lock_guard<std::mutex> lk(g_mtx);
    RoomOpenEntry e;
    e.ts = static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    e.roomId = roomId;
    e.roomName = roomName;
    g_entries.push_back(std::move(e));
    if (g_entries.size() > kMaxEntries) {
        g_entries.erase(g_entries.begin(), g_entries.begin() + (g_entries.size() - kMaxEntries));
    }
}

std::vector<RoomOpenEntry> roomOpenLogSnapshot() {
    std::lock_guard<std::mutex> lk(g_mtx);
    std::vector<RoomOpenEntry> out(g_entries.rbegin(), g_entries.rend());
    return out;
}

} // namespace progressive::desktop
