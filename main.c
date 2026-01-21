#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#ifdef _WIN32
  #include <windows.h>
  #include <direct.h>
  #define MKDIR(path) _mkdir(path)
#else
  #include <dirent.h>
  #include <sys/stat.h>
  #define MKDIR(path) mkdir(path, 0755)
#endif

#define N 8
#define MAX_NAME 32
#define SAVE_DIR "saves"
#define MAX_SAVES 200
#define MAX_PATH_LEN 512

typedef struct {
    int own[N][N];
    int shots[N][N];
} Player;

typedef struct {
    int mode;
    Player p1;
    Player p2;
    char name1[MAX_NAME];
    char name2[MAX_NAME];
} Game;


static void clear_grid(int g[N][N]) {
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            g[i][j] = 0;
}

static void init_game(Game *G) {
    memset(G, 0, sizeof(*G));
    clear_grid(G->p1.own);
    clear_grid(G->p1.shots);
    clear_grid(G->p2.own);
    clear_grid(G->p2.shots);
    strcpy(G->name1, "Joueur1");
    strcpy(G->name2, "Joueur2");
    G->mode = 2;
    G->turn = 0;
}

static char row_label(int r) {
    return (char)('A' + r);
}

static int row_index_from_char(char ch) {
    ch = (char)toupper((unsigned char)ch);
    if (ch < 'A' || ch >= 'A' + N) return -1;
    return (int)(ch - 'A');
}

static void print_shots_grid(int shots[N][N]) {
    printf("    ");
    for (int j = 0; j < N; j++) printf("%d ", j);
    printf("\n");

    for (int i = 0; i < N; i++) {
        printf(" %c: ", row_label(i));
        for (int j = 0; j < N; j++) {
            char c = '.';
            if (shots[i][j] == 2) c = 'o';
            if (shots[i][j] == 3) c = 'X';
            printf("%c ", c);
        }
        printf("\n");
    }
}

static void print_own_grid(int own[N][N]) {
    printf("    ");
    for (int j = 0; j < N; j++) printf("%d ", j);
    printf("\n");

    for (int i = 0; i < N; i++) {
        printf(" %c: ", row_label(i));
        for (int j = 0; j < N; j++) {
            char c = '.';
            if (own[i][j] == 1) c = 'B';
            else if (own[i][j] == 2) c = 'o';
            else if (own[i][j] == 3) c = 'X';
            printf("%c ", c);
        }
        printf("\n");
    }
}

static int can_place(int own[N][N], int r, int c, int len, int horiz) {
    if (horiz) {
        if (c + len > N) return 0;
        for (int j = c; j < c + len; j++)
            if (own[r][j] != 0) return 0;
    } else {
        if (r + len > N) return 0;
        for (int i = r; i < r + len; i++)
            if (own[i][c] != 0) return 0;
    }
    return 1;
}

static void place_ship(int own[N][N], int len) {
    while (1) {
        int horiz = rand() % 2;
        int r = rand() % N;
        int c = rand() % N;
        if (!can_place(own, r, c, len, horiz)) continue;

        if (horiz) {
            for (int j = c; j < c + len; j++) own[r][j] = 1;
        } else {
            for (int i = r; i < r + len; i++) own[i][c] = 1;
        }
        return;
    }
}

static void auto_place_all(Player *P) {
    clear_grid(P->own);
    clear_grid(P->shots);
    place_ship(P->own, 2);
    place_ship(P->own, 3);
    place_ship(P->own, 4);
}

static int remaining_boat_cells(int own[N][N]) {
    int count = 0;
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            if (own[i][j] == 1) count++;
    return count;
}

static int take_shot(Player *attacker, Player *defender, int r, int c) {
    // returns 1 if hit, 0 if miss, -1 invalid / already tried
    if (r < 0 || r >= N || c < 0 || c >= N) return -1;
    if (attacker->shots[r][c] != 0) return -1;

    if (defender->own[r][c] == 1) {
        defender->own[r][c] = 3;
        attacker->shots[r][c] = 3;
        return 1;
    } else {
        attacker->shots[r][c] = 2;
        if (defender->own[r][c] == 0) defender->own[r][c] = 2; // optionnel affichage
        return 0;
    }
}

static void ensure_save_dir(void) {
    if (MKDIR(SAVE_DIR) != 0) {
        if (errno != EEXIST) {
            // on ne bloque pas tout, mais possible souci
        }
    }
}

static void make_save_path(char *out, size_t outsz, const char *filename) {
#ifdef _WIN32
    snprintf(out, outsz, "%s\\%s", SAVE_DIR, filename);
#else
    snprintf(out, outsz, "%s/%s", SAVE_DIR, filename);
#endif
}

