#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <time.h>
#include <ncurses.h>

// Constantes del juego
#define MAX_X 80
#define MIN_X 0
#define MAX_Y 24
#define ALIEN_ROWS 5
#define ALIEN_COLS 5
#define DELAY 30000
#define ALIEN_DELAY 300000
#define BULLET_DELAY 80000
#define INVADER_SHOOT_DELAY 500000

// Variables del juego
int player_x = MAX_X / 2;
int player_y = MAX_Y - 2;
int score = 0;
int lives = 3;
int invader_speed = ALIEN_DELAY;
char aliens[ALIEN_ROWS][ALIEN_COLS];
char bunkers[3][5] = {{'-', '-', '-', '-', '-'},
                      {'-', '-', '-', '-', '-'},
                      {'-', '-', '-', '-', '-'}};
bool game_over = false;

// Mutex y semáforos
pthread_mutex_t lock;
sem_t sem_bullets;
pthread_mutex_t lock_player, lock_bullet;

// Inicializa los alienígenas
void init_aliens() {
    for (int i = 0; i < ALIEN_ROWS; ++i) {
        for (int j = 0; j < ALIEN_COLS; ++j) {
            aliens[i][j] = 'x';
        }
    }
}

// Dibuja la pantalla del juego
void draw_game() {
    clear();
    mvprintw(player_y, player_x, "^");  // Dibuja la nave
    for (int i = 0; i < ALIEN_ROWS; ++i) {  // Dibuja los alienígenas
        for (int j = 0; j < ALIEN_COLS; ++j) {
            if (aliens[i][j] != ' ') {
                mvprintw(i + 2, j * 2 + 2, "%c", aliens[i][j]);
            }
        }
    }
    for (int i = 0; i < 3; ++i) {  // Dibuja los búnkeres
        mvprintw(MAX_Y - 5, i * 20 + 10, "%s", bunkers[i]);
    }
    mvprintw(0, 0, "Score: %d", score);  // Puntaje
    mvprintw(0, 20, "Lives: %d", lives); // Vidas
    refresh();
}

void *move_player(void *arg) {
    int ch;
    while (!game_over) {
        ch = getch();
        pthread_mutex_lock(&lock_player);  // Usa el mutex solo para el jugador
        if (ch == KEY_LEFT && player_x > MIN_X) {
            player_x--;
        } else if (ch == KEY_RIGHT && player_x < MAX_X) {
            player_x++;
        } else if (ch == ' ') {
            sem_post(&sem_bullets);  // Dispara al presionar espacio
        }
        pthread_mutex_unlock(&lock_player);
        usleep(DELAY);
    }
    return NULL;
}

// Movimiento de los alienígenas
void *move_aliens(void *arg) {
    bool direction_right = true;
    while (!game_over) {
        pthread_mutex_lock(&lock);
        for (int i = 0; i < ALIEN_ROWS; ++i) {
            for (int j = 0; j < ALIEN_COLS; ++j) {
                aliens[i][j] = ' ';
            }
        }
        if (direction_right) {
            for (int i = 0; i < ALIEN_ROWS; ++i) {
                for (int j = 0; j < ALIEN_COLS; ++j) {
                    if (j + 1 < ALIEN_COLS) {
                        aliens[i][j + 1] = 'x';
                    }
                }
            }
            direction_right = false;
        } else {
            for (int i = 0; i < ALIEN_ROWS; ++i) {
                for (int j = 0; j < ALIEN_COLS; ++j) {
                    if (j - 1 >= 0) {
                        aliens[i][j - 1] = 'x';
                    }
                }
            }
            direction_right = true;
        }
        pthread_mutex_unlock(&lock);
        usleep(invader_speed);
    }
    return NULL;
}

// Movimiento del disparo del jugador
void *player_shoot(void *arg) {
    while (!game_over) {
        sem_wait(&sem_bullets);
        int bullet_x = player_x;
        int bullet_y = player_y - 1;

        while (bullet_y > 0 && !game_over) {
            pthread_mutex_lock(&lock_bullet);  // Protege la posición del disparo
            mvprintw(bullet_y, bullet_x, "*");
            refresh();
            pthread_mutex_unlock(&lock_bullet);

            usleep(BULLET_DELAY);

            pthread_mutex_lock(&lock_bullet);  // Borra el disparo
            mvprintw(bullet_y, bullet_x, " ");
            bullet_y--;
            pthread_mutex_unlock(&lock_bullet);
        }
    }
    return NULL;
}


// Verifica colisiones
void check_collisions() {
    // Por implementar: detección de colisiones entre disparos e invasores.
}

// Main del juego
int main() {
    srand(time(NULL));
    initscr();             // Inicializa ncurses
    noecho();              // No muestra las teclas presionadas
    curs_set(FALSE);       // Oculta el cursor
    keypad(stdscr, TRUE);  // Habilita teclas especiales
    nodelay(stdscr, TRUE); // No bloquea el getch
    init_aliens();         // Inicializa los alienígenas
    pthread_mutex_init(&lock, NULL);
    pthread_mutex_init(&lock_player, NULL);
    pthread_mutex_init(&lock_bullet, NULL);
    sem_init(&sem_bullets, 0, 0);

    // Hilos
    pthread_t player_thread, aliens_thread, shoot_thread;
    pthread_create(&player_thread, NULL, move_player, NULL);
    pthread_create(&aliens_thread, NULL, move_aliens, NULL);
    pthread_create(&shoot_thread, NULL, player_shoot, NULL);

    while (!game_over) {
        draw_game();
        check_collisions();
        usleep(DELAY);
    }

    // Finaliza el juego
    endwin();
    pthread_mutex_destroy(&lock);
    pthread_mutex_destroy(&lock_player);
    pthread_mutex_destroy(&lock_bullet);
    sem_destroy(&sem_bullets);
    return 0;
}



