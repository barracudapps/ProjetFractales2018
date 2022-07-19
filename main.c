/*
 * main.c : calcul des fractales
 * Lit des fichiers afin d y recuperer des fractales a calculee
 * Genere des fichiers bmp pour toutes les fractales de valeur moyenne la plus elevee.
 * Utilise la parallelisation du travail a l'aide de threads afin d'optimiser
 * le temps de calcul.
 *
 *
 * Utilisation du programme :
 * ./main [-d] [--maxthreads n] FICHIER1 FICHIER2 ... FICHIERN FICHIER_OUT
 *
 * avec [] les parametres optionnels.
 *
 * Les fractales optimales auront le nom: FICHIER_OUT_nomfractale.bmp
 * Si -d specifie, on genere le fichier bmp pour toutes les fractales calculees.
 * Les fractales non optimales ont alors le nom: nomfractale.bmp
 * 
 * Valeurs par defauts :
 * 		- threads devant calculer les fractales (consommateurs) : 1
 *
 * Groupe 60
 * Auteurs : 	- Calbert Julien 33211500
 * 				- Lamotte Pierre 65441500
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "fractal.h"

#include <stdint.h>
#include "string.h"

#include <semaphore.h>
#include <pthread.h>
#include "main.h"

/* -------------------------------------------------------------------- *
 * 				Donnees initialisees (variables globales)
 * --------------------------------------------------------------------*/

int nbFiles;
char** files;
int printAllFrac = 0; //1 si il faut generer tous le BMP
int entre_standard = 0; //1 si il faut lire l entree standard, le code ne gere pas cet aspect
int N1;  //taille buffer 1
int N2;  //taille buffer 2

int nbFrac1 = 0; //nombre de fractales dans le buffer 1
int nbFrac2 = 0; //nombre de fractales dans le buffer 2
int nbFrac;      //compte aussi le nombre de fractales dans le buffer 1 mais a pour but d identifier lorsque celui-ci est vide

struct fractal** buffer1; //buffer 1 contenant les pointeurs vers des fractales non encore calculee
struct fractal** buffer2; //buffer 2 contenant les pointeurs vers des fractales resolues

int currentLecture1 = 0;
int currentEcriture1  = 0;

int currentLecture2 = 0;
int currentEcriture2  = 0;

sem_t buffer1_full;  //compte le nombre de slot occuppe dans le buffer 1 
sem_t buffer1_empty; //compte le nombre de slot encore libre dans le buffer 1

sem_t buffer2_full;  //compte le nombre de slot occuppe dans le buffer 2
sem_t buffer2_empty; //compte le nombre de slot encore libre dans le buffer 2

int endProducer = 0; //0 si producteur a pas encore fini, 1 si il a fini
int endConsumer = 0; //0 si consommateurs ont pas encore fini , 1 si ils ont fini


pthread_mutex_t mutex_buffer1;
pthread_mutex_t mutex_buffer2;
pthread_mutex_t mutex_endProducer;
pthread_mutex_t mutex_endConsumer;
pthread_mutex_t mutex_nbFrac; //nombre de fractales actuellement dans le buffer1

double bestVal = 0; //valeur courante de la meilleure fractale

struct node* best = NULL;  //pointe vers le debut d une stack contenant les fractales de valeurs moyennes la plus elevee
struct node* other = NULL; //pointe vers le debut d une stack contenant les fractales de valeurs non maximum (uniquement utilise si -d specifie)

struct nodeName* firstName = NULL; //pointe vers le debut d une liste contenant tous les noms differents de fractales en cours de calcul
/*struct node
{
	struct fractal* f;
	struct node* next;
};
struct nodeName //structure pour stocker les noms des fractales
{
	char* s;
	struct nodeName* next;
};*/
/* -------------------------------------------------------------------- *
 * 				Differentes fonctions auxiliaires
 * --------------------------------------------------------------------*/
 
