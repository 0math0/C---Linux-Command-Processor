#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <sys/wait.h>

sig_atomic_t flag_sinal = 0;

void handler (int signal_number) {
	flag_sinal=1;
}

void print_prompt() {

    char dir_atual[250];   
    char host[250];
    char log_name[250];
    char *home_dir = getenv("HOME");
	int i=0;

    getcwd(dir_atual, 500); //Salva em "dir_atual" o diretorio atual
    gethostname(host, 250); //Salva em "host" o hostname do usuário logado no terminal
    strcpy(log_name,getenv("LOGNAME")); //Salva em "log_name" o usuario logado no terminal

	if (strncmp(dir_atual, home_dir, strlen(home_dir))==0){
        strcpy(dir_atual,dir_atual+strlen(home_dir)-sizeof(char));
        dir_atual[0]='~';
	}
		
    printf("[MySh] %s@%s:%s$ ",log_name, host, dir_atual);

}

void ler_argumento (char *lista_argumentos[], char *lista_argumentos2[], struct sigaction sa,int handler) {

	int i=0,l=0,o=0,k=0;
	int comando_pipe=0;
	char comando[250]; //comando: toda string so stdin (saída do teclado)
	char *aux; //auxiliar para pegar os tokens (comandos)
	char *lista_comandos[250]; // lista de tokens (token=comando)
	int des_p[2];

	fgets(comando,250,stdin); //Recebe a saída do teclado (stdin)

	aux = strchr(comando,'|'); //Aux recebe toda a string comando
	while (aux!=NULL) { //Loop para descobrir se existe pipe no comando digitado
		if (strchr(aux + k,'|')) { //Se existir
			comando_pipe = 1;
			break;
		}
		k++;
	}
	//printf("Pipe: %d",pipe);

	if (comando_pipe == 1) {  //Se for digitado pipe
		aux = strtok(comando,"|"); //aux recebe os comandos antes do primeiro pipe
		lista_comandos[l] = aux;
		l++;
		
		while(aux !=NULL) {
			aux = strtok(NULL,"|"); //aux recebe os demais comandos separados por pipe
			lista_comandos[l]= aux;
			l++; //Conta o numero de comandos pelo pipe
		}

		o=0;

        if(pipe(des_p) == -1) {
          perror("Pipe failed");
          exit(1);
        }

		int flag = 1; //Flag para controle da posição no pipe (flag = 1 para read (esquerda do pipe) /flag = 0 para write (direita do pipe))
		while(lista_comandos[o]!=NULL) {
			//Remove espaços a direita
			int j = strlen(lista_comandos[o])-1;
			while(isspace(lista_comandos[o][j])) lista_comandos[o][j--]='\0'; 

			i=0;
			//Separa a lista de comandos (a cada espaço encontrado, o comando é adicionado ao vetor lista_argumentos)
			lista_argumentos2[i] = strtok(lista_comandos[o]," "); //Primeiro argumento da lista recebe os argumentos separados por espaço
			while(lista_argumentos2[i] != NULL) {  //Le todos os argumentos
				i++;
				lista_argumentos2[i] = strtok(NULL," "); //Separa cada argumento em uma posição da lista
			}
			if (flag==1) { //Argumentos antes do pipe
				flag = 0;
				int f=0;
				for (f=0;f<i;f++) {
					lista_argumentos[f] = lista_argumentos2[f]; //Coloca os argumentos de leitura em um vetor separado
				}
			}
			o++;
		}

		if(fork() == 0)            //Primeiro fork (filho 1)
        {
            close(STDOUT_FILENO);  //Fechado a saida stdout
            dup(des_p[1]);         //Substituindo o stdout com pipe write 
            close(des_p[0]);       //Fechando pipe read do segundo filho
            close(des_p[1]);       //Fechando pipe write do segundo filho

            execvp(lista_argumentos[0], lista_argumentos); //Executa o comando especificado à esquerda do pipe (read)
            fprintf(stderr,"Erro ao executar \"%s\": %s\n",lista_argumentos[0], strerror(errno)); //Retorna erro, caso exista
			abort();
        }

        if(fork() == 0)            //Segundo fork (filho 2)
        {
            close(STDIN_FILENO);   //Fechando a entrada stdin
            dup(des_p[0]);         //Substituindo o stdin com pipe read
            close(des_p[1]);       //Fechando pipe write do segundo filho
            close(des_p[0]);	   //Fechando pipe read do segundo filho

            execvp(lista_argumentos2[0], lista_argumentos2); //Executa o comando especificado à direita do pipe (write)
            fprintf(stderr,"Erro ao executar \"%s\": %s\n",lista_argumentos[0], strerror(errno)); //Retorna erro, caso exista
			abort();
        }
		int child_status_1;
		int child_status_2;

        close(des_p[0]); 		   //Fechando pipe read do pai
        close(des_p[1]);		   //Fechando pipe read do pai
        wait(&child_status_1);     //Espera que o processo do filho 1 termine de ser executado
        wait(&child_status_2);     //Espera que o processo do filho 2 termine de ser executado
	}


	else { //Se nao foi digitado pipe
		//Remove espaços a direita
		int j = strlen(comando)-1;
		while(isspace(comando[j])) comando[j--]='\0';

		lista_argumentos[i] = strtok(comando," "); //Primeiro argumento da lista recebe os argumentos separados por espaço
		while(lista_argumentos[i] != NULL) {  //Le todos os argumentos
			i++;
			lista_argumentos[i] = strtok(NULL," "); //Separa cada argumento em uma posição da lista
			//printf("%s\n",lista_argumentos[i-1]);
		}

		if (strcmp(lista_argumentos[0],"exit") == 0) { //Comando exit
			exit(0); 
		}
		else if(flag_sinal) { //Se recebeu um sinal
			printf("\n"); 
			flag_sinal = 0; //Ignora o sinal recebido
			return;
		}
		else if (feof(stdin)) { //Se o valor lido foi Crtl+D (end of file)
			printf("\n");
			exit(0); //Sai do programa
		}
		else if (strcmp(lista_argumentos[0],"cd") == 0) { //Comando cd
			if (lista_argumentos[1]== NULL || strcmp(lista_argumentos[1],"~") == 0) {
				chdir(getenv("HOME")); //chdir(): muda o diretorio atual
			}
			else if (chdir(lista_argumentos[1]) != 0) { //Muda o diretorio especificado, caso ele exista
				fprintf (stderr,"Erro ao executar \"%s\": %s\n",lista_argumentos[0], strerror(errno)); //Retorna erro
				abort();
			}
		}
		else { //Se for outro comando
			pid_t child_pid = fork(); 
			if (child_pid != 0){ //Processo pai
				int child_status;
				wait(&child_status); //Espera que o processo filho termine de ser executado
			}
			else { //Processo filho
				execvp(lista_argumentos[0], lista_argumentos); //Executa o comando especificado
				fprintf(stderr,"Erro ao executar \"%s\": %s\n",lista_argumentos[0], strerror(errno)); //Retorna erro, caso exista
				abort();
			}
		}
	}
}

int main() {

	struct sigaction sa;
	char *lista_argumentos[30];
	char *lista_argumentos2[30];
	int sinal_flag=1;

	//Iniciando o sinal da flag
	memset(&sa, 0, sizeof(sa)); //Copia o valor 0 para o bloco de memória "sa"
	sa.sa_handler = &handler; //Manipulador de sinal
	sigaction(SIGINT,&sa,NULL); //Determina o que o programa deve fazer ao receber o sinal Crtl+C (no caso, nao deve fazer nada)
	sigaction(SIGTSTP,&sa,NULL); //Determina o que o programa deve fazer ao receber o sinal Crtl+Z (no caso, nao deve fazer nada)

	do {
		print_prompt();
		ler_argumento(lista_argumentos,lista_argumentos2,sa,sinal_flag);
	}while(1);

}
