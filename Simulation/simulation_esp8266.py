
import requests
import time
import getpass

# ==========================================
# CONFIGURATION THINGSPEAK
# ==========================================

API_KEY = getpass.getpass("Entrez votre nouvelle Write API Key : ")

URL = "https://api.thingspeak.com/update"

# ==========================================
# SCENARIOS DE SIMULATION
# ==========================================

scenarios = [
    {
        "nom": "FONCTIONNEMENT NORMAL",
        "courant": 2.5,
        "similarite": 95,
        "anomalie": 0,
        "etat": 1,
        "temperature": 30
    },
    {
        "nom": "SURINTENSITE",
        "courant": 6.5,
        "similarite": 20,
        "anomalie": 1,
        "etat": 1,
        "temperature": 30
    },
    {
        "nom": "SURCHAUFFE",
        "courant": 2.5,
        "similarite": 25,
        "anomalie": 1,
        "etat": 1,
        "temperature": 60
    }
]

# ==========================================
# ENVOI DES DONNEES
# ==========================================

def envoyer_donnees(data):

    params = {
        "api_key": API_KEY,
        "field1": data["courant"],
        "field2": data["similarite"],
        "field3": data["anomalie"],
        "field4": data["etat"],
        "field5": data["temperature"]
    }

    try:
        response = requests.get(
            URL,
            params=params,
            timeout=15
        )

        if response.status_code == 200 and response.text.strip() != "0":
            print("ENVOI REUSSI")
            print("Numero de l'entree :", response.text)
        else:
            print("ECHEC DE L'ENVOI :", response.text)

    except requests.RequestException as erreur:
        print("Erreur de connexion :", erreur)


# ==========================================
# BOUCLE PRINCIPALE
# ==========================================

print("\n================================")
print(" SIMULATION ESP8266 - THINGSPEAK")
print("================================")

print("Demarrage de la simulation...\n")

index = 0

while True:

    data = scenarios[index]

    print("\n----------", data["nom"], "----------")
    print("Courant      :", data["courant"], "A")
    print("Similarite   :", data["similarite"], "%")
    print("Anomalie     :", data["anomalie"])
    print("Etat NanoEdge:", data["etat"])
    print("Temperature  :", data["temperature"], "C")

    envoyer_donnees(data)

    index = (index + 1) % len(scenarios)

    print("\nProchaine transmission dans 20 secondes...")
    time.sleep(20)
