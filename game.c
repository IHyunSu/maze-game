/*
    ####--------------------------------####
    #--# Author:   by hyunsu, eunhye    #--#
    #--# License:  GNU GPLv3            #--#
    #--# E-mail:   zmfnwj119@gmail.com  #--#
    ####--------------------------------####
*/

// Includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <utmp.h>
#include <math.h>
#include <ncurses.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>

// Constants
// Const define
#define true 1
#define false 0

// Const state, key kode
#define vk_space  32
#define vk_endter 10
bool EXIT = false;
int key_pressed = 0;

// Const colors
#define c_wall   1
#define c_star   2
#define c_space  3
#define c_plus   4
#define c_minus  5
#define c_player 6
#define c_enemy  7
#define c_hud    1

// Directions (up, down, left, right)
int dx[] = {-2, 2, 0, 0};
int dy[] = {0, 0, -2, 2};

// Global Variables
char username[11] = {0};    // User name stored
short level = 1;            // Current level
short score = 0;            // Player score
short lifes = 3;            // Player lives
int star_in_level = 0;      // Stars in the level
int current_lvl_x, current_lvl_y; // Level size
int w, h;                   // Window width and height
int maze_width = 60;        // Default maze width
int maze_height = 10;       // Default maze height

////////////////////
// Utility Functions
////////////////////

// Shuffle directions randomly
void shuffle_directions(int* dirs) {
    for (int i = 0; i < 4; i++) dirs[i] = i;
    for (int i = 0; i < 4; i++) {
        int j = rand() % 4;
        int tmp = dirs[i];
        dirs[i] = dirs[j];
        dirs[j] = tmp;
    }
}

// Get string length
int str_len(const char* str) {
    int size = 0;
    while (*str++) ++size;
    return size;
}

void SetColor() {
    start_color();
    init_pair(c_wall,   COLOR_WHITE,     COLOR_BLACK);
    init_pair(c_star,   COLOR_YELLOW,    COLOR_BLACK);
    init_pair(c_space,  COLOR_RED,       COLOR_BLACK);   
    init_pair(c_plus,   COLOR_BLUE,      COLOR_BLACK);
    init_pair(c_minus,  COLOR_GREEN,     COLOR_BLACK);
    init_pair(c_player, COLOR_MAGENTA,   COLOR_BLACK);
    init_pair(c_enemy,  COLOR_RED,       COLOR_BLACK);
    init_pair(c_hud,    COLOR_WHITE,     COLOR_BLACK); // HUD color
}

//////////////
// OBJECT 
//////////////

// Class
struct class_obj {
    char symbol[20];
    int hsp, vsp;
    int x, y;
    int direction;
};

// Create objects
struct class_obj player = {};
struct class_obj enemy[5]={};

// Enemy movement
void enemy_move();
void enemy_update();
void clear_enemy();
void obj_init();

/////////////
// PLAYER
////////////
// Player movement variables
int dir_x;
int dir_y;
int dir_shoot;

void player_move();

// Collision
void player_collision();


////////////////////
// Maze Functions
////////////////////

// Maze structure and related functions
typedef struct Maze {
    int width;
    int height;
    bool *right_walls;
    bool *down_walls;
} Maze;

typedef struct Point {
    int x;
    int y;
} Point;

typedef struct PointList {
    Point *items;
    int count;
    int capacity;
} PointList;

PointList make_list(int cap) {
    Point *items = calloc(cap, sizeof(Point));
    PointList ptls = {
        .items = items,
        .count = 0,
        .capacity = cap,
    };
    return ptls;
}

#define da_append(arr, item) \
    do {\
        if ((arr)->count >= (arr)->capacity) {\
            (arr)->capacity *= 2;\
            (arr)->items = realloc((arr)->items, (arr)->capacity * sizeof((arr)->items[0]));\
        }\
        (arr)->items[(arr)->count++] = item;\
    } while(0)

#define da_pop(arr) \
    (arr)->items[--(arr)->count]

void enable_all_walls(Maze *maze) {
    int n_cells = maze->width * maze->height;
    memset(maze->right_walls, 1, sizeof(bool) * n_cells);
    memset(maze->down_walls, 1, sizeof(bool) * n_cells);
}

