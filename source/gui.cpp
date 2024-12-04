#include <3ds.h>
#include <citro2d.h>
#include <cstdarg>
#include <cstdio>
#include <malloc.h>

#include "config.h"
#include "gui.h"
#include "hardware.h"
#include "log.h"
#include "service.h"
#include "textures.h"
#include "utils.h"

namespace GUI {
    enum {
        TARGET_TOP = 0,
        TARGET_BOTTOM,
        TARGET_MAX
    };

    enum PageState {
        KERNEL_INFO_PAGE = 0,
        SYSTEM_INFO_PAGE,
        BATTERY_INFO_PAGE,
        NNID_INFO_PAGE,
        CONFIG_INFO_PAGE,
        HARDWARE_INFO_PAGE,
        WIFI_INFO_PAGE,
        STORAGE_INFO_PAGE,
        MISC_INFO_PAGE,
        EXIT_PAGE,
        MAX_ITEMS
    };

    static C2D_Font font = NULL;
    static C3D_RenderTarget *c3dRenderTarget[TARGET_MAX];
    static C2D_TextBuf guiStaticBuf, guiDynamicBuf, guiSizeBuf;

    static const u32 guiBgcolour = C2D_Color32(62, 62, 62, 255);
    static const u32 guiStatusBarColour = C2D_Color32(44, 44, 44, 255);
    static const u32 guiMenuBarColour = C2D_Color32(52, 52, 52, 255);
    static const u32 guiSelectorColour = C2D_Color32(223, 74, 22, 255);
    static const u32 guiTitleColour = C2D_Color32(252, 252, 252, 255);
    static const u32 guiDescrColour = C2D_Color32(182, 182, 182, 255);
    
    static const u32 guiItemDistance = 20, guiItemHeight = 18, guiItemStartX = 15, guiItemStartY = 84;
    static const float guiTexSize = 0.5f;

    void Init(void) {
        romfsInit();
        gfxInitDefault();
        C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
        C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
        C2D_Prepare();

        font = C2D_FontLoad("romfs:/cbf_std.bcfnt");
        c3dRenderTarget[TARGET_TOP] = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
        c3dRenderTarget[TARGET_BOTTOM] = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

        guiStaticBuf  = C2D_TextBufNew(4096);
        guiDynamicBuf  = C2D_TextBufNew(4096);
        guiSizeBuf = C2D_TextBufNew(4096);

        Textures::Init();
#if defined BUILD_DEBUG
        Log::Open();
#endif
        // Real time services
#if !defined BUILD_CITRA
        mcuHwcInit();
#endif
        ptmuInit();
        cfguInit();
        dspInit();
        socInit(static_cast<u32 *>(memalign(0x1000, 0x10000)), 0x10000);
    }

    void Exit(void) {
        socExit();
        dspExit();
        cfguExit();
        ptmuExit();
#if !defined BUILD_CITRA
        mcuHwcExit();
#endif
#if defined BUILD_DEBUG
        Log::Close();
#endif
        Textures::Exit();
        C2D_TextBufDelete(guiSizeBuf);
        C2D_TextBufDelete(guiDynamicBuf);
        C2D_TextBufDelete(guiStaticBuf);
        C2D_FontFree(font);
        C2D_Fini();
        C3D_Fini();
        gfxExit();
        romfsExit();
    }
    
    static void Begin(u32 topScreenColour, u32 bottomScreenColour) {
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TargetClear(c3dRenderTarget[TARGET_TOP], topScreenColour);
        C2D_TargetClear(c3dRenderTarget[TARGET_BOTTOM], bottomScreenColour);
        C2D_SceneBegin(c3dRenderTarget[TARGET_TOP]);
    }

    static void End(void) {
        C2D_TextBufClear(guiDynamicBuf);
        C2D_TextBufClear(guiSizeBuf);
        C3D_FrameEnd(0);
    }

