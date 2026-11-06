// What the rest of FLvibed may read of the autopilot: core\ui\views.h AutopilotView for the HUD and the pad, and autopilot.h's
// read-only block for the goal panel, so autopilot_impl.h stays the autopilot's own.
#include "autopilot_impl.h"
#include "barbutton.h"
#include "game/modslots.h"
#include "goals.h"
#include "ui/views.h"

namespace autopilot
{
static void PanelUpdate(float dt, const bool* down) { goals::Update(dt, down); }
static void Smart() { StartSmart(); }
static void StopSaid(const char* why) { Stop(why); }

u32 TargetId() { return g.target; }
const char* ObjectiveActionName() { return ActionName(g_obj.action); }
bool Engages() { return g_set.engage; }
bool FastDock() { return g_set.fastDock; }
float FollowDistance() { return g_set.followDistance; }
const int* MpAutopilotSetting() { return &g_set.mpAutopilot; }
const route::Plan* TravelPlan() { return g.tr.plan.ok ? &g.tr.plan : nullptr; }
size_t TravelStep() { return g.tr.step; }

bool StartMode(Mode m, u32 target) { return Start(m, target, nullptr); }

bool StartTravelTo(u32 target)
{
    State::Travel t;
    t.destObject = target;
    t.hasDest = space::ObjectPos(target, t.dest);
    t.phase = State::Travel::Plot;
    const bool ok = t.hasDest && Start(Mode::Travel, target, nullptr);
    if (ok)
        g.tr = t;
    return ok;
}

// What the objective run is waiting for, in the HUD's words; null when it is not waiting.
const wchar_t* WaitReason()
{
    if (!g_obj.on)
        return nullptr;
    switch (g_obj.action)
    {
    case Action::DockWait: return L"WAITING FOR CLEARANCE";
    case Action::LaneWait: return L"WAITING FOR THE LANE";
    case Action::LeadStall: return L"WAITING FOR THE LEADER";
    case Action::PreStage: return L"WAITING FOR THE NEXT STEP";
    case Action::Rest: return L"RESTING";
    case Action::SurviveHold:
    case Action::SurviveStay: return L"HOLDING";
    case Action::Survive: return L"SURVIVING";
    case Action::Leash: return L"KEEPING THE LEASH";
    default: return nullptr;
    }
}

void PublishView()
{
#define G(fn) modslots::Guard<modslots::kAutopilot, fn>::Call
    views::AutopilotView v = {};
    v.WaitReason = G(WaitReason), v.ObjectiveOn = G(ObjectiveOn), v.Target = G(TargetId), v.Active = G(Active), v.MissionObjects = G(MissionObjectIds);
    v.Mode = G(Current), v.OwnDock = G(OwnDock), v.Fly = G(Fly), v.DockSkip = G(DockSkip), v.Stop = G(StopSaid), v.StartSmart = G(Smart);
    v.TracePlayer = G(TracePlayer), v.NoteMenu = G(NoteMenu), v.ForgetGame = G(ForgetGame), v.Command = G(Command);
    v.Defs = G(Defs), v.SettingApplied = G(OnSettingApplied), v.Load = G(Load), v.WriteCommented = G(WriteCommented), v.SettingsReference = G(SettingsReference);
    v.PanelClose = G(goals::Close), v.PanelOpen = G(goals::IsOpen), v.PanelToggle = G(goals::Toggle), v.PanelOwnsButton = G(goals::OwnsButton);
    v.PanelUpdate = G(PanelUpdate), v.PanelTick = G(goals::Tick), v.PanelRender = G(goals::Render), v.PanelKey = G(goals::OnRawKey);
    v.PanelOwnsHotspots = G(goals::OwnsHotspots), v.PanelHotspot = G(goals::OnHotspot);
    v.BarUpdate = G(barbutton::Update), v.BarDraw = G(barbutton::Draw), v.BarClick = G(barbutton::OnClick), v.BarKey = G(barbutton::OnKey);
    v.BarShutdown = G(barbutton::Shutdown), v.KeyName = G(barbutton::KeyName);
#undef G
    views::autopilot = v;
}
void WithdrawView() { views::autopilot = {}; }
} // namespace autopilot
