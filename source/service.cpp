#include <3ds.h>

#include "config.h"
#include "hardware.h"
#include "kernel.h"
#include "misc.h"
#include "service.h"
#include "storage.h"
#include "system.h"
#include "utils.h"

namespace MCUHWC {
    Result GetBatteryTemperature(u8 *temp) {
        Result ret = 0;
        u32 *cmdbuf = getThreadCommandBuffer();
        
        cmdbuf[0] = IPC_MakeHeader(0xE,2,0); // 0x000E0080
        
        if (R_FAILED(ret = svcSendSyncRequest(*mcuHwcGetSessionHandle()))) {
            return ret;
        }
        
        *temp = cmdbuf[2];
        return static_cast<Result>(cmdbuf[1]);
    }
}

namespace Service {
    void Init(void) {
        amInit();
        acInit();
        cfguInit();
    }

    void Exit(void) {
        acExit();
        cfguExit();
        amExit();
    }

    KernelInfo GetKernelInfo(void) {
        KernelInfo info = { 0 };
        info.kernelVersion = Kernel::GetVersion(VERSION_INFO_KERNEL);
        info.firmVersion = Kernel::GetVersion(VERSION_INFO_FIRM);
        info.systemVersion = Kernel::GetVersion(VERSION_INFO_SYSTEM);
        info.initialVersion = Kernel::GetInitalVersion();
        info.sdmcCid = Kernel::GetSdmcCid();
        info.nandCid = Kernel::GetNandCid();
        info.deviceId = Kernel::GetDeviceId();
        return info;
    }

    SystemInfo GetSystemInfo(void) {
        SystemInfo info = { 0 };
        info.model = System::GetModel();
        info.hardware = System::GetRunningHW();
        info.region = System::GetRegion();
        info.language = System::GetLanguage();
        info.localFriendCodeSeed = System::GetLocalFriendCodeSeed();
        info.macAddress = System::GetMacAddress();
        info.serialNumber = System::GetSerialNumber();
        return info;
	}

    ConfigInfo GetConfigInfo(void) {
        ConfigInfo info = { 0 };
        info.username = Config::GetUsername();
        info.birthday = Config::GetBirthday();
        info.eulaVersion = Config::GetEulaVersion();
        info.parentalPin = Config::GetParentalPin();
        info.parentalEmail = Config::GetParentalEmail();
        info.parentalSecretAnswer = Config::GetParentalSecretAnswer();
        return info;
    }

    HardwareInfo GetHardwareInfo(void) {
        HardwareInfo info = { 0 };

        gspLcdScreenType top, bottom;
        Hardware::GetScreenType(top, bottom);

        if (top == GSPLCD_SCREEN_UNK) {
            info.screenUpper = "unknown";
        }
        else {
            info.screenUpper = (top == GSPLCD_SCREEN_TN)? "TN" : "IPS";
        }

        if (bottom == GSPLCD_SCREEN_UNK) {
            info.screenLower = "unknown";
        }
        else {
            info.screenLower = (bottom == GSPLCD_SCREEN_TN)? "TN" : "IPS";
        }

        info.soundOutputMode = Hardware::GetSoundOutputMode();
        return info;
    }

    MiscInfo GetMiscInfo(void) {
        MiscInfo info = { 0 };
        info.sdTitleCount = Misc::GetTitleCount(MEDIATYPE_SD);
        info.nandTitleCount = Misc::GetTitleCount(MEDIATYPE_NAND);
        info.ticketCount = Misc::GetTicketCount();
        return info;
    }

    StorageInfo GetStorageInfo(void) {
        StorageInfo info = { 0 };
        
        for (int i = 0; i < 4; i++) {
            info.usedSize[i] = Storage::GetUsedStorage(static_cast<FS_SystemMediaType>(i));
            info.totalSize[i] = Storage::GetTotalStorage(static_cast<FS_SystemMediaType>(i));
            
            Utils::GetSizeString(info.freeSizeString[i], Storage::GetFreeStorage(static_cast<FS_SystemMediaType>(i)));
            Utils::GetSizeString(info.usedSizeString[i], Storage::GetUsedStorage(static_cast<FS_SystemMediaType>(i)));
            Utils::GetSizeString(info.totalSizeString[i], Storage::GetTotalStorage(static_cast<FS_SystemMediaType>(i)));
        }

        return info;
    }
}
