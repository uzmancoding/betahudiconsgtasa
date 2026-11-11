// BetaHudIcons.cpp
// GTA San Andreas PC v1.0 US - jetpack / cep telefonu HUD weapon ikonu (beta ozelligi)
//
// - Ikonlar oyunun kendi dosyalarindan alinir: jetpack.txd\jetpackicon, cellphone.txd\cellphoneicon
//   (gta3.img icinde, oyun kullanmaz). Istenirse yedek olarak BetaHudIcons.txd de kullanilabilir.
// - Orijinal weapon ikonu MinHook ile cizilirken yutulur ve koordinati yakalanir.
// - Kendi ikonumuz yakalanan koordinata birebir cizilir (INI'den scale/shift ile ince ayar).
// - Ikon HUD'dan ONCE cizilir, boylece mermi yazisi (outline'i ile) ustte kalir.
// - Jetpackte kullanilamayan silahin mermi sayisi gizlenir.
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
#include <vector>
#include <cstdlib>
#include <windows.h>

using namespace plugin;

class BetaHudIcons {
public:
    // Yedek (kendi txd'miz) ve oyundan odunc alinan sprite'lar
    static inline CSprite2d phoneSprite;       // BetaHudIcons.txd'den (yedek)
    static inline CSprite2d jetpackSprite;
    static inline CSprite2d gamePhoneSprite;   // oyunun txd'sinden (sahiplik YOK, her kare yeniden cozulur)
    static inline CSprite2d gameJetpackSprite;
    static inline int txdSlot = -1;
    static inline bool loaded = false;         // yedek txd yuklendi mi

    enum Kind { K_NONE, K_JETPACK, K_PHONE };
    static inline Kind curKind = K_NONE;

    // --- INI ayarlari ---
    // Texture kaynagi: 0 = otomatik (once oyun, olmazsa yedek txd), 1 = sadece oyun, 2 = sadece BetaHudIcons.txd
    static inline int   source = 0;
    static inline std::string phoneTxdName = "cellphone", phoneTexName = "cellphoneicon";
    static inline std::string jetTxdName   = "jetpack",   jetTexName   = "jetpackicon";

    // Yedek konum (orijinal rect yakalanamazsa) + orijinal ikonu ARAMA kutusu
    static inline float posX  = 322.0f;  // SAG kenara uzaklik (sanal birim)
    static inline float posY  = 44.0f;   // ustten uzaklik
    static inline float sizeW = 108.0f;
    static inline float sizeH = 132.0f;
    static inline bool  hideOriginal = true;
    // Orijinal rect'e gore ince ayar
    static inline bool  useOriginalRect = true;
    static inline float scaleW = 100.0f; // yuzde
    static inline float scaleH = 100.0f; // yuzde
    static inline float shiftX = 0.0f;   // sanal birim (+ = saga)
    static inline float shiftY = 0.0f;   // sanal birim (+ = asagi)

    // Jetpackteyken kullanilamayan silahin mermi sayisini gizle
    static inline bool hideAmmoUnusable = true;
    static inline std::vector<int> usableWeapons = { 22, 23, 24, 28, 32 }; // pistol, silenced, deagle, uzi, tec9
    static inline unsigned char savedSlot = 0;
    static inline bool swapped = false;

    // --- Durum ---
    static inline bool suppress = false;           // HUD cizilirken ve ikon aktifken true
    static inline bool  accValid = false;          // bu karede yakalanan rect'ler
    static inline float accL = 0, accT = 0, accR = 0, accB = 0;
    static inline bool  haveCap = false;           // son yakalanan orijinal ikon rect'i
    static inline float capL = 0, capT = 0, capR = 0, capB = 0;

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

    static float ReadF(const char* key, int def, const std::string& ini) {
        return (float)(int)GetPrivateProfileIntA("Icon", key, def, ini.c_str());
    }

    static std::string ReadS(const char* section, const char* key, const char* def, const std::string& ini) {
        char buf[128] = {};
        GetPrivateProfileStringA(section, key, def, buf, sizeof(buf), ini.c_str());
        return std::string(buf);
    }

