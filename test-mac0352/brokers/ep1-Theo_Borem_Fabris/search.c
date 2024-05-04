#include "search.h"

struct Search *searchCreate (int (*compare)(void *, void *), void (*freeValue)(void *), void (*freeKey)(void *))
{
    struct Search *search;

    if (compare == NULL
            || (search = malloc(sizeof(struct Search))) == NULL)
        return NULL;
    search->size = 0;
    search->start = NULL;
    search->compare = compare;
    search->freeValue = freeValue;
    search->freeKey = freeKey;
    return search;
}

ssize_t searchSize (struct Search *search)
{
    if (search == NULL)
        return -1;
    return search->size;
}

int searchIsEmpty (struct Search *search)
{
    if (search == NULL)
        return 0;
    return (search->size == 0);
}

struct Element *elementCreate (void *key, void *value, struct Element *left, struct Element *right)
{
    struct Element *new;
    if ((new = malloc(sizeof(struct Element))) == NULL)
        return NULL;
    new->key = key;
    new->value = value;
    new->left = left;
    new->right = right;
    return new;
}

int searchAdd (struct Search *search, void *key, void *value)
{
    struct Element *root, *child;
    int cmp;

    if (search == NULL || key == NULL)
        return SEARCH_FAILURE;

    if (search->start == NULL) {
        if ((search->start = elementCreate(key, value, NULL, NULL)) == NULL)
            return SEARCH_FAILURE;
        search->size++;
        return SEARCH_SUCCESS;
    }

    child = search->start;
    do {
        root = child;
        cmp = (*search->compare)(key, root->key);
        if (cmp == 0) {
            return SEARCH_FIND;
        }
        else if (cmp < 0)
            child = root->left;
        else
            child = root->right;
    } while (child != NULL);

    if ((child = elementCreate(key, value, NULL, NULL)) == NULL)
        return SEARCH_FAILURE;
    if (cmp < 0)
        root->left = child;
    else
        root->right = child;
    search->size++;
    return SEARCH_SUCCESS;
}

void *searchFind (struct Search *search, void *key)
{
    struct Element *root, *child;
    int cmp;

    if (key == NULL || search == NULL || search->start == NULL)
        return NULL;

    child = search->start;
    do {
        root = child;
        cmp = (*search->compare)(key, root->key);
        if (cmp == 0)
            return root->value;
        else if (cmp < 0)
            child = root->left;
        else
            child = root->right;
    } while (child != NULL);
    return child;
}

void elementPurge (struct Element *root, void (*freeValue)(void *), void (*freeKey)(void *))
{
    if (root == NULL)
        return ;
    elementPurge(root->left, freeValue, freeKey);
    elementPurge(root->right, freeValue, freeKey);
    if (freeValue != NULL) {
        (*freeValue)(root->value);
    }
    if (freeKey != NULL) {
        (*freeKey)(root->key);
    }
    free(root);
}

void searchPurge (struct Search *search)
{
    if (search != NULL)
        elementPurge(search->start, search->freeValue, search->freeKey);
    free(search);
}

struct Element *searchDeleteRec(struct Element *root, void *key, int (*compare)(void*, void*), void (*freeValue)(void*), void (*freeKey)(void*), int* removed)
{
    struct Element *child, *parent;
    int cmp;

    if (root == NULL)
        return NULL;
    cmp = (*compare)(key, root->key);
    if (cmp < 0) {
        root->left = searchDeleteRec(root->left, key, compare, freeValue, freeKey, removed);
        return root;
    }
    if (cmp > 0) {
        root->right = searchDeleteRec(root->right, key, compare, freeValue, freeKey, removed);
        return root;
    }
    if (root->left == NULL || root->right == NULL) {
        if (root->left != NULL && root->right == NULL) {
            child = root->left; root->left = NULL;
        }
        else if (root->right != NULL && root->left == NULL) {
            child = root->right; root->right = NULL;
        }
        else {
            child = NULL;
        }
        elementPurge(root, freeValue, freeKey);
        *removed = 1;
        return child;
    }
    /* find the smallest element of the right subtree */ 
    if (root->right->left == NULL) {
        child = root->right;
        child->left = root->left;
    }
    else {
        parent = root->right;
        child = root->right->left;
        while (child->left != NULL) {
            parent = child;
            child = child->left;
        }
        child->left = root->left;
        parent->left = child->right;
        child->right = root->right;
    }
    root->right = NULL;
    root->left = NULL;
    *removed = 1;
    elementPurge(root, freeValue, freeKey);
    return child;
}

void searchDelete (struct Search *search, void *key)
{
    int removed = 0;
    if (key == NULL || search == NULL)
        return ;
    search->start = searchDeleteRec(search->start, key, search->compare, search->freeValue, search->freeKey, &removed);
    if (removed == 1)
        search->size--;
}

void searchApplyFunctionForEachElementRec (Element *root, void (*function)(void *, void *), void *data)
{
    if (root == NULL)
        return ;
    (*function)(root->value, data);
    searchApplyFunctionForEachElementRec (root->left, function, data);
    searchApplyFunctionForEachElementRec (root->right, function, data);
}

void searchApplyFunctionForEachElement (Search *search, void (*function)(void *, void *), void *data)
{
    if (search != NULL)
        searchApplyFunctionForEachElementRec (search->start, function, data);
}
