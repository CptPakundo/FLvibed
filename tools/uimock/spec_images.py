"""spec_images.py - the living spec page's real captures (release build) as page-size JPEGs.

    research\\tools\\ocr\\.venv\\Scripts\\python.exe -I tools\\uimock\\spec_images.py

Writes research\\evidence\\controller_ui\\mockup\\img\\cap\\ (git-ignored, beside the page build): the flight HUD 2 showcase
(research\\experience\\runs\\hud2_fix2_2026-10-25\\final_*.png) with its classic counterparts, and the Extended HUD 2
before_ / final_ pairs plus the aspect frames (research\\experience\\runs\\hud2x_final_2026-10-25\\). Names are the page's
(part2b_doc.js); a missing source is reported and skipped.
"""
import os

from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
RUNS = os.path.join(ROOT, 'research', 'experience', 'runs')
OUT = os.path.join(ROOT, 'research', 'evidence', 'controller_ui', 'mockup', 'img', 'cap')
FIX2 = os.path.join(RUNS, 'hud2_fix2_2026-10-25')
# the 0.1.0 recapture (26 Oct 2026, HEAD c33e680): HUD 2's ring row, the LB hints, the card's RS x2 CLEAR footer
REFRESH = os.path.join(RUNS, 'spec_refresh_2026-10-26')
X = os.path.join(RUNS, 'hud2x_final_2026-10-25')

# page name -> source
FLIGHT = {
    'f_combat_h2': os.path.join(REFRESH, 'final_combat.png'),
    'f_combat_cl': os.path.join(RUNS, 'ui_glyphs_2026-10-25', 'after_combat_4k-062628.png'),
    'f_travel_h2': os.path.join(REFRESH, 'final_travel.png'),
    'f_travel_cl': os.path.join(RUNS, 'ui_glyphs_2026-10-25', 'after_4k-062627.png'),
    'f_cruise_h2': os.path.join(REFRESH, 'final_cruise.png'),
    'f_cruise_cl': os.path.join(ROOT, 'research', 'experience', 'ui_2026-10-25', 'screens', 'space_cruise-013824.png'),
    'f_dock_h2': os.path.join(REFRESH, 'final_dock.png'),
    'f_dock_cl': os.path.join(RUNS, 'hud2_integrate_2026-10-25', 'classic_target_station-094106.png'),
    'f_kbm_h2': os.path.join(REFRESH, 'final_kbm.png'),
    'f_kbm_cl': os.path.join(RUNS, 'hud2_build_2026-10-25', 'native-075908.png'),
    'f_lane': os.path.join(REFRESH, 'final_lane.png'),
    'f_jump': os.path.join(FIX2, 'final_jump.png'),
    'f_turret': os.path.join(REFRESH, 'final_turret.png'),
    'f_missile': os.path.join(REFRESH, 'final_missile_alert.png'),
    'f_m03': os.path.join(REFRESH, 'final_m03_protect.png'),
    'f_720p': os.path.join(REFRESH, 'final_720p_combat.png'),
}
# The FLvibed-wide sections (25 Oct 2026 evening): the HUD cues and aim captured on instance 3 (release build, SaveC705,
# research\experience\runs\spec_flvibed_2026-10-25\), the subtitles / autopilot / speedrun frames reused from earlier runs
SPEC = os.path.join(RUNS, 'spec_flvibed_2026-10-25')
UIV = os.path.join(RUNS, 'ui_verify_2026-10-25')
SUBS = os.path.join(RUNS, 'hud2_subs_1025')
FLVIBED = {
    'f_classic_km': os.path.join(RUNS, 'hud2x_modal_2026-10-25', 'resumed_classic_kbm.png'),
    'c_break_cl': os.path.join(SPEC, 'cue_break_hs1-193822.png'),
    'c_break_h2': os.path.join(SPEC, 'brk_hs2_a-193935.png'),
    'c_hull_cl': os.path.join(SPEC, 'cue_hull_hs1-193823.png'),
    'c_hull_h2': os.path.join(SPEC, 'cue_hull_hud2-193905.png'),
    'c_real_pad': os.path.join(SPEC, 'real_hs0_pad-193818.png'),
    'c_real_km': os.path.join(SPEC, 'real_hs0_kbm-193818.png'),
    'c_real_h2': os.path.join(SPEC, 'real_hs2_pad-193820.png'),
    'c_ring_ap': os.path.join(SPEC, 'cue_ring_hs0-193822.png'),
    'c_ring_nano': os.path.join(SPEC, 'cue_ring_hud2-193906.png'),
    'c_modal': os.path.join(SPEC, 'modal_pause_ui1-193907.png'),
    'a_gimbal_cl': os.path.join(SPEC, 'aim_hs0_mode1-193939.png'),
    'a_reticle_cl': os.path.join(SPEC, 'aim_hs0_mode0-193940.png'),
    'a_gimbal_h2': os.path.join(REFRESH, 'aim_hs2_mode1.png'),
    'a_reticle_h2': os.path.join(REFRESH, 'aim_hs2_mode0.png'),
    's_portrait_h2': os.path.join(SUBS, 'real_portrait_4k-081228.png'),
    's_portrait_cl': os.path.join(SUBS, 'real_switch_native_4k-081229.png'),
    's_nn_h2': os.path.join(SUBS, 'real_lines_4k-081212.png'),
    'p_ap_h2': os.path.join(REFRESH, 'ap_hold_hud2_timer.png'),
    'p_ap_cl': os.path.join(UIV, 'c_apstatus_4k-054415.png'),
    'p_goals': os.path.join(UIV, 'c_goals_4k-054415.png'),
    'p_picker': os.path.join(UIV, 'c_picker_4k-054413.png'),
    'p_wheel': os.path.join(UIV, 'c_wheel_4k-054414.png'),
    'p_timer': os.path.join(UIV, 'c_timer_4k-054416.png'),
    # glyph_look (commit c5d0191; research\experience\runs\glyph_look_2026-10-25\)
    'g_h2_look0': os.path.join(REFRESH, '4k_flight_hud2_xbox_look0.png'),
    'g_h2_look1': os.path.join(REFRESH, '4k_flight_hud2_xbox_look1.png'),
    'g_cl_look0': os.path.join(RUNS, 'glyph_look_2026-10-25', '4k_flight_classic_xbox_look0-194351.png'),
    'g_cl_look2': os.path.join(RUNS, 'glyph_look_2026-10-25', '4k_flight_classic_xbox_look2-194354.png'),
}
# The FIX tracks (25 Oct 2026 night): text fit, 4:3 lane, the alert vs the subtitles, the options page's text size, vanilla
# (research\experience\runs\fix_text_2026-10-25\, fix_text2_2026-10-25\); name -> (source, crop box or None). The vanilla
# frames of the DPI-unaware 1280x960 window come as 2560x1920 with the game in the top-left quarter: cropped to it.
FT = os.path.join(RUNS, 'fix_text_2026-10-25')
FT2 = os.path.join(RUNS, 'fix_text2_2026-10-25')
# The last fix tracks (25 Oct 2026, late night): HUD 2's aspect factor and the formatted objective (hud2_43), the comm
# names / maneuver bar / portrait (fix_text4, fix_text5), the opaque options page (fix_text3).
H43 = os.path.join(RUNS, 'hud2_43_2026-10-25')
FT3 = os.path.join(RUNS, 'fix_text3_2026-10-25')
FT4 = os.path.join(RUNS, 'fix_text4_2026-10-25')
FT5 = os.path.join(RUNS, 'fix_text5_2026-10-25')
FIXES = {
    't_43_fight': (os.path.join(H43, '1600x1200_donau_live2-222137.png'), None),
    't_43_alert': (os.path.join(FT2, '1600x1200_max_alert_4.png'), None),
    't_vanilla_fight': (os.path.join(H43, 'vanilla_1280x960_donau-222446.png'), (0, 0, 1280, 960)),
    's_43_portrait_h2': (os.path.join(FT5, '1600_b2_hud2_portrait_real-221556.png'), None),
    's_vanilla_portrait_cl': (os.path.join(FT5, 'vanilla_b2_hud0-221458.png'), (0, 0, 1280, 960)),
    's_longname_cl': (os.path.join(FT4, '1600_hud0_max_long40.png'), None),
    't_43_room': (os.path.join(FT, '1600x1200_after_room.png'), None),
    't_43_jobboard': (os.path.join(FT, '1600x1200_after_jobboard.png'), None),
    't_43_pause': (os.path.join(FT, '1600x1200_after_pause_docked.png'), None),
    't_43_equipment': (os.path.join(FT, '1600x1200_after_equipment.png'), None),
    'o_4k_min': (os.path.join(FT2, '4k_min_hosted.png'), None),
    'o_4k_max': (os.path.join(FT2, '4k_max_hosted.png'), None),
    'o_43_max': (os.path.join(FT3, 'rel_1600_style1_standalone-214657.png'), None),
}
SCREENS = ['room_trader', 'room_bar', 'room_launchpad', 'jobboard', 'news', 'offer', 'commodity', 'equipment', 'shipdealer',
           'repair', 'nn_inventory_docked', 'nn_playerinfo_docked', 'nn_log_docked', 'nn_help_docked', 'pause_docked',
           'pause_space', 'save_dialog', 'osk_save', 'load_ingame', 'confirm_docked', 'options_ingame_video',
           'options_front_video', 'mainmenu', 'load_front', 'newgame_prompt', 'navmap_docked', 'navmap_space', 'death',
           'chat_space']
