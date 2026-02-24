#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h> // <--- AÑADE ESTA LÍNEA AQUÍ

#define RESET "\033[0m"
#define ROJO "\033[31m"
#define VERDE "\033[32m"
#define AMARILLO "\033[33m"
#define AZUL "\033[34m"
#define MAGENTA "\033[35m"

#define MAX_SHIPS 200
#define MAX_CAPTAINS 20

// Estructura para registrar a los barcos
typedef struct {
    pid_t pid;
    int x;
    int y;
    int food;
    int gold;
    int active;
} ShipRecord;

// Estructura para registrar a las capitanas
typedef struct {
    pid_t pid;
    int active;
} CaptainRecord;

// Variables globales de Úrsula
ShipRecord ships[MAX_SHIPS];
CaptainRecord captains[MAX_CAPTAINS];

int ursula_gold = 100; // Tesoro inicial
int active_ships_count = 0;
int active_captains_count = 0;
int simulation_started = 0; // Para saber si ya hemos recibido algún INIT

// Busca un barco en el array por su PID, o devuelve un hueco libre (-1)
int find_ship_index(pid_t pid) {
    int free_idx = -1;
    for (int i = 0; i < MAX_SHIPS; i++) {
        if (ships[i].active && ships[i].pid == pid) return i;
        if (!ships[i].active && free_idx == -1) free_idx = i;
    }
    return free_idx;
}

// Busca una capitana en el array
int find_captain_index(pid_t pid) {
    int free_idx = -1;
    for (int i = 0; i < MAX_CAPTAINS; i++) {
        if (captains[i].active && captains[i].pid == pid) return i;
        if (!captains[i].active && free_idx == -1) free_idx = i;
    }
    return free_idx;
}

