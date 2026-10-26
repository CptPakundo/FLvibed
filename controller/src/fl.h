// Freelancer.exe helpers for the controller. Every Freelancer.exe address is a symbol of core\flexe.h (exe::,
// verified by signature at startup; an unresolved one fails closed). Credits to the FLUF, ChaosMod, flplusplus
// and FLHDE projects for documenting many of them.
#pragma once
#include "common.h"
#include "flexe.h"

namespace fl
{
// Mouse device wrapper (exe::MouseObj) members.
constexpr uintptr_t MOUSE_DI_DEVICE = 0x10; // IDirectInputDevice8A*
constexpr uintptr_t MOUSE_FLAGS = 0x14;     // u8 flags (bit 0: captured; dimouse.cpp)

enum Cmd : int
{
    USER_NONE = 0,
    USER_KEY,
    USER_CHAR,
    USER_BUTTON,
    USER_WHEEL,
    USER_MOVE,
    USER_BUTTON0,
    USER_BUTTON1,
    USER_BUTTON2,
    USER_BUTTON3,
    USER_BUTTON4,
    USER_BUTTON5,
    USER_BUTTON6,
    USER_BUTTON7,
    USER_EVENTS,
    USER_CANCEL,
    USER_STACKABLE_CANCEL,
    USER_ENTER,
    USER_NO,
    USER_YES,
    USER_PAUSE,
    USER_RESET,
    USER_SAVE_GAME,
    USER_FULLSCREEN,
    USER_USE_ITEM,
    USER_ACTIVATE_ITEM,
    USER_NEXT_ITEM,
    USER_PREV_ITEM,
    USER_SET_TARGET,
    USER_NEXT_TARGET,
    USER_PREV_TARGET,
    USER_NEXT_WEAPON_SUBTARGET,
    USER_PREV_WEAPON_SUBTARGET,
    USER_NEXT_SUBTARGET,
    USER_PREV_SUBTARGET,
    USER_CLEAR_TARGET,
    USER_NEXT_ENEMY,
    USER_PREV_ENEMY,
    USER_CLOSEST_ENEMY,
    USER_ZOOM_IN,
    USER_ZOOM_OUT,
    USER_ZOOM_TOGGLE,
    USER_X_ROTATE,
    USER_X_UNROTATE,
    USER_Y_ROTATE,
    USER_Y_UNROTATE,
    USER_Z_ROTATE,
    USER_Z_UNROTATE,
    USER_VIEW_RESET,
    USER_COCKPIT_CAMERA_MODE,
    USER_LOOK_ROTATE_CAMERA_LEFT,
    USER_LOOK_ROTATE_CAMERA_RIGHT,
    USER_LOOK_ROTATE_CAMERA_UP,
    USER_LOOK_ROTATE_CAMERA_DOWN,
    USER_LOOK_ROTATE_CAMERA_RESET,
    USER_CHASE_CAMERA_MODE,
    USER_REARVIEW_CAMERA_MODE,
    USER_REARVIEW_CAMERA_MODE_OFF,
    USER_AFTERBURN,
    USER_AFTERBURN_OFF,
    USER_AFTERBURN_ON,
    USER_TOGGLE_AUTO_AVOIDANCE,
    USER_TOGGLE_AUTO_LEVEL,
    USER_TOGGLE_LEVEL_CAMERA,
    USER_WARP,
    USER_ENGINE_TOGGLE,
    USER_SET_THROTTLE,
    USER_INC_THROTTLE,
    USER_DEC_THROTTLE,
    USER_CRUISE,
    USER_THROTTLE_0,
    USER_THROTTLE_10,
    USER_THROTTLE_20,
    USER_THROTTLE_25,
    USER_THROTTLE_30,
    USER_THROTTLE_40,
    USER_THROTTLE_50,
    USER_THROTTLE_60,
    USER_THROTTLE_70,
    USER_THROTTLE_75,
    USER_THROTTLE_80,
    USER_THROTTLE_90,
    USER_THROTTLE_100,
    USER_RADAR,
    USER_CONTACT_LIST,
    USER_RADAR_ZOOM,
    USER_SWITCH_TO_PLAYER_SHIP,
    USER_SWITCH_TO_WEAPON_LIST,
    USER_SWITCH_TO_TARGET,
    USER_FORMATION_LIST,
    USER_RADAR_ZOOM_IN,
    USER_RADAR_ZOOM_OUT,
    USER_RADIO,
    USER_CHAT,
    USER_STATUS,
    USER_TARGET,
    USER_STATUS_MODE,
    USER_TOGGLE_MAINFRAME,
    USER_NN,
    USER_NAV_MAP,
    USER_NAVMAP,
    USER_PLAYER_STATS,
    USER_INVENTORY,
    USER_STORY_STAR,
    USER_CHAT_WINDOW,
    USER_GROUP_WINDOW,
    USER_HELP,
    USER_INVENTORY_CLOSE,
    NN_TOGGLE_OPEN,
    USER_MINIMIZE_HUD,
    USER_DISPLAY_LAST_OBJECTIVE,
    USER_COLLECT_LOOT,
    USER_ACTIONS,
    USER_MANEUVER,
    USER_SET_MANEUVER_DIRECTION,
    USER_MANEUVER_WINDOW,
    USER_MANEUVER_TRAIL,
    USER_MANEUVER_AFTERBURNER,
    USER_MANEUVER_EVADE,
    USER_MANEUVER_ENGINEKILL,
    USER_MANEUVER_BRAKE_REVERSE,
    USER_MANEUVER_DOCK,
    USER_MANEUVER_GOTO,
    USER_MANEUVER_FACE,
    USER_MANEUVER_CRUISE,
    USER_MANEUVER_TRADE_LANE,
    USER_MANEUVER_DRASTIC_EVADE,
    USER_MANEUVER_FORMATION,
    USER_MANEUVER_STRAFE,
    USER_MANEUVER_TRAIL_CLOSER,
    USER_MANEUVER_TRAIL_FARTHER,
    USER_MANEUVER_CORKSCREW_EVADE,
    USER_MANEUVER_SLIDE_EVADE,
    USER_MANEUVER_SLIDE_EVADE_LEFT,
    USER_MANEUVER_SLIDE_EVADE_RIGHT,
    USER_MANEUVER_SLIDE_EVADE_UP,
    USER_MANEUVER_SLIDE_EVADE_DOWN,
    USER_ACTIVATE_MANEUVER,
    USER_SCAN_CARGO,
    USER_TRACTOR_BEAM,
    USER_JAMMER,
    USER_STEALTH,
    USER_CLOAK,
    USER_REPAIR_HEALTH,
    USER_REPAIR_SHIELD,
    USER_WEAPON_GROUP1,
    USER_WEAPON_GROUP2,
    USER_WEAPON_GROUP3,
    USER_WEAPON_GROUP4,
    USER_WEAPON_GROUP5,
    USER_WEAPON_GROUP6,
    USER_TOGGLE_WEAPON1,
    USER_TOGGLE_WEAPON2,
    USER_TOGGLE_WEAPON3,
    USER_TOGGLE_WEAPON4,
    USER_TOGGLE_WEAPON5,
    USER_TOGGLE_WEAPON6,
    USER_TOGGLE_WEAPON7,
    USER_TOGGLE_WEAPON8,
    USER_TOGGLE_WEAPON9,
    USER_TOGGLE_WEAPON10,
    USER_FIRE_WEAPON1,
    USER_FIRE_WEAPON2,
    USER_FIRE_WEAPON3,
    USER_FIRE_WEAPON4,
    USER_FIRE_WEAPON5,
    USER_FIRE_WEAPON6,
    USER_FIRE_WEAPON7,
    USER_FIRE_WEAPON8,
    USER_FIRE_WEAPON9,
    USER_FIRE_WEAPON10,
    USER_FIRE_WEAPON_GROUP1,
    USER_FIRE_WEAPON_GROUP2,
    USER_FIRE_WEAPON_GROUP3,
    USER_FIRE_WEAPON_GROUP4,
    USER_FIRE_WEAPON_GROUP5,
    USER_FIRE_WEAPON_GROUP6,
    USER_ASSIGN_WEAPON_GROUP1,
    USER_ASSIGN_WEAPON_GROUP2,
    USER_ASSIGN_WEAPON_GROUP3,
    USER_ASSIGN_WEAPON_GROUP4,
    USER_ASSIGN_WEAPON_GROUP5,
    USER_ASSIGN_WEAPON_GROUP6,
    USER_REMAPPABLE_LEFT,
    USER_REMAPPABLE_RIGHT,
    USER_REMAPPABLE_UP,
    USER_REMAPPABLE_DOWN,
    USER_FIRE_FORWARD,
    USER_LAUNCH_MISSILES,
    USER_LAUNCH_MINES,
    USER_LAUNCH_COUNTERMEASURES,
    USER_AUTO_TURRET,
    USER_LAUNCH_TORPEDOS,
    USER_LAUNCH_CRUISE_DISRUPTORS,
    USER_TURN,
    USER_NEXT_OBJECT,
    USER_PREV_OBJECT,
    USER_EXIT_GAME,
    USER_MANEUVER_FREE_FLIGHT,
    USER_FIRE_WEAPONS,
    USER_TURN_SHIP,
    USER_GROUP_INVITE,
    USER_TRADE_REQUEST,
    USER_SCREEN_SHOT,
    CMD_COUNT
};
static_assert(USER_SCREEN_SHOT == 0xCB, "command enum must match Freelancer's table");

inline const char* CmdNameRaw(int cmd)
{
    if (cmd < 0 || cmd >= CMD_COUNT)
        return "?";
    if (!exe::KeyCmdNames.ok())
        return "?";
    const char* name = reinterpret_cast<const char* const*>(exe::KeyCmdNames.get())[cmd];
    return name ? name : "?";
}
const char* CmdName(int cmd);
int CmdFromName(const char* name); // accepts "USER_X" or a number; -1 if unknown

// Event mask of a key command from keymap.ini ("state = keydown, keyup, repeat...").
enum CmdFlags : u32
{
    CMDF_KEYDOWN = 1,
    CMDF_KEYUP = 2,
    CMDF_REPEAT = 4,
    CMDF_SHIFT_IGNORE = 8,
};
u32 CommandFlags(int cmd); // CMDF_KEYDOWN if the command is not in the key map

// Fire a key command through Freelancer's own dispatcher, as if a bound key went down/up.
bool FireCommand(int cmd, bool down);
// Feed a raw keyboard message through Freelancer's keyboard handler, exactly like a
// physical key: UI controls see it first, then the player's key bindings apply.
bool SendKey(int vk, bool down);
void TapKey(int vk);
bool SendChar(wchar_t ch);
// Mark a command as "held" so Freelancer's polling (e.g. fire weapons) sees it pressed.
void SetHeld(int cmd, bool held);
void ClearAllHeld();

// Game state helpers
inline int ScreenWidth() { return Mem<int>(exe::ScreenW); }
inline int ScreenHeight() { return Mem<int>(exe::ScreenH); }
inline int MouseX() { return Mem<int>(exe::MouseX); }
inline int MouseY() { return Mem<int>(exe::MouseY); }
inline u32 MouseButtons() { return Mem<u32>(exe::MouseButtons); }
inline HWND MainWindow() { return Mem<HWND>(exe::MainHwnd); }
bool GamePaused();
inline float GetThrottle() { return Mem<float>(exe::PlayerThrottle); }
inline void SetThrottle(float t) { hk::Call<void(__cdecl*)(float)>(exe::SetThrottle, t); }
inline u32 CurrentCamera() { return Mem<u32>(exe::CurrentCamera); }
// The death camera object (code references only its rotation at +4), 0 when unresolved.
inline u32 CamDeath() { return exe::CamDeathRotation.ok() ? static_cast<u32>(exe::CamDeathRotation.get()) - 4 : 0; }
void* PlayerIObj();
u32 PlayerBase();
u32 PlayerSystem();
const char* CursorName();

// Player ship state through its IObjInspect interface (null when not in space).
struct Kinematics
{
    float pos[3];
    float vel[3];    // world units/s
    float angVel[3]; // rad/s, world space
    float rot[9];    // orientation, row-major 3x3 (columns = ship right/up/back axes)
};
bool PlayerKinematics(Kinematics& out);
bool TargetKinematics(Kinematics& out); // the player's current target
uintptr_t TargetHandle();               // identity of the current target (IObjRW*), 0 if none
u32 TargetId();                         // the current target's space object id (pub::SpaceObj), 0 if none
bool TargetRadius(float& out);          // physical radius of the current target (metres)
// The current target's attitude towards the player (-1 hostile .. 1 friendly); false without target.
bool TargetAttitude(float& out);
// Freelancer's lead ("prediction cross") position for the current target, world space.
// Returns the game's result code (-1 = not available).
int TargetLeadPos(float out[3]);
// Project a world position with the current 3D camera: out = (pixel x, pixel y, depth).
// False when the point is behind the camera.
bool WorldToScreen(const float world[3], float out[3]);
} // namespace fl
