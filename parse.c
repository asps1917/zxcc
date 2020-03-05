#include "zxcc.h"

// パース処理中に現れたローカル変数を追加するための連結リスト
VarList *locals;

// 変数を名前で検索する。見つからなかった場合はNULLを返す。
static LVar *find_lvar(Token *tok) {
    for(VarList *vlist = locals; vlist; vlist = vlist->next) {
        LVar *var = vlist->var;
        if(var->len == tok->len && !memcmp(tok->str, var->name, var->len)) {
            return var;
        }
    }
    return NULL;
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
// 初期化済みのLvar構造体を作成する。
static LVar *init_lvar() {
    LVar *lvar = calloc(1, sizeof(LVar));
    return lvar;
}

static Function *function();
static Node *stmt();
static Node *expr();
static Node *assign();
static Node *equality();
static Node *relational();
static Node *add();
static Node *mul();
static Node *unary();
static Node *primary();

// program = function*
Function *program() {
    Function head = {};
    Function *cur = &head;

    while(!at_eof()) {
        cur->next = function();
        cur = cur->next;
    }

    return head.next;
}

// params   = ident ("," ident)*
static VarList *params() {
    VarList *head = calloc(1, sizeof(VarList));
    head->var = init_lvar();
    VarList *cur = head;

    while(1) {
        char *var_name = expect_ident();
        // identを以下のVarListに追加
        // * 関数定義内の引数リスト
        // * locals(ローカル変数リスト)
        LVar *lvar = calloc(1, sizeof(LVar));
        VarList *vl = calloc(1, sizeof(VarList));
        vl->next = locals;
        vl->var = lvar;
        lvar->name = var_name;
        lvar->len = strlen(var_name);
        locals = vl;

        cur->next = calloc(1, sizeof(VarList));
        cur->next->var = lvar;
        cur = cur->next;

        if(!consume(",")) {
            return head->next;
        }
    }
}

// function = ident "(" params? ")" "{" stmt* "}"
static Function *function() {
    locals = NULL;

    Function *func = calloc(1, sizeof(Function));
    func->name = expect_ident();
    expect("(");
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

    func->node = head.next;
    func->locals = locals;
    return func;
}

// stmt = "return" expr ";"
//      | "{" stmt* "}"
//      | "if" "(" expr ")" stmt ("else" stmt)?
//      | "while" "(" expr ")" stmt
//      | "for" "(" expr? ";" expr? ";" expr? ")" stmt
//      | expr ";"
static Node *stmt() {
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

        // stmtを任意個数分parseする
        while(!consume("}")) {
            cur->next = stmt();
            cur = cur->next;
        }

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
            node->init = expr();
            expect(";");
        }
        if(!consume(";")) {
            // ループの継続条件式が存在する
            node->cond = expr();
            expect(";");
        }
        if(!consume(")")) {
            // ループ一周終了時の実行処理が存在する
            node->post = expr();
            expect(")");
        }
        node->then = stmt();
        return node;
    }

    node = expr();
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

// add = mul ("+" mul | "-" mul)*
static Node *add() {
    Node *node = mul();

    for(;;) {
        if(consume("+"))
            node = new_node(ND_ADD, node, mul());
        else if(consume("-"))
            node = new_node(ND_SUB, node, mul());
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

// unary = ("+" | "-")? primary
static Node *unary() {
    if(consume("+")) return primary();
    if(consume("-")) return new_node(ND_SUB, new_node_num(0), primary());
    return primary();
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

// primary = num
//         | ident func_args?
//         | "(" expr ")"
static Node *primary() {
    // 次のトークンが"("なら、"(" expr ")"のはず
    if(consume("(")) {
        Node *node = expr();
        expect(")");
        return node;
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
        node = alloc_node(ND_LVAR);
        LVar *lvar = find_lvar(tok);
        if(lvar) {
            // 割り当て済みのローカル変数
            // スタック上のアドレスを取得する
            node->lvar = lvar;
        } else {
            // 初めて出現したローカル変数
            // スタック上のローカル変数用領域に割り当てる
            lvar = calloc(1, sizeof(LVar));
            VarList *vl = calloc(1, sizeof(VarList));
            vl->var = lvar;
            vl->next = locals;
            lvar->name = tok->str;
            lvar->len = tok->len;
            locals = vl;
            node->lvar = lvar;
        }
        return node;
    }

    // そうでなければ数値のはず
    return new_node_num(expect_number());
}