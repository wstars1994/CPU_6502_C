
#include <stdio.h>
#include <malloc.h>

void compile(FILE *fp){
    char ch[256] = {0};
    while (!feof(fp)) {
        int line = 1;
        fgets(ch,sizeof(ch) ,fp);
        char *ins_mode = malloc(4);
        char opcode[4] = {-1};
        char mode = '0';
        for (int i = 0; i < sizeof(ch); ++i) {
            char c = ch[i];
            if(c != ';') {
                if(i <= 2) {
                    if((c>='A' && c<='Z') || (c>='a' && c<='z')) {
                        ins_mode[i] = c;
                    } else {
                        printf("Instruction error! line:%d,pos:%d\n",line,i+1);
                        return;
                    }
                }else if(i == 3 && c!= ' ') {
                    printf("Error! expected space! line:%d,pos:%d\n",line,i+1);
                    return;
                }else if(i == 4) {
                    if(c!= '#' && c!= '$' && c!= '('){
                        printf("Error! expected '#' or '$' or '('! line:%d,pos:%d\n",line,i+1);
                        return;
                    }
                    mode = c;
                } else if(mode == '#') {
                    if(i == 5) {
                        if(c != '$'){
                            printf("Error! expected '$'! line:%d,pos:%d\n",line,i+1);
                            return;
                        }
                        ins_mode[3] = '1';
                    }else if(i > 5 && c!=' ') {
                        opcode[i-6] = c;
                    }
                }else if(mode == '$') {
                    if(i == 5) {
                        ins_mode[3] = '2';
                    }
                    if(c!=' ') {
                        opcode[i-5] = c;
                    }
                }else if(mode == '(') {
                    if(i == 5) {
                        ins_mode[3] = '2';
                    }
                    if(c!=' ') {
                        opcode[i-5] = c;
                    }
                }
            } else {
                //注释跳过
                break;
            }
        }
        printf("%s %s \n",ins_mode,opcode);
        line++;
    }
}