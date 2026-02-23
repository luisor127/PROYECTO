#include <stdio.h>
#include <stdlib.h> // Necesario para strtol
#include <unistd.h> // Necesario para read, getpid
#include <string.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <signal.h>
#include "map.h"

#define RESET "\033[0m"
#define ROJO "\033[31m"
#define VERDE "\033[32m"
#define AMARILLO "\033[33m"
#define AZUL "\033[34m"
#define NARANJA "\033[38;5;208m"
#define MAGENTA "\033[35m"

// Direcciones: {dx, dy} -> Abajo, Derecha, Arriba, Izquierda
int directions[4][2] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};

// Estructura para gestionar el estado del barco
typedef struct {
    Map *mapa;
    int x;
    int y;
    int food;
    int gold;
    pid_t pid;
} Ship;

// Variable GLOBAL
Ship ship; 

// Inicialización barco
void ship_init(Map *mapa, int x, int y, int food) {

    ship.mapa = mapa;
    ship.x = x;
    ship.y = y;
    ship.food = food;
    ship.gold = 0;
    ship.pid = getpid();
    map_set_ship(ship.mapa, ship.x, ship.y);
}

void ship_print() {

    fprintf(stderr, "Barco %d en (%d, %d) con "AZUL"%d de comida"RESET" y "AMARILLO"%d de oro.\n"RESET, 
            ship.pid, ship.x, ship.y, ship.food, ship.gold);
}

// Manejador de señales
void handle_signal(int sig) {

    switch (sig) {

        case SIGALRM:

            break; // Despierta al pause()

        case SIGUSR1:

            ship.gold += 10;
            fprintf(stderr, "Barco %d (SIGUSR1): "AMARILLO "Oro +10."RESET" Total: %d\n", ship.pid, ship.gold);
            break;

        case SIGUSR2:
        
            if (ship.food < 10) ship.food = 0;
                else ship.food -= 10;

            if (ship.gold < 10) ship.gold = 0;
                else ship.gold -= 10;

            fprintf(stderr, "Barco %d (SIGUSR2): "ROJO"Comida -10, Oro -10.\n"RESET, ship.pid);
            break;

        case SIGTSTP:

            ship_print();
            break;

        case SIGINT:
        
            fprintf(stderr, "\n" ROJO "Terminando todos los procesos..." RESET "\n");
            map_remove_ship(ship.mapa, ship.x, ship.y); // Borramos el barco del mapa
            map_destroy(ship.mapa);                     // Liberamos la memoria
            exit(ship.gold);                            // Salimos del programa
            break;    
            
        case SIGQUIT:

            fprintf(stderr, "Barco %d (SIGQUIT): Terminando con oro %d.\n", ship.pid, ship.gold);
            map_remove_ship(ship.mapa, ship.x, ship.y);
            map_destroy(ship.mapa);
            exit(ship.gold);
            break;
    }
}

// Función genérica para intentar moverse
void try_move(int dx, int dy) {

   // 1. Verificar Comida
    if (ship.food < 5) {

            printf(ROJO "NOK" RESET "\n"); 
            ship_print(); // AÑADIDO: Para imprimir estado al fallar
            fflush(stdout); 
            return;
    }

    int new_x = ship.x + dx;
    int new_y = ship.y + dy;

    // 2. Verificar Mapa (Rocas o límites)
    if (!map_can_sail(ship.mapa, new_x, new_y)) {

            printf(ROJO "NOK" RESET "\n"); 
            ship_print(); // Imprimes mapa al chocar igualmente, para mostrar la posición del barco.
            fflush(stdout); 
            return; 
    }

    // 3. Leer qué hay en el destino ANTES de movernos
    char cell_type = map_get_cell_type(ship.mapa, new_x, new_y);

    // 4. Realizar Movimiento
    map_remove_ship(ship.mapa, ship.x, ship.y); // Borrar la posición anterior

    ship.x = new_x;
    ship.y = new_y;

    map_set_ship(ship.mapa, ship.x, ship.y);    // Colocar en la nueva posición
    ship.food -= 5;


    // 5. Lógica de premios
    if (cell_type == 'I') 

        ship.gold += 10;

    else if (cell_type == 'P') 

        ship.food += 20;


    // 6. IMPRIMIR (ÉXITO)
        //map_print(ship.mapa); // Mapa primero

        // Mensajes de eventos
        if (cell_type == 'I') 

            fprintf(stderr, "Barco %d alcanzó una " VERDE "ISLA " RESET "(%d, %d)," AMARILLO" oro incrementado a %d.\n"RESET, ship.pid, ship.x, ship.y, ship.gold);

        else if (cell_type == 'P') 

            fprintf(stderr, "Barco %d alcanzó un " MAGENTA "PUERTO " RESET "(%d, %d),"AZUL" comida incrementada a %d.\n"RESET, ship.pid, ship.x, ship.y, ship.food);

        printf(VERDE "OK" RESET "\n");
        fflush(stdout); 
        ship_print(); // Aquí se imprime el estado en caso de éxito
    
}

