// FLvibed - the host's core as a module's host part sees it. FLvibed.dll has one host: this core (namespace flv_host, the
// sources of core\host plus core\hook, log and text, compiled once) and a part per module (modules\controller\src\host,
// modules\subtitles\src\host) in the module's own namespace. build.cmd's part wrapper includes this file inside flv_host and opens
// the part's namespace with `using namespace flv_host`, so a part calls hk::, exe:: and the services below as before, and
// every part reaches the one instance. The logic has its own instance of hook, log and text (core\loader.h): never mix them.
#pragma once
#include "common.h"
#include "flexe.h"
#include "hook.h"
#include "log.h"
#include "text.h"

#include "game/events.h"
#include "game/fl.h"

#include "dimouse.h"

#include "router.h"
#include "ui_buttons.h"

#include "chatlines.h"
#include "commands.h"
#include "deferred.h"
#include "fixes.h"
#include "focus.h"
#include "input.h"
#include "moviehooks.h"
#include "pagehost.h"
#include "uibtn.h"
#include "uihooks.h"
#include "win7sim.h"