// Remove wall between two cells
void remove_wall_between(Maze *maze, Point old, Point new) {
    if (old.x + 1 == new.x) {
        maze->right_walls[old.x + old.y * maze->width] = false; // Remove right wall of old
    } else if (old.x - 1 == new.x) {
        maze->right_walls[new.x + new.y * maze->width] = false; // Remove left wall of new
    } else if (old.y + 1 == new.y) {
        maze->down_walls[old.x + old.y * maze->width] = false; // Remove bottom wall of old
    } else if (old.y - 1 == new.y) {
        maze->down_walls[new.x + new.y * maze->width] = false; // Remove top wall of new
    }
}

// Check if a Point exists in a PointList
bool list_contains(PointList list, Point point) {
    for (int i = 0; i < list.count; i++) {
        if (list.items[i].x == point.x && list.items[i].y == point.y) {
            return true;
        }
    }
    return false;
}

// Maze generation step
void maze_gen_step(Maze *maze, Point *current, PointList *visited, PointList *path, PointList *backtracked) {
    if (!list_contains(*visited, *current)) {
        da_append(visited, *current);
    }

    int dirs[] = {0, 1, 2, 3}; // 0: UP, 1: DOWN, 2: LEFT, 3: RIGHT
    shuffle_directions(dirs);

    for (int i = 0; i < 4; i++) {
        Point next = *current;
        switch (dirs[i]) {
            case 0: next.y -= 1; break; // UP
            case 1: next.y += 1; break; // DOWN
            case 2: next.x -= 1; break; // LEFT
            case 3: next.x += 1; break; // RIGHT
        }

        if (next.x >= 0 && next.x < maze->width && next.y >= 0 && next.y < maze->height &&
            !list_contains(*visited, next)) {
            remove_wall_between(maze, *current, next);
            da_append(path, *current);
            *current = next;
            return;
        }
    }

    if (path->count > 0) {
        *current = da_pop(path);
    } else {
        da_append(backtracked, *current);
    }
}

// Structure for portal position
typedef struct {
    int x;
    int y;
} Portal;

Portal portal;

// Helper function: Check if a cell is a valid path
bool is_valid_path(Maze *maze, int x, int y) {
    int idx = x + y * maze->width;
    if (x < maze->width - 1 && maze->right_walls[idx]) return false;
    if (y < maze->height - 1 && maze->down_walls[idx]) return false;
    return true;
}

// Helper function: Place a portal based on the player's position
Point place_portal(Maze *maze, Point player_pos) {
    Point portal_pos = { maze->width - 1 - player_pos.x, maze->height - 1 - player_pos.y };
    while (1) {
        int pidx = portal_pos.x + portal_pos.y * maze->width;
        if (is_valid_path(maze, portal_pos.x, portal_pos.y) &&
            !(portal_pos.x == player_pos.x && portal_pos.y == player_pos.y)) {
            break;
        }
        // Adjust portal position closer to (0,0) if invalid
        if (portal_pos.x > 0) portal_pos.x--;
        if (portal_pos.y > 0) portal_pos.y--;
    }
    return portal_pos;
}

// Helper function: Place enemies randomly
void place_enemies(Maze *maze, struct class_obj *enemies, int enemy_count, Point player_pos, Point portal_pos) {
    int placed = 0;
    while (placed < enemy_count) {
        int x = rand() % maze->width;
        int y = rand() % maze->height;
        if (!is_valid_path(maze, x, y)) continue;
        if ((x == player_pos.x && y == player_pos.y) || (x == portal_pos.x && y == portal_pos.y)) continue;

        // Check for overlap with already placed enemies
        bool overlap = false;
        for (int i = 0; i < placed; i++) {
            if (enemies[i].x == x && enemies[i].y == y) {
                overlap = true;
                break;
            }
        }
        if (overlap) continue;

        // Place enemy
        enemies[placed].x = x;
        enemies[placed].y = y;
        placed++;
    }
}

// Main function: Place player, enemies, and portal
void place_player_and_enemies(Maze *maze, struct class_obj *player, struct class_obj *enemies, int enemy_count) {
    // Place player at the first valid path cell
    for (int y = 0; y < maze->height; y++) {
        for (int x = 0; x < maze->width; x++) {
            if (is_valid_path(maze, x, y)) {
                player->x = x;
                player->y = y;

                // Place portal symmetrically to the player
                Point player_pos = { x, y };
                Point portal_pos = place_portal(maze, player_pos);
                portal.x = portal_pos.x;
                portal.y = portal_pos.y;

                // Place enemies
                place_enemies(maze, enemies, enemy_count, player_pos, portal_pos);
                return; // All placements done
            }
        }
    }
}

