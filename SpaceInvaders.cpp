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
#define LIGNE_VAISSEAU 17
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
bool ok = false;
int nbAliens = 24; // param flotteAliens
int lh = 2;
int cg = 8;
int lb = 8;
int cd = 18;
int delay = 1000;
void HandlerSigusr1(int sig);
void HandlerSigusr2(int sig);
void HandlerSighup(int sig);
void HandlerSigint(int sig);
void HandlerSigquit(int sig);
struct sigaction gauche,droite,espace,missile,mortVaisseau;
pthread_t Vaiss,Missile,TimeOutMissile,Event,FlotteAliens,Invader; // identifiant different thread
pthread_mutex_t mutexGrille,mutexColonne,mutexFlotteAliens; // mutex de la grille
void *fctVaisseau(void *);
void *fctMissile(S_POSITION* mi);
void *fctTimeOut(void*);
void *fctEvent(void*);
void *fctInvader(void*);
void *fctFlotteAliens(void*);
void modifyFlotte();
void afficheFlotte();
void supprimeAlien();

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
  if(  pthread_mutex_init(&mutexColonne,NULL) != 0)
  {
    perror("Erreur creation mutex colonne\n");
    exit(1);
  }
  if(  pthread_mutex_init(&mutexFlotteAliens,NULL) != 0)
  {
    perror("Erreur creation mutex flotteAliens\n");
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

  missile.sa_handler = HandlerSigint;
  sigemptyset(&missile.sa_mask);
  missile.sa_flags = 0;
  sigaction(SIGINT,&missile,NULL); 

  mortVaisseau.sa_handler = HandlerSigquit;
  sigemptyset(&mortVaisseau.sa_mask);
  mortVaisseau.sa_flags = 0;
  sigaction(SIGQUIT,&mortVaisseau,NULL); 



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
  if(pthread_create(&Event,NULL,fctEvent,NULL) != 0)
  {
    perror("Error create thread event \n");
    exit(1);
  }
  if(pthread_create(&Vaiss,NULL,fctVaisseau,NULL) != 0)
  {
    perror("Error create thread vaisseau \n");
    exit(1);
  }
  if(pthread_create(&Invader,NULL,fctInvader,NULL) != 0)
  {
    perror("Error create thread invader \n");
    exit(1);
  }

  if(pthread_join(Event,NULL) != 0)
  {
    perror("Erreur pthread join main\n");
    exit(1);
  }

  exit(0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void *fctInvader(void*)
{
  bool fin = false;
  // cas 1 pour invasion reussi
  // 2 pour invasion rate -> nbAliens = 0
  int *cas;
  printf("Creation premiere flotte\n");
    if(pthread_create(&FlotteAliens,NULL,fctFlotteAliens,NULL) != 0)
    {
      perror("Error create thread flotteAliens \n");
      exit(1);
    }
    if(pthread_join(FlotteAliens,(void **)&cas) != 0)
    {
      perror("Erreur pthread join main\n");
      exit(1);
    }   
  while(!fin)
  {
    if(*cas == 1)
    {
      fin = true;
      if( pthread_mutex_lock(&mutexGrille) != 0)
      {
        perror("Erreur lock mutex grille \n");
        exit(1);
      }
      supprimeAlien();
      kill(0,SIGQUIT); 
      if( pthread_mutex_unlock(&mutexGrille) != 0)
      {
        perror("Erreur lock mutex grille \n");
        exit(1);
      }
    }
    if(*cas == 2)
    {
      *cas == -1;
      delay = (delay/100)*70;
      if(pthread_create(&FlotteAliens,NULL,fctFlotteAliens,NULL) != 0)
      {
        perror("Error create thread flotteAliens \n");
        exit(1);
      }
      if(pthread_join(FlotteAliens,(void **)&cas) != 0)
      {
        perror("Erreur pthread join main\n");
        exit(1);
      }       
    }
    // on recrée les bouclier
    if( pthread_mutex_lock(&mutexGrille) != 0)
    {
      perror("Erreur lock mutex grille \n");
      exit(1);
    }
    setTab(NB_LIGNE-2,11,BOUCLIER1,0);  DessineBouclier(NB_LIGNE-2,11,1);
    setTab(NB_LIGNE-2,12,BOUCLIER1,0);  DessineBouclier(NB_LIGNE-2,12,1);
    setTab(NB_LIGNE-2,13,BOUCLIER1,0);  DessineBouclier(NB_LIGNE-2,13,1);
    setTab(NB_LIGNE-2,17,BOUCLIER1,0);  DessineBouclier(NB_LIGNE-2,17,1);
    setTab(NB_LIGNE-2,18,BOUCLIER1,0);  DessineBouclier(NB_LIGNE-2,18,1);
    setTab(NB_LIGNE-2,19,BOUCLIER1,0);  DessineBouclier(NB_LIGNE-2,19,1);
    if( pthread_mutex_unlock(&mutexGrille) != 0)
    {
      perror("Erreur lock mutex grille \n");
      exit(1);
    }
  }
  pthread_exit(NULL);
}
void *fctFlotteAliens(void*)
{
  int i,j,k;
  int rtr;
  if( pthread_mutex_lock(&mutexGrille) != 0)
  {
    perror("Erreur lock mutex grille \n");
    exit(1);
  }
  // Creation des aliens
  for(i = 2;i < 9 ; i=i+2)
  {
    for(j = 8 ; j < 19; j = j+ 2)
    {
      setTab(i, j, ALIEN, pthread_self());
      DessineAlien(i,j);
    }
  }
  if( pthread_mutex_unlock(&mutexGrille) != 0)
  {
    perror("Erreur lock mutex grille \n");
    exit(1);
  }
  if( pthread_mutex_lock(&mutexFlotteAliens) != 0)
  {
    perror("Erreur lock mutex flotte \n");
    exit(1);
  }
  lh = 2;
  cg = 8;
  lb = 8;
  cd = 18;
  nbAliens = 24;
  if( pthread_mutex_unlock(&mutexFlotteAliens) != 0)
  {
    perror("Erreur unlock mutex flotte \n");
    exit(1);
  }


  while(nbAliens > 0 && lb < 15)
  {
    // 4 pas sur la droite
    printf("4 pas sur la droite\n");
    printf("nb alien : %d\n",nbAliens);
    for(k = cd; k < 22; k++)
    {
      Attente(delay);
      if(nbAliens == 0)
      { 
        printf("fin flotte plus d'aliens\n");
        rtr = 2;
        pthread_exit(&rtr);
      }
      printf("represente cd : k %d",k);
      afficheFlotte();
      for(i = lh;i < lb + 1 ; i=i+2)
      {
        for(j = cg ; j < cd + 1; j = j+ 2)
        {
          if( pthread_mutex_lock(&mutexGrille) != 0)
          {
            perror("Erreur lock mutex grille \n");
            exit(1);
          }
          if(tab[i][j].type == ALIEN)
          {
            if(tab[i][j+1].type == VIDE)
            {
              setTab(i,j,VIDE,0);
              EffaceCarre(i,j);
              setTab(i, j+1, ALIEN, pthread_self());
              DessineAlien(i,j+1);            
            }
            else
            {
              pthread_kill(tab[i][j+1].tid,SIGINT);
              setTab(i,j+1,VIDE,0);
              EffaceCarre(i,j+1);
              setTab(i,j,VIDE,0);
              EffaceCarre(i,j);
              if( pthread_mutex_lock(&mutexFlotteAliens) != 0)
              {
                perror("Erreur lock mutex grille \n");
                exit(1);
              }
              nbAliens--;
              modifyFlotte();
              if( pthread_mutex_unlock(&mutexFlotteAliens) != 0)
              {
                perror("Erreur lock mutex grille \n");
                exit(1);
              }
            }            
          }
          if( pthread_mutex_unlock(&mutexGrille) != 0)
          {
            perror("Erreur lock mutex grille \n");
            exit(1);
          } 
        }
      }
      if( pthread_mutex_lock(&mutexFlotteAliens) != 0)
      {
        perror("Erreur lock mutex grille \n");
        exit(1);
      }  
      cg++;
      cd++;
      if( pthread_mutex_unlock(&mutexFlotteAliens) != 0)
      {
        perror("Erreur lock mutex grille \n");
        exit(1);
      }  

    }
    // 4 pas sur la gauche
    printf("4 pas sur la gauche\n");
    printf("nb alien : %d\n",nbAliens);
    for(k = cg; k > 8; k--)
    {
      if(nbAliens == 0)
      { 
        printf("fin flotte plus d'aliens\n");
        rtr = 2;
        pthread_exit(&rtr);
      }
      printf("represente cg: k %d",k);
      afficheFlotte();
      Attente(delay);
      for(i = lh;i < lb + 1 ; i=i+2)
      {
        for(j = cd ; j > cg - 1; j = j- 2)
        {
          if( pthread_mutex_lock(&mutexGrille) != 0)
          {
            perror("Erreur lock mutex grille \n");
            exit(1);
          }
          if(tab[i][j].type == ALIEN)
          {
            if(tab[i][j-1].type == VIDE)
            {
              setTab(i,j,VIDE,0);
              EffaceCarre(i,j);
              setTab(i, j-1, ALIEN, pthread_self());
              DessineAlien(i,j-1);            
            }
            else
            {
              pthread_kill(tab[i][j-1].tid,SIGINT);
              setTab(i,j-1,VIDE,0);
              EffaceCarre(i,j-1);
              setTab(i,j,VIDE,0);
              EffaceCarre(i,j);
              if( pthread_mutex_lock(&mutexFlotteAliens) != 0)
              {
                perror("Erreur lock mutex grille \n");
                exit(1);
              }
              nbAliens--;
              modifyFlotte();
              if( pthread_mutex_unlock(&mutexFlotteAliens) != 0)
              {
                perror("Erreur lock mutex grille \n");
                exit(1);
              }
            }
          }
          if( pthread_mutex_unlock(&mutexGrille) != 0)
          {
            perror("Erreur lock mutex grille \n");
            exit(1);
          } 
        }
      }
      if( pthread_mutex_lock(&mutexFlotteAliens) != 0)
      {
        perror("Erreur lock mutex grille \n");
        exit(1);
      }  
      cg--;
      cd--;
      if( pthread_mutex_unlock(&mutexFlotteAliens) != 0)
      {
        perror("Erreur lock mutex grille \n");
        exit(1);
      }   
    }
    // on descend de 1
    printf("1 pas vers le bas\n");
    printf("nb alien : %d\n",nbAliens);
    Attente(delay);
    if(nbAliens == 0)
    { 
      printf("fin flotte plus d'aliens\n");
      rtr = 2;
      pthread_exit(&rtr);
    }
    for(i = lh ;i < lb + 1 ; i=i+2)
    {
      for(j = cg ; j < cd + 1; j = j+ 2)
      {
        if( pthread_mutex_lock(&mutexGrille) != 0)
        {
          perror("Erreur lock mutex grille \n");
          exit(1);
        }
        if(tab[i][j].type == ALIEN)
        {
          if(tab[i + 1][j].type == VIDE)
          {
            setTab(i,j,VIDE,0);
            EffaceCarre(i,j);
            setTab(i + 1, j, ALIEN, pthread_self());
            DessineAlien(i + 1,j);            
          }
          else
          {
            pthread_kill(tab[i+1][j].tid,SIGINT);
            setTab(i+1,j,VIDE,0);
            EffaceCarre(i+1,j);
            setTab(i,j,VIDE,0);
            EffaceCarre(i,j);
            if( pthread_mutex_lock(&mutexFlotteAliens) != 0)
            {
              perror("Erreur lock mutex grille \n");
              exit(1);
            }
            nbAliens--;
            modifyFlotte();
            if( pthread_mutex_unlock(&mutexFlotteAliens) != 0)
            {
              perror("Erreur lock mutex grille \n");
              exit(1);
            }
          }          
        }
        if( pthread_mutex_unlock(&mutexGrille) != 0)
        {
          perror("Erreur lock mutex grille \n");
          exit(1);
        } 
      }
    }

    if( pthread_mutex_lock(&mutexFlotteAliens) != 0)
    {
      perror("Erreur lock mutex Flotte \n");
      exit(1);
    }  
    lh++;
    lb++;
    if( pthread_mutex_unlock(&mutexFlotteAliens) != 0)
    {
      perror("Erreur unlock mutex Flotte \n");
      exit(1);
    } 
  }
  printf("On sort du thread flotte\n");
  if(nbAliens == 0)
  {
    rtr = 2;
    pthread_exit(&rtr);
  }
  else
  {
    rtr = 1;
    pthread_exit(&rtr);
  }
}
void *fctEvent(void*)
{
  EVENT_GRILLE_SDL event;
  while(!ok)
  {
    event = ReadEvent();
    if (event.type == CROIX) ok = true;
    if (event.type == CLAVIER)
    {
      if (event.touche == KEY_RIGHT){ 
        //printf("Flèche droite enfoncée\n");
        kill(0,SIGUSR1);
      }
      if (event.touche == KEY_LEFT)
      {
        //printf("Flèche droite enfoncée\n");
        kill(0,SIGUSR2);
      } 
      if(event.touche == KEY_SPACE)
      {
        //printf("Espace enfoncée\n");
        kill(0,SIGHUP);
      }
      //printf("Touche enfoncee : %c\n",event.touche);
    }
  }

  // Fermeture de la fenetre
  printf("(ThreadEvent %ld) Fermeture de la fenetre graphique...",pthread_self()); fflush(stdout);
  FermetureFenetreGraphique();
  printf("OK\n");
  pthread_exit(NULL);
}
void *fctVaisseau(void *)
{
  sigset_t mask;

  droite.sa_handler = HandlerSigusr1;
  sigaction(SIGUSR1,&droite,NULL);

  gauche.sa_handler = HandlerSigusr2;
  sigaction(SIGUSR2,&gauche,NULL);

  espace.sa_handler = HandlerSighup;
  sigaction(SIGHUP,&espace,NULL); 

  mortVaisseau.sa_handler = HandlerSigquit;
  sigaction(SIGQUIT,&mortVaisseau,NULL);

  sigemptyset(&mask);
  sigaddset(&mask,SIGINT);
  sigaddset(&mask,SIGALRM);
  sigaddset(&mask,SIGCHLD);
  sigprocmask(SIG_SETMASK,&mask,NULL);

  colonne = 15;
  if( pthread_mutex_lock(&mutexGrille) != 0)
  {
    perror("Erreur lock mutex grille \n");
    exit(1);
  }
  if(tab[LIGNE_VAISSEAU][colonne].type == VIDE)
  {  
    setTab(LIGNE_VAISSEAU, colonne, VAISSEAU, pthread_self());
    DessineVaisseau(LIGNE_VAISSEAU,colonne);
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
void HandlerSigusr1(int sig)
{
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
    setTab(LIGNE_VAISSEAU, colonne, VIDE, 0);
    EffaceCarre(LIGNE_VAISSEAU,colonne);

    if(tab[LIGNE_VAISSEAU][colonne+1].type == VIDE)
    {
      colonne++;
      setTab(LIGNE_VAISSEAU, colonne, VAISSEAU, pthread_self());
      DessineVaisseau(LIGNE_VAISSEAU,colonne);
      //printf("Vaisseau deplacé sur la droite\n");     
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
    setTab(LIGNE_VAISSEAU, colonne, VIDE, 0);
    EffaceCarre(LIGNE_VAISSEAU,colonne);
    if(tab[LIGNE_VAISSEAU][colonne-1].type == VIDE)
    {
      colonne--;
      setTab(LIGNE_VAISSEAU, colonne, VAISSEAU, pthread_self());
      DessineVaisseau(NB_LIGNE-1,colonne);     
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
  if(fireOn)
  {
    fireOn = false;
    S_POSITION * mi = (S_POSITION *)malloc(sizeof(S_POSITION));
    mi->L = NB_LIGNE -2;
    mi->C = colonne;
    if(pthread_create(&Missile,NULL,(void*(*)(void*))fctMissile,mi) != 0)
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


}
void HandlerSigquit(int sig)
{
  printf("Siquit recu\n");

  if( pthread_mutex_lock(&mutexColonne) != 0)
  {
    perror("Erreur lock mutex grille \n");
    exit(1);
  }  
  setTab(LIGNE_VAISSEAU,colonne,VIDE,0);
  EffaceCarre(LIGNE_VAISSEAU,colonne);
  if( pthread_mutex_unlock(&mutexColonne) != 0)
  {
    perror("Erreur lock mutex grille \n");
    exit(1);
  }
}
void *fctMissile(S_POSITION* mi)
{
  sigset_t mask;
  missile.sa_handler = HandlerSigint;
  sigaction(SIGINT,&missile,NULL);

  sigemptyset(&mask);
  sigaddset(&mask,SIGUSR1);
  sigaddset(&mask,SIGALRM);
  sigaddset(&mask,SIGCHLD);
  sigaddset(&mask,SIGUSR2);
  sigaddset(&mask,SIGHUP);
  sigaddset(&mask,SIGQUIT);
  sigprocmask(SIG_SETMASK,&mask,NULL);

  if( pthread_mutex_lock(&mutexGrille) != 0)
  {
    perror("Erreur lock mutex grille \n");
    exit(1);
  }

  switch(tab[mi->L][mi->C].type)
  {
    case 0: {
      // case vide
      setTab(mi->L, mi->C, MISSILE, pthread_self());
      DessineMissile(mi->L, mi->C);
      if(pthread_mutex_unlock(&mutexGrille) != 0)
      {
        perror("Erreur unlock mutex grille \n");
        exit(1);
      }
    }
    break;
    case 3:{
      setTab(mi->L,mi->C,VIDE,0);
      EffaceCarre(mi->L,mi->C);
      if(pthread_mutex_lock(&mutexFlotteAliens) != 0)
      {
        perror("Erreur unlock mutex grille \n");
        exit(1);
      }
      nbAliens--;
      modifyFlotte();
      if(pthread_mutex_unlock(&mutexFlotteAliens) != 0)
      {
        perror("Erreur unlock mutex grille \n");
        exit(1);
      }
      if(pthread_mutex_unlock(&mutexGrille) != 0)
      {
        perror("Erreur unlock mutex grille \n");
        exit(1);
      }
      free(mi);
      pthread_exit(NULL); 
    }
    break;
    case 5:{
      // bouclier 1
      setTab(mi->L,mi->C,VIDE,0);
      EffaceCarre(mi->L,mi->C);
      setTab(mi->L, mi->C , BOUCLIER2 , 0);
      DessineBouclier(mi->L, mi->C,2);
      if(pthread_mutex_unlock(&mutexGrille) != 0)
      {
        perror("Erreur unlock mutex grille \n");
        exit(1);
      }
      free(mi);
      pthread_exit(NULL);         
    }
    break;
    case 6:
    {
      //bouclier 2
      setTab(mi->L,mi->C,VIDE,0);
      EffaceCarre(mi->L,mi->C);
      if(pthread_mutex_unlock(&mutexGrille) != 0)
      {
        perror("Erreur unlock mutex grille \n");
        exit(1);
      }
      free(mi);
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
    if(mi->L > 0)
    {
      switch(tab[mi->L -1][mi->C].type)
      {
        case 0: {
          // case vide
          setTab(mi->L,mi->C,VIDE,0);
          EffaceCarre(mi->L,mi->C);
          setTab(mi->L -1 , mi->C, MISSILE, pthread_self());
          DessineMissile(mi->L -1, mi->C);
          mi->L--;
          if(pthread_mutex_unlock(&mutexGrille) != 0)
          {
            perror("Erreur unlock mutex grille \n");
            exit(1);
          }
        }
        break;
        case 3:{
          setTab(mi->L,mi->C,VIDE,0);
          EffaceCarre(mi->L,mi->C);
          setTab(mi->L -1, mi->C , VIDE , 0);
          EffaceCarre(mi->L -1, mi->C);
          if(pthread_mutex_lock(&mutexFlotteAliens) != 0)
          {
            perror("Erreur unlock mutex grille \n");
            exit(1);
          }
          nbAliens--;
          modifyFlotte();
          if(pthread_mutex_unlock(&mutexFlotteAliens) != 0)
          {
            perror("Erreur unlock mutex grille \n");
            exit(1);
          }
          if(pthread_mutex_unlock(&mutexGrille) != 0)
          {
            perror("Erreur unlock mutex grille \n");
            exit(1);
          }
          free(mi);
          pthread_exit(NULL);
        }
        break;
        case 5:{
          // bouclier 1
          setTab(mi->L,mi->C,VIDE,0);
          EffaceCarre(mi->L,mi->C);
          setTab(mi->L -1, mi->C , BOUCLIER2 , 0);
          EffaceCarre(mi->L -1, mi->C);
          DessineBouclier(mi->L -1, mi->C,2);
          if(pthread_mutex_unlock(&mutexGrille) != 0)
          {
            perror("Erreur unlock mutex grille \n");
            exit(1);
          }
          free(mi);
          pthread_exit(NULL);          
        }
        break;
        case 6:
        {
          //bouclier 2
          setTab(mi->L,mi->C,VIDE,0);
          EffaceCarre(mi->L,mi->C);
          setTab(mi->L -1, mi->C , VIDE , 0);
          EffaceCarre(mi->L -1, mi->C);
          if(pthread_mutex_unlock(&mutexGrille) != 0)
          {
            perror("Erreur unlock mutex grille \n");
            exit(1);
          }
          free(mi);
          pthread_exit(NULL);   
        }
      }
    }
    else
    {
      // detruit missile fin de tab
      setTab(mi->L,mi->C,VIDE,0);
      EffaceCarre(mi->L,mi->C);      
      if(pthread_mutex_unlock(&mutexGrille) != 0)
      {
        perror("Erreur unlock mutex grille \n");
        exit(1);
      }
      free(mi);
      pthread_exit(NULL);  
    }
  Attente(80);
  }
}
void HandlerSigint(int sig)
{
  pthread_exit(NULL);
}

void *fctTimeOut(void*){
  Attente(600);
  fireOn = true;
  pthread_exit(NULL);
}
void modifyFlotte()
{
  // appeler cette fonction en ayant pris le mutexFlotteAlien !!!
  int i = cg;
  int cpt = 0;
  while(cpt == 0 && i < cd + 1)
  {
    if(tab[lh][i].type == ALIEN)
      cpt++;
    i++;    
  }
  if(cpt == 0)
    lh  = lh + 2;
  cpt = 0;
  i = cg;
  while(cpt == 0 && i < cd + 1)
  {
    if(tab[lb][i].type == ALIEN)
      cpt++; 
    i++;   
  }
  if(cpt == 0)
    lb  = lb - 2;
  cpt = 0;
  i = lh;
  while(cpt == 0 && i < lb +1)
  {
    if(tab[i][cd].type == ALIEN)
      cpt++;
    i++;
  }
  if(cpt == 0)
    cd  = cd - 2;
  cpt = 0;
  i = lh;
  while(cpt == 0 && i < lb +1)
  {
    if(tab[i][cg].type == ALIEN)
      cpt++;
    i++;
  }
  if(cpt == 0)
    cg  = cg + 2;
  afficheFlotte();
}
void afficheFlotte()
{
  // fonction test
  printf("Sup gauche : %d\n",cg);
  printf("Sup droit : %d\n",cd);
  printf("ligne haut : %d\n",lh);
  printf("ligne bas : %d\n",lb);
}
void supprimeAlien(){
  // fonction a lancer avec mutexGrille pris
  int i,j;
  if(pthread_mutex_lock(&mutexFlotteAliens) != 0)
  {
    perror("Erreur unlock mutex grille \n");
    exit(1);
  }
  for(i = lh;i < lb + 1; i=i+2)
  {
    for(j = cg ; j < cd + 1 ; j = j+ 2)
    {
      if(tab[i][j].type == ALIEN)
      {
        setTab(i,j,VIDE,0);
        EffaceCarre(i,j);
      }
    }
  }  
  if(pthread_mutex_unlock(&mutexFlotteAliens) != 0)
  {
    perror("Erreur unlock mutex grille \n");
    exit(1);
  }   
}
