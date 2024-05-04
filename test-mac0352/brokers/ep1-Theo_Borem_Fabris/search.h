#ifndef SEARCH_EP1
#define SEARCH_EP1

#include <stdlib.h>
#include <stdio.h>
#define SEARCH_SUCCESS 1
#define SEARCH_FIND    2
#define SEARCH_FAILURE -1

/* Binary Search Tree */

typedef struct Element {
    void *value;
    void *key;
    struct Element *left;
    struct Element *right;
} Element;

typedef struct Search {
    ssize_t size;
    struct Element *start;
    /* Function compare returns:
     * - positive value if the first key is bigger than the second key
     * - zero if the keys are equal
     * - negative value if the first key is less than the second key
     */
    int (*compare)(void *, void *);
    void (*freeValue)(void *);
    void (*freeKey)(void *);
} Search;

struct Search *searchCreate (int (*compare)(void *, void *), void (*freeValue)(void *), void (*freeKey)(void *));
ssize_t searchSize (struct Search *search);
int searchIsEmpty (struct Search *search);
int searchAdd (struct Search *search, void *key, void *value);
void searchDelete (struct Search *search, void *key);
void *searchFind (struct Search *search, void *key);
void searchPurge (struct Search *search);
void searchApplyFunctionForEachElement (Search *search, void (*function)(void *, void *), void *data);


#endif