// Función auxiliar para modo random
void move_randomly_step() {

    int dir_idx = rand() % 4;
    try_move(directions[dir_idx][0], directions[dir_idx][1]);
    //ship_print(); 
}

// Función para parsear argumentos 
static void parse_args(int argc, char *argv[], char **map_file, int *pos_x, int *pos_y, int *food, int *random_steps, int *random_speed, int *captain_mode) {

    int errores = 0;

    for (int i = 1; i < argc; i++) {

        // strtol(cadena, puntero_final, base_decimal)
        if (strcmp(argv[i], "--map") == 0) {

            if (i + 1 >= argc || strncmp(argv[i+1], "--", 2) == 0) {

                fprintf(stderr, ROJO "Error, '--map' requiere un nombre de archivo." RESET "\n");
                //print_help(argv[0]);
                //exit(1);
                errores = 1;

            } else {

                *map_file = argv[++i];
            }
        }    

        // --- POS ---
        else if (strcmp(argv[i], "--pos") == 0) { 

            int faltan_coords = 0;

            // 1. Intentamos leer la PRIMERA coordenada (x)
            if (i + 1 < argc && strncmp(argv[i+1], "--", 2) != 0) {

                *pos_x = (int)strtol(argv[++i], NULL, 10); 
                
                // Comprobamos si la 'x' tiene un valor inválido (ej. negativo)
                if (*pos_x < 0) {

                    fprintf(stderr, ROJO "Error: La coordenada 'x' de '--pos' no puede ser negativa." RESET "\n");
                    errores = 1;
                }
                
                // 2. Intentamos leer la SEGUNDA coordenada (y)
                if (i + 1 < argc && strncmp(argv[i+1], "--", 2) != 0) {

                    *pos_y = (int)strtol(argv[++i], NULL, 10); 
                    
                    // Comprobamos si la 'y' tiene un valor inválido
                    if (*pos_y < 0) {

                        fprintf(stderr, ROJO "Error: La coordenada 'y' de '--pos' no puede ser negativa." RESET "\n");
                        errores = 1;
                    }
                } else {
                    // Teníamos la X, pero nos falta la Y
                    faltan_coords = 1;
                }

            } else {
                // Faltan las dos directamente
                faltan_coords = 1;
            }

            // 3. Si en algún momento vimos que faltaba algo, soltamos el error general
            if (faltan_coords == 1) {

                fprintf(stderr, ROJO "Error: '--pos' requiere de dos coordenadas, 'x' e 'y'." RESET "\n");
                errores = 1;
            }
        }

        // --- FOOD ---
        else if (strcmp(argv[i], "--food") == 0) {

            if (i + 1 >= argc || strncmp(argv[i+1], "--", 2) == 0) {

                fprintf(stderr, ROJO "Error: '--food' requiere una cantidad inicial." RESET "\n");
                //print_help(argv[0]);
                //exit(1);
                errores = 1;

            } else {

                *food = (int)strtol(argv[++i], NULL, 10);
            }
        } 
        
        else if (strcmp(argv[i], "--random") == 0) { 

            int faltan_parametros = 0;

            // 1. Intentamos leer el PRIMER parámetro (los pasos)
            if (i + 1 < argc && strncmp(argv[i+1], "--", 2) != 0) {

                *random_steps = (int)strtol(argv[++i], NULL, 10); 
                
                // Comprobamos su valor inmediatamente
                if (*random_steps <= 0) {

                    fprintf(stderr, ROJO "Error: El número de pasos en '--random' debe ser mayor que 0." RESET "\n");
                    errores = 1;
                }
                
                // 2. Intentamos leer el SEGUNDO parámetro (la velocidad)
                if (i + 1 < argc && strncmp(argv[i+1], "--", 2) != 0) {

                    *random_speed = (int)strtol(argv[++i], NULL, 10); 
                    
                    // Comprobamos su valor
                    if (*random_speed <= 0) {

                        fprintf(stderr, ROJO "Error: La velocidad en '--random' debe ser mayor que 0." RESET "\n");
                        errores = 1;

                    }
                } else {

                    // Teníamos el primero, pero nos falta el segundo
                    faltan_parametros = 1;
                }
            } else {

                // Faltan los dos directamente
                faltan_parametros = 1;
            }

            // 3. Si en algún momento vimos que faltaba algo, soltamos el error general
            if (faltan_parametros == 1) {

                fprintf(stderr, ROJO "Error: '--random' requiere de dos parámetros, 'pasos' y 'velocidad'." RESET "\n");
                errores = 1;
            }
        }
        
        else if (strcmp(argv[i], "--captain") == 0) {

            *captain_mode = 1;
        } 

        else {

            fprintf(stderr, ROJO "Error, argumento '%s' no reconocido."RESET "\n", argv[i]);
            //print_help(argv[0]);
            //exit(1);
            errores = 1;
        }
    }
    if (errores == 1) {

        fprintf(stderr, AMARILLO "\nCorrige los errores anteriores para ejecutar el programa." RESET "\n");
        exit(1);
    }
}

