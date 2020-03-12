#!/bin/bash
cat <<EOF | gcc -xc -c -o tmp2.o -
int ret3() { return 3; }
int ret5() { return 5; }
int add(int x, int y) { return x+y; }
int sub(int x, int y) { return x-y; }

int add6(int a, int b, int c, int d, int e, int f) {
  return a+b+c+d+e+f;
}
EOF

try() {
  expected="$1"
  input="$2"

  ./zxcc "$input" > tmp.s
  gcc -static -o tmp tmp.s tmp2.o
  ./tmp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but got $actual"
    exit 1
  fi
}

try 0 'int main() { return 0; }'
try 42 'int main() { return 42; }'
try 21 'int main() { return 5+20-4; }'
try 41 'int main() { return 12 + 34 - 5 ; }'
try 47 'int main() { return 5+6*7; }'
try 15 'int main() { return 5*(9-6); }'
try 4 'int main() { return (3+5)/2; }'
try 10 'int main() { return -10+20; }'
try 20 'int main() { return -(-3*+5)+5; }'

try 0 'int main() { return 0==1; }'
try 1 'int main() { return 42==42; }'
try 1 'int main() { return 0!=1; }'
try 0 'int main() { return 42!=42; }'

try 1 'int main() { return 0<1; }'
try 0 'int main() { return 1<1; }'
try 0 'int main() { return 2<1; }'
try 1 'int main() { return 0<=1; }'
try 1 'int main() { return 1<=1; }'
try 0 'int main() { return 2<=1; }'
try 1 'int main() { return 1>0; }'
try 0 'int main() { return 1>1; }'
try 0 'int main() { return 1>2; }'
try 1 'int main() { return 1>=0; }'
try 1 'int main() { return 1>=1; }'
try 0 'int main() { return 1>=2; }'

try 1 'int main() { return 1;3;return 5; }'
try 3 'int main() { 1;return 3;5; }'
try 5 'int main() { 1;3;return 5; }'

try 3 'int main() { int a = 3; return a; }'
try 14 'int main() { int a; int b; a = 3; b = 5 * 6 - 8; return a + b / 2; }'
try 6 'int main() { int foo; int bar; foo = 1;bar = 2 + 3;return foo + bar; }'
try 24 'int main() { int foo_123; int _aiueo99; foo_123 = 12;_aiueo99 = -1 + 3;return foo_123*_aiueo99; }'

try 3 'int main() { if (1) return 3; }'
try 3 'int main() { if (1-1) return 2; return 3; }'
try 2 'int main() { if (2-1) return 2; return 3; }'
try 2 'int main() { int foo=3; foo=foo-3; if (foo) return 1; else return 2; }'

try 1 'int main() { while(1) return 1; return 2; }'
try 2 'int main() { while(0) return 1; return 2; }'
try 8 'int main() { int foo = 20; while(foo>=10) foo = foo-3; return foo; }'

try 10 'int main() { int limit = 10; int a; int b; for(a=0; a<limit; a = a+1) b=1; return a; }'
try 10 'int main() { int limit = 10; int a = 0; int b; for(; a<limit; a = a+1) b=1; return a; }'
try 0 'int main() { int limit ; int a; limit = 10;for(a = 0;; a = a+1) return a; }'
try 10 'int main() { int limit = 10; int a; for(a=0; a<limit;) a = a+1; return a; }'
try 10 'int main() { int limit = 10; for(;;) return limit; return 0; }'

try 30 'int main() { int limit; int b;{limit = 10; b=0;} int a; for(a=0; a<limit; a = a+1) {b=b+1; b=b+2; } return b; }'
try 10 'int main() { int a = 0; while(1) {if(a==10) return a; a =a+1;} }'
try 20 'int main() { int a; int b; a = 0; b=0; while(1) {if(a==10) {return b; } a =a+1; b = b+2;} }'
try 21 'int main() { int a; int b; a = 0; b=0; while(1) {if(a==10) b = b+1; if(a==20) return b; a =a+1; b=b+1;} }'

try 3 'int main() { return ret3(); }'
try 5 'int main() { return ret5(); }'
try 8 'int main() { return add(3, 5); }'
try 2 'int main() { return sub(5, 3); }'
try 21 'int main() { return add6(1,2,3,4,5,6); }'

try 10 'int main() { int a; int b; a = 1; b =2; b = a+b; return b + ret7(); } int ret7() { int a = 3; int b = 4; a = a+b; return a;}'

try 7 'int main() { return add2(3,4); } int add2(int x,int y) { return x+y; }'
try 1 'int main() { return sub2(4,3); } int sub2(int x,int y) { return x-y; }'
try 55 'int main() { return fib(9); } int fib(int x) { if (x<=1) return 1; return fib(x-1) + fib(x-2); }'
try 17 'int main() { return foo(6,4); } int foo(int x,int y) { int a; int b; a = 1; b =2; a = a + b; return x + y - a + add2(3, 7); } int add2(int x,int y) { return x+y; }'
try 21 'int main() {return add_6(1,2,3,4,5,6);} int add_6(int a,int b,int c,int d,int e,int f) {return a+b+c+d+e+f; }'

