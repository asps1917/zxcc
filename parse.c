#include "zxcc.h"

// パース処理中に現れたローカル変数を追加するための連結リスト
VarList *locals;

// 変数を名前で検索する。見つからなかった場合はNULLを返す。
static LVar *find_lvar(Token *tok) {
    for(VarList *vlist = locals; vlist; vlist = vlist->next) {
        LVar *var = vlist->var;
        if(strlen(var->name) == tok->len &&
           !memcmp(tok->str, var->name, tok->len)) {
            return var;
        }
    }
    return NULL;
}

// 引数として与えられた変数名のLVar構造体を生成する。
// 生成したLVar構造体はlocalsリストに追加される。
static LVar *new_lvar(char *name, Type *type) {
    LVar *lvar = calloc(1, sizeof(LVar));
    VarList *vl = calloc(1, sizeof(VarList));

    vl->var = lvar;
    vl->next = locals;
    lvar->name = name;
    lvar->type = type;
    locals = vl;
    return lvar;
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

// basetype = "int" "*"*
// パースした型を表すType構造体へのポインタを返す
static Type *basetype() {
    expect("int");
    Type *cur = calloc(1, sizeof(Type));
    cur->ty = INT;
    while(consume("*")) {
        Type *type = calloc(1, sizeof(Type));
        type->ty = PTR;
        type->ptr_to = cur;
        cur = type;
    }
    return cur;
}

// params   = basetype ident ("," basetype ident)*
static VarList *params() {
    VarList *head = calloc(1, sizeof(VarList));
    head->var = calloc(1, sizeof(LVar));
    VarList *cur = head;

    while(1) {
        Type *type = basetype();
        char *var_name = expect_ident();
        // identを以下のVarListに追加
        // * 関数定義内の引数リスト
        // * locals(ローカル変数リスト)
        LVar *lvar = new_lvar(var_name, type);
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
//      | basetype ident ";"
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

    // 変数定義
    if(match("int")) {
        Type *type = basetype();
        Token *tok = consume_ident();
        if(!tok) {
            error("変数定義の構文エラー");
        }
        expect(";");

        // localsに定義した変数を追加
        node = alloc_node(ND_LVAR);
        LVar *lvar = find_lvar(tok);
        if(lvar) {
            error("変数%sは重複して定義されています", lvar->name);
        }
        lvar = new_lvar(strndup(tok->str, tok->len), type);
        node->lvar = lvar;
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

// unary = "+"? primary
//       | "-"? primary
//       | "*" unary
//       | "&" unary
static Node *unary() {
    if(consume("+")) return primary();
    if(consume("-")) return new_node(ND_SUB, new_node_num(0), primary());
    if(consume("*")) return new_node(ND_DEREF, unary(), NULL);
    if(consume("&")) return new_node(ND_ADDR, unary(), NULL);
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
            // 定義済みのローカル変数への参照
            node->lvar = lvar;
            return node;
        } else {
            // 未定義のローカル変数
            char *s = strndup(tok->str, tok->len);
            error("未定義のローカル変数%sを参照しています", s);
        }
    }

    // そうでなければ数値のはず
    return new_node_num(expect_number());
}