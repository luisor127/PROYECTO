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
            int s_id = -1;
            for(int k=0; k<ship_count; k++) {
                if(fleet[k].pid == pid) s_id = fleet[k].id;
            }
            fprintf(stderr, "\nCapitana: Barco %d (PID %d) terminó con estado %d. Quedan %d vivos.\n", 
                    s_id, pid, WEXITSTATUS(status), ships_alive);
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
    for (int i = 0; i < ship_count; i++) {
        int pasos = random_mode ? (rand() % 11) + 10 : 0;
        pid_t pid = fork();

        if (pid == 0) {
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
            fleet[i].pid = pid;
            ships_alive++;
        }
    }

    // [CAMBIO 5] Bucle de espera no bloqueante
    fprintf(stderr, "Capitana: Flota lanzada. Esperando eventos...\n");
    while (ships_alive > 0) {
        pause(); // Se duerme hasta recibir CUALQUIER señal 
    }

    fprintf(stderr, "Capitana: Todos los barcos han terminado. Saliendo.\n");
    free(fleet);
    return 0;
}
