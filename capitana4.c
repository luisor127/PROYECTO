#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>

#define RESET "\033[0m"
#define ROJO "\033[31m"
#define VERDE "\033[32m"
#define AMARILLO "\033[33m"
#define AZUL "\033[34m"
#define NARANJA "\033[38;5;208m"
#define MAGENTA "\033[35m"

typedef struct {
    int id;
    int x;
    int y;
    int speed;
    pid_t pid; 

    //AQUI EMPIEZA LA PARTE 4
    int fd_write; // Tubo por donde la Capitana ESCRIBE al barco
    int fd_read;  // Tubo por donde la Capitana LEE la respuesta del barco
    int food;     // Para el comando status
    int gold;     // Para el comando status
} ShipInfo;

ShipInfo *fleet = NULL;
int ship_count = 0;
int ships_alive = 0;

// Variable global para la tubería de Úrsula
int ursula_fd = -1;

void handle_sigchld(int sig) {
    int status;
    pid_t pid;

    // Recoge al hijo sin bloquear a la Capitana
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {

        ships_alive--;
        if (WIFEXITED(status)) {

            int s_id = -1;

            for(int k=0; k<ship_count; k++) {

                if(fleet[k].pid == pid) {

                    s_id = fleet[k].id;
                    fleet[k].pid = -fleet[k].pid;
                }    
            }

            fprintf(stderr, "\nCapitana: Barco %d (PID %d) terminó con estado %d. Quedan %d vivos.\n", 
                    s_id, pid, WEXITSTATUS(status), ships_alive);
        }
    }

    if (ships_alive <= 0) {

        // --- Despedirse de Úrsula ---
        if (ursula_fd != -1) {
            char msg[256];
            sprintf(msg, "%d, END_CAPT\n", getpid());
            write(ursula_fd, msg, strlen(msg));
            close(ursula_fd);
        }

        fprintf(stderr, "\nCapitana: Todos los barcos han terminado. Saliendo.\n");
        exit(0); // Fuerza a la Capitana a cerrarse y devolverte la terminal
    }
}

