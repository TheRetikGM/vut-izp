/**
 * Kostra programu pro 2. projekt IZP 2022/23
 *
 * Jednoducha shlukova analyza: 2D nejblizsi soused.
 * Single linkage
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h> // sqrtf
#include <limits.h> // INT_MAX
#include <string.h> // memcpy
#include <stdbool.h>
#include <time.h> // time()

/*****************************************************************
 * Ladici makra. Vypnout jejich efekt lze definici makra
 * NDEBUG, napr.:
 *   a) pri prekladu argumentem prekladaci -DNDEBUG
 *   b) v souboru (na radek pred #include <assert.h>
 *      #define NDEBUG
 */
#ifdef NDEBUG
#define debug(s)
#define dfmt(s, ...)
#define dint(i)
#define dfloat(f)
#else

// vypise ladici retezec
#define debug(s) printf("- %s\n", s)

// vypise formatovany ladici vystup - pouziti podobne jako printf
#define dfmt(s, ...) printf(" - "__FILE__":%u: "s"\n",__LINE__,__VA_ARGS__)

// vypise ladici informaci o promenne - pouziti dint(identifikator_promenne)
#define dint(i) printf(" - " __FILE__ ":%u: " #i " = %d\n", __LINE__, i)

// vypise ladici informaci o promenne typu float - pouziti
// dfloat(identifikator_promenne)
#define dfloat(f) printf(" - " __FILE__ ":%u: " #f " = %g\n", __LINE__, f)

#endif

// Vypise error hlasku do stderr.
#define perr(fmt, ...) fprintf(stderr, __FILE__ ":%i - error: " fmt "\n", __LINE__, ##__VA_ARGS__)
// Makro, ktere funguje jako assert, akorat s tim rozdilem, ze vypise error, spusti cleanup funkci a vrati navratovou hodnotu.
#define CHECK(expr, func, ret, format, ...) if (!(expr)) { perr(format, ##__VA_ARGS__); func; return ret; }
// Maximalni hodnota souradnice za zadani.
#define MAX_XY_VALUE 1000
#define IN_RANGE(v) (v >= 0 && v <= MAX_XY_VALUE)

/*****************************************************************
 * Deklarace potrebnych datovych typu:
 *
 * TYTO DEKLARACE NEMENTE
 *
 *   struct obj_t - struktura objektu: identifikator a souradnice
 *   struct cluster_t - shluk objektu:
 *      pocet objektu ve shluku,
 *      kapacita shluku (pocet objektu, pro ktere je rezervovano
 *          misto v poli),
 *      ukazatel na pole shluku.
 */

struct obj_t {
  int id;
  float x;
  float y;
};

struct cluster_t {
  int size;
  int capacity;
  struct obj_t *obj;
};

// Pro prenaseni argumentu programu.
typedef struct prg_arg_t {
  char  *filename;
  int   n_clusters;
  bool  k_means;
} PrgArg;

/*****************************************************************
 * Deklarace potrebnych funkci.
 *
 * PROTOTYPY FUNKCI NEMENTE
 *
 * IMPLEMENTUJTE POUZE FUNKCE NA MISTECH OZNACENYCH 'TODO'
 *
 */

/*
 Inicializace shluku 'c'. Alokuje pamet pro cap objektu (kapacitu).
 Ukazatel NULL u pole objektu znamena kapacitu 0.
*/
void init_cluster(struct cluster_t *c, int cap)
{
  assert(c != NULL);
  assert(cap >= 0);

  c->size = 0;
  c->capacity = cap;
  c->obj = cap > 0 ? (struct obj_t *)malloc(cap * sizeof(*c->obj)) : NULL;
  assert(cap == 0 || c->obj != NULL); // Dosla pamet? :(
}

/*
 Odstraneni vsech objektu shluku a inicializace na prazdny shluk.
 */
void clear_cluster(struct cluster_t *c)
{
  if (c->size == 0)
    return;

  if (c->capacity > 0)
    free(c->obj);
  init_cluster(c, 0);
}

// Chunk cluster objektu. Tato hodnota je doporucena pro realokaci.
const int CLUSTER_CHUNK = 10;

/*
 Zmena kapacity shluku 'c' na kapacitu 'new_cap'.
 */
