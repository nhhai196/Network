#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void token_payload(char * buffer, char * tokens[]){
	int i = 0;
	char * temp; 
	temp = strtok(buffer, "$");
	while (temp != NULL){
		tokens[i++] = temp;
		printf("%s\n", temp);
		temp = strtok(NULL, "$");
	}
}

int main(){
	char buffer[] = "$aaaaaaaaaaaaaaaaa$bb$cc$dd$ee$ff$";
	char * tokens[10];
	token_payload(buffer, tokens);
	return 0;
}