// Manejador de SIGINT: Se activa con Ctrl+C
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
    char *ursula_fifo = NULL;
    int random_mode = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--map") == 0 && i + 1 < argc) 
            map_file = argv[++i];
        else if (strcmp(argv[i], "--ships") == 0 && i + 1 < argc) 
            ships_file = argv[++i];
        else if (strcmp(argv[i], "--random") == 0) 
            random_mode = 1;
        else if (strcmp(argv[i], "--ursula") == 0) {
            if (i + 1 < argc) {
                ursula_fifo = argv[++i];
            }
        }    
    }

    fprintf(stderr, "Captain: Capitana Amina al-Sirafi, con PID %d\n", getpid());

    // --- LECTURA DE FICHERO ---
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

        //NUEVO PARTE 4
        fleet[i].food = 100; // Barcos al inicio tienen 100 de comida
        fleet[i].gold = 0;   // Barcos al inicio tienen 0 de oro

        i++;
    }
    free(line);
    fclose(f);

    // Registrar señales
    signal(SIGINT, handle_sigint);
    signal(SIGCHLD, handle_sigchld);

    // --- NUEVO PARA LA CAPITANA ---
    signal(SIGTSTP, SIG_IGN); 
    signal(SIGQUIT, SIG_IGN); 


    // --- CONECTAR CON ÚRSULA ---
    if (ursula_fifo != NULL) {
        // Abrimos la tubería en modo ESCRITURA
        ursula_fd = open(ursula_fifo, O_WRONLY);
        if (ursula_fd == -1) {
            perror("Error: No se pudo conectar con Úrsula. ¿Está despierta?");
        } else {
            char msg[256];
            // Mandamos el saludo inicial: <PID>, INIT_CAPT
            sprintf(msg, "%d, INIT_CAPT\n", getpid());
            write(ursula_fd, msg, strlen(msg));
        }
    }


    // --- LANZAR BARCOS (CON TUBERIAS) ---
    for (int i = 0; i < ship_count; i++) {
        int pasos = random_mode ? (rand() % 11) + 10 : 0;
        
        // 1. Declaramos las tuberías
        int pipe_c2s[2]; // Capitana -> Barco
        int pipe_s2c[2]; // Barco -> Capitana
        
        // Solo las creamos si NO estamos en modo random
        if (!random_mode) {
            if (pipe(pipe_c2s) == -1 || pipe(pipe_s2c) == -1) {
                perror("Error creando tuberías");
                exit(1);
            }
        }

        pid_t pid = fork();

        if (pid == 0) {
           
            char sx[12], sy[12], sf[12], st[12], ss[12];
            sprintf(sx, "%d", fleet[i].x); 
            sprintf(sy, "%d", fleet[i].y);
            sprintf(sf, "100"); 

            if (random_mode) {
                // MODO AUTOMÁTICO (Sin tuberías)
                sprintf(st, "%d", pasos); 
                sprintf(ss, "%d", fleet[i].speed);
            
                // --- Pasamos Úrsula si existe ---
                if (ursula_fifo != NULL) {
                    execl("./ship3", "ship3", "--map", map_file, "--pos", sx, sy, "--food", sf, "--random", st, ss, "--ursula", ursula_fifo, NULL);
                } else {
                    execl("./ship3", "ship3", "--map", map_file, "--pos", sx, sy, "--food", sf, "--random", st, ss, NULL);
                }

            } else {
                // MODO CAPITANA
                
                // Cerramos los extremos del tubo que el barco no va a usar
                close(pipe_c2s[1]); // El barco NO escribe en el tubo de ida
                close(pipe_s2c[0]); // El barco NO lee del tubo de vuelta

                // Conectamos al tubo de lectura
                dup2(pipe_c2s[0], STDIN_FILENO);
                close(pipe_c2s[0]); 

                // Conectamos al tubo de escritura
                dup2(pipe_s2c[1], STDOUT_FILENO);
                close(pipe_s2c[1]);
                
                // --- Pasamos Úrsula si existe ---
                if (ursula_fifo != NULL) {
                    execl("./ship3", "ship3", "--map", map_file, "--pos", sx, sy, "--food", sf, "--captain", "--ursula", ursula_fifo, NULL);
                } else {
                    execl("./ship3", "ship3", "--map", map_file, "--pos", sx, sy, "--food", sf, "--captain", NULL);
                }
            }
            
            perror("Error al hacer execl"); 
            exit(1);
        
        } else {
            
            fleet[i].pid = pid;
            ships_alive++;

            if (!random_mode) {
                // Cerramos los extremos que la Capitana no usa
                close(pipe_c2s[0]); // La Capitana NO lee por el tubo de ida
                close(pipe_s2c[1]); // La Capitana NO escribe por el tubo de vuelta

                // Guardamos los tubos en la estructura para usarlos luego
                fleet[i].fd_write = pipe_c2s[1]; // Por aquí le mandará órdenes (up, down...)
                fleet[i].fd_read = pipe_s2c[0];  // Por aquí leerá las respuestas (OK, NOK)
            }
        }
    }

    // Bucle de espera no bloqueante
    fprintf(stderr, "Capitana: Flota lanzada. Esperando eventos...\n");

