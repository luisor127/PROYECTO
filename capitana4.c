#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h> 

typedef struct {
    int id;
    int x;
    int y;
    int speed;
    pid_t pid; 
	int is_alive; // [NUEVO] 1 = Vivo, 0 = Terminado
    int food;     // [NUEVO]
    int gold;     // [NUEVO]
} ShipInfo;

ShipInfo *fleet = NULL;
int ship_count = 0;
int ships_alive = 0;

void handle_sigchld(int sig) {
    int status;
    pid_t pid;

    // waitpid con WNOHANG recoge al hijo sin bloquear a la Capitana
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        ships_alive--;
       if (WIFEXITED(status)) {
            for(int k=0; k<ship_count; k++) {
                if(fleet[k].pid == pid) {
                    fleet[k].is_alive = 0; // <--- CÁMBIA ESTO (Antes era pid = -1)
                    fprintf(stderr, "Capitana recibió SIGCHLD\nBarco %d con PID %d salió con estado %d\n", 
                            fleet[k].id, pid, WEXITSTATUS(status));
                }
            }
        }
    }
}

// [CAMBIO 3] Manejador de SIGINT: Se activa con Ctrl+C
void handle_sigint(int sig) {
    fprintf(stderr, "\nCapitana: Recibido SIGINT. Finalizando flota...\n");
    for (int i = 0; i < ship_count; i++) {
        if (fleet[i].pid > 0) {
            kill(fleet[i].pid, SIGQUIT); // Enviamos SIGQUIT a los barcos 
        }
    }
}

int main(int argc, char *argv[]) {
    srand(time(NULL));
    char *map_file = "map.txt";
    char *ships_file = "ships.txt";
    int random_mode = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--map") == 0 && i + 1 < argc) 
            map_file = argv[++i];
        else if (strcmp(argv[i], "--ships") == 0 && i + 1 < argc) 
            ships_file = argv[++i];
        else if (strcmp(argv[i], "--random") == 0) 
            random_mode = 1;
    }

    fprintf(stderr, "Captain: Capitana Amina al-Sirafi, con PID %d\n", getpid());

    // --- LECTURA DE FICHERO (Tu lógica de getline/strtol) ---
    FILE *f = fopen(ships_file, "r");
    if (!f) { perror("Error abriendo fichero"); return 1; }

    char *line = NULL;
    size_t len = 0;
    while (getline(&line, &len, f) > 1) ship_count++;
    
    rewind(f);
    fleet = malloc(sizeof(ShipInfo) * ship_count);
    int i = 0;
    while (getline(&line, &len, f) > 1 && i < ship_count) {
        char *ptr = line;
        char *endptr;
        fleet[i].id = strtol(ptr, &endptr, 10);
        ptr = strchr(endptr, '(') + 1;
        fleet[i].x = strtol(ptr, &endptr, 10);
        ptr = strchr(endptr, ',') + 1;
        fleet[i].y = strtol(ptr, &endptr, 10);
        ptr = strchr(endptr, ')') + 1;
        fleet[i].speed = strtol(ptr, &endptr, 10);
        fleet[i].pid = 0;
        i++;
    }
    free(line);
    fclose(f);

    // [CAMBIO 4] Registrar señales
    signal(SIGINT, handle_sigint);
    signal(SIGCHLD, handle_sigchld);

    // --- LANZAR BARCOS ---

	// [Arrays para las tuberías (Pipes)
    // c2s: Capitana to Ship (Capitana escribe, Barco lee)
    // s2c: Ship to Capitana (Barco escribe, Capitana lee)
    int (*pipes_c2s)[2] = malloc(sizeof(int[2]) * ship_count);
    int (*pipes_s2c)[2] = malloc(sizeof(int[2]) * ship_count);
for (int i = 0; i < ship_count; i++) {
        
        // [PASO 2] Creamos las dos tuberías para este barco en concreto
        pipe(pipes_c2s[i]);
        pipe(pipes_s2c[i]);

        int pasos = random_mode ? (rand() % 11) + 10 : 0;
        pid_t pid = fork();

        if (pid == 0) {
            // --- CÓDIGO DEL HIJO (BARCO) ---
            
            // 1. Conectamos la lectura del barco (STDIN) al pipe de la capitana
            dup2(pipes_c2s[i][0], STDIN_FILENO);
            // 2. Conectamos la escritura del barco (STDOUT) al pipe hacia la capitana
            dup2(pipes_s2c[i][1], STDOUT_FILENO);

            // 3. IMPORTANTE: Cerramos todos los extremos originales de las tuberías.
            // Hay que cerrar tanto las del barco actual (i) como las de los barcos anteriores (j)
            for (int j = 0; j <= i; j++) {
                close(pipes_c2s[j][0]); 
                close(pipes_c2s[j][1]);
                close(pipes_s2c[j][0]); 
                close(pipes_s2c[j][1]);
            }

            char sx[12], sy[12], sf[12], st[12], ss[12];
            sprintf(sx, "%d", fleet[i].x); 
            sprintf(sy, "%d", fleet[i].y);
            sprintf(sf, "100"); 

            if (random_mode) {
                // MODO AUTOMÁTICO
                sprintf(st, "%d", pasos); 
                sprintf(ss, "%d", fleet[i].speed);
                execl("./ship3", "ship3", "--map", map_file, "--pos", sx, sy, "--food", sf, "--random", st, ss, NULL);
            } else {
                // MODO CAPITANA (Parte 4)
                execl("./ship3", "ship3", "--map", map_file, "--pos", sx, sy, "--food", sf, "--captain", NULL);
            }
            
            perror("Error al hacer execl"); // Por si no encuentra el archivo ship3
            exit(1);
        
       } else {
    // --- CÓDIGO DEL PADRE (CAPITANA) ---
    		fleet[i].pid = pid;
            fleet[i].is_alive = 1; // El barco nace vivo
            fleet[i].food = 100;   // Comida inicial
            fleet[i].gold = 0;     // Oro inicial
            ships_alive++;
            
            fprintf(stderr,"Barco ID: %d, Posición Inicial: (%d, %d), Velocidad: %d\n", fleet[i].id, fleet[i].x, fleet[i].y, fleet[i].speed);
            
            close(pipes_c2s[i][0]);
            close(pipes_s2c[i][1]);
        }
    }


 
    
