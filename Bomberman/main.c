#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h> 
#include <allegro5/allegro_image.h>      
#include <allegro5/allegro_font.h>       
#include <allegro5/allegro_ttf.h>        
#include <allegro5/allegro_audio.h>      
#include <allegro5/allegro_acodec.h>     
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h> 

/**
 * @file main.c
 * @brief Prosta implementacja gry Bomberman przy użyciu biblioteki Allegro 5.
 * * Gra zawiera podstawowe mechaniki takie jak ruch gracza, podkładanie bomb,
 * niszczenie ścian, zbieranie power-upów, walkę z wrogami oraz system punktacji.
 * Celem gry jest pokonanie wszystkich wrogów i znalezienie ukrytego wyjścia.
 */

 // --- Definicje globalne ---
 /** @def MAP_WIDTH Szerokość mapy w kafelkach. */
#define MAP_WIDTH 15            
/** @def MAP_HEIGHT Wysokość mapy w kafelkach. */
#define MAP_HEIGHT 13           
/** @def TILE_SIZE Rozmiar pojedynczego kafelka na mapie w pikselach. */
#define TILE_SIZE 32            
/** @def HUD_HEIGHT Wysokość paska interfejsu użytkownika (HUD) w pikselach. */
#define HUD_HEIGHT (TILE_SIZE * 2) 

// --- Globalne wskaźniki na zasoby Allegro ---
/** @var font_main Główna czcionka używana w grze. */
ALLEGRO_FONT* font_main = NULL;

// Sprity gracza dla różnych kierunków
/** @var player_sprite_front Sprite gracza zwróconego przodem (do dołu). */
ALLEGRO_BITMAP* player_sprite_front = NULL;
/** @var player_sprite_back Sprite gracza zwróconego tyłem (do góry). */
ALLEGRO_BITMAP* player_sprite_back = NULL;
/** @var player_sprite_left Sprite gracza zwróconego w lewo. */
ALLEGRO_BITMAP* player_sprite_left = NULL;
/** @var player_sprite_right Sprite gracza zwróconego w prawo. */
ALLEGRO_BITMAP* player_sprite_right = NULL;

// Sprity dla obiektów w grze
/** @var destructible_wall_sprite Sprite zniszczalnej ściany (pudełka). */
ALLEGRO_BITMAP* destructible_wall_sprite = NULL;
/** @var dynamite_sprite Sprite bomby (dynamitu). */
ALLEGRO_BITMAP* dynamite_sprite = NULL;
/** @var sparks_sprite Sprite efektu eksplozji (iskier). */
ALLEGRO_BITMAP* sparks_sprite = NULL;
/** @var exit_sprite Sprite wyjścia z poziomu. */
ALLEGRO_BITMAP* exit_sprite = NULL;

// Zasoby audio
/** @var background_music_sample Wskaźnik do załadowanego pliku muzyki tła. */
ALLEGRO_SAMPLE* background_music_sample = NULL;
/** @var background_music_instance Wskaźnik do instancji muzyki tła, używanej do odtwarzania. */
ALLEGRO_SAMPLE_INSTANCE* background_music_instance = NULL;

// --- Typy wyliczeniowe (enumy) ---

/** @enum PLAYER_DIRECTION
 * @brief Kierunki, w które może być zwrócony gracz.
 */
typedef enum {
    PLAYER_DIR_DOWN, ///< Gracz zwrócony w dół.
    PLAYER_DIR_UP,   ///< Gracz zwrócony w górę.
    PLAYER_DIR_LEFT, ///< Gracz zwrócony w lewo.
    PLAYER_DIR_RIGHT ///< Gracz zwrócony w prawo.
} PLAYER_DIRECTION;

/** @enum GAME_STATE
 * @brief Możliwe stany, w jakich może znajdować się gra.
 */
typedef enum {
    START_SCREEN, ///< Ekran startowy gry.
    PLAYING,      ///< Gra jest w toku.
    GAME_OVER     ///< Gra zakończona (wygrana lub przegrana).
} GAME_STATE;

/** @var current_game_state Aktualny stan gry. */
GAME_STATE current_game_state = START_SCREEN;

/** @enum TILE_TYPE
 * @brief Typy kafelków, z których może składać się mapa gry.
 */
typedef enum {
    EMPTY,             ///< Puste pole, po którym można się poruszać.
    SOLID_WALL,        ///< Niezniszczalna ściana, blokująca ruch i eksplozje.
    DESTRUCTIBLE_WALL, ///< Zniszczalna ściana (pudełko), którą można zniszczyć bombą.
} TILE_TYPE;

/** @var game_map Dwuwymiarowa tablica reprezentująca mapę gry.
 * Przechowuje typ każdego kafelka na mapie.
 */
int game_map[MAP_HEIGHT][MAP_WIDTH];

// --- Zmienne związane z wyjściem z poziomu ---
/** @var exit_x Współrzędna X ukrytego wyjścia na mapie. */
int exit_x = -1;
/** @var exit_y Współrzędna Y ukrytego wyjścia na mapie. */
int exit_y = -1;
/** @var exit_revealed Flaga wskazująca, czy wyjście zostało odkryte przez gracza. */
bool exit_revealed = false;

// --- Definicje dla gracza ---
/** @def PLAYER_MAX_LIVES Maksymalna liczba żyć, jaką może posiadać gracz. */
#define PLAYER_MAX_LIVES 3       
/** @def INVINCIBILITY_DURATION Czas trwania nietykalności gracza po otrzymaniu obrażeń, w klatkach. */
#define INVINCIBILITY_DURATION 120 

/**
 * @struct Player
 * @brief Struktura przechowująca wszystkie informacje dotyczące gracza.
 */
typedef struct {
    int x, y;                     ///< Pozycja gracza na mapie (współrzędne kafelków).
    int lives;                    ///< Aktualna liczba żyć gracza.
    int score;                    ///< Aktualny wynik punktowy gracza.
    bool is_alive;                ///< Flaga wskazująca, czy gracz żyje.
    bool invincible;              ///< Flaga wskazująca, czy gracz jest aktualnie nietykalny.
    int invincibility_timer;      ///< Licznik pozostałego czasu nietykalności.
    int current_max_bombs;        ///< Maksymalna liczba bomb, które gracz może jednocześnie podłożyć.
    int current_bomb_radius;      ///< Aktualny promień rażenia bomb gracza.
    PLAYER_DIRECTION direction;   ///< Kierunek, w którym gracz jest obecnie zwrócony.
} Player;

/** @var player Globalna instancja struktury gracza. */
Player player;

// --- Definicje dla wrogów ---
/** @def MAX_ENEMIES Maksymalna liczba wrogów na planszy. */
#define MAX_ENEMIES 5             
/** @def ENEMY_MOVE_DELAY Opóźnienie między kolejnymi próbami ruchu wroga, w klatkach. */
#define ENEMY_MOVE_DELAY 30       
/** @def POINTS_PER_ENEMY Liczba punktów przyznawana za pokonanie jednego wroga. */
#define POINTS_PER_ENEMY 100      
/** @def POINTS_PER_WALL Liczba punktów przyznawana za zniszczenie jednej zniszczalnej ściany. */
#define POINTS_PER_WALL 10        

/** @enum ENEMY_DIRECTION
 * @brief Kierunki, w których mogą poruszać się wrogowie.
 */
typedef enum {
    DIR_UP,    ///< Ruch w górę.
    DIR_DOWN,  ///< Ruch w dół.
    DIR_LEFT,  ///< Ruch w lewo.
    DIR_RIGHT, ///< Ruch w prawo.
    DIR_COUNT  ///< Liczba możliwych kierunków (używane do losowania).
} ENEMY_DIRECTION;

/**
 * @struct Enemy
 * @brief Struktura przechowująca informacje o pojedynczym wrogu.
 */
typedef struct {
    int x, y;                  ///< Pozycja wroga na mapie (współrzędne kafelków).
    bool is_alive;             ///< Flaga wskazująca, czy wróg żyje.
    ALLEGRO_COLOR color;       ///< Kolor wroga (używany, jeśli brakuje dedykowanego sprite'a).
    ENEMY_DIRECTION direction; ///< Aktualny kierunek ruchu wroga.
    int move_timer;            ///< Licznik czasu do następnej próby ruchu wroga.
} Enemy;

/** @var enemies Tablica przechowująca instancje wszystkich wrogów w grze. */
Enemy enemies[MAX_ENEMIES];

// --- Definicje dla power-upów ---
/** @def MAX_POWERUPS Maksymalna liczba power-upów, które mogą pojawić się na mapie. */
#define MAX_POWERUPS (MAX_ENEMIES) 
/** @def POWERUP_DROP_CHANCE Szansa na wypadnięcie power-upa po pokonaniu wroga (1 do POWERUP_DROP_CHANCE). */
#define POWERUP_DROP_CHANCE 3      

/** @enum POWERUP_TYPE
 * @brief Typy dostępnych power-upów w grze.
 */
typedef enum {
    POWERUP_BOMB_CAP,     ///< Power-up zwiększający maksymalną liczbę bomb.
    POWERUP_RADIUS_INC,   ///< Power-up zwiększający promień rażenia bomb.
    POWERUP_EXTRA_LIFE,   ///< Power-up dający dodatkowe życie.
    POWERUP_TYPE_COUNT    ///< Liczba dostępnych typów power-upów.
} POWERUP_TYPE;

