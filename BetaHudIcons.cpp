// BetaHudIcons.cpp
// GTA San Andreas PC v1.0 US - jetpack / cep telefonu HUD weapon ikonu (beta ozelligi)
// Orijinal weapon ikonu, ikon kutusuna dusen cizimler MinHook ile engellenerek gizlenir.
//
// Hook'lanan fonksiyonlar (SA 1.0 US, MTA kaynaginda ayni adresler):
//   CSprite2d::Draw(CRect const&, CRGBA const&)        0x728350  (thiscall)
//   CSprite::RenderOneXLUSprite(...)                    0x70D000  (cdecl)

#include "plugin.h"
#include "CHud.h"
#include "CSprite2d.h"
#include "CTxdStore.h"
#include "CTimer.h"
#include "CWorld.h"
#include "CPed.h"
#include "CPlayerPed.h"
#include "CPedIntelligence.h"
#include "CAnimManager.h"
#include "CFont.h"
#include "common.h"

#include "MinHook.h"

#include <string>
#include <algorithm>
#include <windows.h>

using namespace plugin;

class BetaHudIcons {
public:
    static inline CSprite2d phoneSprite;
    static inline CSprite2d jetpackSprite;
    static inline int txdSlot = -1;
    static inline bool loaded = false;

    // INI ayarlari (sanal birim, SCREEN_COORD ile olceklenir)
    static inline float posX  = 317.0f;  // ikonun SAG kenara uzakligi
    static inline float posY  = 50.0f;   // ustten uzaklik
    static inline float sizeW = 100.0f;
    static inline float sizeH = 100.0f;
    static inline bool  hideOriginal = true;

    // HUD cizilirken ve jetpack/telefon aktifken true: orijinal ikon cizimleri yutulur
    static inline bool suppress = false;

    typedef void(__thiscall* Sprite2dDraw_t)(CSprite2d*, CRect const&, CRGBA const&);
    typedef void(__cdecl* RenderXLU_t)(float, float, float, float, float,
                                       unsigned char, unsigned char, unsigned char,
                                       short, float, unsigned char, unsigned char, unsigned char);
    static inline Sprite2dDraw_t origDraw = nullptr;
    static inline RenderXLU_t    origXlu  = nullptr;
    static inline bool hooksOk = false;