static void save_game(const Game *G, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        printf("Erreur: impossible d'ouvrir %s en ecriture.\n", filename);
        return;
    }

    fprintf(f, "MODE %d\n", G->mode);
    fprintf(f, "TURN %d\n", G->turn);
    fprintf(f, "NAME1 %s\n", G->name1);
    fprintf(f, "NAME2 %s\n", G->name2);

    #define DUMP_GRID(tag, grid)                          \
        fprintf(f, tag "\n");                             \
        for (int i = 0; i < N; i++) {                     \
            for (int j = 0; j < N; j++) fprintf(f, "%d ", grid[i][j]); \
            fprintf(f, "\n");                             \
        }

    DUMP_GRID("P1_OWN",   G->p1.own);
    DUMP_GRID("P1_SHOTS", G->p1.shots);
    DUMP_GRID("P2_OWN",   G->p2.own);
    DUMP_GRID("P2_SHOTS", G->p2.shots);

    #undef DUMP_GRID

    fclose(f);
    printf("Sauvegarde faite dans: %s\n", filename);
}

static int load_game(Game *G, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        printf("Erreur: impossible d'ouvrir %s en lecture.\n", filename);
        return 0;
    }

    init_game(G);

    char tag[64];
    if (fscanf(f, "%63s %d", tag, &G->mode) != 2 || strcmp(tag, "MODE") != 0) { fclose(f); return 0; }
    if (fscanf(f, "%63s %d", tag, &G->turn) != 2 || strcmp(tag, "TURN") != 0) { fclose(f); return 0; }

    if (fscanf(f, "%63s %31s", tag, G->name1) != 2 || strcmp(tag, "NAME1") != 0) { fclose(f); return 0; }
    if (fscanf(f, "%63s %31s", tag, G->name2) != 2 || strcmp(tag, "NAME2") != 0) { fclose(f); return 0; }

    #define READ_GRID(expectedTag, grid)                  \
        if (fscanf(f, "%63s", tag) != 1 || strcmp(tag, expectedTag) != 0) { fclose(f); return 0; } \
        for (int i = 0; i < N; i++)                        \
            for (int j = 0; j < N; j++)                    \
                if (fscanf(f, "%d", &grid[i][j]) != 1) { fclose(f); return 0; }

    READ_GRID("P1_OWN",   G->p1.own);
    READ_GRID("P1_SHOTS", G->p1.shots);
    READ_GRID("P2_OWN",   G->p2.own);
    READ_GRID("P2_SHOTS", G->p2.shots);

    #undef READ_GRID

    fclose(f);
    printf("Partie chargee depuis: %s\n", filename);
    return 1;
}

static int list_saves(char saves[MAX_SAVES][MAX_PATH_LEN]) {
    int count = 0;
    ensure_save_dir();

#ifdef _WIN32
    char pattern[MAX_PATH_LEN];
    snprintf(pattern, sizeof(pattern), "%s\\*.txt", SAVE_DIR);

    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(pattern, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return 0;

    do {
        if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            snprintf(saves[count], MAX_PATH_LEN, "%s", ffd.cFileName);
            count++;
            if (count >= MAX_SAVES) break;
        }
    } while (FindNextFileA(hFind, &ffd) != 0);

    FindClose(hFind);
#else
    DIR *dir = opendir(SAVE_DIR);
    if (!dir) return 0;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;

        const char *name = ent->d_name;
        size_t L = strlen(name);
        if (L >= 4 && strcmp(name + (L - 4), ".txt") == 0) {
            snprintf(saves[count], MAX_PATH_LEN, "%s", name);
            count++;
            if (count >= MAX_SAVES) break;
        }
    }
    closedir(dir);
#endif
    return count;
}

static void flush_stdin(void) {
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF) {}
}


static void show_shots_only(Player *me) {
    printf("Grille de tirs (chez l'adversaire):\n");
    print_shots_grid(me->shots);
    printf("\n"); // espacement demandé
}

static int human_turn(Game *G, Player *me, Player *enemy, const char *meName) {
    printf("\n--- A toi de jouer (%s) ---\n", meName);

    show_shots_only(me);

    printf("Tape une commande:\n");
    printf("  t L C  -> tirer Ligne(Lettre A-H) Colonne(0-7) (ex: t C 4)\n");
    printf("  s      -> sauvegarder\n");
    printf("  q      -> quitter (sans sauvegarder)\n");
    printf("> ");

    char cmd;
    if (scanf(" %c", &cmd) != 1) return 0;

    if (cmd == 's') {
        ensure_save_dir();

        char filename[128];
        printf("Nom du fichier (sans espaces, ex: partie1.txt) : ");
        if (scanf("%127s", filename) != 1) {
            printf("Nom invalide.\n");
            flush_stdin();
            return 1;
        }

        size_t L = strlen(filename);
        if (L < 4 || strcmp(filename + (L - 4), ".txt") != 0) {
            strncat(filename, ".txt", sizeof(filename) - strlen(filename) - 1);
        }

        char path[MAX_PATH_LEN];
        make_save_path(path, sizeof(path), filename);

        save_game(G, path);
        return 1;
    }

    if (cmd == 'q') {
        return 0;
    }

    if (cmd == 't') {
        char rch;
        int c;
        if (scanf(" %c %d", &rch, &c) != 2) {
            printf("Commande invalide.\n");
            flush_stdin();
            return 1;
        }

        int r = row_index_from_char(rch);
        int res = take_shot(me, enemy, r, c);

        if (res == -1) {
            printf("Case invalide ou deja tiree.\n");
        } else if (res == 1) {
            printf("TOUCHE ! (donc COULE)\n");
        } else {
            printf("A l'eau.\n");
        }
        return 1;
    }

    printf("Commande inconnue.\n");
    flush_stdin();
    return 1;
}

