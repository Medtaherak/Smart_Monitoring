"""Tests automatises du Lot 2 : simulation logicielle sur PC.

Les seuils (5.0 A / 50 degC) sont des parametres de SIMULATION synthetiques,
pas des seuils electriques de securite. Ces tests ne representent pas
l'inference NanoEdge executee sur STM32.
"""
import math

import pytest

import detection as d
import simulation_esp8266 as sim

P = d.DEFAULT_PARAMS
I_MAX, T_MAX = P.courant_max, P.temperature_max
EPS = 0.01


# ---------- Scenarios normaux / anormaux ----------
@pytest.mark.parametrize(
    "courant, temperature, attendu",
    [
        (2.5, 30, d.NORMAL),                        # cas normal
        (6.5, 30, d.SURINTENSITE),                  # surintensite seule
        (2.5, 60, d.SURCHAUFFE),                    # surchauffe seule
        (6.5, 60, d.ANOMALIE_COMBINEE),             # anomalie combinee
    ],
)
def test_scenarios_nominaux(courant, temperature, attendu):
    # Arrange : entrees parametrees ; Act :
    resultat = d.classifier(courant, temperature)
    # Assert
    assert resultat == attendu


# ---------- Valeurs limites ----------
@pytest.mark.parametrize(
    "courant, temperature, attendu",
    [
        (I_MAX - EPS, 30, d.NORMAL),                # juste sous le seuil
        (I_MAX, 30, d.NORMAL),                      # sur le seuil
        (I_MAX + EPS, 30, d.SURINTENSITE),          # juste au-dessus
        (2.5, T_MAX - EPS, d.NORMAL),
        (2.5, T_MAX, d.NORMAL),
        (2.5, T_MAX + EPS, d.SURCHAUFFE),
        (I_MAX, T_MAX, d.NORMAL),                   # deux seuils exactement
        (I_MAX + EPS, T_MAX + EPS, d.ANOMALIE_COMBINEE),
    ],
)
def test_valeurs_limites(courant, temperature, attendu):
    assert d.classifier(courant, temperature) == attendu


def test_seuils_personnalises_sont_des_parametres():
    params = d.SimulationParams(courant_max=1.0, temperature_max=20.0)
    assert d.classifier(1.5, 10, params) == d.SURINTENSITE
    assert d.classifier(0.5, 25, params) == d.SURCHAUFFE


# ---------- Entrees invalides ----------
@pytest.mark.parametrize("mauvais", [None, float("nan"), math.inf, -math.inf])
def test_valeur_absente_ou_non_finie_leve_valueerror(mauvais):
    with pytest.raises(ValueError):
        d.classifier(mauvais, 30)
    with pytest.raises(ValueError):
        d.classifier(2.5, mauvais)


@pytest.mark.parametrize("mauvais", ["abc", "3.2", [1], True])
def test_texte_ou_type_invalide_leve_typeerror(mauvais):
    with pytest.raises(TypeError):
        d.classifier(mauvais, 30)
    with pytest.raises(TypeError):
        d.classifier(2.5, mauvais)


def test_valider_mesure_convertit_entier_en_float():
    assert d.valider_mesure(3) == 3.0
    assert isinstance(d.valider_mesure(3), float)


# ---------- Coherence avec le script existant (scenarios du depot) ----------
def test_scenarios_du_depot_sont_coherents_avec_classifier():
    attendu = {
        "FONCTIONNEMENT NORMAL": d.NORMAL,
        "SURINTENSITE": d.SURINTENSITE,
        "SURCHAUFFE": d.SURCHAUFFE,
    }
    assert len(sim.scenarios) == 3
    for sc in sim.scenarios:
        assert d.classifier(sc["courant"], sc["temperature"]) == attendu[sc["nom"]]
        # le drapeau 'anomalie' envoye a ThingSpeak doit suivre la classe
        assert sc["anomalie"] == (0 if attendu[sc["nom"]] == d.NORMAL else 1)


def test_import_du_simulateur_ne_bloque_pas():
    # Sans refactorisation, l'import appelait getpass + while True.
    assert callable(sim.main)


# ---------- envoyer_donnees : reseau simule (aucun appel reel) ----------
class FausseReponse:
    def __init__(self, status_code=200, text="42"):
        self.status_code, self.text = status_code, text


def test_envoyer_donnees_construit_les_champs_thingspeak(capsys):
    appels = []

    def faux_get(url, params, timeout):
        appels.append((url, params, timeout))
        return FausseReponse()

    sim.envoyer_donnees(sim.scenarios[1], "CLE_FACTICE", http_get=faux_get)

    url, params, timeout = appels[0]
    assert url == sim.URL
    assert timeout == 15
    assert params == {
        "api_key": "CLE_FACTICE",
        "field1": 6.5, "field2": 20, "field3": 1, "field4": 1, "field5": 30,
    }
    assert "ENVOI REUSSI" in capsys.readouterr().out


@pytest.mark.parametrize("reponse", [FausseReponse(200, "0"), FausseReponse(500, "err")])
def test_envoyer_donnees_signale_un_echec(reponse, capsys):
    sim.envoyer_donnees(sim.scenarios[0], "K", http_get=lambda *a, **k: reponse)
    assert "ECHEC DE L'ENVOI" in capsys.readouterr().out


def test_envoyer_donnees_gere_erreur_reseau(capsys):
    def get_en_panne(*a, **k):
        raise sim.requests.ConnectionError("pas de reseau")

    sim.envoyer_donnees(sim.scenarios[0], "K", http_get=get_en_panne)  # ne doit pas lever
    assert "Erreur de connexion" in capsys.readouterr().out


# ---------- Plages des jeux de donnees synthetiques (equipe) ----------
# Valeurs de SIMULATION, pas des seuils de securite valides sur materiel reel.
@pytest.mark.parametrize(
    "courant, temperature",
    [(2.0, 28.0), (2.5, 30.0), (3.0, 32.0), (2.0, 32.0), (3.0, 28.0)],
)
def test_plage_normale_du_jeu_de_donnees(courant, temperature):
    assert d.classifier(courant, temperature) == d.NORMAL


@pytest.mark.parametrize("courant", [5.8, 6.0, 6.5, 8.0])
def test_surintensite_du_jeu_de_donnees(courant):
    assert d.classifier(courant, 30.0) == d.SURINTENSITE


@pytest.mark.parametrize("temperature", [55.0, 60.0, 70.0])
def test_surchauffe_du_jeu_de_donnees(temperature):
    assert d.classifier(2.5, temperature) == d.SURCHAUFFE


def test_zone_entre_seuil_de_simulation_et_jeu_de_donnees():
    # Entre 5.0 A (seuil provisoire) et 5.8 A (debut de la surintensite des
    # donnees), le jeu de donnees ne definit rien : ici on documente la
    # decision provisoire (5.0 A = seuil). A revoir si l'equipe change de seuil.
    assert d.classifier(5.4, 30.0) == d.SURINTENSITE
