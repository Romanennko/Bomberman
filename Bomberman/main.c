#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h> 
#include <allegro5/allegro_image.h>   
#include <allegro5/allegro_font.h>   
#include <allegro5/allegro_ttf.h>     
#include <stdio.h> 
#include <stdlib.h> 
#include <time.h>   

#define MAP_WIDTH 15
#define MAP_HEIGHT 13
#define TILE_SIZE 32 // Rozmiar blocka w pikselach
#define HUD_HEIGHT (TILE_SIZE * 2) // Wysokość status bar

ALLEGRO_FONT* font_main = NULL;

ALLEGRO_BITMAP* player_sprite_front = NULL;
ALLEGRO_BITMAP* player_sprite_back = NULL;
ALLEGRO_BITMAP* player_sprite_left = NULL;
ALLEGRO_BITMAP* player_sprite_right = NULL;

ALLEGRO_BITMAP* destructible_wall_sprite = NULL;
ALLEGRO_BITMAP* dynamite_sprite = NULL;
ALLEGRO_BITMAP* sparks_sprite = NULL;

typedef enum {
    PLAYER_DIR_DOWN,
    PLAYER_DIR_UP,
    PLAYER_DIR_LEFT,
    PLAYER_DIR_RIGHT
} PLAYER_DIRECTION;

typedef enum {
    START_SCREEN,
    PLAYING,
    GAME_OVER
} GAME_STATE;

GAME_STATE current_game_state = START_SCREEN;

// Typy kafelków
enum TILE_TYPE {
    EMPTY,
    SOLID_WALL,
    DESTRUCTIBLE_WALL,
};

int game_map[MAP_HEIGHT][MAP_WIDTH];

#define PLAYER_MAX_LIVES 3
#define INVINCIBILITY_DURATION 120 // 2 sekundy nietykalności przy 60 FPS

typedef struct {
    int x;
    int y;
    int lives;
    bool is_alive;
    bool invincible;
    int invincibility_timer;
    int current_max_bombs;
    int current_bomb_radius;
    PLAYER_DIRECTION direction; // Aktualny kierunek gracza
} Player;

Player player; // Zmienna globalna dla naszego gracza

#define MAX_ENEMIES 5 // ilosc enemy
#define ENEMY_MOVE_DELAY 30 // Wróg będzie próbował się ruszać co 0.5 sekundy (przy 60 FPS)

// Kierunki ruchu wrogów
typedef enum {
    DIR_UP,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT,
    DIR_COUNT
} ENEMY_DIRECTION;

typedef struct {
    int x, y;                   // Koordynaty na mapie
    bool is_alive;              // Czy wróg żyje
    ALLEGRO_COLOR color;        // Kolor wroga
    ENEMY_DIRECTION direction;  // Aktualny kierunek ruchu
    int move_timer;             // Licznik czasu do następnej próby ruchu
} Enemy;

Enemy enemies[MAX_ENEMIES]; // Tablica naszych wrogów

#define MAX_POWERUPS (MAX_ENEMIES) // Maksymalna ilość power-upów na mapie = liczbie wrogów (można więcej)
#define POWERUP_DROP_CHANCE 3      // Szansa na drop 1 do 3 (rand() % POWERUP_DROP_CHANCE == 0)

typedef enum {
    POWERUP_BOMB_CAP,       // +1 bomba
    POWERUP_RADIUS_INC,     // +1 do promienia wybuchu
    POWERUP_EXTRA_LIFE,     // +1 życie
    POWERUP_TYPE_COUNT      // Liczba typów power-upów
} POWERUP_TYPE;

typedef struct {
    int x, y;
    POWERUP_TYPE type;
    bool is_active;
    ALLEGRO_COLOR color;
} Powerup;

Powerup powerups[MAX_POWERUPS];

#define MAX_BOMBS 5 // Zaczynamy z jedną bombą, potem można zwiększyć
#define EXPLOSION_DURATION 30 // 0.5 sekundy przy 60 FPS
#define BOMB_TIMER_DURATION 120 // 3 sekundy przy 60 FPS
#define MAX_EXPLOSION_CELLS (1 + 4 * 7) // Maksymalny promień 7 (1 centrum + 4 ramiona po 7 kafelków) - dopasuj jeśli trzeba

typedef struct {
    int x, y;
    int timer;
    int radius;
    bool active;
    bool exploding;
    int explosion_timer;
    int affected_explosion_cells_x[MAX_EXPLOSION_CELLS];
    int affected_explosion_cells_y[MAX_EXPLOSION_CELLS];
    int num_affected_explosion_cells;
} Bomb;

Bomb bombs[MAX_BOMBS]; // Tablica do przechowywania bomb

// Inicjalizacja mapy
void initialize_map() {
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (y == 0 || y == MAP_HEIGHT - 1 || x == 0 || x == MAP_WIDTH - 1) {
                game_map[y][x] = SOLID_WALL; // Ściany na obwodzie
            }
            else if (x % 2 == 0 && y % 2 == 0) { // Rozstawiamy trochę nierozbieralnych bloków w środku
                game_map[y][x] = SOLID_WALL;
            }
            else if (rand() % 3 == 0) { // Losowo ściany z możliwością zniszczenia 
                game_map[y][x] = DESTRUCTIBLE_WALL;
            }
            else {
                game_map[y][x] = EMPTY; // Puste miejsce
            }
        }
    }
}

