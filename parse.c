#include "zxcc.h"

// ローカル・グローバル変数、typedef、enumのスコープ
typedef struct VarScope VarScope;
struct VarScope {
    VarScope *next;
    char *name;
    int depth;

    Var *var;
    Type *type_def;
    Type *enum_ty;
    int enum_val;
};

// 構造体タグ、enumタグのスコープ
typedef struct TagScope TagScope;
struct TagScope {
    TagScope *next;
    char *name;
    int depth;
    Type *ty;
};

typedef struct {
    VarScope *var_scope;
    TagScope *tag_scope;
} Scope;

// パース処理中に現れたローカル変数を追加するための連結リスト
static VarList *locals;
// パース処理中に現れたグローバル変数を追加するための連結リスト
static VarList *globals;

// 変数、typedefのスコープ
static VarScope *var_scope;
// 構造体タグ、enumタグのスコープ
static TagScope *tag_scope;
static int scope_depth;

// switch文のパース中にswitchノードへのポインタを保持する変数
static Node *current_switch;

// ブロックスコープの開始処理
static Scope *enter_scope(void) {
    Scope *sc = calloc(1, sizeof(Scope));
    sc->var_scope = var_scope;
    sc->tag_scope = tag_scope;
    scope_depth++;
    return sc;
}

// ブロックスコープの終了処理
static void leave_scope(Scope *sc) {
    var_scope = sc->var_scope;
    tag_scope = sc->tag_scope;
    scope_depth--;
}

// 変数、typedefを名前で検索する。検索対象はローカル変数リスト→グローバル変数リストの順番。
// 見つからなかった場合はNULLを返す。
static VarScope *find_var(Token *tok) {
    for(VarScope *sc = var_scope; sc; sc = sc->next) {
        if(strlen(sc->name) == tok->len &&
           !strncmp(tok->str, sc->name, tok->len)) {
            return sc;
        }
    }
    return NULL;
}

// 構造体タグを名前で検索する。見つからなかった場合はNULLを返す。
static TagScope *find_tag(Token *tok) {
    for(TagScope *sc = tag_scope; sc; sc = sc->next) {
        if(strlen(sc->name) == tok->len &&
           !strncmp(tok->str, sc->name, tok->len)) {
            return sc;
        }
    }
    return NULL;
}

static VarScope *push_scope(char *name) {
    VarScope *sc = calloc(1, sizeof(VarScope));
    sc->name = name;
    sc->next = var_scope;
    sc->depth = scope_depth;
    var_scope = sc;
    return sc;
}

// 引数として与えられた変数名のVar構造体を生成する
static Var *new_var(char *name, Type *type, bool is_local) {
    Var *var = calloc(1, sizeof(Var));
    var->name = name;
    var->type = type;
    var->is_local = is_local;
    return var;
}

// 引数として与えられた変数名のVar構造体を生成する。
// 生成したVar構造体はlocalsリストに追加される。
static Var *new_lvar(char *name, Type *type) {
    Var *lvar = new_var(name, type, true);
    push_scope(name)->var = lvar;

    VarList *vl = calloc(1, sizeof(VarList));
    vl->var = lvar;
    vl->next = locals;
    locals = vl;
    return lvar;
}

// 引数として与えられた変数名のVar構造体を生成する。
// 生成したVar構造体はglobalsリストに追加される。
static Var *new_gvar(char *name, Type *type, bool emit) {
    Var *gvar = new_var(name, type, false);
    push_scope(name)->var = gvar;

    if(emit) {
        VarList *vl = calloc(1, sizeof(VarList));
        vl->var = gvar;
        vl->next = globals;
        globals = vl;
    }

    return gvar;
}

static Type *find_typedef(Token *tok) {
    if(tok->kind == TK_IDENT) {
        VarScope *sc = find_var(tok);
        if(sc) {
            return sc->type_def;
        }
    }
    return NULL;
}

static Node *alloc_node(NodeKind kind) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    return node;
}

