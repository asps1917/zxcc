#include "zxcc.h"

// ローカル・グローバル変数、typedef、enumのスコープ
typedef struct VarScope VarScope;
struct VarScope {
    VarScope *next;
    char *name;
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

// ブロックスコープの開始処理
static Scope *enter_scope(void) {
    Scope *sc = calloc(1, sizeof(Scope));
    sc->var_scope = var_scope;
    sc->tag_scope = tag_scope;
    return sc;
}

// ブロックスコープの終了処理
static void leave_scope(Scope *sc) {
    var_scope = sc->var_scope;
    tag_scope = sc->tag_scope;
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
static Node *assign();
static Node *equality();
static Node *relational();
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

// type-suffix = ("[" num "]" type-suffix)?
// 変数宣言の型名のsuffix([])を読み取る
static Type *type_suffix(Type *ty) {
    if(!consume("[")) {
        return ty;
    }
    int sz = expect_number();
    expect("]");
    ty = type_suffix(ty);
    return array_of(ty, sz);
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
    sc->ty = ty;
    tag_scope = sc;
}

// struct-decl = "struct" ident
//             | "struct" ident? "{" struct-member "}"
static Type *struct_decl(void) {
    // 構造体タグの読み出し
    expect("struct");
    Token *tag = consume_ident();
    if(tag && !match("{")) {
        TagScope *sc = find_tag(tag);
        if(!sc) {
            error("未定義の構造体の型です");
        }
        if(sc->ty->ty != STRUCT) {
            error("構造体タグではありません");
        }
        return sc->ty;
    }

    expect("{");

    // 構造体メンバの読み出し
    Member head = {};
    Member *cur = &head;

    while(!consume("}")) {
        cur->next = struct_member();
        cur = cur->next;
    }

    Type *ty = calloc(1, sizeof(Type));
    ty->ty = STRUCT;
    ty->members = head.next;

    // 構造体メンバへのoffset割り当て
    int offset = 0;
    for(Member *mem = ty->members; mem; mem = mem->next) {
        offset = align_to(offset, mem->ty->align);
        mem->offset = offset;
        offset += mem->ty->size;

        if(ty->align < mem->ty->align) {
            ty->align = mem->ty->align;
        }
    }
    ty->size = align_to(offset, ty->align);

    // 構造体の型を登録
    if(tag) {
        push_tag_scope(tag, ty);
    }
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

// enum-specifier = "enum" ident
//                | "enum" ident? "{" enum-list? "}"
//
// enum-list = ident ("=" num)? ("," ident ("=" num)?)* ","?
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
            cnt = expect_number();
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
    char *name = NULL;
    ty = declarator(ty, &name);
    ty = type_suffix(ty);
    expect(";");

    Member *mem = calloc(1, sizeof(Member));
    mem->name = name;
    mem->ty = ty;
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
        // global変数に定義した変数を追加
        new_gvar(strndup(var_name, strlen(var_name)), type, true);
    }
}

// declaration = basetype declarator type-suffix ("=" expr)? ";"
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

    if(consume(";")) {
        // 関数宣言のみ
        return alloc_node(ND_NULL);
    }

    // 関数宣言 + 代入式
    expect("=");
    Node *node_expr = expr();
    expect(";");
    Node *node_var = alloc_node(ND_VAR);
    node_var->var = lvar;
    Node *node_assign = new_binary(ND_ASSIGN, node_var, node_expr);
    return new_unary(ND_EXPR_STMT, node_assign);
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
//      | "while" "(" expr ")" stmt
//      | "for" "(" expr? ";" expr? ";" expr? ")" stmt
//      | expr ";"
//      | declaration
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
        if(!consume(";")) {
            // 初期化式が存在する
            node->init = read_expr_stmt();
            expect(";");
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
        return node;
    }

    // 変数定義
    if(is_typename()) {
        return declaration();
    }

    node = read_expr_stmt();
    expect(";");
    return node;
}

// expr = assign
static Node *expr() { return assign(); }

// assign = equality ("=" assign)?
static Node *assign() {
    Node *node = equality();
    if(consume("=")) node = new_binary(ND_ASSIGN, node, assign());
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

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
static Node *relational() {
    Node *node = add();

    for(;;) {
        if(consume("<"))
            node = new_binary(ND_LT, node, add());
        else if(consume("<="))
            node = new_binary(ND_LE, node, add());
        else if(consume(">"))
            node = new_binary(ND_LT, add(), node);
        else if(consume(">="))
            node = new_binary(ND_LE, add(), node);
        else
            return node;
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

// unary = ("+" | "-" | "*" | "&")? cast
//       | postfix
static Node *unary() {
    if(consume("+")) return cast();
    if(consume("-")) return new_binary(ND_SUB, new_node_num(0), cast());
    if(consume("*")) return new_unary(ND_DEREF, cast());
    if(consume("&")) return new_unary(ND_ADDR, cast());
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

// postfix = primary ("[" expr "]" | "." ident | "->" ident)*
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
                expect(")");
                return new_node_num(ty->size);
            }
            token = tok->next;
        }

        // 演算対象となる子ノードの型サイズを出力
        Node *node = unary();
        add_type(node);
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
                node = alloc_node(ND_VAR);
                node->var = sc->var;
                return node;
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
        Node *node = alloc_node(ND_VAR);
        Var *gvar =
            new_gvar(new_label(), array_of(char_type, tok->cont_len), true);
        gvar->contents = tok->contents;
        gvar->cont_len = tok->cont_len;
        node->var = gvar;
        return node;
    }

    // そうでなければ数値のはず
    return new_node_num(expect_number());
}