/**
 * @struct Powerup
 * @brief Struktura przechowująca informacje o pojedynczym power-upie.
 */
typedef struct {
    int x, y;             ///< Pozycja power-upa na mapie (współrzędne kafelków).
    POWERUP_TYPE type;    ///< Typ power-upa.
    bool is_active;       ///< Flaga wskazująca, czy power-up jest aktywny (widoczny i do zebrania).
    ALLEGRO_COLOR color;  ///< Kolor power-upa (używany do rysowania).
} Powerup;

/** @var powerups Tablica przechowująca instancje wszystkich power-upów na mapie. */
Powerup powerups[MAX_POWERUPS];

// --- Definicje dla bomb ---
/** @def MAX_BOMBS Maksymalna liczba bomb, które mogą istnieć jednocześnie na planszy (globalny limit systemu). */
#define MAX_BOMBS 5                   
/** @def EXPLOSION_DURATION Czas trwania efektu eksplozji bomby, w klatkach. */
#define EXPLOSION_DURATION 30         
/** @def BOMB_TIMER_DURATION Czas od podłożenia bomby do jej wybuchu, w klatkach. */
#define BOMB_TIMER_DURATION 120       
/** @def MAX_EXPLOSION_CELLS Maksymalna liczba kafelków, które mogą zostać objęte pojedynczą eksplozją. */
#define MAX_EXPLOSION_CELLS (1 + 4 * 7) 

/**
 * @struct Bomb
 * @brief Struktura przechowująca informacje o pojedynczej bombie.
 */
typedef struct {
    int x, y;                     ///< Pozycja bomby na mapie (współrzędne kafelków).
    int timer;                    ///< Licznik czasu pozostałego do wybuchu bomby.
    int radius;                   ///< Promień rażenia eksplozji bomby.
    bool active;                  ///< Flaga wskazująca, czy bomba jest aktywna (tyka lub wybucha).
    bool exploding;               ///< Flaga wskazująca, czy bomba aktualnie wybucha.
    int explosion_timer;          ///< Licznik czasu pozostałego do zakończenia efektu eksplozji.
    int affected_explosion_cells_x[MAX_EXPLOSION_CELLS]; ///< Tablica współrzędnych X kafelków objętych eksplozją.
    int affected_explosion_cells_y[MAX_EXPLOSION_CELLS]; ///< Tablica współrzędnych Y kafelków objętych eksplozją.
    int num_affected_explosion_cells; ///< Liczba kafelków faktycznie objętych daną eksplozją.
} Bomb;

/** @var bombs Tablica przechowująca instancje wszystkich bomb na mapie. */
Bomb bombs[MAX_BOMBS];

/**
 * @struct DestructibleWallCoord
 * @brief Struktura pomocnicza do przechowywania współrzędnych zniszczalnych ścian.
 * Używana podczas losowego umieszczania ukrytego wyjścia.
 */
typedef struct {
    int x; ///< Współrzędna X zniszczalnej ściany.
    int y; ///< Współrzędna Y zniszczalnej ściany.
} DestructibleWallCoord;

// --- Deklaracje funkcji ---

// Funkcje inicjalizacyjne
void initialize_map();
void hide_exit_randomly();
void initialize_enemies(int map[MAP_HEIGHT][MAP_WIDTH], Player* p_player);
void find_and_set_player_spawn(Player* p_player, int map[MAP_HEIGHT][MAP_WIDTH]);
void try_plant_bomb(Player* p);
void setup_new_game();

// Funkcje obsługi logiki gry
void obsluz_wejscie(ALLEGRO_EVENT event, Player* p, GAME_STATE* current_state);
void aktualizuj_gre(Player* p, Bomb bombs_arr[], Enemy enemies_arr[], Powerup powerups_arr[], int game_map_arr[MAP_HEIGHT][MAP_WIDTH], GAME_STATE* current_state, bool* exit_rev, int ex_x, int ex_y);
void aktualizuj_nietykalnosc_gracza(Player* p);
void aktualizuj_bomby(Bomb bombs_arr[], Player* p, Enemy enemies_arr[], Powerup powerups_arr[], int game_map_arr[MAP_HEIGHT][MAP_WIDTH], GAME_STATE* current_state, bool* exit_rev, int ex_x, int ex_y);
void aktualizuj_wrogow(Enemy enemies_arr[], Player* p, Bomb bombs_arr[], int game_map_arr[MAP_HEIGHT][MAP_WIDTH], bool exit_rev, int ex_x, int ex_y);
void sprawdz_kolizje_gracz_wrog(Player* p, Enemy enemies_arr[], GAME_STATE* current_state);
void sprawdz_warunek_wygranej(Player* p, Enemy enemies_arr[], bool exit_rev, int ex_x, int ex_y, GAME_STATE* current_state);

// Funkcje rysowania
void rysuj_gre(ALLEGRO_DISPLAY* display, Player* p, Bomb bombs_arr[], Enemy enemies_arr[], Powerup powerups_arr[], int game_map_arr[MAP_HEIGHT][MAP_WIDTH], GAME_STATE current_s, bool exit_rev, int ex_x, int ex_y);
void rysuj_ekran_startowy(ALLEGRO_DISPLAY* display);
void rysuj_ekran_konca_gry(ALLEGRO_DISPLAY* display, Player* p, Enemy enemies_arr[], bool exit_rev, int ex_x, int ex_y);
void rysuj_hud(Player* p);
void rysuj_mape(int game_map_arr[MAP_HEIGHT][MAP_WIDTH]);
void rysuj_wyjscie(bool exit_rev, int ex_x, int ex_y);
void rysuj_powerupy(Powerup powerups_arr[]);
void rysuj_bomby_i_eksplozje(Bomb bombs_arr[], int game_map_arr[MAP_HEIGHT][MAP_WIDTH]);
void rysuj_wrogow(Enemy enemies_arr[]);
void rysuj_gracza(Player* p);


// --- Implementacje funkcji ---

/**
 * @brief Ukrywa wyjście pod losowo wybraną zniszczalną ścianą na mapie.
 * * Funkcja przeszukuje mapę w poszukiwaniu wszystkich kafelków typu DESTRUCTIBLE_WALL.
 * Następnie, jeśli takie kafelki istnieją, losowo wybiera jeden z nich
 * i ustawia globalne zmienne `exit_x` oraz `exit_y` na jego współrzędne.
 * Flaga `exit_revealed` jest ustawiana na `false`.
 */
void hide_exit_randomly() {
    DestructibleWallCoord possible_exits[MAP_WIDTH * MAP_HEIGHT];
    int num_possible_exits = 0;

    for (int y_coord = 0; y_coord < MAP_HEIGHT; y_coord++) {
        for (int x_coord = 0; x_coord < MAP_WIDTH; x_coord++) {
            if (game_map[y_coord][x_coord] == DESTRUCTIBLE_WALL) {
                if (num_possible_exits < MAP_WIDTH * MAP_HEIGHT) {
                    possible_exits[num_possible_exits].x = x_coord;
                    possible_exits[num_possible_exits].y = y_coord;
                    num_possible_exits++;
                }
            }
        }
    }

    if (num_possible_exits > 0) {
        int random_index = rand() % num_possible_exits;
        exit_x = possible_exits[random_index].x;
        exit_y = possible_exits[random_index].y;
        printf("Exit hidden under a box at (%d, %d)\n", exit_x, exit_y);
    }
    else {
        printf("WARNING: No destructible walls found to hide the exit! Exit will not be placed.\n");
        exit_x = -1;
        exit_y = -1;
    }
    exit_revealed = false;
}

/**
 * @brief Inicjalizuje mapę gry, rozmieszczając na niej ściany (stałe i zniszczalne) oraz puste pola.
 * * Tworzy obramowanie z niezniszczalnych ścian, rozmieszcza niezniszczalne bloki
 * wewnątrz mapy (wzorzec szachownicy) oraz losowo umieszcza zniszczalne ściany
 * i puste pola. Gwarantuje również puste miejsca dla startu gracza.
 */
void initialize_map() {
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (y == 0 || y == MAP_HEIGHT - 1 || x == 0 || x == MAP_WIDTH - 1) {
                game_map[y][x] = SOLID_WALL;
            }
            else if (x % 2 == 0 && y % 2 == 0) {
                game_map[y][x] = SOLID_WALL;
            }
            else if ((x == 1 && y == 1) || (x == 1 && y == 2) || (x == 2 && y == 1) || (x == MAP_WIDTH - 2 && y == 1)) {
                game_map[y][x] = EMPTY;
            }
            else if (rand() % 2 == 0) {
                game_map[y][x] = DESTRUCTIBLE_WALL;
            }
            else {
                game_map[y][x] = EMPTY;
            }
        }
    }
}

/**
 * @brief Inicjalizuje wrogów, rozmieszczając ich na mapie.
 * * Dla każdego wroga losuje pozycję na pustym polu, upewniając się, że nie jest to
 * miejsce startowe gracza, ukryte wyjście, ani pozycja innego, już umieszczonego wroga.
 * Wrogowie nie powinni również pojawiać się zbyt blisko gracza na starcie.
 * @param map Tablica reprezentująca mapę gry.
 * @param p_player Wskaźnik do struktury gracza.
 */
