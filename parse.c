#include "zxcc.h"

// パース処理中に現れたローカル変数を追加するための連結リスト
static VarList *locals;
static VarList *globals;
static VarList *scope;

// 変数を名前で検索する。検索対象はローカル変数リスト→グローバル変数リストの順番。
// 見つからなかった場合はNULLを返す。
static Var *find_var(char *var_name) {
    int var_len = strlen(var_name);
    for(VarList *vlist = scope; vlist; vlist = vlist->next) {
        Var *var = vlist->var;
        if(strlen(var->name) == var_len &&
           !strncmp(var_name, var->name, var_len)) {
            return var;
        }
    }
    return NULL;
}

// 引数として与えられた変数名のVar構造体を生成する
static Var *new_var(char *name, Type *type, bool is_local) {
    Var *var = calloc(1, sizeof(Var));
    var->name = name;
    var->type = type;
    var->is_local = is_local;

    VarList *sc = calloc(1, sizeof(VarList));
    sc->var = var;
    sc->next = scope;
    scope = sc;
    return var;
}

// 引数として与えられた変数名のVar構造体を生成する。
// 生成したVar構造体はlocalsリストに追加される。
static Var *new_lvar(char *name, Type *type) {
    Var *lvar = new_var(name, type, true);
    VarList *vl = calloc(1, sizeof(VarList));

    vl->var = lvar;
    vl->next = locals;
    locals = vl;
    return lvar;
}

// 引数として与えられた変数名のVar構造体を生成する。
// 生成したVar構造体はglobalsリストに追加される。
static Var *new_gvar(char *name, Type *type) {
    Var *gvar = new_var(name, type, false);
    VarList *vl = calloc(1, sizeof(VarList));

    vl->var = gvar;
    vl->next = globals;
    globals = vl;
    return gvar;
}

static Node *alloc_node(NodeKind kind) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    return node;
}

static Node *new_node(NodeKind kind, Node *lhs, Node *rhs) {
    Node *node = alloc_node(kind);
    node->lhs = lhs;
    node->rhs = rhs;
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

static Type *basetype();
static Function *function();
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
static Node *unary();
static Node *postfix();
static Node *primary();

// 現在のトークンが関数か判定する
static bool is_function() {
    Token *cur = token;
    basetype();
    bool retval = consume_ident() && consume("(");
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
            cur->next = function();
            cur = cur->next;
        } else {
            // グローバル変数
            global_var();
        }
    }
    Program *prog = calloc(1, sizeof(Program));
    prog->funcs = head.next;
    prog->globals = globals;
    return prog;
}

// basetype = ("int" | "char") "*"*
// パースした型を表すType構造体へのポインタを返す
static Type *basetype() {
    Type *cur;
    if(consume("int")) {
        cur = int_type;
    } else if(consume("char")) {
        cur = char_type;
    } else {
        error("不正な型です");
    }

    while(consume("*")) {
        cur = pointer_to(cur);
    }
    return cur;
}

// 変数宣言の型名のsuffix([])を読み取る
static Type *read_type_suffix(Type *base) {
    if(!consume("[")) {
        return base;
    }
    int sz = expect_number();
    expect("]");
    base = read_type_suffix(base);
    return array_of(base, sz);
}

