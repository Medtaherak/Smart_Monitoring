
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

// =====================================================
// WIFI
// =====================================================
const char* ssid = "";
const char* password = "";

// =====================================================
// THINGSPEAK
// =====================================================
const char* writeAPIKey = "";

// =====================================================
// VARIABLES
// =====================================================
String message = "";

float courant = 0.0;
float temperature = 0.0;
float humidite = 0.0;

int similarity = 0;
int anomalie = 0;
int etatNanoEdge = 0;

unsigned long lastWifiAttempt = 0;
const unsigned long wifiRetryInterval = 10000;

// =====================================================
// SETUP
// =====================================================
void setup()
{
  Serial.begin(115200);
  Serial.setTimeout(1000);

  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println(" ESP8266 - STM32 - ThingSpeak");
  Serial.println("================================");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.println("Connexion au WiFi...");
}

// =====================================================
// LOOP
// =====================================================
void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    if (millis() - lastWifiAttempt >= wifiRetryInterval)
    {
      lastWifiAttempt = millis();

      Serial.println("WiFi non connecte.");
      Serial.println("Nouvelle tentative...");

      WiFi.disconnect();
      WiFi.begin(ssid, password);
    }
  }
  else
  {
    static bool wifiMessageDisplayed = false;

    if (!wifiMessageDisplayed)
    {
      Serial.println("WiFi CONNECTE !");
      Serial.print("Adresse IP : ");
      Serial.println(WiFi.localIP());

      wifiMessageDisplayed = true;
    }
  }

  // Reception des donnees du STM32
  if (Serial.available())
  {
    message = Serial.readStringUntil('\n');
    message.trim();

    if (message.length() > 0)
    {
      Serial.print("Message STM32 : ");
      Serial.println(message);

      parseSTM32Message(message);
    }
  }
}

// =====================================================
// EXTRACTION D'UNE VALEUR
// Exemple : I_x100=245
// =====================================================
bool extractValue(String data, String label, int &value)
{
  int position = data.indexOf(label);

  if (position == -1)
    return false;

  position += label.length();

  int endPosition = data.indexOf(';', position);

  if (endPosition == -1)
    endPosition = data.length();

  String valueString = data.substring(position, endPosition);
  valueString.trim();

  if (valueString.length() == 0)
    return false;

  value = valueString.toInt();
  return true;
}

// =====================================================
// PARSING STM32
// Format :
// I_x100=245;T_x10=305;H_x10=450;S=96;A=0
// =====================================================
void parseSTM32Message(String data)
{
  int currentRaw;
  int temperatureRaw;
  int humidityRaw;
  int similarityRaw;
  int anomalyRaw;

  bool valid =
      extractValue(data, "I_x100=", currentRaw) &&
      extractValue(data, "T_x10=", temperatureRaw) &&
      extractValue(data, "H_x10=", humidityRaw) &&
      extractValue(data, "S=", similarityRaw) &&
      extractValue(data, "A=", anomalyRaw);

  if (!valid)
  {
    Serial.println("ERREUR : format STM32 invalide.");
    return;
  }

  // Conversion des valeurs
  courant = currentRaw / 100.0;
  temperature = temperatureRaw / 10.0;
  humidite = humidityRaw / 10.0;

  similarity = similarityRaw;
  anomalie = anomalyRaw;

  // 1 = programme actif et message valide
  etatNanoEdge = 1;

  // Affichage
  Serial.println("---------- DONNEES ----------");

  Serial.print("Courant     : ");
  Serial.print(courant);
  Serial.println(" A");

  Serial.print("Temperature : ");
  Serial.print(temperature);
  Serial.println(" C");

  Serial.print("Humidite    : ");
  Serial.print(humidite);
  Serial.println(" %");

  Serial.print("Similarite  : ");
  Serial.print(similarity);
  Serial.println(" %");

  Serial.print("Anomalie    : ");
  Serial.println(anomalie);

  Serial.print("Etat NanoEdge : ");
  Serial.println(etatNanoEdge);

  Serial.println("-----------------------------");

  sendToThingSpeak();
}

// =====================================================
// ENVOI THINGSPEAK
// =====================================================
void sendToThingSpeak()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("ERREUR : WiFi non disponible.");
    return;
  }

  WiFiClient client;
  HTTPClient http;

  String url = "http://api.thingspeak.com/update?api_key=";
  url += writeAPIKey;

  // Field 1 = Courant
  url += "&field1=";
  url += String(courant, 2);

  // Field 2 = Similarite
  url += "&field2=";
  url += String(similarity);

  // Field 3 = Anomalie
  url += "&field3=";
  url += String(anomalie);

  // Field 4 = Etat NanoEdge
  url += "&field4=";
  url += String(etatNanoEdge);

  // Field 5 = Temperature
  url += "&field5=";
  url += String(temperature, 1);

  Serial.println("Envoi vers ThingSpeak...");

  http.begin(client, url);

  int httpCode = http.GET();

  if (httpCode > 0)
  {
    Serial.print("Code HTTP : ");
    Serial.println(httpCode);

    String response = http.getString();
    response.trim();

    Serial.print("Reponse ThingSpeak : ");
    Serial.println(response);

    if (response != "0")
    {
      Serial.println(">>> ENVOI THINGSPEAK REUSSI <<<");
      Serial.print("Numero de l'entree : ");
      Serial.println(response);
    }
    else
    {
      Serial.println("ECHEC : ThingSpeak a refuse la donnee.");
    }
  }
  else
  {
    Serial.print("Erreur HTTP : ");
    Serial.println(http.errorToString(httpCode));
  }

  http.end();
}
