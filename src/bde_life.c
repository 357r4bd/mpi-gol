/* 
 * The Game Of Life
*/ 

#define ROOT        0
#define HEIGHT      DIM
#define WIDTH       DIM
#define GENERATIONS GEN
 
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "mpi.h"

/* function declarations */
void board_to_file(int (*current)[HEIGHT],int i);

/* func declaration uses pointer notation to pass 2d arrays */
void rules(int (*current)[HEIGHT],int generation,int start_col,int end_col);
int get_neighbor_count (int i, int j, int (*current)[HEIGHT]);

int main(int argc, char** argv) {
  int my_rank;                                               /* My process rank           */
  int p;                                                     /* The number of processes   */
  int (*game_board)[HEIGHT];                                 /* Main board                */
  int (*temp)[HEIGHT];                                       /* pointer to 2d array       */
  int i,j,k,m,n;                                             /* available iterators       */
  int tmp,per_proc,left_over,my_start_col,my_end_col;
  int tmp_start_col,tmp_end_col;
  MPI_Status  status;
  FILE *INIT;
  
  MPI_Init(&argc, &argv);
  /* get number of procs */
  MPI_Comm_size(MPI_COMM_WORLD,&p); 
  /* get rank of current process */
  MPI_Comm_rank(MPI_COMM_WORLD,&my_rank);  

  /* size game board */
  game_board=malloc(HEIGHT*WIDTH*sizeof(int));

  if (my_rank == ROOT) {
    
    /*
     * Board initialization!! 
     * 1) look for "init.dat" is not found in CWD
     * 2) if "init.dat" is not found, ask user interactively what pattern to start with:
     *    a) random
     *    b) alternating rows (starting with all 1)
     *    c) upper triangular matrix of 1's
    */  

    /* look for init.dat */
    INIT = fopen("init.dat","r");
    if (INIT) {
      printf("Initializing grid from input.dat\n");
      for (i=0;i<HEIGHT;i++) {
        for (j=0;j<WIDTH;j++) {
          fscanf(INIT,"%d",&game_board[i][j]);
        }
      }      
    } else {
      printf("Init file not found, please choose how to initialize game board\n 1) Random\n 2) Alternating rows of 1's & 0's\n 3) Upper triangular matrix of 1's\n");
      scanf("%d",&n);
      switch (n) {
	case 1:
	for (i=0;i<HEIGHT; i++) {
	  for (j=0;j<WIDTH;j++) {
            game_board[i][j] = rand()%2;	
	  }  
        } 
        break;
	case 2:
	for (i=0;i<HEIGHT; i++) {
	  for (j=0;j<WIDTH;j++) {
	    if (i%2 == 0) {
	      tmp = 1;
	    } else {
	      tmp = 0;
	    }
            game_board[i][j] = tmp;	      
	  }  
        } 
	break;
	case 3:
	for (i=0;i<HEIGHT; i++) {
	  for (j=0;j<WIDTH;j++) {
            game_board[j][i] = 0;	      	      
	  }  
        } 
	for (i=0;i<HEIGHT; i++) {
	  for (j=0;j<WIDTH-i;j++) {
            game_board[j][i] = 1;	      	      
	  }  
        } 
	break;
      }    
    }
       
    /* dump initial grid as frame 0 - 0.dat */
    board_to_file(game_board,0);

    printf("Running the game of life on %d process(es)\n",p);\
    
    per_proc =  (int)floor(WIDTH/p);
    left_over = (int)(WIDTH%p);   
  } /* ROOT only */
     
  /* wait for initialization if not ROOT */  
  (void) MPI_Bcast(&per_proc,1,MPI_INT,ROOT,MPI_COMM_WORLD);
  (void) MPI_Bcast(&left_over,1,MPI_INT,ROOT,MPI_COMM_WORLD);

  /* let each processor decide what column(s) to use */
  my_start_col = my_rank * per_proc;
  my_end_col = my_start_col + per_proc - 1;
  if (my_rank == (p-1)) {
    my_end_col += left_over;
  }

  /* allocate memory for array holding temp values */
  if (my_rank != ROOT) {
    temp = malloc(HEIGHT*(my_end_col-my_start_col+1)*sizeof(int));
  }
  
  /*
  *
  * MAIN LOOP
  *
  */ 
  
  for (i=1;i<=GENERATIONS;i++) {
    if (my_rank == ROOT) {
      printf("Generation %4d/%d on %d\n",i,GENERATIONS,my_rank);
    }

    if (p > 1) {
      /* broadcast current game_board to all non-ROOT procs */
      (void) MPI_Bcast(game_board,HEIGHT*WIDTH,MPI_INT,ROOT,MPI_COMM_WORLD);
    } 
    
    /* apply rules */  
    rules(game_board,i,my_start_col,my_end_col);
    
    if (my_rank != ROOT) {                              /* <<----------------------------------- send    */
      /* populate temp game_board cols owned by this process */
      for (m=my_start_col;m<=my_end_col;m++) {
        for (j=0;j<HEIGHT;j++) {
	  temp[m-my_start_col][j] = game_board[m][j];
	}
      }
      /* send it to root */
      /*printf("Process %d trying to send info to ROOT\n",my_rank);*/
      (void) MPI_Send(temp,HEIGHT*(my_end_col-my_start_col+1),MPI_INT,ROOT,0,MPI_COMM_WORLD);   
      fflush(stdout);
    } else if (my_rank == ROOT && p > 1) {              /* <<----------------------------------- receive */
      for (k=1;k<p;k++) {
        /* calc start and end cols for proc k */
	tmp_start_col = k * per_proc;
	tmp_end_col = tmp_start_col + per_proc - 1;
	if (k == (p-1)) {
	  tmp_end_col += left_over;
	}
	/* size temp for receiving */
	temp=malloc(HEIGHT*(tmp_end_col-tmp_start_col+1)*sizeof(int));
        /*printf("Root waiting for info from process %d\n",k);*/
        (void) MPI_Recv(temp,HEIGHT*(tmp_end_col-tmp_start_col+1),MPI_INT,k,0,MPI_COMM_WORLD,&status);
        fflush(stdout);
 	/* changes ROOT's game_board for proc k's columns */
        for (m=tmp_start_col;m<=tmp_end_col;m++) {
  	  for (j=0;j<HEIGHT;j++) {
	    game_board[m][j] = temp[m-tmp_start_col][j];
	  }
	}
	free(temp);
      }      
    }  

    /* output compite game_board */
    if (my_rank == ROOT) {
      for (m=0;m<HEIGHT; m++) {
	for (j=0;j<WIDTH;j++) {
	  printf("%d ",game_board[m][j]);
	}
	printf("\n");
	fflush(stdout);
      }
      printf("\n");
    
      board_to_file(game_board,i);
    }
  }
  
  MPI_Finalize();
  exit(0);
  
  return 0;
}