// --- IMPRIMIR ESTADO INICIAL ---
    fprintf(stderr, "\n=== ESTADO INICIAL DE LA FLOTA ===\n");
    for (int j = 0; j < ship_count; j++) {
        fprintf(stderr, "Barco %d Vivo (ID: %d, PID: %d) En: (%d, %d) Comida: %d Oro: %d\n", 
                j + 1, fleet[j].id, fleet[j].pid, fleet[j].x, fleet[j].y, fleet[j].food, fleet[j].gold);
    }
    fprintf(stderr, "Número de barcos vivos: %d\n", ships_alive);
    fprintf(stderr, "==================================\n");

   // Bucle principald e la Capitana
    if (random_mode) {
        // MODO AUTOMÁTICO
        fprintf(stderr, "Capitana: Flota lanzada en modo automático. Esperando eventos...\n");
        while (ships_alive > 0) {
            pause(); 
        }
    } else {
        // MODO CAPITAN
        char buffer[256];
        
        while (ships_alive > 0) {
            fprintf(stderr, "\nIntroducir comando [exit | status | (Num, up/down/right/left/exit)]:\n> ");
            
            // Leemos del teclado
            if (fgets(buffer, sizeof(buffer), stdin) == NULL) break;
            
            // Limpiamos el salto de línea que mete el Enter
            buffer[strcspn(buffer, "\n")] = 0; 
            if (strlen(buffer) == 0) continue; 
            
            // --- EXIT GENERAL ---
            if (strcmp(buffer, "exit") == 0) {
                fprintf(stderr, "Saliendo y terminando todos los barcos.\n");
                for (int i = 0; i < ship_count; i++) {
                    if (fleet[i].pid > 0) {
                        kill(fleet[i].pid, SIGQUIT); // Matamos a todos
                    }
                }
                while (ships_alive > 0) { pause(); } // Esperamos a que mueran
                break;
            } 
            // --- STATUS ---
            else if (strcmp(buffer, "status") == 0) {
                for (int i = 0; i < ship_count; i++) {
                    if (fleet[i].pid > 0) {
                        
                        // 1. Le mandamos el ping oculto al barco por la tubería
                        write(fleet[i].fd_write, "status\n", 7);
                        
                        // 2. Leemos su respuesta real actualizada
                        char respuesta[256] = {0};
                        int bytes = read(fleet[i].fd_read, respuesta, sizeof(respuesta) - 1);
                        if (bytes > 0) {
                            respuesta[bytes] = '\0';
                            char *ok_ptr = strstr(respuesta, "OK");
                            if (ok_ptr != NULL) {
                                ok_ptr += 2;
                                char *endptr;
                                fleet[i].food = (int)strtol(ok_ptr, &endptr, 10);
                                fleet[i].gold = (int)strtol(endptr, NULL, 10);
                            }
                        }
                        
                        // 3. Imprimimos los datos
                        fprintf(stderr, "Barco %d Vivo (ID: %d, PID: %d) En: (%d, %d) "AZUL "Comida: %d"RESET""AMARILLO"  Oro: %d\n"RESET, 
                                i+1, fleet[i].id, fleet[i].pid, fleet[i].x, fleet[i].y, fleet[i].food, fleet[i].gold);
                    } else {
                        // Si está muerto, imprimimos lo último que supimos
                        fprintf(stderr, "Barco %d Terminado (ID: %d, PID: %d) En: (%d, %d) "AZUL "Comida: %d"RESET""AMARILLO"  Oro: %d\n"RESET, 
                                i+1, fleet[i].id, -fleet[i].pid, fleet[i].x, fleet[i].y, fleet[i].food, fleet[i].gold);
                    }
                }
            }
    
            // --- <ID> <ACCIÓN> ---
            else {
                int target_id;
                char cmd[50] = {0};
                
                // 1. Cogemos el número
                char *endptr;
                target_id = (int)strtol(buffer, &endptr, 10);
                
                // 2. Comprobamos si realmente leyó un número (el puntero avanzó)
                if (endptr != buffer) {
                    
                    // Saltamos los espacios en blanco que haya entre el número y la palabra
                    while (*endptr == ' ' || *endptr == '\t') {
                        endptr++;
                    }
                    
                    // 3. Si queda texto, lo metemos en el array 'cmd'
                    if (*endptr != '\0') {
                        strncpy(cmd, endptr, sizeof(cmd) - 1);
                        cmd[sizeof(cmd) - 1] = '\0'; // Cierre 

                        if (strcmp(cmd, "up") != 0 && strcmp(cmd, "down") != 0 && 
                            strcmp(cmd, "left") != 0 && strcmp(cmd, "right") != 0 && 
                            strcmp(cmd, "exit") != 0) {
                            
                            fprintf(stderr, "ERROR, el comando que has introducido es invalido\n");
                            continue; // evita que se imprima el número de barcos vivos abajo del todo.
                        }

                    // Buscamos a qué índice de nuestro array corresponde esa ID
                    int idx = -1;
                    for (int i = 0; i < ship_count; i++) {
                        if (fleet[i].id == target_id && fleet[i].pid > 0) {
                            idx = i; 
                            break;
                        }
                    }
                    
                    if (idx == -1) {
                        fprintf(stderr, "Error: Barco no encontrado o ya está muerto.\n");
                        continue;
                    }

                    // --- ANTI-COLISIONES ENTRE HERMANOS ---
                    int nx = fleet[idx].x;
                    int ny = fleet[idx].y;
                    if (strcmp(cmd, "up") == 0) ny--;
                    else if (strcmp(cmd, "down") == 0) ny++;
                    else if (strcmp(cmd, "left") == 0) nx--;
                    else if (strcmp(cmd, "right") == 0) nx++;
                    
                    int colision = 0;
                    if (strcmp(cmd, "exit") != 0) {
                        for (int i = 0; i < ship_count; i++) {
                            if (i != idx && fleet[i].pid > 0 && fleet[i].x == nx && fleet[i].y == ny) {
                                colision = 1;
                                break;
                            }
                        }
                    }

                    if (colision) {
                        fprintf(stderr, "Mover %s para barco %d no es posible debido a colisión.\n", cmd, target_id);
                        fprintf(stderr, "Número de barcos vivos: %d\n", ships_alive);
                        continue;
                    }

                    // --- MANDAR ORDEN POR LA TUBERÍA ---
                    fprintf(stderr, "Enviando acción %s al barco %d\n", cmd, target_id);
                    
                    char msg[60];
                    sprintf(msg, "%s\n", cmd); // Le metemos \n para que el barco detecte el Enter
                    write(fleet[idx].fd_write, msg, strlen(msg)); // Enviamos la orden por el tubo

                    // --- LEER RESPUESTA (OK/NOK) ---
                    if (strcmp(cmd, "exit") != 0) {
                        char respuesta[4096] = {0};
                        int found_response = 0;
                        
                        // Leemos hasta encontrar la respuesta
                        while (!found_response) {
                            char chunk[256];
                            int bytes = read(fleet[idx].fd_read, chunk, sizeof(chunk) - 1);
                            
                            if (bytes > 0) {
                                chunk[bytes] = '\0';
                                strcat(respuesta, chunk); 
                                
                                // Buscamos si el barco gritó NOK (chocó o no hay comida)
                                if (strstr(respuesta, "NOK") != NULL) {
                                    fprintf(stderr, ROJO"NOK"RESET);
                                    found_response = 1; // Salimos sin actualizar coordenadas
                                } 

                                else if (strstr(respuesta, "INVALID") != NULL) {
                                    // Descongelamos a la Capitana en silencio, sin imprimir nada
                                    fprintf(stderr, "ERROR, el comando que has introducido es invalido.\n");
                                    found_response = 1;
                                }

                                // Buscamos si el barco gritó OK (éxito) 
                                else if (strstr(respuesta, "OK") != NULL) {
                                    // 1. Actualizamos coordenadas
                                    fleet[idx].x = nx;
                                    fleet[idx].y = ny;
                                    
                                    // 2. Buscamos dónde empieza la palabra "OK"
                                    char *ok_ptr = strstr(respuesta, "OK");
                                    ok_ptr += 2; // Saltamos la 'O' y la 'K' para leer los números
                                    
                                    // 3. Cogemos la comida y el oro reales
                                    char *endptr;
                                    fleet[idx].food = (int)strtol(ok_ptr, &endptr, 10);
                                    fleet[idx].gold = (int)strtol(endptr, NULL, 10);
                                    
                                    // 4. Imprimimos el mensaje de éxito en pantalla
                                    fprintf(stderr, "Barco %d nueva posicion: (%d, %d)\n", target_id, nx, ny);
                                    found_response = 1;
                                }
                            } else {
                                break; // El tubo se rompió o el barco murió
                            }
                        }
                    }
                    
                    fprintf(stderr, "\nNúmero de barcos vivos: %d\n", ships_alive);

                } else {
                    fprintf(stderr, "Comando no válido. Formato: <id> <comando>\n");
                }
            }
        }
    }

    fprintf(stderr, "Capitana: Todos los barcos han terminado. Saliendo.\n");
    free(fleet);
    return 0;
    } 
}
