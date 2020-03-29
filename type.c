#include "zxcc.h"

Type *void_type = &(Type){VOID, 1, 1};
Type *bool_type = &(Type){BOOL, 1, 1};
Type *char_type = &(Type){CHAR, 1, 1};
Type *short_type = &(Type){SHORT, 2, 2};
Type *int_type = &(Type){INT, 4, 4};
Type *long_type = &(Type){LONG, 8, 8};

bool is_integer(Type *type) {
    TypeKind kind = type->ty;
    return (kind == BOOL || kind == CHAR || kind == SHORT || kind == INT ||
            kind == LONG);
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

Type *func_type(Type *return_ty) {
    Type *ty = new_type(FUNC, 1, 1);
    ty->return_ty = return_ty;
    return ty;
}

Type *enum_type() { return new_type(ENUM, 4, 4); }

Type *struct_type() {
    Type *ty = new_type(STRUCT, 0, 1);
    ty->is_incomplete = true;
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
        case ND_BITAND:
        case ND_BITOR:
        case ND_BITXOR:
        case ND_EQ:
        case ND_NE:
        case ND_LT:
        case ND_LE:
        case ND_NUM:
        case ND_NOT:
        case ND_LOGOR:
        case ND_LOGAND:
            node->type = long_type;
            return;
        case ND_PTR_ADD:
        case ND_PTR_SUB:
        case ND_ASSIGN:
        case ND_SHL:
        case ND_SHR:
        case ND_PRE_INC:
        case ND_PRE_DEC:
        case ND_POST_INC:
        case ND_POST_DEC:
        case ND_ADD_EQ:
        case ND_PTR_ADD_EQ:
        case ND_SUB_EQ:
        case ND_PTR_SUB_EQ:
        case ND_MUL_EQ:
        case ND_DIV_EQ:
        case ND_SHL_EQ:
        case ND_SHR_EQ:
        case ND_BITNOT:
            node->type = node->lhs->type;
            return;
        case ND_VAR:
            node->type = node->var->type;
            return;
        case ND_COMMA:
            node->type = node->rhs->type;
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
        case ND_DEREF: {
            if(!node->lhs->type->ptr_to) {
                error("無効なデリファレンスです");
            }
            Type *ty = node->lhs->type->ptr_to;
            if(ty->ty == VOID) {
                error("void型へのポインタに対するデリファレンスです");
            }
            if(ty->ty == STRUCT && ty->is_incomplete) {
                error("不完全な構造体型です");
            }
            node->type = ty;
            return;
        }
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
