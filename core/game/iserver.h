// FLvibed - the server interface's slots by name (types only; nothing calls them yet: audit 70b F-sym).
// Every client request goes through one vtable: (*exe::ServerIface)->vt[slot / 4](..., client id), the client id being
// exe::OwnClientId (core\flexe.h "screens: shared"); in single player the object is Server.dll's IServerImpl, in
// multiplayer RemoteServer.dll's, both behind the same IServer layout. The offsets below were read from the game's own
// files (12 Nov 2026): Server.dll exports the vtable `??_7IServerImpl@@6BIServer@@@` (188 slots) and each method under
// its mangled name; a slot is where the vtable holds the method's export. HDE 0.7.1's and vanilla 1.1's Server.dll give
// the same offsets. The mangled names fix the argument types; the arguments in the comments are the game's own call
// sites (docs\FREELANCER_RE.md "Screens: the server interface and the shared stores"). Connect and ReqCargo share one
// function body (slots 0x3C and 0x74): placed by the vtable's order against FLHook's SDK header (FLCoreServer.h).
// Calling a slot directly skips every gate the game's own caller keeps (trade's pending byte, save's docked test, a
// popup the server did not open): a caller keeps them, or the action stays borrowed (70b 2.3).
#pragma once
#include "common.h"

namespace iserver
{
// IServer, byte offsets into the vtable.
enum Slot : u32
{
    kConnect = 0x3C,            // (const char* address, ...) - shares its body with ReqCargo (0x74)
    kDisconnect = 0x40,         // (u32 client, EFLConnection) - "DisConnect" in the export
    kLogin = 0x48,              // (const SLoginInfo&, u32 client)
    kCharacterInfoReq = 0x4C,   // (u32 client, bool)
    kCharacterSelect = 0x50,    // (const CHARACTER_ID&, u32 client)
    kCreateNewCharacter = 0x58, // (const SCreateCharacterInfo&, u32 client)
    kDestroyCharacter = 0x5C,   // (const CHARACTER_ID&, u32 client)
    kReqAddItem = 0x78,         // (u32 arch, const char* hardpoint, int count, float health, bool mounted, u32 client)
    kReqRemoveItem = 0x7C,      // (u16 item id, int count, u32 client)
    kReqModifyItem = 0x80,      // (u16 item id, const char* hardpoint, int count, float health, bool mounted, u32 client)
    kReqSetCash = 0x84,         // (int cash, u32 client)
    kReqChangeCash = 0x88,      // (int delta, u32 client)
    kMissionResponse = 0xAC,    // (u32, unsigned long, bool accept, u32 client): the job board's (record +8, +0xC)
    kGFGoodBuy = 0xB4,          // (const SGFGoodBuyInfo& {base, 0, good, count}, u32 client)
    kGFGoodSell = 0xB8,         // (const SGFGoodSellInfo& {base, good, count}, u32 client)
    kJumpInComplete = 0xC8,     // (u32 system, u32 ship)
    kSaveGame = 0xE8,           // (const CHARACTER_ID&, const wchar_t* description, u32 client)
    kSetVisitedState = 0x11C,   // (u32 client, u8* {1, id, flags}, int 9)
    kRequestBestPath = 0x120,   // (u32 client, u8* request, int size)
    kPopUpDialog = 0x128,       // (u32 client, u32 button): only for a box the server opened
    kTerminateTrade = 0x140,    // (u32 client, int)
    kAcceptTrade = 0x144,       // (u32 client, bool)
    kSetTradeMoney = 0x148,     // (u32 client, unsigned long)
    kAddTradeEquip = 0x14C,     // (u32 client, const EquipDesc&)
    kDelTradeEquip = 0x150,     // (u32 client, const EquipDesc&)
    kRequestTrade = 0x154,      // (u32 client, u32 other client): the game sets byte [0x674B00] and sends one at a time
    kStopTradeRequest = 0x158,  // (u32 requester): one argument
};

// IChatServer: Server.dll's `??_7IServerImpl@@6BIChatServer@@@`, behind exe::ChatServerIface.
enum ChatSlot : u32
{
    kSubmitChat = 0x0, // (CHAT_ID from, u32 size, const void* rdl, CHAT_ID to, int -1)
};

// Each slot with its export (`?<name>@IServerImpl@@UAEX<args>`), for a check against a Server.dll in memory.
struct SlotExport
{
    u32 offset;
    const char* method;
    const char* mangled;
};
inline const SlotExport kExports[] = {
    {kConnect, "Connect", "?Connect@IServerImpl@@UAEXPBDPAG@Z"},
    {kDisconnect, "DisConnect", "?DisConnect@IServerImpl@@UAEXIW4EFLConnection@@@Z"},
    {kLogin, "Login", "?Login@IServerImpl@@UAEXABUSLoginInfo@@I@Z"},
    {kCharacterInfoReq, "CharacterInfoReq", "?CharacterInfoReq@IServerImpl@@UAEXI_N@Z"},
    {kCharacterSelect, "CharacterSelect", "?CharacterSelect@IServerImpl@@UAEXABUCHARACTER_ID@@I@Z"},
    {kCreateNewCharacter, "CreateNewCharacter", "?CreateNewCharacter@IServerImpl@@UAEXABUSCreateCharacterInfo@@I@Z"},
    {kDestroyCharacter, "DestroyCharacter", "?DestroyCharacter@IServerImpl@@UAEXABUCHARACTER_ID@@I@Z"},
    {kReqAddItem, "ReqAddItem", "?ReqAddItem@IServerImpl@@UAEXIPBDHM_NI@Z"},
    {kReqRemoveItem, "ReqRemoveItem", "?ReqRemoveItem@IServerImpl@@UAEXGHI@Z"},
    {kReqModifyItem, "ReqModifyItem", "?ReqModifyItem@IServerImpl@@UAEXGPBDHM_NI@Z"},
    {kReqSetCash, "ReqSetCash", "?ReqSetCash@IServerImpl@@UAEXHI@Z"},
    {kReqChangeCash, "ReqChangeCash", "?ReqChangeCash@IServerImpl@@UAEXHI@Z"},
    {kMissionResponse, "MissionResponse", "?MissionResponse@IServerImpl@@UAEXIK_NI@Z"},
    {kGFGoodBuy, "GFGoodBuy", "?GFGoodBuy@IServerImpl@@UAEXABUSGFGoodBuyInfo@@I@Z"},
    {kGFGoodSell, "GFGoodSell", "?GFGoodSell@IServerImpl@@UAEXABUSGFGoodSellInfo@@I@Z"},
    {kJumpInComplete, "JumpInComplete", "?JumpInComplete@IServerImpl@@UAEXII@Z"},
    {kSaveGame, "SaveGame", "?SaveGame@IServerImpl@@UAEXABUCHARACTER_ID@@PBGI@Z"},
    {kSetVisitedState, "SetVisitedState", "?SetVisitedState@IServerImpl@@UAEXIPAEH@Z"},
    {kRequestBestPath, "RequestBestPath", "?RequestBestPath@IServerImpl@@UAEXIPAEH@Z"},
    {kPopUpDialog, "PopUpDialog", "?PopUpDialog@IServerImpl@@UAEXII@Z"},
    {kTerminateTrade, "TerminateTrade", "?TerminateTrade@IServerImpl@@UAEXIH@Z"},
    {kAcceptTrade, "AcceptTrade", "?AcceptTrade@IServerImpl@@UAEXI_N@Z"},
    {kSetTradeMoney, "SetTradeMoney", "?SetTradeMoney@IServerImpl@@UAEXIK@Z"},
    {kAddTradeEquip, "AddTradeEquip", "?AddTradeEquip@IServerImpl@@UAEXIABUEquipDesc@@@Z"},
    {kDelTradeEquip, "DelTradeEquip", "?DelTradeEquip@IServerImpl@@UAEXIABUEquipDesc@@@Z"},
    {kRequestTrade, "RequestTrade", "?RequestTrade@IServerImpl@@UAEXII@Z"},
    {kStopTradeRequest, "StopTradeRequest", "?StopTradeRequest@IServerImpl@@UAEXI@Z"},
};
} // namespace iserver