void initialize_enemies(int map[MAP_HEIGHT][MAP_WIDTH], Player* p_player) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].is_alive = true;
        enemies[i].color = al_map_rgb(255, 100, 100);
        enemies[i].move_timer = rand() % ENEMY_MOVE_DELAY;
        enemies[i].direction = (ENEMY_DIRECTION)(rand() % DIR_COUNT);

        bool spot_found = false;
        int attempts = 0;
        while (!spot_found && attempts < MAP_WIDTH * MAP_HEIGHT) {
            int ex = rand() % MAP_WIDTH;
            int ey = rand() % MAP_HEIGHT;

            if (map[ey][ex] == EMPTY && (ex != p_player->x || ey != p_player->y) && (ex != exit_x || ey != exit_y)) {
                bool too_close_to_player = (abs(ex - p_player->x) < 3 && abs(ey - p_player->y) < 3);
                bool occupied_by_other_enemy = false;
                for (int j = 0; j < i; j++) {
                    if (enemies[j].is_alive && enemies[j].x == ex && enemies[j].y == ey) {
                        occupied_by_other_enemy = true;
                        break;
                    }
                }
                if (!occupied_by_other_enemy && !too_close_to_player) {
                    enemies[i].x = ex;
                    enemies[i].y = ey;
                    spot_found = true;
                    printf("Enemy %d spawned at (%d, %d)\n", i, ex, ey);
                }
            }
            attempts++;
        }
        if (!spot_found) {
            enemies[i].is_alive = false;
            printf("Could not find a spot for enemy %d\n", i);
        }
    }
}

/**
 * @brief Znajduje i ustawia bezpieczne miejsce startowe dla gracza na mapie.
 * * Funkcja najpierw próbuje umieścić gracza w jednym z preferowanych rogów mapy,
 * upewniając się, że sąsiednie pola są puste. Jeśli to się nie uda, szuka
 * dowolnego pustego pola z dwoma wolnymi sąsiadami. W ostateczności wybiera
 * dowolne puste pole lub wymusza puste pole na (1,1).
 * @param p_player Wskaźnik do struktury gracza.
 * @param map Tablica reprezentująca mapę gry.
 */
void find_and_set_player_spawn(Player* p_player, int map[MAP_HEIGHT][MAP_WIDTH]) {
    bool found_spawn = false;
    int spawn_candidates_x[] = { 1, 1, MAP_WIDTH - 2, MAP_WIDTH - 2 };
    int spawn_candidates_y[] = { 1, MAP_HEIGHT - 2, 1, MAP_HEIGHT - 2 };

    for (int i = 0; i < 4 && !found_spawn; ++i) {
        int sx = spawn_candidates_x[i];
        int sy = spawn_candidates_y[i];
        if (map[sy][sx] == EMPTY) {
            bool clear_around = true;
            if (sx + 1 < MAP_WIDTH && map[sy][sx + 1] != EMPTY) clear_around = false;
            if (sy + 1 < MAP_HEIGHT && map[sy + 1][sx] != EMPTY) clear_around = false;

            if (clear_around) {
                p_player->x = sx; p_player->y = sy; found_spawn = true;
            }
        }
    }

    if (!found_spawn) {
        for (int y = 0; y < MAP_HEIGHT && !found_spawn; y++) {
            for (int x = 0; x < MAP_WIDTH && !found_spawn; x++) {
                if (map[y][x] == EMPTY) {
                    if (x + 1 < MAP_WIDTH && map[y][x + 1] == EMPTY && y + 1 < MAP_HEIGHT && map[y + 1][x] == EMPTY) {
                        p_player->x = x; p_player->y = y; found_spawn = true;
                    }
                }
            }
        }
    }

    if (!found_spawn) {
        printf("Warning: Ideal spawn point not found. Searching for any empty cell...\n");
        for (int y = 0; y < MAP_HEIGHT && !found_spawn; y++) {
            for (int x = 0; x < MAP_WIDTH && !found_spawn; x++) {
                if (map[y][x] == EMPTY) {
                    p_player->x = x; p_player->y = y; found_spawn = true;
                }
            }
        }
    }

    if (!found_spawn) {
        p_player->x = 1;
        p_player->y = 1;
        map[1][1] = EMPTY;
        if (map[1][2] == SOLID_WALL) map[1][2] = EMPTY;
        if (map[2][1] == SOLID_WALL) map[2][1] = EMPTY;
        printf("CRITICAL: No empty spawn point found. Defaulting to (1,1) and forcing empty.\n");
    }
    map[p_player->y][p_player->x] = EMPTY;
    printf("Player spawned at (%d, %d)\n", p_player->x, p_player->y);
}

/**
 * @brief Umożliwia graczowi podłożenie bomby.
 * * Sprawdza, czy gracz nie przekroczył swojego limitu aktywnych bomb
 * oraz czy na danym polu nie znajduje się już inna bomba. Jeśli warunki są spełnione,
 * nowa bomba jest aktywowana na pozycji gracza.
 * @param p Wskaźnik do struktury gracza.
 */
void try_plant_bomb(Player* p) {
    int active_player_bombs = 0;
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (bombs[i].active) {
            active_player_bombs++;
        }
    }

    if (active_player_bombs >= p->current_max_bombs) {
        printf("Bomb limit reached (%d)!\n", p->current_max_bombs);
        return;
    }

    for (int i = 0; i < MAX_BOMBS; i++) {
        if (!bombs[i].active) {
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
                bombs[i].num_affected_explosion_cells = 0;
                printf("Bomb (radius %d) planted at (%d, %d)!\n", bombs[i].radius, bombs[i].x, bombs[i].y);
                break;
            }
            else {
                printf("Another bomb is already here!\n");
                break;
            }
        }
    }
}

/**
 * @brief Przygotowuje nową grę, resetując stan wszystkich elementów gry.
 * * Wywołuje funkcje inicjalizujące mapę, wyjście, gracza i wrogów.
 * Resetuje również stan bomb i power-upów oraz uruchamia muzykę w tle.
 */
void setup_new_game() {
    initialize_map();

    exit_x = -1;
    exit_y = -1;
    exit_revealed = false;
    hide_exit_randomly();

    find_and_set_player_spawn(&player, game_map);

    player.lives = PLAYER_MAX_LIVES;
    player.score = 0;
    player.is_alive = true;
    player.invincible = false;
    player.invincibility_timer = 0;
    player.current_max_bombs = 1;
    player.current_bomb_radius = 1;
    player.direction = PLAYER_DIR_DOWN;

    initialize_enemies(game_map, &player);

    for (int i = 0; i < MAX_BOMBS; i++) {
        bombs[i].active = false;
        bombs[i].exploding = false;
        bombs[i].explosion_timer = 0;
        bombs[i].num_affected_explosion_cells = 0;
    }

    for (int i = 0; i < MAX_POWERUPS; i++) {
        powerups[i].is_active = false;
    }

    if (background_music_instance) {
        if (al_get_sample_instance_playing(background_music_instance)) {
            al_stop_sample_instance(background_music_instance);
        }
        al_play_sample_instance(background_music_instance);
        printf("Background music started.\n");
    }

    current_game_state = PLAYING;
    printf("New game started!\n");
}

// --- Funkcje obsługi logiki gry ---

/**
 * @brief Obsługuje wejście z klawiatury.
 * * Reaguje na wciśnięcia klawiszy w zależności od aktualnego stanu gry (START_SCREEN, PLAYING, GAME_OVER).
 * Umożliwia ruch gracza, podkładanie bomb oraz nawigację w menu.
 * @param event Zdarzenie Allegro (oczekiwane jest zdarzenie klawiatury).
 * @param p Wskaźnik do struktury gracza.
 * @param current_s Wskaźnik do aktualnego stanu gry.
 */
void obsluz_wejscie(ALLEGRO_EVENT event, Player* p, GAME_STATE* current_s) {
    if (event.type == ALLEGRO_EVENT_KEY_DOWN) {
        if (*current_s == START_SCREEN) {
            if (event.keyboard.keycode == ALLEGRO_KEY_ENTER) {
                setup_new_game();
            }
        }
        else if (*current_s == PLAYING && p->is_alive) {
            int next_x = p->x;
            int next_y = p->y;
            bool moved = false;

            if (event.keyboard.keycode == ALLEGRO_KEY_UP || event.keyboard.keycode == ALLEGRO_KEY_W) {
                next_y--; p->direction = PLAYER_DIR_UP; moved = true;
            }
            else if (event.keyboard.keycode == ALLEGRO_KEY_DOWN || event.keyboard.keycode == ALLEGRO_KEY_S) {
                next_y++; p->direction = PLAYER_DIR_DOWN; moved = true;
            }
            else if (event.keyboard.keycode == ALLEGRO_KEY_LEFT || event.keyboard.keycode == ALLEGRO_KEY_A) {
                next_x--; p->direction = PLAYER_DIR_LEFT; moved = true;
            }
            else if (event.keyboard.keycode == ALLEGRO_KEY_RIGHT || event.keyboard.keycode == ALLEGRO_KEY_D) {
                next_x++; p->direction = PLAYER_DIR_RIGHT; moved = true;
            }
            else if (event.keyboard.keycode == ALLEGRO_KEY_SPACE) {
                try_plant_bomb(p);
            }

            if (moved) {
                if (next_x >= 0 && next_x < MAP_WIDTH &&
                    next_y >= 0 && next_y < MAP_HEIGHT &&
                    game_map[next_y][next_x] != SOLID_WALL &&
                    game_map[next_y][next_x] != DESTRUCTIBLE_WALL) {
                    p->x = next_x;
                    p->y = next_y;

                    for (int i = 0; i < MAX_POWERUPS; i++) {
                        if (powerups[i].is_active && powerups[i].x == p->x && powerups[i].y == p->y) {
                            powerups[i].is_active = false;
                            printf("Player picked up power-up type %d!\n", powerups[i].type);
                            if (powerups[i].type == POWERUP_BOMB_CAP) {
                                if (p->current_max_bombs < MAX_BOMBS) { p->current_max_bombs++; }
                            }
                            else if (powerups[i].type == POWERUP_RADIUS_INC) {
                                if (p->current_bomb_radius < 7) { p->current_bomb_radius++; }
                            }
                            else if (powerups[i].type == POWERUP_EXTRA_LIFE) {
                                if (p->lives < PLAYER_MAX_LIVES) { p->lives++; }
                            }
                            break;
                        }
                    }
                }
            }
        }
        else if (*current_s == GAME_OVER) {
            if (event.keyboard.keycode == ALLEGRO_KEY_ENTER) {
                setup_new_game();
            }
        }
    }
}

