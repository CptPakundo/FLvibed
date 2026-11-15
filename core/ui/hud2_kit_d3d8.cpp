// HUD 2's drawing kit, its game side (hud2_kit_backend.h): the fonts, faces and icons from core\game, the six vertex
// lists drawn with Direct3D 8 - one draw call per layer, the device's state saved before and restored after.
#include "hud2_kit_backend.h"
#include "hud2_kit_fldl.h"

#include "game/flicons_decode.h"
#include "game/gamefont.h"
#include "game/render.h"
#include "hook.h"

namespace hud2kit
{
namespace backend
{
namespace dev
{
constexpr int BeginScene = 34, EndScene = 35, SetViewport = 40, SetRenderState = 50, ApplyStateBlock = 54,
              CaptureStateBlock = 55, DeleteStateBlock = 56, CreateStateBlock = 57, SetTexture = 61,
              SetTextureStageState = 63, DrawPrimitiveUP = 72, SetVertexShader = 76, SetPixelShader = 88;
}
constexpr u32 kFvf = 0x004 | 0x040 | 0x100; // XYZRHW | DIFFUSE | TEX1

static void* g_dev = nullptr;
static u32 g_stateBlock = 0;

// ---------------------------------------------------------------- KitAssets
bool GameFont() { return gamefont::Load(); }
const wchar_t* LabelFace() { return gamefont::LabelFace(); }
bool BuildFont(render::Font& font, const wchar_t* face, int px, int weight, const std::wstring& chars)
{
    return render::BuildFont(font, face, px, weight, chars);
}
void FreeFont(render::Font& font) { render::FreeFont(font); }
bool IconUv(flicons::Icon icon, float sizePx, void** texture, float uv[4]) { return flicons::Uv(icon, sizePx, texture, uv); }

bool Describe(const void* texture, fldl::Texture& out)
{
    // a font render::BuildFont filled: its glyph boxes from its table (the atlas fontraster packed), its fingerprint as
    // `hud2 fontdump` writes it; named without the bench's .n (one pass holds one build of a face, px and weight)
    if (const render::Font* f = render::FontOfTexture(texture))
    {
        out = fldl::Texture();
        out.font = true;
        out.set = fldl::SetName(f->face, f->px, f->weight);
        out.w = f->texW, out.h = f->texH;
        for (const auto& [c, g] : f->glyphs)
            if (g.w > 0 && g.h > 0)
            {
                char hex[16];
                snprintf(hex, sizeof hex, "U+%04X", static_cast<unsigned>(c));
                out.boxes.push_back({"glyph:" + out.set + ":" + hex, static_cast<int>(lroundf(g.u0 * f->texW)),
                                     static_cast<int>(lroundf(g.v0 * f->texH)), g.w, g.h});
            }
        out.face = f->face, out.px = f->px, out.weight = f->weight, out.ascent = f->ascent, out.lineHeight = f->lineHeight;
        out.source = f->face == gamefont::LabelFace() ? gamefont::LabelSource() : 2;
        out.print = render::FontPrintOf(*f);
        return true;
    }
    // an icon atlas: the one flicons gives for a small draw, or for a large one
    for (const bool smallAtlas : {false, true})
    {
        void* t = nullptr;
        float uv[4];
        if (flicons::Uv(flicons::Icon(0), smallAtlas ? 8.0f : 512.0f, &t, uv) && t == texture)
            return fldl::IconAtlas(smallAtlas ? flicons::kSmall : flicons::kBig, out), true;
    }
    return false;
}

// ---------------------------------------------------------------- KitBackend
void Bind(void* device)
{
    if (device != g_dev)
        g_stateBlock = 0;
    g_dev = device;
    render::SetDevice(device);
}

static void SetRS(u32 s, u32 v) { VCall<HRESULT>(g_dev, dev::SetRenderState, s, v); }
static void SetTSS(u32 stage, u32 t, u32 v) { VCall<HRESULT>(g_dev, dev::SetTextureStageState, stage, t, v); }

void Submit(const Layer* layers, int count, int sw, int sh, bool keepDestAlpha, u32& calls)
{
    if (!g_stateBlock && FAILED(VCall<HRESULT>(g_dev, dev::CreateStateBlock, 1u /*D3DSBT_ALL*/, &g_stateBlock)))
        g_stateBlock = 0;
    if (g_stateBlock)
        VCall<HRESULT>(g_dev, dev::CaptureStateBlock, g_stateBlock);
    struct
    {
        u32 x, y, w, h;
        float minZ, maxZ;
    } vp = {0, 0, static_cast<u32>(sw), static_cast<u32>(sh), 0.0f, 1.0f};
    VCall<HRESULT>(g_dev, dev::SetViewport, static_cast<void*>(&vp));
    SetRS(7, 0);     // ZENABLE
    SetRS(14, 0);    // ZWRITEENABLE
    SetRS(8, 3);     // FILLMODE solid
    SetRS(15, 0);    // ALPHATESTENABLE
    SetRS(27, 1);    // ALPHABLENDENABLE
    SetRS(19, 5);    // SRCBLEND srcalpha
    SetRS(20, 6);    // DESTBLEND invsrcalpha
    SetRS(171, 1);   // BLENDOP add
    SetRS(22, 1);    // CULLMODE none
    SetRS(28, 0);    // FOGENABLE
    SetRS(29, 0);    // SPECULARENABLE
    SetRS(52, 0);    // STENCILENABLE
    SetRS(137, 0);   // LIGHTING
    SetRS(152, 0);   // CLIPPLANEENABLE
    SetRS(168, keepDestAlpha ? 0x7 : 0xF); // COLORWRITEENABLE (rgb only: the back buffer's alpha left as it was)
    VCall<HRESULT>(g_dev, dev::SetPixelShader, 0u);
    VCall<HRESULT>(g_dev, dev::SetVertexShader, kFvf);
    VCall<HRESULT>(g_dev, dev::SetTexture, 1u, static_cast<void*>(nullptr));
    SetTSS(0, 1, 4);  // COLOROP modulate
    SetTSS(0, 2, 2);  // COLORARG1 texture
    SetTSS(0, 3, 0);  // COLORARG2 diffuse
    SetTSS(0, 4, 4);  // ALPHAOP modulate
    SetTSS(0, 5, 2);  // ALPHAARG1 texture
    SetTSS(0, 6, 0);  // ALPHAARG2 diffuse
    SetTSS(0, 11, 0); // TEXCOORDINDEX
    SetTSS(0, 24, 0); // TEXTURETRANSFORMFLAGS
    SetTSS(0, 13, 3); // ADDRESSU clamp
    SetTSS(0, 14, 3); // ADDRESSV clamp
    SetTSS(0, 16, 2); // MAGFILTER linear
    SetTSS(0, 17, 2); // MINFILTER linear
    SetTSS(0, 18, 0); // MIPFILTER none
    SetTSS(1, 1, 1);  // stage 1 COLOROP disable
    SetTSS(1, 4, 1);  // stage 1 ALPHAOP disable
    for (int li = 0; li < count; ++li)
    {
        const Layer& l = layers[li];
        if (l.v.empty() || !l.tex)
            continue;
        // the flat icons in the HUD tint (the critique, round 3: the maneuver icons read multicoloured): their colour is
        // the vertex colour alone, the shape the texture's alpha; shapes and text modulate (the white texel / the font)
        const bool icon = li == LIconBig || li == LIconSmall;
        SetTSS(0, 1, icon ? 3 : 4); // COLOROP selectarg2 (diffuse) / modulate
        VCall<HRESULT>(g_dev, dev::SetTexture, 0u, l.tex);
        VCall<HRESULT>(g_dev, dev::DrawPrimitiveUP, 4u /*TRIANGLELIST*/, static_cast<UINT>(l.v.size() / 3),
                       static_cast<const void*>(l.v.data()), static_cast<UINT>(sizeof(Vertex)));
        ++calls;
    }
    if (g_stateBlock)
        VCall<HRESULT>(g_dev, dev::ApplyStateBlock, g_stateBlock);
}

void DeviceLost()
{
    if (g_dev && g_stateBlock)
        VCall<HRESULT>(g_dev, dev::DeleteStateBlock, g_stateBlock);
    g_stateBlock = 0;
}
} // namespace backend
} // namespace hud2kit