// Check if player can move to the next cell
bool can_move_player(Maze *maze, int x, int y, int dx, int dy) {
    int nx = x + dx;
    int ny = y + dy;
    if (nx < 0 || nx >= maze->width || ny < 0 || ny >= maze->height) return false;
    if (dx == -1 && maze->right_walls[(x-1) + y * maze->width]) return false; // Left
    if (dx == 1 && maze->right_walls[x + y * maze->width]) return false; // Right
    if (dy == -1 && maze->down_walls[x + (y-1) * maze->width]) return false; // Up
    if (dy == 1 && maze->down_walls[x + y * maze->width]) return false; // Down
    return true;
}

// Move enemy one step in a random valid direction
void move_enemy(struct class_obj *enemy, Maze *maze) {
    int dirs[4][2] = { {0,-1}, {0,1}, {-1,0}, {1,0} }; // Up, Down, Left, Right
    int valid[4] = {0,};
    int valid_count = 0;
    int x = enemy->x;
    int y = enemy->y;

    // Find valid directions
    if (y > 0 && (maze->down_walls[x + (y-1) * maze->width] == false)) { // Up
        valid[valid_count++] = 0;
    }
    if (y < maze->height-1 && (maze->down_walls[x + y * maze->width] == false)) { // Down
        valid[valid_count++] = 1;
    }
    if (x > 0 && (maze->right_walls[(x-1) + y * maze->width] == false)) { // Left
        valid[valid_count++] = 2;
    }
    if (x < maze->width-1 && (maze->right_walls[x + y * maze->width] == false)) { // Right
        valid[valid_count++] = 3;
    }

    if (valid_count > 0) {
        int dir = valid[rand() % valid_count];
        enemy->x += dirs[dir][0];
        enemy->y += dirs[dir][1];
    }
}

////////////////////
// Menu Functions
////////////////////

const char *menu_logo[5] = {
    " # #       # #           #          # # # # # #   # # # # #",
    " #   #   #   #         #   #                #     #",
    " #     #     #      #    #    #           #       # # # # #",
    " #           #    #            #        #         #",
    " #           #  #                #  # # # # # #   # # # # #",
};

// Get logo size
int logo_h_size = sizeof(menu_logo)/sizeof(menu_logo[0]);

// Get logo width size
int get_logo_w_size(void) {
    int logo_w_size = 1;

    for (int i = 0; i < logo_h_size; i++) {
        int len = str_len(menu_logo[i]);
        if (len > logo_w_size) {
            logo_w_size = len;
        }
    }
    return logo_w_size;
}

// Draw the logo

int logo_w_size = 1;
void draw_logo(int h, int w) {  
    // Get logo width
    if (logo_w_size == 1) {
        logo_w_size = get_logo_w_size() / 2;
    }

    // Draw logo
    attron(COLOR_PAIR(c_hud));
    for (int i = 0; i < logo_h_size; i++) {
        mvprintw(3 + i /* Logo Y pos */, w / 2 - logo_w_size, "%s", menu_logo[i]);
    }
    attroff(COLOR_PAIR(c_hud));
}

////////////////////
// Main Function
////////////////////

