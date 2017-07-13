#include "Arduino.h"
#include "AzureIotHub.h"
#include "AudioClass.h"
#include "AZ3166WiFi.h"
#include "OLEDDisplay.h"
#include "http_client.h"
#include "iothub_client_ll.h"

#define MAX_UPLOAD_SIZE (64 * 1024)

static boolean hasWifi = false;
static const int recordedDuration = 3;
static char *waveFile = NULL;
static int wavFileSize;
static const uint32_t delayTimes = 1000;
static AudioClass Audio;
static const int audioSize = ((32000 * recordedDuration) + 44);
static bool validParameters = false;
static const char *deviceConnectionString = "";
static const char *azureFunctionUri = "";
static IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle;

enum STATUS
{
    Idle,
    Recorded,
    WavReady,
    Uploaded
};

static STATUS status;

static void logTime(const char *event)
{
    time_t t = time(NULL);
    Serial.printf("%s: %s", event, ctime(&t));
}

static void enterIdleState(bool clean = true)
{
    status = Idle;
    if (clean)
    {
        Screen.clean();
    }
    Screen.print(0, "Hold B to talk");
}

static int httpTriggerTranslator(const char *content, int length)
{
    if (content == NULL || length <= 0 || length > MAX_UPLOAD_SIZE)
    {
        Serial.println("Content not valid");
        return -1;
    }
    logTime("begin httppost");
    HTTPClient client = HTTPClient(HTTP_POST, azureFunctionUri);    
    client.set_header("source", source);
    client.set_header("target", target);
    const Http_Response *response = client.send(content, length);
    logTime("response back");
    if (response != NULL && response->status_code == 200)
    {
        return 0;
    }
    return -1;
}

static IOTHUBMESSAGE_DISPOSITION_RESULT c2dMessageCallback(IOTHUB_MESSAGE_HANDLE message, void *userContextCallback)
{
    const char *buffer;
    size_t size;
    IOTHUBMESSAGE_DISPOSITION_RESULT result;

    if (IoTHubMessage_GetByteArray(message, (const unsigned char **)&buffer, &size) != IOTHUB_MESSAGE_OK)
    {
        Screen.print(1, "unable to do IoTHubMessage_GetByteArray", true);
        result = IOTHUBMESSAGE_REJECTED;
    }
    else
    {
        char *temp = (char *)malloc(size + 1);
        if (temp == NULL)
        {
            Screen.print(1, "Failed to malloc");
            result = IOTHUBMESSAGE_REJECTED;
        }
        else
        {
            memcpy(temp, buffer, size);
            temp[size] = '\0';
            Screen.print(1, "Translation: ");
            Screen.print(2, temp, true);
            logTime("translated");
            free(temp);
            result = IOTHUBMESSAGE_ACCEPTED;
        }
    }
    freeWavFile();
    enterIdleState(false);
    return result;
}

static int iothubInit()
{
    if ((iotHubClientHandle = IoTHubClient_LL_CreateFromConnectionString(deviceConnectionString, MQTT_Protocol)) == NULL)
    {
        return -1;
    }

    if (IoTHubClient_LL_SetOption(iotHubClientHandle, "TrustedCerts", certificates) != IOTHUB_CLIENT_OK)
    {
        return -1;
    }

    if (IoTHubClient_LL_SetMessageCallback(iotHubClientHandle, c2dMessageCallback, NULL) != IOTHUB_CLIENT_OK)
    {
        return -1;
    }
    return 0;
}

void setup()
{
    Screen.clean();
    Screen.print(0, "DevKitTranslator");
    Screen.print(2, "Init WiFi...");
    hasWifi = (WiFi.begin() == WL_CONNECTED);
    if (!hasWifi)
    {
        Screen.print(2, "No Wifi");
        return;
    }
    validParameters = (deviceConnectionString != NULL && *deviceConnectionString != '\0' && azureFunctionUri != NULL && *azureFunctionUri != '\0');
    if (!validParameters || iothubInit() !=0)
    {
        validParameters = false;
        Screen.print(2, "IoTHub init failed", true);
        return;
    }
    enterIdleState();
}

void freeWavFile()
{
    if (waveFile != NULL)
    {
        free(waveFile);
        waveFile = NULL;
    }
}

void loop()
{
    if (!hasWifi || !validParameters)
    {
        return;
    }

    uint32_t curr = millis();
    switch (status)
    {
    case Idle:
        if (digitalRead(USER_BUTTON_B) == LOW)
        {
            waveFile = (char *)malloc(audioSize + 1);
            if (waveFile == NULL)
            {
                Serial.println("No enough Memory! ");
                enterIdleState();
                return;
            }
            memset(waveFile, 0, audioSize + 1);
            Audio.format(8000, 16);
            Audio.startRecord(waveFile, audioSize, recordedDuration);
            status = Recorded;
            Screen.clean();
            Screen.print(0, "Release to send\r\nMax duraion: 3 sec");
        }
        break;
    case Recorded:
        if (digitalRead(USER_BUTTON_B) == HIGH)
        {
            Audio.getWav(&wavFileSize);
            if (wavFileSize > 0)
            {
                wavFileSize = Audio.convertToMono(waveFile, wavFileSize, 16);
                if (wavFileSize <= 0)
                {
                    Serial.println("ConvertToMono failed! ");
                    enterIdleState();
                    freeWavFile();
                }
                else
                {
                    status = WavReady;
                    Screen.clean();
                    Screen.print(0, "Processing...");
                    Screen.print(1, "Uploading...");
                }
            }
            else
            {
                Serial.println("No Data Recorded! ");
                freeWavFile();
                enterIdleState();
            }
        }
        break;
    case WavReady:
        if (wavFileSize > 0 && waveFile != NULL)
        {
            if (0 == httpTriggerTranslator(waveFile, wavFileSize))
            {
                status = Uploaded;
                Screen.print(1, "Receiving...");
            }
            else
            {
                Serial.println("Error happened when translating");
                freeWavFile();
                enterIdleState();
                Screen.print(2, "azure function failed", true);
            }
        }
        else
        {
            freeWavFile();
            enterIdleState();
            Screen.print(1, "wav not ready");
        }
        break;
    case Uploaded:
        IoTHubClient_LL_DoWork(iotHubClientHandle);
        break;
    }

    curr = millis() - curr;
    if (curr < delayTimes)
    {
        delay(delayTimes - curr);
    }
}
