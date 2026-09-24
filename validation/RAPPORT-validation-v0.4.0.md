# Rapport de validation matérielle — PicoWiFiModemUSB v0.4.0 — 2026-09-24

- Firmware : `wifi_modem.uf2` compilé depuis le commit `25d9916` (SDK 1.5.1, mbedTLS 2.28.1),
  image 689 540 o
- Carte : Raspberry Pi Pico W (USB `cafe:4001`, `/dev/ttyACM0`), flashée par `AT+BOOTSEL` depuis le
  firmware Neo6502picowifi puis copie sur `RPI-RP2`
- État avant flash : zone LittleFS d'une ancienne installation 0.3.x conservée — Wi-Fi configuré,
  CA de 224 449 o stocké (bundle Mozilla de juin, `../validation/ca-bundle-full.pem`, même taille)
- Script : `validation/validate_trust_store.py` (versionné)
- Résultat : **18/18 étapes réussies**, code de retour 0

## Constats

| Point | Résultat |
|---|---|
| Migration 0.3.x | réglages conservés (Wi-Fi reconnecté sans ressaisie), `AT$CV?` → `1` |
| Magasin intégré | `AT$CA?` → `CA: 0 bytes (built-in store: 150 roots)` après `AT$CA-` (suppression du bundle de juin, décidée par le PO) |
| Autorités acceptées | Let's Encrypt (badssl.com), DigiCert Global Root G2 (www.digicert.com), Sectigo E46 (github.com) |
| Refus | racine inconnue, nom d'hôte faux, certificat expiré (racine COMODO présente : refus par la date) |
| `AT$CA=` | ISRG Root X1 seule : badssl.com accepté, www.digicert.com refusé ; `AT$CA-` rétablit le magasin |
| `AT$CV0` / `AT$CV1` | refus explicite puis réactivation sans CA chargé |

## Temps de connexion `ATGET` (mesure unique, réseau compris)

| Hôte | Bundle LittleFS lu en entier (chemin 0.3.x, 150 certificats) | Magasin intégré indexé |
|---|---|---|
| badssl.com | 6,7 s | 3,8 s |
| www.digicert.com | 2,5 s | 0,9 s |
| github.com | 14,4 s | 12,4 s |

github.com reste lent avec les deux méthodes : la chaîne est en ECDSA P-384.

## Optimisation ECDSA (`MBEDTLS_ECP_NIST_OPTIM`) — même jour, version publiée

`ECP_WINDOW_SIZE` (4) et `ECP_FIXED_POINT_OPTIM` (1) étant déjà les valeurs par défaut de
mbedTLS 2.28, seul `MBEDTLS_ECP_NIST_OPTIM` (réduction rapide des courbes NIST) a été ajouté.
Mesures A/B sur la même carte, 3 essais par hôte :

| Hôte | Sans (`Build Sep 24 2026 16:58:15`) | Avec (`Build Sep 24 2026 18:52:49`) |
|---|---|---|
| badssl.com | 3,8 / 4,0 / 3,8 s | 2,1 / 2,1 / 2,0 s |
| www.digicert.com | 0,9 / 0,9 / 0,9 s | 0,9 / 0,9 / 0,9 s |
| github.com | 12,4 / 12,4 / 12,4 s | **3,0 / 3,0 / 3,0 s** |

`validate_trust_store.py` rejoué sur la version avec optimisation (celle publiée en v0.4.0) :
**18/18**, 2026-09-24 18:58.

## Résultats

| Étape | Résultat | Détail |
|---|---|---|
| ATI reports v0.4.0 | OK | Pico WiFi modem v0.4.0 |
| verification ON (default or migrated from 0.3.x) | OK | 1 |
| no uploaded CA → built-in store in use | OK | CA: 0 bytes (built-in store: 150 roots) |
| badssl.com → CONNECT (Let's Encrypt, ISRG Root X1) | OK | CONNECT, 3.8 s |
| www.digicert.com → CONNECT (DigiCert Global Root G2, RSA) | OK | CONNECT, 0.9 s |
| github.com → CONNECT (Sectigo E46, ECDSA P-384) | OK | CONNECT, 12.4 s |
| untrusted-root.badssl.com → NO CARRIER (unknown root) | OK | NO CARRIER, 0.7 s |
| wrong.host.badssl.com → NO CARRIER (wrong host name) | OK | NO CARRIER, 1.2 s |
| expired.badssl.com → NO CARRIER (expired; COMODO root is in the store) | OK | NO CARRIER, 0.8 s |
| AT$CA= ISRG Root X1 stored | OK | OK |
| AT$CA? reports the replacement | OK | CA: 1939 bytes (replaces the built-in store) |
| badssl.com → CONNECT (ISRG = uploaded CA) | OK | CONNECT, 3.8 s |
| www.digicert.com → NO CARRIER (DigiCert not in the uploaded CA) | OK | NO CARRIER, 0.4 s |
| www.digicert.com → CONNECT (AT$CA- restores the built-in store) | OK | CONNECT, 0.9 s |
| AT$CV0 | OK |  |
| untrusted-root.badssl.com → CONNECT (AT$CV0 accepts any certificate) | OK | CONNECT, 3.0 s |
| AT$CV1 (restore; no CA needed any more) | OK |  |
| untrusted-root.badssl.com → NO CARRIER (verification back on) | OK | NO CARRIER, 0.7 s |

## v0.4.1 — 2026-09-24 22:30

`dist/wifi_modem-v0.4.1.uf2` (SHA-256 `01691352…`, compilation reproductible) flashé sur la même
carte : `validate_trust_store.py` **19/19** (nouvelle étape : `ATI` → `Build......: v0.4.1 (Sep 24
2026 20:05:40 UTC)`, l'identifiant du tag). Temps : badssl.com 1,9 s, www.digicert.com 0,9 s,
github.com 3,0 s.

## v0.4.2 — 2026-09-24 22:39

`dist/wifi_modem-v0.4.2.uf2` (SHA-256 `b21a5847…`, reproductible) : `validate_trust_store.py`
**21/21**, dont la connexion Wi-Fi au démarrage et un `ATC0`/`ATC1` en mode WPA2 mixte (réseau
WPA2-AES). Contrôle manuel : après `ATC0`, `WiFi status: LINK IS DOWN` ; `ATC1` →
`CONNECTED TO WIFI` en 5,4 s. Pas de réseau TKIP disponible pour tester l'apport du mode mixte.
