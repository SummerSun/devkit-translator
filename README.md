# DevKitTranslator

DevKit Translator is a mini solution of devkit which leverages http triggered azure function, cognitive service and iothub to achieve speech translation.

Currently supported source languages include: Arabic, Chinese, French, German, Italian, Japanese, Portuguese, Russian, Spanish.
Only English is supported as the target language.

Pre-requirements

**Cognitive Service**
Provision a cognitive service with API type: Translation Speech API in azure portal, or through arm tempalte, note the subscription key.

**Azure Function**
Provision an azure function app, set a key-value pari app settings, *subscriptionKey: [value noted in previous step]*
Create an http triggered c# azure function. Deploy it with source code in folder azureFunction.