int main() {

    // Start curses mode
    initscr();
    keypad(stdscr, TRUE);
    savetty();
    cbreak();
    noecho();
    timeout(0);
    leaveok(stdscr, TRUE);
    start_color();
    curs_set(0);
    init_pair(c_hud, COLOR_WHITE, COLOR_BLACK); // Initialize color pair

    srand(time(NULL));

    ////////////////////
    // Enum game state
    ///////////////////
    typedef enum {
        STATE_MENU,
        STATE_GAME,
        STATE_INFO,
        STATE_USER,
        STATE_EXIT,
    } game_states;

    // Init current state
    game_states current_state;
    current_state = STATE_MENU;

    //////////////
    // init obj
    //////////////

    ////////////////
    // Main loop
    ///////////////

    // Menu items
    const char *item_start_game[2] = {
        "> START GAME <",
        "start game",
    };

    const char *item_info[2] = {
        "> INFO <",
        "info",
    };

    const char *item_exit[2] = {
        "> EXIT <",
        "exit",
    };

    const char *item_user[2] = {
        "> USER <",
        "user",
    };

    while (!EXIT) {

        // Set color
        SetColor();

        // Get window width & height
        getmaxyx(stdscr, h, w);

        // Menu state
        static int menu_item = 0;
        if (key_pressed == KEY_UP)   menu_item--;
        if (key_pressed == KEY_DOWN) menu_item++;

        if (menu_item >= 3) menu_item = 3;
        if (menu_item <= 0) menu_item = 0;

        // In menu state
        switch(current_state) {
            // Menu
            case STATE_MENU:
                // Draw logo
                draw_logo(h, w);

                // Draw menu items
                int select_start_game = menu_item == 0 ? 0 : 1;
                mvprintw(h/2 - logo_h_size + 9, w/2 - str_len(item_start_game[select_start_game])/2, "%s", item_start_game[select_start_game]);

                int select_user = menu_item == 1 ? 0 : 1;
                mvprintw(h/2 - logo_h_size + 11, w/2 - str_len(item_user[select_user])/2, "%s", item_user[select_user]);
                
                int select_info = menu_item == 2 ? 0 : 1;
                mvprintw(h/2 - logo_h_size + 13, w/2 - str_len(item_info[select_info])/2, "%s", item_info[select_info]);

                int select_exit = menu_item == 3 ? 0 : 1;
                mvprintw(h/2 - logo_h_size + 15, w/2 - str_len(item_exit[select_exit])/2, "%s", item_exit[select_exit]);

                // By dev
                mvprintw(h-2, 2, "%s", "Develop: hyunsu, eunhye");

                // Draw box
                attron(COLOR_PAIR(c_hud));
                box(stdscr, 0, 0);
                attron(COLOR_PAIR(c_hud));

                // Click handler
                if (key_pressed == vk_endter) {
                    switch(menu_item) {
                        case 0:
                            current_state = STATE_GAME;
                        break;
                        
                        case 1:
                            current_state = STATE_USER;
                        break;

                        case 2:
                            current_state = STATE_INFO;
                        break;

                        case 3:
                            current_state = STATE_EXIT;
                        break;
                    }
                }
            break;

            // Info
            case STATE_INFO:
                static int len_xoff = 31;
                static int len_yoff = 2;
                if (username[0] == '\0') {
                    mvprintw(h/2-len_yoff-1, w/2-len_xoff, "Hello, new User :)");
                    mvprintw(h/2-len_yoff,   w/2-len_xoff, "Your cleared up to Stage %d, score %d", level, score);
                    mvprintw(h/2-len_yoff+1, w/2-len_xoff, "First logged in on: __"); // use utmp
                    mvprintw(h/2-len_yoff+4, w/2-len_xoff, "------ Have a good game! ------");
                    mvprintw(h/2-len_yoff+5, w/2-len_xoff, "This project was conducted in the System Programming course at");
                    mvprintw(h/2-len_yoff+6, w/2-len_xoff, "Kyungpook National University.");
                } else {
                    mvprintw(h/2-len_yoff-1, w/2-len_xoff, "Hello, %s", username);
                    mvprintw(h/2-len_yoff,   w/2-len_xoff, "%s cleared up to Stage %d, score %d", username, level, score);
                    mvprintw(h/2-len_yoff+1, w/2-len_xoff, "First logged in on: __"); // use utmp
                    mvprintw(h/2-len_yoff+4, w/2-len_xoff, "------ Have a good game! ------");
                    mvprintw(h/2-len_yoff+5, w/2-len_xoff, "This project was conducted in the System Programming course at");
                    mvprintw(h/2-len_yoff+6, w/2-len_xoff, "Kyungpook National University.");
                }

                // To menu
                mvprintw(h-4, w/2-ceil(len_xoff/2), "%s", "press 'q' to exit menu");

                // By dev
                mvprintw(h-2, 2, "%s", "Develop: hyunsu, eunhye");

                box(stdscr, 0, 0);
        
                // Exit to menu
                if (key_pressed == 'q') {
                    current_state = STATE_MENU;
                    erase();
                }
            break;

            // User
            case STATE_USER: {
                int index = 0;
                int ch;
                box(stdscr, 0, 0);
                char input[11] = {0};

                if (username[0] == '\0') {
                    mvprintw(h / 2 - logo_h_size + 4, w / 2 - 35, "Hello ! new User :)");
                    mvprintw(h / 2 - logo_h_size + 5, w / 2 - 35, "press 'y' to edit name");
                    mvprintw(h / 2 - logo_h_size + 6, w / 2 - 35, "press 'q' to exit menu");
                    // By dev
                    mvprintw(h-2, 2, "%s", "Develop: hyunsu, eunhye");

                    // User name edit or exit
                    ch = wgetch(stdscr);
                    if (ch == 'y') {
                        while (1) {
                            ch = wgetch(stdscr); // Use ncurses input function

                            // Handle Enter key
                            if (ch == '\n' || ch == '\r') {
                                break; // End input
                            }

                            // Handle Backspace key
                            if ((ch == 127 || ch == KEY_BACKSPACE) && index > 0) {
                                index--;
                                input[index] = '\0'; // Remove last character
                            }
                            // Handle valid character input
                            else if (index < 10 && ch >= 32 && ch <= 126) { // Printable ASCII range
                                input[index++] = ch;
                                input[index] = '\0'; // Null-terminate the string
                            }
                            
                            // Clear and redraw input line
                            mvprintw(h / 2 - logo_h_size + 4, w / 2 - 15, "Your Name :"); // Clear line
                            mvprintw(h / 2 - logo_h_size + 4, w / 2 - 3, "                    "); // Clear line
                            mvprintw(h / 2 - logo_h_size + 4, w / 2 - 3, "%s", input); // Redraw username
                        }
                        input[index] = '\0';
                        strcpy(username, input);
                        // Return to menu
                        current_state = STATE_MENU;
                        erase();
                        break;
                    } else if (ch == 'q') {
                        current_state = STATE_MENU;
                        erase();
                    }
                    break;
                } else {
                    mvprintw(h / 2 - logo_h_size + 3, w / 2 - 35, "Hello ! %s", username); // Clear line
                    mvprintw(h / 2 - logo_h_size + 4, w / 2 - 35, "Would you like to edit your name?");
                    mvprintw(h / 2 - logo_h_size + 5, w / 2 - 35, "press 'y' to edit name");
                    mvprintw(h / 2 - logo_h_size + 6, w / 2 - 35, "press 'q' to exit menu");
                    // By dev
                    mvprintw(h-2, 2, "%s", "Develop: hyunsu, eunhye");
                    
                    // User name edit or exit
                    ch = wgetch(stdscr);
                    if (ch == 'y') {
                        while (1) {
                            ch = wgetch(stdscr); // Use ncurses input function

                            // Handle Enter key
                            if (ch == '\n' || ch == '\r') {
                                break; // End input
                            }

                            // Handle Backspace key
                            if ((ch == 127 || ch == KEY_BACKSPACE) && index > 0) {
                                index--;
                                input[index] = '\0'; // Remove last character
                            }
                            // Handle valid character input
                            else if (index < 10 && ch >= 32 && ch <= 126) { // Printable ASCII range
                                input[index++] = ch;
                                input[index] = '\0'; // Null-terminate the string
                            }
                            
                            // Clear and redraw input line
                            mvprintw(h / 2 - logo_h_size + 3, w / 2 - 15, "Your Name :"); // Clear line
                            mvprintw(h / 2 - logo_h_size + 3, w / 2 - 3, "                    "); // Clear line
                            mvprintw(h / 2 - logo_h_size + 3, w / 2 - 3, "%s", input); // Redraw username
                        }
                        input[index] = '\0';
                        strcpy(username, input);
                        // Return to menu
                        current_state = STATE_MENU;
                        erase();
                        break;
                    } else if (ch == 'q') {
                        current_state = STATE_MENU;
                        erase();
                    }
                    break;
                }
            }

            // Game
            case STATE_GAME: {
                static Maze maze;
                static Point current;
                static PointList visited, path, backtracked;
                static bool initialized = false;
                static bool placed = false;
                static int tick = 0;
                static int enemy_move_tick = 0;

                if (!initialized) {
                    maze.width = maze_width;
                    maze.height = maze_height;
                    maze.right_walls = calloc(maze.width * maze.height, sizeof(bool));
                    maze.down_walls = calloc(maze.width * maze.height, sizeof(bool));
                    enable_all_walls(&maze);

                    current.x = 0;
                    current.y = 0;
                    visited = make_list(maze.width * maze.height);
                    path = make_list(maze.width * maze.height);
                    backtracked = make_list(maze.width * maze.height);

                    initialized = true;
                    placed = false;
                }

                // Generate maze until complete
                while (visited.count < maze.width * maze.height) {
                    maze_gen_step(&maze, &current, &visited, &path, &backtracked);
                }

                // Place player and enemies after maze is generated (only once)
                if (!placed) {
                    place_player_and_enemies(&maze, &player, enemy, 5);
                    placed = true;
                }

                // Movement Player
                int dx = 0, dy = 0;
                if (key_pressed == KEY_UP) dy = -1;
                else if (key_pressed == KEY_DOWN) dy = 1;
                else if (key_pressed == KEY_LEFT) dx = -1;
                else if (key_pressed == KEY_RIGHT) dx = 1;

                if ((dx != 0 || dy != 0) && can_move_player(&maze, player.x, player.y, dx, dy)) {
                    player.x += dx;
                    player.y += dy;
                }

                // Movement Enemy
                tick++;
                enemy_move_tick++;
                if (enemy_move_tick >= 15) {
                    for (int e = 0; e < 5; e++) {
                        move_enemy(&enemy[e], &maze);
                    }
                    enemy_move_tick = 0;
                }

                // Check collision: if player meets any enemy, go to main menu
                for (int e = 0; e < 5; e++) {
                    if (player.x == enemy[e].x && player.y == enemy[e].y) {
                        current_state = STATE_MENU;
                        initialized = false;
                        placed = false;
                        free(maze.right_walls);
                        free(maze.down_walls);
                        free(visited.items);
                        free(path.items);
                        free(backtracked.items);
                        erase();
                        goto after_game_case; // break out of the drawing loop
                    }
                }

                // Draw the maze
                for (int j = 0; j < maze.height; j++) {
                    for (int i = 0; i < maze.width; i++) {
                        int x = i * 2;
                        int y = j * 2 + 5;

                        // Display portal
                        if (portal.x == i && portal.y == j) {
                            mvprintw(y + 1, x, "O"); // Portal
                            continue;
                        }

                        // Display player and enemies
                        bool drawn = false;
                        if (player.x == i && player.y == j) {
                            mvprintw(y + 1, x, "@"); // Player
                            drawn = true;
                        } else {
                            for (int e = 0; e < 5; e++) {
                                if (enemy[e].x == i && enemy[e].y == j) {
                                    mvprintw(y + 1, x, "X"); // Enemy
                                    drawn = true;
                                    break;
                                }
                            }
                        }
                        if (!drawn) {
                            mvprintw(y + 1, x, " ");
                        }

                        // Draw the right wall
                        if (i < maze.width - 1 && maze.right_walls[i + j * maze.width]) {
                            mvprintw(y + 1, x + 1, "|");
                        }

                        // Draw the bottom wall
                        if (j < maze.height - 1 && maze.down_walls[i + j * maze.width]) {
                            mvprintw(y + 2, x, "_");
                        }
                    }
                }

                // If player reaches portal, go to next level
                if (player.x == portal.x && player.y == portal.y) {
                    level++;
                    maze_width += 2;
                    maze_height += 1;
                    if (maze_width > 80) maze_width = 80;
                    if (maze_height > 25) maze_height = 25;
                    initialized = false;
                    placed = false;
                    erase();
                    continue;
                }

                if (key_pressed == 'q') {
                    current_state = STATE_MENU;
                    initialized = false;
                    placed = false;
                    free(maze.right_walls);
                    free(maze.down_walls);
                    free(visited.items);
                    free(path.items);
                    free(backtracked.items);
                    erase();
                }

                usleep(30000); // pause 30ms

                box(stdscr, 0, 0);
after_game_case:
                break;
            }

            // Exit
            case STATE_EXIT:
                endwin();
                EXIT = TRUE;
            break;
        }

        // Get key pressed
        key_pressed = wgetch(stdscr);

        // Clear
        erase();
    }

    // End curses mode
    endwin();

    return 0;
}
