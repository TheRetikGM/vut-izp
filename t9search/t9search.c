#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#define MAX_LINE_LEN 100
#define MAX_ENTRIES 50
#define MAX_FILTER_LEN 10   // Must be < 64 due to skip_mask being 64 bit number.
#define ANY_FILTER "*"
#define perr(msg, ...) fprintf(stderr, msg, ##__VA_ARGS__)
#define TRY_ALL 0
#define MAX_SIM_ENTRIES 50      // Only needed for storing similiar results for later. As we print them only after we cannot find a single match.

const char* T9_MAP[10] = {
    "0+",
    "1",     "2abc", "3def",
    "4ghi",  "5jkl", "6mno",
    "7pqrs", "8tuv", "9wxyz",
};

typedef struct tel_entry_t {
    char name[MAX_LINE_LEN];
    char number[MAX_LINE_LEN];
} TelEntry;

// Define which characters should be skipped in t9match_string_sep().
typedef uint64_t SkipMask;

// Store result details from is_similiar_sep() function.
typedef struct similiar_result_t {
    int mistakes;
    SkipMask mask;
    const char* p_filter;
} SimiliarResult;

// We use these to choose between different implementations, depending on the '-s' parameter.
typedef bool (*SimFun)(const char*, const char*, const char*, int, SimiliarResult*);
typedef bool (*T9MatchFun)(const char*, const char*, SkipMask);


// Parse arguments from command line and return parse success status.
bool parse_arguments(int argc, char** argv, char* out_filter, int* out_lev, bool* out_sep);
// Just print usage to stderr.
void print_usage(const char* program_name);
// Print all matches from stdin and if none are found, print all similiar matches according
// to the lev (maximum edit distance).
int print_matches_from_stdin(const char* filter, int lev, T9MatchFun is_match_fun, SimFun is_similiar_fun);
// Print entry with required format.
void print_entry(const TelEntry* t) { printf("%s, %s\n", t->name, t->number); }
void print_entry_similiar(const TelEntry* t, const SimiliarResult* r);
// Try to match given T9 filter to string. We assume, that there can be any number of
// characters between two matches.
// Using skip_mask we can flag which characters in filter should be skipped.
bool t9match_string_sep(const char* filter, const char* s, SkipMask skip_mask);
// Check if filter is substring of s.
bool t9match_string(const char* filter, const char* s, SkipMask skip_mask);
// We remove all combinations of up to 'lev' filter characters and check if the resulting filter matches.
// Only removal is needed because we assume any number of characters can be between matches. Than means:
//   - There cannot be a missing character. 
//   - Extra character is the same thing as wrong character => we just remove them. 
bool is_similiar_sep(const char* filter, const char* name_num, const char* num_num, int lev, SimiliarResult* res);
// Check https://en.wikipedia.org/wiki/Levenshtein_distance. Algorithm used is iterative with two matrixes.
bool is_similiar(const char* filter, const char* name_num, const char* num_num, int lev, SimiliarResult* res);

int main(int argc, char* argv[])
{
    int lev = 0;
    bool separated = false;     // Find matches with any number of characters between matches.
    char filter[MAX_FILTER_LEN];
    if (!parse_arguments(argc, argv, filter, &lev, &separated)) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    SimFun sim_fun = separated ? &is_similiar_sep : &is_similiar;
    T9MatchFun match_fun = separated ? &t9match_string_sep : &t9match_string;

    if (print_matches_from_stdin(filter, lev, match_fun, sim_fun) == 0) {
        printf("Not found\n");
    }

  return EXIT_SUCCESS;
}

// Get T9 number representing the current character.
char get_t9number(char c)
{
    const int MAP_SIZE = (int)sizeof(T9_MAP) / (int)sizeof(T9_MAP[0]);
    for (int i = 0; i < MAP_SIZE; i++)
        if (strchr(T9_MAP[i], c))
            return T9_MAP[i][0];
    return 0;
}
char to_lower(char c)
{
    if (c >= 'A' && c <= 'Z')
        return c + 'a' - 'A';
    return c;
}

// Convert string to sequence of T9 numbers.
// Special characters such as space and dot are skipped.
void to_t9number(const char* str, char* out_num, int max_len)
{
    for (int i = 0; str[i] && i < max_len; i++)
        // Save converted as well as check if it was converted successfully.
        if ((out_num[i] = get_t9number(to_lower(str[i]))) == 0)
            out_num[i] = str[i];
}

// Correct the read line: Remove trailing newline and make sure, entire line (from stdin) was read.
void correct_line(char* str)
{
    int line_len = strlen(str);
    if (str[line_len - 1] == '\n')
        str[line_len - 1] = 0;
    else {
        // When only a part of the line is read (defined by MAX_LINE_LEN), 
        // then we need to dispose of the remaining characters in this line.
        for (char c = 0; c != '\n' && c != EOF;)
            c = getchar();
    }
} 