// Inicjalizacja wrogów z pozycją startową
void initialize_enemies(int map[MAP_HEIGHT][MAP_WIDTH], Player* p_player) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].is_alive = true;
        enemies[i].color = al_map_rgb(255, 100, 100);
        enemies[i].move_timer = rand() % ENEMY_MOVE_DELAY; // Mały rozrzut startu ruchu
        enemies[i].direction = (ENEMY_DIRECTION)(rand() % DIR_COUNT); // Losowy kierunek startowy

        // Szukamy bezpiecznego miejsca do pojawienia się wroga
        bool spot_found = false;
        int attempts = 0;
        while (!spot_found && attempts < MAP_WIDTH * MAP_HEIGHT) { // Ograniczamy liczbę prób
            int ex = rand() % MAP_WIDTH;
            int ey = rand() % MAP_HEIGHT;

            // Sprawdzamy, czy pole jest puste, nie zajęte przez gracza ani innego wroga
            if (map[ey][ex] == EMPTY && (ex != p_player->x || ey != p_player->y)) {
                bool occupied_by_other_enemy = false;
                for (int j = 0; j < i; j++) { // Sprawdzamy tylko już utworzonych wrogów
                    if (enemies[j].is_alive && enemies[j].x == ex && enemies[j].y == ey) {
                        occupied_by_other_enemy = true;
                        break;
                    }
                }
                if (!occupied_by_other_enemy) {
                    enemies[i].x = ex;
                    enemies[i].y = ey;
                    spot_found = true;
                    printf("Wróg %d pojawił się na (%d, %d)\n", i, ex, ey);
                }
            }
            attempts++;
        }
        if (!spot_found) { // Jeśli nie znaleźliśmy miejsca
            enemies[i].is_alive = false; // Ten wróg nie będzie aktywny
            printf("Nie znaleziono miejsca dla wroga %d\n", i);
        }
    }
}

// Szukamy i ustawiamy miejsce startowe gracza
void find_and_set_player_spawn(Player* p_player, int map[MAP_HEIGHT][MAP_WIDTH]) {
    bool found_spawn = false;

    // Próba znalezienia idealnego miejsca: aktualna komórka + 2 puste w jednym kierunku
    for (int y = 0; y < MAP_HEIGHT && !found_spawn; y++) {
        for (int x = 0; x < MAP_WIDTH && !found_spawn; x++) {
            if (map[y][x] == EMPTY) { // Kandydat na spawn (x,y)

                // Sprawdzenie W PRAWO
                if (x + 2 < MAP_WIDTH && map[y][x + 1] == EMPTY && map[y][x + 2] == EMPTY) {
                    p_player->x = x; p_player->y = y; found_spawn = true;
                }
                // Sprawdzenie W LEWO
                if (!found_spawn && x - 2 >= 0 && map[y][x - 1] == EMPTY && map[y][x - 2] == EMPTY) {
                    p_player->x = x; p_player->y = y; found_spawn = true;
                }
                // Sprawdzenie W DÓŁ
                if (!found_spawn && y + 2 < MAP_HEIGHT && map[y + 1][x] == EMPTY && map[y + 2][x] == EMPTY) {
                    p_player->x = x; p_player->y = y; found_spawn = true;
                }
                // Sprawdzenie W GÓRĘ
                if (!found_spawn && y - 2 >= 0 && map[y - 1][x] == EMPTY && map[y - 2][x] == EMPTY) {
                    p_player->x = x; p_player->y = y; found_spawn = true;
                }
            }
        }
    }

    if (!found_spawn) {
        // Jeśli nie znaleziono idealnego miejsca, szukamy dowolnej pustej komórki
        printf("Uwaga: Nie znaleziono idealnego miejsca startowego na 2 kroki. Szukam dowolnej pustej komórki...\n");
        for (int y = 0; y < MAP_HEIGHT && !found_spawn; y++) {
            for (int x = 0; x < MAP_WIDTH && !found_spawn; x++) {
                if (map[y][x] == EMPTY) {
                    p_player->x = x; p_player->y = y; found_spawn = true;
                }
            }
        }
    }

    if (!found_spawn) {
        // Najgorszy przypadek: brak pustych pól 
        // Ustawiamy gracza na (1,1) i wymuszamy puste pole.
        p_player->x = 1;
        p_player->y = 1;
        map[1][1] = EMPTY;
        printf("KRYTYCZNY KOMUNIKAT: Nie znaleziono żadnego pustego punktu startowego. Domyślnie ustawiam na (1,1) i wymuszam pustość.\n");
    }

    // Upewniamy się, że wybrane pole startowe jest puste
    map[p_player->y][p_player->x] = EMPTY;
    printf("Gracz pojawił się na (%d, %d)\n", p_player->x, p_player->y);
}

