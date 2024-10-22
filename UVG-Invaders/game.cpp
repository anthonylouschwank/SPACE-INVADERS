#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <time.h>
#include <ncurses.h>
#include <stdbool.h>

//Constantes del juego
#define MAX_X 80
#define MIN_X 0
#define MAX_Y 24
#define ALIEN_ROWS 5
#define ALIEN_COLS 11
#define DELAY 30000
#define ALIEN_DELAY 300000
#define BULLET_DELAY 50000
#define INVADER_SHOOT_DELAY 500000
#define MAX_PLAYER_BULLETS 3
#define MAX_ALIEN_BULLETS 5

//Informacion del jugador y las balas
int player_x = MAX_X / 2;
int player_y = MAX_Y - 2;
int score = 0;
int lives = 3;

//Poscionamiento de arays
int invader_speed = ALIEN_DELAY;
char aliens[ALIEN_ROWS][ALIEN_COLS];
char bunkers[3][5] = {{'-', '-', '-', '-', '-'},
                      {'-', '-', '-', '-', '-'},
                      {'-', '-', '-', '-', '-'}};
bool game_over = false;


//Informacion de movimiento y posicion alienigenas
int alien_start_col = 0;
int alien_direction = 1;  
int alien_row_offset = 0;

//Variables de pausa y menu
bool in_menu = true;
bool paused = false;
int menu_selection = 0;

//Estructura de bala
typedef struct {
    int x, y;
    bool active;
} Bullet;

Bullet player_bullets[MAX_PLAYER_BULLETS];
Bullet alien_bullets[MAX_ALIEN_BULLETS];

//Declaracion de semaforos y mutex
pthread_mutex_t lock;
sem_t sem_bullets;
pthread_mutex_t lock_player, lock_bullet;

//Inicializa los aliens
void init_aliens() {
    for (int i = 0; i < ALIEN_ROWS; ++i) {
        for (int j = 0; j < ALIEN_COLS; ++j) {
            aliens[i][j] = (i == 0) ? '@' : (i <= 2) ? '#' : '%';
        }
    }
}

//Inicializar balas
void init_bullets() {
    for (int i = 0; i < MAX_PLAYER_BULLETS; i++) {
        player_bullets[i].active = false;
    }
    for (int i = 0; i < MAX_ALIEN_BULLETS; i++) {
        alien_bullets[i].active = false;
    }
}

//Dibuja pantalla del juego
void draw_game() {
    clear();
    mvprintw(player_y, player_x, "^");  //Se dibuja la nave del jugador
    for (int i = 0; i < ALIEN_ROWS; ++i) {  // Se dibuja las naves de los alienigenas
        for (int j = 0; j < ALIEN_COLS; ++j) {
            if (aliens[i][j] != ' ') {
                int screen_col = alien_start_col + j * 2;
                int screen_row = i + 2 + alien_row_offset;
                mvprintw(screen_row, screen_col, "%c", aliens[i][j]);
            }
        }
    }
    //Dibujo de los bunkeres
    for (int i = 0; i < 3; ++i) {
        mvprintw(MAX_Y - 5, i * 25 + 10, "%s", bunkers[i]);
    }

    //Dibuja las balas del jugador una vez activadas
    for (int i = 0; i < MAX_PLAYER_BULLETS; i++) {
        if (player_bullets[i].active) {
            mvaddch(player_bullets[i].y, player_bullets[i].x, '|');
        }
    }
    //Sibuja las balas de los alienigenas una vez activadas
    for (int i = 0; i < MAX_ALIEN_BULLETS; i++) {
        if (alien_bullets[i].active) {
            mvaddch(alien_bullets[i].y, alien_bullets[i].x, '*');
        }
    }
    mvprintw(0, 0, "Score: %d", score);  // Puntaje
    mvprintw(0, 20, "Lives: %d", lives); // Vidas
    refresh();
}

//Movimiento del jugador definido por ncurses
void *move_player(void *arg) {
    int ch;
    while (!game_over) {
        ch = getch();
        pthread_mutex_lock(&lock_player);
        if (ch == KEY_LEFT && player_x > MIN_X) {
            player_x--;
        } else if (ch == KEY_RIGHT && player_x < MAX_X - 1) {
            player_x++;
        } else if (ch == ' ') {
            sem_post(&sem_bullets);
        }
        pthread_mutex_unlock(&lock_player);
        usleep(DELAY);
    }
    return NULL;
}

//Movimiento de los alienigenas
void *move_aliens(void *arg) {
    while (!game_over) {
        pthread_mutex_lock(&lock);
        
        alien_start_col += alien_direction;
        
        if (alien_start_col <= 0 || alien_start_col + ALIEN_COLS * 2 >= MAX_X) { //Revisa cada sierto tiempo en que lugar estan para verificar direccion
            alien_direction *= -1;
            alien_row_offset++; 
            alien_start_col += alien_direction;
        }
        
        pthread_mutex_unlock(&lock);
        usleep(invader_speed);

        //Aumento de la velocidad segun el tiempo de juego
        if (invader_speed > ALIEN_DELAY / 2) {
            invader_speed -= 1000;
        }

        //Probabilidades de disparo del alienigena
        if (rand() % 100 < 10) {  //10% de probabilidad
            sem_post(&sem_bullets);
        }
    }
    return NULL;
}

