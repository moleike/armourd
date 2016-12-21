#ifndef _ARMOUR_LIST_H
#define _ARMOUR_LIST_H

#include <string.h>

#define LIST_INIT(head)                     \
    head = NULL

#define LIST_ADD(head, item)                \
({                                          \
    typeof (item) _p = item;                \
    _p->next = head;                        \
    head = _p;                              \
})

#define LIST_REM(head, item)                \
({                                          \
    typeof (head) *_pp = &head;             \
    typeof (item) _p = *_pp;                \
    while (_p != NULL) {                    \
        if (_p == item) {                   \
            *_pp = _p->next;                \
            _p->next = NULL;                \
            break;                          \
        }                                   \
        _pp = &_p->next;                    \
        _p = _p->next;                      \
    }                                       \
    _p;                                     \
})

#define LIST_FOREACH(head, p)               \
    for (p = head;                          \
         p != NULL;                         \
         p = p->next)

#define LIST_FOREACH_SAFE(head, p)          \
    for (typeof(p) _q,                      \
         p = head;                          \
         p != NULL &&                       \
         (_q = p->next, 1);                 \
         p = _q)

#define LIST_LOOKUP_STR(head, member, str)  \
({                                          \
    typeof (head) _p = (head);              \
    const char *_str = (str);               \
    while (_p != NULL) {                    \
        if ((strcmp (_str,                  \
            _p->member) == 0))              \
            break;                          \
        _p = _p->next;                      \
    }                                       \
    _p;                                     \
})

#define LIST_LOOKUP(head, member, val)      \
({                                          \
    typeof (head) _p = (head);              \
    typeof (val) _val = (val);              \
    while (_p != NULL) {                    \
        if (_p->member == _val)             \
            break;                          \
        _p = _p->next;                      \
    }                                       \
    _p;                                     \
})

#endif /* _ARMOUR_LIST_H */

