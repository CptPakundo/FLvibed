// FLvibed - which XInput slot the pad is read from (gamepad.cpp; header only, unit test tests\pad_slots_test.cpp).
// The pad in use stays on its slot while it answers; when it stops answering (unplugged) the slots are scanned again at
// once and then every kRescan seconds, the first that answers wins - polling slots that have nothing connected can
// stall on some systems, so they are not polled every frame. The same rule for a real pad and the device-layer test
// pads (shm.h v2: IDEAS T36), so a test exercises the slot scan and hot-plug a player meets.
#pragma once

namespace padslots
{
constexpr int kSlots = 4;          // XUSER_MAX_COUNT
constexpr float kRescan = 1.5f;    // seconds between scans while no slot is in use

enum class Event
{
    None,
    Connected,    // a slot answered (Index() is it)
    Disconnected, // the slot in use stopped answering (Lost() is it); a scan follows in the same call
};

class Scan
{
  public:
    // Once per frame. get(i) -> true when slot i answers (the caller keeps that slot's state). Returns whether a slot
    // is read this frame; ev / ev2 = what changed (an unplug and a replug on another slot in one call: both).
    template <class Get> bool Read(Get get, float dt, Event& ev, Event& ev2)
    {
        ev = ev2 = Event::None;
        timer_ -= dt;
        if (index_ >= 0)
        {
            if (get(index_))
                return true;
            ev = Event::Disconnected;
            lost_ = index_;
            index_ = -1;
            timer_ = 0;
        }
        if (timer_ > 0)
            return false;
        timer_ = kRescan;
        for (int i = 0; i < kSlots; ++i)
            if (get(i))
            {
                index_ = i;
                (ev == Event::None ? ev : ev2) = Event::Connected;
                return true;
            }
        return false;
    }
    int Index() const { return index_; }
    int Lost() const { return lost_; }

  private:
    int index_ = -1;
    int lost_ = -1;
    float timer_ = 0;
};
} // namespace padslots