/*
 * ajoute la fractale f a la stack dont le premier element est pointe par *first 
 */
void push_stackFrac(struct fractal* f,struct node** first)
{
	struct node* new = (struct node*) malloc(sizeof(struct node));
	new->f = f;
	if(*first == NULL)
	{
		new->next = NULL;
		*first = new;
	}
	else
	{
		new->next = *first;
		*first = new;
	}
}

/*
 * return un pointeur vers la premiere fractale de la stack et enleve cette fractale de la stack 
 */
struct fractal* pop_stackFrac(struct node** first)
{
	if(*first == NULL) return NULL;
	else
	{
		struct fractal*  f = (*first)->f;
		struct node *destroy = *first;
		*first = (*first)->next;
		free(destroy);
		return f;
	}
}

/*
 *  concatene s1 suivi de s2 dans result
 */
void concate(char* s1, char* s2, char* result)
{
	int i = 0;
	while(s1[i]!='\0')
	{
		result[i] = s1[i];
		i++;
	}
	int j = 0;
	while(s2[j]!='\0')
	{
		result[i] = s2[j];
		i++;
		j++;
	}
	result[i] = '\0';
}

/*
 * cette fonction permet de negliger les lignes ayant un commentaire (commencant par #)
 * et les lignes vides
 * return 0 si une fractale suit (et replace le pointeur virtuel au debut de cette ligne)
 *        1 si il n y a plus de fractales dans le fichier
 */
int zap(FILE* file)
{
	char * line = NULL;
	size_t len = 0;
	ssize_t read;
	long pos = ftell(file);
	read = getline(&line, &len, file);
	if (read == -1) 
	{
		free(line); 
		return 1;
	}
	while(line[0] == '#' || line[0] == '\n')
	{
		pos = ftell(file);
		read = getline(&line, &len, file);
		if (read == -1)
		{
			free(line); 
			return 1;
		} 
	}
	fseek(file, pos, SEEK_SET);
	free(line);
	return 0;
}

/*
 *	compare les 2 string
 *  return 1 si pas les memes, 0 sinon 
 */
int compare(char* s1, char* s2)
{
	int bool = 1;
	int i = 0;
	while(s1[i] == s2[i] && bool == 1)
	{
		if(s1[i] == '\0')
		{
			bool = 0;
		}
		i++;
	}
	if(bool == 1) //pas les memes
	{
		return 1;
	}
	else //les memes
	{
		return 0;
	}
}
/*
 * verifie si il n y a pas une fractale de meme nom deja traitee
 * return 1 si le nom n etait pas deja present et ajoute le nom a la liste
 * return 0 si le nom etait deja present, free s et affiche message erreur dans stderr
 */
int check_name(char* s)
{
	if(firstName == NULL)
	{
		firstName = (struct nodeName*) malloc(sizeof(struct nodeName));
		firstName->s = s;		
		firstName->next = NULL;
		return 1;
	}
	struct nodeName* currentNode = firstName;
	struct nodeName* lastNode = NULL;
	while(currentNode != NULL)
	{
		int result = compare(currentNode->s, s);
		if(result == 0)
		{
			fprintf(stderr, "erreur: noms de fractale déjà utilisé\n");
			return 0;
		}
		else
		{
			lastNode = currentNode;
			currentNode = currentNode->next;
		}
	}
	lastNode->next = (struct nodeName*) malloc(sizeof(struct nodeName));
	lastNode->next->s = s;		
	lastNode->next->next = NULL;
	return 1;
}

/*
 * libere la memoire de la liste chainee contennant les noms des fractales calculees 
 */
void free_name()
{
	struct nodeName* currentNode = firstName;
	struct nodeName* nextNode = NULL;
	while(currentNode != NULL)
	{
		nextNode = currentNode->next;
		free(currentNode->s);
		free(currentNode);
		currentNode = nextNode;
	}
}