//Disparo del jugador
void *player_shoot(void *arg) {
    int last_shot_time = 0;
    const int shot_cooldown = 500000; //Tiempo de espera entre cada disparo

    while (!game_over) {
        sem_wait(&sem_bullets);
        
        int current_time = clock() * (1000000 / CLOCKS_PER_SEC);
        
        //Se revisa cuanto tiempo a pasado desde la ultima vez que se disparo
        if (current_time - last_shot_time >= shot_cooldown) {
            //Una vez sea cierto, se encierra la zona critica
            pthread_mutex_lock(&lock_bullet);
            for (int i = 0; i < MAX_PLAYER_BULLETS; i++) {
                if (!player_bullets[i].active) {
                    player_bullets[i].x = player_x;
                    player_bullets[i].y = player_y - 1;
                    player_bullets[i].active = true;
                    last_shot_time = current_time;
                    break;
                }
            }
            pthread_mutex_unlock(&lock_bullet);
        }
    }
    return NULL;
}

//Disparos de los aliens
void *alien_shoot(void *arg) {
    while (!game_over) {
        usleep(INVADER_SHOOT_DELAY);
        if (rand() % 100 < 10) {  //10% de probabilidad de disparo
            pthread_mutex_lock(&lock);
            for (int j = 0; j < ALIEN_COLS; j++) {
                for (int i = ALIEN_ROWS - 1; i >= 0; i--) {
                    if (aliens[i][j] != ' ') {
                        for (int k = 0; k < MAX_ALIEN_BULLETS; k++) {
                            if (!alien_bullets[k].active) {
                                alien_bullets[k].x = alien_start_col + j * 2;
                                alien_bullets[k].y = i + 2 + alien_row_offset + 1;
                                alien_bullets[k].active = true;
                                pthread_mutex_unlock(&lock);
                                goto bullet_fired;
                            }
                        }
                        break;
                    }
                }
            }
            pthread_mutex_unlock(&lock);
        }
        bullet_fired:
        continue;
    }
    return NULL;
}

//Actualiza las posiciones de las balas y su interaccion con el medio
void update_bullets() {
    pthread_mutex_lock(&lock_bullet);

    //Recorremos todas las balas del jugador
    for (int i = 0; i < MAX_PLAYER_BULLETS; i++) {
        if (player_bullets[i].active) {
            player_bullets[i].y--;
            if (player_bullets[i].y <= 0) {
                player_bullets[i].active = false;
                continue;
            }

            //Colision de la bala del jugador con un alienigena
            for (int row = 0; row < ALIEN_ROWS; row++) {
                for (int col = 0; col < ALIEN_COLS; col++) {
                    if (aliens[row][col] != ' ' &&
                        player_bullets[i].y == row + 2 + alien_row_offset &&
                        player_bullets[i].x >= alien_start_col + col * 2 &&
                        player_bullets[i].x < alien_start_col + col * 2 + 2) {
                        aliens[row][col] = ' ';
                        player_bullets[i].active = false;
                        score += (row == 0) ? 30 : (row <= 2) ? 20 : 10;
                        goto next_player_bullet;
                    }
                }
            }

            //Colision de la bala del jugador con un bunker
            for (int b = 0; b < 3; b++) {
                if (player_bullets[i].y == MAX_Y - 5 &&
                    player_bullets[i].x >= b * 25 + 10 && 
                    player_bullets[i].x < b * 25 + 15) {  
                    int bunker_pos = player_bullets[i].x - (b * 25 + 10);
                    if (bunkers[b][bunker_pos] != ' ') {
                        bunkers[b][bunker_pos] = ' '; 
                        player_bullets[i].active = false;
                        break;
                    }
                }
            }
        }
    next_player_bullet: 
        continue;
    }

    //Recorremos todas las balas alienigenas
    for (int i = 0; i < MAX_ALIEN_BULLETS; i++) {
        if (alien_bullets[i].active) {
            alien_bullets[i].y++;
            if (alien_bullets[i].y >= MAX_Y) {
                alien_bullets[i].active = false;
                continue;
            }

            //Colision de una bala alienigena con un jugador
            if (alien_bullets[i].y == player_y && alien_bullets[i].x == player_x) {
                lives--;
                alien_bullets[i].active = false;
                if (lives <= 0) {
                    game_over = true;
                }
                continue;
            }

            //Colision de una bala alienigena con un bunker
            for (int b = 0; b < 3; b++) {
                if (alien_bullets[i].y == MAX_Y - 5 &&
                    alien_bullets[i].x >= b * 20 + 10 &&
                    alien_bullets[i].x < b * 20 + 15) {
                    int bunker_pos = alien_bullets[i].x - (b * 20 + 10);
                    if (bunkers[b][bunker_pos] != ' ') {
                        bunkers[b][bunker_pos] = ' ';
                        alien_bullets[i].active = false;
                        break;
                    }
                }
            }
        }
    }

    pthread_mutex_unlock(&lock_bullet);
}

