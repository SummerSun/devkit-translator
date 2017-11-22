# DevKitTranslator

Azure IoT Developer Kit is a device designed and developed by Microsoft VS China team.

If it is the first time you play with Azure IoT Developer Kit, I strongly recommend you to read this get-started first: https://microsoft.github.io/azure-iot-developer-kit/docs/get-started/.

DevKit Translator is a mini solution of devkit which leverages http triggered azure function, cognitive service to achieve speech translation.

Currently supported source languages include: Arabic, Chinese, French, German, Italian, Japanese, Portuguese, Russian, Spanish.
Only English is supported as the target language.

## Before Starting

There are two azure resources required in this sample. If you are not familair with Microsoft Azure Cognitive service, you could find lots of useful information here: https://docs.microsoft.com/en-us/azure/#pivot=products&panel=cognitive

**Cognitive Service**

Provision a cognitive service with API type: Translation Speech API in azure portal, or through arm tempalte, note the subscription key.
The free one in enough for this sample.

**Azure Function**

Provision an azure function app, set a key-value pair in app settings, *subscriptionKey: [value noted in previous step]*
Create an http triggered c# azure function. Deploy it with source code in folder azureFunction.

Fill in azure function app name and azure function name in app.ino. Verify and upload to device to play following the instructions in the screen.

## Environment

[VSCode](https://code.visualstudio.com/) with [Arduino extension](https://marketplace.visualstudio.com/items?itemName=vsciot-vscode.vscode-arduino) is recommend as the developer environment. You could of cource choose any other tools you prefer.
