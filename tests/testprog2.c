#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {
    int x, y, result;
    scanf("%d", &x);
    scanf("%d", &y);
    if (strcmp(argv[1], "add") == 0) {
        result = x + y;
	printf("%d\n\r", result);
    } else if (strcmp(argv[1], "sub") == 0) {
        result = x - y;
	printf("%d\n\r", result);
    } else {
        printf("invalid argument: provide add or sub\n\r");
    }
    return 0;
}