/**
 * @brief Aktualizuje stan nietykalności gracza.
 * * Jeśli gracz jest nietykalny, dekrementuje licznik nietykalności.
 * Gdy licznik osiągnie zero, nietykalność jest wyłączana.
 * @param p Wskaźnik do struktury gracza.
 */
void aktualizuj_nietykalnosc_gracza(Player* p) {
    if (p->invincible) {
        p->invincibility_timer--;
        if (p->invincibility_timer <= 0) {
            p->invincible = false;
        }
    }
}

/**
 * @brief Aktualizuje stan wszystkich bomb na mapie oraz obsługuje ich eksplozje.
 * * Dla każdej aktywnej bomby dekrementuje jej timer. Jeśli timer osiągnie zero,
 * bomba wybucha. Funkcja oblicza zasięg eksplozji, niszczy zniszczalne ściany
 * (przyznając punkty i potencjalnie odkrywając wyjście), zadaje obrażenia graczowi
 * i wrogom oraz obsługuje wypadanie power-upów z pokonanych wrogów.
 * @param bombs_arr Tablica bomb.
 * @param p Wskaźnik do struktury gracza.
 * @param enemies_arr Tablica wrogów.
 * @param powerups_arr Tablica power-upów.
 * @param game_map_arr Tablica mapy gry.
 * @param current_s Wskaźnik do aktualnego stanu gry.
 * @param exit_rev Wskaźnik do flagi odkrycia wyjścia.
 * @param ex_x Współrzędna X wyjścia.
 * @param ex_y Współrzędna Y wyjścia.
 */
