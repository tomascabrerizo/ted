#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef int64_t s64;
typedef int32_t s32;
typedef int16_t s16;
typedef int8_t s8;

typedef float f32;
typedef double f64;
typedef u32 b32;

typedef size_t sz;

#define array_len(v) (sizeof((v)) / sizeof((v)[0]))
#define is_power_of_two(expr) (((expr) & (expr - 1)) == 0)
#define unused(v) ((void)(v))

#define kb(x) ((x) * 1024ll)
#define mb(x) (kb(x) * 1024ll)
#define gb(x) (mb(x) * 1024ll)

#define clamp(a, b, c) max(min(a, c), b)

typedef struct Arena {
    void *data;
    size_t used;
    size_t size;
} Arena;

Arena arena_create(size_t size) {
    Arena arena;
    arena.data = malloc(size);
    arena.used = 0;
    arena.size = size;
    return arena;
}

void *arena_push(Arena *arena, size_t size, size_t align) {
    assert(is_power_of_two(align));
    uintptr_t unaligned_addr = (uintptr_t)arena->data + arena->used;
    uintptr_t aligned_addr   = (unaligned_addr + (align - 1)) & ~(align - 1);
    assert(aligned_addr % align == 0);
    size_t current_used = (aligned_addr - (uintptr_t)arena->data);
    size_t total_used   = current_used + size;
    assert(total_used <= arena->size);
    arena->used  = total_used;
    void *result = (void *)aligned_addr;
    memset(result, 0, size);
    return result;
}

void arena_clear(Arena *arena) {
    arena->used = 0;
}

typedef struct Str8 {
    u8 *data;
    sz size;
} Str8;

#define str8(str) str8_((u8 *)(str), strlen((str)))
Str8 str8_(u8 *str, sz size) {
    return (Str8){ str, size };
}

#define str8_alloc(arena, str) str8_alloc_((arena), (u8 *)(str), strlen((str)))
Str8 str8_alloc_(Arena *arena, u8 *str, sz size) {
    Str8 result = { (u8 *)arena_push(arena, size, 8), size };
    memcpy(result.data, str, size);
    return result;
}

Str8 str8_copy(Arena *arena, Str8 str) {
    Str8 result = str8_alloc_(arena, str.data, str.size);
    return result;
}

Str8 str8_split(Arena *arena, Str8 *str, sz index) {
    assert(index <= str->size);
    Str8 res  = str8_alloc_(arena, str->data, index);
    str->size = str->size - index;
    str->data += index;
    return res;
}

char *str8_c(Arena *arena, Str8 str) {
    char *buffer     = (char *)arena_push(arena, str.size + 1, 8);
    buffer[str.size] = 0;
    return buffer;
}

void str8_print(Str8 str) {
    printf("%.*s\n", (int)str.size, (char *)str.data);
}

typedef enum RopeType { ROPE_TYPE_COUNT, ROPE_TYPE_STR8 } RopeType;

// TODO: Convert to self balancing binary tree (red black tree or something)
typedef struct Rope {
    RopeType type;
    union {
        Str8 str;
        sz count;
    };
    struct Rope *p;
    struct Rope *l;
    struct Rope *r;
} Rope;

bool rope_leaf(Rope *rop) {
    return (rop->type == ROPE_TYPE_STR8);
}

sz rope_count(Rope *rop) {
    sz count = 0;
    if(rope_leaf(rop)) {
        count += rop->str.size;
    } else {
        count += rope_count(rop->l);
        count += rope_count(rop->r);
    }
    return count;
}

static inline bool rope_is_l(Rope *rop) {
    if(rop->p) {
        return rop->p->l == rop;
    }
    return false;
}

static inline bool rope_is_r(Rope *rop) {
    if(rop->p) {
        return rop->p->r == rop;
    }
    return false;
}

static inline void rope_set_l(Rope *rut, Rope *rop) {
    assert(rut->type == ROPE_TYPE_COUNT);
    rop->p = rut;
    rut->l = rop;
}

static inline void rope_set_r(Rope *rut, Rope *rop) {
    assert(rut->type == ROPE_TYPE_COUNT);
    rop->p = rut;
    rut->r = rop;
}

Rope *rope_alloc(Arena *arena) {
    Rope *res = (Rope *)arena_push(arena, sizeof(*res), 8);
    res->p    = 0;
    return res;
}

Rope *rope_alloc_count(Arena *arena, sz count) {
    Rope *res  = rope_alloc(arena);
    res->type  = ROPE_TYPE_COUNT;
    res->count = count;
    res->l     = 0;
    res->r     = 0;
    return res;
}

Rope *rope_alloc_str8(Arena *arena, Str8 str) {
    Rope *res = rope_alloc(arena);
    res->type = ROPE_TYPE_STR8;
    res->str  = str;
    res->l    = 0;
    res->r    = 0;
    return res;
}

char rope_index(Rope *rop, sz index) {
    if(rope_leaf(rop)) {
        return (char)rop->str.data[index];
    } else {
        if(index >= rop->count) {
            return rope_index(rop->r, index - rop->count);
        } else {
            return rope_index(rop->l, index);
        }
    }
}

Rope *rope_insert(Arena *arena, Rope *rop, Str8 str) {
    Rope *rut = rope_alloc_count(arena, rope_count(rop));
    Rope *res = rope_alloc_str8(arena, str);
    rope_set_l(rut, rop);
    rope_set_r(rut, res);
    return rut;
}

