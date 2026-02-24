#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#define RESET "\033[0m"
#define ROJO "\033[31m"
#define VERDE "\033[32m"
#define AMARILLO "\033[33m"
#define AZUL "\033[34m"
#define NARANJA "\033[38;5;208m"
#define MAGENTA "\033[35m"


#define MAX_BUFFER 256
#define MAX_SHIPS 100
#define MAX_CAPTAINS 20

// Estructura de Úrsula
typedef struct {
    pid_t pid;
    int x;
    int y;
    int food;
    int gold;
    int active;
} ShipRecord;

ShipRecord ships[MAX_SHIPS];
pid_t captains[MAX_CAPTAINS];

int num_ships = 0;
int num_captains = 0;
int active_entities = 0;
int ursula_treasure = 100; // Tesoro inicial 
int apocalypse_triggered = 0;

// --- FUNCIONES DE REGISTRO ---
void register_captain(pid_t pid) {
    captains[num_captains++] = pid;
    active_entities++;
}

void unregister_captain(pid_t pid) {
    for (int i = 0; i < num_captains; i++) {
        if (captains[i] == pid) {
            captains[i] = -1;
            active_entities--;
            break;
        }
    }
}

void register_ship(pid_t pid, int x, int y, int food, int gold) {
    ships[num_ships].pid = pid;
    ships[num_ships].x = x;
    ships[num_ships].y = y;
    ships[num_ships].food = food;
    ships[num_ships].gold = gold;
    ships[num_ships].active = 1;
    num_ships++;
    active_entities++;
}

void unregister_ship(pid_t pid) {
    for (int i = 0; i < num_ships; i++) {
        if (ships[i].pid == pid && ships[i].active) {
            ships[i].active = 0;
            active_entities--;
            break;
        }
    }
}

// --- EL FIN DEL MUNDO ---
void trigger_apocalypse() {
    if (apocalypse_triggered) return;
    apocalypse_triggered = 1;
    fprintf(stderr, "\nÚRSULA: ¡Mi tesoro se ha acabado, el mundo ha llegado asu fin!\n");
    // Mandar señal de muerte a todas las capitanas
    for (int i = 0; i < num_captains; i++) {
        if (captains[i] > 0) {
            kill(captains[i], SIGINT); 
        }
    }
}

// --- LÓGICA DE BATALLAS ---
void handle_move(pid_t pid, int x, int y, int food, int gold) {
    // 1. Actualizar el barco que se acaba de mover
    int ship_idx = -1;
    for (int i = 0; i < num_ships; i++) {
        if (ships[i].pid == pid && ships[i].active) {
            ships[i].x = x; ships[i].y = y; 
            ships[i].food = food; ships[i].gold = gold;
            ship_idx = i;
            break;
        }
    }
    if (ship_idx == -1) return;


    // 2. Buscar si hay más barcos en esa misma casilla
    int fighters[MAX_SHIPS];
    int num_fighters = 0;
    for (int i = 0; i < num_ships; i++) {
        if (ships[i].active && ships[i].x == x && ships[i].y == y) {
            fighters[num_fighters++] = i;
        }
    }

    // 3. ¡HAY PELEA!
    if (num_fighters > 1) {
        fprintf(stderr, ROJO"\n¡PELEA en (%d, %d) entre %d barcos!\n"RESET, x, y, num_fighters);
        
        // Elegir un ganador al azar 
        int winner_idx = rand() % num_fighters;
        int total_collected = 0;

        for (int i = 0; i < num_fighters; i++) {
            if (i != winner_idx) {
                int loser = fighters[i];
                // Mandamos el castigo por señal
                kill(ships[loser].pid, SIGUSR2);
                
                // Calculamos cuánto oro le hemos podido robar al perdedor
                int gold_taken = (ships[loser].gold >= 10) ? 10 : ships[loser].gold;
                total_collected += gold_taken;
                
                // Actualizamos la estructura de Úrsula temporalmente
                ships[loser].gold -= gold_taken;
            }
        }

        // Mandamos el premio al ganador
        kill(ships[fighters[winner_idx]].pid, SIGUSR1);
        ships[fighters[winner_idx]].gold += 10;

        fprintf(stderr, VERDE"Ganador: Barco %d.\n"RESET, ships[fighters[winner_idx]].pid);

        // Ajustar el tesoro de Úrsula
        if (total_collected >= 10) {
            ursula_treasure += (total_collected - 10);
        } else {
            ursula_treasure -= (10 - total_collected);
        }
        
        fprintf(stderr, AMARILLO"Tesoro de Úrsula: %d monedas de oro.\n\n"RESET, ursula_treasure);

        // ¿Se acabó el dinero?
        if (ursula_treasure ==  0) {
            trigger_apocalypse();
        }
    }
}

// --- PARSEADOR DE MENSAJES DE LA TUBERÍA ---
void parse_message(char *msg) {
    char *token = strtok(msg, ", ");
    if (!token) return;
    pid_t pid = strtol(token, NULL, 10);

    token = strtok(NULL, ", \n");
    if (!token) return;

    if (strcmp(token, "INIT_CAPT") == 0) {
        register_captain(pid);
    } else if (strcmp(token, "END_CAPT") == 0) {
        unregister_captain(pid);
    } else if (strcmp(token, "TERMINATE") == 0) {
        unregister_ship(pid);
    } else if (strcmp(token, "INIT") == 0 || strcmp(token, "MOVE") == 0) {
        int is_move = (strcmp(token, "MOVE") == 0);
        
        token = strtok(NULL, ", "); if(!token) return; int x = strtol(token, NULL, 10);
        token = strtok(NULL, ", "); if(!token) return; int y = strtol(token, NULL, 10);
        token = strtok(NULL, ", "); if(!token) return; int food = strtol(token, NULL, 10);
        token = strtok(NULL, ", "); if(!token) return; int gold = strtol(token, NULL, 10);

        if (is_move) handle_move(pid, x, y, food, gold);
        else register_ship(pid, x, y, food, gold);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <nombre_tuberia>\n", argv[0]);
        return 1;
    }

    srand(time(NULL)); // Inicializar al azar
    char *fifo_name = argv[1];

    if (mkfifo(fifo_name, 0666) == -1 && errno != EEXIST) {
        perror("Error al crear la tubería de Úrsula");
        return 1;
    }

    fprintf(stderr, "Úrsula: Esperando capitanas y barcos... '%s'...\n", fifo_name);

    int fd = open(fifo_name, O_RDWR);
    if (fd == -1) {
        perror("Error al abrir la tubería");
        exit(1);
    }

    char buffer[MAX_BUFFER];
    char c;
    int i = 0;
    int has_started = 0;

    while (read(fd, &c, 1) > 0) {
        if (c == '\n') {
            buffer[i] = '\0';
            
            // --- Imprimir lo que recibe ---
            fprintf(stderr, "%s\n", buffer);
            
            // --- 2. Hacemos una copia para que strtok no rompa el texto original ---
            char msg_copy[MAX_BUFFER];
            strcpy(msg_copy, buffer);
            
            parse_message(msg_copy); // Analiza la copia, no el original
            
            has_started = 1;
            i = 0;
            
            // Si ya no quedan barcos ni capitanas vivas, Úrsula se va a dormir
            if (has_started && active_entities <= 0) {
                break;
            }
        } else {
            if (i < MAX_BUFFER - 1) buffer[i++] = c;
        }
    }

    close(fd);
    fprintf(stderr, "Ya no quedan barcos vivos. Terminando Ursula... \n");
    return 0;
}
