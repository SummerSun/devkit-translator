#include "Arduino.h"
#include "AudioClass.h"
#include "AZ3166WiFi.h"
#include "AzureIotHub.h"
#include "OLEDDisplay.h"
#include "http_client.h"

#define MAX_WORDS 12
#define LANGUAGES_COUNT 9
#define MAX_RECORD_DURATION 3
#define MAX_UPLOAD_SIZE (64 * 1024)
// fill in the azure function app name and azure function name
#define AZURE_FUNCTION_APP_NAME ""
#define AZURE_FUNCTION_NAME ""

static const int audioSize = ((32000 * MAX_RECORD_DURATION) + 44);
static int wavFileSize;
static int curIndex = 0;
static char *waveFile = NULL;
static char azureFunctionUri[128];
static char source[MAX_WORDS] = "Chinese";
static char allSource[LANGUAGES_COUNT][MAX_WORDS] = {"Arabic", "Chinese", "French", "German", "Italian", "Japanese", "Portuguese", "Russian", "Spanish"};
static bool ready = false;
static bool setupMode = false;
static AudioClass Audio;

enum STATUS
{
    Idle,
    Recorded,
    WavReady
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

static const char *httpTriggerTranslator(const char *content, int length)
{
    if (content == NULL || length <= 0 || length > MAX_UPLOAD_SIZE)
    {
        Serial.println("Content not valid");
        return NULL;
    }
    HTTPClient client = HTTPClient(HTTP_POST, azureFunctionUri);
    client.set_header("source", source);
    const Http_Response *response = client.send(content, length);

    if (response != NULL && response->status_code == 200)
    {
        return response->body;
    }
    return NULL;
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
            Audio.startRecord(waveFile, audioSize, MAX_RECORD_DURATION);
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
                    Screen.print(1, "Processing...");
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
            const char *result = httpTriggerTranslator(waveFile, wavFileSize);
            if (result != NULL)
            {
                int len = strlen(result);
                char translation[len];
                strcpy(result, translation + 1);
                result[len - 2] = 0;
                Screen.print(1, "Translation: ");
                Screen.print(2, translation, true);
            }
            else
            {
                Screen.print(1, "azure function failed", true);
            }
        }
        else
        {
            Screen.print(1, "wav not ready");
        }
        freeWavFile();
        enterIdleState(false);
        break;
    }
}

static bool isButtonPressed(unsigned char ulPin)
{
    pinMode(ulPin, INPUT);
    return digitalRead(ulPin) == LOW;
}

void setup()
{
    Screen.clean();
    Screen.print(0, "DevKitTranslator");
    Screen.print(2, "Init WiFi...");
    ready = (WiFi.begin() == WL_CONNECTED);
    if (!ready)
    {
        Screen.print(2, "No Wifi");
        return;
    }
    ready = (strlen(AZURE_FUNCTION_APP_NAME) != 0 && strlen(AZURE_FUNCTION_NAME) != 0);
    if (!ready)
    {
        Screen.print(2, "Azure Resource Init Failed", true);
        return;
    }
    sprintf(azureFunctionUri, "http://%s.azurewebsites.net/api/%s", (char *)AZURE_FUNCTION_APP_NAME, (char *)AZURE_FUNCTION_NAME);
    Screen.print(1, "Hold B to talk  Chinese or Press A choose others", true);
}

void loop()
{
    if (!ready)
    {
        delay(3000);
        return;
    }

    if (setupMode)
    {
        if (isButtonPressed(USER_BUTTON_B))
        {
            scrollLanguages(curIndex + 1);
        }
        if (isButtonPressed(USER_BUTTON_A))
        {
            strncpy(source, allSource[curIndex], MAX_WORDS);
            setupMode = false;
            enterIdleState();
        }
    }
    else
    {
        if (isButtonPressed(USER_BUTTON_A))
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