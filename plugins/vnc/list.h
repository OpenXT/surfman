/*
 * Copyright (c) 2011 Citrix Systems, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

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

