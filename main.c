#include "zxcc.h"

static int align_to(int n, int align) { return (n + align - 1) & ~(align - 1); }

int main(int argc, char **argv) {
    if(argc != 2) {
        error("引数の個数が正しくありません");
        return 1;
    }

    // トークナイズしてパースする
    user_input = argv[1];
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