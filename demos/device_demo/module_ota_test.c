/*
 * Copyright (c) 2022-2024 Huawei Cloud Computing Technology Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 *    of conditions and the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include "iota_init.h"
#include "iota_datatrans.h"
#include "string_util.h"
#include "log_util.h"
#include "iota_cfg.h"


// You can get the access address from IoT Console "Overview" -> "Access Information"
char *g_address = "XXXX"; 
char *g_port = "8883";

// deviceId, the mqtt protocol requires the user name to be filled in.
// Please fill in the deviceId
char *g_deviceId = "XXXX"; 
char *g_secret = "XXXX";

char *g_module = "mcu"; // Module name
char *g_ersion = "v1.2.3"; // Module version

void TimeSleep(int ms)
{
#if defined(WIN32) || defined(WIN64)
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

void MyPrintLog(int level, char *format, va_list args)
{
    vprintf(format, args);
    /*
     * if you want to printf log in system log files,you can do this:
     * vsyslog(level, format, args);
     */
}

// ------------------ report device firmware or software version -----------------------
void Test_ReportModuleOTAVersion(char *module, char *version, char *event_id)
{
    ST_IOTA_MODULE_OTA_VERSION_INFO otaVersion;
    otaVersion.event_time = NULL;
    otaVersion.event_id = event_id;
    otaVersion.module = module;
    otaVersion.version = version;
    otaVersion.object_device_id = NULL;

    int messageId = IOTA_ModuleOtaVersionReport(otaVersion, NULL);
    if (messageId < 0) {
        PrintfLog(EN_LOG_LEVEL_ERROR, "module_ota_test: Test_ReportOTAVersion() failed, messageId %d\n", messageId);
    }
}

// ------------------ Proactively obtain module upgrade packages -----------------------
static void Test_GetPackage(char *module, char *event_id)
{
    ST_IOTA_MODULE_GET_PACKAGE_INFO info;
    info.event_id = event_id;
    info.event_time = NULL;
    info.module = module;
    info.object_device_id = NULL;

    int messageId = IOTA_ModuleOtaPackageGet(info, NULL);
    if (messageId < 0) {
        PrintfLog(EN_LOG_LEVEL_ERROR, "module_ota_test: Test_GetPackage() failed, messageId %d\n", messageId);
    }
}

// ---------------- report device upgraded status ---------------------
// success equals to 0 meaning success, others meaning failures
void Test_ReportUpgradeStatus(int success, char *module, char *event_id)
{
    ST_IOTA_MODULE_UPGRADE_STATUS_INFO statusInfo;
    statusInfo.result_code = success;
    statusInfo.progress = 100;
    statusInfo.module = module;
    statusInfo.description = (success == 0) ? "success" : "failed";

    statusInfo.event_time = NULL;
    statusInfo.event_id = event_id;
    statusInfo.object_device_id = NULL;

    int messageId = IOTA_ModuleOtaStatusReport(statusInfo, NULL);
    if (messageId < 0) {
        PrintfLog(EN_LOG_LEVEL_ERROR, "module_ota_test: Test_ReportUpgradeStatus() failed, messageId %d\n", messageId);
    }
}
// ---------------- OTA implementation process -------------------------
static void HandleEvenModuleOtaVersion(char *objectDeviceId,  *paras)
{
    if (paras->code == 200) {
        PrintfLog(EN_LOG_LEVEL_DEBUG, "module_ota_test: Module OTA version reported successfully\n");
    } else {
        PrintfLog(EN_LOG_LEVEL_ERROR, "module_ota_test: Module OTA version reported failed, error:%s\n", paras->error_detail);
    }
}

