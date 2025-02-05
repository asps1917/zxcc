#define _GNU_SOURCE
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct Type Type;
typedef struct Member Member;
typedef struct Initializer Initializer;

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
void warn(Token *tok, char *fmt, ...);
Token *tokenize();
Token *consume(char *op);
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
    ND_ADD,         // num + num
    ND_PTR_ADD,     // ptr + num or num + ptr
    ND_SUB,         // num - num
    ND_PTR_SUB,     // ptr - num or num - ptr
    ND_PTR_DIFF,    // ptr - ptr
    ND_MUL,         // *
    ND_DIV,         // /
    ND_BITAND,      // &
    ND_BITOR,       // |
    ND_BITXOR,      // ^
    ND_SHL,         // <<
    ND_SHR,         // >>
    ND_EQ,          // ==
    ND_NE,          // !=
    ND_LT,          // <
    ND_LE,          // <=
    ND_ASSIGN,      // =
    ND_TERNARY,     // ?:
    ND_PRE_INC,     // pre ++
    ND_PRE_DEC,     // pre --
    ND_POST_INC,    // post ++
    ND_POST_DEC,    // post --
    ND_ADD_EQ,      // +=
    ND_PTR_ADD_EQ,  // +=
    ND_SUB_EQ,      // -=
    ND_PTR_SUB_EQ,  // -=
    ND_MUL_EQ,      // *=
    ND_DIV_EQ,      // /=
    ND_SHL_EQ,      // <<=
    ND_SHR_EQ,      // >>=
    ND_BITAND_EQ,   // &=
    ND_BITOR_EQ,    // |=
    ND_BITXOR_EQ,   // ^=
    ND_COMMA,       // ,
    ND_MEMBER,      // . (構造体のメンバアクセス)
    ND_VAR,         // ローカル変数
    ND_NUM,         // 整数
    ND_RETURN,      // return
    ND_EXPR_STMT,   // Expression statement
    ND_IF,          // if
    ND_WHILE,       // while
    ND_FOR,         // for
    ND_DO,          // "do"
    ND_SWITCH,      // "switch"
    ND_CASE,        // "case"
    ND_BLOCK,       // ブロック
    ND_BREAK,       // "break"
    ND_CONTINUE,    // "continue"
    ND_GOTO,        // "goto"
    ND_LABEL,       // ラベル付きstatement
    ND_FUNCCALL,    // 関数呼び出し
    ND_ADDR,        // 単項 &
    ND_DEREF,       // 単項 *
    ND_NULL,        // null
    ND_STMT_EXPR,   // Statement expression
    ND_CAST,        // 型のキャスト
    ND_NOT,         // !
    ND_BITNOT,      // ~
    ND_LOGAND,      // &&
    ND_LOGOR,       // ||
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
    bool is_static;
    Initializer *initializer;
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
    long val;       // kindがND_NUMの場合のみ使う

    // if, while, for文用
    Node *cond;  // 条件式
    Node *then;  // 条件式を満たす場合の実行処理
    Node *els;   // 条件式を満たさない場合の実行処理
    Node *init;  // for文の初期化処理
    Node *post;  // for文のループ一周終了時処理

    // ブロック
    Node *block;

    // 構造体のメンバアクセス
    Member *member;

    // 関数呼び出し
    char *func_name;
    Node *args;

    // Goto or ラベル付きstatement
    char *label_name;

    // Switch-case
    Node *case_next;
    Node *default_case;
    int case_label;
    int case_end_label;

    // ND_VAR用
    Var *var;
};

// グローバル変数の初期化子。グローバル変数は以下の要素によって初期化可能
// - 定数式
// - 他のグローバル変数へのポインタ
struct Initializer {
    Initializer *next;

    // 定数式
    int sz;
    long val;

    // 他のグローバル変数へのポインタ
    char *label;
    long addend;
};

typedef struct Function Function;
struct Function {
    Function *next;
    char *name;
    VarList *args;
    bool is_static;
    bool has_varargs;

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
    VOID,
    BOOL,
    CHAR,
    SHORT,
    INT,
    LONG,
    PTR,     // ポインタ
    ARRAY,   // 配列
    STRUCT,  // 構造体
    FUNC,    // 関数
    ENUM,    // enum
} TypeKind;

// 型を表す型
struct Type {
    TypeKind ty;
    int size;            // sizeofの返り値
    int align;           // アライメント
    bool is_incomplete;  // 不完全な型か

    struct Type *ptr_to;
    int array_len;    // 配列の要素数
    Member *members;  // 構造体
    Type *return_ty;  // 関数の戻り値の型
};

// 構造体のメンバ
struct Member {
    Member *next;
    Type *ty;
    Token *tok;
    char *name;
    int offset;
};

extern Type *void_type;
extern Type *bool_type;
extern Type *char_type;
extern Type *short_type;
extern Type *int_type;
extern Type *long_type;

Type *pointer_to(Type *base);
Type *array_of(Type *base, int len);
bool is_integer(Type *type);
void add_type(Node *node);
Type *func_type(Type *return_ty);
Type *enum_type();
Type *struct_type();
int align_to(int n, int align);

//
// codegen.c
//

void codegen(Program *prog);