void aktualizuj_bomby(Bomb bombs_arr[], Player* p, Enemy enemies_arr[], Powerup powerups_arr[], int game_map_arr[MAP_HEIGHT][MAP_WIDTH], GAME_STATE* current_s, bool* exit_rev, int ex_x, int ex_y) {
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (bombs_arr[i].active) {
            if (!bombs_arr[i].exploding) {
                bombs_arr[i].timer--;
                if (bombs_arr[i].timer <= 0) {
                    bombs_arr[i].exploding = true;
                    bombs_arr[i].explosion_timer = EXPLOSION_DURATION;
                    bombs_arr[i].num_affected_explosion_cells = 0;

                    if (game_map_arr[bombs_arr[i].y][bombs_arr[i].x] != SOLID_WALL) {
                        bombs_arr[i].affected_explosion_cells_x[bombs_arr[i].num_affected_explosion_cells] = bombs_arr[i].x;
                        bombs_arr[i].affected_explosion_cells_y[bombs_arr[i].num_affected_explosion_cells] = bombs_arr[i].y;
                        if (game_map_arr[bombs_arr[i].y][bombs_arr[i].x] == DESTRUCTIBLE_WALL) {
                            game_map_arr[bombs_arr[i].y][bombs_arr[i].x] = EMPTY;
                            p->score += POINTS_PER_WALL;
                            if (bombs_arr[i].x == ex_x && bombs_arr[i].y == ex_y) {
                                *exit_rev = true;
                                printf("Exit revealed at (%d, %d)!\n", ex_x, ex_y);
                            }
                        }
                        bombs_arr[i].num_affected_explosion_cells++;
                    }

                    int dx[] = { 0, 0, -1, 1 };
                    int dy[] = { -1, 1, 0, 0 };
                    for (int dir = 0; dir < 4; dir++) {
                        for (int r = 1; r <= bombs_arr[i].radius; r++) {
                            int cur_x = bombs_arr[i].x + dx[dir] * r;
                            int cur_y = bombs_arr[i].y + dy[dir] * r;

                            if (cur_x < 0 || cur_x >= MAP_WIDTH || cur_y < 0 || cur_y >= MAP_HEIGHT) break;

                            if (bombs_arr[i].num_affected_explosion_cells < MAX_EXPLOSION_CELLS) {
                                bombs_arr[i].affected_explosion_cells_x[bombs_arr[i].num_affected_explosion_cells] = cur_x;
                                bombs_arr[i].affected_explosion_cells_y[bombs_arr[i].num_affected_explosion_cells] = cur_y;
                                bombs_arr[i].num_affected_explosion_cells++;
                            }
                            else break;

                            if (game_map_arr[cur_y][cur_x] == SOLID_WALL) break;

                            if (game_map_arr[cur_y][cur_x] == DESTRUCTIBLE_WALL) {
                                game_map_arr[cur_y][cur_x] = EMPTY;
                                p->score += POINTS_PER_WALL;
                                if (cur_x == ex_x && cur_y == ex_y) {
                                    *exit_rev = true;
                                    printf("Exit revealed at (%d, %d)!\n", ex_x, ex_y);
                                }
                                break;
                            }
                        }
                    }

                    bool player_hit_this_explosion = false;
                    for (int k = 0; k < bombs_arr[i].num_affected_explosion_cells; k++) {
                        int ex_coord = bombs_arr[i].affected_explosion_cells_x[k];
                        int ey_coord = bombs_arr[i].affected_explosion_cells_y[k];
                        if (p->is_alive && !p->invincible && !player_hit_this_explosion && p->x == ex_coord && p->y == ey_coord) {
                            p->lives--;
                            player_hit_this_explosion = true;
                            printf("Player hit by explosion! Lives left: %d\n", p->lives);
                            if (p->lives <= 0) {
                                p->is_alive = false; *current_s = GAME_OVER;
                                if (background_music_instance) al_stop_sample_instance(background_music_instance);
                            }
                            else {
                                p->invincible = true; p->invincibility_timer = INVINCIBILITY_DURATION;
                            }
                        }
                        for (int e_idx = 0; e_idx < MAX_ENEMIES; e_idx++) {
                            if (enemies_arr[e_idx].is_alive && enemies_arr[e_idx].x == ex_coord && enemies_arr[e_idx].y == ey_coord) {
                                enemies_arr[e_idx].is_alive = false;
                                p->score += POINTS_PER_ENEMY;
                                printf("Enemy %d at (%d, %d) destroyed by explosion! Player score: %d\n", e_idx, ex_coord, ey_coord, p->score);
                                if (rand() % POWERUP_DROP_CHANCE == 0) {
                                    for (int p_idx = 0; p_idx < MAX_POWERUPS; p_idx++) {
                                        if (!powerups_arr[p_idx].is_active) {
                                            powerups_arr[p_idx].is_active = true;
                                            powerups_arr[p_idx].x = enemies_arr[e_idx].x;
                                            powerups_arr[p_idx].y = enemies_arr[e_idx].y;
                                            powerups_arr[p_idx].type = (POWERUP_TYPE)(rand() % POWERUP_TYPE_COUNT);
                                            if (powerups_arr[p_idx].type == POWERUP_BOMB_CAP) powerups_arr[p_idx].color = al_map_rgb(0, 0, 255);
                                            else if (powerups_arr[p_idx].type == POWERUP_RADIUS_INC) powerups_arr[p_idx].color = al_map_rgb(255, 165, 0);
                                            else if (powerups_arr[p_idx].type == POWERUP_EXTRA_LIFE) powerups_arr[p_idx].color = al_map_rgb(255, 20, 147);
                                            printf("Enemy dropped power-up type %d at (%d,%d)!\n", powerups_arr[p_idx].type, powerups_arr[p_idx].x, powerups_arr[p_idx].y);
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
                bombs_arr[i].explosion_timer--;
                if (bombs_arr[i].explosion_timer <= 0) {
                    bombs_arr[i].active = false;
                    bombs_arr[i].exploding = false;
                }
            }
        }
    }
}

/**
 * @brief Aktualizuje stan wrogów, zarządzając ich ruchem i zmianą kierunku.
 * * Dla każdego żywego wroga dekrementuje licznik ruchu. Gdy licznik osiągnie zero,
 * wróg próbuje się poruszyć w aktualnym kierunku. Jeśli ruch jest zablokowany
 * (przez ścianę, bombę, innego wroga lub odkryte wyjście), wróg próbuje zmienić kierunek.
 * @param enemies_arr Tablica wrogów.
 * @param p Wskaźnik do struktury gracza (nieużywany bezpośrednio w tej funkcji, ale może być potrzebny w przyszłości).
 * @param bombs_arr Tablica bomb (do sprawdzania kolizji).
 * @param game_map_arr Tablica mapy gry.
 * @param exit_rev Flaga odkrycia wyjścia.
 * @param ex_x Współrzędna X wyjścia.
 * @param ex_y Współrzędna Y wyjścia.
 */
void aktualizuj_wrogow(Enemy enemies_arr[], Player* p, Bomb bombs_arr[], int game_map_arr[MAP_HEIGHT][MAP_WIDTH], bool exit_rev, int ex_x, int ex_y) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies_arr[i].is_alive) {
            enemies_arr[i].move_timer--;
            if (enemies_arr[i].move_timer <= 0) {
                enemies_arr[i].move_timer = ENEMY_MOVE_DELAY + (rand() % (ENEMY_MOVE_DELAY / 2));

                int next_ex = enemies_arr[i].x;
                int next_ey = enemies_arr[i].y;
                ENEMY_DIRECTION original_direction = enemies_arr[i].direction;
                int attempts_to_move = 0;
                bool moved_this_turn = false;

                while (attempts_to_move < DIR_COUNT * 2 && !moved_this_turn) {
                    next_ex = enemies_arr[i].x;
                    next_ey = enemies_arr[i].y;

                    if (attempts_to_move > 0 && attempts_to_move % DIR_COUNT == 0) {
                        enemies_arr[i].direction = (ENEMY_DIRECTION)(rand() % DIR_COUNT);
                    }

                    if (enemies_arr[i].direction == DIR_UP) next_ey--;
                    else if (enemies_arr[i].direction == DIR_DOWN) next_ey++;
                    else if (enemies_arr[i].direction == DIR_LEFT) next_ex--;
                    else if (enemies_arr[i].direction == DIR_RIGHT) next_ex++;

                    bool can_move = true;
                    if (next_ex <= 0 || next_ex >= MAP_WIDTH - 1 || next_ey <= 0 || next_ey >= MAP_HEIGHT - 1 ||
                        game_map_arr[next_ey][next_ex] == SOLID_WALL ||
                        game_map_arr[next_ey][next_ex] == DESTRUCTIBLE_WALL) {
                        can_move = false;
                    }
                    for (int b = 0; b < MAX_BOMBS; b++) {
                        if (bombs_arr[b].active && bombs_arr[b].x == next_ex && bombs_arr[b].y == next_ey) {
                            can_move = false; break;
                        }
                    }
                    for (int other_enemy_idx = 0; other_enemy_idx < MAX_ENEMIES; other_enemy_idx++) {
                        if (i == other_enemy_idx) continue;
                        if (enemies_arr[other_enemy_idx].is_alive && enemies_arr[other_enemy_idx].x == next_ex && enemies_arr[other_enemy_idx].y == next_ey) {
                            can_move = false; break;
                        }
                    }
                    if (exit_rev && next_ex == ex_x && next_ey == ex_y) {
                        can_move = false;
                    }

                    if (can_move) {
                        enemies_arr[i].x = next_ex;
                        enemies_arr[i].y = next_ey;
                        moved_this_turn = true;
                    }
                    else {
                        if (attempts_to_move < DIR_COUNT) {
                            enemies_arr[i].direction = (ENEMY_DIRECTION)((original_direction + attempts_to_move + 1) % DIR_COUNT);
                        }
                        else {
                            enemies_arr[i].direction = (ENEMY_DIRECTION)(rand() % DIR_COUNT);
                        }
                        attempts_to_move++;
                    }
                }
                if (!moved_this_turn) {
                    enemies_arr[i].direction = original_direction;
                }
            }
        }
    }
}

/**
 * @brief Sprawdza kolizję gracza z wrogami.
 * * Jeśli gracz nie jest nietykalny i wejdzie na pole zajmowane przez żywego wroga,
 * traci życie. Jeśli liczba żyć spadnie do zera, gra się kończy.
 * Po kolizji gracz staje się na chwilę nietykalny.
 * @param p Wskaźnik do struktury gracza.
 * @param enemies_arr Tablica wrogów.
 * @param current_s Wskaźnik do aktualnego stanu gry.
 */
void sprawdz_kolizje_gracz_wrog(Player* p, Enemy enemies_arr[], GAME_STATE* current_s) {
    if (p->is_alive && !p->invincible) {
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (enemies_arr[i].is_alive && p->x == enemies_arr[i].x && p->y == enemies_arr[i].y) {
                p->lives--;
                printf("Player collided with enemy! Lives left: %d\n", p->lives);
                if (p->lives <= 0) {
                    p->is_alive = false; *current_s = GAME_OVER;
                    if (background_music_instance) al_stop_sample_instance(background_music_instance);
                }
                else {
                    p->invincible = true; p->invincibility_timer = INVINCIBILITY_DURATION;
                }
                break;
            }
        }
    }
}

/**
 * @brief Sprawdza, czy warunki zwycięstwa zostały spełnione.
 * * Warunki zwycięstwa: gracz żyje, wszyscy wrogowie są pokonani, wyjście jest odkryte,
 * a gracz znajduje się na polu wyjścia.
 * @param p Wskaźnik do struktury gracza.
 * @param enemies_arr Tablica wrogów.
 * @param exit_rev Flaga odkrycia wyjścia.
 * @param ex_x Współrzędna X wyjścia.
 * @param ex_y Współrzędna Y wyjścia.
 * @param current_s Wskaźnik do aktualnego stanu gry.
 */
void sprawdz_warunek_wygranej(Player* p, Enemy enemies_arr[], bool exit_rev, int ex_x, int ex_y, GAME_STATE* current_s) {
    if (p->is_alive && *current_s == PLAYING) {
        bool all_enemies_defeated_now = true;
        for (int k = 0; k < MAX_ENEMIES; k++) {
            if (enemies_arr[k].is_alive) {
                all_enemies_defeated_now = false;
                break;
            }
        }
        if (all_enemies_defeated_now && exit_rev && p->x == ex_x && p->y == ex_y) {
            printf("CONGRATULATIONS! LEVEL COMPLETED!\n");
            *current_s = GAME_OVER;
            if (background_music_instance) al_stop_sample_instance(background_music_instance);
        }
    }
}

/**
 * @brief Główna funkcja aktualizująca logikę gry.
 * * Wywoływana w każdej klatce gry (jeśli stan gry to PLAYING).
 * Odpowiada za aktualizację stanu nietykalności gracza, bomb, wrogów
 * oraz sprawdzanie kolizji i warunków zwycięstwa.
 * @param p Wskaźnik do struktury gracza.
 * @param bombs_arr Tablica bomb.
 * @param enemies_arr Tablica wrogów.
 * @param powerups_arr Tablica power-upów.
 * @param game_map_arr Tablica mapy gry.
 * @param current_s Wskaźnik do aktualnego stanu gry.
 * @param exit_rev Wskaźnik do flagi odkrycia wyjścia.
 * @param ex_x Współrzędna X wyjścia.
 * @param ex_y Współrzędna Y wyjścia.
 */
void aktualizuj_gre(Player* p, Bomb bombs_arr[], Enemy enemies_arr[], Powerup powerups_arr[], int game_map_arr[MAP_HEIGHT][MAP_WIDTH], GAME_STATE* current_s, bool* exit_rev, int ex_x, int ex_y) {
    if (*current_s == PLAYING) {
        aktualizuj_nietykalnosc_gracza(p);
        aktualizuj_bomby(bombs_arr, p, enemies_arr, powerups_arr, game_map_arr, current_s, exit_rev, ex_x, ex_y);
        aktualizuj_wrogow(enemies_arr, p, bombs_arr, game_map_arr, *exit_rev, ex_x, ex_y);
        sprawdz_kolizje_gracz_wrog(p, enemies_arr, current_s);
        sprawdz_warunek_wygranej(p, enemies_arr, *exit_rev, ex_x, ex_y, current_s);
    }
}


// --- Funkcje rysowania ---

/**
 * @brief Rysuje ekran startowy gry.
 * @param display Wskaźnik do ekranu Allegro.
 */
void rysuj_ekran_startowy(ALLEGRO_DISPLAY* display) {
    if (font_main) {
        float display_w = al_get_display_width(display);
        float display_h = al_get_display_height(display);
        al_draw_text(font_main, al_map_rgb(255, 255, 0), display_w / 2, display_h / 4, ALLEGRO_ALIGN_CENTER, "Bomberman");
        al_draw_text(font_main, al_map_rgb(200, 200, 200), display_w / 2, display_h / 2, ALLEGRO_ALIGN_CENTER, "Press ENTER to start");
        al_draw_text(font_main, al_map_rgb(150, 150, 150), display_w / 2, display_h / 2 + al_get_font_line_height(font_main) * 1.5f, ALLEGRO_ALIGN_CENTER, "ESC to exit");
    }
}

/**
 * @brief Rysuje ekran końca gry (informację o wygranej lub przegranej oraz wynik).
 * @param display Wskaźnik do ekranu Allegro.
 * @param p Wskaźnik do struktury gracza.
 * @param enemies_arr Tablica wrogów (do sprawdzenia warunku wygranej).
 * @param exit_rev Flaga odkrycia wyjścia.
 * @param ex_x Współrzędna X wyjścia.
 * @param ex_y Współrzędna Y wyjścia.
 */
void rysuj_ekran_konca_gry(ALLEGRO_DISPLAY* display, Player* p, Enemy enemies_arr[], bool exit_rev, int ex_x, int ex_y) {
    if (font_main) {
        float display_w = al_get_display_width(display);
        float display_h = al_get_display_height(display);
        float game_area_h = display_h - HUD_HEIGHT;
        float center_y_game_area = HUD_HEIGHT + game_area_h / 2;
        char score_text[50];

        bool all_enemies_defeated_final_check = true;
        for (int k = 0; k < MAX_ENEMIES; k++) {
            if (enemies_arr[k].is_alive) {
                all_enemies_defeated_final_check = false; break;
            }
        }
        al_draw_filled_rectangle(0, HUD_HEIGHT, display_w, display_h, al_map_rgba(0, 0, 0, 150));

        snprintf(score_text, sizeof(score_text), "Score: %d", p->score);
        float score_y_offset = al_get_font_line_height(font_main) * 1.5f;


        if (p->is_alive && all_enemies_defeated_final_check && exit_rev && p->x == ex_x && p->y == ex_y) {
            al_draw_text(font_main, al_map_rgb(0, 255, 0),
                display_w / 2, center_y_game_area - (al_get_font_line_height(font_main) * 2),
                ALLEGRO_ALIGN_CENTER, "VICTORY!");
            al_draw_text(font_main, al_map_rgb(255, 255, 0), display_w / 2, center_y_game_area - score_y_offset + al_get_font_line_height(font_main), ALLEGRO_ALIGN_CENTER, score_text);
        }
        else {
            al_draw_text(font_main, al_map_rgb(255, 0, 0),
                display_w / 2, center_y_game_area - (al_get_font_line_height(font_main) * 2),
                ALLEGRO_ALIGN_CENTER, "GAME OVER");
            al_draw_text(font_main, al_map_rgb(255, 255, 255), display_w / 2, center_y_game_area - score_y_offset + al_get_font_line_height(font_main), ALLEGRO_ALIGN_CENTER, score_text);
        }
        al_draw_text(font_main, al_map_rgb(200, 200, 200),
            display_w / 2, center_y_game_area + (al_get_font_line_height(font_main) * 1.5f),
            ALLEGRO_ALIGN_CENTER, "Press ENTER to restart");
    }
}

/**
 * @brief Rysuje interfejs użytkownika (HUD) na górze ekranu.
 * Wyświetla informacje takie jak liczba żyć, wynik, liczba pozostałych wrogów,
 * aktualna liczba bomb i moc eksplozji.
 * @param p Wskaźnik do struktury gracza.
 */
void rysuj_hud(Player* p) {
    if (font_main) {
        char text_buffer[50];

        snprintf(text_buffer, sizeof(text_buffer), "Lives: %d", p->lives);
        al_draw_text(font_main, al_map_rgb(255, 255, 255), 10, 5, ALLEGRO_ALIGN_LEFT, text_buffer);

        snprintf(text_buffer, sizeof(text_buffer), "Score: %d", p->score);
        al_draw_text(font_main, al_map_rgb(255, 255, 255), al_get_display_width(al_get_current_display()) / 2, 5, ALLEGRO_ALIGN_CENTER, text_buffer);

        int active_enemies_count = 0;
        for (int i = 0; i < MAX_ENEMIES; ++i) if (enemies[i].is_alive) active_enemies_count++;
        snprintf(text_buffer, sizeof(text_buffer), "Enemies: %d", active_enemies_count);
        al_draw_text(font_main, al_map_rgb(255, 255, 255), al_get_display_width(al_get_current_display()) - 10, 5, ALLEGRO_ALIGN_RIGHT, text_buffer);

        float second_line_y = 5 + al_get_font_line_height(font_main) + 2;

        snprintf(text_buffer, sizeof(text_buffer), "Bombs: %d", p->current_max_bombs);
        al_draw_text(font_main, al_map_rgb(255, 255, 255), 10, second_line_y, ALLEGRO_ALIGN_LEFT, text_buffer);

        snprintf(text_buffer, sizeof(text_buffer), "Power: %d", p->current_bomb_radius);
        al_draw_text(font_main, al_map_rgb(255, 255, 255), al_get_display_width(al_get_current_display()) - 10, second_line_y, ALLEGRO_ALIGN_RIGHT, text_buffer);
    }
}

/**
 * @brief Rysuje kafelki mapy gry (ściany, puste pola).
 * @param game_map_arr Tablica mapy gry.
 */
void rysuj_mape(int game_map_arr[MAP_HEIGHT][MAP_WIDTH]) {
    for (int y_map = 0; y_map < MAP_HEIGHT; y_map++) {
        for (int x_map = 0; x_map < MAP_WIDTH; x_map++) {
            int tile_x_pos = x_map * TILE_SIZE;
            int tile_y_pos = y_map * TILE_SIZE + HUD_HEIGHT;

            if (game_map_arr[y_map][x_map] == SOLID_WALL) {
                al_draw_filled_rectangle(tile_x_pos, tile_y_pos, tile_x_pos + TILE_SIZE, tile_y_pos + TILE_SIZE, al_map_rgb(80, 80, 80));
            }
            else if (game_map_arr[y_map][x_map] == DESTRUCTIBLE_WALL) {
                if (destructible_wall_sprite) {
                    al_draw_bitmap(destructible_wall_sprite, tile_x_pos, tile_y_pos, 0);
                }
                else {
                    al_draw_filled_rectangle(tile_x_pos, tile_y_pos, tile_x_pos + TILE_SIZE, tile_y_pos + TILE_SIZE, al_map_rgb(150, 75, 0));
                }
            }
            else {
                al_draw_filled_rectangle(tile_x_pos, tile_y_pos, tile_x_pos + TILE_SIZE, tile_y_pos + TILE_SIZE, al_map_rgb(0, 0, 0));
            }
        }
    }
}

/**
 * @brief Rysuje wyjście z poziomu, jeśli zostało odkryte.
 * * Jeśli dostępny jest sprite wyjścia (`exit_sprite`), używa go.
 * W przeciwnym razie rysuje domyślny, pulsujący efekt portalu.
 * @param exit_rev Flaga wskazująca, czy wyjście jest odkryte.
 * @param ex_x Współrzędna X wyjścia.
 * @param ex_y Współrzędna Y wyjścia.
 */
void rysuj_wyjscie(bool exit_rev, int ex_x, int ex_y) {
    if (exit_rev) {
        if (exit_sprite) {
            al_draw_bitmap(exit_sprite, ex_x * TILE_SIZE, ex_y * TILE_SIZE + HUD_HEIGHT, 0);
        }
        else {
            float center_x = ex_x * TILE_SIZE + TILE_SIZE / 2.0f;
            float center_y = ex_y * TILE_SIZE + HUD_HEIGHT + TILE_SIZE / 2.0f;

            al_draw_filled_rectangle(ex_x * TILE_SIZE, ex_y * TILE_SIZE + HUD_HEIGHT,
                ex_x * TILE_SIZE + TILE_SIZE, ex_y * TILE_SIZE + HUD_HEIGHT + TILE_SIZE,
                al_map_rgb(30, 0, 50));

            double time_now = al_get_time();
            float pulse_factor = (sin(time_now * 5.0) + 1.0) / 2.0;

            float outer_radius = TILE_SIZE * 0.4f * (0.8f + pulse_factor * 0.2f);
            unsigned char r_outer = 100 + (unsigned char)(pulse_factor * 50);
            unsigned char g_outer = 50 + (unsigned char)(pulse_factor * 50);
            al_draw_filled_circle(center_x, center_y, outer_radius, al_map_rgb(r_outer, g_outer, 200));

            float inner_radius = TILE_SIZE * 0.25f * (0.7f + pulse_factor * 0.3f);
            unsigned char r_inner = 200 + (unsigned char)(pulse_factor * 55);
            unsigned char g_inner = 180 + (unsigned char)(pulse_factor * 75);
            al_draw_filled_circle(center_x, center_y, inner_radius, al_map_rgb(r_inner, g_inner, 255));

            if (((int)(time_now * 10)) % 2 == 0) {
                al_draw_filled_circle(center_x, center_y, TILE_SIZE * 0.05f, al_map_rgb(255, 255, 255));
            }
        }
    }
}


/**
 * @brief Rysuje aktywne power-upy na mapie.
 * @param powerups_arr Tablica power-upów.
 */
void rysuj_powerupy(Powerup powerups_arr[]) {
    for (int i = 0; i < MAX_POWERUPS; i++) {
        if (powerups_arr[i].is_active) {
            al_draw_filled_rectangle(powerups_arr[i].x * TILE_SIZE + TILE_SIZE / 4,
                powerups_arr[i].y * TILE_SIZE + TILE_SIZE / 4 + HUD_HEIGHT,
                powerups_arr[i].x * TILE_SIZE + (TILE_SIZE * 3) / 4,
                powerups_arr[i].y * TILE_SIZE + (TILE_SIZE * 3) / 4 + HUD_HEIGHT,
                powerups_arr[i].color);
            al_draw_rectangle(powerups_arr[i].x * TILE_SIZE + TILE_SIZE / 4,
                powerups_arr[i].y * TILE_SIZE + TILE_SIZE / 4 + HUD_HEIGHT,
                powerups_arr[i].x * TILE_SIZE + (TILE_SIZE * 3) / 4,
                powerups_arr[i].y * TILE_SIZE + (TILE_SIZE * 3) / 4 + HUD_HEIGHT,
                al_map_rgb(255, 255, 255), 2);
        }
    }
}

/**
 * @brief Rysuje bomby (tykające) oraz efekty ich eksplozji.
 * @param bombs_arr Tablica bomb.
 * @param game_map_arr Tablica mapy gry (do sprawdzania, czy nie rysować eksplozji na ścianach).
 */
void rysuj_bomby_i_eksplozje(Bomb bombs_arr[], int game_map_arr[MAP_HEIGHT][MAP_WIDTH]) {
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (bombs_arr[i].active) {
            if (bombs_arr[i].exploding) {
                if (sparks_sprite) {
                    for (int k = 0; k < bombs_arr[i].num_affected_explosion_cells; k++) {
                        int ex_coord = bombs_arr[i].affected_explosion_cells_x[k];
                        int ey_coord = bombs_arr[i].affected_explosion_cells_y[k];
                        if (game_map_arr[ey_coord][ex_coord] != SOLID_WALL) {
                            al_draw_tinted_scaled_rotated_bitmap_region(
                                sparks_sprite,
                                0, 0, al_get_bitmap_width(sparks_sprite), al_get_bitmap_height(sparks_sprite),
                                al_map_rgba(255, 255, 255, 200 - (EXPLOSION_DURATION - bombs_arr[i].explosion_timer) * (200 / (EXPLOSION_DURATION + 1))),
                                al_get_bitmap_width(sparks_sprite) / 2, al_get_bitmap_height(sparks_sprite) / 2,
                                ex_coord * TILE_SIZE + TILE_SIZE / 2,
                                ey_coord * TILE_SIZE + HUD_HEIGHT + TILE_SIZE / 2,
                                (float)TILE_SIZE / al_get_bitmap_width(sparks_sprite),
                                (float)TILE_SIZE / al_get_bitmap_height(sparks_sprite),
                                (float)(rand() % 360) * ALLEGRO_PI / 180.0f,
                                0
                            );
                        }
                    }
                }
                else { /* Fallback rysowania eksplozji */ }
            }
            else {
                if (dynamite_sprite) {
                    float scale = 1.0f;
                    if (bombs_arr[i].timer < 45) {
                        scale = 1.0f + (((BOMB_TIMER_DURATION - bombs_arr[i].timer) % 12 < 6) ? 0.1f * sinf((BOMB_TIMER_DURATION - bombs_arr[i].timer) * 0.5f) : -0.1f * sinf((BOMB_TIMER_DURATION - bombs_arr[i].timer) * 0.5f));
                    }
                    al_draw_scaled_bitmap(dynamite_sprite,
                        0, 0, al_get_bitmap_width(dynamite_sprite), al_get_bitmap_height(dynamite_sprite),
                        bombs_arr[i].x * TILE_SIZE + TILE_SIZE / 2.0f * (1.0f - scale),
                        bombs_arr[i].y * TILE_SIZE + HUD_HEIGHT + TILE_SIZE / 2.0f * (1.0f - scale),
                        TILE_SIZE * scale, TILE_SIZE * scale, 0);

                    if (bombs_arr[i].timer > 0) {
                        float fuse_length_factor = (float)bombs_arr[i].timer / BOMB_TIMER_DURATION;
                        float fuse_x_start = bombs_arr[i].x * TILE_SIZE + TILE_SIZE * 0.7f;
                        float fuse_y_start = bombs_arr[i].y * TILE_SIZE + HUD_HEIGHT + TILE_SIZE * 0.2f;
                        float fuse_x_end = fuse_x_start + (TILE_SIZE / 6.0f) * fuse_length_factor;
                        float fuse_y_end = fuse_y_start - (TILE_SIZE / 12.0f) * (1.0f - fuse_length_factor);
                        al_draw_line(fuse_x_start, fuse_y_start, fuse_x_end, fuse_y_end, al_map_rgb(60, 60, 60), 3.0f);
                        if ((bombs_arr[i].timer / 6) % 2 == 0) {
                            al_draw_filled_circle(fuse_x_end, fuse_y_end, TILE_SIZE / 9.0f, al_map_rgb(255, (rand() % 100) + 100, 0));
                        }
                    }
                }
                else { /* Fallback rysowania bomby */ }
            }
        }
    }
}

/**
 * @brief Rysuje wrogów na mapie.
 * @param enemies_arr Tablica wrogów.
 */
void rysuj_wrogow(Enemy enemies_arr[]) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies_arr[i].is_alive) {
            al_draw_filled_rectangle(enemies_arr[i].x * TILE_SIZE + TILE_SIZE * 0.1f,
                enemies_arr[i].y * TILE_SIZE + HUD_HEIGHT + TILE_SIZE * 0.1f,
                enemies_arr[i].x * TILE_SIZE + TILE_SIZE * 0.9f,
                enemies_arr[i].y * TILE_SIZE + TILE_SIZE + HUD_HEIGHT - TILE_SIZE * 0.1f,
                enemies_arr[i].color);

            float eye_base_x_l = enemies_arr[i].x * TILE_SIZE + TILE_SIZE * 0.3f;
            float eye_base_x_r = enemies_arr[i].x * TILE_SIZE + TILE_SIZE * 0.7f;
            float eye_base_y = enemies_arr[i].y * TILE_SIZE + HUD_HEIGHT + TILE_SIZE * 0.35f;
            float pupil_offset_x = 0;
            float pupil_offset_y = 0;
            float eye_radius_outer = TILE_SIZE * 0.12f;
            float eye_radius_inner = TILE_SIZE * 0.07f;

            switch (enemies_arr[i].direction) {
            case DIR_UP: pupil_offset_y = -TILE_SIZE * 0.035f; break;
            case DIR_DOWN: pupil_offset_y = TILE_SIZE * 0.035f; break;
            case DIR_LEFT: pupil_offset_x = -TILE_SIZE * 0.035f; break;
            case DIR_RIGHT: pupil_offset_x = TILE_SIZE * 0.035f; break;
            default: break;
            }
            al_draw_filled_circle(eye_base_x_l, eye_base_y, eye_radius_outer, al_map_rgb(255, 255, 255));
            al_draw_filled_circle(eye_base_x_r, eye_base_y, eye_radius_outer, al_map_rgb(255, 255, 255));
            al_draw_filled_circle(eye_base_x_l + pupil_offset_x, eye_base_y + pupil_offset_y, eye_radius_inner, al_map_rgb(10, 10, 10));
            al_draw_filled_circle(eye_base_x_r + pupil_offset_x, eye_base_y + pupil_offset_y, eye_radius_inner, al_map_rgb(10, 10, 10));
        }
    }
}