// Próba położenia bomby przez gracza
void try_plant_bomb(Player* p) {
    int active_player_bombs = 0;
    for (int i = 0; i < MAX_BOMBS; i++) { // MAX_BOMBS to ogólna pula dostępnych slotów na bomby
        if (bombs[i].active) {
            active_player_bombs++;
        }
    }

    if (active_player_bombs >= p->current_max_bombs) {
        printf("Osiągnięto limit bomb (%d)!\n", p->current_max_bombs);
        return; // Nie możemy postawić więcej
    }

    // Szukamy wolnego slotu w tablicy bomb
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (!bombs[i].active) {
            // Sprawdzenie, czy nie ma już bomby na tym kafelku (ciągle aktualne)
            bool already_bomb_here = false;
            for (int j = 0; j < MAX_BOMBS; j++) {
                if (bombs[j].active && bombs[j].x == p->x && bombs[j].y == p->y) {
                    already_bomb_here = true;
                    break;
                }
            }

            if (!already_bomb_here) {
                bombs[i].active = true;
                bombs[i].x = p->x;
                bombs[i].y = p->y;
                bombs[i].timer = BOMB_TIMER_DURATION;
                bombs[i].radius = p->current_bomb_radius;
                bombs[i].exploding = false;
                bombs[i].explosion_timer = 0;
                printf("Bomba (promień %d) została położona na (%d, %d)!\n", bombs[i].radius, bombs[i].x, bombs[i].y);
                break;
            }
            else {
                printf("Tu już jest bomba (inna aktywna)!\n");
            }
        }
    }
}

// Przygotowanie nowej gry - resetowanie stanu
void setup_new_game() {
    initialize_map();
    find_and_set_player_spawn(&player, game_map);

    player.lives = PLAYER_MAX_LIVES;
    player.is_alive = true;
    player.invincible = false;
    player.invincibility_timer = 0;
    player.current_max_bombs = 2;
    player.current_bomb_radius = 1;
    player.direction = PLAYER_DIR_DOWN;

    initialize_enemies(game_map, &player);

    // Reset bomb
    for (int i = 0; i < MAX_BOMBS; i++) {
        bombs[i].active = false;
        bombs[i].exploding = false;
        bombs[i].explosion_timer = 0;
        bombs[i].num_affected_explosion_cells = 0;
    }

    // Reset powerupów
    for (int i = 0; i < MAX_POWERUPS; i++) {
        powerups[i].is_active = false;
    }

    current_game_state = PLAYING;
    printf("Nowa gra rozpoczęta!\n");
}