// ... (Después del bucle for que hace los forks) ...

    if (random_mode) {
        // MODO AUTOMÁTICO: La capitana solo espera a que todos mueran
        while (ships_alive > 0) {
            pause(); 
        }
    } else {
        // MODO CAPITANA INTERACTIVO (Paso 4)
        char buffer[100];
        char c;
        int idx_buf = 0;

        fprintf(stderr, "Introducir comando [exit | status | (Num, up/down/right/left/exit]:\n");

        while (ships_alive > 0 && read(STDIN_FILENO, &c, 1) > 0) {
            if (c == '\n') {
                buffer[idx_buf] = '\0';
                idx_buf = 0;

                if (strlen(buffer) == 0) {
                    fprintf(stderr, "Introducir comando [exit | status | (Num, up/down/right/left/exit]:\n");
                    continue;
                }

                if (strcmp(buffer, "exit") == 0) {
                    fprintf(stderr, "Saliendo y terminando todos los barcos.\n");
                    for (int j = 0; j < ship_count; j++) {
                        if (fleet[j].is_alive) {
                            kill(fleet[j].pid, SIGQUIT);
                        }
                    }
                    while(ships_alive > 0) { pause(); }
                    break; 
                }
                else if (strcmp(buffer, "status") == 0) {
                    for (int j = 0; j < ship_count; j++) {
                        fprintf(stderr, "Barco %d %s (ID: %d, PID: %d) En: (%d, %d) Comida: %d Oro: %d\n", 
                                j + 1, 
                                fleet[j].is_alive ? "Vivo" : "Terminado", 
                                fleet[j].id, fleet[j].pid, fleet[j].x, fleet[j].y, 
                                fleet[j].food, fleet[j].gold);
                    }
                    fprintf(stderr, "número de barcos vivos: %d\n", ships_alive);
                }
                else {
                    char *espacio = strchr(buffer, ' ');
                    if (espacio != NULL) {
                        *espacio = '\0';
                        char *cmd = espacio + 1;
                        char *endptr;
                        int id_barco = strtol(buffer, &endptr, 10);
                        
                        int idx_barco = -1;
                        for (int j = 0; j < ship_count; j++) {
                            if (fleet[j].id == id_barco && fleet[j].is_alive) {
                                idx_barco = j;
                                break;
                            }
                        }

                        if (idx_barco != -1) {
                            int nx = fleet[idx_barco].x;
                            int ny = fleet[idx_barco].y;
                            int is_move = 0;

                            if (strcmp(cmd, "up") == 0) { ny--; is_move = 1; }
                            else if (strcmp(cmd, "down") == 0) { ny++; is_move = 1; }
                            else if (strcmp(cmd, "right") == 0) { nx++; is_move = 1; }
                            else if (strcmp(cmd, "left") == 0) { nx--; is_move = 1; }

                            if (is_move) {
                                int colision = 0;
                                for (int k = 0; k < ship_count; k++) {
                                    if (k != idx_barco && fleet[k].is_alive && fleet[k].x == nx && fleet[k].y == ny) {
                                        colision = 1; break;
                                    }
                                }

                                if (colision) {
                                    fprintf(stderr, "Mover %s para barco %d no es posible debido a colisión.\n", cmd, id_barco);
                                } else {
                                    fprintf(stderr, "Enviando acción %s al barco %d\n", cmd, id_barco);
                                    write(pipes_c2s[idx_barco][1], cmd, strlen(cmd));
                                    write(pipes_c2s[idx_barco][1], "\n", 1);
                                    
                                    char r_c; // Esperar OK/NOK
                                    while (read(pipes_s2c[idx_barco][0], &r_c, 1) > 0 && r_c != '\n');
                                    
                                    fleet[idx_barco].x = nx;
                                    fleet[idx_barco].y = ny;
                                    fleet[idx_barco].food -= 5;
                                    fprintf(stderr, "Barco %d nueva posición: (%d, %d)\n", id_barco, nx, ny);
                                }
                                fprintf(stderr, "número de barcos vivos: %d\n", ships_alive);
                            } else if (strcmp(cmd, "exit") == 0) {
                                fprintf(stderr, "Enviando acción exit al barco %d\n", id_barco);
                                write(pipes_c2s[idx_barco][1], "exit\n", 5);
                                fprintf(stderr, "número de barcos vivos: %d\n", ships_alive);
                            } else {
                                fprintf(stderr, "Comando no válido.\n");
                            }
                        } else {
                            fprintf(stderr, "Capitana: El barco con ID %d no es válido.\n", id_barco);
                        }
                    }
                }
                if (ships_alive > 0) {
                    fprintf(stderr, "Introducir comando [exit | status | (Num, up/down/right/left/exit]:\n");
                }
            } else {
                if (idx_buf < 99) buffer[idx_buf++] = c;
            }
        }
    }

    // --- LIMPIEZA FINAL ---
    free(fleet);
    free(pipes_c2s);
    free(pipes_s2c);
    return 0;
}
