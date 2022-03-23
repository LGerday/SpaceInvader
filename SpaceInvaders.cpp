#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include "GrilleSDL.h"
#include "Ressources.h"

// Dimensions de la grille de jeu
#define NB_LIGNE   18
#define NB_COLONNE 23 

// Direction de mouvement
#define GAUCHE       10
#define DROITE       11
#define BAS          12

// Intervenants du jeu (type)
#define VIDE        0
#define VAISSEAU    1
#define MISSILE     2
#define ALIEN       3
#define BOMBE       4
#define BOUCLIER1   5
#define BOUCLIER2   6
#define AMIRAL      7

typedef struct
{
  int type;
  pthread_t tid;
} S_CASE;

typedef struct 
{
  int L;
  int C;
} S_POSITION;

S_CASE tab[NB_LIGNE][NB_COLONNE];
int colonne; // variable position vaisseau
bool fireOn = true;
void HandlerSigusr1(int sig);
void HandlerSigusr2(int sig);
void HandlerSighup(int sig);
struct sigaction gauche,droite,espace;
pthread_t Vaiss,Missile,TimeOutMissile; // identifiant thread vaisseau
pthread_mutex_t mutexGrille; // mutex de la grille
void *fctVaisseau(void *);
void *fctMissile(void*);
void *fctTimeOut(void*);
int caseVide(int l,int c);

void Attente(int milli);
void setTab(int l,int c,int type=VIDE,pthread_t tid=0);