// params   = basetype ident ("," basetype ident)*
static VarList *params() {
    VarList *head = calloc(1, sizeof(VarList));
    head->var = calloc(1, sizeof(Var));
    VarList *cur = head;

    while(1) {
        Type *type = basetype();
        char *var_name = expect_ident();
        type = read_type_suffix(type);
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

// function = basetype ident "(" params? ")" "{" stmt* "}"
static Function *function() {
    locals = NULL;

    basetype();
    Function *func = calloc(1, sizeof(Function));
    func->name = expect_ident();
    expect("(");

    VarList *sc = scope;
    if(!consume(")")) {
        func->args = params();
        expect(")");
    }
    expect("{");

    Node head = {};
    Node *cur = &head;

    // stmt*
    while(!consume("}")) {
        cur->next = stmt();
        cur = cur->next;
    }
    scope = sc;

    func->node = head.next;
    func->locals = locals;
    return func;
}

// global_var = basetype ident ("[" num "]")* ";"
static void global_var() {
    Type *type = basetype();
    char *var_name = expect_ident();
    type = read_type_suffix(type);
    expect(";");

    // global変数に定義した変数を追加
    new_gvar(strndup(var_name, strlen(var_name)), type);
}

// declaration = basetype ident ("[" num "]")* ("=" expr)? ";"
static Node *declaration() {
    Type *type = basetype();
    char *var_name = expect_ident();
    type = read_type_suffix(type);
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
    Node *node_assign = new_node(ND_ASSIGN, node_var, node_expr);
    return new_node(ND_EXPR_STMT, node_assign, NULL);
}

static Node *read_expr_stmt(void) {
    return new_node(ND_EXPR_STMT, expr(), NULL);
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

        VarList *sc = scope;
        // stmtを任意個数分parseする
        while(!consume("}")) {
            cur->next = stmt();
            cur = cur->next;
        }
        scope = sc;

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
    if(match("int") || match("char")) {
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
    if(consume("=")) node = new_node(ND_ASSIGN, node, assign());
    return node;
}

// equality = relational ("==" relational | "!=" relational)*
static Node *equality() {
    Node *node = relational();

    for(;;) {
        if(consume("=="))
            node = new_node(ND_EQ, node, relational());
        else if(consume("!="))
            node = new_node(ND_NE, node, relational());
        else
            return node;
    }
}

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
static Node *relational() {
    Node *node = add();

    for(;;) {
        if(consume("<"))
            node = new_node(ND_LT, node, add());
        else if(consume("<="))
            node = new_node(ND_LE, node, add());
        else if(consume(">"))
            node = new_node(ND_LT, add(), node);
        else if(consume(">="))
            node = new_node(ND_LE, add(), node);
        else
            return node;
    }
}

// lhs,rhsを元に整数型orポインタ型の加算ノードを生成する。
static Node *new_add(Node *lhs, Node *rhs) {
    add_type(lhs);
    add_type(rhs);

    if(is_integer(lhs->type) && is_integer(rhs->type)) {
        return new_node(ND_ADD, lhs, rhs);
    }
    if(lhs->type->ptr_to && is_integer(rhs->type)) {
        return new_node(ND_PTR_ADD, lhs, rhs);
    }
    if(is_integer(lhs->type) && rhs->type->ptr_to) {
        return new_node(ND_PTR_ADD, rhs, lhs);
    }
    error("無効な演算です");
}

// lhs,rhsを元に整数型orポインタ型の減算ノードを生成する。
static Node *new_sub(Node *lhs, Node *rhs) {
    add_type(lhs);
    add_type(rhs);

    if(is_integer(lhs->type) && is_integer(rhs->type)) {
        return new_node(ND_SUB, lhs, rhs);
    }
    if(lhs->type->ptr_to && is_integer(rhs->type)) {
        return new_node(ND_PTR_SUB, lhs, rhs);
    }
    if(lhs->type->ptr_to && rhs->type->ptr_to) {
        return new_node(ND_PTR_DIFF, lhs, rhs);
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

// mul = unary ("*" unary | "/" unary)*
static Node *mul() {
    Node *node = unary();

    for(;;) {
        if(consume("*"))
            node = new_node(ND_MUL, node, unary());
        else if(consume("/"))
            node = new_node(ND_DIV, node, unary());
        else
            return node;
    }
}

// unary = ("+" | "-" | "*" | "&")? unary
//       | postfix
static Node *unary() {
    if(consume("+")) return unary();
    if(consume("-")) return new_node(ND_SUB, new_node_num(0), unary());
    if(consume("*")) return new_node(ND_DEREF, unary(), NULL);
    if(consume("&")) return new_node(ND_ADDR, unary(), NULL);
    return postfix();
}

// postfix = primary ("[" expr "]")*
static Node *postfix() {
    Node *node = primary();
    while(consume("[")) {
        // x[y]を*(x+y)として読み換える
        Node *node_expr = expr();
        expect("]");
        node = new_node(ND_DEREF, new_add(node, node_expr), NULL);
    }
    return node;
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
    Node *node = alloc_node(ND_STMT_EXPR);
    node->block = stmt();
    Node *cur = node->block;

    while(!consume("}")) {
        cur->next = stmt();
        cur = cur->next;
    }
    expect(")");

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

    // sizeof
    if(consume("sizeof")) {
        // 演算対象となる子ノードの型サイズを出力
        Node *node = unary();
        add_type(node);
        return new_node_num(node->type->size);
    }

    // identトークンのチェック
    Token *tok = consume_ident();
    if(tok) {
        Node *node;

        // 関数呼び出し
        if(consume("(")) {
            node = alloc_node(ND_FUNCCALL);
            node->func_name = strndup(tok->str, tok->len);
            node->args = func_args();
            return node;
        }

        // ローカル変数
        node = alloc_node(ND_VAR);
        char *var_name = strndup(tok->str, tok->len);
        Var *lvar = find_var(var_name);
        if(lvar) {
            // 定義済みのローカル変数への参照
            node->var = lvar;
            return node;
        } else {
            // 未定義のローカル変数
            error("未定義のローカル変数%sを参照しています", var_name);
        }
    }

    // 文字列トークン
    tok = consume_str();
    if(tok) {
        // 文字列リテラルをグローバル変数に追加する
        Node *node = alloc_node(ND_VAR);
        Var *gvar = new_gvar(new_label(), array_of(char_type, tok->cont_len));
        gvar->contents = tok->contents;
        gvar->cont_len = tok->cont_len;
        node->var = gvar;
        return node;
    }

    // そうでなければ数値のはず
    return new_node_num(expect_number());
}