    static void GetTextDimensions(float size, float *width, float *height, const char *text) {
        C2D_Text c2dText;
        float scale = font ? size * 1.1f : size;
        C2D_TextParse(&c2dText, guiSizeBuf, text);
        C2D_TextGetDimensions(&c2dText, scale, scale, width, height);
    }

    static void DrawText(float x, float y, float size, u32 colour, const char *text) {
        C2D_Text c2dText;
        if (font)
            C2D_TextFontParse(&c2dText, font, guiDynamicBuf, text);
        else
            C2D_TextParse(&c2dText, guiDynamicBuf, text);
        C2D_TextOptimize(&c2dText);
        C2D_DrawText(&c2dText, C2D_WithColor, x, y, guiTexSize, size, size, colour);
    }

    static void DrawTextf(float x, float y, float size, u32 colour, const char* text, ...) {
        char buffer[128];
        va_list args;
        va_start(args, text);
        std::vsnprintf(buffer, 128, text, args);
        GUI::DrawText(x, y, size, colour, buffer);
        va_end(args);
    }

    static void DrawItem(float x, float y, const char *title, const char *text) {
        float titleWidth = 0.f;
        GUI::GetTextDimensions(guiTexSize, &titleWidth, nullptr, title);
        GUI::DrawText(x, y, guiTexSize, guiTitleColour, title);
        GUI::DrawText(x + titleWidth + 5, y, guiTexSize, guiDescrColour, text);
    }

    static void DrawItem(int index, const char *title, const char *text) {
        float titleWidth = 0.f;
        float y = guiItemStartY + ((guiItemDistance - guiItemHeight) / 2) + guiItemHeight * index;
        GUI::GetTextDimensions(guiTexSize, &titleWidth, nullptr, title);
        GUI::DrawText(guiItemStartX, y, guiTexSize, guiTitleColour, title);
        GUI::DrawText(guiItemStartX + titleWidth + 5, y, guiTexSize, guiDescrColour, text);
    }

    static void DrawItemf(int index, const char *title, const char *text, ...) {
        float titleWidth = 0.f;
        float y = guiItemStartY + ((guiItemDistance - guiItemHeight) / 2) + guiItemHeight * index;
        GUI::GetTextDimensions(guiTexSize, &titleWidth, nullptr, title);
        GUI::DrawText(guiItemStartX, y, guiTexSize, guiTitleColour, title);
        
        char buffer[256];
        va_list args;
        va_start(args, text);
        std::vsnprintf(buffer, 256, text, args);
        GUI::DrawText(guiItemStartX + titleWidth + 5, y, guiTexSize, guiDescrColour, buffer);
        va_end(args);
    }

    static bool DrawImage(C2D_Image image, float x, float y) {
        return C2D_DrawImageAt(image, x, y, guiTexSize, nullptr, 1.f, 1.f);
    }

    static bool DrawImageBlend(C2D_Image image, float x, float y, u32 colour) {
        C2D_ImageTint tint;
        C2D_PlainImageTint(std::addressof(tint), colour, 0.5f);
        return C2D_DrawImageAt(image, x, y, guiTexSize, std::addressof(tint), 1.f, 1.f);
    }

    static void KernelInfoPage(const KernelInfo &info, bool &displayInfo) {
        GUI::DrawItemf(1, "内核版本:", info.kernelVersion);
        GUI::DrawItem(2, "固件版本:", info.firmVersion);
        GUI::DrawItemf(3, "系统版本:", info.systemVersion);
        GUI::DrawItemf(4, "出厂系统版本:", info.initialVersion);
        GUI::DrawItemf(5, "SDMC CID:", displayInfo? info.sdmcCid : "");
        GUI::DrawItemf(6, "NAND CID:", displayInfo? info.nandCid : "");
        GUI::DrawItemf(7, "设备 ID:", "%llu", displayInfo? info.deviceId : 0);
    }