ASPECT = ['final43_commodity', 'final43_shipdealer', 'final43_options_front_video', 'final1610_commodity',
          'final1610_shipdealer', 'final1610_options_front_video', 'final21x9_commodity', 'final21x9_jobboard',
          'final21x9_pause_docked', 'final21x9_room_trader']


def save(src, name, width=1600, crop=None):
    if not os.path.exists(src):
        print('missing', src)
        return
    im = Image.open(src).convert('RGB')
    if crop:
        im = im.crop(crop)
    if im.width > width:
        im = im.resize((width, round(width * im.height / im.width)), Image.LANCZOS)
    im.save(os.path.join(OUT, name + '.jpg'), quality=80, optimize=True, progressive=True)


def main():
    os.makedirs(OUT, exist_ok=True)
    for n, s in list(FLIGHT.items()) + list(FLVIBED.items()):
        save(s, n)
    for n, (s, box) in FIXES.items():
        save(s, n, 1600, box)
    for k in SCREENS:
        save(os.path.join(X, f'before_{k}.png'), f'x_{k}_cl')
        save(os.path.join(X, f'final_{k}.png'), f'x_{k}_h2')
    for k in ASPECT:
        save(os.path.join(X, k + '.png'), 'a_' + k.replace('final', ''), 1200)
    print(len(os.listdir(OUT)), 'images in', OUT)


if __name__ == '__main__':
    main()