struct cluster_t *resize_cluster(struct cluster_t *c, int new_cap)
{
  // TUTO FUNKCI NEMENTE
  assert(c);
  assert(c->capacity >= 0);
  assert(new_cap >= 0);

  if (c->capacity >= new_cap)
      return c;

  size_t size = sizeof(struct obj_t) * new_cap;

  void *arr = realloc(c->obj, size);
  if (arr == NULL)
      return NULL;

  c->obj = (struct obj_t*)arr;
  c->capacity = new_cap;
  return c;
}

/*
 Prida objekt 'obj' na konec shluku 'c'. Rozsiri shluk, pokud se do nej objekt
 nevejde.
 */
void append_cluster(struct cluster_t *c, struct obj_t obj)
{
  if (c->size + 1 > c->capacity) {
    if (resize_cluster(c, c->capacity + CLUSTER_CHUNK) == NULL) {
      perr("Failed to resize cluster capacity.");
      return;
    }
  }
  c->obj[c->size++] = obj;
}

/*
 Seradi objekty ve shluku 'c' vzestupne podle jejich identifikacniho cisla.
 */
void sort_cluster(struct cluster_t *c);

/*
 Do shluku 'c1' prida objekty 'c2'. Shluk 'c1' bude v pripade nutnosti rozsiren.
 Objekty ve shluku 'c1' budou serazeny vzestupne podle identifikacniho cisla.
 Shluk 'c2' bude nezmenen.
 */
void merge_clusters(struct cluster_t *c1, struct cluster_t *c2)
{
  assert(c1 != NULL);
  assert(c2 != NULL);

  for (int i = 0; i < c2->size; i++)
    append_cluster(c1, c2->obj[i]);
  sort_cluster(c1);
}

/**********************************************************************/
/* Prace s polem shluku */

void print_clusters(struct cluster_t *carr, int narr);

/*
 Odstrani shluk z pole shluku 'carr'. Pole shluku obsahuje 'narr' polozek
 (shluku). Shluk pro odstraneni se nachazi na indexu 'idx'. Funkce vraci novy
 pocet shluku v poli.
*/
int remove_cluster(struct cluster_t *carr, int narr, int idx)
{
  assert(idx < narr);
  assert(narr > 0);

  // Uvolni objekty v clusteru.
  clear_cluster(carr + idx);

  // Kopiruj zbytek pole od smazaneho clusteru o jeden index bliz.
  //  - Timto nam vznikne jeden prazdny index na konci pole.
  if (idx < narr - 1)
    memmove(carr + idx, carr + idx + 1, (narr - idx - 1) * sizeof(*carr));  // Source and Dest overleap, so we have to use memmove, beacuse memcpy's behaivior is undefined.
  
  // Inicializuj posledni cluster jako prazdy.
  init_cluster(carr + narr - 1, 0);

  return narr - 1;
}

/*
 Pocita Euklidovskou vzdalenost mezi dvema objekty.
 */
float obj_distance(struct obj_t *o1, struct obj_t *o2)
{
  assert(o1 != NULL);
  assert(o2 != NULL);

  // Spocitame vzdalenost na ^2 protoze nepotrebujeme vedet presnout vzdalenost.
  int delta_x = o1->x - o2->x;
  int delta_y = o1->y - o2->y;
  return (float)(delta_x * delta_x + delta_y * delta_y);
}

/*
 Pocita vzdalenost dvou shluku.
*/
float cluster_distance(struct cluster_t *c1, struct cluster_t *c2)
{
  assert(c1 != NULL);
  assert(c1->size > 0);
  assert(c2 != NULL);
  assert(c2->size > 0);

  // Pozor! Funkce obj_distance vraci ctvercovou vyzdalenost (obsah cverce nad preponou)
  float min_dist = (MAX_XY_VALUE * MAX_XY_VALUE) + 1;

  // Najdeme nejmensi vzdalenost mezi vsemy elementy dvou clusteru.
  for (int i = 0; i < c1->size; i++)
    for (int j = 0; j < c2->size; j++) {
      float dist = obj_distance(c1->obj + i, c2->obj + j);
      min_dist = fmin(min_dist, dist);
    }

  return min_dist;
}

