# Tchat-client-serveur

Projet réalisé dans le cadre du cours de Systèmes et Réseaux.

Application de discussion client/serveur utilisant le protocole TCP/IP.

## Technologies utilisées

- C
- Python 3
- TCP/IP
- Sockets
- `select()`

## Architecture

Le projet utilise une architecture client/serveur :

Client 1 ───────┐
Client 2 ───────┼──────> Serveur
Client 3 ───────┘


Compilation du serveur
gcc -Wall -Wextra -std=c11 serveur.c -o serveur
Lancement du serveur


Lancement du client
Dans un autre terminal :
python3 Client.py
