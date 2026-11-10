// BetaHudIcons.cpp
// GTA San Andreas PC v1.0 US - jetpack / cep telefonu HUD weapon ikonu (beta ozelligi)
// Gerekenler: plugin-sdk (https://github.com/DK22Pac/plugin-sdk), Visual Studio, x86 (Win32), Release
// Derleyince cikan .dll'in uzantisini .asi yap, oyun klasorune at (ASI Loader sart).
//
// NOT: Bu kodu ben burada derleyemedim (network kapali). Derlerken hata cikarsa
// asagidaki "DOGRULA" yazan yerler ilk supheliler. Hata mesajini bana at, duzeltiriz.

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

#include <string>
#include <windows.h>

using namespace plugin;

class BetaHudIcons {
public:
    static inline CSprite2d phoneSprite;
    static inline CSprite2d jetpackSprite;
    static inline int txdSlot = -1;
    static inline bool loaded = false;

    // INI'den okunan ayarlar (1920x1080 degil, 640x448 sanal koordinat; SCREEN_COORD kullaniyoruz)
    static inline float posX  = 94.0f;   // ikonun SAG kenara uzakligi (sanal birim)
    static inline float posY  = 27.0f;   // ustten uzaklik
    static inline float sizeW = 60.0f;
    static inline float sizeH = 60.0f;

    // Jetpack/telefon aktifken orijinal silah ikonunu + mermi yazisini gizle (HUD cizimi boyunca slotu gecici "yumruk" yapar)
    static inline bool hideOriginal = true;
    static inline unsigned char savedSlot = 0;
    static inline bool swapped = false;

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
        posX  = (float)GetPrivateProfileIntA("Icon", "OffsetFromRight", 94, ini.c_str());
        posY  = (float)GetPrivateProfileIntA("Icon", "OffsetFromTop",   27, ini.c_str());
        sizeW = (float)GetPrivateProfileIntA("Icon", "Width",  60, ini.c_str());
        sizeH = (float)GetPrivateProfileIntA("Icon", "Height", 60, ini.c_str());
        hideOriginal = GetPrivateProfileIntA("Icon", "HideOriginal", 1, ini.c_str()) != 0;
    }

    // HUD cizilmeden HEMEN once: orijinal ikon/mermi yazisi cizilmesin diye slotu gecici degistir.
    // DOGRULA: m_nActiveWeaponSlot alan adi plugin-sdk surumune gore farkli olabilir.
    static void BeforeHud() {
        swapped = false;
        if (!hideOriginal) return;
        CPlayerPed* p = FindPlayerPed(0);
        if (!p) return;
        if (!PlayerHasJetpack(p) && !PlayerUsingPhone(p)) return;
        savedSlot = p->m_nActiveWeaponSlot;
        p->m_nActiveWeaponSlot = 0;
        swapped = true;
    }

    // HUD cizildikten HEMEN sonra: slotu geri koy, sonra kendi ikonumuzu ciz.
    static void AfterHud() {
        if (swapped) {
            CPlayerPed* p = FindPlayerPed(0);
            if (p) p->m_nActiveWeaponSlot = savedSlot;
            swapped = false;
        }
        DrawIcons();
    }

    // Oyun RW'yi baslattiktan sonra texture yukle
    static void LoadTextures() {
        std::string txd = GetIniPath() + "BetaHudIcons.txd";
        txdSlot = CTxdStore::AddTxdSlot("betahudicons");
        CTxdStore::LoadTxd(txdSlot, txd.c_str());
        CTxdStore::AddRef(txdSlot);
        CTxdStore::PushCurrentTxd();
        CTxdStore::SetCurrentTxd(txdSlot);
        phoneSprite.SetTexture((char*)"phone_icon");     // txd icindeki texture isimleri
        jetpackSprite.SetTexture((char*)"jetpack_icon");
        CTxdStore::PopCurrentTxd();
        loaded = true;
    }

    static bool PlayerHasJetpack(CPlayerPed* p) {
        // DOGRULA: plugin-sdk'de CPedIntelligence::GetTaskJetPack() var, yoksa
        // m_TaskMgr.FindActiveTaskByType(TASK_SIMPLE_JETPACK) kullan.
        return p->m_pIntelligence && p->m_pIntelligence->GetTaskJetPack() != nullptr;
    }

    static bool PlayerUsingPhone(CPlayerPed* p) {
        // Cep telefonu animasyonlari "ped" blogunda: phone_in, phone_talk, phone_out
        // DOGRULA: RpAnimBlendClumpGetAssociation imzasi plugin-sdk surumune gore degisebilir.
        if (!p->m_pRwClump) return false;
        return RpAnimBlendClumpGetAssociation(p->m_pRwClump, "phone_in")   != nullptr
            || RpAnimBlendClumpGetAssociation(p->m_pRwClump, "phone_talk") != nullptr
            || RpAnimBlendClumpGetAssociation(p->m_pRwClump, "phone_out")  != nullptr;
    }

    static void DrawIcons() {
        if (!loaded) return;

        CPlayerPed* player = FindPlayerPed(0);
        if (!player) return;

        CSprite2d* spr = nullptr;
        if (PlayerHasJetpack(player))       spr = &jetpackSprite;
        else if (PlayerUsingPhone(player))  spr = &phoneSprite;
        if (!spr) return;

        // Sag ust kose, weapon ikonunun oldugu yer. Widescreen-uyumlu SCREEN_COORD makrolari:
        float w = SCREEN_COORD(sizeW);
        float h = SCREEN_COORD(sizeH);
        float x = SCREEN_COORD_RIGHT(posX);
        float y = SCREEN_COORD_TOP(posY);

        spr->Draw(CRect(x, y, x + w, y + h), CRGBA(255, 255, 255, 255));
    }

    BetaHudIcons() {
        LoadSettings();
        Events::initRwEvent += []() { LoadTextures(); };
        // DOGRULA: .before/.after plugin-sdk'de yoksa derleme hatasi verir, haber ver.
        Events::drawHudEvent.before += []() { BeforeHud(); };
        Events::drawHudEvent.after  += []() { AfterHud(); };
        Events::shutdownRwEvent += []() {
            phoneSprite.Delete();
            jetpackSprite.Delete();
            if (txdSlot >= 0) {
                CTxdStore::RemoveRef(txdSlot);
                CTxdStore::RemoveTxdSlot(txdSlot);
            }
        };
    }
} betaHudIcons;
