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
    fprintf(stderr, "%*s", pos, "");  // pos個の空白を出力
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

// 次のトークンが期待している記号のときには真を返す。(トークンは読み進めない)
// それ以外の場合には偽を返す。
bool match(char *op) {
    if(token->kind != TK_RESERVED || strlen(op) != token->len ||
       memcmp(token->str, op, token->len))
        return false;
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

// 次のトークンが文字列の場合、トークンを1つ読み進めてそのトークンを返す。
// それ以外の場合にはNULLを返す。
Token *consume_str() {
    if(token->kind == TK_STR) {
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
    if(token->kind != TK_NUM) error_at(token->str, "数ではありません");
    int val = token->val;
    token = token->next;
    return val;
}
// 次のトークンが識別子(TK_IDENT)の場合、トークンを1つ読み進めてその文字列を返す。
// それ以外の場合にはエラーを報告する。
char *expect_ident() {
    if(token->kind != TK_IDENT) error_at(token->str, "識別子ではありません");

    char *c = strndup(token->str, token->len);
    token = token->next;
    return c;
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

// 文字列が予約語か判定する。
// 予約語だった場合その予約語を返す。そうでない場合、NULLを返す。
static char *is_reserved(char *p) {
    static char *keywords[] = {"if",  "else", "while", "for",
                               "int", "char", "sizeof"};

    for(int i = 0; i < sizeof(keywords) / sizeof(*keywords); i++) {
        int len_keyword = strlen(keywords[i]);
        if(strncmp(p, keywords[i], len_keyword) == 0 &&
           !is_alnum(p[len_keyword])) {
            return keywords[i];
        }
    }
    return NULL;
}

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

        // 予約語
        char *keyword = is_reserved(p);
        if(keyword) {
            int len = strlen(keyword);
            cur = new_token(TK_RESERVED, cur, p, len);
            p += len;
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
        if(strchr("+-*/(){}[]<>;=,&", *p)) {
            cur = new_token(TK_RESERVED, cur, p++, 1);
            continue;
        }

        // 文字列リテラル
        if(*p == '"') {
            p++;
            char *q = p;
            while(*p != '"') {
                p++;
            }
            cur = new_token(TK_STR, cur, q, (p - q));
            // strndupはヌル終端文字を追加した文字列を返す
            cur->contents = strndup(q, (p - q));
            cur->cont_len = p - q + 1;
            p++;
            continue;
        }

        // 変数、関数
        if(is_alpha(*p)) {
            char *q = p;
            while(is_alnum(*p)) {
                p++;
            }
            cur = new_token(TK_IDENT, cur, q, (p - q));
            continue;
        }

        // 数値
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