/**
 * @brief Rysuje gracza na mapie.
 * Wybiera odpowiedni sprite w zależności od kierunku, w którym gracz jest zwrócony.
 * Obsługuje również efekt migotania podczas nietykalności.
 * @param p Wskaźnik do struktury gracza.
 */
void rysuj_gracza(Player* p) {
    if (p->is_alive) {
        ALLEGRO_BITMAP* sprite_to_draw = player_sprite_front;
        switch (p->direction) {
        case PLAYER_DIR_UP:    sprite_to_draw = player_sprite_back;  break;
        case PLAYER_DIR_DOWN:  sprite_to_draw = player_sprite_front; break;
        case PLAYER_DIR_LEFT:  sprite_to_draw = player_sprite_left;  break;
        case PLAYER_DIR_RIGHT: sprite_to_draw = player_sprite_right; break;
        }

        if (sprite_to_draw) {
            if (p->invincible) {
                if ((p->invincibility_timer / 4) % 2 == 0) {
                    al_draw_bitmap(sprite_to_draw, p->x * TILE_SIZE, p->y * TILE_SIZE + HUD_HEIGHT, 0);
                }
            }
            else {
                al_draw_bitmap(sprite_to_draw, p->x * TILE_SIZE, p->y * TILE_SIZE + HUD_HEIGHT, 0);
            }
        }
        else { /* Fallback, jeśli sprite gracza nie jest załadowany */ }
    }
}

