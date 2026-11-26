"""Logique pure de la simulation (sans reseau, sans saisie clavier).

ATTENTION : les seuils ci-dessous sont des PARAMETRES DE SIMULATION
synthetiques. Ce ne sont pas des seuils electriques de securite et ils ne
representent pas l'inference NanoEdge AI executee sur STM32.
"""
import math
from dataclasses import dataclass


@dataclass(frozen=True)
class SimulationParams:
    courant_max: float = 5.0        # A, seuil de demonstration
    temperature_max: float = 50.0   # degC, seuil de demonstration


DEFAULT_PARAMS = SimulationParams()

NORMAL = "NORMAL"
SURINTENSITE = "SURINTENSITE"
SURCHAUFFE = "SURCHAUFFE"
ANOMALIE_COMBINEE = "ANOMALIE_COMBINEE"


def valider_mesure(valeur, nom="mesure"):
    """Retourne la mesure en float, ou leve TypeError / ValueError."""
    if valeur is None:
        raise ValueError(f"{nom} absente")
    if isinstance(valeur, bool) or not isinstance(valeur, (int, float)):
        raise TypeError(f"{nom} doit etre un nombre, recu {type(valeur).__name__}")
    valeur = float(valeur)
    if not math.isfinite(valeur):
        raise ValueError(f"{nom} non finie")
    return valeur


def classifier(courant, temperature, params=DEFAULT_PARAMS):
    """Classe une mesure (courant en A, temperature en degC).

    Une valeur egale au seuil est NORMALE ; strictement superieure = anormale.
    """
    i = valider_mesure(courant, "courant")
    t = valider_mesure(temperature, "temperature")
    sur_i = i > params.courant_max
    sur_t = t > params.temperature_max
    if sur_i and sur_t:
        return ANOMALIE_COMBINEE
    if sur_i:
        return SURINTENSITE
    if sur_t:
        return SURCHAUFFE
    return NORMAL