int main(int argc, char *argv[]) {

    char *map_file = "map.txt";
    int pos_x = 1, pos_y = 1; 
    int food = 100;
    int random_steps = -1, random_speed = 1;
    int captain_mode = 0;

    parse_args(argc, argv, &map_file, &pos_x, &pos_y, &food, &random_steps, &random_speed, &captain_mode);

    // Ajuste de lógica de modos para la Parte 3
    if (captain_mode && random_steps != -1) {

        fprintf(stderr, "Error: --captain y --random no pueden usarse juntos.\n");
        return 1;
    }

    // Si no es capitán y no hay pasos random (o son 0), por defecto es MANUAL (capitán)
    if (!captain_mode && random_steps == -1 ) {

        fprintf(stderr, ROJO "Error: Debes especificar un modo de juego: " RESET "'captain' o 'random'." "\n");
        //fprintf(stderr, AMARILLO "Usa '--captain' para modo manual o '--random' para automático." RESET "\n");
        exit(1); //Cerramos programa para introducir comando de nuevo
    }

    if (!captain_mode && random_steps <= 0) {

        fprintf(stderr, ROJO "Error: El número de pasos en modo random debe ser mayor que 0." RESET "\n");
        exit(1);
    }

    fprintf(stderr, "\nMapa: %s.\nPosición: (%d, %d).\nComida: %d\n\n", map_file, pos_x, pos_y, food);

    // Carga del mapa 
    Map *mapa_ptr = map_load(map_file);
    if (!mapa_ptr) {

        fprintf(stderr, "Error cargando el mapa: %s\n", map_file);
        return 1;
    }

    if (map_can_sail(mapa_ptr, pos_x, pos_y)) {

        ship_init(mapa_ptr, pos_x, pos_y, food);

    } else {

        fprintf(stderr, "Posición inicial inválida (%d,%d).\n", pos_x, pos_y);
        map_destroy(mapa_ptr);
        exit(1);
    }
    
    fprintf(stderr, "PID del barco: %d.\nModo: %s\n\n", ship.pid, captain_mode ? "CAPITAN" : "RANDOM");

    signal(SIGALRM, handle_signal);
    signal(SIGUSR1, handle_signal);
    signal(SIGUSR2, handle_signal);
    signal(SIGQUIT, handle_signal);
    signal(SIGTSTP, handle_signal);
    signal(SIGINT, handle_signal);

    srand(time(NULL) ^ getpid());

    // --- LÓGICA PRINCIPAL ---
    if (captain_mode) {

        // MODO MANUAL: Sustitución de SCANF por READ (letra a letra)
        char buffer[100]; // Buffer para guardar la palabra
        char c;           // Variable auxiliar para leer 1 byte
        int i = 0;        // Contador del buffer

        fprintf(stderr, "Esperando comandos: {up, down, left, right, exit}\n");

        // Leemos 1 byte del descriptor 0 (Entrada Estándar)
        while (read(STDIN_FILENO, &c, 1) > 0) {
            
            if (c == '\n') {

                // Si encontramos un ENTER, procesamos el comando acumulado
                buffer[i] = '\0'; // Cerramos el string
                i = 0;            // Reiniciamos el contador para la próxima

                if (strcasecmp(buffer, "up") == 0)         try_move(0, -1);
                else if (strcasecmp(buffer, "down") == 0)  try_move(0, 1);
                else if (strcasecmp(buffer, "left") == 0)  try_move(-1, 0);
                else if (strcasecmp(buffer, "right") == 0) try_move(1, 0);
                else if (strcasecmp(buffer, "exit") == 0) {

                    fprintf(stderr, "Barco %d saliendo con "AMARILLO"%d de oro.\n"RESET, ship.pid, ship.gold);
                    break;

                } else {

                    fprintf(stderr, ROJO "ERROR, comando '%s' inválido.\n" RESET "Por favor, introduzca un comando válido: {up, down, left, right, exit}\n", buffer);
                    continue;
                }

                if (ship.food < 5 ) {

                    fprintf(stderr, "Barco %d: "ROJO "Sin comida. "RESET "Fin de la partida.\n", ship.pid);
                    break;
                }

            } else {

                // Si es una letra normal, la guardamos
                if (i < 99) 
                    buffer[i++] = c;
            }
        }

    } else {

        // MODO RANDOM (Argumentos --random N S)
        fprintf(stderr, "Barco %d iniciando %d pasos.\n", ship.pid, random_steps);
        int sin_comida = 0;

        for (int i = 0; i < random_steps; i++) {

            alarm(random_speed);
            pause(); 
            move_randomly_step();

            if (ship.food < 5) {

                fprintf(stderr, "Barco %d " ROJO "Sin comida."RESET " Fin.\n", ship.pid);
                sin_comida = 1;
                break;
            }
        }
        // Parte 3: Avisar al terminar los pasos
        if (sin_comida == 0)
            fprintf(stderr, "Barco %d Límite de pasos alcanzado. Terminando ejecución.\n", ship.pid);
    }
    
    map_remove_ship(ship.mapa, ship.x, ship.y);
    map_destroy(ship.mapa);
    return ship.gold;
}