/**
 * @brief Główna funkcja rysująca całą grę.
 * * W zależności od aktualnego stanu gry, wywołuje odpowiednie funkcje rysujące
 * poszczególne elementy (ekran startowy, HUD, mapę, obiekty, gracza, ekran końca gry).
 * Na końcu odświeża ekran.
 * @param display Wskaźnik do ekranu Allegro.
 * @param p Wskaźnik do struktury gracza.
 * @param bombs_arr Tablica bomb.
 * @param enemies_arr Tablica wrogów.
 * @param powerups_arr Tablica power-upów.
 * @param game_map_arr Tablica mapy gry.
 * @param current_s Aktualny stan gry.
 * @param exit_rev Flaga odkrycia wyjścia.
 * @param ex_x Współrzędna X wyjścia.
 * @param ex_y Współrzędna Y wyjścia.
 */
void rysuj_gre(ALLEGRO_DISPLAY* display, Player* p, Bomb bombs_arr[], Enemy enemies_arr[], Powerup powerups_arr[], int game_map_arr[MAP_HEIGHT][MAP_WIDTH], GAME_STATE current_s, bool exit_rev, int ex_x, int ex_y) {
    al_clear_to_color(al_map_rgb(0, 0, 0));

    if (current_s == START_SCREEN) {
        rysuj_ekran_startowy(display);
    }
    else if (current_s == PLAYING || current_s == GAME_OVER) {
        rysuj_hud(p);
        rysuj_mape(game_map_arr);
        rysuj_wyjscie(exit_rev, ex_x, ex_y);
        rysuj_powerupy(powerups_arr);
        rysuj_bomby_i_eksplozje(bombs_arr, game_map_arr);
        rysuj_wrogow(enemies_arr);
        rysuj_gracza(p);

        if (current_s == GAME_OVER) {
            rysuj_ekran_konca_gry(display, p, enemies_arr, exit_rev, ex_x, ex_y);
        }
    }
    al_flip_display();
}


