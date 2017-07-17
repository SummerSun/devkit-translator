#include "Arduino.h"
#include "AudioClass.h"
#include "AZ3166WiFi.h"
#include "AzureIotHub.h"
#include "EEPROMInterface.h"
#include "http_client.h"
#include "iothub_client_ll.h"
#include "OLEDDisplay.h"

#define MAX_UPLOAD_SIZE (64 * 1024)
#define MAX_WORDS 12
#define LANGUAGES_COUNT 9

static boolean hasWifi = false;
static const int recordedDuration = 3;
static char *waveFile = NULL;
static int wavFileSize;
static const uint32_t delayTimes = 1000;
static AudioClass Audio;
static const int audioSize = ((32000 * recordedDuration) + 44);
static bool validParameters = false;
static IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle;
static bool setupMode = false;
static char source[MAX_WORDS] = "Chinese";
static int curIndex = 1;
static const char *azureFunctionUri = "http://devkittranslator.azurewebsites.net/api/devkit-translator";
static char allSource[LANGUAGES_COUNT][MAX_WORDS] = {"Arabic", "Chinese", "French", "German", "Italian", "Japanese", "Portuguese", "Russian", "Spanish"};

enum STATUS
{
    Idle,
    Recorded,
    WavReady,
    Uploaded
};

static STATUS status = Idle;

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
    HTTPClient client = HTTPClient(HTTP_POST, azureFunctionUri);
    client.set_header("source", source);
    const Http_Response *response = client.send(content, length);
    if (response != NULL && response->status_code == 200)
    {
        return 0;
    }
    return -1;
}

static void scrollLanguages(int index)
{
    Screen.clean();
    Screen.print(0, "Press B Scroll");
    if (index > LANGUAGES_COUNT - 1)
    {
        index = 0;
    }
    curIndex = index++;
    char temp[MAX_WORDS];
    sprintf(temp, "> %s", allSource[curIndex]);
    Screen.print(1, temp);
    for (int i = 2; i <= 3; i++)
    {
        if (index > LANGUAGES_COUNT - 1)
        {
            index = 0;
        }
        Screen.print(i, allSource[index++]);
    }
    free(temp);
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
            Screen.print(1, "Translation: ");
            Screen.print(2, buffer, true);
            result = IOTHUBMESSAGE_ACCEPTED;
        }
    }
    freeWavFile();
    enterIdleState(false);
    return result;
}

static int iothubInit()
{
    EEPROMInterface eeprom;
    uint8_t connString[AZ_IOT_HUB_MAX_LEN + 1] = {'\0'};
    if (eeprom.read(connString, AZ_IOT_HUB_MAX_LEN, 0x00, AZ_IOT_HUB_ZONE_IDX) < 0)
    {
        return -1;
    }
    if ((iotHubClientHandle = IoTHubClient_LL_CreateFromConnectionString((const char *)connString, MQTT_Protocol)) == NULL)
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
    validParameters = (iothubInit() == 0) && (azureFunctionUri != NULL && *azureFunctionUri != '\0');
    if (!validParameters)
    {
        Screen.print(2, "IoTHub init failed", true);
        return;
    }
    Screen.print(1, "Hold B to talk Chinese or Press A choose others", true);
}

static void freeWavFile()
{
    if (waveFile != NULL)
    {
        free(waveFile);
        waveFile = NULL;
    }
}

static void listenVoice()
{
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
}

static bool IsButtonClicked(unsigned char ulPin)
{
    pinMode(ulPin, INPUT);
    return digitalRead(ulPin) == LOW;
}

void loop()
{
    if (!hasWifi || !validParameters)
    {
        return;
    }
    if (setupMode)
    {
        if (IsButtonClicked(USER_BUTTON_B))
        {
            scrollLanguages(curIndex + 1);
        }
        if (IsButtonClicked(USER_BUTTON_A))
        {
            strncpy(source, allSource[curIndex], MAX_WORDS);
            setupMode = false;
            enterIdleState();
        }
    }
    else
    {
        if (IsButtonClicked(USER_BUTTON_A))
        {
            scrollLanguages(0);
            setupMode = true;
        }
        else
        {
            listenVoice();
        }
    }
    delay(200);
}