    static void SystemInfoPage(const SystemInfo &info, bool &displayInfo) {
        GUI::DrawItemf(1, "型号:", "%s (%s - %s)", info.model, info.hardware, info.region);
        GUI::DrawItem(2, "原始系统语言:", info.language);
        GUI::DrawItemf(3, "出厂 LFC 种子:", "%010llX", displayInfo? info.localFriendCodeSeed : 0);
        GUI::DrawItemf(4, "从 NAND 获取到的 LFC 种子:", "%s", displayInfo? info.nandLocalFriendCodeSeed : "");
        GUI::DrawItemf(5, "MAC 地址:", displayInfo? info.macAddress : "");
        GUI::DrawItemf(6, "序列号:", "%s %d", displayInfo? reinterpret_cast<const char *>(info.serialNumber) : "", displayInfo? info.checkDigit : 0);
        GUI::DrawItemf(7, "ECS 设备 ID:", "%llu", displayInfo? info.soapId : 0);
    }

    static void BatteryInfoPage(const SystemStateInfo &info) {
        Result ret = 0;
        u8 percentage = 0, status = 0, voltage = 0, fwVerHigh = 0, fwVerLow = 0, temp = 0;
        bool connected = false;

        ret = MCUHWC_GetBatteryLevel(std::addressof(percentage));
        Result chargeResult = PTMU_GetBatteryChargeState(std::addressof(status));
        GUI::DrawItemf(1, "电量百分比:", "%3d%% (%s)", R_FAILED(ret)? 0 : (percentage),
            R_FAILED(chargeResult)? "未知" : (status? "充电中" : "未在充电"));

        ret = MCUHWC_GetBatteryVoltage(std::addressof(voltage));
        GUI::DrawItemf(2, "电池电压:", "%d (%.1f V)", voltage, 5.f * (static_cast<float>(voltage) / 256.f));

        ret = MCUHWC::GetBatteryTemperature(std::addressof(temp));
        GUI::DrawItemf(3, "电池温度:", "%d °C (%d °F)", 
            R_FAILED(ret)? 0 : (temp), R_FAILED(ret)? 0 : static_cast<u8>((temp * 9) / 5 + 32));

        ret = PTMU_GetAdapterState(std::addressof(connected));
        GUI::DrawItemf(4, "适配器状态:", R_FAILED(ret)? "未知" : (connected? "已连接" : "未连接"));

        MCUHWC_GetFwVerHigh(std::addressof(fwVerHigh));
        MCUHWC_GetFwVerLow(std::addressof(fwVerLow));
        GUI::DrawItemf(5, "MCU 固件:", "%u.%u", (fwVerHigh - 0x10), fwVerLow);

        GUI::DrawItemf(6, "PMIC 厂商代码:", "%x", info.pmicVendorCode);

        GUI::DrawItemf(7, "电池厂商代码:", "%x", info.batteryVendorCode);
    }

    static void NNIDInfoPage(const NNIDInfo &info, bool &displayInfo) {
        GUI::DrawItemf(1, "持久化 ID:", "%u", displayInfo? info.persistentID : 0);
        GUI::DrawItemf(2, "可转移 ID 凭据:", "%llu", displayInfo? info.transferableIdBase : 0);
        GUI::DrawItemf(3, "主 ID:", "%u", displayInfo? info.principalID : 0);
        // The following are not functioning 
        // GUI::DrawItem(4, "Account ID:", info.accountId);
        // GUI::DrawItem(5, "Country:", displayInfo? info.countryName : "");
        // GUI::DrawItem(6, "NFS Password:", displayInfo? info.nfsPassword : "");
    }

    static void ConfigInfoPage(const ConfigInfo &info, bool &displayInfo) {
        GUI::DrawItem(1, "用户昵称:", info.username);
        GUI::DrawItem(2, "生日:", displayInfo? info.birthday : "");
        GUI::DrawItem(3, "EULA 版本:", info.eulaVersion);
        GUI::DrawItem(4, "家长控制 Pin 码:", displayInfo? info.parentalPin : "");
        GUI::DrawItem(5, "家长控制电子邮箱:", displayInfo? info.parentalEmail : "");
        GUI::DrawItem(6, "家长控制安全问题答案:", displayInfo? info.parentalSecretAnswer : "");
        GUI::DrawItem(7, "省电模式:", Config::GetPowersaveStatus());
    }

