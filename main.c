#include "zxcc.h"

int main(int argc, char **argv) {
    if(argc != 2) {
        error("引数の個数が正しくありません");
        return 1;
    }

    // トークナイズしてパースする
    user_input = argv[1];
    token = tokenize();
    Function *prog = program();

    // ローカル変数のオフセット設定 & スタックサイズ算出
    // localsリスト上の各ローカル変数に8byteずつ割り当てる
    for(Function *func = prog; func; func = func->next) {
        int offset = 0;
        for(VarList *vl = func->locals; vl; vl = vl->next) {
            LVar *lvar = vl->var;
            offset += 8;
            lvar->offset = offset;
        }
        func->stack_size = offset;
    }

    codegen(prog);

    return 0;
}