/* -------------------------------------------------------------------- *
 * 		 Differents threads : producteur, consommateurs, trieur
 * --------------------------------------------------------------------*/

void* producteur (void* arg)
{
	int nf; //numero du fichier courant (en lecture)
	for(nf=0;nf<nbFiles-1;nf++)
	{
		FILE* file = fopen((char*) files[nf], "r");
	    if(file == NULL) exit(1); //erreur a l'ouverture du fichier
	    else
	    { 
			struct fractal* f1;
			struct fractal* f2;
		    char name[64]; 
		    int size[2] = {0};
		    double complex[2] = {0};
		    int endWithComment = 0;
		    endWithComment = zap(file);
		    
		    fscanf(file, "%s %d %d %lf %lf \n", (char*)&name,  &size[0], &size[1], &complex[0], &complex[1]);
		    f1 = fractal_new(name, size[0], size[1], complex[0], complex[1]);
		    if(f1 == NULL) { exit(1);}
		    
		    /* */
		    char* s1  = (char*) malloc(64*sizeof(char));
			int i = 0;
			while(name[i] != '\0')
			{
				s1[i] = name[i];
				i++;
			}
			s1[i] = '\0';
			int check_the_name  = check_name(s1);
			/* */
			
		    endWithComment =  zap(file);
		    int ret;
		    while((ret = fscanf(file, "%s %d %d %lf %lf \n", (char*)&name,  &size[0], &size[1], &complex[0], &complex[1])) != EOF && ret != 0 && endWithComment == 0)
		    {   //f1 n est pas la derniere fractale du fichier
				s1  = (char*) malloc(64*sizeof(char));
				int i = 0;
				while(name[i] != '\0')
				{
					s1[i] = name[i];
					i++;
				}
				s1[i] = '\0';
				check_the_name = check_name(s1);
				if(check_the_name == 1) //si nom pas encore utilise
				{
					f2 = fractal_new(name, size[0], size[1], complex[0], complex[1]);
					if(f2 == NULL) exit(1);
				
					sem_wait(&buffer1_empty);
					pthread_mutex_lock(&mutex_buffer1);
					buffer1[currentEcriture1] = f1;
					currentEcriture1 = (currentEcriture1 + 1)%N1;
					pthread_mutex_lock(&mutex_nbFrac);
					nbFrac++;
					pthread_mutex_unlock(&mutex_nbFrac);
					nbFrac1++;
					pthread_mutex_unlock(&mutex_buffer1);
					sem_post(&buffer1_full);
					f1 = f2;
				}
				else{free(s1);} //si nom deja utilise, on neglige cette fractale
				/* */
				
				endWithComment = zap(file);
			}//derniere fractale du fichier
			
			sem_wait(&buffer1_empty);
			pthread_mutex_lock(&mutex_buffer1);
			buffer1[currentEcriture1] = f1;
			currentEcriture1 = (currentEcriture1 + 1)%N1;
			if(nf==nbFiles-2)
			{
				pthread_mutex_lock(&mutex_endProducer);
				endProducer = 1;
				pthread_mutex_unlock(&mutex_endProducer);
			}
			pthread_mutex_lock(&mutex_nbFrac);
			nbFrac++;
			pthread_mutex_unlock(&mutex_nbFrac);
			nbFrac1++;
			pthread_mutex_unlock(&mutex_buffer1);
			sem_post(&buffer1_full);
			
			fclose(file); 
        }
    }
    free_name();
    return (NULL);
}