/*
 Funkce najde dva nejblizsi shluky. V poli shluku 'carr' o velikosti 'narr'
 hleda dva nejblizsi shluky. Nalezene shluky identifikuje jejich indexy v poli
 'carr'. Funkce nalezene shluky (indexy do pole 'carr') uklada do pameti na
 adresu 'c1' resp. 'c2'.
*/
void find_neighbours(struct cluster_t *carr, int narr, int *c1, int *c2)
{
  assert(narr > 0);

  // Pole shulku o jednom shluku ma pouze jeden shluk, ktery je nejblizsi sam sobe.
  if (narr == 1) {
    *c1 = *c2 = 0;
    return;
  }

  *c1 = 0; *c2 = 1;
  float min_dist = (MAX_XY_VALUE * MAX_XY_VALUE) + 1, dist = 0.0f;

  for (int i = 0; i < narr; i++) {
    for (int j = i + 1; j < narr; j++) {
      dist = cluster_distance(carr + i, carr + j);
      if (dist < min_dist) {
        *c1 = i;
        *c2 = j;
        min_dist = dist;
      }
    }
  }
}

// pomocna funkce pro razeni shluku
static int obj_sort_compar(const void *a, const void *b)
{
  // TUTO FUNKCI NEMENTE
  const struct obj_t *o1 = (const struct obj_t *)a;
  const struct obj_t *o2 = (const struct obj_t *)b;
  if (o1->id < o2->id) return -1;
  if (o1->id > o2->id) return 1;
  return 0;
}

/*
 Razeni objektu ve shluku vzestupne podle jejich identifikatoru.
*/
void sort_cluster(struct cluster_t *c)
{
  // TUTO FUNKCI NEMENTE
  qsort(c->obj, c->size, sizeof(struct obj_t), &obj_sort_compar);
}

/*
 Tisk shluku 'c' na stdout.
*/
void print_cluster(struct cluster_t *c)
{
  // TUTO FUNKCI NEMENTE
  for (int i = 0; i < c->size; i++)
  {
      if (i) putchar(' ');
      printf("%d[%g,%g]", c->obj[i].id, c->obj[i].x, c->obj[i].y);
  }
  putchar('\n');
}

void delete_clusters(struct cluster_t **arr, int n_clusters)
{
  if (n_clusters < 1)
    return;

  // Uvolni pamet alokovanou shlukem objektu.
  for (int i = 0; i < n_clusters; i++) 
    clear_cluster(*arr + i);

  free(*arr);
  *arr = NULL;
}

void load_cleanup(struct cluster_t **arr, int narr, FILE *fd)
{
  if (*arr != NULL)
    delete_clusters(arr, narr);
  fclose(fd);
} 
bool is_unique_id(int id, struct cluster_t *arr, int first_n_elements)
{
  for (int i = 0; i < first_n_elements; i++)
    for (int j = 0; j < arr[i].size; j++)
      if (id == arr[i].obj[j].id)
        return false;
  return true;
}
/*
 Ze souboru 'filename' nacte objekty. Pro kazdy objekt vytvori shluk a ulozi
 jej do pole shluku. Alokuje prostor pro pole vsech shluku a ukazatel na prvni
 polozku pole (ukalazatel na prvni shluk v alokovanem poli) ulozi do pameti,
 kam se odkazuje parametr 'arr'. Funkce vraci pocet nactenych objektu (shluku).
 V pripade nejake chyby uklada do pameti, kam se odkazuje 'arr', hodnotu NULL.
*/
int load_clusters(char *filename, struct cluster_t **arr)
{
  assert(arr != NULL);
  *arr = NULL;

  FILE* fd = fopen(filename, "r");
  if (!fd) {
    *arr = NULL;
    perr("Failed to open file '%s' for reading.", filename);
    return 0;
  }

  // Nacteni poctu objektu ke cteni.
  char last_char = 0;
  int n_obj = 0;
  int count_scanned = fscanf(fd, "count=%i%c", &n_obj, &last_char);
  CHECK(count_scanned != 0 && last_char == '\n', load_cleanup(arr, 0, fd), 0, "Count parameter is in invalid format.");
  CHECK(n_obj > 0, load_cleanup(arr, 0, fd), 0, "Invalid number of rows passed (%i).", n_obj);

  // Alokovani pameti pro vsechny clustery. Ke smazani je funkce delete_clusters().
  *arr = (struct cluster_t *)malloc(n_obj * sizeof(**arr));
  CHECK(*arr != NULL, load_cleanup(arr, 0, fd), 0, "Failed to allocate memory for cluster array.");
  memset(*arr, 0, n_obj * sizeof(**arr));
  
  // Nacteni vsech bodu radek po radku.
  int obj_id = 0, obj_x = 0, obj_y = 0;
  for (int i = 0; i < n_obj; i++)
  {
    int n_scanned = fscanf(fd, "%i %i %i%c", &obj_id, &obj_x, &obj_y, &last_char);

    // Kontrola poctu nactenych cisel v jedne radce.
    CHECK(n_scanned == 4 && last_char == '\n', load_cleanup(arr, n_obj, fd), 0, "Invalid row format.");
    // Kontrola rozsahu souradnic
    CHECK(IN_RANGE(obj_x) && IN_RANGE(obj_y), load_cleanup(arr, n_obj, fd), 0, "Object coordinates are out of range. OBJID = %i, X = %i, Y = %i\n", obj_id, obj_x, obj_y);
    // Kontrola unikatniho ID
    CHECK(is_unique_id(obj_id, *arr, i), load_cleanup(arr, n_obj, fd), 0, "ID is not unique! ID = %i", obj_id);

    init_cluster(*arr + i, CLUSTER_CHUNK);
    (*arr)[i].size = 1;
    (*arr)[i].obj->id = obj_id;
    (*arr)[i].obj->x = obj_x;
    (*arr)[i].obj->y = obj_y;
  }

  fclose(fd);
  return n_obj;
}

