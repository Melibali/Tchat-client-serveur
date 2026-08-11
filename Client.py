import socket
import threading
import time
import sys

HOST = "127.0.0.1"
PORT = 'mettre le num du port du serveur ici'

PING_AFTER = 15

client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

last_activity = time.monotonic()
running = True


def update_activity():
    global last_activity
    last_activity = time.monotonic()


def receive_messages():
    global running

    while running:
        try:
            data = client.recv(4096)

            if not data:
                print("\n[Serveur] Connexion fermée.")
                running = False
                break

            # Un recv() peut contenir plusieurs lignes.
            messages = data.decode("utf-8", errors="replace").splitlines()

            for message in messages:
                if message:
                    print(f"\n{message}")
                    print("> ", end="", flush=True)

        except OSError:
            if running:
                print("\n[Client] Connexion interrompue.")
            running = False
            break


def ping_loop():
    global running

    while running:
        time.sleep(1)

        if time.monotonic() - last_activity >= PING_AFTER:
            try:
                client.sendall(b"PING\n")
                update_activity()
            except OSError:
                running = False
                break


def send_command(command):
    update_activity()
    client.sendall((command + "\n").encode("utf-8"))


def main():
    global running

    try:
        client.connect((HOST, PORT))
    except ConnectionRefusedError:
        print("Erreur : le serveur n'est pas disponible.")
        sys.exit(1)
    except OSError as error:
        print(f"Erreur de connexion : {error}")
        sys.exit(1)

    print("===================================")
    print("      CLIENT TCHAT")
    print("===================================")
    print("Connecté au serveur.")
    print()
    print("Commandes disponibles :")
    print("  NAME pseudo")
    print("  PRIV pseudo message")
    print("  LIST")
    print("  JOIN channel")
    print("  EXIT channel")
    print("  TALK channel message")
    print("  QUIT")
    print("===================================")

    receiver = threading.Thread(
        target=receive_messages,
        daemon=True
    )

    pinger = threading.Thread(
        target=ping_loop,
        daemon=True
    )

    receiver.start()
    pinger.start()

    try:
        while running:
            command = input("> ")

            if not command.strip():
                continue

            send_command(command)

            if command.strip().upper() == "QUIT":
                # On laisse le serveur recevoir QUIT.
                time.sleep(0.2)
                running = False
                break

    except KeyboardInterrupt:
        print("\nDéconnexion...")

        try:
            send_command("QUIT")
        except OSError:
            pass

        running = False

    except EOFError:
        try:
            send_command("QUIT")
        except OSError:
            pass

        running = False

    finally:
        try:
            client.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass

        client.close()

    print("Client arrêté.")


if __name__ == "__main__":
    main()
