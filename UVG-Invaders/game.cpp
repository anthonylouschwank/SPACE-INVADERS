#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <time.h>
#include <ncurses.h>
#include <stdbool.h>

//cons del juego
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

//vars del juego
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
int alien_start_col = 0;
int alien_direction = 1;  //1=derecha, -1=izquierda
int alien_row_offset = 0;

//Estructura bala
typedef struct {
    int x, y;
    bool active;
} Bullet;

Bullet player_bullets[MAX_PLAYER_BULLETS];
Bullet alien_bullets[MAX_ALIEN_BULLETS];

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
    mvprintw(player_y, player_x, "^");  // nave
    for (int i = 0; i < ALIEN_ROWS; ++i) {  // aliens
        for (int j = 0; j < ALIEN_COLS; ++j) {
            if (aliens[i][j] != ' ') {
                int screen_col = alien_start_col + j * 2;
                int screen_row = i + 2 + alien_row_offset;
                mvprintw(screen_row, screen_col, "%c", aliens[i][j]);
            }
        }
    }
    //bunkeres
    for (int i = 0; i < 3; ++i) {
        mvprintw(MAX_Y - 5, i * 25 + 10, "%s", bunkers[i]);
    }

    //balas jugador
    for (int i = 0; i < MAX_PLAYER_BULLETS; i++) {
        if (player_bullets[i].active) {
            mvaddch(player_bullets[i].y, player_bullets[i].x, '|');
        }
    }
    //balas aliens
    for (int i = 0; i < MAX_ALIEN_BULLETS; i++) {
        if (alien_bullets[i].active) {
            mvaddch(alien_bullets[i].y, alien_bullets[i].x, '*');
        }
    }
    mvprintw(0, 0, "Score: %d", score);  // Puntaje
    mvprintw(0, 20, "Lives: %d", lives); // Vidas
    refresh();
}

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

//movimiento de los aliens
void *move_aliens(void *arg) {
    while (!game_over) {
        pthread_mutex_lock(&lock);
        
        alien_start_col += alien_direction;
        
        if (alien_start_col <= 0 || alien_start_col + ALIEN_COLS * 2 >= MAX_X) {
            alien_direction *= -1;
            alien_row_offset++; 
            alien_start_col += alien_direction;
        }
        
        pthread_mutex_unlock(&lock);
        usleep(invader_speed);

        //aumento de velocidad
        if (invader_speed > ALIEN_DELAY / 2) {
            invader_speed -= 1000;
        }

        //chance de disparo
        if (rand() % 100 < 10) {  //10%
            sem_post(&sem_bullets);
        }
    }
    return NULL;
}

//disparo del jugador
void *player_shoot(void *arg) {
    int last_shot_time = 0;
    const int shot_cooldown = 500000; //cooldown

    while (!game_over) {
        sem_wait(&sem_bullets);
        
        int current_time = clock() * (1000000 / CLOCKS_PER_SEC);
        
        //checkeo del tiempo
        if (current_time - last_shot_time >= shot_cooldown) {
            // Sección crítica: manejo de balas
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
        if (rand() % 100 < 10) {  //10%
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

//actualizar posiciones de balas y detectar colisiones
void update_bullets() {

    pthread_mutex_lock(&lock_bullet);

    for (int i = 0; i < MAX_PLAYER_BULLETS; i++) {
        if (player_bullets[i].active) {
            player_bullets[i].y--;
            if (player_bullets[i].y <= 0) {
                player_bullets[i].active = false;
                continue;
            }

            //colision alien
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

            //colision bunkeres
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

    //balas de los aliens
    for (int i = 0; i < MAX_ALIEN_BULLETS; i++) {
        if (alien_bullets[i].active) {
            alien_bullets[i].y++;
            if (alien_bullets[i].y >= MAX_Y) {
                alien_bullets[i].active = false;
                continue;
            }

            //colision jugador
            if (alien_bullets[i].y == player_y && alien_bullets[i].x == player_x) {
                lives--;
                alien_bullets[i].active = false;
                if (lives <= 0) {
                    game_over = true;
                }
                continue;
            }

            //colision búnkeres
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


//game over
bool check_game_over() {
    pthread_mutex_lock(&lock);
    bool aliens_alive = false;
    for (int i = 0; i < ALIEN_ROWS; ++i) {
        for (int j = 0; j < ALIEN_COLS; ++j) {
            if (aliens[i][j] != ' ') {
                aliens_alive = true;
                if (i + alien_row_offset >= MAX_Y - 6) {
                    game_over = true;
                    pthread_mutex_unlock(&lock);
                    return true;
                }
            }
        }
    }
    if (!aliens_alive) {
        game_over = true;
        pthread_mutex_unlock(&lock);
        return true;
    }
    pthread_mutex_unlock(&lock);
    return game_over;
}

// Main del juego
int main() {
    srand(time(NULL));
    initscr();
    noecho();
    curs_set(FALSE);
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    init_aliens();
    init_bullets();
    pthread_mutex_init(&lock, NULL);
    pthread_mutex_init(&lock_player, NULL);
    pthread_mutex_init(&lock_bullet, NULL);
    sem_init(&sem_bullets, 0, 0);

    pthread_t player_thread, aliens_thread, shoot_thread, alien_shoot_thread;
    pthread_create(&player_thread, NULL, move_player, NULL);
    pthread_create(&aliens_thread, NULL, move_aliens, NULL);
    pthread_create(&shoot_thread, NULL, player_shoot, NULL);
    pthread_create(&alien_shoot_thread, NULL, alien_shoot, NULL);

    while (!game_over) {
        update_bullets();
        draw_game();
        if (check_game_over()) break;
        usleep(DELAY);
    }

    // Muestra mensaje de fin del juego
    clear();
    mvprintw(MAX_Y/2, MAX_X/2 - 5, "GAME OVER");
    mvprintw(MAX_Y/2 + 1, MAX_X/2 - 7, "Final Score: %d", score);
    refresh();
    sleep(3);

    // Finaliza el juego
    endwin();
    pthread_mutex_destroy(&lock);
    pthread_mutex_destroy(&lock_player);
    pthread_mutex_destroy(&lock_bullet);
    sem_destroy(&sem_bullets);
    return 0;
}