bool t9match_string(const char* filter, const char* s, SkipMask skip_mask)
{
    if (strcmp(filter, ANY_FILTER) == 0)
        return true;

    (void)skip_mask;
    return strstr(s, filter) != NULL;
} 
bool t9match_string_sep(const char* filter, const char* s, SkipMask skip_mask)
{
    if (strcmp(filter, ANY_FILTER) == 0)
        return true;

    int index = -1;
    int filter_len = strlen(filter);
    for (int i = 0; i < filter_len; i++)
    {
        // If bit in mask is 1 then skip this filter character.
        if (skip_mask & (1 << (filter_len - i - 1)))
            continue;

        // Find match from index.
        for (index = index + 1; s[index]; index++)
            if (filter[i] == s[index]) 
                break;

        // Check if index is valid. It could be an end of the string meaning it's not mathing. 
        if (!s[index])
            return false;
    }

    return true;
}

// See https://stackoverflow.com/questions/70155565/generate-a-list-of-binary-numbers-where-n-bits-are-set-to-1 for details.
SkipMask get_next_bit_permutation(SkipMask v) 
{
  SkipMask t = v | (v - 1);
  return (t + 1) | (((~t & -~t) - 1) >> (__builtin_ctz(v) + 1));
}

int min3(int a, int b, int c)
{
    int min = a;
    if (b < min)
        min = b;
    if (c < min)
        min = c;
    return min;
}
int min2(int a, int b) { return a < b ? a : b;}
int get_edit_distance(const char* s1, const char* s2)
{
	// Maxmimum length of the string + 1 space for 0.
	static int distances[MAX_LINE_LEN + 1][MAX_LINE_LEN + 1] = {};
	int len1 = strlen(s1);
	int len2 = strlen(s2);

	for (int i = 1; i <= len1; i++)
		distances[i][0] = i;
	for (int j = 1; j <= len2; j++)
		distances[0][j] = j;

	int sub_cost = 0;
	for (int j = 1; j <= len2; j++)
	{
		for (int i = 1; i <= len1; i++)
		{
			sub_cost = (int)(s1[i - 1] != s2[j - 1]);
			distances[i][j] = min3(
			        distances[i - 1][j] + 1, // Deletion.
			        distances[i][j - 1] + 1, // Insertion.
			        distances[i - 1][j - 1] + sub_cost); // Substitution.
		}
	}

	return distances[len1][len2];
}
bool is_similiar(const char* filter, const char* name_num, const char* num_num, int lev, SimiliarResult* res)
{
    int lev1 = get_edit_distance(filter, name_num);
    int lev2 = get_edit_distance(filter, num_num);
    int min_lev = min2(lev1, lev2);
    
    if (min_lev > lev)
        return false;
    res->mistakes = min2(lev1, lev2);
    res->p_filter = filter;
    res->mask = 0;

    return true;
}
bool is_similiar_sep(const char* filter, const char* name_num, const char* num_num, int lev, SimiliarResult* res)
{
    int filter_len = strlen(filter);
    if (filter_len > MAX_FILTER_LEN) {
        perr("error: Filter is too long. Maximum length is: %i\n", MAX_FILTER_LEN);
        return false;
    }

    for (int mistakes = 1; mistakes <= lev; mistakes++)
    {
        // Generate number with 'mistakes' number of 1 at the beggining.
        SkipMask current_permutation = pow(2, mistakes) - 1;

        while(!(current_permutation & (1 << filter_len)))
        {
            // Check for match with filter with given skip mask.
            if (t9match_string_sep(filter, name_num, current_permutation) || t9match_string_sep(filter, num_num, current_permutation)) {
                if (res != NULL) {
                    res->mask = current_permutation;
                    res->mistakes = mistakes;
                    res->p_filter = filter;
                }
                // Early exit, we dont need to check rest of the permutations as we have already found one matching.
                return true;
            }
            current_permutation = get_next_bit_permutation(current_permutation);
        };
    }

    return false;
}
// Print how filter looks with skip mask applied.
void print_filter(const char* filter, int skip_mask)
{
    int filter_len = strlen(filter);
    for (int i = 0; i < filter_len; i++)
    {
        if (skip_mask & (1 << (filter_len - 1 - i)))
            continue;
        printf("%c", filter[i]);
    }
    printf("\n");
}
void print_entry_similiar(const TelEntry* t, const SimiliarResult* r)
{
    print_entry(t);
    printf("    - mistakes: %i\n      skip mask: 0b", r->mistakes);
    int len = strlen(r->p_filter);
    for (int i = 0; i < len; i++)
        printf("%d", (int)((r->mask & (1 << (len - i - 1))) != 0));
    printf("\n");
    printf("      matching filter: ");
    print_filter(r->p_filter, r->mask);
} 
int get_entry_from_stdin(TelEntry* out)
{
    // Get name from stdin. If name was not read, then we have reached end of file or blank line.
    if (fgets(out->name, MAX_LINE_LEN, stdin) == NULL || strlen(out->name) == 1)
        return -1;
    correct_line(out->name);

    // Get number from stdin.
    if (fgets(out->number, MAX_LINE_LEN, stdin) == NULL) {
        perr("error: Contact is missing number! Name: %s\n", out->name);
        return -1;
    }
    correct_line(out->number);

    return 0;
}
// Convert name and number to T9 strings.
void entry_to_t9entry(const TelEntry* entry, TelEntry* out)
{
    memset(out->name, 0, MAX_LINE_LEN);
    to_t9number(entry->name, out->name, MAX_LINE_LEN);
    memset(out->number, 0, MAX_LINE_LEN);
    to_t9number(entry->number, out->number, MAX_LINE_LEN);
}
int print_matches_from_stdin(const char* filter, int lev, T9MatchFun is_match_fun, SimFun is_similiar_fun)
{
    // We store a limited number of similiar results, as we print them only after we cannot find any match.
    static TelEntry sim_buf[MAX_SIM_ENTRIES];
    static SimiliarResult sim_res_buf[MAX_SIM_ENTRIES];
    int n_sim = 0;

    TelEntry t9_entry;
    int n_printed = 0;
    while (true) 
    {
        TelEntry t;
        if (get_entry_from_stdin(&t) != 0)
            break;
        
        // Get entry in T9 format.
        entry_to_t9entry(&t, &t9_entry);
        
        // Print entry if it matches the current filter.
        SimiliarResult r;
        if (is_match_fun(filter, t9_entry.name, TRY_ALL) || is_match_fun(filter, t9_entry.number, TRY_ALL)) {
            print_entry(&t);
            n_printed++;
        } else if (n_printed < 1 && lev > 0 && n_sim < MAX_SIM_ENTRIES && is_similiar_fun(filter, t9_entry.name, t9_entry.number, lev, &r)) {
            sim_buf[n_sim] = t;
            sim_res_buf[n_sim++] = r;
        }
    }

    // Print all similiar entries.
    if (n_printed == 0 && n_sim != 0) {
        printf("Found similiar: \n");
        for (int i = 0; i < n_sim; i++)
            print_entry_similiar(sim_buf + i, sim_res_buf + i);
    }

    return n_printed + n_sim;
}
void print_usage(const char* program_name)
{
    perr("\n");
    perr("Search for telephone entry from STDIN using a T9 filter.\n");
    perr("Usage: %s [filter][-l:-s]\n", program_name);
    perr("    - filter is a sequence of numbers from 0-9\n");
    perr("    - stdin should contain newline separated list of name and numbers\n");
    perr("-l  - Maximum number of mistakes allowed\n");
    perr("-s  - Search for entries, that have any number of characters between filter matches.\n");
}
// Fitler should only consist of numbers.
bool is_number(const char* filter)
{
   for (int i = 0; filter[i] && i < MAX_FILTER_LEN; i++)
       if (filter[i] < '0' || filter[i] > '9')
           return false;
   return true;
}
bool parse_arguments(int argc, char** argv, char* filter, int* lev, bool* out_sep)
{
    // By default the filter is ANY_FILTER
    // If more than 1 arguments are passed then print usage.
    strcpy(filter, ANY_FILTER);

    bool lev_arg = false;
    for (int i = 1; i < argc; i++)
    {
        // If this argument is data of -l agrument.
        if (lev_arg) {
            *lev = (int)strtol(argv[i], NULL, 10);

            if (!is_number(argv[i]) || *lev < 0) {
                perr("error: Lev must be a number greater than 0\n.");
                return false;
            }
            lev_arg = false;
            continue;
        }

        if (strcmp(argv[i], "-l") == 0)
            lev_arg = true;
        else if (strcmp(argv[i], "-s") == 0) {
            if (i != 1) {
                perr("error: For some reason parameter -s must be the first parammeter.\n");
                return false;
            }
            *out_sep = true;
        }
        else if (strcmp(filter, ANY_FILTER) != 0) {
            perr("error: Unexpected argument %s\n", argv[i]);
            return false;
        }
        else if (is_number(argv[i]) == false) {
            perr("error: Filter is in unexpected format.\n");
            return false;
        }
        else {
            if (strlen(argv[i]) >= MAX_FILTER_LEN) {
                perr("error: filter length is too big. Maximum is: %i\n", MAX_FILTER_LEN);
                return false;
            }
            strcpy(filter, argv[i]);
        }
    }

    if (lev_arg) {
        perr("error: Expected parameter for -l\n");
        return false;
    }
    if (*lev >= (int)strlen(filter)) {
        perr("error: Edit distance must not be >= than length of the T9 filter.\n");
        return false;
    }

    return true;
}