    static std::string GetIniPath() {
        char path[MAX_PATH];
        HMODULE hm = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)&GetIniPath, &hm);
        GetModuleFileNameA(hm, path, MAX_PATH);
        std::string p(path);
        return p.substr(0, p.find_last_of("\\/") + 1);
    }

    static void LoadSettings() {
        std::string ini = GetIniPath() + "BetaHudIcons.ini";
        posX  = (float)GetPrivateProfileIntA("Icon", "OffsetFromRight", 317, ini.c_str());
        posY  = (float)GetPrivateProfileIntA("Icon", "OffsetFromTop",   50,  ini.c_str());
        sizeW = (float)GetPrivateProfileIntA("Icon", "Width",  100, ini.c_str());
        sizeH = (float)GetPrivateProfileIntA("Icon", "Height", 100, ini.c_str());
        hideOriginal = GetPrivateProfileIntA("Icon", "HideOriginal", 1, ini.c_str()) != 0;
    }

    // Ekran koordinatinda ikon kutusu icinde mi? (kucuk bir tolerans payi ile)
    static bool InIconBox(float x1, float y1, float x2, float y2) {
        float l = (std::min)(x1, x2), r = (std::max)(x1, x2);
        float t = (std::min)(y1, y2), b = (std::max)(y1, y2);

        const float m = SCREEN_COORD(10.0f);
        float bx0 = SCREEN_COORD_RIGHT(posX) - m;
        float by0 = SCREEN_COORD_TOP(posY) - m;
        float bx1 = SCREEN_COORD_RIGHT(posX) + SCREEN_COORD(sizeW) + m;
        float by1 = SCREEN_COORD_TOP(posY) + SCREEN_COORD(sizeH) + m;

        return l >= bx0 && r <= bx1 && t >= by0 && b <= by1;
    }

    // --- Hook: CSprite2d::Draw(CRect const&, CRGBA const&) ---
    static void __fastcall DrawHook(CSprite2d* self, void* /*edx*/, CRect const& rect, CRGBA const& color) {
        if (suppress && self != &phoneSprite && self != &jetpackSprite) {
            const float* f = reinterpret_cast<const float*>(&rect); // pozisyonel: x1, y1, x2, y2
            if (InIconBox(f[0], f[1], f[2], f[3]))
                return;
        }
        origDraw(self, rect, color);
    }

    // --- Hook: CSprite::RenderOneXLUSprite(x,y,z,halfW,halfH,r,g,b,intensity,rhw,a,uDir,vDir) ---
    static void __cdecl XluHook(float x, float y, float z, float hw, float hh,
                                unsigned char r, unsigned char g, unsigned char b,
                                short intensity, float rhw, unsigned char a,
                                unsigned char uDir, unsigned char vDir) {
        if (suppress && InIconBox(x - hw, y - hh, x + hw, y + hh))
            return;
        origXlu(x, y, z, hw, hh, r, g, b, intensity, rhw, a, uDir, vDir);
    }

    static void InstallHooks() {
        if (!hideOriginal) return;
        if (MH_Initialize() != MH_OK) return;
        bool a = MH_CreateHook((LPVOID)0x728350, (LPVOID)&DrawHook, (LPVOID*)&origDraw) == MH_OK;
        bool b = MH_CreateHook((LPVOID)0x70D000, (LPVOID)&XluHook,  (LPVOID*)&origXlu)  == MH_OK;
        if (a || b) MH_EnableHook(MH_ALL_HOOKS);
        hooksOk = a || b;
    }

    static void RemoveHooks() {
        if (!hooksOk) return;
        suppress = false;
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        hooksOk = false;
    }

    // Oyun RW'yi baslattiktan sonra texture yukle
    static void LoadTextures() {
        std::string txd = GetIniPath() + "BetaHudIcons.txd";
        txdSlot = CTxdStore::AddTxdSlot("betahudicons");
        CTxdStore::LoadTxd(txdSlot, txd.c_str());
        CTxdStore::AddRef(txdSlot);
        CTxdStore::PushCurrentTxd();
        CTxdStore::SetCurrentTxd(txdSlot);
        phoneSprite.SetTexture((char*)"phone_icon");
        jetpackSprite.SetTexture((char*)"jetpack_icon");
        CTxdStore::PopCurrentTxd();
        loaded = true;
    }

    static bool PlayerHasJetpack(CPlayerPed* p) {
        return p->m_pIntelligence && p->m_pIntelligence->GetTaskJetPack() != nullptr;
    }

    static bool PlayerUsingPhone(CPlayerPed* p) {
        if (!p->m_pRwClump) return false;
        return RpAnimBlendClumpGetAssociation(p->m_pRwClump, "phone_in")   != nullptr
            || RpAnimBlendClumpGetAssociation(p->m_pRwClump, "phone_talk") != nullptr
            || RpAnimBlendClumpGetAssociation(p->m_pRwClump, "phone_out")  != nullptr;
    }

    // Su an hangi ikon gosterilmeli? (nullptr = hicbiri)
    static CSprite2d* CurrentSprite() {
        if (!loaded) return nullptr;
        CPlayerPed* player = FindPlayerPed(0);
        if (!player) return nullptr;
        if (PlayerHasJetpack(player))      return &jetpackSprite;
        if (PlayerUsingPhone(player))      return &phoneSprite;
        return nullptr;
    }

    // HUD cizilmeden hemen once: orijinal ikon cizimlerini yutmaya basla
    static void BeforeHud() {
        suppress = hooksOk && CurrentSprite() != nullptr;
    }

    // HUD cizildikten hemen sonra: yutmayi birak, kendi ikonumuzu ciz
    static void AfterHud() {
        suppress = false;

        CSprite2d* spr = CurrentSprite();
        if (!spr) return;

        float w = SCREEN_COORD(sizeW);
        float h = SCREEN_COORD(sizeH);
        float x = SCREEN_COORD_RIGHT(posX);
        float y = SCREEN_COORD_TOP(posY);

        spr->Draw(CRect(x, y, x + w, y + h), CRGBA(255, 255, 255, 255));
    }

    BetaHudIcons() {
        LoadSettings();
        Events::initRwEvent += []() { LoadTextures(); InstallHooks(); };
        Events::drawHudEvent.before += []() { BeforeHud(); };
        Events::drawHudEvent.after  += []() { AfterHud(); };
        Events::shutdownRwEvent += []() {
            RemoveHooks();
            phoneSprite.Delete();
            jetpackSprite.Delete();
            if (txdSlot >= 0) {
                CTxdStore::RemoveRef(txdSlot);
                CTxdStore::RemoveTxdSlot(txdSlot);
            }
        };
    }
} betaHudIcons;