void* consommateur( void* arg )
{
	
	struct fractal* f; //pointe vers la fractale sur laquelle il va travailler
	int state = 1;     //running 
	while(state == 1)
	{
		
		sem_wait(&buffer1_full); //pause s'il n'y a plus de Frac dans le buffer1
		pthread_mutex_lock(&mutex_buffer1);
		f = buffer1[currentLecture1]; 
		if(f!=NULL)
		{
			buffer1[currentLecture1] = NULL;
			currentLecture1 = (currentLecture1+1)%N1;
			nbFrac1--;
		}

		//si plus aucune Frac a calcule et qu'il s'agit de la derniere frac
		if(endProducer == 1 && nbFrac1 == 0 && f!= NULL)
		{
			state = 2; // finish apres cete fractale
			sem_post(&buffer1_full); //permet de commencer la liberation en cascade des threads bloques sur le semaphore
		//si plus aucune frac a traduire alors le thread peut s'arreter
		}
		else if(endProducer == 1 && nbFrac1 == 0 && f==NULL)
		{
			state = -1; //finish
			sem_post(&buffer1_full); //liberation en cascade : libere un thread sur le semaphore
		}
		pthread_mutex_unlock(&mutex_buffer1);
        sem_post(&buffer1_empty);
        
		if(state != -1) //si le thread a une frac a traiter
		{
			
			int i;
			int j;
			double sum = 0;
			for(i = 0; i<f->height;i++)
			{
				for(j = 0;j<f->width;j++)
				{
					int val = fractal_compute_value(f, j, i);
					sum = sum +val;
					fractal_set_value(f, j, i, val);
				}
			}
			f->val = sum/(f->height*f->width);
			
			sem_wait(&buffer2_empty);
			pthread_mutex_lock(&mutex_buffer2);
			buffer2[currentEcriture2] = f;
			currentEcriture2 = (currentEcriture2+1)%N2;
			nbFrac2++;
			pthread_mutex_lock(&mutex_nbFrac);
			nbFrac--;
			pthread_mutex_lock(&mutex_endProducer);
			if(nbFrac == 0 && endProducer == 1)
			{
				endConsumer = 1;
			}
			pthread_mutex_unlock(&mutex_nbFrac);
			pthread_mutex_unlock(&mutex_endProducer);
			pthread_mutex_unlock(&mutex_buffer2);
			sem_post(&buffer2_full);
		}
	}
	return NULL;
}


void* trieur( void* arg )
{
	
	int state = 1; // running
	struct fractal* f;
	while(state)
	{
		pthread_mutex_lock(&mutex_buffer2);
		if(endConsumer == 1 && nbFrac2 == 0) // si plus aucune Frac a triee
		{
			state = 0; // finish
			pthread_mutex_unlock(&mutex_buffer2);
		}
		else
		{
			pthread_mutex_unlock(&mutex_buffer2);
			sem_wait(&buffer2_full); // pause s'il n'y a plus de Frac calculee dans le buffer 2

			pthread_mutex_lock(&mutex_buffer2);
			
			f = buffer2[currentLecture2];
			buffer2[currentLecture2] = NULL;
			currentLecture2 = (currentLecture2 +1)%N2;
			nbFrac2--;
			
			pthread_mutex_unlock(&mutex_buffer2);
			sem_post(&buffer2_empty);
			 
			double val = f->val; //recupere la valeur de la fractale
			if(val < bestVal)
			{
				if(printAllFrac == 1) //on conserve les fractales non optimales que si -d specifie
				{
					push_stackFrac(f,&other);
				}
				else
				{
					fractal_free(f);
				}
			}
			else if(val == bestVal)
			{
				push_stackFrac(f,&best);
			}
			else if(val > bestVal)
			{
				struct fractal* frac = pop_stackFrac(&best);
				while(frac != NULL)
				{
					if(printAllFrac == 1)
					{
						push_stackFrac(frac,&other);
					}
					else
					{
						fractal_free(frac);
					}
					frac = pop_stackFrac(&best);
				}
				push_stackFrac(f,&best);
				bestVal = val;
			}
		}
	}
	return NULL;
}