Rope *rope_concat(Arena *arena, Rope *r0, Rope *r1) {
    Rope *res = rope_alloc_count(arena, rope_count(r0));
    rope_set_l(res, r0);
    rope_set_r(res, r1);
    return res;
}

void rope_split(Arena *arena, Rope *rop, sz index) {
    if(rope_leaf(rop)) {

        if(index == 0) {
            return;
        }

        assert(index < rop->str.size);

        Str8 str = str8_split(arena, &rop->str, index);

        Rope *rut = rope_alloc_count(arena, str.size);
        Rope *new = rope_alloc_str8(arena, str);

        if(rope_is_l(rop)) {
            rope_set_l(rop->p, rut);
        } else if(rope_is_r(rop)) {
            rope_set_r(rop->p, rut);
        } else {
            assert(!"invalid code path");
        }

        rope_set_l(rut, new);
        rope_set_r(rut, rop);

    } else {
        if(index >= rop->count) {
            rope_split(arena, rop->r, index - rop->count);
        } else {
            rope_split(arena, rop->l, index);
        }
    }
}

Rope *rope_rebalance(Rope *rope) {
    return rope;
}

void rope_print(Rope *rop) {
    if(rope_leaf(rop)) {
        str8_print(rop->str);
    } else {
        printf("count: %d\n", (u32)rop->count);
        rope_print(rop->l);
        rope_print(rop->r);
    }
}

typedef enum RbColor { RB_BLACK, RB_RED } RbColor;

typedef struct RbNode {
    u32 index;
    RbColor color;
    struct RbNode *p;
    struct RbNode *l;
    struct RbNode *r;
} RbNode;

void rb_rotate_l(RbNode **rut, RbNode *nod) {
    assert(nod->r);

    RbNode *x = nod;
    RbNode *y = x->r;
    y->p      = x->p;

    if(y->l) {
        y->l->p = x;
    }

    if(!x->p) {
        *rut = y;
    }

    if(x == x->p->l) {
        x->p->l = y;
    } else {
        x->p->r = y;
    }

    y->l = x;
    x->p = y;
}

void rb_rotate_r(RbNode **rut, RbNode *nod) {
    assert(nod->l);

    RbNode *x = nod;
    RbNode *y = x->l;
    y->p      = x->p;

    if(y->r) {
        y->r->p = x;
    }

    if(!x->p) {
        *rut = y;
    }

    if(x == x->p->l) {
        x->p->l = y;
    } else {
        x->p->r = y;
    }

    y->r = x;
    x->p = y;
}

void rb_insert_fix(RbNode **rut, RbNode *nod) {

    while(nod->p && nod->p->color == RB_RED) {
        if(nod->p->p && nod->p == nod->p->p->l) {
            RbNode *y = nod->p->p->r;
            if(y->color == RB_RED) {
                nod->p->color    = RB_BLACK;
                y->color         = RB_BLACK;
                nod->p->p->color = RB_RED;
                nod              = nod->p->p;
            } else if(nod == nod->p->r) {
                nod = nod->p;
                rb_rotate_l(rut, nod);
            }
            nod->p->color    = RB_BLACK;
            nod->p->p->color = RB_RED;
            rb_rotate_r(rut, nod->p->p);
        } else {
            RbNode *y = nod->p->p->l;
            if(y->color == RB_RED) {
                nod->p->color    = RB_BLACK;
                y->color         = RB_BLACK;
                nod->p->p->color = RB_RED;
                nod              = nod->p->p;
            } else if(nod == nod->p->l) {
                nod = nod->p;
                rb_rotate_r(rut, nod);
            }
            nod->p->color    = RB_BLACK;
            nod->p->p->color = RB_RED;
            rb_rotate_l(rut, nod->p->p);
        }
    }

    (*rut)->color = RB_BLACK;
}

void rb_insert(RbNode **rut, RbNode *nod) {
    RbNode *p = 0;
    RbNode *x = *rut;

    while(x) {
        p = x;
        if(nod->index < x->index) {
            x = x->l;
        } else {
            x = x->r;
        }
    }

    nod->p = p;

    if(!p) {
        *rut = nod;
    } else if(nod->index < p->index) {
        p->l = nod;
    } else {
        p->r = nod;
    }

    nod->l     = 0;
    nod->r     = 0;
    nod->color = RB_RED;

    rb_insert_fix(rut, nod);
}

RbNode *rb_remove(RbNode *rut, RbNode *nod) {
    unused(rut);
    unused(nod);
    return 0;
}

int main(void) {

    Arena arena = arena_create(mb(124));

    Rope *r0 = rope_alloc_str8(&arena, str8("Hello, "));
    Rope *r1 = rope_alloc_str8(&arena, str8("World!"));

    Rope *rut = rope_concat(&arena, r0, r1);

    rope_print(rut);
    rope_split(&arena, rut, 7);
    printf("-----------------------------\n");
    rope_print(rut);
    rope_split(&arena, rut, 5);
    printf("-----------------------------\n");
    rope_print(rut);

    printf("memory used: %d\n", (u32)arena.used);

    printf("-----------------------------\n");
#if 1
    {
        u32 count = (u32)rope_count(rut);
        printf("root count: %d\n", count);
        for(u32 i = 0; i < count; ++i) {
            char c = rope_index(rut, i);
            printf("%c", c);
        }
    }
    printf("\n");
#endif

    return 0;
}
