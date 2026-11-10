// src/ui/shared/room_activity_log.hpp — log of room opens (viewable in UI).
//
// Records every room the user opens (click / keyboard / switcher) with a
// timestamp. The "Room activity log" dialog (Settings menu) shows the last
// entries — useful to answer "when did I open which room".

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pcp::desktop {

struct RoomOpenEntry {
    int64_t ts = 0;          // unix ms
    std::string roomId;
    std::string roomName;
};

// Append a room-open entry (thread-safe, bounded to the last 500 entries).
void logRoomOpen(const std::string& roomId, const std::string& roomName);

// Snapshot of the recorded entries, newest first.
std::vector<RoomOpenEntry> roomOpenLogSnapshot();

} // namespace pcp::desktop