/*
 Tisk pole shluku. Parametr 'carr' je ukazatel na prvni polozku (shluk).
 Tiskne se prvnich 'narr' shluku.
*/
void print_clusters(struct cluster_t *carr, int narr)
{
  printf("Clusters:\n");
  for (int i = 0; i < narr; i++)
  {
      printf("cluster %d: ", i);
      print_cluster(&carr[i]);
  }
}

bool parse_arguments(int argc, char **argv, PrgArg *args)
{
  // Brzky exit.
  if (argc == 1 || argc > 4)
    return false;

  // Parsni filename a cluster count.
  args->filename = argv[1];
  if (argc >= 3) {
    char *perr = NULL;
    args->n_clusters = (int)strtol(argv[2], &perr, 10);

    if (*perr != '\0' || args->n_clusters < 1)
      return false;
  } else {
    args->n_clusters = 1;
  }

  // Zkontroluj flag -k.
  args->k_means = (argc == 4 && strcmp("-k", argv[3]) == 0);

  return args->k_means || argc != 4;
}

// Metoda nejblizsiho souseda pro shlukovani clusteru.
void nn_method(struct cluster_t *clusters, int narr, int n_wanted_clusters)
{
  // Ze zacatku jsou vsechny clustery ve svem clusteru.
  int n_clusters = narr, c1, c2;
  while (n_clusters > n_wanted_clusters) {
    // Najdi dva nejbizsi clustery.
    find_neighbours(clusters, n_clusters, &c1, &c2);
    // Sluc druhy cluster do prvniho.
    merge_clusters(clusters + c1, clusters + c2);
    // Odstran druhy cluster, protoze ho uz nepotrebujeme.
    n_clusters = remove_cluster(clusters, n_clusters, c2);
  }
}

// Get index of the nearest centroid to point.
int main(int argc, char *argv[])
{
  struct cluster_t *clusters = NULL;

  // Parse argumentu a vypis error pokud potreba.
  PrgArg args;
  if (!parse_arguments(argc, argv, &args)) {
    perr("Invalid arguments provided.");
    return EXIT_FAILURE;
  }

  // Nacteni clusteru ze souboru.
  int n_loaded_clusters = load_clusters(args.filename, &clusters);
  if (clusters == NULL)
    return EXIT_FAILURE;

  // Pocet pozadovanych clusteru nesmi byt veci nez pocet bodu v souboru.
  CHECK(n_loaded_clusters >= args.n_clusters, delete_clusters(&clusters, n_loaded_clusters), EXIT_FAILURE, "Number of wanted clusters is too high.");

  if (n_loaded_clusters != args.n_clusters) {
      nn_method(clusters, n_loaded_clusters, args.n_clusters);
  }
  else {
    perr("k-means not impleneted yet.");
  }

  print_clusters(clusters, args.n_clusters);
  delete_clusters(&clusters, n_loaded_clusters);

  return EXIT_SUCCESS;
}

