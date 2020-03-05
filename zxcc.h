#define _GNU_SOURCE
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// tokenize.c
//

// トークンの種類
typedef enum {
    TK_RESERVED,  // 記号
    TK_IDENT,     // 識別子
    TK_NUM,       // 整数トークン
    TK_EOF,       // 入力の終わりを表すトークン
    TK_RETURN,    // return
} TokenKind;

typedef struct Token Token;

// トークン型
struct Token {
    TokenKind kind;  // トークンの型
    Token *next;     // 次の入力トークン
    int val;         // kindがTK_NUMの場合、その数値
    char *str;       // トークン文字列
    int len;         // トークンの長さ
};

extern Token *token;
extern char *user_input;

void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
Token *tokenize();
bool consume(char *op);
Token *consume_ident();
Token *consume_return();
void expect(char *op);
int expect_number();
char *expect_ident();
bool at_eof();

//
// parse.c
//

// 抽象構文木のノードの種類
typedef enum {
    ND_ADD,       // +
    ND_SUB,       // -
    ND_MUL,       // *
    ND_DIV,       // /
    ND_EQ,        // ==
    ND_NE,        // !=
    ND_LT,        // <
    ND_LE,        // <=
    ND_ASSIGN,    // =
    ND_LVAR,      // ローカル変数
    ND_NUM,       // 整数
    ND_RETURN,    // return
    ND_IF,        // if
    ND_WHILE,     // while
    ND_FOR,       // for
    ND_BLOCK,     // ブロック
    ND_FUNCCALL,  // 関数呼び出し
} NodeKind;

// ローカル変数の型
typedef struct LVar LVar;
struct LVar {
    char *name;  // 変数の名前
    int len;     // 名前の長さ
    int offset;  // RBPからのオフセット
};

typedef struct VarList VarList;
struct VarList {
    VarList *next;
    LVar *var;
};

typedef struct Node Node;

// 抽象構文木のノードの型
struct Node {
    NodeKind kind;  // ノードの型
    Node *next;     // 次のノード
    Node *lhs;      // 左辺
    Node *rhs;      // 右辺
    int val;        // kindがND_NUMの場合のみ使う

    // if, while, for文用
    Node *cond;  // 条件式
    Node *then;  // 条件式を満たす場合の実行処理
    Node *els;   // 条件式を満たさない場合の実行処理
    Node *init;  // for文の初期化処理
    Node *post;  // for文のループ一周終了時処理

    // ブロック
    Node *block;

    // 関数呼び出し
    char *func_name;
    Node *args;

    // ND_LVAR用
    LVar *lvar;
};

typedef struct Function Function;
struct Function {
    Function *next;
    char *name;
    VarList *args;

    Node *node;
    VarList *locals;
    int stack_size;
};

Function *program();

//
// codegen.c
//

void codegen(Function *prog);