// Lógica de batalla
void check_battles(int x, int y) {
    int fighters[MAX_SHIPS];
    int num_fighters = 0;

    // Buscamos cuántos barcos hay en la misma coordenada
    for (int i = 0; i < MAX_SHIPS; i++) {
        if (ships[i].active && ships[i].x == x && ships[i].y == y) {
            fighters[num_fighters++] = i;
        }
    }

    // Si hay 2 o más, ¡BATALLA!
    if (num_fighters >= 2) {
        printf(ROJO "\n¡BATALLA EN (%d, %d)! Participan %d barcos.\n" RESET, x, y, num_fighters);
        
        // 1. Elegir ganador aleatorio
        int winner_idx = rand() % num_fighters;
        int winner_id = fighters[winner_idx];
        
        int oro_recaudado = 0;

        // 2. Aplicar penalizaciones a los perdedores
        for (int i = 0; i < num_fighters; i++) {
            if (i != winner_idx) {
                int loser_id = fighters[i];
                
                // Reducir comida del perdedor
                if (ships[loser_id].food < 10) ships[loser_id].food = 0;
                else ships[loser_id].food -= 10;

                // Reducir oro y sumar a la recaudación
                int oro_aportado = 0;
                if (ships[loser_id].gold >= 10) {
                    oro_aportado = 10;
                    ships[loser_id].gold -= 10;
                } else {
                    oro_aportado = ships[loser_id].gold;
                    ships[loser_id].gold = 0;
                }
                oro_recaudado += oro_aportado;
                printf("  -> Perdedor PID %d aportó %d oro.\n", ships[loser_id].pid, oro_aportado);
            }
        }

        // 3. Repartir el botín
        ships[winner_id].gold += 10;
        ursula_gold += (oro_recaudado - 10); // Puede ser negativo y restarle a Úrsula

        printf(VERDE "  -> Ganador PID %d recibe 10 oro.\n" RESET, ships[winner_id].pid);
        printf(AMARILLO "  -> Tesoro de Úrsula actualizado a: %d oro.\n\n" RESET, ursula_gold);

        // 4. ¿Fin del mundo? (Úrsula arruinada)
        if (ursula_gold < 0) {
            printf(MAGENTA "\n¡ÚRSULA ESTÁ ARRUINADA! EL MUNDO HA LLEGADO A SU FIN.\n" RESET);
            // Avisar a todas las capitanas (en tu código SIGINT mata a todos)
            for (int i = 0; i < MAX_CAPTAINS; i++) {
                if (captains[i].active) {
                    kill(captains[i].pid, SIGINT);
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <nombre_tuberia>\n", argv[0]);
        return 1;
    }

    char *pipe_name = argv[1];

    // Inicializar arrays
    for (int i = 0; i < MAX_SHIPS; i++) ships[i].active = 0;
    for (int i = 0; i < MAX_CAPTAINS; i++) captains[i].active = 0;

    srand(time(NULL) ^ getpid());

    // 1. Crear el Named Pipe
    // Usamos unlink por si ya existía de una ejecución anterior fallida
    unlink(pipe_name); 
    if (mkfifo(pipe_name, 0666) == -1) {
        perror("Error creando la tubería de Úrsula");
        return 1;
    }

    printf("Úrsula la Señora del Mar (PID: %d) despertando...\n", getpid());
    printf("Esperando mensajes en la tubería: %s\n", pipe_name);
    printf("Tesoro inicial: " AMARILLO "%d\n" RESET, ursula_gold);

    // TRUCO: Abrimos con O_RDWR (Lectura y Escritura) en lugar de O_RDONLY. 
    // Esto evita que 'fgets' devuelva EOF inmediatamente cuando no hay escritores conectados.
    int fd = open(pipe_name, O_RDWR);
    if (fd == -1) {
        perror("Error abriendo la tubería");
        return 1;
    }

    FILE *f = fdopen(fd, "r");
    char line[512];

    // 2. Bucle principal de lectura
    while (fgets(line, sizeof(line), f)) {
        // Limpiamos el salto de línea
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) == 0) continue;

        int pid;
        char tipo[32];
        
        // Primero intentamos extraer el PID y el TIPO
        if (sscanf(line, "%d,%31[^,]", &pid, tipo) >= 2) {
            
            // --- MENSAJES DE CAPITANAS ---
            if (strcmp(tipo, "INIT_CAPT") == 0) {
                int idx = find_captain_index(pid);
                if (idx != -1) {
                    captains[idx].pid = pid;
                    captains[idx].active = 1;
                    active_captains_count++;
                    simulation_started = 1;
                    printf("Capitana unida. PID: %d. Activas: %d\n", pid, active_captains_count);
                }
            } 
            else if (strcmp(tipo, "END_CAPT") == 0) {
                int idx = find_captain_index(pid);
                if (idx != -1 && captains[idx].active) {
                    captains[idx].active = 0;
                    active_captains_count--;
                    printf("Capitana finalizada. PID: %d. Activas: %d\n", pid, active_captains_count);
                }
            } 
            
            // --- MENSAJES DE BARCOS ---
            else if (strcmp(tipo, "TERMINATE") == 0) {
                int idx = find_ship_index(pid);
                if (idx != -1 && ships[idx].active) {
                    ships[idx].active = 0;
                    active_ships_count--;
                    printf("Barco hundido/finalizado. PID: %d. Activos: %d\n", pid, active_ships_count);
                }
            } 
            else if (strcmp(tipo, "INIT") == 0 || strcmp(tipo, "MOVE") == 0) {
                // Para INIT y MOVE necesitamos leer los demás datos
                int x, y, food, gold;
                sscanf(line, "%d,%[^,],%d,%d,%d,%d", &pid, tipo, &x, &y, &food, &gold);
                
                int idx = find_ship_index(pid);
                if (idx != -1) {
                    ships[idx].pid = pid;
                    ships[idx].x = x;
                    ships[idx].y = y;
                    ships[idx].food = food;
                    ships[idx].gold = gold;
                    
                    if (strcmp(tipo, "INIT") == 0 && !ships[idx].active) {
                        ships[idx].active = 1;
                        active_ships_count++;
                        printf("Barco %d listo en (%d, %d). Activos: %d\n", pid, x, y, active_ships_count);
                    } 
                    else if (strcmp(tipo, "MOVE") == 0) {
                        printf(AZUL "MOVIMIENTO: " RESET "Barco %d se ha movido a (%d, %d). Comida: %d, Oro: %d\n", 
                               pid, x, y, food, gold);
                        // Cuando un barco se mueve, Úrsula comprueba si hay batalla
                        check_battles(x, y);
                    }
                }
            }
        }

        // Condición de finalización de Úrsula: 
        // Si el juego ya ha empezado y no quedan barcos ni capitanas vivas
        if (simulation_started && active_captains_count == 0 && active_ships_count == 0) {
            printf("\nÚrsula: No quedan más barcos ni capitanas. Cerrando el mar.\n");
            break;
        }
    }

    // Limpieza
    fclose(f);
    unlink(pipe_name); // Destruir la tubería
    return 0;
}
