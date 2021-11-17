/*
 * Copyright (C) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "translate.h"
#include "host_updater.h"

namespace Hdc {
namespace TranslateCommand {
    string Usage()
    {
        string ret;

        ret = "\n                         OpenHarmony flash ...\n\n"
              "---------------------------------global commands:----------------------------------\n"
              " -h/help                               - Print flashd help\n"
              " -v/version                            - Print flashd version\n"
              " -l 0-5                                - Set runtime loglevel\n"
              " -t connectkey                         - Use device with given connect key\n"
              "\n"
              "---------------------------------component commands:-------------------------------\n"
              "session commands(on server):\n"
              " discover                              - Discover devices listening on TCP via LAN broadcast\n"
              " list targets [-v]                     - List all devices status, -v for detail\n"
              " tconn key                             - Connect device via key, TCP use ip:port\n"
              "                                         example:192.168.0.100:10178/192.168.0.100\n"
              "                                         USB connect automatic, TCP need to connect manually\n"
              " start [-r]                            - Start server. If with '-r', will be restart server\n"
              " kill [-r]                             - Kill server. If with '-r', will be restart server\n"
              " -s [ip:]port                          - Set flash server listen config\n"
              "\n"
              "service commands(on daemon):\n"
              " target boot [-bootloader|-recovery]   - Reboot the device or boot into bootloader\\recovery.\n"
              " tmode usb                             - Reboot the device, listening on USB\n"
              " tmode port [port]                     - Reboot the device, listening on TCP port\n"
              "\n"
              "---------------------------------flash commands:-------------------------------------\n"
              "flash commands:\n"
              " update packagename                    - Update system by package\n"
              " flash [-f] partition imagename        - Flash partition by image\n"
              " erase [-f] partition                  - Erase partition\n"
              " format [-f] partition -t fs_type      - Format partition -t [ext4 | f2fs]\n";
        return ret;
    }

    string TargetConnect(FormatCommand *outCmd)
    {
        string stringError;
        if (Base::StringEndsWith(outCmd->parameters, " -remove")) {
            outCmd->parameters = outCmd->parameters.substr(0, outCmd->parameters.size() - 8); // 8 leng
            outCmd->cmdFlag = CMD_KERNEL_TARGET_DISCONNECT;
        } else {
            outCmd->cmdFlag = CMD_KERNEL_TARGET_CONNECT;
            if (outCmd->parameters.size() > 22) {  // 22: tcp max=21,USB max=8bytes
                stringError = "Error connect key's size";
                outCmd->bJumpDo = true;
            }
        }
        if (outCmd->parameters.find(":") != std::string::npos) {
            // tcp mode
            string ip = outCmd->parameters.substr(0, outCmd->parameters.find(":"));
            string sport = outCmd->parameters.substr(outCmd->parameters.find(":") + 1);
            int port = std::stoi(sport);
            sockaddr_in addr;
            if ((port <= 0 || port > MAX_IP_PORT) || uv_ip4_addr(ip.c_str(), port, &addr) < 0) {
                stringError = "IP:Port incorrect";
                outCmd->bJumpDo = true;
            }
        }
        return stringError;
    }

    string ForwardPort(const char *input, FormatCommand *outCmd)
    {
        string stringError;
        const char *pExtra = input + 6;  // CMDSTR_FORWARD_FPORT CMDSTR_FORWARD_RPORT + " " size
        if (!strcmp(pExtra, "ls")) {
            outCmd->cmdFlag = CMD_FORWARD_LIST;
        } else if (!strncmp(pExtra, "rm", 2)) { // 2 rm size
            outCmd->cmdFlag = CMD_FORWARD_REMOVE;
            if (strcmp(pExtra, "rm")) {
                outCmd->parameters = input + 9; // 9 rm extra size
            }
        } else {
            const char *p = input + 6; // 6 length
            // clang-format off
            if (strncmp(p, "tcp:", 4) && // 4 legnth of tcp
                strncmp(p, "localabstract:", 14) && // 14 legnth of localabstract
                strncmp(p, "localreserved:", 14) && // 14 legnth of localreserved
                strncmp(p, "localfilesystem:", 16) && // 14 legnth of localfilesystem
                strncmp(p, "dev:", 4) && // 4 legnth of dev
                strncmp(p, "jdwp:", 5)) { // 14 legnth of jdwp
                stringError = "Incorrect forward command";
                outCmd->bJumpDo = true;
            }
            // clang-format on
            outCmd->cmdFlag = CMD_FORWARD_INIT;
            outCmd->parameters = input;
        }
        return stringError;
    }

    string RunMode(const char *input, FormatCommand *outCmd)
    {
        string stringError;
        outCmd->cmdFlag = CMD_UNITY_RUNMODE;
        outCmd->parameters = input + CMDSTR_TARGET_MODE.size() + 1;  // with  ' '
        if (!strncmp(outCmd->parameters.c_str(), "port", 4) // 4 port len
            && !strcmp(outCmd->parameters.c_str(), CMDSTR_TMODE_USB.c_str())) {
            stringError = "Error tmode command";
            outCmd->bJumpDo = true;
        } else if (!strncmp(outCmd->parameters.c_str(), "port ", 5)) { // 5 port len
            int port = atoi(input + 4); // 4 port len
            if (port > MAX_IP_PORT || port <= 0) {
                stringError = "Incorrect port range";
                outCmd->bJumpDo = true;
            }
        }
        return stringError;
    }

    string TargetReboot(const char *input, FormatCommand *outCmd)
    {
        string stringError;
        outCmd->cmdFlag = CMD_UNITY_REBOOT;
        if (strcmp(input, CMDSTR_TARGET_REBOOT.c_str())) {
            outCmd->parameters = input + 12; // 12 length of target boot
            if (outCmd->parameters != "-bootloader"
                && outCmd->parameters != "-recovery" && outCmd->parameters != "-flashd") {
                stringError = "Error reboot paramenter";
                outCmd->bJumpDo = true;
            } else {
                outCmd->parameters.erase(outCmd->parameters.begin());
            }
        }
        return stringError;
    }

    // command input
    // client side:Enter string data formatting conversion to module see internal processing command
    string String2FormatCommand(const char *inputRaw, int sizeInputRaw, FormatCommand *outCmd)
    {
        string stringError;
        string input = string(inputRaw, sizeInputRaw);
        if (!strcmp(input.c_str(), CMDSTR_SOFTWARE_HELP.c_str())) {
            outCmd->cmdFlag = CMD_KERNEL_HELP;
            stringError = Usage();
            outCmd->bJumpDo = true;
        } else if (!strcmp(input.c_str(), CMDSTR_SOFTWARE_VERSION.c_str())) {
            outCmd->cmdFlag = CMD_KERNEL_HELP;
            stringError = Base::GetVersion();
            outCmd->bJumpDo = true;
        } else if (!strcmp(input.c_str(), CMDSTR_TARGET_DISCOVER.c_str())) {
            outCmd->cmdFlag = CMD_KERNEL_TARGET_DISCOVER;
        } else if (!strncmp(input.c_str(), CMDSTR_LIST_TARGETS.c_str(), CMDSTR_LIST_TARGETS.size())) {
            outCmd->cmdFlag = CMD_KERNEL_TARGET_LIST;
            if (strstr(input.c_str(), " -v")) {
                outCmd->parameters = "v";
            }
        } else if (!strcmp(input.c_str(), CMDSTR_CONNECT_ANY.c_str())) {
            outCmd->cmdFlag = CMD_KERNEL_TARGET_ANY;
        } else if (!strncmp(input.c_str(), CMDSTR_CONNECT_TARGET.c_str(), CMDSTR_CONNECT_TARGET.size())) {
            outCmd->parameters = input.c_str() + CMDSTR_CONNECT_TARGET.size() + 1;  // with ' '
            stringError = TargetConnect(outCmd);
        } else if (!strncmp(input.c_str(), (CMDSTR_SHELL + " ").c_str(), CMDSTR_SHELL.size() + 1)) {
            outCmd->cmdFlag = CMD_UNITY_EXECUTE;
            outCmd->parameters = input.c_str() + CMDSTR_SHELL.size() + 1;
        } else if (!strcmp(input.c_str(), CMDSTR_SHELL.c_str())) {
            outCmd->cmdFlag = CMD_SHELL_INIT;
        } else if (!strncmp(input.c_str(), CMDSTR_FILE_SEND.c_str(), CMDSTR_FILE_SEND.size())
                   || !strncmp(input.c_str(), CMDSTR_FILE_RECV.c_str(), CMDSTR_FILE_RECV.size())) {
            outCmd->cmdFlag = CMD_FILE_INIT;
            outCmd->parameters = input.c_str() + 5;  // 5: CMDSTR_FORWARD_FPORT CMDSTR_FORWARD_RPORT size
        } else if (!strncmp(input.c_str(), string(CMDSTR_FORWARD_FPORT + " ").c_str(), CMDSTR_FORWARD_FPORT.size() + 1)
                   || !strncmp(input.c_str(), string(CMDSTR_FORWARD_RPORT + " ").c_str(),
                               CMDSTR_FORWARD_RPORT.size() + 1)) {
            stringError = ForwardPort(input.c_str(), outCmd);
        } else if (!strcmp(input.c_str(), CMDSTR_KILL_SERVER.c_str())) {
            outCmd->cmdFlag = CMD_KERNEL_SERVER_KILL;
        } else if (!strcmp(input.c_str(), CMDSTR_KILL_DAEMON.c_str())) {
            outCmd->cmdFlag = CMD_UNITY_TERMINATE;
            outCmd->parameters = "0";
        } else if (!strncmp(input.c_str(), CMDSTR_APP_INSTALL.c_str(), CMDSTR_APP_INSTALL.size())) {
            outCmd->cmdFlag = CMD_APP_INIT;
            outCmd->parameters = input;
        } else if (!strncmp(input.c_str(), CMDSTR_APP_UNINSTALL.c_str(), CMDSTR_APP_UNINSTALL.size())) {
            outCmd->cmdFlag = CMD_APP_UNINSTALL;
            outCmd->parameters = input;
            if (outCmd->parameters.size() > 512 || outCmd->parameters.size() < 4) { // 512 4 max and min name length
                stringError = "Package's path incorrect";
                outCmd->bJumpDo = true;
            }
        } else if (!strcmp(input.c_str(), CMDSTR_TARGET_MOUNT.c_str())) {
            outCmd->cmdFlag = CMD_UNITY_REMOUNT;
        } else if (!strcmp(input.c_str(), CMDSTR_LIST_JDWP.c_str())) {
            outCmd->cmdFlag = CMD_UNITY_JPID;
        } else if (!strncmp(input.c_str(), CMDSTR_TARGET_REBOOT.c_str(), CMDSTR_TARGET_REBOOT.size())) {
            TargetReboot(input.c_str(), outCmd);
        } else if (!strncmp(input.c_str(), CMDSTR_TARGET_MODE.c_str(), CMDSTR_TARGET_MODE.size())) {
            RunMode(input.c_str(), outCmd);
        } else if (!strcmp(input.c_str(), CMDSTR_CONNECT_ANY.c_str())) {
            outCmd->cmdFlag = CMD_KERNEL_TARGET_ANY;
        } else if (!strncmp(input.c_str(), CMDSTR_HILOG.c_str(), CMDSTR_HILOG.size())) {
            outCmd->cmdFlag = CMD_UNITY_HILOG;
            if (strstr(input.c_str(), " -v")) {
                outCmd->parameters = "v";
            }
        } else if (!strncmp(input.c_str(), CMDSTR_STARTUP_MODE.c_str(), CMDSTR_STARTUP_MODE.size())) {
            outCmd->cmdFlag = CMD_UNITY_ROOTRUN;
            if (strstr(input.c_str(), " -r")) {
                outCmd->parameters = "r";
            }
        } else if (!strncmp(input.c_str(), CMDSTR_APP_SIDELOAD.c_str(), CMDSTR_APP_SIDELOAD.size())) {
            if (strlen(input.c_str()) == CMDSTR_APP_SIDELOAD.size()) {
                stringError = "Incorrect command, please with local path";
                outCmd->bJumpDo = true;
            }
            outCmd->cmdFlag = CMD_APP_SIDELOAD;
            outCmd->parameters = input;
        } else if (!strncmp(input.c_str(), CMDSTR_BUGREPORT.c_str(), CMDSTR_BUGREPORT.size())) {
            outCmd->cmdFlag = CMD_UNITY_BUGREPORT_INIT;
            outCmd->parameters = input;
            if (outCmd->parameters.size() == CMDSTR_BUGREPORT.size()) {
                outCmd->parameters += " ";
            }
        } else if (input == CMDSTR_INNER_ENABLE_KEEPALIVE) { // Inner command, protocol uses only
            outCmd->cmdFlag = CMD_KERNEL_ENABLE_KEEPALIVE;
        } else if (HostUpdater::CheckMatchUpdate(input, stringError, outCmd->cmdFlag, outCmd->bJumpDo)) {
            outCmd->parameters = input;
        } else {
            stringError = "Unknown command...";
            outCmd->bJumpDo = true;
        }
        // nl
        if (stringError.size()) {
            stringError += "\n";
        }
        return stringError;
    };
}
}  // namespace Hdc