int main(int argc, char *argv[])
{
	/* -------------------------------------------------------------------- *
	 *  		Gestion des arguments (parametres utilisateur)
	 * --------------------------------------------------------------------*/
	printf("BEGIN\n");
	
	nbFiles = 0;       //valeur par defaut
	int nbThreads = 1; //nombre de threads consommateur par defaut
    N1 = 10;            //taille du buffer 1
	N2 = 10;            //taille du buffer 2
	
	char* ptr;
	char* files_tab[argc-1]; //argc-1 = nombre maximal de fichiers de hash
	int i;
	for(i=1;i<argc;i++)
	{
		if(strcmp(argv[i],"--maxthreads") == 0) //parametre sur le nombre de threads consommateur
		{
			nbThreads = strtol(argv[i+1],&ptr,10);
			if(strcmp(ptr,"\0") != 0) exit(1);
			i++;
		}
		else if(strcmp(argv[i],"-d") == 0) //generer le fichier BMP pour toutes les fractales calculees
		{
			printAllFrac = 1;
		}
		else if(strcmp(argv[i],"-") == 0) //lire l entree standard
		{
			entre_standard = 1;
		}
		else //sinon il doit s'agir d'un nom de fichier de fractales
		{
			files_tab[nbFiles] = argv[i];
			nbFiles++;
		}
	}
	
	files = (char**) files_tab;
	
	//creation des buffers
	buffer1 = (struct fractal**) malloc(N1*sizeof(struct fractal*));
	int k;
	for(k = 0; k<N1;k++)
	{
		buffer1[k] = NULL;
	}
	buffer2 = (struct fractal**) malloc(N2*sizeof(struct fractal*));
	for(k = 0; k<N2;k++)
	{
		buffer2[k] = NULL;
	}
	
	//semaphores
	int err = sem_init(&buffer1_full,0,0);
	if(err==-1) exit(1);
	err = sem_init(&buffer1_empty,0,N1);
	if(err==-1) exit(1);
	
	err = sem_init(&buffer2_full,0,0);
	if(err==-1) exit(1);
	err = sem_init(&buffer2_empty,0,N2);
	if(err==-1) exit(1);
	
	//mutex
	pthread_mutex_init(&mutex_buffer1,NULL);
	pthread_mutex_init(&mutex_endProducer,NULL);
	pthread_mutex_init(&mutex_buffer2,NULL);
	pthread_mutex_init(&mutex_endConsumer,NULL);
	pthread_mutex_init(&mutex_nbFrac,NULL);
	
	//threads
	pthread_t producer;            //producteur
	pthread_t consumer[nbThreads]; //consommateur
	pthread_t sorter;              //trieur

	//demarrage
    pthread_create(&producer, NULL, producteur, NULL);
    for(i=0;i<nbThreads;i++)
    {
		pthread_create(&consumer[i], NULL, consommateur, NULL);
	}
	pthread_create(&sorter, NULL, trieur, NULL);

    //fin des threads
    pthread_join(producer, NULL);
    for(i=0;i<nbThreads;i++)
    {
		pthread_join(consumer[i], NULL);
 	}
 	pthread_join(sorter, NULL);

	//liberation memeoire
 	sem_destroy(&buffer1_full);
 	sem_destroy(&buffer1_empty);
 	sem_destroy(&buffer2_full);
 	sem_destroy(&buffer2_empty);
	free(buffer1);
	free(buffer2);
	
	//generation des fichiers de sorties
 	struct fractal* f = pop_stackFrac(&best);
 	while(f != NULL) //fractales optimales
 	{
		char result[133];
		concate(files[nbFiles-1], "_",result);
		concate(result,f->name,result);
		concate(result, ".bmp",result);
		err = write_bitmap_sdl(f,result);
		if(err==-1) exit(1);
		fractal_free(f);
		f = pop_stackFrac(&best);
	}
	f = pop_stackFrac(&other);
	while(f != NULL) //fractales non optimales
 	{
		char result[68];
		concate(f->name,".bmp",result);
		err = write_bitmap_sdl(f,result);
		if(err==-1) exit(1);
		fractal_free(f);
		f = pop_stackFrac(&other);
	}
	
	return 0; //reussite
}