/**
 * @brief Główna funkcja programu.
 * * Odpowiada za inicjalizację biblioteki Allegro i jej dodatków, ładowanie zasobów,
 * tworzenie okna, timera i kolejki zdarzeń. Zawiera główną pętlę gry, która obsługuje
 * zdarzenia, aktualizuje logikę gry i rysuje klatki. Na końcu zwalnia wszystkie
 * zaalokowane zasoby.
 * @return Zwraca 0 w przypadku pomyślnego zakończenia, lub wartość ujemną w przypadku błędu.
 */
int main() {
    ALLEGRO_DISPLAY* display = NULL;
    ALLEGRO_EVENT_QUEUE* event_queue = NULL;
    ALLEGRO_TIMER* timer = NULL;
    bool keyboard_installed = false;
    bool audio_installed = false;
    int ret_val = 0;

    font_main = NULL;
    player_sprite_front = NULL; player_sprite_back = NULL; player_sprite_left = NULL; player_sprite_right = NULL;
    destructible_wall_sprite = NULL; dynamite_sprite = NULL; sparks_sprite = NULL; exit_sprite = NULL;
    background_music_sample = NULL; background_music_instance = NULL;


    if (!al_init()) {
        fprintf(stderr, "Failed to initialize Allegro!\n");
        return -1;
    }

    if (!al_install_keyboard()) {
        fprintf(stderr, "Failed to install keyboard...\n");
        ret_val = -1;
        goto cleanup;
    }
    keyboard_installed = true;

    if (!al_init_primitives_addon()) { fprintf(stderr, "Failed to initialize primitives addon!\n"); ret_val = -1; goto cleanup; }
    if (!al_init_image_addon()) { fprintf(stderr, "Failed to initialize image addon!\n"); ret_val = -1; goto cleanup; }
    if (!al_init_font_addon()) { fprintf(stderr, "Failed to initialize font addon!\n"); ret_val = -1; goto cleanup; }
    if (!al_init_ttf_addon()) { fprintf(stderr, "Failed to initialize TTF addon!\n"); ret_val = -1; goto cleanup; }

    if (!al_install_audio()) {
        fprintf(stderr, "Failed to initialize audio!\n");
        ret_val = -1;
        goto cleanup;
    }
    audio_installed = true;

    if (!al_init_acodec_addon()) {
        fprintf(stderr, "Failed to initialize audio codecs! (OGG support might be missing)\n");
    }

    if (!al_reserve_samples(1)) {
        fprintf(stderr, "Failed to reserve samples!\n");
    }


    font_main = al_load_ttf_font("arial.ttf", 18, 0);
    if (!font_main) {
        fprintf(stderr, "Failed to load font! (arial.ttf)\n");
    }

    display = al_create_display(MAP_WIDTH * TILE_SIZE, (MAP_HEIGHT * TILE_SIZE) + HUD_HEIGHT);
    if (!display) {
        fprintf(stderr, "Failed to create display!\n");
        ret_val = -1;
        goto cleanup;
    }
    al_set_window_title(display, "Bomberman");

    player_sprite_front = al_load_bitmap("player-front.png");
    if (!player_sprite_front) { fprintf(stderr, "Failed to load player-front.png!\n"); ret_val = -1; goto cleanup; }
    player_sprite_back = al_load_bitmap("player-back.png");
    if (!player_sprite_back) { fprintf(stderr, "Failed to load player-back.png!\n"); ret_val = -1; goto cleanup; }
    player_sprite_left = al_load_bitmap("player-left.png");
    if (!player_sprite_left) { fprintf(stderr, "Failed to load player-left.png!\n"); ret_val = -1; goto cleanup; }
    player_sprite_right = al_load_bitmap("player-right.png");
    if (!player_sprite_right) { fprintf(stderr, "Failed to load player-right.png!\n"); ret_val = -1; goto cleanup; }

    destructible_wall_sprite = al_load_bitmap("box.png");
    if (!destructible_wall_sprite) { fprintf(stderr, "Failed to load box.png!\n"); ret_val = -1; goto cleanup; }

    dynamite_sprite = al_load_bitmap("dynamite.png");
    if (!dynamite_sprite) { fprintf(stderr, "Failed to load dynamite.png!\n"); ret_val = -1; goto cleanup; }

    sparks_sprite = al_load_bitmap("sparks.png");
    if (!sparks_sprite) { fprintf(stderr, "Failed to load sparks.png!\n"); ret_val = -1; goto cleanup; }

    exit_sprite = al_load_bitmap("exit.png");
    if (!exit_sprite) {
        fprintf(stderr, "Failed to load exit.png! Using default exit drawing.\n");
    }

    background_music_sample = al_load_sample("Background_Music.ogg");
    if (!background_music_sample) {
        fprintf(stderr, "Failed to load Background_Music.ogg!\n");
    }
    else {
        background_music_instance = al_create_sample_instance(background_music_sample);
        if (!background_music_instance) {
            fprintf(stderr, "Failed to create background music instance!\n");
            al_destroy_sample(background_music_sample);
            background_music_sample = NULL;
        }
        else {
            al_set_sample_instance_playmode(background_music_instance, ALLEGRO_PLAYMODE_LOOP);
            al_attach_sample_instance_to_mixer(background_music_instance, al_get_default_mixer());
            al_set_sample_instance_gain(background_music_instance, 0.05f);
        }
    }


    event_queue = al_create_event_queue();
    if (!event_queue) {
        fprintf(stderr, "Failed to create event queue.\n");
        ret_val = -1;
        goto cleanup;
    }
    al_register_event_source(event_queue, al_get_display_event_source(display));
    al_register_event_source(event_queue, al_get_keyboard_event_source());

    srand(time(NULL));

    timer = al_create_timer(1.0 / 60.0);
    if (!timer) {
        fprintf(stderr, "Failed to create timer.\n");
        ret_val = -1;
        goto cleanup;
    }
    al_register_event_source(event_queue, al_get_timer_event_source(timer));
    al_start_timer(timer);

    bool done = false;
    // Główna pętla gry
    while (!done) {
        ALLEGRO_EVENT event;
        al_wait_for_event(event_queue, &event);

        if (event.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
            done = true;
        }
        else if (event.type == ALLEGRO_EVENT_KEY_DOWN && event.keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
            done = true;
        }
        else if (event.type == ALLEGRO_EVENT_KEY_DOWN) {
            obsluz_wejscie(event, &player, &current_game_state);
        }
        else if (event.type == ALLEGRO_EVENT_TIMER) {
            aktualizuj_gre(&player, bombs, enemies, powerups, game_map, &current_game_state, &exit_revealed, exit_x, exit_y);
            rysuj_gre(display, &player, bombs, enemies, powerups, game_map, current_game_state, exit_revealed, exit_x, exit_y);
        }
    }

cleanup:
    // Zwalnianie wszystkich załadowanych zasobów Allegro
    if (timer) al_destroy_timer(timer);

    if (player_sprite_front) al_destroy_bitmap(player_sprite_front);
    if (player_sprite_back) al_destroy_bitmap(player_sprite_back);
    if (player_sprite_left) al_destroy_bitmap(player_sprite_left);
    if (player_sprite_right) al_destroy_bitmap(player_sprite_right);
    if (destructible_wall_sprite) al_destroy_bitmap(destructible_wall_sprite);
    if (dynamite_sprite) al_destroy_bitmap(dynamite_sprite);
    if (sparks_sprite) al_destroy_bitmap(sparks_sprite);
    if (exit_sprite) al_destroy_bitmap(exit_sprite);

    if (background_music_instance) al_destroy_sample_instance(background_music_instance);
    if (background_music_sample) al_destroy_sample(background_music_sample);


    if (font_main) al_destroy_font(font_main);
    if (event_queue) al_destroy_event_queue(event_queue);
    if (display) al_destroy_display(display);

    if (keyboard_installed) al_uninstall_keyboard();
    if (audio_installed) al_uninstall_audio();

    al_shutdown_ttf_addon();
    al_shutdown_font_addon();
    al_shutdown_image_addon();
    al_shutdown_primitives_addon();

    return ret_val;
}