    static void HardwareInfoPage(const HardwareInfo &info, bool &isNew3DS) {
        GUI::DrawItem(1, "上屏类型:", info.screenUpper);
        GUI::DrawItem(2, "下屏类型:", info.screenLower);
        GUI::DrawItem(3, "耳机孔状态:", Hardware::GetAudioJackStatus()? "已插接" : "未插接");
        GUI::DrawItem(4, "卡带卡槽状态:", Hardware::GetCardSlotStatus()? "已插卡" : "未插卡");
        GUI::DrawItem(5, "SD卡槽状态:", Hardware::IsSdInserted()? "已插卡" : "未插卡");
        GUI::DrawItem(6, "音频输出:", info.soundOutputMode);
        
        if (isNew3DS) {
            GUI::DrawItemf(7, "亮度等级:", "%lu (自动亮度模式: %s)", Hardware::GetBrightness(GSPLCD_SCREEN_TOP), 
                Hardware::GetAutoBrightnessStatus());
        }
        else {
            GUI::DrawItemf(7, "亮度等级:", "%lu", Hardware::GetBrightness(GSPLCD_SCREEN_TOP));
        }
    }

    static void WifiInfoPage(const WifiInfo &info, bool &displayInfo) {
        const u32 slotDistance = 68;
        C2D_DrawRectSolid(0, 20, guiTexSize, 400, 220, guiBgcolour);

        for (int i = 0; i < 3; i++) {
            if (info.slot[i]) {
                C2D_DrawRectSolid(15, 27 + (i * slotDistance), guiTexSize, 370, 70, guiTitleColour);
                C2D_DrawRectSolid(16, 28 + (i * slotDistance), guiTexSize, 368, 68, guiStatusBarColour);
                GUI::DrawTextf(20, 30 + (i * slotDistance), guiTexSize, guiTitleColour, "WiFi 位 %d:", i + 1);
                GUI::DrawTextf(20, 46 + (i * slotDistance), guiTexSize, guiTitleColour, "SSID: %s", info.ssid[i]);
                GUI::DrawTextf(20, 62 + (i * slotDistance), guiTexSize, guiTitleColour, "密码: %s (%s)",
                    displayInfo? info.passphrase[i] : "", info.securityMode[i]);
            }
        }
    }