static Node *new_binary(NodeKind kind, Node *lhs, Node *rhs) {
    Node *node = alloc_node(kind);
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

static Node *new_unary(NodeKind kind, Node *expr) {
    Node *node = alloc_node(kind);
    node->lhs = expr;
    return node;
}

static Node *new_node_num(int val) {
    Node *node = alloc_node(ND_NUM);
    node->val = val;
    return node;
}

static Node *new_var_node(Var *var) {
    Node *node = alloc_node(ND_VAR);
    node->var = var;
    return node;
}

// 文字列リテラル用のラベルを生成する
static char *new_label(void) {
    static int cnt = 0;
    char buf[20];
    sprintf(buf, ".L.data.%d", cnt++);
    return strndup(buf, 20);
}

typedef enum {
    TYPEDEF = 1 << 0,
    STATIC = 1 << 1,
} StorageClass;

static Type *basetype(StorageClass *sclass);
static bool is_typename();
static Function *function();
static Type *declarator(Type *ty, char **name);
static Type *abstract_declarator(Type *ty);
static Type *type_suffix(Type *ty);
static Type *type_name();
static Type *struct_decl();
static Type *enum_specifier();
static Member *struct_member();
static void global_var();
static Node *declaration();
static Node *stmt();
static Node *stmt2();
static Node *expr();
static long const_expr();
static Node *assign();
static Node *conditional();
static Node *logor();
static Node *logand();
static Node *bitand();
static Node * bitor ();
static Node *bitxor();
static Node *equality();
static Node *relational();
static Node *shift();
static Node *new_add(Node *lhs, Node *rhs);
static Node *add();
static Node *mul();
static Node *cast();
static Node *unary();
static Node *postfix();
static Node *primary();

// 現在のトークンが関数か判定する
static bool is_function() {
    Token *cur = token;

    StorageClass sclass;
    Type *ty = basetype(&sclass);
    char *name = NULL;
    declarator(ty, &name);
    bool retval = name && consume("(");

    token = cur;
    return retval;
}

// program = (global_var | function)*
Program *program() {
    Function head = {};
    Function *cur = &head;
    globals = NULL;

    while(!at_eof()) {
        if(is_function()) {
            Function *func = function();
            if(!func) {
                continue;
            }
            cur->next = func;
            cur = cur->next;
            continue;
        }

        global_var();
    }

    Program *prog = calloc(1, sizeof(Program));
    prog->funcs = head.next;
    prog->globals = globals;
    return prog;
}

// basetype = builtin-type | struct-decl | typedef-name | enum-specifier
// builtin-type   = "void" | "_Bool" | "char" | "short" | "int" | "long"
//                | "long" "long"
// パースした型を表すType構造体へのポインタを返す
static Type *basetype(StorageClass *sclass) {
    if(!is_typename()) {
        error("型名ではありません");
    }

    enum {
        VOID = 1 << 0,
        BOOL = 1 << 2,
        CHAR = 1 << 4,
        SHORT = 1 << 6,
        INT = 1 << 8,
        LONG = 1 << 10,
        OTHER = 1 << 12,
    };

    Type *ty = int_type;
    int counter = 0;

    if(sclass) {
        *sclass = 0;
    }

    while(is_typename()) {
        Token *tok = token;

        // 記憶クラス指定子の処理
        if(match("typedef") || match("static")) {
            if(!sclass) {
                error("記憶クラス指定子は許可されていません");
            }

            if(consume("typedef")) {
                *sclass |= TYPEDEF;
            } else if(consume("static")) {
                *sclass |= STATIC;
            }

            if(*sclass & (*sclass - 1)) {
                error("typedefとstaticは同時に使用できません");
            }
            continue;
        }

        // ユーザが定義した型の処理
        if(!match("void") && !match("_Bool") && !match("char") &&
           !match("short") && !match("int") && !match("long")) {
            if(counter) {
                break;
            }

            if(match("struct")) {
                ty = struct_decl();
            } else if(match("enum")) {
                ty = enum_specifier();
            } else {
                ty = find_typedef(token);
                assert(ty);
                token = token->next;
            }

            counter |= OTHER;
            continue;
        }

        // 組み込み型の処理
        if(consume("void")) {
            counter += VOID;
        } else if(consume("_Bool")) {
            counter += BOOL;
        } else if(consume("char")) {
            counter += CHAR;
        } else if(consume("short")) {
            counter += SHORT;
        } else if(consume("int")) {
            counter += INT;
        } else if(consume("long")) {
            counter += LONG;
        }

        switch(counter) {
            case VOID:
                ty = void_type;
                break;
            case BOOL:
                ty = bool_type;
                break;
            case CHAR:
                ty = char_type;
                break;
            case SHORT:
            case SHORT + INT:
                ty = short_type;
                break;
            case INT:
                ty = int_type;
                break;
            case LONG:
            case LONG + INT:
            case LONG + LONG:
            case LONG + LONG + INT:
                ty = long_type;
                break;
            default:
                error("無効な型です");
        }
    }

    return ty;
}

// declarator = "*"* ("(" declarator ")" | ident) type-suffix
static Type *declarator(Type *ty, char **name) {
    while(consume("*")) {
        ty = pointer_to(ty);
    }

    if(consume("(")) {
        Type *placeholder = calloc(1, sizeof(Type));
        Type *new_ty = declarator(placeholder, name);
        expect(")");
        memcpy(placeholder, type_suffix(ty), sizeof(Type));
        return new_ty;
    }

    *name = expect_ident();
    return type_suffix(ty);
}

// abstract-declarator = "*"* ("(" abstract-declarator ")")? type-suffix
static Type *abstract_declarator(Type *ty) {
    while(consume("*")) {
        ty = pointer_to(ty);
    }

    if(consume("(")) {
        Type *placeholder = calloc(1, sizeof(Type));
        Type *new_ty = abstract_declarator(placeholder);
        expect(")");
        memcpy(placeholder, type_suffix(ty), sizeof(Type));
        return new_ty;
    }
    return type_suffix(ty);
}

// type-suffix = ("[" const-expr? "]" type-suffix)?
// 変数宣言の型名のsuffix([])を読み取る
static Type *type_suffix(Type *ty) {
    if(!consume("[")) {
        return ty;
    }

    int sz = 0;
    bool is_incomplete = true;
    if(!consume("]")) {
        sz = const_expr();
        is_incomplete = false;
        expect("]");
    }

    ty = type_suffix(ty);
    if(ty->is_incomplete) {
        error("不完全な型です");
    }

    ty = array_of(ty, sz);
    ty->is_incomplete = is_incomplete;
    return ty;
}

// type-name = basetype abstract-declarator type-suffix
static Type *type_name() {
    Type *ty = basetype(NULL);
    ty = abstract_declarator(ty);
    return type_suffix(ty);
}

static void push_tag_scope(Token *tok, Type *ty) {
    TagScope *sc = calloc(1, sizeof(TagScope));
    sc->next = tag_scope;
    sc->name = strndup(tok->str, tok->len);
    sc->depth = scope_depth;
    sc->ty = ty;
    tag_scope = sc;
}

// struct-decl = "struct" ident? ("{" struct-member "}")?
static Type *struct_decl(void) {
    // 構造体タグの読み出し
    expect("struct");
    Token *tag = consume_ident();
    if(tag && !match("{")) {
        TagScope *sc = find_tag(tag);
        if(!sc) {
            Type *ty = struct_type();
            push_tag_scope(tag, ty);
            return ty;
        }
        if(sc->ty->ty != STRUCT) {
            error("構造体タグではありません");
        }
        return sc->ty;
    }

    if(!consume("{")) {
        return struct_type();
    }

    Type *ty;
    TagScope *sc = NULL;
    if(tag) {
        sc = find_tag(tag);
    }

    if(sc && sc->depth == scope_depth) {
        // 構造体の再定義
        if(sc->ty->ty != STRUCT) {
            error("構造体タグではありません");
        }
        ty = sc->ty;
    } else {
        // 構造体型を不完全な型として登録する
        ty = struct_type();
        if(tag) {
            push_tag_scope(tag, ty);
        }
    }

    // 構造体メンバの読み出し
    Member head = {};
    Member *cur = &head;

    while(!consume("}")) {
        cur->next = struct_member();
        cur = cur->next;
    }

    ty->members = head.next;

    // 構造体メンバへのoffset割り当て
    int offset = 0;
    for(Member *mem = ty->members; mem; mem = mem->next) {
        if(mem->ty->is_incomplete) {
            error("構造体メンバが不完全です");
        }
        offset = align_to(offset, mem->ty->align);
        mem->offset = offset;
        offset += mem->ty->size;

        if(ty->align < mem->ty->align) {
            ty->align = mem->ty->align;
        }
    }
    ty->size = align_to(offset, ty->align);

    ty->is_incomplete = false;
    return ty;
}

// パース中のトークンがenumのリストの末尾だった場合trueを返す。
static bool consume_end() {
    Token *tok = token;
    if(consume("}") || (consume(",") && consume("}"))) {
        return true;
    }
    token = tok;
    return false;
}

static bool peek_end() {
    Token *tok = token;
    bool ret = consume("}") || (consume(",") && consume("}"));
    token = tok;
    return ret;
}

static void expect_end() {
    if(!consume_end()) {
        expect("}");
    }
}

// enum-specifier = "enum" ident
//                | "enum" ident? "{" enum-list? "}"
//
// enum-list = enum-elem ("," enum-elem)* ","?
// enum-elem = ident ("=" const-expr)?
static Type *enum_specifier() {
    expect("enum");
    Type *ty = enum_type();

    // enumタグの読み出し
    Token *tag = consume_ident();
    if(tag && !match("{")) {
        TagScope *sc = find_tag(tag);
        if(!sc) {
            error("未定義のenum型です");
        }
        if(sc->ty->ty != ENUM) {
            error("enumタグではありません");
        }
        return sc->ty;
    }

    expect("{");

    // enumのリストを読み出す
    int cnt = 0;
    for(;;) {
        char *name = expect_ident();
        if(consume("=")) {
            cnt = const_expr();
        }

        VarScope *sc = push_scope(name);
        sc->enum_ty = ty;
        sc->enum_val = cnt++;

        if(consume_end()) {
            break;
        }
        expect(",");
    }

    if(tag) {
        push_tag_scope(tag, ty);
    }
    return ty;
}

// struct-member = basetype declarator type-suffix ";"
static Member *struct_member(void) {
    Type *ty = basetype(NULL);
    Token *tok = token;
    char *name = NULL;
    ty = declarator(ty, &name);
    ty = type_suffix(ty);
    expect(";");

    Member *mem = calloc(1, sizeof(Member));
    mem->name = name;
    mem->ty = ty;
    mem->tok = tok;
    return mem;
}

// params   =
// basetype declarator type-suffix ("," basetype declarator type-suffix)*
static VarList *params() {
    VarList *head = calloc(1, sizeof(VarList));
    head->var = calloc(1, sizeof(Var));
    VarList *cur = head;

    while(1) {
        Type *type = basetype(NULL);
        char *var_name = NULL;
        type = declarator(type, &var_name);
        type = type_suffix(type);

        // 引数中の"T型の配列"を"T型へのポインタ"に変換する
        // 例: *argv[] → **argv
        if(type->ty == ARRAY) {
            type = pointer_to(type->ptr_to);
        }

        // identを以下のVarListに追加
        // * 関数定義内の引数リスト
        // * locals(ローカル変数リスト)
        Var *lvar = new_lvar(var_name, type);
        cur->next = calloc(1, sizeof(VarList));
        cur->next->var = lvar;
        cur = cur->next;

        if(!consume(",")) {
            return head->next;
        }
    }
}

// function = basetype declarator "(" params? ")" ("{" stmt* "}" | ";")
static Function *function() {
    locals = NULL;

    StorageClass sclass;
    Type *ty = basetype(&sclass);
    char *name = NULL;
    ty = declarator(ty, &name);

    // 関数の型をスコープに追加する
    new_gvar(name, func_type(ty), false);

    // 関数オブジェクトを生成
    Function *func = calloc(1, sizeof(Function));
    func->name = name;
    func->is_static = (sclass == STATIC);

    expect("(");

    Scope *sc = enter_scope();
    if(!consume(")")) {
        func->args = params();
        expect(")");
    }

    if(consume(";")) {
        leave_scope(sc);
        return NULL;
    }

    // 関数本体の読み取り
    Node head = {};
    Node *cur = &head;
    expect("{");
    // stmt*
    while(!consume("}")) {
        cur->next = stmt();
        cur = cur->next;
    }
    leave_scope(sc);

    func->node = head.next;
    func->locals = locals;
    return func;
}

// global-var = basetype declarator type-suffix ";"
static void global_var() {
    StorageClass sclass;
    Type *type = basetype(&sclass);
    char *var_name = NULL;
    type = declarator(type, &var_name);
    type = type_suffix(type);
    expect(";");

    if(sclass == TYPEDEF) {
        push_scope(strndup(var_name, strlen(var_name)))->type_def = type;
    } else {
        if(type->is_incomplete) {
            error("不完全な型です");
        }
        // global変数に定義した変数を追加
        new_gvar(strndup(var_name, strlen(var_name)), type, true);
    }
}

typedef struct Designator Designator;
struct Designator {
    Designator *next;
    int idx;
};

// 配列へのアクセスに相当するノードを生成する。
// 例: var=x, desg=3,4の場合、x[3][4]に相当するノードをこの関数は返す
static Node *new_desg_node2(Var *var, Designator *desg) {
    if(!desg) {
        return new_var_node(var);
    }

    Node *node = new_desg_node2(var, desg->next);
    node = new_add(node, new_node_num(desg->idx));
    return new_unary(ND_DEREF, node);
}

static Node *new_desg_node(Var *var, Designator *desg, Node *rhs) {
    Node *lhs = new_desg_node2(var, desg);
    Node *node = new_binary(ND_ASSIGN, lhs, rhs);
    return new_unary(ND_EXPR_STMT, node);
}

static Node *lvar_init_zero(Node *cur, Var *var, Type *ty, Designator *desg) {
    if(ty->ty == ARRAY) {
        for(int i = 0; i < ty->array_len; i++) {
            Designator desg2 = {desg, i++};
            cur = lvar_init_zero(cur, var, ty->ptr_to, &desg2);
        }
        return cur;
    }

    cur->next = new_desg_node(var, desg, new_node_num(0));
    return cur->next;
}

// lvar-initializer2 = assign
// | "{" (lvar-initializer2 ("," lvar-initializer2)* ","?)? "}"
static Node *lvar_initializer2(Node *cur, Var *var, Type *ty,
                               Designator *desg) {
    if(ty->ty == ARRAY) {
        expect("{");
        int i = 0;

        if(!match("}")) {
            do {
                Designator desg2 = {desg, i++};
                cur = lvar_initializer2(cur, var, ty->ptr_to, &desg2);
            } while(!peek_end() && consume(","));
        }
        expect_end();

        // 余った配列要素にゼロをセットする
        while(i < ty->array_len) {
            Designator desg2 = {desg, i++};
            cur = lvar_init_zero(cur, var, ty->ptr_to, &desg2);
        }

        return cur;
    }

    cur->next = new_desg_node(var, desg, assign());
    return cur->next;
}

static Node *lvar_initializer(Var *var) {
    Node head = {};
    lvar_initializer2(&head, var, var->type, NULL);

    Node *node = alloc_node(ND_BLOCK);
    node->block = head.next;
    return node;
}

// declaration = basetype declarator type-suffix ("=" lvar-initializer)? ";"
//             | basetype ";"
static Node *declaration() {
    StorageClass sclass;
    Type *type = basetype(&sclass);
    if(consume(";")) {
        return alloc_node(ND_NULL);
    }

    char *var_name = NULL;
    type = declarator(type, &var_name);
    type = type_suffix(type);

    if(sclass == TYPEDEF) {
        expect(";");
        push_scope(var_name)->type_def = type;
        return alloc_node(ND_NULL);
    }

    if(type->ty == VOID) {
        error("変数がvoid型として宣言されています");
    }

    // localsに定義した変数を追加
    Var *lvar = new_lvar(strndup(var_name, strlen(var_name)), type);
    if(type->is_incomplete) {
        error("不完全な型です");
    }

    if(consume(";")) {
        // 関数宣言のみ
        return alloc_node(ND_NULL);
    }

    // 関数宣言 + 代入式
    expect("=");
    Node *node = lvar_initializer(lvar);
    expect(";");
    return node;
}

static Node *read_expr_stmt(void) { return new_unary(ND_EXPR_STMT, expr()); }

// 次のトークンが型の場合trueを返す
static bool is_typename(void) {
    return match("void") || match("_Bool") || match("char") || match("short") ||
           match("int") || match("long") || match("struct") || match("enum") ||
           match("typedef") || match("static") || find_typedef(token);
}

static Node *stmt() {
    Node *node = stmt2();
    // stmt2によって生成されたノードツリーの各ノードに型を設定する
    add_type(node);
    return node;
}

// stmt = "return" expr ";"
//      | "{" stmt* "}"
//      | "if" "(" expr ")" stmt ("else" stmt)?
//      | "switch" "(" expr ")" stmt
//      | "case" const-expr ":" stmt
//      | "default" ":" stmt
//      | "while" "(" expr ")" stmt
//      | "for" "(" (expr? ";" | declaration) expr? ";" expr? ")" stmt
//      | expr ";"
//      | declaration
//      | "break" ";"
//      | "continue" ";"
//      | "goto" ident ";"
//      | ident ":" stmt
static Node *stmt2() {
    Node *node;

    if(consume_return()) {
        node = alloc_node(ND_RETURN);
        node->lhs = expr();
        expect(";");
        return node;
    }

    if(consume("{")) {
        Node head = {};
        Node *cur = &head;

        Scope *sc = enter_scope();
        // stmtを任意個数分parseする
        while(!consume("}")) {
            cur->next = stmt();
            cur = cur->next;
        }
        leave_scope(sc);

        Node *node = alloc_node(ND_BLOCK);
        node->block = head.next;
        return node;
    }

    if(consume("if")) {
        node = alloc_node(ND_IF);
        expect("(");
        node->cond = expr();
        expect(")");
        node->then = stmt();

        if(consume("else")) {
            node->els = stmt();
        }
        return node;
    }

    if(consume("switch")) {
        Node *node = alloc_node(ND_SWITCH);
        expect("(");
        node->cond = expr();
        expect(")");

        Node *sw = current_switch;
        current_switch = node;
        node->then = stmt();
        current_switch = sw;
        return node;
    }

    if(consume("case")) {
        if(!current_switch) {
            error("不正なcase句です");
        }
        int val = const_expr();
        expect(":");

        Node *node = new_unary(ND_CASE, stmt());
        node->val = val;
        node->case_next = current_switch->case_next;
        current_switch->case_next = node;
        return node;
    }

    if(consume("default")) {
        if(!current_switch) {
            error("不正なdefault句です");
        }
        expect(":");

        Node *node = new_unary(ND_CASE, stmt());
        current_switch->default_case = node;
        return node;
    }

    if(consume("while")) {
        node = alloc_node(ND_WHILE);
        expect("(");
        node->cond = expr();
        expect(")");
        node->then = stmt();
        return node;
    }

    if(consume("for")) {
        node = alloc_node(ND_FOR);
        expect("(");
        Scope *sc = enter_scope();

        if(!consume(";")) {
            // 初期化式が存在する
            if(is_typename()) {
                node->init = declaration();
            } else {
                node->init = read_expr_stmt();
                expect(";");
            }
        }
        if(!consume(";")) {
            // ループの継続条件式が存在する
            node->cond = expr();
            expect(";");
        }
        if(!consume(")")) {
            // ループ一周終了時の実行処理が存在する
            node->post = read_expr_stmt();
            expect(")");
        }
        node->then = stmt();
        leave_scope(sc);
        return node;
    }

    if(consume("break")) {
        expect(";");
        return alloc_node(ND_BREAK);
    }

    if(consume("continue")) {
        expect(";");
        return alloc_node(ND_CONTINUE);
    }

    if(consume("goto")) {
        Node *node = alloc_node(ND_GOTO);
        node->label_name = expect_ident();
        expect(";");
        return node;
    }

    Token *tok;
    if(tok = consume_ident()) {
        if(consume(":")) {
            Node *node = new_unary(ND_LABEL, stmt());
            node->label_name = strndup(tok->str, tok->len);
            return node;
        }
        token = tok;
    }

    // 変数定義
    if(is_typename()) {
        return declaration();
    }

    node = read_expr_stmt();
    expect(";");
    return node;
}

// expr = assign ("," assign)*
static Node *expr() {
    Node *node = assign();
    while(consume(",")) {
        node = new_unary(ND_EXPR_STMT, node);
        node = new_binary(ND_COMMA, node, assign());
    }
    return node;
}

// 与えられたnodeを定数式として評価する
static long eval(Node *node) {
    switch(node->kind) {
        case ND_ADD:
            return eval(node->lhs) + eval(node->rhs);
        case ND_SUB:
            return eval(node->lhs) - eval(node->rhs);
        case ND_MUL:
            return eval(node->lhs) * eval(node->rhs);
        case ND_DIV:
            return eval(node->lhs) / eval(node->rhs);
        case ND_BITAND:
            return eval(node->lhs) & eval(node->rhs);
        case ND_BITOR:
            return eval(node->lhs) | eval(node->rhs);
        case ND_BITXOR:
            return eval(node->lhs) | eval(node->rhs);
        case ND_SHL:
            return eval(node->lhs) << eval(node->rhs);
        case ND_SHR:
            return eval(node->lhs) >> eval(node->rhs);
        case ND_EQ:
            return eval(node->lhs) == eval(node->rhs);
        case ND_NE:
            return eval(node->lhs) != eval(node->rhs);
        case ND_LT:
            return eval(node->lhs) < eval(node->rhs);
        case ND_LE:
            return eval(node->lhs) <= eval(node->rhs);
        case ND_TERNARY:
            return eval(node->cond) ? eval(node->then) : eval(node->els);
        case ND_COMMA:
            return eval(node->rhs);
        case ND_NOT:
            return !eval(node->lhs);
        case ND_BITNOT:
            return ~eval(node->lhs);
        case ND_LOGAND:
            return eval(node->lhs) && eval(node->rhs);
        case ND_LOGOR:
            return eval(node->lhs) || eval(node->rhs);
        case ND_NUM:
            return node->val;
    }

    error("定数式ではありません");
}

static long const_expr() { return eval(conditional()); }

// assign    = conditional (assign-op assign)?
// assign-op = "=" | "+=" | "-=" | "*=" | "/=" | "<<=" | ">>="
static Node *assign() {
    Node *node = conditional();

    if(consume("=")) {
        return new_binary(ND_ASSIGN, node, assign());
    }
    if(consume("*=")) {
        return new_binary(ND_MUL_EQ, node, assign());
    }
    if(consume("/=")) {
        return new_binary(ND_DIV_EQ, node, assign());
    }
    if(consume("<<=")) {
        return new_binary(ND_SHL_EQ, node, assign());
    }
    if(consume(">>=")) {
        return new_binary(ND_SHR_EQ, node, assign());
    }

    if(consume("+=")) {
        add_type(node);
        if(node->type->ptr_to) {
            return new_binary(ND_PTR_ADD_EQ, node, assign());
        } else {
            return new_binary(ND_ADD_EQ, node, assign());
        }
    }
    if(consume("-=")) {
        add_type(node);
        if(node->type->ptr_to) {
            return new_binary(ND_PTR_SUB_EQ, node, assign());
        } else {
            return new_binary(ND_SUB_EQ, node, assign());
        }
    }

    return node;
}

// conditional = logor ("?" expr ":" conditional)?
static Node *conditional() {
    Node *node = logor();

    if(!consume("?")) {
        return node;
    }

    Node *ternary = alloc_node(ND_TERNARY);
    ternary->cond = node;
    ternary->then = expr();
    expect(":");
    ternary->els = conditional();
    return ternary;
}

// logor = logand ("||" logand)*
static Node *logor() {
    Node *node = logand();
    while(consume("||")) {
        node = new_binary(ND_LOGOR, node, logand());
    }
    return node;
}

// logand = bitor ("&&" bitor)*
static Node *logand() {
    Node *node = bitor ();
    while(consume("&&")) {
        node = new_binary(ND_LOGAND, node, bitor ());
    }
    return node;
}

// bitor = bitxor ("|" bitxor)*
static Node * bitor () {
    Node *node = bitxor();
    while(consume("|")) {
        node = new_binary(ND_BITOR, node, bitxor());
    }
    return node;
}

// bitxor = bitand ("^" bitand)*
static Node *bitxor() {
    Node *node = bitand();
    while(consume("^")) {
        node = new_binary(ND_BITXOR, node, bitxor());
    }
    return node;
}

// bitand = equality ("&" equality)*
static Node *bitand() {
    Node *node = equality();
    while(consume("&")) {
        node = new_binary(ND_BITAND, node, equality());
    }
    return node;
}

// equality = relational ("==" relational | "!=" relational)*
static Node *equality() {
    Node *node = relational();

    for(;;) {
        if(consume("=="))
            node = new_binary(ND_EQ, node, relational());
        else if(consume("!="))
            node = new_binary(ND_NE, node, relational());
        else
            return node;
    }
}

// relational = shift ("<" shift | "<=" shift | ">" shift | ">=" shift)*
static Node *relational() {
    Node *node = shift();

    for(;;) {
        if(consume("<"))
            node = new_binary(ND_LT, node, shift());
        else if(consume("<="))
            node = new_binary(ND_LE, node, shift());
        else if(consume(">"))
            node = new_binary(ND_LT, shift(), node);
        else if(consume(">="))
            node = new_binary(ND_LE, shift(), node);
        else
            return node;
    }
}

// shift = add ("<<" add | ">>" add)*
static Node *shift() {
    Node *node = add();

    for(;;) {
        if(consume("<<")) {
            node = new_binary(ND_SHL, node, add());
        } else if(consume(">>")) {
            node = new_binary(ND_SHR, node, add());
        } else {
            return node;
        }
    }
}

// lhs,rhsを元に整数型orポインタ型の加算ノードを生成する。
static Node *new_add(Node *lhs, Node *rhs) {
    add_type(lhs);
    add_type(rhs);

    if(is_integer(lhs->type) && is_integer(rhs->type)) {
        return new_binary(ND_ADD, lhs, rhs);
    }
    if(lhs->type->ptr_to && is_integer(rhs->type)) {
        return new_binary(ND_PTR_ADD, lhs, rhs);
    }
    if(is_integer(lhs->type) && rhs->type->ptr_to) {
        return new_binary(ND_PTR_ADD, rhs, lhs);
    }
    error("無効な演算です");
}

// lhs,rhsを元に整数型orポインタ型の減算ノードを生成する。
static Node *new_sub(Node *lhs, Node *rhs) {
    add_type(lhs);
    add_type(rhs);

    if(is_integer(lhs->type) && is_integer(rhs->type)) {
        return new_binary(ND_SUB, lhs, rhs);
    }
    if(lhs->type->ptr_to && is_integer(rhs->type)) {
        return new_binary(ND_PTR_SUB, lhs, rhs);
    }
    if(lhs->type->ptr_to && rhs->type->ptr_to) {
        return new_binary(ND_PTR_DIFF, lhs, rhs);
    }
    error("無効な演算です");
}

// add = mul ("+" mul | "-" mul)*
static Node *add() {
    Node *node = mul();

    for(;;) {
        if(consume("+"))
            node = new_add(node, mul());
        else if(consume("-"))
            node = new_sub(node, mul());
        else
            return node;
    }
}

// mul = cast ("*" cast | "/" cast)*
static Node *mul() {
    Node *node = cast();

    for(;;) {
        if(consume("*"))
            node = new_binary(ND_MUL, node, cast());
        else if(consume("/"))
            node = new_binary(ND_DIV, node, cast());
        else
            return node;
    }
}

// cast = "(" type-name ")" cast | unary
static Node *cast() {
    Token *tok = token;

    if(consume("(")) {
        if(is_typename()) {
            Type *ty = type_name();
            expect(")");
            Node *node = new_unary(ND_CAST, cast());
            add_type(node->lhs);
            node->type = ty;
            return node;
        }
        token = tok;
    }

    return unary();
}

// unary = ("+" | "-" | "*" | "&" | "!" | "~")? cast
//       | ("++" | "--") unary
//       | postfix
static Node *unary() {
    if(consume("+")) return cast();
    if(consume("-")) return new_binary(ND_SUB, new_node_num(0), cast());
    if(consume("*")) return new_unary(ND_DEREF, cast());
    if(consume("&")) return new_unary(ND_ADDR, cast());
    if(consume("!")) return new_unary(ND_NOT, cast());
    if(consume("~")) return new_unary(ND_BITNOT, cast());
    if(consume("++")) return new_unary(ND_PRE_INC, unary());
    if(consume("--")) return new_unary(ND_PRE_DEC, unary());
    return postfix();
}

static Member *find_member(Type *ty, char *name) {
    for(Member *mem = ty->members; mem; mem = mem->next) {
        if(!strcmp(mem->name, name)) {
            return mem;
        }
    }
    return NULL;
}

static Node *struct_ref(Node *lhs) {
    add_type(lhs);
    if(lhs->type->ty != STRUCT) {
        error("構造体ではありません");
    }

    Member *mem = find_member(lhs->type, expect_ident());
    if(!mem) {
        error("構造体が見つかりません");
    }

    Node *node = new_unary(ND_MEMBER, lhs);
    node->member = mem;
    return node;
}

// postfix = primary ("[" expr "]" | "." ident | "->" ident | "++" | "--")*
static Node *postfix() {
    Node *node = primary();

    for(;;) {
        if(consume("[")) {
            // x[y]を*(x+y)として読み換える
            Node *node_expr = expr();
            expect("]");
            node = new_unary(ND_DEREF, new_add(node, node_expr));
            continue;
        }

        if(consume(".")) {
            node = struct_ref(node);
            continue;
        }

        if(consume("->")) {
            // x->yを(*x).yとして読み替える
            node = new_unary(ND_DEREF, node);
            node = struct_ref(node);
            continue;
        }

        if(consume("++")) {
            node = new_unary(ND_POST_INC, node);
            continue;
        }

        if(consume("--")) {
            node = new_unary(ND_POST_DEC, node);
            continue;
        }

        return node;
    }
}

// func_args = "(" (assign ("," assign)*)? ")"
static Node *func_args() {
    // "("はprimary関数内でconsume済みなので")"の存在をチェックする
    if(consume(")")) {
        return NULL;
    }
    Node *head = assign();
    Node *cur = head;
    while(consume(",")) {
        cur->next = assign();
        cur = cur->next;
    }
    expect(")");
    return head;
}

// stmt-expr = "(" "{" stmt stmt* "}" ")"
//
// statement expressionはGNUの拡張機能。
// 括弧で囲んだ複数のstatementを1つの式として取り扱う。
static Node *stmt_expr() {
    Scope *sc = enter_scope();
    Node *node = alloc_node(ND_STMT_EXPR);
    node->block = stmt();
    Node *cur = node->block;

    while(!consume("}")) {
        cur->next = stmt();
        cur = cur->next;
    }
    expect(")");
    leave_scope(sc);

    if(cur->kind != ND_EXPR_STMT) {
        error("voidを返すstatement expressionは非サポートです");
    }
    memcpy(cur, cur->lhs, sizeof(Node));
    return node;
}

// primary = num
//         | str
//         | ident func_args?
//         | "(" expr ")"
//         | "sizeof" "(" type-name ")"
//         | "sizeof" unary
//         | "(" "{" stmt-expr-tail
static Node *primary() {
    // 次のトークンが"("なら、"(" expr ")"のはず
    if(consume("(")) {
        if(consume("{")) {
            return stmt_expr();
        }

        Node *node = expr();
        expect(")");
        return node;
    }
    Token *tok;

    // sizeof
    if(tok = consume("sizeof")) {
        if(consume("(")) {
            if(is_typename()) {
                Type *ty = type_name();
                if(ty->is_incomplete) {
                    error("不完全な型です");
                }
                expect(")");
                return new_node_num(ty->size);
            }
            token = tok->next;
        }

        // 演算対象となる子ノードの型サイズを出力
        Node *node = unary();
        add_type(node);
        if(node->type->is_incomplete) {
            error("不完全な型です");
        }
        return new_node_num(node->type->size);
    }

    // identトークンのチェック
    tok = consume_ident();
    if(tok) {
        Node *node;

        // 関数呼び出し
        if(consume("(")) {
            node = alloc_node(ND_FUNCCALL);
            node->func_name = strndup(tok->str, tok->len);
            node->args = func_args();
            add_type(node);

            VarScope *sc = find_var(tok);
            if(sc) {
                if(!sc->var || sc->var->type->ty != FUNC) {
                    error("関数ではありません");
                }
                node->type = sc->var->type->return_ty;
            } else {
                warn(tok, "暗黙的な関数宣言です");
                node->type = int_type;
            }
            return node;
        }

        // 変数、enum定数
        VarScope *sc = find_var(tok);

        if(sc) {
            if(sc->var) {
                return new_var_node(sc->var);
            }
            if(sc->enum_ty) {
                return new_node_num(sc->enum_val);
            }
        }

        error("未定義のローカル変数%sを参照しています",
              strndup(tok->str, tok->len));
    }

    // 文字列トークン
    tok = consume_str();
    if(tok) {
        // 文字列リテラルをグローバル変数に追加する
        Var *gvar =
            new_gvar(new_label(), array_of(char_type, tok->cont_len), true);
        gvar->contents = tok->contents;
        gvar->cont_len = tok->cont_len;
        return new_var_node(gvar);
    }

    // そうでなければ数値のはず
    return new_node_num(expect_number());
}