static int ai_turn(Player *ai, Player *human, const char *humanName, Player *humanPlayerForDisplay) {
    int r, c;
    while (1) {
        r = rand() % N;
        c = rand() % N;
        if (ai->shots[r][c] == 0) break;
    }

    printf("\n--- Tour ORDI ---\n");
    printf("L'ordi tire: %c %d\n", row_label(r), c);

    int res = take_shot(ai, human, r, c);
    if (res == 1) printf("L'ordi a TOUCHE !\n");
    else printf("L'ordi a rate.\n");

    printf("\nGrille de %s:\n", humanName);
    print_own_grid(humanPlayerForDisplay->own);
    printf("\n");

    return 1;
}


static void setup_new_game(Game *G) {
    init_game(G);

    printf("Mode de jeu:\n");
    printf("1) Jouer contre l'ordi\n");
    printf("2) Jouer a 2 joueurs\n");
    printf("> ");

    int m = 0;
    scanf("%d", &m);
    if (m != 1 && m != 2) m = 2;
    G->mode = m;

    printf("Nom joueur 1 (sans espaces): ");
    scanf("%31s", G->name1);

    if (G->mode == 2) {
        printf("Nom joueur 2 (sans espaces): ");
        scanf("%31s", G->name2);
    } else {
        strcpy(G->name2, "ORDI");
    }

    auto_place_all(&G->p1);
    auto_place_all(&G->p2);
    G->turn = 0;

    printf("\nBateaux places automatiquement.\n");

    printf("\nGrille de %s:\n", G->name1);
    print_own_grid(G->p1.own);

    printf("\nGrille de %s:\n", G->name2);
    if (G->mode == 2) print_own_grid(G->p2.own);
    else printf("(cachee)\n");

    printf("\n");
}

static int setup_load_game(Game *G) {
    char saves[MAX_SAVES][MAX_PATH_LEN];
    int n = list_saves(saves);

    if (n == 0) {
        printf("Aucune sauvegarde trouvee dans le dossier '%s'.\n", SAVE_DIR);
        return 0;
    }

    printf("Sauvegardes disponibles:\n");
    for (int i = 0; i < n; i++) {
        printf("  %d) %s\n", i + 1, saves[i]);
    }
    printf("Choisis un numero (1-%d): ", n);

    int pick = 0;
    if (scanf("%d", &pick) != 1 || pick < 1 || pick > n) {
        printf("Choix invalide.\n");
        return 0;
    }

    char path[MAX_PATH_LEN];
    make_save_path(path, sizeof(path), saves[pick - 1]);

    if (!load_game(G, path)) {
        printf("Chargement impossible.\n");
        return 0;
    }

    printf("\n(Partie chargee) Grille de %s:\n", G->name1);
    print_own_grid(G->p1.own);

    printf("\n(Partie chargee) Grille de %s:\n", G->name2);
    if (G->mode == 2) print_own_grid(G->p2.own);
    else printf("(cachee)\n");

    printf("\n");
    return 1;
}

static void play_game(Game *G) {
    while (1) {
        int p1_left = remaining_boat_cells(G->p1.own);
        int p2_left = remaining_boat_cells(G->p2.own);

        if (p1_left == 0) { printf("\n%s a perdu. %s gagne !\n", G->name1, G->name2); break; }
        if (p2_left == 0) { printf("\n%s a perdu. %s gagne !\n", G->name2, G->name1); break; }

        int keep_running = 1;

        if (G->turn == 0) {
            keep_running = human_turn(G, &G->p1, &G->p2, G->name1);
        } else {
            if (G->mode == 2) {
                keep_running = human_turn(G, &G->p2, &G->p1, G->name2);
            } else {
                keep_running = ai_turn(&G->p2, &G->p1, G->name1, &G->p1);
            }
        }

        if (!keep_running) {
            printf("Fin du jeu.\n");
            break;
        }

        G->turn = 1 - G->turn;
    }
}


int main(void) {
    srand((unsigned)time(NULL));

    Game G;

    while (1) {
        printf("BATAILLE NAVALE 8x8 (bateaux 2,3,4) - touche=coule\n");
        printf("1) Nouvelle partie\n");
        printf("2) Charger une sauvegarde\n", SAVE_DIR);
        printf("0) Quitter\n");
        printf("> ");

        int choice = -1;
        if (scanf("%d", &choice) != 1) return 0;

        if (choice == 0) {
            printf("Au revoir.\n");
            return 0;
        }

        if (choice == 1) {
            setup_new_game(&G);
            play_game(&G);
            printf("\n");
            continue;
        }

        if (choice == 2) {
            if (setup_load_game(&G)) {
                play_game(&G);
            }
            printf("\n");
            continue;
        }

        printf("Choix invalide.\n\n");
    }
}
