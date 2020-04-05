#include "zxcc.h"

// 現在着目しているトークン
Token *token;
// 入力プログラム
char *user_input;
// 入力ファイル名
char *filename;

// エラーを報告するための関数
// printfと同じ引数を取る
void error(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

// エラーが起きた場所を報告する
// 下のようなフォーマットでエラーメッセージを表示する
//
// foo.c:10: x = y + + 5;
//                   ^ 式ではありません
static void verror_at(char *loc, char *fmt, va_list ap) {
    // locが含まれている行の開始地点と終了地点を取得
    char *line = loc;
    while(user_input < line && line[-1] != '\n') line--;

    char *end = loc;
    while(*end != '\n') end++;

    // 見つかった行が全体の何行目なのかを調べる
    int line_num = 1;
    for(char *p = user_input; p < line; p++)
        if(*p == '\n') line_num++;

    // 見つかった行を、ファイル名と行番号と一緒に表示
    int indent = fprintf(stderr, "%s:%d: ", filename, line_num);
    fprintf(stderr, "%.*s\n", (int)(end - line), line);

    // エラー箇所を"^"で指し示して、エラーメッセージを表示
    int pos = loc - line + indent;
    fprintf(stderr, "%*s", pos, "");  // pos個の空白を出力
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
}

// エラーが起きた場所を報告してプログラムを終了する
void error_at(char *loc, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    verror_at(loc, fmt, ap);
    exit(1);
}

// 警告文を表示する
void warn(Token *tok, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    verror_at(tok->str, fmt, ap);
}

// 次のトークンが期待している記号のときには、トークンを1つ読み進めて
// そのトークンを返す。それ以外の場合にはNULLを返す。
Token *consume(char *op) {
    if(token->kind != TK_RESERVED || strlen(op) != token->len ||
       memcmp(token->str, op, token->len))
        return NULL;
    Token *tok = token;
    token = token->next;
    return tok;
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
    static char *keywords[] = {
        "if",     "else",   "while",   "for",    "int",      "char",
        "sizeof", "struct", "typedef", "short",  "long",     "void",
        "_Bool",  "enum",   "static",  "break",  "continue", "goto",
        "switch", "case",   "default", "extern", "_Alignof", "do"};

    for(int i = 0; i < sizeof(keywords) / sizeof(*keywords); i++) {
        int len_keyword = strlen(keywords[i]);
        if(strncmp(p, keywords[i], len_keyword) == 0 &&
           !is_alnum(p[len_keyword])) {
            return keywords[i];
        }
    }
    return NULL;
}

// エスケープシーケンスを取得する
static char get_escape_char(char c) {
    switch(c) {
        case 'a':
            return '\a';
        case 'b':
            return '\b';
        case 't':
            return '\t';
        case 'n':
            return '\n';
        case 'v':
            return '\v';
        case 'f':
            return '\f';
        case 'r':
            return '\r';
        case 'e':
            return 27;
        case '0':
            return 0;
        default:
            return c;
    }
}

// 文字列リテラルを読み出す
static Token *read_string_literal(Token *cur, char *start) {
    char *p = start + 1;
    char buf[1024];
    int len = 0;

    for(;;) {
        if(len == sizeof(buf)) error_at(start, "string literal too large");
        if(*p == '\0') error_at(start, "unclosed string literal");
        if(*p == '"') break;

        if(*p == '\\') {
            p++;
            buf[len++] = get_escape_char(*p++);
        } else {
            buf[len++] = *p++;
        }
    }

    Token *tok = new_token(TK_STR, cur, start, p - start + 1);
    tok->contents = malloc(len + 1);
    memcpy(tok->contents, buf, len);
    tok->contents[len] = '\0';
    tok->cont_len = len + 1;
    return tok;
}

static Token *read_char_literal(Token *cur, char *start) {
    char *p = start + 1;
    if(*p == '\0') {
        error_at(start, "文字リテラルが閉じられていません");
    }

    char c;
    if(*p == '\\') {
        p++;
        c = get_escape_char(*p++);
    } else {
        c = *p++;
    }

    if(*p != '\'') {
        error_at(start, "長すぎる文字リテラルです");
    }
    p++;

    Token *tok = new_token(TK_NUM, cur, start, p - start);
    tok->val = c;
    return tok;
}

static Token *read_int_literal(Token *cur, char *start) {
    char *p = start;

    int base;
    if(!strncasecmp(p, "0x", 2) && is_alnum(p[2])) {
        p += 2;
        base = 16;
    } else if(!strncasecmp(p, "0b", 2) && is_alnum(p[2])) {
        p += 2;
        base = 2;
    } else if(*p == '0') {
        base = 8;
    } else {
        base = 10;
    }

    long val = strtol(p, &p, base);
    if(is_alnum(*p)) {
        error_at(p, "無効な数字です");
    }

    Token *tok = new_token(TK_NUM, cur, start, p - start);
    tok->val = val;
    return tok;
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

        // 行コメントをスキップ
        if(strncmp(p, "//", 2) == 0) {
            p += 2;
            while(*p != '\n') p++;
            continue;
        }

        // ブロックコメントをスキップ
        if(strncmp(p, "/*", 2) == 0) {
            char *q = strstr(p + 2, "*/");
            if(!q) error_at(p, "コメントが閉じられていません");
            p = q + 2;
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

        // 3文字の演算子
        if(strncmp("<<=", p, 3) == 0 || strncmp(">>=", p, 3) == 0) {
            cur = new_token(TK_RESERVED, cur, p, 3);
            p += 3;
            continue;
        }

        // 2文字の演算子
        if(strncmp("<=", p, 2) == 0 || strncmp(">=", p, 2) == 0 ||
           strncmp("==", p, 2) == 0 || strncmp("!=", p, 2) == 0 ||
           strncmp("->", p, 2) == 0 || strncmp("++", p, 2) == 0 ||
           strncmp("--", p, 2) == 0 || strncmp("+=", p, 2) == 0 ||
           strncmp("-=", p, 2) == 0 || strncmp("*=", p, 2) == 0 ||
           strncmp("/=", p, 2) == 0 || strncmp("&&", p, 2) == 0 ||
           strncmp("||", p, 2) == 0 || strncmp("<<", p, 2) == 0 ||
           strncmp(">>", p, 2) == 0 || strncmp("&=", p, 2) == 0 ||
           strncmp("|=", p, 2) == 0 || strncmp("^=", p, 2) == 0) {
            cur = new_token(TK_RESERVED, cur, p, 2);
            p += 2;
            continue;
        }

        // 1文字の演算子
        if(strchr("+-*/(){}[]<>;:=,.&!?~|^", *p)) {
            cur = new_token(TK_RESERVED, cur, p++, 1);
            continue;
        }

        // 文字列リテラル
        if(*p == '"') {
            cur = read_string_literal(cur, p);
            p += cur->len;
            continue;
        }

        // 文字リテラル
        if(*p == '\'') {
            cur = read_char_literal(cur, p);
            p += cur->len;
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
            cur = read_int_literal(cur, p);
            p += cur->len;
            continue;
        }

        error_at(p, "トークナイズできません");
    }

    new_token(TK_EOF, cur, p, 0);
    return head.next;
}