//Se revisa si el juego cumple requisitos para acabar
bool check_game_over() {
    pthread_mutex_lock(&lock);
    bool aliens_alive = false;
    //Se revisa si hay caracteres distintos a vacio dentro del array
    for (int i = 0; i < ALIEN_ROWS; ++i) {
        for (int j = 0; j < ALIEN_COLS; ++j) {
            if (aliens[i][j] != ' ') {
                aliens_alive = true;
                //Se revisa que no hayan llegado a la posicion justo arriba del jugador
                if (i + alien_row_offset >= MAX_Y - 6) {
                    game_over = true;
                    pthread_mutex_unlock(&lock);
                    return true;
                }
            }
        }
    }

    //Se finaliza el juego si la conficion se cumple
    if (!aliens_alive) {
        game_over = true;
        pthread_mutex_unlock(&lock);
        return true;
    }
    pthread_mutex_unlock(&lock);
    return game_over;
}

//Se dibuja el menu con su sistema de interaccion
void draw_menu() {
    clear();
    mvprintw(MAX_Y/2 - 2, MAX_X/2 - 10, "SPACE INVADERS");
    mvprintw(MAX_Y/2, MAX_X/2 - 5, menu_selection == 0 ? "> Start" : "  Start");
    mvprintw(MAX_Y/2 + 1, MAX_X/2 - 5, menu_selection == 1 ? "> Exit" : "  Exit");
    refresh();
}

//Sistema de interaccion del menu
void handle_menu() {
    int ch;
    while (in_menu) {
        draw_menu();
        ch = getch();
        switch (ch) {
            case KEY_UP:
                menu_selection = (menu_selection - 1 + 2) % 2;
                break;
            case KEY_DOWN:
                menu_selection = (menu_selection + 1) % 2;
                break;
            case 10:  //Llegan entradas sobre la seleccion del menu
                if (menu_selection == 0) {
                    in_menu = false;  //Comienzan el juego
                } else {
                    endwin();
                    exit(0);  //Se sale de la aplicacion
                }
                break;
        }
    }
}

// Main del juego
int main() {
    //Se inicializan librerias y sistemas de interaccion con el usuario
    srand(time(NULL));
    initscr();
    noecho();
    curs_set(FALSE);
    keypad(stdscr, TRUE);
    nodelay(stdscr, FALSE); 

    while (1) {
        handle_menu();  // Mostrar menú

        // Inicializar el juego
        nodelay(stdscr, TRUE);
        init_aliens();
        init_bullets();
        pthread_mutex_init(&lock, NULL);
        pthread_mutex_init(&lock_player, NULL);
        pthread_mutex_init(&lock_bullet, NULL);
        sem_init(&sem_bullets, 0, 0);

        //Se declaran las variables con numeros concretos
        player_x = MAX_X / 2;
        player_y = MAX_Y - 2;
        score = 0;
        lives = 3;
        invader_speed = ALIEN_DELAY;
        alien_start_col = 0;
        alien_direction = 1;
        alien_row_offset = 0;
        game_over = false;
        paused = false;

        //Se inicializan las threads de los alienigenas, disparos de ambos, y el jugador
        pthread_t player_thread, aliens_thread, shoot_thread, alien_shoot_thread;
        pthread_create(&player_thread, NULL, move_player, NULL);
        pthread_create(&aliens_thread, NULL, move_aliens, NULL);
        pthread_create(&shoot_thread, NULL, player_shoot, NULL);
        pthread_create(&alien_shoot_thread, NULL, alien_shoot, NULL);

        //Se chequea durante el juego que las conficiones de ganar no hayn sucedido
        while (!game_over) {
            int ch = getch();
            if (ch == 'p' || ch == 'P') {
                paused = !paused;
                if (paused) {
                    mvprintw(MAX_Y/2, MAX_X/2 - 5, "PAUSED");
                    refresh();
                }
            }
            if (!paused) {
                update_bullets();
                draw_game();
                if (check_game_over()) break;
            }
            usleep(DELAY);
        }

        // Muestra mensaje de fin del juego
        clear();
        mvprintw(MAX_Y/2, MAX_X/2 - 5, "GAME OVER");
        mvprintw(MAX_Y/2 + 1, MAX_X/2 - 7, "Final Score: %d", score);
        mvprintw(MAX_Y/2 + 2, MAX_X/2 - 12, "Press any key to continue");
        refresh();
        nodelay(stdscr, FALSE);
        getch();

        // Limpiar recursos
        pthread_mutex_destroy(&lock);
        pthread_mutex_destroy(&lock_player);
        pthread_mutex_destroy(&lock_bullet);
        sem_destroy(&sem_bullets);

        // Volver al menú
        in_menu = true;
    }

    endwin();
    return 0;
}