/* 
 * apply rules  - func declaration uses pointer notation to pass 2d arrays 
 *
 * rule 1 -  live cells with 1 or 0 neighbors die from lonliness
 * rule 2 -  live cells with 2 or 3 neighbors live to the next generation
 * rule 3 -  live cells with 4 or more neighbors will die from over crowding
 * rule 4 -  dead cells with exactly 3 neighbors come to life
 *
*/

void rules(int (*current)[HEIGHT], /* IN current board */
	   int generation,         /* IN generation */
	   int start_col,          /* In start column for the current proc */
	   int end_col)            /* In end column for the current proc */
{
  int i;
  int j;
  int neighbors,w,my_rank;
  int (*temp)[HEIGHT];

  MPI_Comm_rank(MPI_COMM_WORLD,&my_rank);

  /* size temp */
  temp=malloc(HEIGHT*WIDTH*sizeof(int));

  /* store temp array */
  for (i=0;i<HEIGHT; i++) {
    for (j=0;j<WIDTH;j++) {
      temp[i][j] = current[i][j];
    }
  } 

  if (my_rank == -1) {
    for (i=0;i<HEIGHT; i++) {
      for (j=0;j<WIDTH;j++) {
	printf("%d ",temp[i][j]);
      }
      printf("\n");
      fflush(stdout);
    }
    printf("\n");
  }
  
  
  for (i=start_col;i<=end_col;i++) {
    /* only worry about the proc's own columns */
    for (j=0;j<HEIGHT;j++) {
      neighbors = get_neighbor_count(j,i,temp);
      if (temp[i][j] == 1) {
	if (neighbors == 0 || neighbors == 1) {
            current[i][j] = 0;
	} else if (neighbors > 3) {
	    current[i][j] = 0;
	}
      } else if (temp[i][j] == 0) {
        if (neighbors == 3) {
	  current[i][j] = 1;
	}      
      }
    }  
  }
}

/*
 * returns the number of neighbors surrounding cell
*/

int get_neighbor_count (int i, int j, int (*current)[HEIGHT]) {
  int a,b,c,d,e,f,x,count;
  a = j-1; /* bounded by WIDTH bdy   */
  b = j;   /* won't violate bdy      */
  c = j+1; /* bounded by WIDTH bdy   */ 
  d = i-1; /* bounded by HEIGHT  bdy */
  e = i;   /* won't violate bdy      */ 
  f = i+1; /* bounded by HEIGHT  bdy */
  count = 0;
  for (x=1;x <= 8;x++) {
    switch (x) {
      case 1:
        if (a >= 0 && d >= 0) { 
          count += current[a][d];
        }
      break;
      case 2:
        if (a >= 0) {
          count += current[a][e];
        }
      break;
      case 3:
        if (a >= 0 && f < HEIGHT) {
          count += current[a][f]; 
	}       
      break;
      case 4:
        if (f < HEIGHT) {
          count += current[b][f];      
        }
      break;
      case 5:
        if (c < WIDTH && f < HEIGHT) {
          count += current[c][f];
	}  
      break;
      case 6:
        if (c < WIDTH) {
          count += current[c][e];
        }
      break;      
      case 7:
        if (c < WIDTH && d >= 0) {
          count += current[c][d];
        }
      break;      
      case 8:
        if (d >= 0) {
          count += current[b][d];
        }
      break;      
    }
  }
  return count;
}

/*
 * dump frame to stdout
*/

void board_to_file(int (*current)[HEIGHT],int generation) {
  int i;
  int j;
  char filename[10];
  sprintf(filename,"frames/%d.dat",generation);
  FILE *OUTPUT = fopen(filename,"w");
  for (i=0;i<HEIGHT; i++) {
    for (j=0;j<WIDTH;j++) {
      if (current[i][j] == 1) {
        fprintf(OUTPUT,"%d ",current[i][j]);
      } else {
        fprintf(OUTPUT,"  ");
      }
    }
    fprintf(OUTPUT,"\n");
  }
  fflush(OUTPUT);
  fclose(OUTPUT);
}
