#define LIST_HEAD(name, type)                                                  \
struct name {			                                               \
    type *e_first;                                                             \
}

#define LIST_HEAD_INITIALIZER {NULL}
#define LIST_HEAD_INIT(a) (a)->e_first=NULL

#define LIST_ENTRY(type)                                                       \
struct {                                                                       \
    type *e_next;                                                              \
    type **e_prev;                                                             \
}

#define LIST_INIT(head)                                                        \
do { (head)->e_first = NULL } while (0)

#define LIST_INSERT_AFTER(this_e, new_e, field)                                \
do {                                                                           \
    if (((new_e)->field.e_next = (this_e)->field.e_next) != NULL)              \
        (this_e)->field.e_next->field.e_prev = &(new_e)->field.e_next;         \
    (this_e)->field.e_next = (new_e);                                          \
    (new_e)->field.e_prev = &(this_e)->field.e_next;                           \
} while (0)

#define LIST_INSERT_BEFORE(this_e, new_e, field)                               \
do {                                                                           \
    (new_e)->field.e_prev = (this_e)->field.e_prev;                            \
    (new_e)->field.e_next = (this_e);                                          \
    *(this_e)->field.e_prev = (new_e);                                         \
    (this_e)->field.e_prev = &(new_e)->field.e_next;                           \
} while (0)

#define LIST_INSERT_HEAD(head, new_e, field)                                   \
do {                                                                           \
    if (((new_e)->field.e_next = (head)->e_first) != NULL)                     \
        (head)->e_first->field.e_prev = &(new_e)->field.e_next;                \
    (head)->e_first = (new_e);                                                 \
    (new_e)->field.e_prev = &(head)->e_first;                                  \
} while (0)

#define LIST_REMOVE(this_e, field)                                             \
do {                                                                           \
    if ((this_e)->field.e_next != NULL)                                        \
        (this_e)->field.e_next->field.e_prev = (this_e)->field.e_prev;         \
    *(this_e)->field.e_prev = (this_e)->field.e_next;                          \
} while (0)

#define LIST_FOREACH(var, head, field)                                         \
    for ((var) = (head)->e_first; (var) != NULL; (var) = (var)->field.e_next)

#define LIST_FOREACH_SAFE(var, next, head, field)                              \
    for ((var) = (head)->e_first, (next) = (var) ? (var)->field.e_next : NULL; \
         (var) != NULL;                                                        \
         (var) = (next), (next) = (var) ? (var)->field.e_next : NULL)

#define LIST_EMPTY(head)            ((head)->e_first == NULL)
#define LIST_FIRST(head)            ((head)->e_first)
#define LIST_NEXT(this_e, field)    ((this_e)->field.e_next)