///////////////////////////////////////////////////////////////////////////////////////////////////
void Attente(int milli)
{
  struct timespec del;
  del.tv_sec = milli/1000;
  del.tv_nsec = (milli%1000)*1000000;
  nanosleep(&del,NULL);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void setTab(int l,int c,int type,pthread_t tid)
{
  tab[l][c].type = type;
  tab[l][c].tid = tid;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc,char* argv[])
{
  EVENT_GRILLE_SDL event;
 
  srand((unsigned)time(NULL));

  // Ouverture de la fenetre graphique
  printf("(MAIN %ld) Ouverture de la fenetre graphique\n",pthread_self()); fflush(stdout);
  if (OuvertureFenetreGraphique() < 0)
  {
    printf("Erreur de OuvrirGrilleSDL\n");
    fflush(stdout);
    exit(1);
  }

  // Initialisation des mutex et variables de condition
  // TO DO
  if(  pthread_mutex_init(&mutexGrille,NULL) != 0)
  {
    perror("Erreur creation mutex grille\n");
    exit(1);
  }

  // Armement des signaux
  // TO DO
  droite.sa_handler = HandlerSigusr1;
  sigemptyset(&droite.sa_mask);
  droite.sa_flags = 0;
  sigaction(SIGUSR1,&droite,NULL);

  gauche.sa_handler = HandlerSigusr2;
  sigemptyset(&gauche.sa_mask);
  gauche.sa_flags = 0;
  sigaction(SIGUSR2,&gauche,NULL);

  espace.sa_handler = HandlerSighup;
  sigemptyset(&espace.sa_mask);
  espace.sa_flags = 0;
  sigaction(SIGHUP,&espace,NULL); 


  // Initialisation de tab
  for (int l=0 ; l<NB_LIGNE ; l++)
    for (int c=0 ; c<NB_COLONNE ; c++)
      setTab(l,c);

  // Initialisation des boucliers
  setTab(NB_LIGNE-2,11,BOUCLIER1,0);  DessineBouclier(NB_LIGNE-2,11,1);
  setTab(NB_LIGNE-2,12,BOUCLIER1,0);  DessineBouclier(NB_LIGNE-2,12,1);
  setTab(NB_LIGNE-2,13,BOUCLIER1,0);  DessineBouclier(NB_LIGNE-2,13,1);
  setTab(NB_LIGNE-2,17,BOUCLIER1,0);  DessineBouclier(NB_LIGNE-2,17,1);
  setTab(NB_LIGNE-2,18,BOUCLIER1,0);  DessineBouclier(NB_LIGNE-2,18,1);
  setTab(NB_LIGNE-2,19,BOUCLIER1,0);  DessineBouclier(NB_LIGNE-2,19,1);

  // Creation des threads
  // TO DO
  if(pthread_create(&Vaiss,NULL,fctVaisseau,NULL) != 0)
  {
    perror("Error create thread vaisseau \n");
    exit(1);
  }

  // Exemples d'utilisation du module Ressources --> a supprimer
  /*DessineChiffre(13,4,7);
  DessineMissile(4,12);
  DessineAlien(2,12);
  DessineVaisseauAmiral(0,15);
  DessineBombe(8,16);*/

  printf("(MAIN %ld) Attente du clic sur la croix\n",pthread_self());  
  bool ok = false;
  while(!ok)
  {
    event = ReadEvent();
    if (event.type == CROIX) ok = true;
    if (event.type == CLIC_GAUCHE)
    {
      /*if (tab[event.ligne][event.colonne].type == VIDE) 
      {
        DessineVaisseau(event.ligne,event.colonne);
        setTab(event.ligne,event.colonne,VAISSEAU,pthread_self());
      }*/
    }
    if (event.type == CLAVIER)
    {
      if (event.touche == KEY_RIGHT){ 
        printf("Flèche droite enfoncée\n");
        kill(0,SIGUSR1);
      }
      if (event.touche == KEY_LEFT)
      {
        printf("Flèche droite enfoncée\n");
        kill(0,SIGUSR2);
      } 
      if(event.touche == KEY_SPACE)
      {
        printf("Espace enfoncée\n");
        kill(0,SIGHUP);
      }
      printf("Touche enfoncee : %c\n",event.touche);
    }
  }

  // Fermeture de la fenetre
  printf("(MAIN %ld) Fermeture de la fenetre graphique...",pthread_self()); fflush(stdout);
  FermetureFenetreGraphique();
  printf("OK\n");

  exit(0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////


void *fctVaisseau(void *)
{
  droite.sa_handler = HandlerSigusr1;
  sigaction(SIGUSR1,&droite,NULL);

  gauche.sa_handler = HandlerSigusr2;
  sigaction(SIGUSR2,&gauche,NULL);

  espace.sa_handler = HandlerSighup;
  sigaction(SIGHUP,&espace,NULL); 
  colonne = 15;
  printf("Lancement thread vaisseau\n");
  if( pthread_mutex_lock(&mutexGrille) != 0)
  {
    perror("Erreur lock mutex grille \n");
    exit(1);
  }
  if(tab[NB_LIGNE-1][colonne].type == VIDE)
  {  
    tab[NB_LIGNE-1][colonne].type = VAISSEAU;
    tab[NB_LIGNE-1][colonne].tid = pthread_self();
    DessineVaisseau(NB_LIGNE-1,colonne);
  }
  else
  {
    perror("Erreur emplacement vaisseau occupé\n");
    exit(1);
  }
  if(pthread_mutex_unlock(&mutexGrille) != 0)
  {
    perror("Erreur unlock mutex grille \n");
    exit(1);
  }
  while(1)
    pause();
  pthread_exit(NULL);
}


int caseVide(int l,int c)
{



  return 0;


}



void HandlerSigusr1(int sig)
{
  printf("SIGUSR1 recu un pas a droite\n");
  if( pthread_mutex_lock(&mutexGrille) != 0)
  {
    perror("Erreur lock mutex grille \n");
    exit(1);
  }

  if((colonne + 1) > 22)
  {
    printf("Impossible d'avancer sur la droite\n");
  }
  else
  {
    tab[NB_LIGNE-1][colonne].type = VIDE;
    tab[NB_LIGNE-1][colonne].tid = 0;
    EffaceCarre(NB_LIGNE-1,colonne);
    if(tab[NB_LIGNE-1][colonne+1].type == VIDE)
    {
      colonne++;
      tab[NB_LIGNE-1][colonne].type = VAISSEAU;
      tab[NB_LIGNE-1][colonne].tid = pthread_self();
      DessineVaisseau(NB_LIGNE-1,colonne);
      printf("Vaisseau deplacer sur la droite\n");     
    }
    else
      printf("Erreur colision vaisseau droite\n");

  }

  if(pthread_mutex_unlock(&mutexGrille) != 0)
  {
    perror("Erreur unlock mutex grille \n");
    exit(1);
  }
}
void HandlerSigusr2(int sig)
{
  printf("SIGUSR1 recu un pas a droite\n");
  if( pthread_mutex_lock(&mutexGrille) != 0)
  {
    perror("Erreur lock mutex grille \n");
    exit(1);
  }

  if((colonne - 1) < 8)
  {
    printf("Impossible d'avancer sur la gauche\n");
  }
  else
  {
    tab[NB_LIGNE-1][colonne].type = VIDE;
    tab[NB_LIGNE-1][colonne].tid = 0;
    EffaceCarre(NB_LIGNE-1,colonne);
    if(tab[NB_LIGNE-1][colonne-1].type == VIDE)
    {
      colonne--;
      tab[NB_LIGNE-1][colonne].type = VAISSEAU;
      tab[NB_LIGNE-1][colonne].tid = pthread_self();
      DessineVaisseau(NB_LIGNE-1,colonne);
      printf("Vaisseau deplacer sur la gauche\n");     
    }
    else
      printf("Erreur colision vaisseau gauche\n");
  }

  if(pthread_mutex_unlock(&mutexGrille) != 0)
  {
    perror("Erreur unlock mutex grille \n");
    exit(1);
  }
}

void HandlerSighup(int sig)
{
  printf("SIGHUP recu on tire un missile\n");
  if(fireOn)
  {
    fireOn = false;
    if(pthread_create(&Missile,NULL,fctMissile,NULL) != 0)
    {
      perror("Error create thread vaisseau \n");
      exit(1);
    }  
    if(pthread_create(&TimeOutMissile,NULL,fctTimeOut,NULL) != 0)
    {
      perror("Error create thread vaisseau \n");
      exit(1);
    }  

  }
  else
    printf("Missile non dispo\n");


}
void *fctMissile(void*)
{
  int missColone;
  int ligne = NB_LIGNE -2;
  missColone = colonne;
  if( pthread_mutex_lock(&mutexGrille) != 0)
  {
    perror("Erreur lock mutex grille \n");
    exit(1);
  }
  switch(tab[ligne][missColone].type)
  {
    case 0: {
      // case vide
      printf("Case vide\n");
      setTab(ligne, missColone, MISSILE, pthread_self());
      DessineMissile(ligne, missColone);
      if(pthread_mutex_unlock(&mutexGrille) != 0)
      {
        perror("Erreur unlock mutex grille \n");
        exit(1);
      }
    }
    break;
    case 5:{
      // bouclier 1
      printf("Case bouclier1\n");
      setTab(ligne,missColone,VIDE,0);
      EffaceCarre(ligne,missColone);
      setTab(ligne, missColone , BOUCLIER2 , 0);
      DessineBouclier(ligne, missColone,2);
      if(pthread_mutex_unlock(&mutexGrille) != 0)
      {
        perror("Erreur unlock mutex grille \n");
        exit(1);
      }
      printf("Fin thread missile\n") ;
      pthread_exit(NULL);         
    }
    break;
    case 6:
    {
      //bouclier 2
      printf("Case bouclier2\n");
      setTab(ligne,missColone,VIDE,0);
      EffaceCarre(ligne,missColone);
      if(pthread_mutex_unlock(&mutexGrille) != 0)
      {
        perror("Erreur unlock mutex grille \n");
        exit(1);
      }
      printf("Fin thread missile\n") ;
      pthread_exit(NULL); 
    }
  }
  Attente(80);
  while(1)
  {
    if( pthread_mutex_lock(&mutexGrille) != 0)
    {
      perror("Erreur lock mutex grille \n");
      exit(1);
    }
    if(ligne > 0)
    {
      switch(tab[ligne -1][missColone].type)
      {
        case 0: {
          // case vide
          printf("Case vide\n");
          setTab(ligne,missColone,VIDE,0);
          EffaceCarre(ligne,missColone);
          setTab(ligne -1 , missColone, MISSILE, pthread_self());
          DessineMissile(ligne -1, missColone);
          ligne--;
          if(pthread_mutex_unlock(&mutexGrille) != 0)
          {
            perror("Erreur unlock mutex grille \n");
            exit(1);
          }
        }
        break;
        case 5:{
          // bouclier 1
          printf("Case bouclier1\n");
          setTab(ligne,missColone,VIDE,0);
          EffaceCarre(ligne,missColone);
          setTab(ligne -1, missColone , BOUCLIER2 , 0);
          EffaceCarre(ligne -1, missColone);
          DessineBouclier(ligne -1, missColone,2);
          if(pthread_mutex_unlock(&mutexGrille) != 0)
          {
            perror("Erreur unlock mutex grille \n");
            exit(1);
          }
          printf("Fin thread missile\n") ;
          pthread_exit(NULL);          
        }
        break;
        case 6:
        {
          //bouclier 2
          printf("Case bouclier2\n");
          setTab(ligne,missColone,VIDE,0);
          EffaceCarre(ligne,missColone);
          setTab(ligne -1, missColone , VIDE , 0);
          EffaceCarre(ligne -1, missColone);
          if(pthread_mutex_unlock(&mutexGrille) != 0)
          {
            perror("Erreur unlock mutex grille \n");
            exit(1);
          }
          printf("Fin thread missile\n");
          pthread_exit(NULL);   
        }
      }
    }
    else
    {
      // detruit missile fin de tab
      printf("fin de tab\n");
      setTab(ligne,missColone,VIDE,0);
      EffaceCarre(ligne,missColone);      
      if(pthread_mutex_unlock(&mutexGrille) != 0)
      {
        perror("Erreur unlock mutex grille \n");
        exit(1);
      }
      printf("Fin thread missile\n");
      pthread_exit(NULL);  
    }
  Attente(80);
  }
}
void *fctTimeOut(void*){
  Attente(600);
  fireOn = true;
  pthread_exit(NULL);
}