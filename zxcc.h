#define _GNU_SOURCE
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Type Type;

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
    TK_STR,       // 文字列リテラル
} TokenKind;

typedef struct Token Token;

// トークン型
struct Token {
    TokenKind kind;  // トークンの型
    Token *next;     // 次の入力トークン
    int val;         // kindがTK_NUMの場合、その数値
    char *str;       // トークン文字列
    int len;         // トークンの長さ

    char *contents;  // 文字列リテラル(終端文字を含む)
    char cont_len;   // 文字列リテラルの長さ
};

extern Token *token;
extern char *user_input;
extern char *filename;

void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
Token *tokenize();
bool consume(char *op);
bool match(char *op);
Token *consume_ident();
Token *consume_str();
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
    ND_ADD,       // num + num
    ND_PTR_ADD,   // ptr + num or num + ptr
    ND_SUB,       // num - num
    ND_PTR_SUB,   // ptr - num or num - ptr
    ND_PTR_DIFF,  // ptr - ptr
    ND_MUL,       // *
    ND_DIV,       // /
    ND_EQ,        // ==
    ND_NE,        // !=
    ND_LT,        // <
    ND_LE,        // <=
    ND_ASSIGN,    // =
    ND_VAR,       // ローカル変数
    ND_NUM,       // 整数
    ND_RETURN,    // return
    ND_IF,        // if
    ND_WHILE,     // while
    ND_FOR,       // for
    ND_BLOCK,     // ブロック
    ND_FUNCCALL,  // 関数呼び出し
    ND_ADDR,      // 単項 &
    ND_DEREF,     // 単項 *
    ND_NULL,      // null
} NodeKind;

// 変数の型
typedef struct Var Var;
struct Var {
    char *name;     // 変数の名前
    Type *type;     // 変数の型
    bool is_local;  // ローカル変数か

    // ローカル変数
    int offset;  // RBPからのオフセット

    // グローバル変数
    char *contents;
    int cont_len;
};

typedef struct VarList VarList;
struct VarList {
    VarList *next;
    Var *var;
};

typedef struct Node Node;

// 抽象構文木のノードの型
struct Node {
    NodeKind kind;  // ノードの型
    Node *next;     // 次のノード
    Type *type;     // 型
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

    // ND_VAR用
    Var *var;
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

typedef struct Program Program;
struct Program {
    VarList *globals;
    Function *funcs;
};

Program *program();

//
// type.c
//

typedef enum {
    INT,    // 整数
    PTR,    // ポインタ
    ARRAY,  // 配列
    CHAR,   // 文字
} TypeKind;

// 型を表す型
struct Type {
    TypeKind ty;
    int size;  // sizeofの返り値
    struct Type *ptr_to;
    int array_len;  // 配列の要素数
};

Type *char_type;
Type *int_type;

Type *pointer_to(Type *base);
Type *array_of(Type *base, int len);
bool is_integer(Type *type);
void add_type(Node *node);

//
// codegen.c
//

void codegen(Program *prog);