try 3 'int main() { int x = 3; int *y = &x; return *y;}'
try 3 'int main() { int x; int y; x = 3; y = 5; int *z = &y - 1; return *z;}'
try 5 'int main() { int x; int y; x = 3; y = 5; int *z = &x + 1; return *z;}'
try 5 'int main() { int x=3; int y=5; return *(&x+1); }'
try 5 'int main() { int x=3; int y=5; return *(1+&x); }'
try 3 'int main() { int x=3; int y=5; return *(&y-1); }'
try 2 'int main() { int x=3; return (&x+2)-&x; }'
try 3 'int main() { int x; int *y; x=3; y=&x; int **z=&y; return **z; }'
try 5 'int main() { int x; int *y; x=3; y=&x; *y=5; return x; }'
try 7 'int main() { int x; int y; x=3; y=5; *(&x+1)=7; return y; }'
try 7 'int main() { int x; int y; x=3; y=5; *(&y-1)=7; return x; }'
try 8 'int main() { int x=3; int y=5; return foo(&x, y); } int foo(int *x, int y) { return *x + y; }'

try 8 'int main() { return sizeof(1); }'
try 8 'int main() { return sizeof(sizeof(1)); }'
try 8 'int main() { int x; return sizeof(x); }'
try 8 'int main() { int *x; return sizeof(x); }'
try 8 'int main() { int x; return sizeof(x + 3); }'
try 8 'int main() { int *x; return sizeof(x + 3); }'
try 8 'int main() { int *x; return sizeof(*x); }'
try 24 'int main() { int x[3]; return sizeof(x); }'

try 3 'int main() { int x[2]; int *y=&x; *y=3; return *x; }'

try 3 'int main() { int x[3]; *x=3; *(x+1)=4; *(x+2)=5; return *x; }'
try 4 'int main() { int x[3]; *x=3; *(x+1)=4; *(x+2)=5; return *(x+1); }'
try 5 'int main() { int x[3]; *x=3; *(x+1)=4; *(x+2)=5; return *(x+2); }'

try 3 'int main() { int x[3]; x[0]=3; x[1]=4; x[2]=5; return *x; }'
try 3 'int main() { int x[3]; *x=3; x[1]=4; x[2]=5; return *x; }'
try 4 'int main() { int x[3]; *x=3; x[1]=4; x[2]=5; return *(x+1); }'
try 5 'int main() { int x[3]; *x=3; x[1]=4; x[2]=5; return *(x+2); }'
try 5 'int main() { int x[3]; *x=3; x[1]=4; 2[x]=5; return *(x+2); }'

try 0 'int x; int main() { return x; }'
try 3 'int x; int main() { x=3; return x; }'
try 3 'int x; int main() { x=3; foo(); return x;} int foo() {int x=4; return 0;}'
try 0 'int x[4]; int main() { x[0]=0; x[1]=1; x[2]=2; x[3]=3; return x[0]; }'
try 1 'int x[4]; int main() { x[0]=0; x[1]=1; x[2]=2; x[3]=3; return x[1]; }'
try 2 'int x[4]; int main() { x[0]=0; x[1]=1; x[2]=2; x[3]=3; return x[2]; }'
try 3 'int x[4]; int main() { x[0]=0; x[1]=1; x[2]=2; x[3]=3; return x[3]; }'

try 8 'int x; int main() { return sizeof(x); }'
try 32 'int x[4]; int main() { return sizeof(x); }'

try 3 'int main() { char x[3]; x[0] = -1; x[1] = 2; int y = 4; return x[0] + y; }'
try 1 'int main() { char x[4]; x[0] = 1; x[1] = 2; x[2] = 3; x[3] = 4; return x[0];}'
try 2 'int main() { char x[4]; x[0] = 1; x[1] = 2; x[2] = 3; x[3] = 4; return x[1];}'
try 3 'int main() { char x[4]; x[0] = 1; x[1] = 2; x[2] = 3; x[3] = 4; return x[2];}'
try 4 'int main() { char x[4]; x[0] = 1; x[1] = 2; x[2] = 3; x[3] = 4; return x[3];}'
try 2 'int main() { char x[4]; x[0] = -1; x[1] = 2; x[2] = -3; x[3] = 4; return x[0] + x[1] + x[2] + x[3];}'

try 1 'int main() { char x=1; char y=2; return x; }'
try 2 'int main() { char x=1; char y=2; return y; }'
try 1 'int main() { char x; return sizeof(x); }'
try 10 'int main() { char x[10]; return sizeof(x); }'
try 1 'int main() { return sub_char(7, 3, 3); } int sub_char(char a, char b, char c) { return a-b-c; }'

echo OK