    static void StorageInfoPage(const StorageInfo &info) {
        C2D_DrawRectSolid(0, 20, guiTexSize, 400, 220, guiBgcolour);

        // SD info
        C2D_DrawRectSolid(20, 105, guiTexSize, 60, 10, guiTitleColour);
        C2D_DrawRectSolid(21, 106, guiTexSize, 58, 8, guiBgcolour);
        C2D_DrawRectSolid(21, 106, guiTexSize, ((static_cast<double>(info.usedSize[SYSTEM_MEDIATYPE_SD]) / static_cast<double>(info.totalSize[SYSTEM_MEDIATYPE_SD])) * 58.f), 8, guiSelectorColour);
        GUI::DrawItem(85, 50, "SD:", "");
        GUI::DrawItem(85, 71, "可用:", info.freeSizeString[SYSTEM_MEDIATYPE_SD]);
        GUI::DrawItem(85, 87, "已用:", info.usedSizeString[SYSTEM_MEDIATYPE_SD]);
        GUI::DrawItem(85, 103, "容量:", info.totalSizeString[SYSTEM_MEDIATYPE_SD]);
        GUI::DrawImage(driveIcon, 20, 40);
        
        // Nand info
        C2D_DrawRectSolid(220, 105, guiTexSize, 60, 10, guiTitleColour);
        C2D_DrawRectSolid(221, 106, guiTexSize, 58, 8, guiBgcolour);
        C2D_DrawRectSolid(221, 106, guiTexSize, ((static_cast<double>(info.usedSize[SYSTEM_MEDIATYPE_CTR_NAND]) / static_cast<double>(info.totalSize[SYSTEM_MEDIATYPE_CTR_NAND])) * 58.f), 8, guiSelectorColour);
        GUI::DrawItem(285, 50, "CTR Nand:", "");
        GUI::DrawItem(285, 71, "可用:", info.freeSizeString[SYSTEM_MEDIATYPE_CTR_NAND]);
        GUI::DrawItem(285, 87, "已用:", info.usedSizeString[SYSTEM_MEDIATYPE_CTR_NAND]);
        GUI::DrawItem(285, 103, "容量:", info.totalSizeString[SYSTEM_MEDIATYPE_CTR_NAND]);
        GUI::DrawImage(driveIcon, 220, 40);
        
        // TWL nand info
        C2D_DrawRectSolid(20, 200, guiTexSize, 60, 10, guiTitleColour);
        C2D_DrawRectSolid(21, 201, guiTexSize, 58, 8, guiBgcolour);
        C2D_DrawRectSolid(21, 201, guiTexSize, ((static_cast<double>(info.usedSize[SYSTEM_MEDIATYPE_TWL_NAND]) / static_cast<double>(info.totalSize[SYSTEM_MEDIATYPE_TWL_NAND])) * 58.f), 8, guiSelectorColour);
        GUI::DrawItem(85, 145, "TWL Nand:", "");
        GUI::DrawItem(85, 166, "可用:", info.freeSizeString[SYSTEM_MEDIATYPE_TWL_NAND]);
        GUI::DrawItem(85, 182, "已用:", info.usedSizeString[SYSTEM_MEDIATYPE_TWL_NAND]);
        GUI::DrawItem(85, 198, "容量:", info.totalSizeString[SYSTEM_MEDIATYPE_TWL_NAND]);
        GUI::DrawImage(driveIcon, 20, 135);

        // TWL photo info
        C2D_DrawRectSolid(220, 200, guiTexSize, 60, 10, guiTitleColour);
        C2D_DrawRectSolid(221, 201, guiTexSize, 58, 8, guiBgcolour);
        C2D_DrawRectSolid(221, 201, guiTexSize, ((static_cast<double>(info.usedSize[SYSTEM_MEDIATYPE_TWL_PHOTO]) / static_cast<double>(info.totalSize[SYSTEM_MEDIATYPE_TWL_PHOTO])) * 58.f), 8, guiSelectorColour);
        GUI::DrawItem(285, 145, "TWL Photo:", "");
        GUI::DrawItem(285, 166, "可用:", info.freeSizeString[SYSTEM_MEDIATYPE_TWL_PHOTO]);
        GUI::DrawItem(285, 182, "已用:", info.usedSizeString[SYSTEM_MEDIATYPE_TWL_PHOTO]);
        GUI::DrawItem(285, 198, "容量:", info.totalSizeString[SYSTEM_MEDIATYPE_TWL_PHOTO]);
        GUI::DrawImage(driveIcon, 220, 135);
    }

    static void MiscInfoPage(const MiscInfo &info, bool &displayInfo) {
        GUI::DrawItemf(1, "已安装内容:", "SD: %lu (NAND: %lu)", info.sdTitleCount, info.nandTitleCount);
        GUI::DrawItemf(2, "已安装票据:", "%lu", info.ticketCount);

        u8 wifiStrength = osGetWifiStrength();
        GUI::DrawItemf(3, "WiFi 信号强度:", "%d (%.0lf%%)", wifiStrength, static_cast<float>(wifiStrength * 33.33));
        
        char hostname[128];
        gethostname(hostname, sizeof(hostname));
        GUI::DrawItem(4, "IP:", displayInfo? hostname : "");
    }

