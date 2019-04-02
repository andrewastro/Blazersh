#include <stdio.h>


//takes a value from stdin and performs value-cubed bitshifts
//used to study active background processes
int main() {
    int x, i, j, k, sum = 2;
    scanf("%d", &x);
    for (i = 0; i < x; i++) {
        for (j = 0; j < x; j++) {
            for (k = 0; k < x; k++) {
                sum = sum * 2;
                sum = sum / 2;
            }
        }
    }
    return 0;
}
