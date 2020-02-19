#include "zxcc.h"

// 現在着目しているトークン
Token *token;
// 入力プログラム
char *user_input;

// エラーを報告するための関数
// printfと同じ引数を取る
void error(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

// エラー箇所を報告する
void error_at(char *loc, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    int pos = loc - user_input;
    fprintf(stderr, "%s\n", user_input);
    fprintf(stderr, "%*s", pos, ""); // pos個の空白を出力
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

// 次のトークンが期待している記号のときには、トークンを1つ読み進めて
// 真を返す。それ以外の場合には偽を返す。
bool consume(char *op) {
    if(token->kind != TK_RESERVED || strlen(op) != token->len ||
       memcmp(token->str, op, token->len))
        return false;
    token = token->next;
    return true;
}

// 次のトークンが識別子の場合、トークンを1つ読み進めてそのトークンを返す。
// それ以外の場合にはNULLを返す。
Token *consume_ident() {
    if(token->kind == TK_IDENT) {
        Token *retval = token;
        token = token->next;
        return retval;
    }
    return NULL;
}

// 次のトークンがreturnの場合、トークンを1つ読み進めてそのトークンを返す。
// それ以外の場合にはNULLを返す。
Token *consume_return() {
    if(token->kind == TK_RETURN) {
        Token *retval = token;
        token = token->next;
        return retval;
    }
    return NULL;
}

// 次のトークンが期待している記号のときには、トークンを1つ読み進める。
// それ以外の場合にはエラーを報告する。
void expect(char *op) {
    if(token->kind != TK_RESERVED || strlen(op) != token->len ||
       memcmp(token->str, op, token->len))
        error_at(token->str, "'%s'ではありません", op);
    token = token->next;
}

// 次のトークンが数値の場合、トークンを1つ読み進めてその数値を返す。
// それ以外の場合にはエラーを報告する。
int expect_number() {
    if(token->kind != TK_NUM)
        error_at(token->str, "数ではありません");
    int val = token->val;
    token = token->next;
    return val;
}

bool at_eof() { return token->kind == TK_EOF; }

// 新しいトークンを作成してcurに繋げる
static Token *new_token(TokenKind kind, Token *cur, char *str, int len) {
    Token *tok = calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->str = str;
    tok->len = len;
    cur->next = tok;
    return tok;
}

// 文字がアルファベットか(アンダースコアを含む)判定する
static bool is_alpha(char c) {
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

// 文字がアルファベットか、もしくは数字であるか判定する
static bool is_alnum(char c) { return is_alpha(c) || ('0' <= c && c <= '9'); }

// 入力文字列user_inputをトークナイズしてそれを返す
Token *tokenize() {
    char *p = user_input;
    Token head;
    head.next = NULL;
    Token *cur = &head;

    while(*p) {
        // 空白文字をスキップ
        if(isspace(*p)) {
            p++;
            continue;
        }

        // return
        if(strncmp(p, "return", 6) == 0 && !is_alnum(p[6])) {
            cur = new_token(TK_RETURN, cur, p, 6);
            p += 6;
            continue;
        }

        // 2文字の演算子
        if(strncmp("<=", p, 2) == 0 || strncmp(">=", p, 2) == 0 ||
           strncmp("==", p, 2) == 0 || strncmp("!=", p, 2) == 0) {
            cur = new_token(TK_RESERVED, cur, p, 2);
            p += 2;
            continue;
        }

        // 1文字の演算子
        if(strchr("+-*/()<>;=", *p)) {
            cur = new_token(TK_RESERVED, cur, p++, 1);
            continue;
        }

        // ローカル変数
        if(is_alpha(*p)) {
            char *q = p;
            while(is_alnum(*p)) {
                p++;
            }
            cur = new_token(TK_IDENT, cur, q, (p - q));
            continue;
        }

        if(isdigit(*p)) {
            cur = new_token(TK_NUM, cur, p, 0);
            char *q = p;
            cur->val = strtol(p, &p, 10);
            cur->len = p - q;
            continue;
        }

        error_at(p, "トークナイズできません");
    }

    new_token(TK_EOF, cur, p, 0);
    return head.next;
}