    static void DrawControllerImage(int keys, C2D_Image button, int defaultX, int defaultY, int keyLeft, int keyRight, int keyUp, int keyDown) {
        int x = defaultX, y = defaultY;
        
        if (keys & keyLeft) {
            x -= 5;
        }
        else if (keys & keyRight) {
            x += 5;
        }
        else if (keys & keyUp) {
            y -= 5;
        }
        else if (keys & keyDown) {
            y += 5;
        }
        
        if (keys & (keyLeft | keyRight | keyUp | keyDown)) {
            GUI::DrawImageBlend(button, x, y, guiSelectorColour);
        }
        else {
            GUI::DrawImage(button, x, y);
        }
    }

    void ButtonTester(bool &enabled) {
        circlePosition circlePad, cStick;
        touchPosition touch;
        u16 touchX = 0, touchY = 0;
        u8 volume = 0;
        
        const u32 guiButtonTesterText = C2D_Color32(77, 76, 74, 255);
        const u32 guiButtonTesterSliderBorder = C2D_Color32(219, 219, 219, 255);
        const u32 guiButtonTesterSlider = C2D_Color32(241, 122, 74, 255);
        
        while (enabled) {
            hidScanInput();
            
            hidCircleRead(std::addressof(circlePad));
            hidCstickRead(std::addressof(cStick));
            
            u32 kDown = hidKeysDown();
            u32 kHeld = hidKeysHeld();
            
            HIDUSER_GetSoundVolume(std::addressof(volume));
            
            if (((kHeld & KEY_L) && (kDown & KEY_R)) || ((kHeld & KEY_R) && (kDown & KEY_L))) {
                aptSetHomeAllowed(true);
                enabled = false;
            }
            
            if (kHeld & KEY_TOUCH)  {
                hidTouchRead(&touch);
                touchX = touch.px;
                touchY = touch.py;
            }

            GUI::Begin(C2D_Color32(60, 61, 63, 255), C2D_Color32(94, 39, 80, 255));
            
            C2D_DrawRectSolid(75, 30, guiTexSize, 250, 210, C2D_Color32(97, 101, 104, 255));
            C2D_DrawRectSolid(85, 40, guiTexSize, 230, 175, C2D_Color32(242, 241, 239, 255));
            C2D_DrawRectSolid(85, 40, guiTexSize, 230, 15, C2D_Color32(66, 65, 61, 255));
            
            GUI::DrawText(90, 40, 0.45f, guiTitleColour, "3DSident 按键测试");
            
            GUI::DrawTextf(90, 56, 0.45f, guiButtonTesterText, "方向摇杆: %04d, %04d", circlePad.dx, circlePad.dy);
            GUI::DrawTextf(90, 70, 0.45f, guiButtonTesterText, "C 摇杆: %04d, %04d", cStick.dx, cStick.dy);
            GUI::DrawTextf(90, 84, 0.45f, guiButtonTesterText, "触摸位置: %03d, %03d", touch.px, touch.py);
            
            GUI::DrawImage(volumeIcon, 90, 98);
            double volPercent = (volume * 1.5873015873);
            C2D_DrawRectSolid(115, 104, guiTexSize, 190, 5, guiButtonTesterSliderBorder);
            C2D_DrawRectSolid(115, 104, guiTexSize, ((volPercent / 100) * 190), 5, guiButtonTesterSlider);
            
            GUI::DrawText(90, 118, 0.45f, guiButtonTesterText, "3D");
            double _3dSliderPercent = (osGet3DSliderState() * 100.0);
            C2D_DrawRectSolid(115, 122, guiTexSize, 190, 5, guiButtonTesterSliderBorder);
            C2D_DrawRectSolid(115, 122, guiTexSize, ((_3dSliderPercent / 100) * 190), 5, guiButtonTesterSlider);
            
            GUI::DrawText(90, 138, 0.45f, guiButtonTesterText, "按 L + R 以返回");

#if !defined BUILD_CITRA
            SystemStateInfo info = Service::GetSystemStateInfo();
            ((info.rawButtonState >> 1) & 1) == 0? GUI::DrawImageBlend(btnHome, 180, 215, guiSelectorColour): GUI::DrawImage(btnHome, 180, 215);
#else
            aptCheckHomePressRejected() ? GUI::DrawImageBlend(btnHome, 180, 215, guiSelectorColour) : GUI::DrawImage(btnHome, 180, 215);
#endif

            kHeld & KEY_L? GUI::DrawImageBlend(btnL, 0, 0, guiSelectorColour) : GUI::DrawImage(btnL, 0, 0);
            kHeld & KEY_R? GUI::DrawImageBlend(btnR, 345, 0, guiSelectorColour) : GUI::DrawImage(btnR, 345, 0);
            kHeld & KEY_ZL? GUI::DrawImageBlend(btnZL, 60, 0, guiSelectorColour) : GUI::DrawImage(btnZL, 60, 0);
            kHeld & KEY_ZR? GUI::DrawImageBlend(btnZR, 300, 0, guiSelectorColour) : GUI::DrawImage(btnZR, 300, 0);
            kHeld & KEY_A? GUI::DrawImageBlend(btnA, 370, 80, guiSelectorColour) : GUI::DrawImage(btnA, 370, 80);
            kHeld & KEY_B? GUI::DrawImageBlend(btnB, 350, 100, guiSelectorColour) : GUI::DrawImage(btnB, 350, 100);
            kHeld & KEY_X? GUI::DrawImageBlend(btnX, 350, 60, guiSelectorColour) : GUI::DrawImage(btnX, 350, 60);
            kHeld & KEY_Y? GUI::DrawImageBlend(btnY, 330, 80, guiSelectorColour) : GUI::DrawImage(btnY, 330, 80);
            kHeld & KEY_START? GUI::DrawImageBlend(btnStartSelect, 330, 140, guiSelectorColour) : GUI::DrawImage(btnStartSelect, 330, 140);
            kHeld & KEY_SELECT? GUI::DrawImageBlend(btnStartSelect, 330, 165, guiSelectorColour) : GUI::DrawImage(btnStartSelect, 330, 165);
            
            GUI::DrawControllerImage(kHeld, btnCpad, 8, 55, KEY_CPAD_LEFT, KEY_CPAD_RIGHT, KEY_CPAD_UP, KEY_CPAD_DOWN);
            GUI::DrawControllerImage(kHeld, btnDpad, 5, 110, KEY_DLEFT, KEY_DRIGHT, KEY_DUP, KEY_DDOWN);
            GUI::DrawControllerImage(kHeld, btnCstick, 330, 35, KEY_CSTICK_LEFT, KEY_CSTICK_RIGHT, KEY_CSTICK_UP, KEY_CSTICK_DOWN);
            
            C2D_SceneBegin(c3dRenderTarget[TARGET_BOTTOM]);
            GUI::DrawImage(cursor, touchX, touchY);
            GUI::End();
        }
    }