int main() {
    if (!al_init()) {
        fprintf(stderr, "Allegro nie zainicjalizowało się!\n");
        return -1;
    }

    if (!al_install_keyboard()) {
        fprintf(stderr, "Klawiatura nie została zainstalowana...\n");
        return -1;
    }

    al_init_primitives_addon();
    al_init_image_addon();
    al_init_font_addon();
    al_init_ttf_addon();


    font_main = al_load_ttf_font("arial.ttf", 36, 0);
    if (!font_main) {
        fprintf(stderr, "Czcionka nie załadowała się! (arial.ttf)\n");
    }
    ALLEGRO_DISPLAY* display = al_create_display(MAP_WIDTH * TILE_SIZE, (MAP_HEIGHT * TILE_SIZE) + HUD_HEIGHT);
    if (!display) {
        fprintf(stderr, "Okno się nie utworzyło!\n");
        return -1;
    }

    player_sprite_front = al_load_bitmap("player-front.png");
    player_sprite_back = al_load_bitmap("player-back.png");
    player_sprite_left = al_load_bitmap("player-left.png");
    player_sprite_right = al_load_bitmap("player-right.png");

    if (!player_sprite_front || !player_sprite_back || !player_sprite_left || !player_sprite_right) {
        fprintf(stderr, "Jeden z sprite'ów gracza nie załadował się! Sprawdź pliki: player-front.png, player-back.png, player-left.png, player-right.png\n");
        if (player_sprite_back) al_destroy_bitmap(player_sprite_back);
        if (player_sprite_left) al_destroy_bitmap(player_sprite_left);
        if (player_sprite_right) al_destroy_bitmap(player_sprite_right);
        al_uninstall_keyboard();
        return -1;
    }

    destructible_wall_sprite = al_load_bitmap("box.png");
    if (!destructible_wall_sprite) {
        fprintf(stderr, "Sprite pudełka (box.png) nie załadował się!\n");
        if (player_sprite_front) al_destroy_bitmap(player_sprite_front);
        if (font_main) al_destroy_font(font_main);
        return -1;
    }

    dynamite_sprite = al_load_bitmap("dynamite.png");
    if (!dynamite_sprite) {
        fprintf(stderr, "Sprite dynamitu (dynamite.png) nie załadował się!\n");
        if (player_sprite_front) al_destroy_bitmap(player_sprite_front);
        if (destructible_wall_sprite) al_destroy_bitmap(destructible_wall_sprite);
        if (font_main) al_destroy_font(font_main);
        if (display) al_destroy_display(display);
        return -1;
    }

    sparks_sprite = al_load_bitmap("sparks.png");
    if (!sparks_sprite) {
        fprintf(stderr, "Sprite iskier (sparks.png) nie załadował się!\n");
        if (player_sprite_front) al_destroy_bitmap(player_sprite_front);
        if (destructible_wall_sprite) al_destroy_bitmap(destructible_wall_sprite);
        if (dynamite_sprite) al_destroy_bitmap(dynamite_sprite);
        if (font_main) al_destroy_font(font_main);
        if (display) al_destroy_display(display);
        return -1;
    }

    ALLEGRO_EVENT_QUEUE* event_queue = al_create_event_queue(); // Kolejka zdarzeń
    if (!event_queue) {
        fprintf(stderr, "Problem z kolejką zdarzeń.\n");
        al_destroy_display(display);
        return -1;
    }

    al_register_event_source(event_queue, al_get_display_event_source(display));
    al_register_event_source(event_queue, al_get_keyboard_event_source());

    srand(time(NULL));

    bool done = false;
    ALLEGRO_TIMER* timer = al_create_timer(1.0 / 60.0); // Timer na 60 FPS
    if (!timer) {
        fprintf(stderr, "Timer się zepsuł...\n");
        al_destroy_display(display);
        al_destroy_event_queue(event_queue);
        return -1;
    }
    al_register_event_source(event_queue, al_get_timer_event_source(timer));
    al_start_timer(timer);

    // Główna pętla gry
    while (!done) {
        ALLEGRO_EVENT event;
        al_wait_for_event(event_queue, &event); // Czekamy na zdarzenie

        if (event.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
            done = true;
        }
        else if (event.type == ALLEGRO_EVENT_KEY_DOWN) {
            if (event.keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
                done = true;
            }
            if (current_game_state == START_SCREEN) {
                if (event.keyboard.keycode == ALLEGRO_KEY_ENTER) {
                    setup_new_game();
                }
            }
            else if (current_game_state == PLAYING && player.is_alive) {
                int next_x = player.x;
                int next_y = player.y;

                if (event.keyboard.keycode == ALLEGRO_KEY_UP || event.keyboard.keycode == ALLEGRO_KEY_W) {
                    next_y--;
                    player.direction = PLAYER_DIR_UP;
                }
                else if (event.keyboard.keycode == ALLEGRO_KEY_DOWN || event.keyboard.keycode == ALLEGRO_KEY_S) {
                    next_y++;
                    player.direction = PLAYER_DIR_DOWN;
                }
                else if (event.keyboard.keycode == ALLEGRO_KEY_LEFT || event.keyboard.keycode == ALLEGRO_KEY_A) {
                    next_x--;
                    player.direction = PLAYER_DIR_LEFT;
                }
                else if (event.keyboard.keycode == ALLEGRO_KEY_RIGHT || event.keyboard.keycode == ALLEGRO_KEY_D) {
                    next_x++;
                    player.direction = PLAYER_DIR_RIGHT;
                }
                else if (event.keyboard.keycode == ALLEGRO_KEY_SPACE) {
                    try_plant_bomb(&player);
                }

                if (next_x != player.x || next_y != player.y) {
                    if (next_x >= 0 && next_x < MAP_WIDTH &&
                        next_y >= 0 && next_y < MAP_HEIGHT &&
                        game_map[next_y][next_x] != SOLID_WALL &&
                        game_map[next_y][next_x] != DESTRUCTIBLE_WALL) {
                        // Jeśli wszystkie testy przejdą, przesuwamy gracza:
                        player.x = next_x;
                        player.y = next_y;

                        //  sprawdzenie czy zebrany power up 
                        for (int i = 0; i < MAX_POWERUPS; i++) {
                            if (powerups[i].is_active && powerups[i].x == player.x && powerups[i].y == player.y) {
                                powerups[i].is_active = false; // Power-up zebrany
                                printf("Gracz zebrał power-up typu %d!\n", powerups[i].type);

                                if (powerups[i].type == POWERUP_BOMB_CAP) {
                                    if (player.current_max_bombs < MAX_BOMBS) {
                                        player.current_max_bombs++;
                                        printf("Limit bomb zwiększony do %d!\n", player.current_max_bombs);
                                    }
                                    else {
                                        printf("Osiągnięto maksymalny limit bomb!\n");
                                    }
                                }
                                else if (powerups[i].type == POWERUP_RADIUS_INC) {
                                    if (player.current_bomb_radius < 7) {
                                        player.current_bomb_radius++;
                                        printf("Promień wybuchu zwiększony do %d!\n", player.current_bomb_radius);
                                    }
                                    else {
                                        printf("Osiągnięto maksymalny promień wybuchu!\n");
                                    }
                                }
                                else if (powerups[i].type == POWERUP_EXTRA_LIFE) {
                                    if (player.lives < PLAYER_MAX_LIVES) {
                                        player.lives++;
                                        printf("Zdobyto dodatkowe życie! Łącznie żyć: %d\n", player.lives);
                                    }
                                    else {
                                        printf("Osiągnięto maksymalną liczbę żyć!\n");
                                    }
                                }
                                break;
                            }
                        }
                    }
                }
            }
            else if (current_game_state == GAME_OVER) {
                if (event.keyboard.keycode == ALLEGRO_KEY_ENTER) {
                    setup_new_game();
                }
            }
        }
        else if (event.type == ALLEGRO_EVENT_TIMER) {
            if (current_game_state == PLAYING) {
                // logika game(aktualizacja pozycji, stanów) 
                if (player.invincible) {
                    player.invincibility_timer--;
                    if (player.invincibility_timer <= 0) {
                        player.invincible = false;
                        printf("Gracz znowu jest wrażliwy.\n");
                    }
                }

                // Obsługa bomb i wybuchów
                for (int i = 0; i < MAX_BOMBS; i++) {
                    if (bombs[i].active) {
                        if (!bombs[i].exploding) {
                            bombs[i].timer--;
                            if (bombs[i].timer <= 0) {
                                printf("ROZPOCZĘCIE WYBUCHU bomby na (%d, %d) z promieniem %d!\n", bombs[i].x, bombs[i].y, bombs[i].radius);
                                bombs[i].exploding = true;
                                bombs[i].explosion_timer = EXPLOSION_DURATION;
                                bombs[i].num_affected_explosion_cells = 0; // Reset licznika dla bieżącej bomby

                                // Komórka centralna (dodajemy do pól struktury bombs[i])
                                if (game_map[bombs[i].y][bombs[i].x] != SOLID_WALL) {
                                    bombs[i].affected_explosion_cells_x[bombs[i].num_affected_explosion_cells] = bombs[i].x;
                                    bombs[i].affected_explosion_cells_y[bombs[i].num_affected_explosion_cells] = bombs[i].y;
                                    if (game_map[bombs[i].y][bombs[i].x] == DESTRUCTIBLE_WALL) {
                                        game_map[bombs[i].y][bombs[i].x] = EMPTY;
                                    }
                                    bombs[i].num_affected_explosion_cells++;
                                }

                                int dx[] = { 0, 0, -1, 1 };
                                int dy[] = { -1, 1, 0, 0 };

                                // Obliczanie zakresu wybuchu w czterech kierunkach
                                for (int dir = 0; dir < 4; dir++) {
                                    for (int r = 1; r <= bombs[i].radius; r++) {
                                        int cur_x = bombs[i].x + dx[dir] * r;
                                        int cur_y = bombs[i].y + dy[dir] * r;

                                        if (cur_x < 0 || cur_x >= MAP_WIDTH || cur_y < 0 || cur_y >= MAP_HEIGHT) break;

                                        if (bombs[i].num_affected_explosion_cells < MAX_EXPLOSION_CELLS) {
                                            bombs[i].affected_explosion_cells_x[bombs[i].num_affected_explosion_cells] = cur_x;
                                            bombs[i].affected_explosion_cells_y[bombs[i].num_affected_explosion_cells] = cur_y;
                                            bombs[i].num_affected_explosion_cells++;
                                        }
                                        else break;

                                        if (game_map[cur_y][cur_x] == SOLID_WALL) break;

                                        if (game_map[cur_y][cur_x] == DESTRUCTIBLE_WALL) {
                                            game_map[cur_y][cur_x] = EMPTY;
                                            break;
                                        }
                                    }
                                }

                                // Sprawdzamy czy gracz został trafiony wybuchem
                                bool player_hit_this_explosion = false;
                                for (int k = 0; k < bombs[i].num_affected_explosion_cells; k++) {
                                    int ex = bombs[i].affected_explosion_cells_x[k];
                                    int ey = bombs[i].affected_explosion_cells_y[k];
                                    if (player.is_alive && !player.invincible && !player_hit_this_explosion && player.x == ex && player.y == ey) {
                                        player.lives--;
                                        player_hit_this_explosion = true;
                                        printf("Gracz otrzymał obrażenia! Żyć zostało: %d\n", player.lives);
                                        if (player.lives <= 0) {
                                            player.is_alive = false;
                                            printf("GRACZ ZGINĄŁ! KONIEC GRY!\n");
                                            current_game_state = GAME_OVER;
                                        }
                                        else {
                                            player.invincible = true;
                                            player.invincibility_timer = INVINCIBILITY_DURATION;
                                            printf("Gracz nietykalny przez %d klatek.\n", INVINCIBILITY_DURATION);
                                        }
                                    }
                                    // Sprawdzamy trafienia wrogów wybuchem
                                    for (int e_idx = 0; e_idx < MAX_ENEMIES; e_idx++) {
                                        if (enemies[e_idx].is_alive && enemies[e_idx].x == ex && enemies[e_idx].y == ey) {
                                            enemies[e_idx].is_alive = false;
                                            printf("Wróg %d na (%d, %d) zniszczony wybuchem!\n", e_idx, ex, ey);

                                            // logika drop chanse of power up
                                            if (rand() % POWERUP_DROP_CHANCE == 0) {
                                                for (int p_idx = 0; p_idx < MAX_POWERUPS; p_idx++) {
                                                    if (!powerups[p_idx].is_active) {
                                                        powerups[p_idx].is_active = true;
                                                        powerups[p_idx].x = enemies[e_idx].x;
                                                        powerups[p_idx].y = enemies[e_idx].y;
                                                        powerups[p_idx].type = (POWERUP_TYPE)(rand() % POWERUP_TYPE_COUNT);

                                                        if (powerups[p_idx].type == POWERUP_BOMB_CAP) powerups[p_idx].color = al_map_rgb(0, 0, 255);   // Niebieski
                                                        else if (powerups[p_idx].type == POWERUP_RADIUS_INC) powerups[p_idx].color = al_map_rgb(255, 165, 0); // Pomarańczowy
                                                        else if (powerups[p_idx].type == POWERUP_EXTRA_LIFE) powerups[p_idx].color = al_map_rgb(255, 20, 147); // Różowy

                                                        printf("Upuścił power-up typu %d na (%d,%d)!\n", powerups[p_idx].type, powerups[p_idx].x, powerups[p_idx].y);
                                                        break;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        else {
                            bombs[i].explosion_timer--;
                            if (bombs[i].explosion_timer <= 0) {
                                bombs[i].active = false;
                                bombs[i].exploding = false;
                                printf("Wybuch na (%d, %d) zakończony.\n", bombs[i].x, bombs[i].y);
                            }
                        }
                    }
                }

                // logika wrtogow
                for (int i = 0; i < MAX_ENEMIES; i++) {
                    if (enemies[i].is_alive) {
                        enemies[i].move_timer--;
                        if (enemies[i].move_timer <= 0) {
                            enemies[i].move_timer = ENEMY_MOVE_DELAY;

                            int next_ex = enemies[i].x;
                            int next_ey = enemies[i].y;

                            // Próba ruchu w aktualnym kierunku
                            if (enemies[i].direction == DIR_UP) next_ey--;
                            else if (enemies[i].direction == DIR_DOWN) next_ey++;
                            else if (enemies[i].direction == DIR_LEFT) next_ex--;
                            else if (enemies[i].direction == DIR_RIGHT) next_ex++;

                            // Sprawdzenie kolizji dla wroga
                            bool can_move = true;
                            if (next_ex < 0 || next_ex >= MAP_WIDTH || next_ey < 0 || next_ey >= MAP_HEIGHT ||
                                game_map[next_ey][next_ex] == SOLID_WALL ||
                                game_map[next_ey][next_ex] == DESTRUCTIBLE_WALL) {
                                can_move = false;
                            }

                            // Aby wrogowie nie chodzili po bombach
                            for (int b = 0; b < MAX_BOMBS; b++) {
                                if (bombs[b].active && bombs[b].x == next_ex && bombs[b].y == next_ey) {
                                    can_move = false;
                                    break;
                                }
                            }

                            if (can_move) {
                                enemies[i].x = next_ex;
                                enemies[i].y = next_ey;
                            }
                            else {
                                // Jeśli nie może się ruszyć, wybiera losowy nowy kierunek
                                enemies[i].direction = (ENEMY_DIRECTION)(rand() % DIR_COUNT);
                            }
                        }
                    }
                }

                // Sprawdzamy kolizję gracza z wrogami
                if (player.is_alive && !player.invincible) {
                    for (int i = 0; i < MAX_ENEMIES; i++) {
                        if (enemies[i].is_alive && player.x == enemies[i].x && player.y == enemies[i].y) {
                            player.lives--;
                            printf("Gracz zderzył się z wrogiem! Pozostało żyć: %d\n", player.lives);

                            if (player.lives <= 0) {
                                player.is_alive = false;
                                printf("GRACZ ZGINĄŁ OD WROGA! KONIEC GRY!\n");
                                current_game_state = GAME_OVER;
                            }
                            else {
                                player.invincible = true;
                                player.invincibility_timer = INVINCIBILITY_DURATION;
                                printf("Gracz nietykalny przez %d klatek po zderzeniu z wrogiem.\n", INVINCIBILITY_DURATION);
                            }
                            break;
                        }
                    }
                }
            }
            else if (current_game_state == GAME_OVER) {
            }

            al_clear_to_color(al_map_rgb(0, 0, 0));

            if (current_game_state == START_SCREEN) {
                if (font_main) {
                    float display_w = al_get_display_width(display);
                    float display_h = al_get_display_height(display);
                    al_draw_text(font_main, al_map_rgb(255, 255, 0), display_w / 2, display_h / 4, ALLEGRO_ALIGN_CENTER, "Bomberman");
                    al_draw_text(font_main, al_map_rgb(200, 200, 200), display_w / 2, display_h / 2, ALLEGRO_ALIGN_CENTER, "Press ENTER to start");
                    al_draw_text(font_main, al_map_rgb(150, 150, 150), display_w / 2, display_h / 2 + al_get_font_line_height(font_main) + 10, ALLEGRO_ALIGN_CENTER, "ESC to exit");
                }
            }
            else if (current_game_state == PLAYING) {
                // rysowanie top bar (hp)
                if (font_main) {
                    char lives_text[20];
                    sprintf_s(lives_text, sizeof(lives_text), "HP: %d", player.lives);
                    al_draw_text(font_main, al_map_rgb(255, 255, 255), 10, 10, ALLEGRO_ALIGN_LEFT, lives_text);

                    char bomb_info_text[50];
                    sprintf_s(bomb_info_text, sizeof(bomb_info_text), "Bomby: %d | promień: %d", player.current_max_bombs, player.current_bomb_radius);
                    al_draw_text(font_main, al_map_rgb(255, 255, 255), 10, 10 + al_get_font_line_height(font_main) + 5, ALLEGRO_ALIGN_LEFT, bomb_info_text);
                }

                // Rysujemy mapę
                for (int y_map = 0; y_map < MAP_HEIGHT; y_map++) {
                    for (int x_map = 0; x_map < MAP_WIDTH; x_map++) {
                        int tile_x_pos = x_map * TILE_SIZE;
                        int tile_y_pos = y_map * TILE_SIZE + HUD_HEIGHT;

                        if (game_map[y_map][x_map] == SOLID_WALL) {
                            al_draw_filled_rectangle(tile_x_pos, tile_y_pos, tile_x_pos + TILE_SIZE, tile_y_pos + TILE_SIZE, al_map_rgb(100, 100, 100));
                        }
                        else if (game_map[y_map][x_map] == DESTRUCTIBLE_WALL) {
                            if (destructible_wall_sprite) {
                                al_draw_bitmap(destructible_wall_sprite, tile_x_pos, tile_y_pos, 0);
                            }
                            else {
                                al_draw_filled_rectangle(tile_x_pos, tile_y_pos, tile_x_pos + TILE_SIZE, tile_y_pos + TILE_SIZE, al_map_rgb(150, 75, 0));
                            }
                        }
                        al_draw_rectangle(tile_x_pos, tile_y_pos, tile_x_pos + TILE_SIZE, tile_y_pos + TILE_SIZE, al_map_rgb(50, 50, 50), 1); // Siatka
                    }
                }

                // Rysujemy power-upy
                for (int i = 0; i < MAX_POWERUPS; i++) {
                    if (powerups[i].is_active) {
                        al_draw_filled_rectangle(powerups[i].x * TILE_SIZE + TILE_SIZE / 4,
                            powerups[i].y * TILE_SIZE + TILE_SIZE / 4 + HUD_HEIGHT,
                            powerups[i].x * TILE_SIZE + (TILE_SIZE * 3) / 4,
                            powerups[i].y * TILE_SIZE + (TILE_SIZE * 3) / 4 + HUD_HEIGHT,
                            powerups[i].color);
                        al_draw_rectangle(powerups[i].x * TILE_SIZE + TILE_SIZE / 4,
                            powerups[i].y * TILE_SIZE + TILE_SIZE / 4 + HUD_HEIGHT,
                            powerups[i].x * TILE_SIZE + (TILE_SIZE * 3) / 4,
                            powerups[i].y * TILE_SIZE + (TILE_SIZE * 3) / 4 + HUD_HEIGHT,
                            al_map_rgb(255, 255, 255), 1);
                    }
                }

                // Rysujemy bomby i wybuchy
                for (int i = 0; i < MAX_BOMBS; i++) {
                    if (bombs[i].active) {
                        if (bombs[i].exploding) {
                            if (sparks_sprite) {
                                for (int k = 0; k < bombs[i].num_affected_explosion_cells; k++) {
                                    int ex = bombs[i].affected_explosion_cells_x[k];
                                    int ey = bombs[i].affected_explosion_cells_y[k];

                                    if (game_map[ey][ex] != SOLID_WALL) {
                                        al_draw_bitmap(sparks_sprite,
                                            ex * TILE_SIZE,
                                            ey * TILE_SIZE + HUD_HEIGHT,
                                            0);
                                    }
                                }
                            }
                            else {
                                ALLEGRO_COLOR explosion_color = al_map_rgba(255, rand() % 155 + 100, 0, 150 + rand() % 105);
                                for (int k = 0; k < bombs[i].num_affected_explosion_cells; k++) {
                                    int ex = bombs[i].affected_explosion_cells_x[k];
                                    int ey = bombs[i].affected_explosion_cells_y[k];

                                    if (game_map[ey][ex] != SOLID_WALL) {
                                        al_draw_filled_rectangle(ex * TILE_SIZE,
                                            ey * TILE_SIZE + HUD_HEIGHT,
                                            ex * TILE_SIZE + TILE_SIZE,
                                            ey * TILE_SIZE + TILE_SIZE + HUD_HEIGHT,
                                            explosion_color);
                                    }
                                }
                            }
                        }
                        else { // Rysujemy odliczającą bombę
                            if (dynamite_sprite) {
                                al_draw_bitmap(dynamite_sprite,
                                    bombs[i].x * TILE_SIZE,
                                    bombs[i].y * TILE_SIZE + HUD_HEIGHT,
                                    0);

                                if (bombs[i].timer < 60 && (bombs[i].timer / 5) % 2 == 0) {
                                    float fuse_x = bombs[i].x * TILE_SIZE + TILE_SIZE / 2.0f;
                                    float fuse_y = bombs[i].y * TILE_SIZE + HUD_HEIGHT + TILE_SIZE / 4.0f;
                                    float fuse_radius = TILE_SIZE / 8.0f;
                                    al_draw_filled_circle(fuse_x, fuse_y, fuse_radius, al_map_rgb(255, 0, 0));
                                }
                            }
                            else {
                                float bomb_visual_size_mod = (float)(bombs[i].timer % 30) / 60.0f;
                                if (bombs[i].timer < 60) { bomb_visual_size_mod = (float)(bombs[i].timer % 10) / 20.0f; }
                                al_draw_filled_circle(bombs[i].x * TILE_SIZE + TILE_SIZE / 2.0f,
                                    bombs[i].y * TILE_SIZE + TILE_SIZE / 2.0f + HUD_HEIGHT,
                                    TILE_SIZE / 3.0f + bomb_visual_size_mod * 5.0f,
                                    al_map_rgb(30, 30, 30));
                                if (bombs[i].timer < 60 && (bombs[i].timer / 5) % 2 == 0) {
                                    al_draw_filled_circle(bombs[i].x * TILE_SIZE + TILE_SIZE / 2.0f,
                                        bombs[i].y * TILE_SIZE + TILE_SIZE / 2.0f + HUD_HEIGHT,
                                        TILE_SIZE / 6.0f,
                                        al_map_rgb(255, 0, 0));
                                }
                            }
                        }
                    }
                }

                // Rysujemy wrogów
                for (int i = 0; i < MAX_ENEMIES; i++) {
                    if (enemies[i].is_alive) {
                        al_draw_filled_rectangle(enemies[i].x * TILE_SIZE,
                            enemies[i].y * TILE_SIZE + HUD_HEIGHT,
                            enemies[i].x * TILE_SIZE + TILE_SIZE,
                            enemies[i].y * TILE_SIZE + TILE_SIZE + HUD_HEIGHT,
                            enemies[i].color);
                        al_draw_rectangle(enemies[i].x * TILE_SIZE + 1,
                            enemies[i].y * TILE_SIZE + 1 + HUD_HEIGHT,
                            enemies[i].x * TILE_SIZE + TILE_SIZE - 1,
                            enemies[i].y * TILE_SIZE + TILE_SIZE - 1 + HUD_HEIGHT,
                            al_map_rgb(0, 0, 0), 1);
                    }
                }

                // Rysujemy gracza
                if (player.is_alive) {
                    ALLEGRO_BITMAP* sprite_to_draw = NULL;

                    switch (player.direction) {
                    case PLAYER_DIR_UP:
                        sprite_to_draw = player_sprite_back;
                        break;
                    case PLAYER_DIR_DOWN:
                        sprite_to_draw = player_sprite_front;
                        break;
                    case PLAYER_DIR_LEFT:
                        sprite_to_draw = player_sprite_left;
                        break;
                    case PLAYER_DIR_RIGHT:
                        sprite_to_draw = player_sprite_right;
                        break;
                    default:
                        sprite_to_draw = player_sprite_front;
                        break;
                    }

                    if (sprite_to_draw) {
                        if (player.invincible) {
                            if ((player.invincibility_timer / 5) % 2 == 0) {
                                al_draw_bitmap(sprite_to_draw,
                                    player.x * TILE_SIZE,
                                    player.y * TILE_SIZE + HUD_HEIGHT,
                                    0);
                            }
                        }
                        else {
                            al_draw_bitmap(sprite_to_draw,
                                player.x * TILE_SIZE,
                                player.y * TILE_SIZE + HUD_HEIGHT,
                                0);
                        }
                    }
                }
            }
            else if (current_game_state == GAME_OVER) {
                if (font_main) {
                    float display_w = al_get_display_width(display);
                    float display_h = al_get_display_height(display);
                    al_draw_text(font_main, al_map_rgb(255, 0, 0),
                        display_w / 2,
                        display_h / 2 - (al_get_font_line_height(font_main) / 2),
                        ALLEGRO_ALIGN_CENTER, "GAME OVER");
                }
            }

            al_flip_display();
        }
    }


    al_destroy_timer(timer);

    if (player_sprite_front) al_destroy_bitmap(player_sprite_front);
    if (player_sprite_back) al_destroy_bitmap(player_sprite_back);
    if (player_sprite_left) al_destroy_bitmap(player_sprite_left);
    if (player_sprite_right) al_destroy_bitmap(player_sprite_right);

    if (destructible_wall_sprite) al_destroy_bitmap(destructible_wall_sprite);
    if (dynamite_sprite) al_destroy_bitmap(dynamite_sprite);
    if (sparks_sprite) al_destroy_bitmap(sparks_sprite);

    al_destroy_display(display);
    al_destroy_event_queue(event_queue);
    al_uninstall_keyboard();
    if (font_main) {
        al_destroy_font(font_main);
    }
    return 0;
}
