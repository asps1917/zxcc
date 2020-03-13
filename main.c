#include "zxcc.h"

static int align_to(int n, int align) { return (n + align - 1) & ~(align - 1); }

// 指定されたファイルの内容を返す
static char *read_file(char *path) {
    // ファイルを開く
    FILE *fp = fopen(path, "r");
    if(!fp) error("cannot open %s: %s", path, strerror(errno));

    int filemax = 10 * 1024 * 1024;
    char *buf = malloc(filemax);
    int size = fread(buf, 1, filemax - 2, fp);
    if(!feof(fp)) error("%s: file too large");

    // ファイルが必ず"\n\0"で終わっているようにする
    if(size == 0 || buf[size - 1] != '\n') buf[size++] = '\n';
    buf[size] = '\0';
    fclose(fp);
    return buf;
}

int main(int argc, char **argv) {
    if(argc != 2) {
        error("引数の個数が正しくありません");
        return 1;
    }

    // トークナイズしてパースする
    filename = argv[1];
    user_input = read_file(filename);
    token = tokenize();
    Program *prog = program();

    // ローカル変数のオフセット設定 & スタックサイズ算出
    // localsリスト上の各ローカル変数に8byteずつ割り当てる
    for(Function *func = prog->funcs; func; func = func->next) {
        int offset = 0;
        for(VarList *vl = func->locals; vl; vl = vl->next) {
            Var *lvar = vl->var;
            offset += lvar->type->size;
            lvar->offset = offset;
        }
        func->stack_size = align_to(offset, 8);
    }

    codegen(prog);

    return 0;
}