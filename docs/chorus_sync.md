# Chœur des cigales : synchroniser des badges par radio

Faire jouer un même morceau à plusieurs badges, chacun sa voix, sans carte SD (les morceaux sont dans le firmware)
et sans horloge commune : ce document résume les techniques connues et le protocole retenu (`src/menu/chorus.c`).

## Le problème

Chaque badge a sa propre horloge (quartz 12 MHz du RP2040, précision de l'ordre de ±30 ppm) et ne connaît pas
l'heure des autres. Pour que les voix sonnent ensemble, les départs doivent être alignés à quelques millisecondes
près (l'oreille perçoit un décalage à partir de 20 à 30 ms), et le rester pendant le morceau (une minute environ).

## Les techniques de synchronisation des réseaux de capteurs

| Technique | Principe | Précision typique | Intérêt ici |
|---|---|---|---|
| **NTP** | échanges aller-retour avec un serveur, estimation de la latence | ms sur Internet | trop de messages pour une salle entière |
| **TPSN** (Timing-sync Protocol for Sensor Networks, 2003) | échanges aller-retour le long d'un arbre, horodatage au niveau MAC | ~20 µs | nécessite un arbre et des allers-retours |
| **RBS** (Reference Broadcast Synchronization, Elson et al., 2002) | un émetteur de référence diffuse un paquet ; les récepteurs le reçoivent **au même instant** (la propagation est négligeable) et s'en servent comme référence commune | quelques µs | **un seul paquet pour tous** : idéal quand tous entendent le même émetteur |
| **FTSP** (Flooding Time Synchronization Protocol, Maróti et al., 2004) | horodatage au niveau MAC dans le paquet, inondation multi-sauts, régression linéaire de la dérive | ~1,5 µs par saut | utile sur plusieurs sauts ; ici une salle suffit |
| Méthodes « beat » des applications musicales (Ableton Link, etc.) | tempo et phase partagés, corrections douces | ms | idée reprise : on synchronise une **position dans le morceau**, pas une heure |

Sources d'erreur dans le cas des badges :
- **Latence d'émission** : entre `net_send()` et l'antenne, la file d'attente et le temps d'accès au canal varient
  (des ms). Avec RBS cette erreur disparaît : tous les récepteurs voient le **même** paquet.
- **Latence de réception** : la fin du paquet est détectée par la boucle principale (sondage toutes les 2 ms) :
  quelques ms d'écart au pire entre badges, acceptable. Une interruption sur GDO0 ferait mieux si besoin.
- **Dérive des quartz** : ±30 ppm par badge, soit 60 ppm entre deux badges, soit ~4 ms par minute : inaudible
  pour une chanson d'une minute. Des re-synchronisations régulières corrigent les morceaux plus longs.
- **Démarrage de l'audio** : chaque badge ouvre sa sortie son au même instant local et écrit ses échantillons
  juste à temps (tampon court), la latence est la même partout.

## Le protocole retenu

Paquets `NET_SONG` (réseau des cigales, +10 dBm) :

| Octets | Contenu |
|---|---|
| 0 | morceau (index dans le firmware) |
| 1 | type : 1 = départ, 2 = position (re-synchronisation), 3 = arrêt |
| 2-3 | session (hasard, pour ignorer les doublons) |
| 4-7 | départ : délai en ms entre la fin du paquet et le premier temps ; position : ms écoulées depuis le départ |
| 8 | nombre de voix utilisées |

1. **Départ** : le badge chef (menu admin « Choeur : lancer », ou télécommande Flipper `0xC16A3n`) diffuse
   « départ dans 2 000 ms », trois fois, en recalculant le délai restant à chaque envoi : les trois copies désignent
   le même instant.
2. Chaque badge qui reçoit le paquet planifie le départ à `réception + délai` (principe RBS) et choisit sa voix
   d'après son identifiant : le chef chante la première voix, les autres badges se partagent les autres voix
   (`1 + id % (voix - 1)`), pour que la mélodie principale soit toujours là.
3. **Retardataires** : pendant le morceau, le chef diffuse sa position toutes les 2 secondes ; un badge qui
   arrive en cours de route (il a manqué le départ) rejoint le chœur à cette position. Un badge déjà parti suit son
   horloge jusqu'au bout : il ne se recale pas en cours de morceau (la dérive des quartz reste sous quelques ms
   sur une chanson).
4. Plusieurs chefs (deux télécommandes) : le départ le plus proche gagne, à égalité le plus petit identifiant.

## Limites

- Tous les badges doivent entendre le chef (une salle) : pas de relais multi-sauts.
- Le mode muet (conférence) coupe le son : le chœur est silencieux.
- Les badges qui écoutent les télécommandes OOK à ce moment-là peuvent manquer un paquet : le départ est répété
  et les positions régulières rattrapent les retardataires.

## Références

- J. Elson, L. Girod, D. Estrin, *Fine-Grained Network Time Synchronization using Reference Broadcasts*, OSDI 2002.
- S. Ganeriwal, R. Kumar, M. Srivastava, *Timing-sync Protocol for Sensor Networks*, SenSys 2003.
- M. Maróti, B. Kusy, G. Simon, Á. Lédeczi, *The Flooding Time Synchronization Protocol*, SenSys 2004.