    void MainMenu(void) {
        int selection = 0;
        bool isNew3DS = Utils::IsNew3DS(), displayInfo = true, buttonTestEnabled = false;

        const char *items[] = {
            "内核",
            "系统",
            "电池",
            "NNID",
            "用户配置",
            "硬件",
            "Wi-Fi",
            "存储",
            "杂项",
            "退出"
        };

        float titleHeight = 0.f;
        GUI::GetTextDimensions(guiTexSize, nullptr, &titleHeight, "3DSident v0.0.0");

        Service::Init();
        KernelInfo kernelInfo = Service::GetKernelInfo();
        SystemInfo systemInfo = Service::GetSystemInfo();
        NNIDInfo nnidInfo = Service::GetNNIDInfo();
        ConfigInfo configInfo = Service::GetConfigInfo();
        HardwareInfo hardwareInfo = Service::GetHardwareInfo();
        WifiInfo wifiInfo = Service::GetWifiInfo();
        StorageInfo storageInfo = Service::GetStorageInfo();
        MiscInfo miscInfo = Service::GetMiscInfo();
        SystemStateInfo systemStateInfo = Service::GetSystemStateInfo();
        Service::Exit();

        while (aptMainLoop()) {
            GUI::Begin(guiBgcolour, guiBgcolour);

            C2D_DrawRectSolid(0, 0, guiTexSize, 400, 20, guiStatusBarColour);
            GUI::DrawTextf(5, (20 - titleHeight) / 2, guiTexSize, guiTitleColour, "3DSident v%d.%d.%dc", VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO);
            GUI::DrawImage(banner, (400 - banner.subtex->width) / 2, ((82 - banner.subtex->height) / 2) + 20);

            switch (selection) {
                case KERNEL_INFO_PAGE:
                    GUI::KernelInfoPage(kernelInfo, displayInfo);
                    break;

                case SYSTEM_INFO_PAGE:
                    GUI::SystemInfoPage(systemInfo, displayInfo);
                    break;

                case BATTERY_INFO_PAGE:
                    GUI::BatteryInfoPage(systemStateInfo);
                    break;

                case NNID_INFO_PAGE:
                    GUI::NNIDInfoPage(nnidInfo, displayInfo);
                    break;

                case CONFIG_INFO_PAGE:
                    GUI::ConfigInfoPage(configInfo, displayInfo);
                    break;

                case HARDWARE_INFO_PAGE:
                    GUI::HardwareInfoPage(hardwareInfo, isNew3DS);
                    break;

                case WIFI_INFO_PAGE:
                    GUI::WifiInfoPage(wifiInfo, displayInfo);
                    break;

                case STORAGE_INFO_PAGE:
                    GUI::StorageInfoPage(storageInfo);
                    break;

                case MISC_INFO_PAGE:
                    GUI::MiscInfoPage(miscInfo, displayInfo);
                    break;

                case EXIT_PAGE:
                    GUI::DrawItem(1, "按 select 键隐藏用户隐私信息", "");
                    GUI::DrawItem(2, "按 L + R 以启动按键测试", "");
                    break;

                default:
                    break;
            }

            C2D_SceneBegin(c3dRenderTarget[TARGET_BOTTOM]);
            
            C2D_DrawRectSolid(15, 15, guiTexSize, 290, 210, guiTitleColour);
            C2D_DrawRectSolid(16, 16, guiTexSize, 288, 208, guiMenuBarColour);
            C2D_DrawRectSolid(16, 16 + (guiItemDistance * selection), guiTexSize, 288, 18, guiSelectorColour);

            for (int i = 0; i < MAX_ITEMS; i++) {
                C2D_DrawImageAt(menuIcon[i], 20, 17 + ((guiItemDistance - guiItemHeight) / 2) + (guiItemDistance * i), guiTexSize, nullptr, 0.7f, 0.7f);
                GUI::DrawText(40, 17 + ((guiItemDistance - guiItemHeight) / 2) + (guiItemDistance * i), guiTexSize, guiTitleColour, items[i]);
            }
            
            GUI::End();
            GUI::ButtonTester(buttonTestEnabled);

            hidScanInput();
            u32 kDown = hidKeysDown();
            u32 kHeld = hidKeysHeld();

            if (kDown & KEY_DOWN) {
                selection++;
            }
            else if (kDown & KEY_UP) {
                selection--;
            }

            if (selection > EXIT_PAGE) {
                selection = KERNEL_INFO_PAGE;
            }
            if (selection < KERNEL_INFO_PAGE) {
                selection = EXIT_PAGE;
            }

            if (kDown & KEY_SELECT) {
                displayInfo = !displayInfo;
            }

            if (((kHeld & KEY_L) && (kDown & KEY_R)) || ((kHeld & KEY_R) && (kDown & KEY_L))) {
                aptSetHomeAllowed(false);
                buttonTestEnabled = true;
            }

            if ((kDown & KEY_START) || ((kDown & KEY_A) && (selection == EXIT_PAGE))) {
                break;
            }
        }
    }
}
