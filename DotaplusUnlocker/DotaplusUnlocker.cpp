#include <BlackBone/Asm/AsmFactory.h>
#include <BlackBone/Asm/AsmHelper64.h>
#include <BlackBone/Asm/IAsmHelper.h>
#include <BlackBone/Patterns/PatternSearch.h>
#include <BlackBone/Process/Process.h>
#include <BlackBone/Process/ProcessMemory.h>
#include <BlackBone/Process/RPC/RemoteFunction.hpp>
#include <BlackBone/Process/RPC/RemoteHook.h>
#include <iostream>
#include <vector>
#include <string>
#include "Color.h"

class CrimsonUI {
public:
    void SetTitle(const std::string& title);
    void ShowLogo() const;
};

void CrimsonUI::SetTitle(const std::string& title) {
    system(("title " + title).c_str());
}

void CrimsonUI::ShowLogo() const {
    std::cout << dye::purple(
        "   _____      _                              \n"
        "  / ____|    (_)                             \n"
        " | |     _ __ _ _ __ ___  ___  ___  _ __     \n"
        " | |    | '__| | '_ ` _ \\/ __|/ _ \\| '_ \\ \n"
        " | |____| |  | | | | | | \\__ \\ (_) | | | | \n"
        "  \\_____|_|  |_|_| |_| |_|___/\\___/|_| |_| \n"
    ) << std::endl;
}

// Константы для смещений
constexpr int32_t CGC_OFFSET = 0x598;
constexpr int32_t PTR_18_OFFSET = 0x18;
constexpr int32_t CACHE_ZERO_CHECK = 0x0;
constexpr int32_t CACHE_FAIL_CHECK = 0x48;

constexpr int32_t GAME_ACCOUNT_PLUS_OFFSET = 0x20;
constexpr int32_t DOTAPLUS_OFFSET = 0x2C;
constexpr const char* ACHIEVEMENT_DESCRIPTION = "#DOTA_WinterMajor2016_Achievement_Win_10000xp_Wagering_Description";

blackbone::ptr_t GetCGCClientSharedObjectCache(blackbone::ProcessMemory& memory, blackbone::ptr_t CDOTAGCLIENTSYSTEM) {
    bool notFoundMessagePrinted = false;

    while (true) {
        auto CGCClientSharedObjectCache = memory.Read<blackbone::ptr_t>(CDOTAGCLIENTSYSTEM + CGC_OFFSET).result();
        auto zero_check = memory.Read<blackbone::ptr_t>(CGCClientSharedObjectCache + CACHE_ZERO_CHECK).result();
        auto failed_check = memory.Read<blackbone::ptr_t>(CGCClientSharedObjectCache + CACHE_FAIL_CHECK).result();

        if (zero_check == 0 && failed_check == 0) {
            if (!notFoundMessagePrinted) {
                std::cout << dye::yellow("CGCClientSharedObjectCache not found. Retrying...") << std::endl;
                notFoundMessagePrinted = true;
            }
            Sleep(200);
        }
        else {
            std::cout << dye::light_green("CGCClientSharedObjectCache successfully found.") << std::endl;
            return CGCClientSharedObjectCache;
        }
    }
}

void ProcessDotaplus(blackbone::ProcessMemory& memory, blackbone::ptr_t CGCClientSharedObjectCache) {
    while (true) {
        auto ptr_18 = memory.Read<blackbone::ptr_t>(CGCClientSharedObjectCache + PTR_18_OFFSET).result();
        auto CGCClientSharedObjectTypeCache = memory.Read<blackbone::ptr_t>(ptr_18 + PTR_18_OFFSET).result();
        auto CDOTAGameAccountPlus = memory.Read<blackbone::ptr_t>(CGCClientSharedObjectTypeCache + GAME_ACCOUNT_PLUS_OFFSET).result();

        std::cout << dye::light_blue("CDOTAGameAccountPlus address: 0x") << dye::yellow(std::hex) << CDOTAGameAccountPlus << std::dec << std::endl;

        uint32_t DotaplusValue = memory.Read<uint32_t>(CDOTAGameAccountPlus + DOTAPLUS_OFFSET).result();
        std::cout << dye::light_blue("Dotaplus value at offset 0x2C: ") << dye::green(DotaplusValue) << std::endl;

        char description_buffer[256] = { 0 };
        memory.Read(CDOTAGameAccountPlus + 0x51, sizeof(description_buffer), description_buffer);

        if (DotaplusValue == 0) {
            memory.Write<uint32_t>(CDOTAGameAccountPlus + DOTAPLUS_OFFSET, 2);
            std::cout << dye::light_green("Modified Dotaplus value from 0 to 2!") << std::endl;
        }
        else if (DotaplusValue == 1 || DotaplusValue == 2) {
            std::cout << dye::yellow("Dotaplus is already enabled.") << std::endl;
        }
        else {
            std::cout << dye::red("Unexpected Dotaplus value: ") << dye::green(DotaplusValue) << std::endl;
        }
        break;
    }
}
    


void Dotaplus(blackbone::Process& dota_proc, blackbone::ProcessMemory& memory) {
    auto client_module = dota_proc.modules().GetModule(L"client.dll")->baseAddress;
    auto client_size = dota_proc.modules().GetModule(L"client.dll")->size;

    blackbone::PatternSearch pattern_search{
        "\x48\x8D\x0D\xCC\xCC\xCC\xCC\xE8\xCC\xCC\xCC\xCC\xB9\xCC\xCC\xCC\xCC\xE8\xCC\xCC\xCC\xCC\x48\x8B\xD8"
    };

    std::vector<blackbone::ptr_t> search_result;
    pattern_search.SearchRemote(dota_proc, 0xCC, client_module, client_size, search_result, 1);

    if (!search_result.empty()) {
        auto base_addr = search_result[0];
        std::cout << dye::light_blue("Pattern found at address: 0x") << dye::yellow(std::hex) << base_addr << std::dec << std::endl;

        int32_t relative_offset = 0;
        memory.Read(base_addr + 3, sizeof(relative_offset), &relative_offset);
        blackbone::ptr_t CDOTAGCLIENTSYSTEM = base_addr + relative_offset + 7;

        auto CGCClientSharedObjectCache = GetCGCClientSharedObjectCache(memory, CDOTAGCLIENTSYSTEM);
        ProcessDotaplus(memory, CGCClientSharedObjectCache);
    }
    else {
        std::cout << dye::red("Pattern not found!") << std::endl;
    }
}

int main() {
    CrimsonUI ui;
    ui.SetTitle("Crimson Software");
    ui.ShowLogo();

    blackbone::Process dota;
    bool notFoundMessagePrinted = false;

    while (true) {
        if (NT_SUCCESS(dota.Attach(L"dota2.exe")) && dota.modules().GetModule(L"client.dll")) {
            std::cout << dye::light_green("Successfully attached to dota2.exe!") << std::endl;
            Dotaplus(dota, dota.memory());
            break;
        }
        else {
            if (!notFoundMessagePrinted) {
                std::cout << dye::red("dota2.exe or client.dll not found, retrying...") << std::endl;
                notFoundMessagePrinted = true;
            }
            Sleep(200);
        }
    }

    std::cout << dye::light_blue("Press Enter to exit...") << std::endl;
    std::cin.get();
    return 0;
}