// ---------------- OTA implementation process -------------------------
static void HandleEvenModuleOtaUrlResponse(char *objectDeviceId, EN_IOTA_MODULE_UPGRADE_PARAS *ota_paras)
{
    /* The following is an example of OTA, please modify according to your needs */

    // start to receive firmware_upgrade or software_upgrade packages
    // Store file packages in ./${filename}中
    char filename[PKGNAME_MAX + 1];
    if (IOTA_GetOTAPackages_Ext(ota_paras->url, NULL, 1000, ".", filename) == 0) {
        
        TimeSleep(3000);

        // Verify if the package is complete
        if (!strcmp(ota_paras->sign_method, "SHA256") && IOTA_OTAVerifySign(ota_paras->sign, ".", filename) != 0) {
            Test_ReportUpgradeStatus(-1, ota_paras->module, objectDeviceId); 
            PrintfLog(EN_LOG_LEVEL_ERROR, "File verification failed, file may be incomplete. the filename is %s\n", filename);
            return;
        }
        
        // report successful upgrade status
        Test_ReportUpgradeStatus(0, ota_paras->module, objectDeviceId);
        PrintfLog(EN_LOG_LEVEL_INFO, "Module OTA package downloaded successfully, filename:%s\n", filename);

        // For demonstration purposes only.
        Test_ReportModuleOTAVersion(ota_paras->module, ota_paras->version, objectDeviceId);
        return;
    }

    // Failed to retrieve package
    PrintfLog(EN_LOG_LEVEL_ERROR, "File verification failed, file may be incomplete. the filename is %s\n", filename);
    Test_ReportUpgradeStatus(-1, ota_paras->module, objectDeviceId);
}
    
static void HandleEvenModuleOtaProgressReport(char *objectDeviceId, EN_IOTA_MODULE_REPORT_PARAS *paras)
{
    if (paras->code == 200) {
        PrintfLog(EN_LOG_LEVEL_DEBUG, "module_ota_test: Module OTA progress reported successfully\n");
    } else {
        PrintfLog(EN_LOG_LEVEL_ERROR, "module_ota_test: Module OTA progress reported failed, error:%s\n", paras->error_detail);
    }
}
    
static void HandleEvenModuleOtaGetPackage(char *objectDeviceId, EN_IOTA_MODULE_GET_PACKAGE_PARAS *paras)
{
    if (paras->report_result->code == 200) {
        PrintfLog(EN_LOG_LEVEL_DEBUG, "module_ota_test: Module OTA get package reported successfully\n");
        HandleEvenModuleOtaUrlResponse(objectDeviceId, paras->upgrade_paras);
    } else {
        PrintfLog(EN_LOG_LEVEL_ERROR, "module_ota_test: Module OTA get package reported failed, error:%s\n", paras->report_result->error_detail);
    }
}
    
// ---------------------------- secret authentication --------------------------------------
static void mqttDeviceSecretInit(char *address, char *port, char *deviceId, char *password) 
{
    IOTA_Init("."); // The certificate address is ./conf/rootcert.pem
    IOTA_SetPrintLogCallback(MyPrintLog); 
 
    // MQTT protocol when the connection parameter is 1883; MQTTS protocol when the connection parameter is 8883
    IOTA_ConnectConfigSet(address, port, deviceId, password);
    // Set authentication method to secret authentication
    IOTA_ConfigSetUint(EN_IOTA_CFG_AUTH_MODE, EN_IOTA_CFG_AUTH_MODE_SECRET);

    // Set connection callback function
    IOTA_DefaultCallbackInit();
}

static void HandleSubscribesuccess(EN_IOTA_MQTT_PROTOCOL_RSP *rsp)
{
    // You can write the actions that need to be taken after a successful subscription here
    
    // Report the current version
    char *event_id = "40cc9ab1-3579-488c-95c6-c18941c99eb4";
    Test_ReportModuleOTAVersion(g_module, g_version, event_id);
}

int main(int argc, char **argv) 
{
    // secret authentication initialization
    mqttDeviceSecretInit(g_address, g_port, g_deviceId, g_secret); 
    IOTA_SetProtocolCallback(EN_IOTA_CALLBACK_SUBSCRIBE_SUCCESS, HandleSubscribesuccess);

    // Module ota upgrade callback
    TagModuleOtaOps tag = {
        .onVersionUpReport = HandleEvenModuleOtaVersion,
        .onUrlResponse = HandleEvenModuleOtaUrlResponse,
        .onProgressReport = HandleEvenModuleOtaProgressReport,
        .onGetPackage = HandleEvenModuleOtaGetPackage
    };
    IOTA_SetEvenModuleOtaCallback(tag);

    int ret = IOTA_Connect();
    if (ret != 0) {
        PrintfLog(EN_LOG_LEVEL_ERROR, "ota_test: IOTA_Connect() error, Auth failed, result %d\n", ret);
        return -1;
    }
    TimeSleep(1500);

    // Actively obtain OTA package
    char *event_id = "434242-3579-488c-95c6-c18941c99eb4";
    Test_GetPackage(g_module, event_id);

    while(1);
    IOTA_Destroy();
}