    static void LoadSettings() {
        std::string ini = GetIniPath() + "BetaHudIcons.ini";
        posX  = ReadF("OffsetFromRight", 322, ini);
        posY  = ReadF("OffsetFromTop",   44,  ini);
        sizeW = ReadF("Width",  108, ini);
        sizeH = ReadF("Height", 132, ini);
        hideOriginal    = GetPrivateProfileIntA("Icon", "HideOriginal", 1, ini.c_str()) != 0;
        useOriginalRect = GetPrivateProfileIntA("Icon", "UseOriginalRect", 1, ini.c_str()) != 0;
        scaleW = ReadF("ScaleWidthPercent",  100, ini);
        scaleH = ReadF("ScaleHeightPercent", 100, ini);
        shiftX = ReadF("ShiftX", 0, ini);
        shiftY = ReadF("ShiftY", 0, ini);

        hideAmmoUnusable = GetPrivateProfileIntA("Jetpack", "HideAmmoWhenUnusable", 1, ini.c_str()) != 0;
        char buf[256] = {};
        GetPrivateProfileStringA("Jetpack", "UsableWeapons", "22,23,24,28,32", buf, sizeof(buf), ini.c_str());
        std::vector<int> list;
        for (char* tok = strtok(buf, ", "); tok; tok = strtok(nullptr, ", "))
            list.push_back(atoi(tok));
        if (!list.empty()) usableWeapons = list;

        source = (int)GetPrivateProfileIntA("Textures", "Source", 0, ini.c_str());
        phoneTxdName = ReadS("Textures", "PhoneTxd",     "cellphone",     ini);
        phoneTexName = ReadS("Textures", "PhoneTexture", "cellphoneicon", ini);
        jetTxdName   = ReadS("Textures", "JetpackTxd",     "jetpack",     ini);
        jetTexName   = ReadS("Textures", "JetpackTexture", "jetpackicon", ini);
    }

    static bool IsOurSprite(CSprite2d* s) {
        return s == &phoneSprite || s == &jetpackSprite || s == &gamePhoneSprite || s == &gameJetpackSprite;
    }

    // Ikon ARAMA kutusu icinde mi (INI konumu +- genis tolerans) ve ikon gibi buyuk mu?
    static bool LooksLikeWeaponIcon(float x1, float y1, float x2, float y2) {
        float l = (std::min)(x1, x2), r = (std::max)(x1, x2);
        float t = (std::min)(y1, y2), b = (std::max)(y1, y2);

        if ((r - l) < SCREEN_COORD(50.0f) || (b - t) < SCREEN_COORD(50.0f))
            return false; // kucuk seyleri (bar, yildiz vs.) yutma

        const float m = SCREEN_COORD(20.0f);
        float bx0 = SCREEN_COORD_RIGHT(posX) - m;
        float by0 = SCREEN_COORD_TOP(posY) - m;
        float bx1 = SCREEN_COORD_RIGHT(posX) + SCREEN_COORD(sizeW) + m;
        float by1 = SCREEN_COORD_TOP(posY) + SCREEN_COORD(sizeH) + m;
        return l >= bx0 && r <= bx1 && t >= by0 && b <= by1;
    }

    static void Accumulate(float x1, float y1, float x2, float y2) {
        float l = (std::min)(x1, x2), r = (std::max)(x1, x2);
        float t = (std::min)(y1, y2), b = (std::max)(y1, y2);
        if (!accValid) {
            accL = l; accT = t; accR = r; accB = b; accValid = true;
        } else {
            accL = (std::min)(accL, l); accT = (std::min)(accT, t);
            accR = (std::max)(accR, r); accB = (std::max)(accB, b);
        }
    }

    // --- Hook: CSprite2d::Draw(CRect const&, CRGBA const&) ---
    static void __fastcall DrawHook(CSprite2d* self, void* /*edx*/, CRect const& rect, CRGBA const& color) {
        if (suppress && !IsOurSprite(self)) {
            const float* f = reinterpret_cast<const float*>(&rect); // pozisyonel: x1, y1, x2, y2
            if (LooksLikeWeaponIcon(f[0], f[1], f[2], f[3])) {
                Accumulate(f[0], f[1], f[2], f[3]);
                return;
            }
        }
        origDraw(self, rect, color);
    }

