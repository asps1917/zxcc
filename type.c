#include "zxcc.h"

Type *char_type = &(Type){CHAR, 1, 1};
Type *short_type = &(Type){SHORT, 2, 2};
Type *int_type = &(Type){INT, 4, 4};
Type *long_type = &(Type){LONG, 8, 8};

bool is_integer(Type *type) {
    TypeKind kind = type->ty;
    return (kind == CHAR || kind == SHORT || kind == INT || kind == LONG);
}

// nをalignでアライメントする
int align_to(int n, int align) { return (n + align - 1) & ~(align - 1); }

static Type *new_type(TypeKind kind, int size, int align) {
    Type *ty = calloc(1, sizeof(Type));
    ty->ty = kind;
    ty->size = size;
    ty->align = align;
    return ty;
}

// 引数baseに対するポインタ型を返す。
Type *pointer_to(Type *base) {
    Type *ty = new_type(PTR, 8, 8);
    ty->ptr_to = base;
    return ty;
}

// 引数baseに対する配列型を返す。
Type *array_of(Type *base, int len) {
    Type *ty = new_type(ARRAY, base->size * len, base->align);
    ty->ptr_to = base;
    ty->array_len = len;
    return ty;
}

// 引数nodeと子ノードに対して、そのnodeを評価した結果適用される型をセットする。
// 例: "1 + 1"を表すnodeには整数型がセットされる。
//     "&x + 1"を表すnodeにはポインタ型がセットされる。
void add_type(Node *node) {
    if(!node || node->type) return;

    add_type(node->lhs);
    add_type(node->rhs);
    add_type(node->cond);
    add_type(node->then);
    add_type(node->els);
    add_type(node->init);
    add_type(node->post);

    for(Node *n = node->block; n; n = n->next) add_type(n);
    for(Node *n = node->args; n; n = n->next) add_type(n);

    switch(node->kind) {
        case ND_ADD:
        case ND_SUB:
        case ND_PTR_DIFF:
        case ND_MUL:
        case ND_DIV:
        case ND_EQ:
        case ND_NE:
        case ND_LT:
        case ND_LE:
        case ND_FUNCCALL:
        case ND_NUM:
            node->type = long_type;
            return;
        case ND_PTR_ADD:
        case ND_PTR_SUB:
        case ND_ASSIGN:
            node->type = node->lhs->type;
            return;
        case ND_VAR:
            node->type = node->var->type;
            return;
        case ND_MEMBER:
            node->type = node->member->ty;
            return;
        case ND_ADDR:
            if(node->lhs->type->ty == ARRAY) {
                node->type = pointer_to(node->lhs->type->ptr_to);
            } else {
                node->type = pointer_to(node->lhs->type);
            }
            return;
        case ND_DEREF:
            if(!node->lhs->type->ptr_to) {
                error("無効なデリファレンスです");
            }
            node->type = node->lhs->type->ptr_to;
            return;
        case ND_STMT_EXPR: {
            Node *last = node->block;
            while(last->next) {
                last = last->next;
            }
            node->type = last->type;
            return;
        }
    }
}