    // --- Hook: CSprite::RenderOneXLUSprite(x,y,z,halfW,halfH,r,g,b,intensity,rhw,a,uDir,vDir) ---
    static void __cdecl XluHook(float x, float y, float z, float hw, float hh,
                                unsigned char r, unsigned char g, unsigned char b,
                                short intensity, float rhw, unsigned char a,
                                unsigned char uDir, unsigned char vDir) {
        if (suppress && LooksLikeWeaponIcon(x - hw, y - hh, x + hw, y + hh)) {
            Accumulate(x - hw, y - hh, x + hw, y + hh);
            return;
        }
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

    // Yedek: BetaHudIcons.txd (sadece dosya varsa ve kaynak "sadece oyun" degilse)
    static void LoadTextures() {
        if (source == 1) return;
        std::string txd = GetIniPath() + "BetaHudIcons.txd";
        if (GetFileAttributesA(txd.c_str()) == INVALID_FILE_ATTRIBUTES) return;

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

    // Oyunun kendi txd'sinden texture'i bul ve sprite'a ODUNC ver.
    // Not: txd'ler streaming ile yuklenir; jetpack/telefon aktifken modelleri yuklu oldugu icin
    // genelde hazirdir. Hazir degilse (dictionary yok) false doner, yedege dusulur.
    static bool ResolveGameTexture(Kind k, CSprite2d& spr) {
        const std::string& txd = (k == K_JETPACK) ? jetTxdName : phoneTxdName;
        const std::string& tex = (k == K_JETPACK) ? jetTexName : phoneTexName;

        int slot = CTxdStore::FindTxdSlot((char*)txd.c_str());
        if (slot < 0) return false;
        // Slotu gecici aktif yap, RW'dan gecerli sozlugu oku, eski haline dondur.
        // (Txd henuz streaming ile yuklenmediyse sozluk nullptr olur.)
        CTxdStore::PushCurrentTxd();
        CTxdStore::SetCurrentTxd(slot);
        RwTexDictionary* dict = RwTexDictionaryGetCurrent();
        RwTexture* t = dict ? RwTexDictionaryFindNamedTexture(dict, tex.c_str()) : nullptr;
        CTxdStore::PopCurrentTxd();
        if (!t) return false;
        spr.m_pTexture = t;                                 // DOGRULA: alan adi m_pTexture
        return true;
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

    // Su an hangi ikon gosterilmeli ve CIZILEBILIR mi? (nullptr = hicbiri; curKind'i de ayarlar)
    static CSprite2d* CurrentSprite() {
        curKind = K_NONE;
        CPlayerPed* player = FindPlayerPed(0);
        if (!player) return nullptr;

        Kind k = K_NONE;
        if (PlayerHasJetpack(player))      k = K_JETPACK;
        else if (PlayerUsingPhone(player)) k = K_PHONE;
        if (k == K_NONE) return nullptr;

        CSprite2d& game   = (k == K_JETPACK) ? gameJetpackSprite : gamePhoneSprite;
        CSprite2d& custom = (k == K_JETPACK) ? jetpackSprite     : phoneSprite;

        CSprite2d* result = nullptr;
        if (source != 2 && ResolveGameTexture(k, game)) result = &game;
        else if (source != 1 && loaded && custom.m_pTexture) result = &custom;

        if (result) curKind = k; // texture yoksa orijinali yutmayiz ve slotla da oynamayiz
        return result;
    }

    static void DrawIcon(CSprite2d* spr) {
        float x, y, w, h;
        if (useOriginalRect && haveCap) {
            // Orijinal ikonun gercek rect'i + yuzde olcek + kaydirma
            float cx = (capL + capR) * 0.5f, cy = (capT + capB) * 0.5f;
            w = (capR - capL) * scaleW / 100.0f;
            h = (capB - capT) * scaleH / 100.0f;
            x = cx - w * 0.5f + SCREEN_COORD(shiftX);
            y = cy - h * 0.5f + SCREEN_COORD(shiftY);
        } else {
            // Yedek: INI'deki sabit konum
            w = SCREEN_COORD(sizeW);
            h = SCREEN_COORD(sizeH);
            x = SCREEN_COORD_RIGHT(posX);
            y = SCREEN_COORD_TOP(posY);
        }
        spr->Draw(CRect(x, y, x + w, y + h), CRGBA(255, 255, 255, 255));

        // Oyundan odunc alinan texture'a sahip degiliz: cizim bitince birak (yikici cift silmeyi onler)
        if (spr == &gamePhoneSprite || spr == &gameJetpackSprite)
            spr->m_pTexture = nullptr;
    }

    // Ped yapisi ofsetleri (SA 1.0 US): aktif silah slotu 0x718 (dogrulandi), silah dizisi 0x5A0, her silah 0x1C bayt, ilk alan = silah tipi
    static unsigned char& ActiveSlot(CPlayerPed* p) {
        return *reinterpret_cast<unsigned char*>(reinterpret_cast<char*>(p) + 0x718);
    }
    static int WeaponTypeInSlot(CPlayerPed* p, int slot) {
        if (slot < 0 || slot > 12) return 0;
        return *reinterpret_cast<int*>(reinterpret_cast<char*>(p) + 0x5A0 + slot * 0x1C);
    }
    static bool IsUsableInJetpack(int type) {
        return std::find(usableWeapons.begin(), usableWeapons.end(), type) != usableWeapons.end();
    }

    // HUD cizilmeden HEMEN once
    static void BeforeHud() {
        swapped = false;
        accValid = false;
        CSprite2d* spr = CurrentSprite();
        suppress = hooksOk && spr != nullptr;
        if (!spr) { haveCap = false; return; }

        // Hook calisiyorsa ikonu HUD'dan once ciz: mermi yazisi (outline'i ile) ustte kalir.
        if (hooksOk) DrawIcon(spr);

        // Jetpackte kullanilamayan silah elindeyse: HUD cizimi boyunca slotu gecici "yumruk" yap,
        // boylece mermi sayisi cizilmez (yumruk ikonunu zaten hook yutuyor).
        if (hideAmmoUnusable && curKind == K_JETPACK) {
            CPlayerPed* p = FindPlayerPed(0);
            if (p) {
                unsigned char slot = ActiveSlot(p);
                int type = WeaponTypeInSlot(p, slot);
                if (slot != 0 && type != 0 && !IsUsableInJetpack(type)) {
                    savedSlot = slot;
                    ActiveSlot(p) = 0;
                    swapped = true;
                }
            }
        }
    }

    // HUD cizildikten HEMEN sonra
    static void AfterHud() {
        // Gecici slot degisikligini geri al
        if (swapped) {
            CPlayerPed* p = FindPlayerPed(0);
            if (p) ActiveSlot(p) = savedSlot;
            swapped = false;
        }

        bool wasSuppressing = suppress;
        suppress = false;

        if (wasSuppressing && accValid) {
            capL = accL; capT = accT; capR = accR; capB = accB;
            haveCap = true;
        }

        // Hook kurulamadiysa eski usul: HUD'dan sonra ustune ciz
        if (!hooksOk) {
            CSprite2d* spr = CurrentSprite();
            if (spr) DrawIcon(spr);
        }
    }

    BetaHudIcons() {
        LoadSettings();
        Events::initRwEvent += []() { LoadTextures(); InstallHooks(); };
        Events::drawHudEvent.before += []() { BeforeHud(); };
        Events::drawHudEvent.after  += []() { AfterHud(); };
        Events::shutdownRwEvent += []() {
            RemoveHooks();
            gamePhoneSprite.m_pTexture = nullptr;     // odunc, silme
            gameJetpackSprite.m_pTexture = nullptr;
            if (loaded) {
                phoneSprite.Delete();
                jetpackSprite.Delete();
                if (txdSlot >= 0) {
                    CTxdStore::RemoveRef(txdSlot);
                    CTxdStore::RemoveTxdSlot(txdSlot);
                }
            }
        };
    }
} betaHudIcons;
